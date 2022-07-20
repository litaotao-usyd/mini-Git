#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "svc.h"

void* malloc_commit(void* helper) {
    svc* s = (svc*)helper;
    commit* c = (commit*)malloc(sizeof(commit));

    c->_next = s->_commits;
    s->_commits = c;

    return c;
}

void* malloc_file(void* helper) {
    svc* s = (svc*)helper;
    file* f = (file*)malloc(sizeof(file));
    f->_next = s->_files;
    s->_files = f;

    return f;
}

void free_commit(void* helper) {
    svc* s = (svc*)helper;
    commit* c = (commit*)s->_commits;
    while (c!=NULL) {
        commit* n = c->_next;
        free(c->id);
        free(c);
        c = n;
    }
}

void free_file(void* helper) {
    svc* s = (svc*)helper;
    file* f = (file*)s->_files;
    while (f!=NULL) {
        file* n = f->_next;
        free(f);
        f = n;
    }
}

void *svc_init(void) {
    branch* master = (branch*)malloc(sizeof(branch));
    strcpy(master->name, "master");
    master->current = NULL;
    master->next = NULL;

    svc* helper = (svc*)malloc(sizeof(svc));
    helper->_commits = NULL;
    helper->_files = NULL;
    helper->head = NULL;
    helper->branches=master;
    helper->current_branch=master;
    helper->branches_cnt = 1;
    helper->tracked = NULL;
    return (void*)helper;
}

void cleanup(void *helper) {
    svc* s = (svc*)helper;

    free_commit(helper);
    free_file(helper);

    branch* b = s->branches;
    while (b!=NULL) {
        branch* n = b->next;
        free(b);
        b = n;
    }

    free(s);
}

int hash_file(void *helper, char *file_path) {
    if (file_path==NULL) {
        return -1;
    }

    char *buffer = NULL;
    FILE* fp = fopen(file_path, "r");
    size_t size = 0;
    if (fp == NULL) {
        return -2;
    }

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    rewind(fp);
    buffer = malloc((size) * sizeof(*buffer));
    fread(buffer, size, 1, fp);
    fclose(fp);

    int hash = 0;
    for (int i=0; i<strlen(file_path); i++) {
        hash = (hash + (unsigned char)file_path[i]) % 1000;
    }
    for (int i=0; i<size; i++) {
        hash = (hash + (unsigned char)buffer[i]) % 2000000000;
    }

    free(buffer);

    return hash;
}

void compare_commmit(void *helper, file* last, file* current, file** added, file** removed, file** changed) {
    svc* s = (svc*)helper;
    file* f = NULL;    
    f = current;
    while (f != NULL) {
        file* tmpf = last;
        while (tmpf != NULL) {
            if (strcmp(tmpf->path, f->path) == 0) {
                break;
            }
            tmpf = tmpf->next;
        }
        if (tmpf == NULL) {
            file* newf = malloc_file(s);
            strcpy(newf->path, f->path);
            newf->hash = f->hash;
            newf->next = *added;
            *added = newf;
        } else {
            if (tmpf->hash != f->hash) {
                file* newf = malloc_file(s);
                strcpy(newf->path, f->path);
                newf->hash = f->hash;
                newf->next = *changed;
                *changed = newf;
            }
        }
        f = f->next;
    }

    f = last;
    while (f!=NULL) {
        file* tmpf = current;
        while (tmpf != NULL) {
            if (strcmp(tmpf->path, f->path) == 0) {
                break;
            }
            tmpf = tmpf->next;
        }
        if (tmpf==NULL) {
                file* newf = malloc_file(s);
                strcpy(newf->path, f->path);
                newf->next = *removed;
                *removed = newf;
        }
        f=f->next;
    }
}

struct node {
    char* name;
    int type; // 0 added 1 removed 2 changed;
};

typedef struct node Node;

int strcicmp(char const *a, char const *b)
{
    for (;; a++, b++) {
        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (d != 0 || !*a)
            return d;
    }
}

