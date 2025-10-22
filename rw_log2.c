#include "rw_log.h"
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <sys/time.h>
#include <inttypes.h> 

/* ------------------ Internal monitor state ------------------ */
static pthread_mutex_t monitor_mutex;
static pthread_cond_t  cond_readers;
static pthread_cond_t  cond_writers;

static size_t buf_capacity = 0;
static rwlog_entry_t *buffer = NULL;
static size_t buf_head = 0;
static size_t buf_count = 0;
static uint64_t global_seq = 0;

/* Reader/Writer bookkeeping */
static int active_readers = 0;
static int active_writers = 0;
static int waiting_writers = 0;

/* for shutdown */
static int monitor_initialized = 0;

/* ------------------ Monitor Implementation ------------------ */

int rwlog_create(size_t capacity) {
    if (capacity == 0) return EINVAL;
    if (pthread_mutex_init(&monitor_mutex, NULL)) return -1;
    if (pthread_cond_init(&cond_readers, NULL)) return -1;
    if (pthread_cond_init(&cond_writers, NULL)) return -1;
    buffer = calloc(capacity, sizeof(rwlog_entry_t));
    if (!buffer) return ENOMEM;
    buf_capacity = capacity;
    buf_head = buf_count = 0;
    global_seq = 0;
    active_readers = active_writers = waiting_writers = 0;
    monitor_initialized = 1;
    return 0;
}

int rwlog_destroy(void) {
    if (!monitor_initialized) return EINVAL;
    free(buffer);
    buffer = NULL;
    pthread_mutex_destroy(&monitor_mutex);
    pthread_cond_destroy(&cond_readers);
    pthread_cond_destroy(&cond_writers);
    monitor_initialized = 0;
    return 0;
}

/* Writer preference: readers wait if a writer active or writers waiting */
int rwlog_begin_read(void) {
    pthread_mutex_lock(&monitor_mutex);
    while (active_writers > 0 || waiting_writers > 0)
        pthread_cond_wait(&cond_readers, &monitor_mutex);
    active_readers++;
    pthread_mutex_unlock(&monitor_mutex);
    return 0;
}

int rwlog_end_read(void) {
    pthread_mutex_lock(&monitor_mutex);
    active_readers--;
    if (active_readers == 0 && waiting_writers > 0)
        pthread_cond_signal(&cond_writers);
    pthread_mutex_unlock(&monitor_mutex);
    return 0;
}

/* Copy ≤ max_entries of the newest entries */
size_t rwlog_snapshot(rwlog_entry_t *buf, size_t max_entries) {
    if (!buf || max_entries == 0) return 0;

    pthread_mutex_lock(&monitor_mutex);
    size_t count = buf_count;
    size_t capacity = buf_capacity;
    size_t head = buf_head;
    pthread_mutex_unlock(&monitor_mutex);

    if (count == 0) return 0;
    size_t to_copy = count < max_entries ? count : max_entries;
    size_t start = (head + capacity - to_copy) % capacity;

    for (size_t i = 0; i < to_copy; i++)
        buf[i] = buffer[(start + i) % capacity];
    return (ssize_t)to_copy;
}

int rwlog_begin_write(void) {
    pthread_mutex_lock(&monitor_mutex);
    waiting_writers++;
    while (active_writers > 0 || active_readers > 0)
        pthread_cond_wait(&cond_writers, &monitor_mutex);
    waiting_writers--;
    active_writers = 1;
    pthread_mutex_unlock(&monitor_mutex);
    return 0;
}

int rwlog_append(const rwlog_entry_t *e) {
    if (!e) return EINVAL;
    pthread_mutex_lock(&monitor_mutex);
    rwlog_entry_t entry = *e;
    entry.seq = ++global_seq;
    entry.tid = pthread_self();
    clock_gettime(CLOCK_REALTIME, &entry.ts);
    buffer[buf_head] = entry;
    buf_head = (buf_head + 1) % buf_capacity;
    if (buf_count < buf_capacity) buf_count++;
    pthread_mutex_unlock(&monitor_mutex);
    return 0;
}

int rwlog_end_write(void) {
    pthread_mutex_lock(&monitor_mutex);
    active_writers = 0;
    if (waiting_writers > 0)
        pthread_cond_signal(&cond_writers);
    else
        pthread_cond_broadcast(&cond_readers);
    pthread_mutex_unlock(&monitor_mutex);
    return 0;
}

void rwlog_wake_all(void) {
    pthread_mutex_lock(&monitor_mutex);
    pthread_cond_broadcast(&cond_readers);
    pthread_cond_broadcast(&cond_writers);
    pthread_mutex_unlock(&monitor_mutex);
}

/* ------------------ Reader/Writer Thread Logic ------------------ */

static volatile bool stop_flag = false;

