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
#include "fbdemos.h"

#include "etna_pipe.h"
#include "util/u_memory.h"

#include <stdio.h>

void fbdemo_init(struct fbdemos_scaffold **out)
{
    struct fbdemos_scaffold *fbs = CALLOC_STRUCT(fbdemos_scaffold);
    int rv;
    
    rv = fb_open(0, &fbs->fb);
    if(rv!=0)
    {
        exit(1);
    }
    fbs->width = fbs->fb.fb_var.xres;
    fbs->height = fbs->fb.fb_var.yres;

    rv = viv_open(VIV_HW_3D, &fbs->conn);
    if(rv!=0)
    {
        fprintf(stderr, "Error opening device\n");
        exit(1);
    }
    printf("Succesfully opened device\n");

    if((fbs->pipe = etna_new_pipe_context(fbs->conn)) == NULL)
    {
        printf("Unable to create etna context\n");
        exit(1);
    }
    fbs->ctx = etna_pipe_get_etna_context(fbs->pipe);
    
    if(etna_bswap_create(fbs->ctx, &fbs->buffers, (etna_set_buffer_cb_t)&fb_set_buffer, (etna_copy_buffer_cb_t)&etna_fb_copy_buffer, &fbs->fb) != ETNA_OK)
    {
        printf("Unable to create buffer swapper\n");
        exit(1);
    }

    *out = fbs;
}

void fbdemo_free(struct fbdemos_scaffold *fbs)
{
    etna_bswap_free(fbs->buffers);
    etna_free(fbs->ctx);
    viv_close(fbs->conn);
    free(fbs);
}

