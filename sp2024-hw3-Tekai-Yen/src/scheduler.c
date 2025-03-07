#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "routine.h"
#include "thread_tool.h"

struct tcb *sleeping_set[THREAD_MAX];
int sleeping[THREAD_MAX]={0};

// TODO::
// Prints out the signal you received.
// This function should not return. Instead, jumps to the scheduler.
void sighandler(int signum) {
    if(signum == SIGTSTP)
        printf("caught SIGTSTP\n");
    else if(signum == SIGALRM)
        printf("caught SIGALRM\n");
    longjmp(sched_buf, 2);
}

// TODO::
// Perfectly setting up your scheduler.
void scheduler() {

    // Get return value from setjmp at the beginning
    int ret_val = setjmp(sched_buf);
    
    // First time initialization
    if(ret_val == 0) {
        struct tcb *thread = malloc(sizeof(struct tcb));
        thread->id = 0;
        thread->args = NULL;
        idle(0, NULL);
    }

    // Reset alarm
    alarm(0);  // Cancel previous alarm
    alarm(time_slice);  // Set new alarm

    // Clear pending signals
    sigset_t mask;
    struct sigaction sa;
    
    // For SIGTSTP
    sigemptyset(&mask);
    sigaddset(&mask, SIGTSTP);
    sa.sa_handler = SIG_IGN;
    sa.sa_mask = mask;
    sa.sa_flags = 0;
    sigaction(SIGTSTP, &sa, NULL);
    sa.sa_handler = sighandler;
    sigaction(SIGTSTP, &sa, NULL);

    // For SIGALRM
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sa.sa_handler = SIG_IGN;
    sa.sa_mask = mask;
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);
    sa.sa_handler = sighandler;
    sigaction(SIGALRM, &sa, NULL);

    // Handle sleeping threads
    if(ret_val == 2) {  // from signal handler
        for(int i = 0; i < THREAD_MAX; i++) {
            if(sleeping[i] && sleeping_set[i]) {
                sleeping_set[i]->sleeping_time -= time_slice;
                if(sleeping_set[i]->sleeping_time <= 0) {
                    ready_queue.arr[(ready_queue.head + ready_queue.size) % THREAD_MAX] = sleeping_set[i];
                    ready_queue.size = (ready_queue.size + 1) % THREAD_MAX;
                    sleeping_set[i] = NULL;
                    sleeping[i] = 0;
                }
            }
        }
    }

    // Handle waiting threads
    while(waiting_queue.size > 0) {
        struct tcb *thread = waiting_queue.arr[waiting_queue.head];
        if(thread->waiting_for == 1 && rwlock.write_count == 0) {
            ready_queue.arr[(ready_queue.head + ready_queue.size) % THREAD_MAX] = thread;
            ready_queue.size = (ready_queue.size + 1) % THREAD_MAX;
            waiting_queue.head = (waiting_queue.head + 1) % THREAD_MAX;
            waiting_queue.size--;
        }
        else if(thread->waiting_for == 2 && rwlock.write_count == 0 && rwlock.read_count == 0) {
            ready_queue.arr[(ready_queue.head + ready_queue.size) % THREAD_MAX] = thread;
            ready_queue.size = (ready_queue.size + 1) % THREAD_MAX;
            waiting_queue.head = (waiting_queue.head + 1) % THREAD_MAX;
            waiting_queue.size--;
        }
        else {
            break;  // 如果第一個執行緒無法取得資源，就停止處理
        }
    }

    // Handle previous thread
    if(current_thread) {
        if(ret_val == 2) {  // from signal handler (thread_yield)
            if(current_thread != idle_thread) {
                ready_queue.arr[(ready_queue.head + ready_queue.size) % THREAD_MAX] = current_thread;
                ready_queue.size = (ready_queue.size + 1) % THREAD_MAX;
                current_thread = NULL;
            }
        }
        else if(ret_val == 1) {  // from normal longjmp
            if(current_thread->waiting_for != 0 && current_thread != idle_thread) {  // thread is waiting for resource
                waiting_queue.arr[(waiting_queue.head + waiting_queue.size) % THREAD_MAX] = current_thread;
                waiting_queue.size = (waiting_queue.size + 1) % THREAD_MAX;
            }
            current_thread = NULL;
            // Note: if thread called thread_exit or thread_sleep, do nothing
        }
        else if(ret_val == 3) {  // from thread_exit
            free(current_thread->args);  // ����Ѽư}�C
            free(current_thread);        // ���� TCB
            current_thread = NULL;
        }
        // Note: if thread called thread_sleep, do nothing
    }

    // Select next thread
    /*
    printf("READY_QUEUE : ");
    for(int p=ready_queue.head; p<ready_queue.head+ready_queue.size; p++){
        printf("%d ", ready_queue.arr[p]->id);
    }
    printf("\n");
    */
    if(ready_queue.size > 0) {
        current_thread = ready_queue.arr[ready_queue.head];
        ready_queue.head = (ready_queue.head + 1) % THREAD_MAX;
        ready_queue.size--;
        longjmp(current_thread->env, 1);
    }
    else {  // ready queue ����
        // �ˬd�O�_�٦��ίv���������
        bool has_sleeping = false;
        for(int i = 1; i < THREAD_MAX; i++) {
            if(sleeping[i]) {
                has_sleeping = true;
                break;
            }
        }
        
        if(has_sleeping) {  // �p�G���ίv����������A���� idle thread
            current_thread = idle_thread;
            longjmp(current_thread->env, 1);
        } 
        else {  // �p�G�S���ίv����������A�����{��
            if(idle_thread) {
                free(idle_thread);
                idle_thread = NULL;
            }
            return;
        }
    }

}

