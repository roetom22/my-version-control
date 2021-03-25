#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <regex.h>

#include "svc.h"
#define MOD 2
#define REMOVAL 1
#define ADDITION 0

// Returns 1 if file_name is in container, else returns 0.
int is_in(void *helper, char *file_name, struct file_hash_array *container) {
	for(int i = 0; i<container->size; i++) {
		if(!strcmp(file_name, container->file_names->data[i])) {
			return 1;
		}
	}
	return 0;
}


int hash_file(void *helper, char *file_path) {
	if(file_path == NULL) {
		return -1;
	}

	/* Check that file at file_path exists*/
	if(access(file_path, F_OK ) == -1 ) {
		return -2;
	}

	FILE *file_in;
	unsigned char *file_contents;
	int file_length;

	file_in = fopen(file_path, "r");

	fseek(file_in, 0L, SEEK_END);
	file_length = (int)ftell(file_in);
	file_contents = calloc(file_length, sizeof(unsigned char));
	fseek(file_in, 0L, SEEK_SET);
	fread(file_contents, sizeof(unsigned char), file_length, file_in);
	fclose(file_in);

	int hash = 0;
	while(*file_path != '\0') {
		hash = (hash + (unsigned char)*file_path) % 1000;
		file_path++;
	}
	for(int i = 0; i< file_length; i++) {
		hash = (hash + (unsigned char)file_contents[i]) % 2000000000;
	}

	free(file_contents);
	return hash;
}


struct change_array *change_array_init() {
	struct change_array *ca = malloc(sizeof(struct change_array));
	ca->size = 0;
	ca->capacity = 1;
	ca->data = malloc(sizeof(struct change *));
	return ca;
}

void change_array_delete(struct change_array *ca) {
	for(int i = 0; i < ca->size; i++) {
		free(ca->data[i]->file_name);
		free(ca->data[i]);
	}
	free(ca->data);
	ca->data = NULL;
	free(ca);
	ca = NULL;
	return;
}

void change_append(struct change_array *array, struct change *element) {
	if(array->size == array->capacity) {
		array->capacity = (array->capacity)*2;
		array->data = realloc(array->data, sizeof(struct change *)*(array->capacity));
		array->data[array->size] = element;
		array->size += 1;
	} else {
		array->data[array->size] = element;
		array->size += 1;
	}
}

void change_remove(struct change_array *array, struct change *element) {
	for(int i = 0; i < array->size; i++) {
		if(array->data[i] == element) {
			// Delete change object.
			free(element->file_name);
			free(element);
			array->data[i] = NULL;
			for(int j = i; j< (array->size - 1); j++) {
				array->data[j] = array->data[j+1];
			}
			array->data[array->size-1] = NULL;
			array->size -= 1;
			if(array->size == 0) {
				return;
			} else {
				array->data = realloc(array->data, sizeof(struct change *)*array->size);
				array->capacity = array->size;
				return;
			}
		}
	}
	printf("change not in array\n");
	return;
}

//returns deep copy of specified file hash array.
struct file_hash_array *copy_file_hash(void *helper, struct file_hash_array *src) {

	struct file_hash_array *dest = malloc(sizeof(struct file_hash_array));
	memcpy(dest, src, sizeof(struct file_hash_array));

	// First we deep copy the string array (file_names).
	dest->file_names = malloc(sizeof(struct str_array));
	memcpy(dest->file_names, src->file_names, sizeof(struct str_array));
	dest->file_names->data = malloc(sizeof(char *)*(src->file_names->capacity));
	for(int i = 0; i < dest->file_names->size; i++) {
		dest->file_names->data[i] = strdup(src->file_names->data[i]);
	}

	// Second, we deep copy the int array (hash_list).
	dest->hash_list = malloc(sizeof(struct int_array));
	memcpy(dest->hash_list, src->hash_list, sizeof(struct int_array));
	dest->hash_list->data = malloc(sizeof(int)*src->hash_list->capacity);
	for(int i = 0; i < dest->hash_list->size; i++) {
		dest->hash_list->data[i] = src->hash_list->data[i];
	}
	return dest;
}

struct branch *branch_init(void *helper, char *name, struct commit *latest_commit) {
	struct helper *help = (struct helper *)helper;

	struct branch *branch = malloc(sizeof(struct branch));
	branch->name = strdup(name);
	branch->latest_commit = latest_commit;
	// Deep copy file_hash_array struct.
	branch->tracked_files = copy_file_hash(helper, help->HEAD->tracked_files);

	return branch;

}

struct branch_array *branch_array_init() {
	struct branch_array *ba = malloc(sizeof(struct branch_array));
	ba->size = 0;
	ba->capacity = 1;
	ba->data = malloc(sizeof(struct branch *));
	return ba;
}

