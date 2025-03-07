// tpool.c
#include "tpool.h"
#include <stdlib.h>
#include <stdio.h>

// �ŧi�~���p��禡
extern int calculation(int n, Vector a, Vector b);

// �����禡�n��
static void* backend_worker(void* arg);
static void* frontend_handler(void* arg);

// ���C�ާ@�G�ШD���C
static void enqueue_request(tpool_t* pool, Matrix a, Matrix b, Matrix c, int num_works) {
    request_t* req = malloc(sizeof(request_t));
    if (!req) {
        perror("Failed to allocate request");
        exit(EXIT_FAILURE);
    }
    req->a = a;
    req->b = b;
    req->c = c;
    req->num_works = num_works;
    req->next = NULL;

    pthread_mutex_lock(&pool->frontend_mutex);
    if (pool->frontend_rear == NULL) {
        pool->frontend_front = pool->frontend_rear = req;
    } else {
        pool->frontend_rear->next = req;
        pool->frontend_rear = req;
    }
    pthread_cond_signal(&pool->frontend_cond);
    pthread_mutex_unlock(&pool->frontend_mutex);
}

static request_t* dequeue_request(tpool_t* pool) {
    pthread_mutex_lock(&pool->frontend_mutex);
    while (pool->frontend_front == NULL && !pool->shutdown) {
        pthread_cond_wait(&pool->frontend_cond, &pool->frontend_mutex);
    }
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->frontend_mutex);
        return NULL;
    }
    request_t* req = pool->frontend_front;
    pool->frontend_front = pool->frontend_front->next;
    if (pool->frontend_front == NULL) {
        pool->frontend_rear = NULL;
    }
    pthread_mutex_unlock(&pool->frontend_mutex);
    return req;
}

// ���C�ާ@�G�u�@���C
static void enqueue_work(tpool_t* pool, work_t* work) {
    pthread_mutex_lock(&pool->worker_mutex);
    if (pool->worker_queue.rear == NULL) {
        pool->worker_queue.front = pool->worker_queue.rear = work;
    } else {
        pool->worker_queue.rear->next = work;
        pool->worker_queue.rear = work;
    }
    pthread_cond_signal(&pool->worker_cond);
    pthread_mutex_unlock(&pool->worker_mutex);
}

static work_t* dequeue_work(tpool_t* pool) {
    pthread_mutex_lock(&pool->worker_mutex);
    while (pool->worker_queue.front == NULL && !pool->shutdown) {
        pthread_cond_wait(&pool->worker_cond, &pool->worker_mutex);
    }
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->worker_mutex);
        return NULL;
    }
    work_t* work = pool->worker_queue.front;
    pool->worker_queue.front = pool->worker_queue.front->next;
    if (pool->worker_queue.front == NULL) {
        pool->worker_queue.rear = NULL;
    }
    pthread_mutex_unlock(&pool->worker_mutex);
    return work;
}

// �e�ݽu�{�禡
static void* frontend_handler(void* arg) {
    tpool_t* pool = (tpool_t*)arg;
    while (1) {
        request_t* req = dequeue_request(pool);
        if (req == NULL) {
            break; // ����פ�H��
        }

        // ��m b �x�}
        Matrix b_transposed = malloc(pool->n * sizeof(Vector));
        if (!b_transposed) {
            perror("Failed to allocate b_transposed");
            exit(EXIT_FAILURE);
        }
        b_transposed[0] = malloc(pool->n * pool->n * sizeof(int));
        if (!b_transposed[0]) {
            perror("Failed to allocate b_transposed[0]");
            free(b_transposed);
            exit(EXIT_FAILURE);
        }
        for (int i = 1; i < pool->n; i++) {
            b_transposed[i] = b_transposed[i - 1] + pool->n;
        }
        for (int i = 0; i < pool->n; i++) {
            for (int j = 0; j < pool->n; j++) {
                b_transposed[i][j] = req->b[j][i];
            }
        }

        // ���Τu�@
        int total_works = pool->n * pool->n;
        int base = total_works / req->num_works;
        int remainder = total_works % req->num_works;
        int current = 0;
        for (int i = 0; i < req->num_works; i++) {
            int work_size = base + (i < remainder ? 1 : 0);
            if (work_size == 0) continue; // �� num_works > total_works ��

            work_t* work = malloc(sizeof(work_t));
            if (!work) {
                perror("Failed to allocate work");
                free(b_transposed[0]);
                free(b_transposed);
                exit(EXIT_FAILURE);
            }
            work->a = req->a;
            work->b_transposed = b_transposed;
            work->c = req->c;
            work->n = pool->n;
            work->start = current;
            work->end = current + work_size;
            work->next = NULL;
            current += work_size;

            // ��s pending works
            pthread_mutex_lock(&pool->sync_mutex);
            pool->pending_works += 1;
            pthread_mutex_unlock(&pool->sync_mutex);

            // �N�u�@�[�J�u�@���C
            enqueue_work(pool, work);
        }

        // �`�N�G���A���� b_transposed�A�]����ݽu�{�٦b�ϥ�
        free(req);
    }

    return NULL;
}

