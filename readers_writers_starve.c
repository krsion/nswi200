/*
 * readers_writers_starve.c
 *
 * BROKEN version of the Readers/Writers monitor that demonstrates
 * WRITER STARVATION.
 *
 * The bug: start_read() only checks active_writers, NOT waiting_writers.
 * So if readers keep arriving continuously, a waiting writer never gets
 * a turn — it starves.
 *
 * Compare with readers_writers.c where start_read() also checks
 * waiting_writers > 0 (writer-preference fix).
 *
 * Run both side-by-side to see the difference:
 *   ./readers_writers_starve   # writers may wait a long time / never run
 *   ./readers_writers          # writers get priority
 *
 * done_write() here only broadcasts to readers (no writer preference),
 * which combined with the missing check in start_read() causes starvation.
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

/* ── monitor procedures (BUGGY: no writer-preference) ──────────────────── */

/*
 * BUG: only blocks on active_writers, ignores waiting_writers.
 * New readers can keep flowing in even when writers are queued.
 * Returns a snapshot of waiting_writers taken before unlock so the
 * caller can print it without a data race.
 */
static int start_read(void) {
    pthread_mutex_lock(&lock);
    waiting_readers++;
    while (active_writers > 0)           /* ← missing: || waiting_writers > 0 */
        pthread_cond_wait(&ok_to_read, &lock);
    waiting_readers--;
    active_readers++;
    int snap = waiting_writers;
    pthread_mutex_unlock(&lock);
    return snap;
}

static void done_read(void) {
    pthread_mutex_lock(&lock);
    active_readers--;
    if (active_readers == 0 && waiting_writers > 0)
        pthread_cond_signal(&ok_to_write);
    pthread_mutex_unlock(&lock);
}

static void start_write(void) {
    pthread_mutex_lock(&lock);
    waiting_writers++;
    while (active_readers > 0 || active_writers > 0)
        pthread_cond_wait(&ok_to_write, &lock);
    waiting_writers--;
    active_writers++;
    pthread_mutex_unlock(&lock);
}

/*
 * BUG: always broadcasts to readers first, never prefers a waiting writer.
 * Combined with the missing check in start_read, the writer queue grows
 * while readers keep cutting in line.
 */
static void done_write(void) {
    pthread_mutex_lock(&lock);
    active_writers--;
    pthread_cond_broadcast(&ok_to_read); /* ← always wakes readers, no writer preference */
    pthread_mutex_unlock(&lock);
}

/* ── thread bodies ──────────────────────────────────────────────────────── */

/* Many readers with short sleep gaps to keep arriving continuously */
#define READER_ITERATIONS 8
#define WRITER_ITERATIONS 4

static void *reader_thread(void *arg) {
    int id = *(int *)arg;
    for (int i = 0; i < READER_ITERATIONS; i++) {
        int snap_w = start_read();
        double t0 = now_ms();
        int val = db_value;
        usleep(15000);
        double t1 = now_ms();
        printf("[%.3f-%.3f]  READER %d  read db=%d  (wait_w=%d)\n",
               t0, t1, id, val, snap_w);
        done_read();
        usleep(5000); /* very short gap → new reader arrives before writer can run */
    }
    return NULL;
}

static void *writer_thread(void *arg) {
    int id = *(int *)arg;
    for (int i = 0; i < WRITER_ITERATIONS; i++) {
        double queued = now_ms();
        start_write();          /* may block here for a long time */
        double t0 = now_ms();
        int old = db_value;
        usleep(20000);
        db_value++;
        double t1 = now_ms();
        printf("[%.3f-%.3f]  WRITER %d  db %d -> %d  (waited %.0f ms) *** FINALLY GOT IN ***\n",
               t0, t1, id, old, db_value, t0 - queued);
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

    printf("Readers/Writers monitor (BUGGY - writer starvation demo)\n");
    printf("Writers are started 100ms late so readers build up first.\n");
    printf("Watch the 'waited N ms' values grow on writer lines.\n\n");

    for (int i = 0; i < NUM_READERS; i++) {
        rids[i] = i + 1;
        pthread_create(&readers[i], NULL, reader_thread, &rids[i]);
    }
    /* small head-start for readers so the starvation is visible immediately */
    usleep(100000);
    for (int i = 0; i < NUM_WRITERS; i++) {
        wids[i] = i + 1;
        pthread_create(&writers[i], NULL, writer_thread, &wids[i]);
    }

    for (int i = 0; i < NUM_READERS; i++) pthread_join(readers[i], NULL);
    for (int i = 0; i < NUM_WRITERS; i++) pthread_join(writers[i], NULL);

    printf("\nFinal db_value = %d  (expected %d)\n",
           db_value, NUM_WRITERS * WRITER_ITERATIONS);
    return 0;
}