char* get_commit_id(char* message, file* added, file* removed, file* changed) {
    int id = 0;
    for (int i=0; i<strlen(message); i++) {
        id = (id + (unsigned char)message[i]) % 1000;
    }

    int cnt = 0;
    file *f = NULL;
    f = added;
    while (f!=NULL) {
        cnt++;
        f = f->next;
    }
    f = removed;
    while (f!=NULL) {
        cnt++;
        f = f->next;
    }
    f = changed;
    while (f!=NULL) {
        cnt++;
        f = f->next;
    }

    Node* node = malloc(sizeof(Node)*cnt);

    f = added;
    int i = 0;
    while (f!=NULL) {
        node[i].name = f->path;
        node[i].type = 0;
        i++;
        f = f->next;
    }
    f = removed;
    while (f!=NULL) {
        node[i].name = f->path;
        node[i].type = 1;
        i++;
        f = f->next;
    }
    f = changed;
    while (f!=NULL) {
        node[i].name = f->path;
        node[i].type = 2;
        i++;
        f = f->next;
    }

    char* tmpname;
    int tmptype;
    for (int i=0; i<cnt-1; i++) {
        for (int j=i+1; j<cnt; j++) {
            if (strcicmp(node[i].name, node[j].name)>0) {
                tmpname = node[i].name;
                node[i].name = node[j].name;
                node[j].name = tmpname;
                tmptype = node[i].type;
                node[i].type = node[j].type;
                node[j].type = tmptype;
            }
        }
    }

    for (int i=0; i<cnt; i++) {
        if (node[i].type == 0) id += 376591;
        if (node[i].type == 1) id += 85973;
        if (node[i].type == 2) id += 9573681;
        for (int j=0; j<strlen(node[i].name); j++) {
            id = (id * ((unsigned char)node[i].name[j]%37)) % 15485863 + 1;
        }
    }

    free(node);

    char* idstr = (char*)malloc(sizeof(char)*7);
    sprintf(idstr, "%06x", id);
    return idstr;
}

void rehash(void* helper) {
    svc* s = (svc*)helper;
    file* f = s->tracked;
    file* last = NULL;

    while (f!=NULL) {
        int h = hash_file(helper, f->path);
        if (h<0) {
            if (last==NULL) {
                s->tracked = f->next;
            } else {
                last->next = f->next;
            }
        } else {
            f->hash = h;
            last = f;
        }
        f=f->next;
    }
}

char *svc_commit(void *helper, char *message) {
    if (message == NULL) {
        return NULL;
    }

    svc* s = (svc*)helper;
    rehash(s);
    file* tracked = s->tracked;
    file* last = NULL;
    if (s->head != NULL) {
        last = s->head->tracked;
    }
    file* added = NULL;
    file* changed = NULL;
    file* removed = NULL;
    compare_commmit(s, last, tracked, &added, &removed, &changed);

    if ((added == NULL)&&(removed == NULL)&&(changed==NULL)) {
        return NULL;
    }

    file* newTracked = NULL;
    file* f = NULL;
    int cnt = 0;
    f = tracked;
    while (f!=NULL) {
        file* newf = malloc_file(s);
        strcpy(newf->path, f->path);
        newf->hash = f->hash;
        newf->next = newTracked;
        newTracked = newf;
        cnt++;
        f=f->next;
    }

    commit *c = malloc_commit(s);
    c->id = get_commit_id(message, added, removed, changed);
    c->tracked = newTracked;
    c->cnt = cnt;
    strcpy(c->message, message);
    c->prev = s->head;

    s->head = c;
    s->current_branch->current = c;

    return c->id;
}

void *get_commit(void *helper, char *commit_id) {
    svc* s = (svc*)helper;
    commit* c = s->head;
    while (c!= NULL) {
        if (strcmp(commit_id, c->id) == 0) {
            return c;
        }
        c = c->prev;
    }
    return NULL;
}

char **get_prev_commits(void *helper, void *_c, int *n_prev) {
    if (n_prev == NULL) {
        return NULL;
    }

    if (_c == NULL) {
        *n_prev = 0;
        return NULL;
    }

    // svc* s = (svc*)helper;
    commit* c = (commit*)_c;

    int cnt = 0;
    commit* tmpc = c->prev;
    while (tmpc!=NULL) {
        cnt++;
        tmpc = tmpc->prev;
    }

    *n_prev = cnt;

    if (cnt == 0) {
        return NULL;
    }

    char** result = (char**)malloc(sizeof(char*)*cnt);
    tmpc = c->prev;
    int i=cnt-1;
    while (tmpc!=NULL) {
        result[i] = tmpc->id;
        i--;
        tmpc = tmpc->prev;
    }

    return result;
}

