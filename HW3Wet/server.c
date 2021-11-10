#include "segel.h"
#include "request.h"
// #include "queue.h"
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#define BLOCK 0
#define DROP_TAIL 1
#define DROP_HEAD 2
#define RANDOM 3

pthread_cond_t buffer_empty;
pthread_cond_t buffer_full;
pthread_mutex_t m1; 
pthread_mutex_t m2;
int queue_size; //buffsz DELETE
int thread_num; //poolsz DELETE
Queue* queue; //int* buffer DELETE
int running_threads = 0; //how many threads are currently running
int count_id = 0;


// 
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//
static int our_ceil(double num) {
    int inum = (int)num;
    // fprintf(stderr, "inum = %d, num = %f\n", inum, num);
    if (num == (double)inum) {
        return inum;
    }
    return inum + 1;
}

void getargs(int *port, int *thread_num, int *queue_size, int *sched_alg, int argc, char *argv[])
{
    if (argc < 5) {
	fprintf(stderr, "Error: Not enough arguments\n");
	exit(1);
    }
    *port = atoi(argv[1]);
    *thread_num = atoi(argv[2]);
    *queue_size = atoi(argv[3]);
    if (strcmp(argv[4], "block") == 0) {
        *sched_alg = BLOCK; 
    }
    else if (strcmp(argv[4], "dt") == 0) {
         *sched_alg = DROP_TAIL;
    }
    else if (strcmp(argv[4], "dh") == 0) {
        *sched_alg = DROP_HEAD;
    }
    else if (strcmp(argv[4], "random") == 0) {
        *sched_alg = RANDOM;
    }
    else {
        *sched_alg = -1;
    }
}



//assuming there is at least one availble thread
void* thread_wrapper() {
    pthread_mutex_lock(&m2);
        int thread_id = count_id++;
    pthread_mutex_unlock(&m2);

    // struct Stats *stats;
    struct Stats *stats = (struct Stats *)malloc(sizeof(struct Stats));
    if (!stats){
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }

    init_stats(stats, thread_id);
    
    while (1) {
        pthread_mutex_lock(&m1);
            // if "waiting queue" is empty, wait until there is a new request
            while(queue->waiting_queue_size <= 0) {
                pthread_cond_wait(&buffer_empty, &m1);
            }
            QueueNode* cur_req = dequeue(queue);
            running_threads++;
            if (queue->waiting_queue_size + running_threads < queue_size) {
                pthread_cond_signal(&buffer_full);
            }
            // int start_time = (int)time(NULL); //TO DELETE
            // fprintf(stderr, "Thread %d is handling fd # %d, time: %d\n", (int)pthread_self(), cur_req, start_time); //TO DELETE
        pthread_mutex_unlock(&m1);
        requestHandle(cur_req, stats);
        Close(cur_req->fd);
        free(cur_req);
        pthread_mutex_lock(&m1);
            running_threads--;
            if (queue->waiting_queue_size + running_threads < queue_size) {
                pthread_cond_signal(&buffer_full);               
            }
        pthread_mutex_unlock(&m1);
        // fprintf(stderr, "Thread %d FINISHEDDDDDD handling fd # %d, time: %d\n", (int)pthread_self(), index, (int)time(NULL)-start_time);
    }
}

int main(int argc, char *argv[])
{
    int listenfd, connfd, port, clientlen, sched_alg;
    struct sockaddr_in clientaddr;

    getargs(&port, &thread_num, &queue_size, &sched_alg, argc, argv);
 
    pthread_mutex_init(&m1, NULL);
    pthread_cond_init(&buffer_empty, NULL);
    pthread_cond_init(&buffer_full, NULL);

    queue = createQueue(queue_size);

    pthread_t threads[thread_num];
    for (int i = 0; i < thread_num; ++i) {
      if (pthread_create(&threads[i], NULL, thread_wrapper, NULL) != 0) {
          fprintf(stderr, "Error: Cannot create thread # %d\n", i);
          exit(0);
        }
    }

    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
        struct timeval arrival_time;
        gettimeofday(&arrival_time, NULL);
        // fprintf(stderr, "accepted fd: %d \n", connfd);
        // char buf[MAXLINE];
        pthread_mutex_lock(&m1);
        if (queue->waiting_queue_size + running_threads >= queue_size){
            while (sched_alg == BLOCK && (queue->waiting_queue_size + running_threads >= queue_size)) {
                // fprintf(stderr, "running threads: %d \n", running_threads);
                pthread_cond_wait(&buffer_full, &m1);
            }
            if (sched_alg == DROP_TAIL) {
                pthread_mutex_unlock(&m1);
                Close(connfd);
                // fprintf(stderr, "dropped \n");
                continue;
            }
            else if (sched_alg == DROP_HEAD) {
                if (queue->waiting_queue_size > 0){
                    QueueNode* node_to_delete = dequeue(queue);
                    free(node_to_delete);
                }
                else {
                    pthread_mutex_unlock(&m1);
                    Close(connfd);
                    // fprintf(stderr, "dropped \n");
                    continue;
                }
            }
            else if (sched_alg == RANDOM){
                if (queue->waiting_queue_size <= 0){
                    pthread_mutex_unlock(&m1);
                    Close(connfd);
                    // fprintf(stderr, "dropped \n");
                    continue;
                }
                double size_1 = (double)queue->waiting_queue_size / 4;
                int size = our_ceil(size_1);
                // fprintf(stderr, "size = %d\n", size);
                time_t t;
                srand((unsigned) time(&t));
                for (int i = 0; i < size; i++) {
                    int index = rand() % queue->waiting_queue_size;
                    // fprintf(stderr, "i = %d, random drop chose index # %d out of %d \n", i, index, queue->waiting_queue_size);
                    int fd_to_close = random_drop(queue, index);
                    Close(fd_to_close);
                }
            }
        }
        enqueue(queue, connfd, arrival_time);
        pthread_cond_signal(&buffer_empty);
        pthread_mutex_unlock(&m1);
    }
    pthread_cond_destroy(&buffer_empty);
    pthread_cond_destroy(&buffer_full);
}