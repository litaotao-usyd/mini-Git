#ifndef svc_h
#define svc_h

#include <stdlib.h>

typedef struct resolution {
    // NOTE: DO NOT MODIFY THIS STRUCT
    char *file_name;
    char *resolved_file;
} resolution;

struct file {
    char path[251];
    int hash;
    struct file* next;

    struct file* _next;
};
typedef struct file file;

struct commit {
    char* id;
    char message[251];
    int cnt;
    struct file* tracked;
    struct commit* prev;

    struct commit* _next;
};
typedef struct commit commit;

typedef struct branch {
    char name[61];
    struct commit* current;
    struct branch* next;
} branch;
typedef struct branch branch;

struct svc {
    struct commit* head;
    struct branch* branches;
    struct branch* current_branch;
    int branches_cnt;
    struct file* tracked;

    struct commit* _commits;
    struct file* _files;
};
typedef struct svc svc;

void* malloc_commit(void* helper);
void* malloc_file(void* helper);
void free_commit(void* helper);
void free_file(void* helper);

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

