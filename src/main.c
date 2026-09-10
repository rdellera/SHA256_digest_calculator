
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdlib.h>

#include "queue.h"
#include "cache.h"
#include "digest.h"

FileNode *queue = NULL;
CacheEntry *cache = NULL;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

void *worker(void *arg) {
    while (1) {
        pthread_mutex_lock(&mtx);
        FileNode *n = dequeue(&queue);
        pthread_mutex_unlock(&mtx);

        if (!n) break;

        const char *cached = cache_get(cache, n->path);
        if (cached) {
            printf("[CACHE] %s -> %s\n", n->path, cached);
        } else {
            char hash[65];
            digest_file(n->path, hash);
            pthread_mutex_lock(&mtx);
            cache_put(&cache, n->path, hash);
            pthread_mutex_unlock(&mtx);
            printf("%s -> %s\n", n->path, hash);
        }

        free(n->path);
        free(n);
    }
    return NULL;
}

int main() {
    char input[512];
    int count = 1;

    printf("Inserisci path (assoluto o relativo) - per terminare l'inserimento inserisci f/F\n");

    while (1) {
        printf("File [%d]: ", count);
        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "f") == 0 || strcmp(input, "F") == 0)
            break;

        struct stat st;
        if (stat(input, &st) != 0) {
            printf("Errore: percorso non valido\n");
            continue;
        }

        enqueue_sorted(&queue, input, st.st_size);
        count++;
    }

    int max_threads;
    printf("Introdurre il limite di thread: ");
    scanf("%d", &max_threads);

    pthread_t tids[max_threads];
    for (int i = 0; i < max_threads; i++)
        pthread_create(&tids[i], NULL, worker, NULL);

    for (int i = 0; i < max_threads; i++)
        pthread_join(tids[i], NULL);

    free_cache(cache);
    return 0;
}