void branch_array_delete(struct branch_array *ba) {
	free(ba->data);
	free(ba);
	ba = NULL;
	return;
}

void branch_append(struct branch_array *array, struct branch *element) {
	if(array->size == array->capacity) {
		array->capacity = (array->capacity)*2;
		array->data = realloc(array->data, sizeof(struct branch *)*(array->capacity));
		array->data[array->size] = element;
		array->size += 1;
	} else {
		array->data[array->size] = element;
		array->size += 1;
	}
}

struct commit_array *commit_array_init() {
	struct commit_array *ca = malloc(sizeof(struct commit_array));
	ca->size = 0;
	ca->capacity = 1;
	ca->data = malloc(sizeof(struct commit *));
	return ca;
}

void commit_array_delete(struct commit_array *ca) {
	free(ca->data);
	ca->data = NULL;
	free(ca);
	ca = NULL;
	return;
}

void commit_append(struct commit_array *array, struct commit *element) {
	if(array->size == array->capacity) {
		array->capacity = (array->capacity)*2;
		array->data = realloc(array->data, sizeof(struct commit *)*(array->capacity));
		array->data[array->size] = element;
		array->size += 1;
	} else {
		array->data[array->size] = element;
		array->size += 1;
	}
}

void commit_remove(struct commit_array *array, struct commit *element) {
	for(int i = 0; i < array->size; i++) {
		if(array->data[i] == element) {
			for(int j = i; j< (array->size - 1); j++) {
				array->data[j] = array->data[j+1];
			}
			array->data[array->size-1] = NULL;
			array->size -= 1;
			array->data = realloc(array->data, sizeof(struct commit *)*array->size);
			array->capacity = array->size;
			return;
		}
	}
	printf("commit not in array\n");
	return;
}


struct int_array *int_init() {
	struct int_array *ia = malloc(sizeof(struct int_array));
	ia->size = 0;
	ia->capacity = 1;
	ia->data = malloc(sizeof(int));
	return ia;
}

void int_delete(struct int_array *ia) {
	free(ia->data);
	ia->data = NULL;
	free(ia);
	ia = NULL;
	return;
}

void int_append(struct int_array *array, int element) {
	if(array->size == array->capacity) {
		array->capacity = (array->capacity)*2;
		array->data = realloc(array->data, sizeof(int)*(array->capacity));
		array->data[array->size] = element;
		array->size += 1;
	} else {
		array->data[array->size] = element;
		array->size += 1;
	}
}

void int_remove(struct int_array *array, int element) {
	for(int i = 0; i < array->size; i++) {
		if(array->data[i] == element) {
			for(int j = i; j < (array->size - 1); j++) {
				array->data[j] = array->data[j+1];
			}
			array->size -= 1;
			array->data = realloc(array->data, sizeof(int)*array->size);
			array->capacity = array->size;
			return;
		}
	}
	printf("element not in array\n");
	return;
}




struct str_array *str_init() {
	struct str_array *sa = malloc(sizeof(struct str_array));
	sa->size = 0;
	sa->capacity = 1;
	sa->data = malloc(sizeof(char *));
	return sa;
}

void str_delete(struct str_array *sa) {
	for(int i = 0; i < sa->size; i++) {
		free(sa->data[i]);
		sa->data[i] = NULL;
	}
	free(sa->data);
	sa->data = NULL;
	free(sa);
	sa = NULL;
	return;
}

void str_append(struct str_array *array, char *element) {
	if(array->size == array->capacity) {
		array->capacity = (array->capacity)*2;
		array->data = realloc(array->data, sizeof(char *)*(array->capacity));
		array->data[array->size] = malloc(sizeof(char)*strlen(element)+1);
		strcpy(array->data[array->size], element);
		array->size += 1;
	} else {
		array->data[array->size] = malloc(sizeof(char)*strlen(element)+1);
		strcpy(array->data[array->size], element);
		array->size += 1;
	}
}

void str_remove(struct str_array *array, char *element) {
	for(int i = 0; i < array->size; i++) {
		if(!strcmp(array->data[i], element)) {

			for(int j = i; j< (array->size - 1); j++) {
				free(array->data[j]);
				array->data[j] = NULL;
				array->data[j] = malloc(sizeof(char)*strlen(array->data[j+1])+1);
				strcpy(array->data[j], array->data[j+1]);
			}
			free(array->data[array->size-1]);
			array->data[array->size-1] = NULL;
			array->size -= 1;
			array->data = realloc(array->data, sizeof(char *)*array->size);
			array->capacity = array->size;
			return;
		}
	}
	printf("element not in array\n");
	return;
}



