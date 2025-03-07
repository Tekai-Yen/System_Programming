#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "thread_tool.h"

void idle(int id, int *args) {
    thread_setup(id, args);

    while (1) {
        printf("thread %d: idle\n", current_thread->id);
        sleep(1);
        thread_yield();
    }
}

void fibonacci(int id, int *args) {
    thread_setup(id, args);

    current_thread->n = current_thread->args[0];
    for (current_thread->i = 1;; current_thread->i++) {
        if (current_thread->i <= 2) {
            current_thread->f_cur = 1;
            current_thread->f_prev = 1;
        } else {
            int f_next = current_thread->f_cur + current_thread->f_prev;
            current_thread->f_prev = current_thread->f_cur;
            current_thread->f_cur = f_next;
        }
        printf("thread %d: F_%d = %d\n", current_thread->id, current_thread->i,
               current_thread->f_cur);

        sleep(1);

        if (current_thread->i == current_thread->n) {
            thread_exit();
        } else {
            thread_yield();
        }
    }
}

void pm(int id, int *args) {
    thread_setup(id, args);
    
    current_thread->n = current_thread->args[0];
    //printf("thread %d:       n=%d\n", current_thread->id, current_thread->n);
    current_thread->i = 1;
    current_thread->f_cur = 1;  // 用 f_cur 來存當前的 pm 值
    
    while(current_thread->i <= current_thread->n) {

        if(current_thread->i == 1) {
            current_thread->f_cur = 1;
        } else {
            if(current_thread->i % 2 == 0) {
                current_thread->f_cur = current_thread->f_cur - current_thread->i ;
            } 
            else {
                current_thread->f_cur = current_thread->f_cur + current_thread->i ;
            }
        }
        printf("thread %d: pm(%d) = %d\n", current_thread->id, current_thread->i, current_thread->f_cur);
        sleep(1);

        if(current_thread->i == current_thread->n) {
            thread_exit();
        }
        else {
            current_thread->i++;
            thread_yield();
        }
    }
    
}

void enroll(int id, int *args) {
    thread_setup(id, args);
    
    current_thread->d_p = current_thread->args[0];  // pj課程意願
    current_thread->d_s = current_thread->args[1];  // sw課程意願
    int s = current_thread->args[2];    // 睡眠時間
    current_thread->b = current_thread->args[3];    // 朋友ID
    
    // Step 1: 睡眠
    printf("thread %d: sleep %d\n", current_thread->id, s);
    thread_sleep(s);
    
    // Step 2: 喚醒朋友並讀取課程資訊
    thread_awake(current_thread->b);
    read_lock();
    printf("thread %d: acquire read lock\n", current_thread->id);
    current_thread->q_p = q_p;
    current_thread->q_s = q_s;
    sleep(1);
    thread_yield();
    
    // Step 3: 計算優先值並釋放讀取鎖
    read_unlock();
    current_thread->p_p = current_thread->d_p * current_thread->q_p;
    current_thread->p_s = current_thread->d_s * current_thread->q_s;
    printf("thread %d: release read lock, p_p = %d, p_s = %d\n", 
           current_thread->id, current_thread->p_p, current_thread->p_s);
    sleep(1);
    thread_yield();
    
    // Step 4: 取得寫入鎖並選課
    write_lock();
    if(q_p == 0 || q_s == 0){
        if(q_p == 0){
            printf("thread %d: acquire write lock, enroll in sw_class\n", current_thread->id);
            q_s--;
        } 
        else {
            printf("thread %d: acquire write lock, enroll in pj_class\n", current_thread->id);
            q_p--;
        }
    }
    else{
        if(current_thread->p_p > current_thread->p_s || (current_thread->p_p == current_thread->p_s && current_thread->d_p > current_thread->d_s)) {
            if(q_p > 0) {
                printf("thread %d: acquire write lock, enroll in pj_class\n", current_thread->id);
                q_p--;
            } else {
                printf("thread %d: acquire write lock, enroll in sw_class\n", current_thread->id);
                q_s--;
            }
        } else {
            if(q_s > 0) {
                printf("thread %d: acquire write lock, enroll in sw_class\n", current_thread->id);
                q_s--;
            } else {
                printf("thread %d: acquire write lock, enroll in pj_class\n", current_thread->id);
                q_p--;
            }
        }
    }
    
    sleep(1);
    thread_yield();
    
    // Step 5: 釋放寫入鎖並結束
    write_unlock();
    printf("thread %d: release write lock\n", current_thread->id);
    sleep(1);
    thread_exit();
    
}