void print_commit(void *helper, char *commit_id) {
    if (commit_id == NULL) {
        printf("Invalid commit id\n");
        return;
    }

    svc* s = (svc*)helper;
    commit* c = get_commit(helper, commit_id);
    if (c == NULL) {
        printf("Invalid commit id\n");
        return;
    }

    file* added = NULL;
    file* changed = NULL;
    file* removed = NULL;
    if (c->prev != NULL) {
        compare_commmit(s, c->prev->tracked, c->tracked, &added, &removed, &changed);
    } else {
        compare_commmit(s, NULL, c->tracked, &added, &removed, &changed);
    }

    printf("%s [%s]: %s\n", c->id, s->current_branch->name, c->message);
    file* f = NULL;
    f = added;
    while (f!=NULL) {
        printf("    + %s\n", f->path);
        f = f->next;
    }
    f = removed;
    while (f!=NULL) {
        printf("    - %s\n", f->path);
        f = f->next;
    }
    f = changed;
    while (f!=NULL) {
        printf("    / %s\n", f->path);
        f = f->next;
    }
    printf("\n");
    printf("    Tracked files (%d):\n", c->cnt);
    f = c->tracked;
    while (f!=NULL) {
        printf("    [%10d] %s\n", f->hash, f->path);
        f = f->next;
    }
    return;
}

int svc_branch(void *helper, char *branch_name) {
    if (branch_name == NULL) {
        return -1;
    }

    for (int i=0; i<strlen(branch_name); i++) {
        char c = branch_name[i];
        if ((c<'a')||(c>'z')) {
            if ((c<'A')||(c>'Z')) {
                if ((c<'0')||(c>'9')) {
                    if ((c!='_')&&(c!='/')&&(c!='-')) {
                        return -1;
                    }
                }
            }
        }
    }

    svc* s = (svc*)helper;
    branch* b = s->branches;
    while (b!=NULL) {
        if (strcmp(b->name, branch_name)==0) {
            break;
        }
        b = b->next;
    }
    if (b != NULL) {
        return -2;
    }

    file* added = NULL;
    file* changed = NULL;
    file* removed = NULL;
    file* last = NULL;
    if (s->head != NULL) {
        last = s->head->tracked;
    }
    compare_commmit(s, last, s->tracked, &added, &removed, &changed); 

    if ((added != NULL)||(removed != NULL)||(changed != NULL)) {
        return -3;
    }

    b = malloc(sizeof(branch));
    b->current = s->head;
    strcpy(b->name, branch_name);

    b->next = s->branches;
    s->branches_cnt++;
    s->branches = b;
    return 0;
}

int svc_checkout(void *helper, char *branch_name) {
    if (branch_name == NULL) {
        return -1;
    }

    svc* s = (svc*)helper;
    branch* b = s->branches;
    while (b!=NULL) {
        if (strcmp(b->name, branch_name)==0) {
            break;
        }
        b = b->next;
    }
    if (b==NULL) {
        return -1;
    }

    file* added = NULL;
    file* changed = NULL;
    file* removed = NULL;
    file* last = NULL;
    if (s->head != NULL) {
        last = s->head->tracked;
    }
    compare_commmit(s, last, s->tracked, &added, &removed, &changed); 

    if ((added != NULL)||(removed != NULL)||(changed != NULL)) {
        return -2;
    }

    s->current_branch = b;
    s->head = b->current;

    file* newTracked = NULL;
    if (s->head != NULL) {
        file* f = s->head->tracked;

        while (f!=NULL) {
            file* newf = malloc_file(s);
            newf->hash = f->hash;
            strcpy(newf->path, f->path);
            newf->next = newTracked;
            newTracked = newf;
            f = f->next;
        }
    }

    s->tracked = newTracked;

    return 0;
}

char **list_branches(void *helper, int *n_branches) {
    if (n_branches == NULL) {
        return NULL;
    }
    svc* s = (svc*)helper;
    *n_branches = s->branches_cnt;
    char** result = malloc(sizeof(char*)*s->branches_cnt);
    int i = s->branches_cnt-1;
    branch* b = s->branches;
    while (b!=NULL) {
        result[i] = b->name;
        i--;
        b = b->next;
    }

    for (int i=0; i<*n_branches; i++) {
        printf("%s\n", result[i]);
    }

    return result;
}

int svc_add(void *helper, char *file_name) {
    if (file_name == NULL) {
        return -1;
    }
    svc* s = (svc*)helper;

    file* f = NULL;
    f=s->tracked;
    while (f!=NULL) {
        if (strcmp(f->path, file_name) == 0) {
            break;
        }
        f=f->next;
    }

    if (f!=NULL) {
        return -2;
    }

    int hash = hash_file(s, file_name);
    if (hash < 0) {
        return -3;
    }

    f = malloc_file(s);
    strcpy(f->path, file_name);
    f->hash = hash;
    f->next = s->tracked;
    s->tracked = f;

    return hash;
}