struct file_hash_array *file_hash_init() {
	struct file_hash_array *fh_array = malloc(sizeof(struct file_hash_array));
	fh_array->file_names = str_init();
	fh_array->hash_list = int_init();
	fh_array->size = 0;
	return fh_array;
}

int file_hash_delete(struct file_hash_array *fh_array) {
	if(fh_array == NULL) {
		return -1;
	}
	str_delete(fh_array->file_names);
	int_delete(fh_array->hash_list);
	free(fh_array);
	fh_array = NULL;
	return 0;
}

int file_hash_append(struct file_hash_array *fh_array, int hash, char *file_name) {
	if(fh_array==NULL) {
		return -1;
	}
	if(file_name == NULL) {
		return -2;
	}
	str_append(fh_array->file_names, file_name);
	int_append(fh_array->hash_list, hash);
	fh_array->size += 1;
	return 0;
}

int file_hash_remove(struct file_hash_array *fh_array, int hash, char *file_name) {
	if(fh_array == NULL) {
		return -1;
	}
	if(file_name == NULL) {
		return -2;
	}
	str_remove(fh_array->file_names, file_name);
	int_remove(fh_array->hash_list, hash);
	fh_array->size -= 1;
	return 0;
}

int get_hash(struct file_hash_array *fh_array, char *file_name) {
	if(file_name == NULL) {
		return -1;
	}

	for(int i = 0; i < fh_array->size; i++) {
		if(!strcmp(file_name, fh_array->file_names->data[i])) {
			return fh_array->hash_list->data[i];
		}
	}

	// FILE_NAME NOT IN fh_array.
	return -2;
}

int update_hash(struct file_hash_array *fh_array, int hash, char *file_name) {
	if(file_name == NULL) {
		return -1;
	}

	for(int i = 0; i < fh_array->size; i++) {
		if(!strcmp(file_name, fh_array->file_names->data[i])) {
			fh_array->hash_list->data[i] = hash;
			return 0;
		}
	}

	// FILE_NAME NOT IN fh_array.
	return -2;
}


void *svc_init() {
	struct helper *helper = malloc(sizeof(struct helper));


	// Initiate branches array, commits array, master branch and HEAD pointer.
	helper->branches = branch_array_init();
	helper->commits = commit_array_init();
	helper->change_list = change_array_init();

	//Manually init master branch.
	struct branch *master = malloc(sizeof(struct branch));
	master->name = strdup("master");
	master->latest_commit = NULL;
	master->tracked_files = file_hash_init();
	branch_append(helper->branches, master);

	helper->HEAD = helper->branches->data[0];

	// Make Directory
	int result = mkdir(".svc", 0777);
	if(result==-1) {
		printf("%s\n", strerror(errno));
	}

	return (void *) helper;
}


void delete_commit(struct commit *commit) {
	free(commit->commit_id);
	free(commit->branch_name);
	free(commit->commit_message);
	file_hash_delete(commit->file_hash_array);
	change_array_delete(commit->changes);
	commit_array_delete(commit->parents);
	free(commit);
	commit = NULL;
}
void delete_branch(struct branch *branch) {
	free(branch->name);
	file_hash_delete(branch->tracked_files);
	free(branch);
	branch = NULL;
}
/* Frees all dynamically allocated memory.
 */
void cleanup(void *helper) {
	struct helper *help = (struct helper *)helper;
	//also deletes change structs themselves.
	change_array_delete(help->change_list);


	for(int i = 0; i < help->branches->size; i++) {
		delete_branch(help->branches->data[i]);
		help->branches->data[i] = NULL;
	}
	branch_array_delete(help->branches);
	help->branches = NULL;

	for(int i = 0; i < help->commits->size; i++) {
		delete_commit(help->commits->data[i]);
		help->commits->data[i] = NULL;
	}
	commit_array_delete(help->commits);
	help->commits = NULL;
	free(help);
}


/* Removes file from current branches tracked_files and returns the
 * files last known hash id.
 */
int svc_rm(void *helper, char *file_name) {
	 if(file_name == NULL) {
		 return -1;
	 }
	 struct helper *help = (struct helper *) helper;

	 // Check that file_name is currently being tracked.
	 if(!is_in(helper, file_name, help->HEAD->tracked_files)) {
		 return -2;
	 }

	 int hash = get_hash(help->HEAD->tracked_files, file_name);

	 // Check if this file was added after the last commit
	 for(int i = 0; i< help->change_list->size; i++) {
		 struct change *change = help->change_list->data[i];

		 if(!strcmp(change->file_name, file_name) && change->type == ADDITION) {
			 // File has been added after last commit.
			 // We delete the previous 'add change' and untrack the file.
			 change_remove(help->change_list, change);
			 file_hash_remove((help->HEAD)->tracked_files, hash, file_name);
			 return hash;
		 }
	 }

	 // Add change
	 struct change *change = malloc(sizeof(struct change));
	 change->file_name = strdup(file_name);
	 change->type = REMOVAL;
	 change_append(help->change_list, change);

	 // Untrack file.
	 file_hash_remove((help->HEAD)->tracked_files, hash, file_name);
	 return hash;
}


