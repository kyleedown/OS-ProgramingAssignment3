#define _POSIX_C_SOURCE 200809L
#include "rw_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <inttypes.h>
#include <getopt.h>

/* Global stop flag (set by timer or SIGINT) */
static volatile sig_atomic_t stop_flag = 0;

/* configuration with defaults */
struct config {
    int capacity;
    int readers;
    int writers;
    int writer_batch;
    int seconds;
    int rd_us;
    int wr_us;
    int dump_csv;
} cfg = {
    .capacity = 1024,
    .readers = 2,
    .writers = 2,
    .writer_batch = 1,
    .seconds = 10,
    .rd_us = 2000,
    .wr_us = 2000,
    .dump_csv = 0
};

static pthread_t *reader_threads = NULL;
static pthread_t *writer_threads = NULL;
static pthread_t timer_thread;

/* Simple accumulators for stats */
typedef struct {
    uint64_t samples;
    double total_ms;
} stat_accum_t;

static stat_accum_t reader_stats = {0, 0.0};
static stat_accum_t writer_stats = {0, 0.0};
static pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;

static void add_reader_stat(double ms) {
    pthread_mutex_lock(&stats_lock);
    reader_stats.samples++;
    reader_stats.total_ms += ms;
    pthread_mutex_unlock(&stats_lock);
}
static void add_writer_stat(double ms) {
    pthread_mutex_lock(&stats_lock);
    writer_stats.samples++;
    writer_stats.total_ms += ms;
    pthread_mutex_unlock(&stats_lock);
}

static void sigint_handler(int sig) {
    (void)sig;
    stop_flag = 1;
    /* We rely on threads checking stop_flag or being woken by monitor signals. */
}

/* timer thread: sleep then set stop_flag */
static void *timer_fn(void *arg) {
    int seconds = *(int *)arg;
    sleep(seconds);
    stop_flag = 1;
    return NULL;
}

/* helper: timespec difference in milliseconds */
static long timespec_to_ms(const struct timespec *start, const struct timespec *end) {
    long sec = end->tv_sec - start->tv_sec;
    long nsec = end->tv_nsec - start->tv_nsec;
    return sec * 1000 + nsec / 1000000;
}

/* Writer thread */
static void *writer_fn(void *arg) {
    long id = (long)arg;
    uint64_t local_count = 0;
    rwlog_entry_t tmp;

    while (!stop_flag) {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);

        if (rwlog_begin_write() != 0) {
            perror("rwlog_begin_write");
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &t1);
        long wait_ms = timespec_to_ms(&t0, &t1);
        add_writer_stat((double)wait_ms);

        for (int i = 0; i < cfg.writer_batch && !stop_flag; ++i) {
            snprintf(tmp.msg, RWLOG_MSG_MAX, "writer%ld-msg%lu", id, (unsigned long)local_count++);
            if (rwlog_append(&tmp) != 0) {
                perror("rwlog_append");
                /* try to continue */
            }
        }

        if (rwlog_end_write() != 0) {
            perror("rwlog_end_write");
            break;
        }

        if (cfg.wr_us > 0) usleep((useconds_t)cfg.wr_us);
    }
    return NULL;
}

/* Reader thread */
static void *reader_fn(void *arg) {
    long id = (long)arg;
    uint64_t last_seq_seen = 0;
    const size_t BUFMAX = 256;
    rwlog_entry_t *localbuf = malloc(sizeof(rwlog_entry_t) * BUFMAX);
    if (!localbuf) return NULL;

    while (!stop_flag) {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);

        if (rwlog_begin_read() != 0) {
            perror("rwlog_begin_read");
            break;
        }

        ssize_t n = rwlog_snapshot(localbuf, BUFMAX);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        long cs_ms = timespec_to_ms(&t0, &t1);
        if (n >= 0) add_reader_stat((double)cs_ms);

        /* simple monotonicity check: ensure seq numbers increase */
        for (ssize_t i = 0; i < n; ++i) {
            if (localbuf[i].seq > last_seq_seen) {
                last_seq_seen = localbuf[i].seq;
            } else {
                /* equal or decreasing might happen if no new entries or overwrite,
                   we don't strictly abort — but could log a warning. */
            }
        }

        if (rwlog_end_read() != 0) {
            perror("rwlog_end_read");
            break;
        }

        if (cfg.rd_us > 0) usleep((useconds_t)cfg.rd_us);
    }

    free(localbuf);
    return NULL;
}

