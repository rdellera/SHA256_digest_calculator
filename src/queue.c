
#include "queue.h"
#include <stdlib.h>
#include <string.h>

void enqueue_sorted(FileNode **head, const char *path, off_t size) {
    FileNode *node = malloc(sizeof(FileNode));
    node->path = strdup(path);
    node->size = size;
    node->next = NULL;

    if (!*head || size < (*head)->size) {
        node->next = *head;
        *head = node;
        return;
    }

    FileNode *curr = *head;
    while (curr->next && curr->next->size <= size)
        curr = curr->next;

    node->next = curr->next;
    curr->next = node;
}

FileNode *dequeue(FileNode **head) {
    if (!*head) return NULL;
    FileNode *n = *head;
    *head = n->next;
    return n;
}

void free_queue(FileNode *head) {
    while (head) {
        FileNode *tmp = head;
        head = head->next;
        free(tmp->path);
        free(tmp);
    }
}