/* Adds file_name to helper add_list and to Head branch tracked_files.
 * returns file hash id.
 */
int svc_add(void *helper, char *file_name) {
	if(file_name == NULL) {
		return -1;
	}
	struct helper *help = (struct helper *) helper;

	// Check if file is already being tracked.
	if(is_in(helper, file_name, help->HEAD->tracked_files)) {
		return -2;
	}

	// Confirm file exists.
	if(access(file_name, F_OK) == -1) {
		return -3;
	}
	int hash = hash_file(helper, file_name);

	 // Check if this file was removed after the last commit, meaning we must remove
	// the original 'remove' change and not create another change.
	for(int i = 0; i< help->change_list->size; i++) {
		struct change *change = help->change_list->data[i];

		if(!strcmp(change->file_name, file_name) && change->type == REMOVAL) {
			// File has been removed after last commit.
			change_remove(help->change_list, change);
			file_hash_append((help->HEAD)->tracked_files, hash, file_name);
			return hash;
		}
	}

	// Add change.
	struct change *change = malloc(sizeof(struct change));
	change->file_name = strdup(file_name);
	change->type = ADDITION;
	change_append(help->change_list, change);

	file_hash_append((help->HEAD)->tracked_files, hash, file_name);
	return hash;
}

// Used for get_commit_id
void quick_sort(struct change_array *changes, int left, int right) {
	int i = left;
	int j = right;
	struct change *x = changes->data[(left+right)/2];
	struct change *temp = NULL;

	do {
		while((strcasecmp(changes->data[i]->file_name, x->file_name)<0) && (i<right)) {
			i++;
		}
		while((strcasecmp(changes->data[j]->file_name, x->file_name) > 0) && (j > left)) {
			j--;
		}
		if(i <= j) {
			temp = changes->data[i];
			changes->data[i] = changes->data[j];
			changes->data[j] = temp;
			i++;
			j--;
		}
	} while(i <= j);

	if(left < j) {
 		quick_sort(changes, left, j);
	}
	if(i < right) {
 		quick_sort(changes, i, right);
	}
}


// Computes commit id.
char *get_commit_id(struct change_array *changes, char *message) {
	char *hex_string = malloc(sizeof(char)*7);

	quick_sort(changes, 0, changes->size-1);

	int id = 0;
	while(*message != '\0') {
		id = (id + (unsigned char)*message) % 1000;
		message++;
	}

	for(int i = 0; i<changes->size; i++) {
		int change_type = changes->data[i]->type;

		if(change_type == ADDITION) {
			id += 376591;
		}
		if(change_type == REMOVAL) {
			id += 85973;
		}
		if(change_type == MOD) {
			id += 9573681;
		}

		char *file_name = changes->data[i]->file_name;
		while(*file_name != '\0') {
			id = (id * ((unsigned char)*file_name % 37)) % 15485863 + 1;
			file_name++;
		}
	}
	sprintf(hex_string, "%06x", id);
	return hex_string;
}


// Loads a file with name hash into at it's original path,
void load_duplicate(char *hash, char *file_name) {
	char repo_path[256] = ".svc/";
	char *save_file_name = strcat(repo_path, hash);
	char *load_path = strdup(file_name);

	// Confirm file exists in repo.
	if(access(save_file_name, F_OK) == -1) {
		printf("file in repo does not exist load duplicate.");
	}

	// Check if file still exists in workspace, if so we must replace it.
	if(access(file_name, F_OK ) != -1) {
		int res = 0;
		res = remove(file_name);
		if(res == -1) {
			printf("remove error load_duplicate.");
		}
	}

	int dest = creat(load_path, 0666);
	int src = open(save_file_name, O_RDONLY);

	struct stat buff;
	fstat(src, &buff);
	int result = sendfile(dest, src, NULL, buff.st_size);
	if(result<0) {
		printf("sendfile err: return: %d, errno: %d\n", result, errno);
	}
	close(dest);
	close(src);
	free(load_path);
}


// Creates a file with name hash in .svc repo,
void save_duplicate(char *hash, char *file_name) {
	char repo_path[256] = ".svc/";
	char *save_file_name = strcat(repo_path, hash);

	int dest = creat(save_file_name, 0666);
	int src = open(file_name, O_RDONLY);

	struct stat buff;
	fstat(src, &buff);
	int result = sendfile(dest, src, NULL, buff.st_size);

	if(result<0) {
		printf("sendfile err: return: %d, errno: %d\n", result, errno);
	}
	close(dest);
	close(src);
}