/* create writer threads */
static int create_writers(int n) {
    writer_threads = calloc((size_t)n, sizeof(pthread_t));
    if (!writer_threads) return -1;
    for (int i = 0; i < n; ++i) {
        if (pthread_create(&writer_threads[i], NULL, writer_fn, (void *)(long)i) != 0) {
            perror("pthread_create writer");
            return -1;
        }
    }
    return 0;
}

/* create reader threads */
static int create_readers(int n) {
    reader_threads = calloc((size_t)n, sizeof(pthread_t));
    if (!reader_threads) return -1;
    for (int i = 0; i < n; ++i) {
        if (pthread_create(&reader_threads[i], NULL, reader_fn, (void *)(long)i) != 0) {
            perror("pthread_create reader");
            return -1;
        }
    }
    return 0;
}

/* join worker threads */
static void join_workers(void) {
    if (writer_threads) {
        for (int i = 0; i < cfg.writers; ++i) {
            pthread_join(writer_threads[i], NULL);
        }
    }
    if (reader_threads) {
        for (int i = 0; i < cfg.readers; ++i) {
            pthread_join(reader_threads[i], NULL);
        }
    }
}

/* print metrics */
static void print_metrics(void) {
    double avg_w_ms = writer_stats.samples ? (writer_stats.total_ms / writer_stats.samples) : 0.0;
    double avg_r_ms = reader_stats.samples ? (reader_stats.total_ms / reader_stats.samples) : 0.0;
    size_t total_written = rwlog_total_written();
    double throughput = cfg.seconds > 0 ? ((double)total_written / (double)cfg.seconds) : 0.0;

    printf("=== Metrics ===\n");
    printf("Total entries written: %zu\n", total_written);
    printf("Throughput (entries/sec): %.2f\n", throughput);
    printf("Average writer wait (ms): %.3f (over %" PRIu64 " samples)\n", avg_w_ms, writer_stats.samples);
    printf("Average reader critical-section time (ms): %.3f (over %" PRIu64 " samples)\n", avg_r_ms, reader_stats.samples);
}

/* usage */
static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [options]\n"
            "  --capacity N\n"
            "  --readers R\n"
            "  --writers W\n"
            "  --writer-batch B\n"
            "  --seconds T\n"
            "  --rd-us N\n"
            "  --wr-us N\n"
            "  --dump-csv (optional)\n",
            prog);
}

int main(int argc, char **argv) {
    static struct option longopts[] = {
        {"capacity", required_argument, NULL, 'c'},
        {"readers", required_argument, NULL, 'r'},
        {"writers", required_argument, NULL, 'w'},
        {"writer-batch", required_argument, NULL, 'b'},
        {"seconds", required_argument, NULL, 's'},
        {"rd-us", required_argument, NULL, 'R'},
        {"wr-us", required_argument, NULL, 'W'},
        {"dump-csv", no_argument, NULL, 'd'},
        { NULL, 0, NULL, 0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "c:r:w:b:s:R:W:d", longopts, NULL)) != -1) {
        switch (opt) {
            case 'c': cfg.capacity = atoi(optarg); break;
            case 'r': cfg.readers = atoi(optarg); break;
            case 'w': cfg.writers = atoi(optarg); break;
            case 'b': cfg.writer_batch = atoi(optarg); break;
            case 's': cfg.seconds = atoi(optarg); break;
            case 'R': cfg.rd_us = atoi(optarg); break;
            case 'W': cfg.wr_us = atoi(optarg); break;
            case 'd': cfg.dump_csv = 1; break;
            default: usage(argv[0]); return 1;
        }
    }

    if (cfg.capacity <= 0 || cfg.readers < 0 || cfg.writers < 0 || cfg.seconds <= 0) {
        usage(argv[0]);
        return 1;
    }

    if (rwlog_create((size_t)cfg.capacity) != 0) {
        fprintf(stderr, "rwlog_create failed\n");
        return 1;
    }

    signal(SIGINT, sigint_handler);

    /* start timer */
    if (pthread_create(&timer_thread, NULL, timer_fn, &cfg.seconds) != 0) {
        perror("pthread_create timer");
        return 1;
    }

    /* start workers */
    if (create_readers(cfg.readers) != 0) {
        fprintf(stderr, "failed to create readers\n");
        stop_flag = 1;
    }
    if (create_writers(cfg.writers) != 0) {
        fprintf(stderr, "failed to create writers\n");
        stop_flag = 1;
    }

    /* wait for timer */
    pthread_join(timer_thread, NULL);
    stop_flag = 1;

    /* join workers */
    join_workers();

    /* optional: implement CSV dump here if cfg.dump_csv set (not implemented) */

    print_metrics();

    rwlog_destroy();
    return 0;
}
