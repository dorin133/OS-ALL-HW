#include "queue.h"
#include <stdlib.h>
#include <stdio.h>


QueueNode *createNode(int fd_in, struct timeval arrival_time_in)
{
    QueueNode *new_node = (QueueNode*)malloc(sizeof(QueueNode));
    if (!new_node){
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }
    new_node->fd = fd_in;
    new_node->arrival_time = arrival_time_in;
    new_node->dispatch_time = arrival_time_in;
    new_node->next = NULL;
    new_node->prev = NULL;
    return new_node;
}

Queue *createQueue(int queue_size)
{
    Queue *queue = (Queue *)malloc(sizeof(Queue));
    if (!queue){
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }
    struct timeval now;
    gettimeofday(&now,NULL);
    queue->head = createNode(-1, now); //dummy head
    queue->end = createNode(-1, now); //dummy end
    queue->head->next = queue->end;
    queue->end->prev = queue->head;
    queue->queue_size=queue_size;
    queue->waiting_queue_size = 0;
    return queue;
}

int getCurrentTime(struct timeval start){
    struct timeval now;
    gettimeofday(&now,NULL);
    int current_time=((now.tv_sec * 1000 + now.tv_usec/1000)-(start.tv_sec * 1000 + start.tv_usec/1000));
    return current_time;
}

void enqueue(Queue* queue, int fd_in, struct timeval arrival_time_in)
{
    QueueNode *new_node, *end_prev;
    new_node = createNode(fd_in, arrival_time_in);
    end_prev = queue->end->prev;
    queue->end->prev = new_node;
    new_node->next = queue->end;
    new_node->prev = end_prev;
    end_prev->next = new_node;
    queue->waiting_queue_size++;
    // fprintf(stderr, "added fd # %d to queue\n", fd_in-3);
}

QueueNode* dequeue(Queue *queue)
{
    QueueNode *node_to_remove = queue->head->next;
    node_to_remove->next->prev = node_to_remove->prev;
    node_to_remove->prev->next = node_to_remove->next;
    queue->waiting_queue_size--;
    struct timeval dispatch_time;
    gettimeofday(&dispatch_time, NULL);
    node_to_remove->dispatch_time = dispatch_time;
    // fprintf(stderr, "removed fd # %d from queue\n", node_to_remove->fd-3);
    return node_to_remove;
}

int random_drop(Queue* queue, int index_to_remove){
    QueueNode *node_to_remove = queue->head->next;
    for (int i = 0; i < index_to_remove; i++) {
        node_to_remove = node_to_remove->next;
    }
    node_to_remove->next->prev=node_to_remove->prev;
    node_to_remove->prev->next=node_to_remove->next;
    queue->waiting_queue_size--;
    int fd_to_close = node_to_remove->fd;
    free (node_to_remove);
    return fd_to_close;
}


// struct node* get_waiting(Queue* q){
//     struct node* pending;
//     pending=q->inprogress_tail->next;
//     int current_time=getCurrentTime(q->main_thread_time_of_start);
//     pending->dispatch=current_time-pending->arrival;

//     q->inprogress_tail = q->inprogress_tail->next;
//     q->waiting_size --;
    
//     return pending;
// }

// int remove_oldest_waiting(Queue* q){
//     struct node *oldest;

//     oldest = q->inprogress_tail->next;
//     q->inprogress_tail->next = oldest->next;
//     oldest->next->prev = q->inprogress_tail;
//     q->current_size--;
//     q->waiting_size--;
//     if(q->head->next==q->tail){
//         q->inprogress_tail=q->head;
//     }
//     int oldest_data=oldest->data;
//     free (oldest);
//     return oldest_data;
// }



