#include <sys/time.h>

#ifndef __QUEUE_H__

typedef struct queue_node_t {
    int fd;
    struct timeval arrival_time;
    struct timeval dispatch_time;
    struct queue_node_t *prev, *next;
} QueueNode;

typedef struct queue_t {
    QueueNode *head;
    QueueNode *end;
    int queue_size;
    int waiting_queue_size; 
}Queue;

Queue *createQueue(int queue_size);
QueueNode *createNode(int fd_in, struct timeval arrival_time_in);
void enqueue(Queue* queue, int fd_in, struct timeval arrival_time_in);
QueueNode* dequeue(Queue *queue);
int random_drop(Queue* queue, int index_to_remove);

#endif