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
/* Surface handling */
#include "etna_surface.h"

#include "etna_internal.h"
#include "etna_pipe.h"
#include "etna_resource.h"
#include "etna_translate.h"
#include "pipe/p_defines.h"
#include "pipe/p_state.h"
#include "util/u_inlines.h"
#include "util/u_memory.h"

#include <etnaviv/common.xml.h>
#include <etnaviv/state.xml.h>
#include <etnaviv/state_3d.xml.h>

/* Macros to define state */
#define SET_STATE(addr, value) cs->addr = (value)
#define SET_STATE_FIXP(addr, value) cs->addr = (value)
#define SET_STATE_F32(addr, value) cs->addr = etna_f32_to_u32(value)

/* Generate clear command for a surface (non-TS case) */
void etna_rs_gen_clear_surface(struct etna_surface *surf, uint32_t clear_value)
{
    uint bs = util_format_get_blocksize(surf->base.format);
    uint format = 0;
    switch(bs) 
    {
    case 2: format = RS_FORMAT_A1R5G5B5; break;
    case 4: format = RS_FORMAT_A8R8G8B8; break;
    default: printf("Unhandled clear blocksize: %i (fmt %i)\n", bs, surf->base.format);
             format = RS_FORMAT_A8R8G8B8;
    }
    etna_compile_rs_state(&surf->clear_command, &(struct rs_state){
            .source_format = format,
            .dest_format = format,
            .dest_addr = surf->surf.address,
            .dest_stride = surf->surf.stride,
            .dest_tiling = surf->layout,
            .dither = {0xffffffff, 0xffffffff},
            .width = surf->surf.width,
            .height = surf->surf.height,
            .clear_value = {clear_value},
            .clear_mode = VIVS_RS_CLEAR_CONTROL_MODE_ENABLED1,
            .clear_bits = 0xffff
        });
}

static struct pipe_surface *etna_pipe_create_surface(struct pipe_context *pipe,
                                      struct pipe_resource *resource_,
                                      const struct pipe_surface *templat)
{
    struct etna_pipe_context *priv = etna_pipe_context(pipe);
    struct etna_surface *surf = CALLOC_STRUCT(etna_surface);
    struct etna_resource *resource = etna_resource(resource_);
    assert(templat->u.tex.first_layer == templat->u.tex.last_layer);
    unsigned layer = templat->u.tex.first_layer;
    unsigned level = templat->u.tex.level;
    assert(layer < resource->base.array_size);
   
    surf->base.context = pipe;

    pipe_reference_init(&surf->base.reference, 1);
    pipe_resource_reference(&surf->base.texture, &resource->base);

    /* Allocate a TS for the resource if there isn't one yet */
    /* XXX for now, don't do TS for render textures as this path
     * is not stable */
    if(!resource->ts & !(resource->base.bind & PIPE_BIND_SAMPLER_VIEW))
        etna_screen_resource_alloc_ts(pipe->screen, resource);

    surf->base.texture = &resource->base;
    surf->base.format = resource->base.format;
    surf->base.width = resource->levels[level].width;
    surf->base.height = resource->levels[level].height;
    surf->base.writable = templat->writable; // what is this for anyway
    surf->base.u = templat->u;

    surf->layout = resource->layout;
    surf->surf = resource->levels[level];
    surf->surf.address += layer * surf->surf.layer_stride; 
    surf->surf.logical += layer * surf->surf.layer_stride; 
    surf->clear_value = 0; /* last clear value */

    if(surf->surf.ts_address)
    {
        /* This abuses the RS as a plain buffer memset().
           Currently uses a fixed row size of 64 bytes. Some benchmarking with different sizes may be in order.
         */
        etna_compile_rs_state(&surf->clear_command, &(struct rs_state){
                .source_format = RS_FORMAT_X8R8G8B8,
                .dest_format = RS_FORMAT_X8R8G8B8,
                .dest_addr = surf->surf.ts_address,
                .dest_stride = 0x40,
                .dither = {0xffffffff, 0xffffffff},
                .width = 16,
                .height = surf->surf.ts_size/0x40,
                .clear_value = {priv->specs.ts_clear_value},  /* XXX should be 0x11111111 for non-2BITPERTILE GPUs */
                .clear_mode = VIVS_RS_CLEAR_CONTROL_MODE_ENABLED1,
                .clear_bits = 0xffff
            });
    } else {
        etna_rs_gen_clear_surface(surf, surf->clear_value);
    }
    etna_resource_touch(pipe, surf->base.texture);
    return &surf->base;
}

static void etna_pipe_surface_destroy(struct pipe_context *pipe, struct pipe_surface *surf)
{
    //struct etna_pipe_context *priv = etna_pipe_context(pipe);
    pipe_resource_reference(&surf->texture, NULL);
    FREE(surf);
}


void etna_pipe_surface_init(struct pipe_context *pc)
{
    pc->create_surface = etna_pipe_create_surface;
    pc->surface_destroy = etna_pipe_surface_destroy;
}

