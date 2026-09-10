
#ifndef QUEUE_H
#define QUEUE_H

#include <sys/types.h>

typedef struct FileNode {
    char *path;
    off_t size;
    struct FileNode *next;
} FileNode;

void enqueue_sorted(FileNode **head, const char *path, off_t size);
FileNode *dequeue(FileNode **head);
void free_queue(FileNode *head);

#endif