int svc_rm(void *helper, char *file_name) {
    if (file_name == NULL) {
        return -1;
    }
    svc* s = (svc*)helper;

    file* f = NULL;
    file* last = NULL;
    f = s->tracked;
    while (f!=NULL) {
        if (strcmp(f->path, file_name) == 0) {
            break;
        }
        last = f;
        f=f->next;
    }
    if (f==NULL) {
        return -2;
    }

    if (last == NULL) {
        s->tracked = f->next;
    } else {
        last->next = f->next;
    }

    return f->hash;
}

int svc_reset(void *helper, char *commit_id) {
    if (commit_id == NULL) {
        return -1;
    }

    svc* s = (svc*)helper;
    commit* c = s->head;

    while (c!=NULL) {
        if (strcmp(c->id, commit_id) == 0) {
            break;
        }
        c = c->prev;
    }

    if (c==NULL) {
        return -2;
    }

    s->head = c;
    s->current_branch->current = c;
    
    file* newTracked = NULL;
    file* f = s->head->tracked;

    while (f!=NULL) {
        file* newf = malloc_file(s);
        newf->hash = f->hash;
        strcpy(newf->path, f->path);
        newf->next = newTracked;
        newTracked = newf;
        f = f->next;
    }

    s->tracked = newTracked;

    return 0;
}

char *svc_merge(void *helper, char *branch_name, struct resolution *resolutions, int n_resolutions) {
    if (branch_name == NULL) {
        printf("Invalid branch name\n");
        return NULL;
    }

    svc* s = (svc*)helper;
    branch* b = s->branches;
    while (b!=NULL) {
        if (strcmp(b->name, branch_name) == 0) {
            break;
        }
        b = b->next;
    }
    if (b==NULL) {
        printf("Branch not found\n");
        return NULL;
    }

    if (b == s->current_branch) {
        printf("Cannot merge a branch with itself\n");
        return NULL;
    }

    file* added = NULL;
    file* changed = NULL;
    file* removed = NULL;
    file* last = NULL;
    if (s->head != NULL) {
        last = s->head->tracked;
    }
    compare_commmit(s, last, s->tracked, &added, &removed, &changed); 

    if ((added != NULL)||(removed != NULL)||(changed != NULL)) {
        printf("Changes must be committed\n");
        return NULL;
    }

    compare_commmit(s, last, b->current->tracked, &added, &removed, &changed);
    
    file* newTracked = added;
    file* f = s->head->tracked;

    while (f!=NULL) {
        file* tmpf = changed;
        while (tmpf != NULL) {
            if (strcmp(tmpf->path, f->path) == 0) {
                break;
            }
            tmpf = tmpf->next;
        }

        if (tmpf != NULL) {
            f=f->next;
            continue;
        }

        file* newf = malloc_file(s);
        strcpy(newf->path, f->path);
        newf->hash = f->hash;
        newf->next = newTracked;
        newTracked = newf;
        f=f->next;
    }

    for (int i=0; i<n_resolutions; i++) {
        file* f = changed;
        while (f!=NULL) {
            if (strcmp(f->path, resolutions[i].file_name) == 0) {
                char* refile = resolutions[i].resolved_file;
                FILE* fr = fopen(refile, "r");
                fseek(fr, 0, SEEK_END);
                int size = ftell(fr);
                rewind(fr);
                char* buffer = malloc((size) * sizeof(*buffer));
                fread(buffer, size, 1, fr);
                fclose(fr);

                FILE* fp = fopen(f->path, "w");
                fwrite(buffer, 1, size, fp);
                fclose(fp);

                free(buffer);
                file* newf = malloc_file(s);
                strcpy(newf->path, f->path);
                newf->hash = hash_file(s, f->path);
                newf->next = newTracked;
                newTracked = newf;
                break;
            }
            f = f->next;
        }
    }

    s->tracked = newTracked;

    int len = strlen(branch_name) + strlen("Merged branch ");
    char* buff = (char*)malloc(sizeof(char)*len+1);
    sprintf(buff, "Merged branch %s", branch_name);

    char* commit_id = svc_commit(s, buff);
    s->head->prev = b->current;
    free(buff);
    printf("Merge successful\n");
    return commit_id;
}