struct commit *init_commit(void *helper, char *message) {
	struct helper *help = (struct helper *)helper;

	// New commit struct on current branch.
	struct commit *commit = malloc(sizeof(struct commit));
	commit->branch_name = strdup(help->HEAD->name);
	commit->commit_message = strdup(message);
	commit->file_hash_array = copy_file_hash(helper, help->HEAD->tracked_files);
	// we don't deep copy the changes list, we just pass over the pointer value from the helper to the commit.
	commit->changes = help->change_list;
	commit->parents = commit_array_init();
	if(help->HEAD->latest_commit != NULL) {
		commit_append(commit->parents, help->HEAD->latest_commit);
	}
	// Compute commit id;
	char *id = get_commit_id(commit->changes, message);
	commit->commit_id = strdup(id);
	free(id);

	return commit;
}

char *commit_modified(void *helper, char *message) {
	struct helper *help = (struct helper *)helper;

	// New commit object with duplicate file_hash_array.
	struct commit *commit = init_commit(helper, message);

	// Add commit to helper's commit array;
	commit_append(help->commits, commit);


	// Set this branches latest commit.
	help->HEAD->latest_commit = commit;

	// Save files with hash as their name.
	for(int i = 0; i < commit->file_hash_array->size; i++) {
		// Check if file already exists
		char hash_string[100];
		sprintf(hash_string,"%d", commit->file_hash_array->hash_list->data[i]);
		if(access(hash_string, F_OK ) == -1 ) {
			// File does not exist, we create a new copy of this file.
			save_duplicate(hash_string, commit->file_hash_array->file_names->data[i]);
		}
	}

	// Free helper's change_list POINTER
	help->change_list = change_array_init();

	return commit->commit_id;
}

char *svc_commit(void *helper, char *message) {
	struct helper *help = (struct helper *)helper;
	struct file_hash_array *curr_tracked_files = (help->HEAD)->tracked_files;

	// check if tracked files have been removed manually.
	for(int i = 0; i < curr_tracked_files->size; i++) {
		if(access(curr_tracked_files->file_names->data[i], F_OK ) == -1) {
			// File no longer exists.
			char *file_name = curr_tracked_files->file_names->data[i];
			svc_rm(helper, file_name);
		}
	}

	// Check if files have been modified.
	if ((help->HEAD)->latest_commit != NULL) {
		struct file_hash_array *prev_commit_tracked = (help->HEAD)->latest_commit->file_hash_array;

		// Compare current hash and hash from last commit of each file that was
		// tracked in last commit and remains tracked.
		for(int i = 0; i < curr_tracked_files->size; i++) {
			char *current_file = curr_tracked_files->file_names->data[i];

			for(int j = 0; j < prev_commit_tracked->size; j++) {
				char *prev_file = prev_commit_tracked->file_names->data[j];

				if(!strcmp(prev_file, current_file)) {
					int hash_prev = get_hash(prev_commit_tracked, current_file);
					int current_hash = hash_file(helper, current_file);

					if(hash_prev != current_hash) {
						//file has been modified.
						struct change *change = malloc(sizeof(struct change));
						change->file_name = strdup(current_file);
						change->type = MOD;
						change_append(help->change_list, change);
						update_hash(curr_tracked_files, current_hash, current_file);
					}
				}
			}
		}
	}

	// Update hash of files that have been added after last commit.
	for(int i = 0; i < help->change_list->size; i++) {
		if(help->change_list->data[i]->type == ADDITION) {
			int new_hash = hash_file(helper, help->change_list->data[i]->file_name);
			update_hash(curr_tracked_files, new_hash, help->change_list->data[i]->file_name);
		}
	}
	// There have been no changes or message is null.
	if(help->change_list->size == 0 || message == NULL) {
		return NULL;
	}

	// New commit object.
	struct commit *commit = init_commit(helper, message);
	commit_append(help->commits, commit);
	help->HEAD->latest_commit = commit;

	// Save commited files to repo with hash as their name.
	for(int i = 0; i < commit->file_hash_array->size; i++) {
		char hash_string[100];
		sprintf(hash_string,"%d", commit->file_hash_array->hash_list->data[i]);
		if(access(hash_string, F_OK ) == -1 ) {
			// File does not exist, we create a new copy of this file.
			save_duplicate(hash_string, commit->file_hash_array->file_names->data[i]);
		}
	}

	// Reset change list.
	help->change_list = change_array_init();

	return commit->commit_id;
}

