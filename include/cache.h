
#ifndef CACHE_H
#define CACHE_H

typedef struct CacheEntry {
    char *path;
    char hash[65];
    struct CacheEntry *next;
} CacheEntry;

const char *cache_get(CacheEntry *head, const char *path);
void cache_put(CacheEntry **head, const char *path, const char *hash);
void free_cache(CacheEntry *head);

#endif