struct writer_arg {
    int id, batch, wr_us;
    uint64_t written;
    double total_wait_ms;
};

struct reader_arg {
    int id, rd_us;
    uint64_t last_seen_seq;
    uint64_t read_count;
    double total_read_ms;
};

static void *writer_thread(void *v) {
    struct writer_arg *a = v;
    while (!stop_flag) {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        rwlog_begin_write();
        clock_gettime(CLOCK_MONOTONIC, &t1);
        a->total_wait_ms += (t1.tv_sec - t0.tv_sec) * 1000.0 +
                            (t1.tv_nsec - t0.tv_nsec) / 1e6;
        for (int i = 0; i < a->batch; i++) {
            rwlog_entry_t e = {0};
            snprintf(e.msg, RWLOG_MSG_MAX, "writer%d-msg%lu",
                     a->id, (unsigned long)a->written + 1);
            rwlog_append(&e);
            a->written++;
        }
        rwlog_end_write();
        usleep(a->wr_us);
    }
    return NULL;
}

static void *reader_thread(void *v) {
    struct reader_arg *a = v;
    rwlog_entry_t buf[128];
    while (!stop_flag) {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        rwlog_begin_read();
        ssize_t n = rwlog_snapshot(buf, 128);
        if (n > 0)
            a->last_seen_seq = buf[n - 1].seq;
        clock_gettime(CLOCK_MONOTONIC, &t1);
        a->total_read_ms += (t1.tv_sec - t0.tv_sec) * 1000.0 +
                            (t1.tv_nsec - t0.tv_nsec) / 1e6;
        a->read_count++;
        rwlog_end_read();
        usleep(a->rd_us);
    }
    return NULL;
}

/* ------------------ Program main ------------------ */

static void handle_sigint(int sig) {
    (void)sig;
    stop_flag = true;
    rwlog_wake_all();
}

int main(int argc, char **argv) {
    int capacity = 1024, readers = 2, writers = 2;
    int batch = 2, seconds = 10, rd_us = 2000, wr_us = 3000;

    static struct option opts[] = {
        {"capacity", required_argument, 0, 'c'},
        {"readers", required_argument, 0, 'r'},
        {"writers", required_argument, 0, 'w'},
        {"writer-batch", required_argument, 0, 'b'},
        {"seconds", required_argument, 0, 't'},
        {"rd-us", required_argument, 0, 'R'},
        {"wr-us", required_argument, 0, 'W'},
        {0,0,0,0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "c:r:w:b:t:R:W:", opts, NULL)) != -1) {
        switch (opt) {
            case 'c': capacity = atoi(optarg); break;
            case 'r': readers = atoi(optarg); break;
            case 'w': writers = atoi(optarg); break;
            case 'b': batch = atoi(optarg); break;
            case 't': seconds = atoi(optarg); break;
            case 'R': rd_us = atoi(optarg); break;
            case 'W': wr_us = atoi(optarg); break;
        }
    }

    signal(SIGINT, handle_sigint);
    rwlog_create(capacity);

    pthread_t rthreads[readers], wthreads[writers];
    struct reader_arg rargs[readers];
    struct writer_arg wargs[writers];

    for (int i = 0; i < readers; i++) {
        rargs[i] = (struct reader_arg){i, rd_us, 0, 0, 0};
        pthread_create(&rthreads[i], NULL, reader_thread, &rargs[i]);
    }
    for (int i = 0; i < writers; i++) {
        wargs[i] = (struct writer_arg){i, batch, wr_us, 0, 0};
        pthread_create(&wthreads[i], NULL, writer_thread, &wargs[i]);
    }

    for (int i = 0; i < seconds && !stop_flag; i++)
        sleep(1);

    stop_flag = true;
    rwlog_wake_all();

    for (int i = 0; i < readers; i++) pthread_join(rthreads[i], NULL);
    for (int i = 0; i < writers; i++) pthread_join(wthreads[i], NULL);

    /* ----- Metrics ----- */
    uint64_t total_written = 0, total_reads = 0;
    double writer_wait_sum = 0, reader_time_sum = 0;

    for (int i = 0; i < writers; i++) {
        total_written += wargs[i].written;
        writer_wait_sum += wargs[i].total_wait_ms;
    }
    for (int i = 0; i < readers; i++) {
        total_reads += rargs[i].read_count;
        reader_time_sum += rargs[i].total_read_ms;
    }

    printf("=== Reader–Writer Log Report ===\n");
    printf("Readers: %d  Writers: %d\n", readers, writers);
    printf("Entries written: %" PRIu64 "\n", total_written);
    printf("Avg writer wait (ms): %.3f\n",
           writers ? writer_wait_sum / writers : 0.0);
    printf("Avg reader section time (ms): %.3f\n",
           readers ? reader_time_sum / readers : 0.0);

    rwlog_destroy();
    return 0;
}