void *get_commit(void *helper, char *commit_id) {
	struct helper *help = (struct helper *)helper;

	if(commit_id == NULL) {
		return NULL;
	}
	for(int i = 0; i < help->commits->size; i++) {
		if(!strcmp(help->commits->data[i]->commit_id, commit_id)) {
			return help->commits->data[i];
		}
	}
	return NULL;
}

char **get_prev_commits(void *helper, void *commit, int *n_prev) {
	struct commit *com = (struct commit *)commit;
	if(n_prev == NULL) {
		return NULL;
	}
	if(commit == NULL) {
		*n_prev = 0;
		return NULL;
	}
	if(com->parents->size == 0) {
		*n_prev = 0;
		return NULL;
	}

	char **result = malloc(sizeof(struct commit *)*(com->parents->size));
	for(int i = 0; i < com->parents->size; i++) {
		result[i] = (com->parents->data)[i]->commit_id;
	}
	*n_prev = com->parents->size;
	return result;


}

void print_commit(void *helper, char *commit_id) {
	struct commit *com = get_commit(helper, commit_id);
	if(com == NULL || commit_id == NULL) {
		printf("Invalid commit id\n");
		return;
	}

	char title[300];
	sprintf(title, "%s [%s]: %s", com->commit_id, com->branch_name, com->commit_message);
	printf("%s\n", title);

	// Print changes.
	for(int i = 0; i < com->changes->size; i++) {
		if((com->changes->data)[i]->type == ADDITION) {
			printf("    + %s\n", (com->changes->data)[i]->file_name);
		}
		if((com->changes->data)[i]->type == REMOVAL) {
			printf("    - %s\n", (com->changes->data)[i]->file_name);
		}
		if((com->changes->data)[i]->type == MOD) {
			struct commit *prev_commit = (com->parents->data)[0];
			int prev_hash = get_hash(prev_commit->file_hash_array, (com->changes->data)[i]->file_name);
			int current_hash = get_hash(com->file_hash_array, (com->changes->data)[i]->file_name);
			printf("    / %s [%10d -> %10d]\n", (com->changes->data)[i]->file_name, prev_hash, current_hash);
		}
	}

	printf("\n    Tracked files (%d):\n", com->file_hash_array->size);
	for(int i = 0; i<com->file_hash_array->size; i++) {
		printf("    [%10d] %s\n", get_hash(com->file_hash_array, (com->file_hash_array->file_names->data)[i]), (com->file_hash_array->file_names->data)[i]);
	}
}

int svc_branch(void *helper, char *branch_name) {
		if(branch_name == NULL) {
			return -1;
		}
		// Make sure branch_name is valid string.
		regex_t my_regex;
		int comp_return;
		int is_valid = 0;

		comp_return = regcomp(&my_regex, "[^A-Za-z0-9/_-]", 0);
		if(comp_return) {
			printf("Could not compile regex\n");
			regfree(&my_regex);
			return -4;
		}

		comp_return = regexec(&my_regex, branch_name, 0, NULL, 0);
		if(!comp_return) {
			regfree(&my_regex);
			return -1;
		} else if(comp_return == REG_NOMATCH) {
			is_valid = 1;
		} else {
			printf("regex error\n");
			exit(1);
		}

		if(!is_valid) {
			regfree(&my_regex);
			return -1;
		}
		regfree(&my_regex);

		struct helper *help = (struct helper *)helper;

		// Check if branch_name already exists.
		for(int i = 0; i<help->branches->size; i++) {
			if(!strcmp(help->branches->data[i]->name, branch_name)) {
				return -2;
			}
		}

		// Check for uncomitted changes.
		if(help->change_list->size != 0) {
			return -3;
		}

		// Check if files have been modified.

		if ((help->HEAD)->latest_commit != NULL) {
			struct file_hash_array *prev_commit_tracked = (help->HEAD)->latest_commit->file_hash_array;
			struct file_hash_array *curr_tracked_files = (help->HEAD)->tracked_files;
			// Compare current hash and hash from last commit of each file that was
			// tracked in last commit and remains tracked.
			for(int i = 0; i < curr_tracked_files->size; i++) {
				char *current_file = curr_tracked_files->file_names->data[i];

				for(int j = 0; j < prev_commit_tracked->size; j++) {
					char *prev_file = prev_commit_tracked->file_names->data[j];

					if(!strcmp(prev_file, current_file)) {
						int hash_prev = get_hash(prev_commit_tracked, current_file);
						int current_hash = hash_file(helper, current_file);

						if(hash_prev != current_hash) {
							//file has been modified.
							return -3;
						}
					}
				}
			}
		}

		// Add new branch to branches array.
		struct branch *new_branch = branch_init(helper, branch_name, help->HEAD->latest_commit);
		branch_append(help->branches, new_branch);

		return 0;
}

