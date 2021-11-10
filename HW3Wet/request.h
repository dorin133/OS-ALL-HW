#include <time.h>
#include "queue.h"

#ifndef __REQUEST_H__



struct Stats{
    int thread_id;
    int thread_count; // The total number of http requests this thread has handled
    int thread_static; // The total number of static requests this thread has handled
    int thread_dynamic; // The total number of dynamic requests this thread has handled
};

void requestHandle(QueueNode* req, struct Stats *stats);

void init_stats(struct Stats *stats , int thread_id_in);



#endif