// ��ݽu�{�禡
static void* backend_worker(void* arg) {
    tpool_t* pool = (tpool_t*)arg;
    while (1) {
        work_t* work = dequeue_work(pool);
        if (work == NULL) {
            break; // ����פ�H��
        }

        // �p��C�Ӥ��n
        for (int idx = work->start; idx < work->end; idx++) {
            int row = idx / pool->n;
            int col = idx % pool->n;
            Vector a_row = work->a[row];
            Vector b_col = work->b_transposed[col];
            int result = calculation(pool->n, a_row, b_col);
            work->c[row][col] = result;
        }

        // ��s pending works
        pthread_mutex_lock(&pool->sync_mutex);
        pool->pending_works -= 1;
        if (pool->pending_works == 0) {
            pthread_cond_broadcast(&pool->sync_cond);
        }
        pthread_mutex_unlock(&pool->sync_mutex);

        // ���� b_transposed �u�b�̫�@�Ӥu�@����������
        pthread_mutex_lock(&pool->sync_mutex);
        if (pool->pending_works == 0) {
            free(work->b_transposed[0]);
            free(work->b_transposed);
        }
        pthread_mutex_unlock(&pool->sync_mutex);

        // ����u�@���c
        free(work);
    }
    return NULL;
}

// ��l�ƽu�{��
struct tpool* tpool_init(int num_threads, int n) {
    tpool_t* pool = malloc(sizeof(tpool_t));
    if (!pool) {
        perror("Failed to allocate thread pool");
        exit(EXIT_FAILURE);
    }
    pool->num_threads = num_threads;
    pool->n = n;
    pool->frontend_front = pool->frontend_rear = NULL;
    pool->worker_queue.front = pool->worker_queue.rear = NULL;
    pool->pending_works = 0;
    pool->shutdown = 0;

    // ��l�Ƥ�����M�����ܶq
    if (pthread_mutex_init(&pool->frontend_mutex, NULL) != 0 ||
        pthread_cond_init(&pool->frontend_cond, NULL) != 0 ||
        pthread_mutex_init(&pool->worker_mutex, NULL) != 0 ||
        pthread_cond_init(&pool->worker_cond, NULL) != 0 ||
        pthread_mutex_init(&pool->sync_mutex, NULL) != 0 ||
        pthread_cond_init(&pool->sync_cond, NULL) != 0) {
        perror("Failed to initialize mutexes or condition variables");
        free(pool);
        exit(EXIT_FAILURE);
    }

    // ���t��ݽu�{�}�C
    pool->backend_threads = malloc(num_threads * sizeof(pthread_t));
    if (!pool->backend_threads) {
        perror("Failed to allocate backend threads");
        // �M�z�w��l�ƪ��귽
        pthread_mutex_destroy(&pool->frontend_mutex);
        pthread_cond_destroy(&pool->frontend_cond);
        pthread_mutex_destroy(&pool->worker_mutex);
        pthread_cond_destroy(&pool->worker_cond);
        pthread_mutex_destroy(&pool->sync_mutex);
        pthread_cond_destroy(&pool->sync_cond);
        free(pool);
        exit(EXIT_FAILURE);
    }

