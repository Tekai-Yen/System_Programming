#ifndef THREAD_TOOL_H
#define THREAD_TOOL_H

#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>

// The maximum number of threads.
#define THREAD_MAX 100


void sighandler(int signum);
void scheduler();

// The thread control block structure.
struct tcb {
    int id;
    int *args;
    // Reveals what resource the thread is waiting for. The values are:
    //  - 0: no resource.
    //  - 1: read lock.
    //  - 2: write lock.
    int waiting_for;
    int sleeping_time;
    jmp_buf env;  // Where the scheduler should jump to.
    int n, i, f_cur, f_prev, d_p, d_s, p_p, p_s, q_p, q_s, b; // TODO: Add some variables you wish to keep between switches.
};

// The only one thread in the RUNNING state.
extern struct tcb *current_thread;
extern struct tcb *idle_thread;

struct tcb_queue {
    struct tcb *arr[THREAD_MAX];  // The circular array.
    int head;                     // The index of the head of the queue
    int size;
};

extern struct tcb_queue ready_queue, waiting_queue;


// The rwlock structure.
//
// When a thread acquires a type of lock, it should increment the corresponding count.
struct rwlock {
    int read_count;
    int write_count;
};

extern struct tcb *sleeping_set[THREAD_MAX];
extern int sleeping[THREAD_MAX];

extern struct rwlock rwlock;

// The remaining spots in classes.
extern int q_p, q_s;

// The maximum running time for each thread.
extern int time_slice;

// The long jump buffer for the scheduler.
extern jmp_buf sched_buf;

// TODO::
// You should setup your own sleeping set as well as finish the marcos below
#define thread_create(func, t_id, t_args)                                              \
    ({                                                                                 \
        func(t_id, t_args);                                                                 \
    })

#define thread_setup(t_id, t_args)                                                     \
    ({                                                                                 \
        struct tcb *thread = malloc(sizeof(struct tcb));              \
        thread->id = t_id;                                            \
        thread->args = t_args;                                        \
        thread->waiting_for = 0;                                      \
        thread->sleeping_time = 0;                                    \
        current_thread = thread;                                      \
        printf("thread %d: set up routine %s\n", t_id, __func__);     \
        if(t_id == 0) {                                              \
            idle_thread = thread;                                     \
        } else {                                                      \
            ready_queue.arr[(ready_queue.head + ready_queue.size)%(THREAD_MAX)] = thread; \
            ready_queue.size = (ready_queue.size + 1) % THREAD_MAX;                                               \
        }                                                            \
        if(setjmp(thread->env) == 0)                                 \
            return;                                                   \
    })

#define thread_yield()                                  \
    ({                                                  \
        if(setjmp(current_thread->env) == 0) {         \
            sigset_t mask;                             \
            sigemptyset(&mask);                        \
            /* Handle SIGTSTP */                       \
            sigaddset(&mask, SIGTSTP);                 \
            sigprocmask(SIG_UNBLOCK, &mask, NULL);     \
            sigprocmask(SIG_BLOCK, &mask, NULL);       \
            /* Clear mask for SIGALRM */               \
            sigemptyset(&mask);                        \
            /* Handle SIGALRM */                       \
            sigaddset(&mask, SIGALRM);                 \
            sigprocmask(SIG_UNBLOCK, &mask, NULL);     \
            sigprocmask(SIG_BLOCK, &mask, NULL);       \
        }                                               \
    })

#define read_lock()                                                      \
    ({                                                                   \
        if(rwlock.write_count > 0) {                                    \
            current_thread->waiting_for = 1;                            \
            if(setjmp(current_thread->env) == 0)                        \
                longjmp(sched_buf, 1);                                  \
        }                                                               \
        rwlock.read_count++;                                           \
    })

#define write_lock()                                                     \
    ({                                                                   \
        while(rwlock.write_count > 0 || rwlock.read_count > 0) {          \
            current_thread->waiting_for = 2;                            \
            if(setjmp(current_thread->env) == 0)                        \
                longjmp(sched_buf, 1);                                  \
        }                                                               \
        rwlock.write_count++;                                           \
    })

#define read_unlock()                                                                 \
    ({                                                                                \
        rwlock.read_count--;                                                         \
    })

#define write_unlock()                                                                \
    ({                                                                                \
        rwlock.write_count--;                                                        \
    })

#define thread_sleep(sec)                                            \
    ({                                                               \
        current_thread->sleeping_time = sec;                        \
        current_thread->waiting_for = 0;                           \
        sleeping_set[current_thread->id] = current_thread;          \
        sleeping[current_thread->id] = 1;                           \
        if(setjmp(current_thread->env) == 0)                       \
            longjmp(sched_buf, 1);                                 \
    })

#define thread_awake(t_id)                                                        \
    ({                                                                            \
        if(sleeping[t_id]) {                                        \
            ready_queue.arr[(ready_queue.head + ready_queue.size) % THREAD_MAX] = sleeping_set[t_id]; \
            ready_queue.size = (ready_queue.size + 1) % THREAD_MAX;                                     \
            sleeping_set[t_id] -> sleeping_time = 0;                              \
            sleeping_set[t_id] = NULL;                              \
            sleeping[t_id] = 0;                                     \
        }                                                           \
    })

#define thread_exit()                                    \
    ({                                                   \
        printf("thread %d: exit\n", current_thread->id); \
        if(setjmp(current_thread->env) == 0)            \
            longjmp(sched_buf, 3);                      \
    })

#endif  // THREAD_TOOL_H
