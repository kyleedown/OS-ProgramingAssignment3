#define _POSIX_C_SOURCE 200809L
#include "rw_log.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>

/* Monitor structure */
typedef struct {
    rwlog_entry_t *buf;      /* circular buffer */
    size_t capacity;

    size_t start;            /* index of oldest entry */
    size_t count;            /* number of valid entries */

    uint64_t seq_counter;    /* global sequence counter */
    size_t total_written;    /* total entries ever appended */

    /* synchronization */
    pthread_mutex_t lock;
    pthread_cond_t readers_cv;
    pthread_cond_t writers_cv;

    int active_readers;
    int active_writers; /* 0 or 1 */
    int waiting_writers;
} rwlog_monitor_t;

static rwlog_monitor_t *mon = NULL;

int rwlog_create(size_t capacity) {
    if (capacity == 0) return EINVAL;
    if (mon) return EALREADY;

    mon = calloc(1, sizeof(*mon));
    if (!mon) return ENOMEM;

    mon->buf = calloc(capacity, sizeof(rwlog_entry_t));
    if (!mon->buf) {
        free(mon);
        mon = NULL;
        return ENOMEM;
    }
    mon->capacity = capacity;
    mon->start = 0;
    mon->count = 0;
    mon->seq_counter = 1; /* start seq at 1 */
    mon->total_written = 0;

    if (pthread_mutex_init(&mon->lock, NULL) != 0) {
        free(mon->buf);
        free(mon);
        mon = NULL;
        return EAGAIN;
    }
    pthread_cond_init(&mon->readers_cv, NULL);
    pthread_cond_init(&mon->writers_cv, NULL);

    mon->active_readers = 0;
    mon->active_writers = 0;
    mon->waiting_writers = 0;
    return 0;
}

int rwlog_destroy(void) {
    if (!mon) return EINVAL;
    /* ideally verify no active users; we'll attempt a best-effort cleanup */
    pthread_mutex_lock(&mon->lock);
    pthread_mutex_unlock(&mon->lock);

    pthread_mutex_destroy(&mon->lock);
    pthread_cond_destroy(&mon->readers_cv);
    pthread_cond_destroy(&mon->writers_cv);

    free(mon->buf);
    free(mon);
    mon = NULL;
    return 0;
}

/* Reader ops: writer-preference */
int rwlog_begin_read(void) {
    if (!mon) return EINVAL;
    int rc = pthread_mutex_lock(&mon->lock);
    if (rc) return rc;

    /* Writer preference: block if a writer is active OR any writers are waiting */
    while (mon->active_writers || mon->waiting_writers > 0) {
        pthread_cond_wait(&mon->readers_cv, &mon->lock);
    }
    mon->active_readers++;
    pthread_mutex_unlock(&mon->lock);
    return 0;
}

/* Snapshot: copy most recent <= max_entries entries into buf (oldest->newest) */
ssize_t rwlog_snapshot(rwlog_entry_t *buf, size_t max_entries) {
    if (!mon || !buf) return -1;
    int rc = pthread_mutex_lock(&mon->lock);
    if (rc) return -1;

    size_t to_copy = mon->count;
    if (to_copy > max_entries) to_copy = max_entries;

    /* compute start index for the 'to_copy' most recent entries */
    size_t start_idx = (mon->start + (mon->count - to_copy)) % mon->capacity;
    for (size_t i = 0; i < to_copy; ++i) {
        size_t idx = (start_idx + i) % mon->capacity;
        buf[i] = mon->buf[idx];
    }

    pthread_mutex_unlock(&mon->lock);
    return (ssize_t)to_copy;
}

int rwlog_end_read(void) {
    if (!mon) return EINVAL;
    pthread_mutex_lock(&mon->lock);
    if (mon->active_readers > 0) mon->active_readers--;
    /* If no more readers, and writers are waiting, wake a writer */
    if (mon->active_readers == 0 && mon->waiting_writers > 0) {
        pthread_cond_signal(&mon->writers_cv);
    }
    pthread_mutex_unlock(&mon->lock);
    return 0;
}

/* Writer ops */
int rwlog_begin_write(void) {
    if (!mon) return EINVAL;
    pthread_mutex_lock(&mon->lock);
    mon->waiting_writers++;
    /* Wait while any writer active OR any readers active */
    while (mon->active_writers || mon->active_readers > 0) {
        pthread_cond_wait(&mon->writers_cv, &mon->lock);
    }
    mon->waiting_writers--;
    mon->active_writers = 1;
    pthread_mutex_unlock(&mon->lock);
    return 0;
}

int rwlog_append(const rwlog_entry_t *e) {
    if (!mon || !e) return EINVAL;
    pthread_mutex_lock(&mon->lock);

    size_t insert_idx;
    if (mon->count < mon->capacity) {
        insert_idx = (mon->start + mon->count) % mon->capacity;
        mon->count++;
    } else {
        /* buffer full: overwrite oldest */
        insert_idx = mon->start;
        mon->start = (mon->start + 1) % mon->capacity;
    }

    /* populate entry: seq, tid, realtime ts, msg (from e->msg) */
    rwlog_entry_t *dest = &mon->buf[insert_idx];
    dest->seq = mon->seq_counter++;
    dest->tid = pthread_self();
    clock_gettime(CLOCK_REALTIME, &dest->ts);
    /* safe string copy */
    strncpy(dest->msg, e->msg, RWLOG_MSG_MAX - 1);
    dest->msg[RWLOG_MSG_MAX - 1] = '\0';

    mon->total_written++;
    pthread_mutex_unlock(&mon->lock);
    return 0;
}

int rwlog_end_write(void) {
    if (!mon) return EINVAL;
    pthread_mutex_lock(&mon->lock);
    mon->active_writers = 0;
    /* On writer exit: if writers waiting -> signal one writer; otherwise broadcast readers */
    if (mon->waiting_writers > 0) {
        pthread_cond_signal(&mon->writers_cv);
    } else {
        pthread_cond_broadcast(&mon->readers_cv);
    }
    pthread_mutex_unlock(&mon->lock);
    return 0;
}

size_t rwlog_total_written(void) {
    if (!mon) return 0;
    pthread_mutex_lock(&mon->lock);
    size_t t = mon->total_written;
    pthread_mutex_unlock(&mon->lock);
    return t;
}