int svc_checkout(void *helper, char *branch_name) {
	struct helper *help = (struct helper *)helper;
	if(branch_name == NULL) {
		return -1;
	}

	struct branch *temp_head = NULL;

	// Check if branch exists.
	int exists = 0;
	for(int i = 0; i<help->branches->size; i++) {
		if(!strcmp(help->branches->data[i]->name, branch_name)) {
			// Branch exists.
			temp_head = help->branches->data[i];
			exists = 1;
		}
	}
	if(!exists) {
		return -1;
	}

	// Check for uncomitted changes.
	if(help->change_list->size != 0) {
		return -2;
	}
	// Check if files have been modified by comparing hash to previous hash.
	if ((help->HEAD)->latest_commit != NULL) {
		struct file_hash_array *prev_commit_tracked = (help->HEAD)->latest_commit->file_hash_array;
		struct file_hash_array *curr_tracked_files = (help->HEAD)->tracked_files;

		for(int i = 0; i < curr_tracked_files->size; i++) {
			char *current_file = curr_tracked_files->file_names->data[i];

			for(int j = 0; j < prev_commit_tracked->size; j++) {
				char *prev_file = prev_commit_tracked->file_names->data[j];

				if(!strcmp(prev_file, current_file)) {
					int hash_prev = get_hash(prev_commit_tracked, current_file);
					int current_hash = hash_file(helper, current_file);

					if(hash_prev != current_hash) {
						//file has been modified.
						return -2;
					}
				}
			}
		}
	}
	help->HEAD = temp_head;
	// Duplicate files from .svc to their original filepath contained in the commit's file_hash_array.
	for(int i = 0; i < (help->HEAD)->latest_commit->file_hash_array->size; i++) {
		struct file_hash_array *prev_commit_tracked = (help->HEAD)->latest_commit->file_hash_array;
		char hash_string[100];
		sprintf(hash_string,"%d", prev_commit_tracked->hash_list->data[i]);
		load_duplicate(hash_string, prev_commit_tracked->file_names->data[i]);
	}
	return 0;
}

char **list_branches(void *helper, int *n_branches) {
	struct helper *help = (struct helper *)helper;
	if(n_branches == NULL) {
		return NULL;
	}


	char **result = malloc(sizeof(char *)*help->branches->size);

	for(int i = 0; i < help->branches->size; i++) {
		result[i] = help->branches->data[i]->name;
		printf("%s\n", result[i]);
	}

	*n_branches = help->branches->size;
	return result;

}



int svc_reset(void *helper, char *commit_id) {

	struct helper *help = (struct helper *)helper;
	if(commit_id == NULL) {
		return -1;
	}
	if(get_commit(helper, commit_id) == NULL) {
		return -2;
	}

	// Discard changes;
	change_array_delete(help->change_list);
	help->change_list = change_array_init();
	help->HEAD->latest_commit = get_commit(helper, commit_id);

	// duplicate commit's file_hash_array into HEADS tracked_files.
	file_hash_delete(help->HEAD->tracked_files);

	help->HEAD->tracked_files =copy_file_hash(helper, help->HEAD->latest_commit->file_hash_array);

	// Duplicate files from .svc to their original filepath contained in the commit's file_hash_array.
	for(int i = 0; i < help->HEAD->latest_commit->file_hash_array->size; i++) {
		char hash_string[100];
		sprintf(hash_string,"%d", help->HEAD->latest_commit->file_hash_array->hash_list->data[i]);
		load_duplicate(hash_string, help->HEAD->latest_commit->file_hash_array->file_names->data[i]);
	}
	return 0;


}



