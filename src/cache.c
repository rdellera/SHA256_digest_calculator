
#include "cache.h"
#include <stdlib.h>
#include <string.h>

const char *cache_get(CacheEntry *head, const char *path) {
    while (head) {
        if (strcmp(head->path, path) == 0)
            return head->hash;
        head = head->next;
    }
    return NULL;
}

void cache_put(CacheEntry **head, const char *path, const char *hash) {
    CacheEntry *e = malloc(sizeof(CacheEntry));
    e->path = strdup(path);
    strcpy(e->hash, hash);
    e->next = *head;
    *head = e;
}

void free_cache(CacheEntry *head) {
    while (head) {
        CacheEntry *tmp = head;
        head = head->next;
        free(tmp->path);
        free(tmp);
    }
}
