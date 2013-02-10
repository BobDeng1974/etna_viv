/*
 * Copyright (c) 2012-2013 Etnaviv Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
/* Automatic buffer swapping */
#ifndef H_ETNA_BUFSWAP
#define H_ETNA_BUFSWAP

#include "etna.h"

#include <pthread.h>
#include <stdbool.h>

#define ETNA_BSWAP_NUM_BUFFERS 2

typedef struct _etna_bswap_buffer {
    pthread_mutex_t available_mutex;
    pthread_cond_t available_cond;
    bool is_available;
    int sig_id_ready;
} etna_bswap_buffer;

typedef struct _etna_bswap_buffers {
    etna_ctx *ctx;
    pthread_t thread;
    int backbuffer, frontbuffer;
    bool terminate;

    int (*set_buffer)(void *, int);
    void *userptr;
    etna_bswap_buffer buf[ETNA_BSWAP_NUM_BUFFERS];
} etna_bswap_buffers;

int etna_bswap_create(etna_ctx *ctx, etna_bswap_buffers **bufs_out, int (*set_buffer)(void *, int), void *userptr);

int etna_bswap_free(etna_bswap_buffers *bufs);

int etna_bswap_wait_available(etna_bswap_buffers *bufs);

int etna_bswap_queue_swap(etna_bswap_buffers *bufs);

#endif

