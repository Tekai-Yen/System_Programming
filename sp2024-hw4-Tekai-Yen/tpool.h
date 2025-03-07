#pragma once

#include <pthread.h>

typedef int** Matrix;
typedef int* Vector;


typedef struct work {
    Matrix a;              
    Matrix b_transposed;    
    Matrix c;               
    int n;              
    int start;              
    int end;                 
    struct work* next;        
} work_t;

typedef struct request {
    Matrix a;                
    Matrix b;              
    Matrix c;               
    int num_works;          
    struct request* next;     
} request_t;

typedef struct queue {
    work_t* front;          
    work_t* rear;            
} queue_t;

typedef struct tpool {
    int num_threads;          
    int n;                   

    request_t* frontend_front; 
    request_t* frontend_rear;  
    pthread_mutex_t frontend_mutex; 
    pthread_cond_t frontend_cond;   

    queue_t worker_queue;         
    pthread_mutex_t worker_mutex; 
    pthread_cond_t worker_cond;   

    pthread_mutex_t sync_mutex;   
    pthread_cond_t sync_cond;     
    int pending_works;            

    pthread_t* backend_threads;   
    pthread_t frontend_thread;    

    int shutdown;                
} tpool_t;

struct tpool* tpool_init(int num_threads, int n);
void tpool_request(struct tpool*, Matrix a, Matrix b, Matrix c, int num_works);
void tpool_synchronize(struct tpool*);
void tpool_destroy(struct tpool*);

extern int calculation(int n, Vector a, Vector b);
