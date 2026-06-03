/*
 * readers_writers.c
 *
 * Mesa-style monitor implementing the Readers/Writers problem with
 * WRITER PREFERENCE (waiting writers block new readers).
 *
 * Monitor state:
 *   active_readers  - threads currently inside a read section
 *   active_writers  - threads currently inside a write section (0 or 1)
 *   waiting_readers - threads blocked in start_read()
 *   waiting_writers - threads blocked in start_write()
 *
 * Key insight (Mesa semantics): condition checks are always `while`, never
 * `if`, because a thread may be woken spuriously or another thread may
 * sneak in between pthread_cond_signal() and the waiter actually running.
 *
 * Observe with:
 *   strace -T -e futex ./readers_writers
 *   bpftrace -e 'tracepoint:sched:sched_switch { printf("%s -> %s\n", args->prev_comm, args->next_comm); }'
 *   perf sched record -- ./readers_writers && perf sched timehist
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

/* ── monitor state ─────────────────────────────────────────────────────── */
static pthread_mutex_t lock          = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  ok_to_read    = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  ok_to_write   = PTHREAD_COND_INITIALIZER;

static int active_readers  = 0;
static int active_writers  = 0;
static int waiting_readers = 0;
static int waiting_writers = 0;

/* ── shared "database" ─────────────────────────────────────────────────── */
static int db_value = 0;

/* ── helpers ────────────────────────────────────────────────────────────── */
static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

/* ── monitor entry/exit procedures ─────────────────────────────────────── */

/*
 * start_read: block while a writer is active OR writers are waiting
 * (writer-preference: waiting_writers > 0 also blocks new readers).
 * Returns a snapshot of active_readers taken while still holding the lock,
 * so callers can print it without a data race.
 */
static int start_read(void) {
    pthread_mutex_lock(&lock);
    waiting_readers++;
    while (active_writers > 0 || waiting_writers > 0)
        pthread_cond_wait(&ok_to_read, &lock);
    waiting_readers--;
    active_readers++;
    int snap = active_readers;
    pthread_mutex_unlock(&lock);
    return snap;
}

static void done_read(void) {
    pthread_mutex_lock(&lock);
    active_readers--;
    /* last reader wakes a waiting writer */
    if (active_readers == 0 && waiting_writers > 0)
        pthread_cond_signal(&ok_to_write);
    pthread_mutex_unlock(&lock);
}

/* start_write: block while any reader or writer is active */
static void start_write(void) {
    pthread_mutex_lock(&lock);
    waiting_writers++;
    while (active_readers > 0 || active_writers > 0)
        pthread_cond_wait(&ok_to_write, &lock);
    waiting_writers--;
    active_writers++;
    pthread_mutex_unlock(&lock);
}

static void done_write(void) {
    pthread_mutex_lock(&lock);
    active_writers--;
    /* prefer waking a waiting writer; otherwise let all readers in */
    if (waiting_writers > 0)
        pthread_cond_signal(&ok_to_write);
    else
        pthread_cond_broadcast(&ok_to_read);
    pthread_mutex_unlock(&lock);
}

/* ── thread bodies ──────────────────────────────────────────────────────── */
#define ITERATIONS 4

static void *reader_thread(void *arg) {
    int id = *(int *)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        int snap_r = start_read();
        double t0 = now_ms();
        /* --- inside read section --- */
        int val = db_value;
        usleep(20000); /* 20 ms: makes concurrent readers visible in timestamps */
        double t1 = now_ms();
        printf("[%.3f-%.3f]  READER %d  read db=%d  (active_r=%d)\n",
               t0, t1, id, val, snap_r);
        /* --- end read section --- */
        done_read();
        usleep(10000);
    }
    return NULL;
}

static void *writer_thread(void *arg) {
    int id = *(int *)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        start_write();
        double t0 = now_ms();
        /* --- inside write section --- */
        int old = db_value;
        usleep(30000); /* 30 ms: longer to make exclusion visible */
        db_value++;
        double t1 = now_ms();
        printf("[%.3f-%.3f]  WRITER %d  db %d -> %d\n",
               t0, t1, id, old, db_value);
        /* --- end write section --- */
        done_write();
        usleep(50000);
    }
    return NULL;
}

/* ── main ───────────────────────────────────────────────────────────────── */
#define NUM_READERS 5
#define NUM_WRITERS 2

int main(void) {
    pthread_t readers[NUM_READERS];
    pthread_t writers[NUM_WRITERS];
    int       rids[NUM_READERS];
    int       wids[NUM_WRITERS];

    printf("Readers/Writers monitor (writer-preference)\n");
    printf("Columns: [start_ms-end_ms]  ROLE id  detail\n\n");

    for (int i = 0; i < NUM_READERS; i++) {
        rids[i] = i + 1;
        pthread_create(&readers[i], NULL, reader_thread, &rids[i]);
    }
    for (int i = 0; i < NUM_WRITERS; i++) {
        wids[i] = i + 1;
        pthread_create(&writers[i], NULL, writer_thread, &wids[i]);
    }

    for (int i = 0; i < NUM_READERS; i++) pthread_join(readers[i], NULL);
    for (int i = 0; i < NUM_WRITERS; i++) pthread_join(writers[i], NULL);

    printf("\nFinal db_value = %d  (expected %d)\n",
           db_value, NUM_WRITERS * ITERATIONS);
    return 0;
}