char *svc_merge(void *helper, char *branch_name, struct resolution *resolutions, int n_resolutions) {
	struct helper *help = (struct helper *)helper;
	if(branch_name == NULL) {
		printf("Invalid branch name\n");
		return NULL;
	}
	int found = 0;
	// Find pointer to target branch;
	struct branch *target_branch = NULL;
	for(int i = 0; i < help->branches->size; i++) {
		if(!strcmp(help->branches->data[i]->name, branch_name)) {
			target_branch = help->branches->data[i];
			found = 1;
			break;
		}
	}
	if(!found) {
		printf("Branch not found\n");
		return NULL;
	}
	if(!strcmp(branch_name, help->HEAD->name)) {
		printf("Cannot merge a branch with itself\n");
		return NULL;
	}

	// Check for uncomitted changes.
	if(help->change_list->size != 0) {
		printf("Changes must be committed\n");
		return NULL;
	}

	// Check if files have been modified by comparing hash to previous hash.
	if ((help->HEAD)->latest_commit != NULL) {
		struct file_hash_array *prev_commit_tracked = (help->HEAD)->latest_commit->file_hash_array;
		struct file_hash_array *curr_tracked_files = (help->HEAD)->tracked_files;

		for(int i = 0; i < curr_tracked_files->size; i++) {
			char *current_file = curr_tracked_files->file_names->data[i];

			for(int j = 0; j < prev_commit_tracked->size; j++) {
				char *prev_file = prev_commit_tracked->file_names->data[j];

				if(!strcmp(prev_file, current_file)) {
					int hash_prev = get_hash(prev_commit_tracked, current_file);
					int current_hash = hash_file(helper, current_file);

					if(hash_prev != current_hash) {
						printf("Changes must be committed\n");
						return NULL;
					}
				}
			}
		}
	}
	// Update hash of files that have been added after last commit.
	for(int i = 0; i < help->change_list->size; i++) {
		if(help->change_list->data[i]->type == ADDITION) {
			int new_hash = hash_file(helper, help->change_list->data[i]->file_name);
			update_hash((help->HEAD)->tracked_files, new_hash, help->change_list->data[i]->file_name);
		}
	}

	// Iterate through resolutions array.
	for(int i = 0; i < n_resolutions; i++) {
		struct file_hash_array *curr_tracked_files = (help->HEAD)->tracked_files;

		if(resolutions[i].resolved_file == NULL) {
			if(is_in(helper, resolutions[i].file_name, curr_tracked_files)) {
				svc_rm(helper, resolutions[i].file_name);
			}

		} else {

			if(is_in(helper, resolutions[i].file_name, curr_tracked_files)) {
				// resolution file is in current branch.
				int hash_prev = hash_file(helper, resolutions[i].file_name);

				// Replace file
				char *res_path = resolutions[i].resolved_file;
				char *load_path = resolutions[i].file_name;
				// Delete currently tracked conflicitng file.
				int res = 0;
				res = remove(resolutions[i].file_name);
				if(res == -1) {
					printf("remove error merge.");
				}
				int dest = creat(load_path, 0666);
				int src = open(res_path, O_RDONLY);
				struct stat buff;
				fstat(src, &buff);
				int result = sendfile(dest, src, NULL, buff.st_size);
				if(result<0) {
					printf("sendfile err: return: %d, errno: %d\n", result, errno);
				}
				close(dest);
				close(src);

				// Check if resolution file has same contents as the file in active branch, in which case there is no modification.
				int hash_post = hash_file(helper, resolutions[i].file_name);
				if(hash_post != hash_prev) {
					// Add modification change.
					struct change *change = malloc(sizeof(struct change));
					change->file_name = strdup(resolutions[i].file_name);
					change->type = MOD;
					change_append(help->change_list, change);
					update_hash(curr_tracked_files, hash_post, resolutions[i].file_name);
				}


			} else {
				// resolution file only in target branch.
				// Replace file
				char *res_path = resolutions[i].resolved_file;
				char *load_path = resolutions[i].file_name;
				// Delete currently tracked conflicitng file.
				int res = 0;
				res = remove(resolutions[i].file_name);
				if(res == -1) {
					printf("remove error merge.");
				}
				int dest = creat(load_path, 0666);
				int src = open(res_path, O_RDONLY);
				struct stat buff;
				fstat(src, &buff);
				int result = sendfile(dest, src, NULL, buff.st_size);
				if(result<0) {
					printf("sendfile err: return: %d, errno: %d\n", result, errno);
				}
				close(dest);
				close(src);

				// Add to master branch.
				svc_add(helper, load_path);
			}
		}
	}

	// Iterate thorugh target branch files.
	for(int i = 0; i < target_branch->tracked_files->size; i++) {
		char *current = target_branch->tracked_files->file_names->data[i];
		int found = 0;
		for(int j = 0; j < n_resolutions; j++) {
			if(!strcmp(current, resolutions[j].file_name)) {
				found = 1;
				break;
			}
		}
		if(!found) {
			if(!is_in(helper, current, help->HEAD->tracked_files)) {
				// Confirm file exists.
				if( access(current, F_OK ) == -1 ) {
					// no longer exists, restore file in target branch.
					int hash = get_hash(target_branch->latest_commit->file_hash_array, current);
					char hash_string[100];
					sprintf(hash_string,"%d", hash);
					load_duplicate(hash_string, current);
				}
				svc_add(helper, current);

			}
		}
	}
	char message[260];
	sprintf(message, "Merged branch %s", branch_name);

	char *id = commit_modified(helper, message);
	struct commit *com = get_commit(helper, id);
	commit_append(com->parents, target_branch->latest_commit);
	printf("Merge successful\n");
	return id;

}