    // �Ыث�ݽu�{
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&pool->backend_threads[i], NULL, backend_worker, pool) != 0) {
            perror("Failed to create backend thread");
            // �M�z�w�Ыت���ݽu�{
            for (int j = 0; j < i; j++) {
                pthread_cancel(pool->backend_threads[j]);
            }
            free(pool->backend_threads);
            pthread_mutex_destroy(&pool->frontend_mutex);
            pthread_cond_destroy(&pool->frontend_cond);
            pthread_mutex_destroy(&pool->worker_mutex);
            pthread_cond_destroy(&pool->worker_cond);
            pthread_mutex_destroy(&pool->sync_mutex);
            pthread_cond_destroy(&pool->sync_cond);
            free(pool);
            exit(EXIT_FAILURE);
        }
    }

    // �Ыثe�ݽu�{
    if (pthread_create(&pool->frontend_thread, NULL, frontend_handler, pool) != 0) {
        perror("Failed to create frontend thread");
        // �M�z�w�Ыت���ݽu�{
        for (int i = 0; i < num_threads; i++) {
            pthread_cancel(pool->backend_threads[i]);
        }
        free(pool->backend_threads);
        pthread_mutex_destroy(&pool->frontend_mutex);
        pthread_cond_destroy(&pool->frontend_cond);
        pthread_mutex_destroy(&pool->worker_mutex);
        pthread_cond_destroy(&pool->worker_cond);
        pthread_mutex_destroy(&pool->sync_mutex);
        pthread_cond_destroy(&pool->sync_cond);
        free(pool);
        exit(EXIT_FAILURE);
    }

    return pool;
}

// �����ШD
void tpool_request(struct tpool* pool, Matrix a, Matrix b, Matrix c, int num_works) {
    if (pool == NULL || a == NULL || b == NULL || c == NULL || num_works <= 0) {
        fprintf(stderr, "Invalid arguments to tpool_request\n");
        return;
    }
    enqueue_request(pool, a, b, c, num_works);
}

// �P�B
void tpool_synchronize(struct tpool* pool) {
    if (pool == NULL) {
        fprintf(stderr, "Invalid pool in tpool_synchronize\n");
        return;
    }

    pthread_mutex_lock(&pool->sync_mutex);
    while (pool->pending_works > 0) {
        pthread_cond_wait(&pool->sync_cond, &pool->sync_mutex);
    }
    pthread_mutex_unlock(&pool->sync_mutex);
}

// �P���u�{��
void tpool_destroy(struct tpool* pool) {
    if (pool == NULL) {
        return;
    }

    // �]�m�פ�лx�óq���Ҧ��u�{
    pthread_mutex_lock(&pool->frontend_mutex);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->frontend_cond);
    pthread_mutex_unlock(&pool->frontend_mutex);

    pthread_mutex_lock(&pool->worker_mutex);
    pthread_cond_broadcast(&pool->worker_cond);
    pthread_mutex_unlock(&pool->worker_mutex);

    // ���ݫe�ݽu�{����
    pthread_join(pool->frontend_thread, NULL);

    // ���ݫ�ݽu�{����
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->backend_threads[i], NULL);
    }

    // �����ݽu�{�}�C
    free(pool->backend_threads);

    // ����u�@���C�����Ѿl�u�@
    while (pool->worker_queue.front != NULL) {
        work_t* work = pool->worker_queue.front;
        pool->worker_queue.front = work->next;
        // ���� b_transposed �p�G�|������
        if (work->b_transposed) {
            free(work->b_transposed[0]);
            free(work->b_transposed);
        }
        free(work);
    }

    // ����e�ݶ��C�����Ѿl�ШD
    while (pool->frontend_front != NULL) {
        request_t* req = pool->frontend_front;
        pool->frontend_front = req->next;
        free(req);
    }

    // �P��������M�����ܶq
    pthread_mutex_destroy(&pool->frontend_mutex);
    pthread_cond_destroy(&pool->frontend_cond);
    pthread_mutex_destroy(&pool->worker_mutex);
    pthread_cond_destroy(&pool->worker_cond);
    pthread_mutex_destroy(&pool->sync_mutex);
    pthread_cond_destroy(&pool->sync_cond);

    // ����u�{�����c
    free(pool);
}

