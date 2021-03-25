#ifndef svc_h
#define svc_h

#include <stdlib.h>



struct int_array {
	int size;
	int capacity;
	int *data;
};

struct str_array {
	int size;
	int capacity;
	char **data;
};

struct file_hash_array {
	struct str_array *file_names;
	struct int_array *hash_list;
	int size;
};

/* ONLY HOLDS POINTERS TO COMMIT OBJECTS THUS ON DELETION/REMOVAL NO ACTUAL COMMITS ARE DELETED,
   JUST THE REFERENCES THAT THIS ARRAY HOLDS.*/
struct commit_array {
	int size;
	int capacity;
	struct commit **data;
};

struct change_array {
	int size;
	int capacity;
	struct change **data;
};

// This array will pointer.
struct branch_array {
	int size;
	int capacity;
	struct branch **data;
};

// We do malloc file_name.
struct change {
	char *file_name;
	int type;
};




struct commit {
	char *commit_id;
	char *branch_name;
	char *commit_message;

	int num_tracked_files;

	struct file_hash_array *file_hash_array;
	struct change_array *changes;

	struct commit_array *parents;
};

struct branch {
	struct commit *latest_commit;
	char *name;

	struct file_hash_array *tracked_files;

};

struct helper {

	struct branch *HEAD;
	struct branch_array *branches;
	struct change_array *change_list;

	int num_current_tracked;

	// This array of pointers to commit objects is only used for cleanup.
	struct commit_array *commits;

};

typedef struct resolution {
    // NOTE: DO NOT MODIFY THIS STRUCT
    char *file_name;
    char *resolved_file;
} resolution;

void *svc_init(void);

void cleanup(void *helper);

int hash_file(void *helper, char *file_path);

char *svc_commit(void *helper, char *message);

void *get_commit(void *helper, char *commit_id);

char **get_prev_commits(void *helper, void *commit, int *n_prev);

void print_commit(void *helper, char *commit_id);

int svc_branch(void *helper, char *branch_name);

int svc_checkout(void *helper, char *branch_name);

char **list_branches(void *helper, int *n_branches);

int svc_add(void *helper, char *file_name);

int svc_rm(void *helper, char *file_name);

int svc_reset(void *helper, char *commit_id);

char *svc_merge(void *helper, char *branch_name, resolution *resolutions, int n_resolutions);

#endif
