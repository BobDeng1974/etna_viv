/* like cube, but try to generate the command stream ourselves using etna_XXX */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>

#include "etna/state.xml.h"
#include "etna/cmdstream.xml.h"
#include "write_bmp.h"
#include "viv.h"

#include "cube_cmd.h"
/* TODO: should actually update context as we go,
   a context switch would currently revert state and likely result in corrupted rendering.
 */
#include "context_cmd.h"

//#define CMD_DEBUG
#define CMD_COMPARE

float vVertices[] = {
  // front
  -1.0f, -1.0f, +1.0f, // point blue
  //-0.5f, -0.6f, +0.5f, // point blue
  +1.0f, -1.0f, +1.0f, // point magenta
  -1.0f, +1.0f, +1.0f, // point cyan
  +1.0f, +1.0f, +1.0f, // point white
  // back
  +1.0f, -1.0f, -1.0f, // point red
  -1.0f, -1.0f, -1.0f, // point black
  +1.0f, +1.0f, -1.0f, // point yellow
  -1.0f, +1.0f, -1.0f, // point green
  // right
  +1.0f, -1.0f, +1.0f, // point magenta
  +1.0f, -1.0f, -1.0f, // point red
  +1.0f, +1.0f, +1.0f, // point white
  +1.0f, +1.0f, -1.0f, // point yellow
  // left
  -1.0f, -1.0f, -1.0f, // point black
  -1.0f, -1.0f, +1.0f, // point blue
  -1.0f, +1.0f, -1.0f, // point green
  -1.0f, +1.0f, +1.0f, // point cyan
  // top
  -1.0f, +1.0f, +1.0f, // point cyan
  +1.0f, +1.0f, +1.0f, // point white
  -1.0f, +1.0f, -1.0f, // point green
  +1.0f, +1.0f, -1.0f, // point yellow
  // bottom
  -1.0f, -1.0f, -1.0f, // point black
  +1.0f, -1.0f, -1.0f, // point red
  -1.0f, -1.0f, +1.0f, // point blue
  +1.0f, -1.0f, +1.0f  // point magenta
};

float vColors[] = {
  // front
  0.0f,  0.0f,  1.0f, // blue
  1.0f,  0.0f,  1.0f, // magenta
  0.0f,  1.0f,  1.0f, // cyan
  1.0f,  1.0f,  1.0f, // white
  // back
  1.0f,  0.0f,  0.0f, // red
  0.0f,  0.0f,  0.0f, // black
  1.0f,  1.0f,  0.0f, // yellow
  0.0f,  1.0f,  0.0f, // green
  // right
  1.0f,  0.0f,  1.0f, // magenta
  1.0f,  0.0f,  0.0f, // red
  1.0f,  1.0f,  1.0f, // white
  1.0f,  1.0f,  0.0f, // yellow
  // left
  0.0f,  0.0f,  0.0f, // black
  0.0f,  0.0f,  1.0f, // blue
  0.0f,  1.0f,  0.0f, // green
  0.0f,  1.0f,  1.0f, // cyan
  // top
  0.0f,  1.0f,  1.0f, // cyan
  1.0f,  1.0f,  1.0f, // white
  0.0f,  1.0f,  0.0f, // green
  1.0f,  1.0f,  0.0f, // yellow
  // bottom
  0.0f,  0.0f,  0.0f, // black
  1.0f,  0.0f,  0.0f, // red
  0.0f,  0.0f,  1.0f, // blue
  1.0f,  0.0f,  1.0f  // magenta
};

float vNormals[] = {
  // front
  +0.0f, +0.0f, +1.0f, // forward
  +0.0f, +0.0f, +1.0f, // forward
  +0.0f, +0.0f, +1.0f, // forward
  +0.0f, +0.0f, +1.0f, // forward
  // back
  +0.0f, +0.0f, -1.0f, // backbard
  +0.0f, +0.0f, -1.0f, // backbard
  +0.0f, +0.0f, -1.0f, // backbard
  +0.0f, +0.0f, -1.0f, // backbard
  // right
  +1.0f, +0.0f, +0.0f, // right
  +1.0f, +0.0f, +0.0f, // right
  +1.0f, +0.0f, +0.0f, // right
  +1.0f, +0.0f, +0.0f, // right
  // left
  -1.0f, +0.0f, +0.0f, // left
  -1.0f, +0.0f, +0.0f, // left
  -1.0f, +0.0f, +0.0f, // left
  -1.0f, +0.0f, +0.0f, // left
  // top
  +0.0f, +1.0f, +0.0f, // up
  +0.0f, +1.0f, +0.0f, // up
  +0.0f, +1.0f, +0.0f, // up
  +0.0f, +1.0f, +0.0f, // up
  // bottom
  +0.0f, -1.0f, +0.0f, // down
  +0.0f, -1.0f, +0.0f, // down
  +0.0f, -1.0f, +0.0f, // down
  +0.0f, -1.0f, +0.0f  // down
};
#define COMPONENTS_PER_VERTEX (3)
#define NUM_VERTICES (6*4)

#ifdef CMD_COMPARE
char is_padding[0x8000 / 4];
#endif

/* XXX store state changes
 * group consecutive states 
 * make LOAD_STATE commands, add to current command buffer
 */
inline void etna_set_state(struct _gcoCMDBUF *commandBuffer, uint32_t address, uint32_t value)
{
#ifdef CMD_DEBUG
    printf("%05x := %08x\n", address, value);
#endif
    uint32_t *tgt = (uint32_t*)((size_t)commandBuffer->logical + commandBuffer->offset);
    tgt[0] = VIV_FE_LOAD_STATE_HEADER_OP_LOAD_STATE |
            (1 << VIV_FE_LOAD_STATE_HEADER_COUNT__SHIFT) |
            ((address >> 2) << VIV_FE_LOAD_STATE_HEADER_OFFSET__SHIFT);
    tgt[1] = value;
    commandBuffer->offset += 8;
}
inline void etna_set_state_multi(struct _gcoCMDBUF *commandBuffer, uint32_t base, uint32_t num, uint32_t *values)
{
    uint32_t *tgt = (uint32_t*)((size_t)commandBuffer->logical + commandBuffer->offset);
    tgt[0] = VIV_FE_LOAD_STATE_HEADER_OP_LOAD_STATE |
            ((num & 0x3ff) << VIV_FE_LOAD_STATE_HEADER_COUNT__SHIFT) |
            ((base >> 2) << VIV_FE_LOAD_STATE_HEADER_OFFSET__SHIFT);
#ifdef CMD_DEBUG
    for(uint32_t idx=0; idx<num; ++idx)
    {
        printf("%05x := %08x\n", base, values[idx]);
        base += 4;
    }
#endif
    memcpy(&tgt[1], values, 4*num);
    commandBuffer->offset += 4 + num*4;
    if(commandBuffer->offset & 4) /* PAD */
    {
#ifdef CMD_COMPARE
        is_padding[commandBuffer->offset / 4] = 1;
#endif
        commandBuffer->offset += 4;
    }
}
inline void etna_set_state_f32(struct _gcoCMDBUF *commandBuffer, uint32_t address, float value)
{
    union {
        uint32_t i32;
        float f32;
    } x = { .f32 = value };
    etna_set_state(commandBuffer, address, x.i32);
}
inline void etna_set_state_fixp(struct _gcoCMDBUF *commandBuffer, uint32_t address, uint32_t value)
{
#ifdef CMD_DEBUG
    printf("%05x := %08x (fixp)\n", address, value);
#endif
    uint32_t *tgt = (uint32_t*)((size_t)commandBuffer->logical + commandBuffer->offset);
    tgt[0] = VIV_FE_LOAD_STATE_HEADER_OP_LOAD_STATE |
            (1 << VIV_FE_LOAD_STATE_HEADER_COUNT__SHIFT) |
            VIV_FE_LOAD_STATE_HEADER_FIXP |
            ((address >> 2) << VIV_FE_LOAD_STATE_HEADER_OFFSET__SHIFT);
    tgt[1] = value;
    commandBuffer->offset += 8;
}
inline void etna_draw_primitives(struct _gcoCMDBUF *cmdPtr, uint32_t primitive_type, uint32_t start, uint32_t count)
{
#ifdef CMD_DEBUG
    printf("draw_primitives %08x %08x %08x %08x\n", 
            VIV_FE_DRAW_PRIMITIVES_HEADER_OP_DRAW_PRIMITIVES,
            primitive_type, start, count);
#endif
    uint32_t *tgt = (uint32_t*)((size_t)cmdPtr->logical + cmdPtr->offset);
    tgt[0] = VIV_FE_DRAW_PRIMITIVES_HEADER_OP_DRAW_PRIMITIVES;
    tgt[1] = primitive_type;
    tgt[2] = start;
    tgt[3] = count;
    cmdPtr->offset += 16;
}

/* macro for MASKED() (multiple can be &ed) */
#define VIV_MASKED(NAME, VALUE) (~(NAME ## _MASK | NAME ## __MASK) | ((VALUE)<<(NAME ## __SHIFT)))
/* for boolean bits */
#define VIV_MASKED_BIT(NAME, VALUE) (~(NAME ## _MASK | NAME) | ((VALUE) ? NAME : 0))
/* for inline enum bit fields */
#define VIV_MASKED_INL(NAME, VALUE) (~(NAME ## _MASK | NAME ## __MASK) | (NAME ## _ ## VALUE))

/* warm up RS on aux render target */
void etna_warm_up_rs(struct _gcoCMDBUF *cmdPtr, viv_addr_t aux_rt_physical, viv_addr_t aux_rt_ts_physical)
{
    etna_set_state(cmdPtr, VIV_STATE_GL_FLUSH_CACHE, VIV_STATE_GL_FLUSH_CACHE_COLOR | VIV_STATE_GL_FLUSH_CACHE_DEPTH);
    etna_set_state(cmdPtr, VIV_STATE_TS_COLOR_STATUS_BASE, aux_rt_ts_physical); /* ADDR_G */
    etna_set_state(cmdPtr, VIV_STATE_TS_COLOR_SURFACE_BASE, aux_rt_physical); /* ADDR_F */
    etna_set_state(cmdPtr, VIV_STATE_RS_FLUSH_CACHE, VIV_STATE_RS_FLUSH_CACHE_FLUSH);
    etna_set_state(cmdPtr, VIV_STATE_RS_CONFIG,  /* wut? */
            (RS_FORMAT_A8R8G8B8 << VIV_STATE_RS_CONFIG_SOURCE_FORMAT__SHIFT) |
            VIV_STATE_RS_CONFIG_UNK7 |
            (RS_FORMAT_R5G6B5 << VIV_STATE_RS_CONFIG_DEST_FORMAT__SHIFT) |
            VIV_STATE_RS_CONFIG_UNK14);
    etna_set_state(cmdPtr, VIV_STATE_RS_SOURCE_ADDR, aux_rt_physical); /* ADDR_F */
    etna_set_state(cmdPtr, VIV_STATE_RS_SOURCE_STRIDE, 0x400);
    etna_set_state(cmdPtr, VIV_STATE_RS_DEST_ADDR, aux_rt_physical); /* ADDR_F */
    etna_set_state(cmdPtr, VIV_STATE_RS_DEST_STRIDE, 0x400);
    etna_set_state(cmdPtr, VIV_STATE_RS_WINDOW_SIZE, 
            (4 << VIV_STATE_RS_WINDOW_SIZE_HEIGHT__SHIFT) |
            (16 << VIV_STATE_RS_WINDOW_SIZE_WIDTH__SHIFT));
    etna_set_state(cmdPtr, VIV_STATE_RS_CLEAR_CONTROL, VIV_STATE_RS_CLEAR_CONTROL_MODE_DISABLED);
    etna_set_state(cmdPtr, VIV_STATE_RS_KICKER, 0xbeebbeeb);

}

int main(int argc, char **argv)
{
    int rv;
    rv = viv_open();
    if(rv!=0)
    {
        fprintf(stderr, "Error opening device\n");
        exit(1);
    }
    printf("Succesfully opened device\n");
    viv_show_chip_info();

    /* allocate command buffer (blob uses four command buffers, but we don't even fill one) */
    viv_addr_t buf0_physical = 0;
    void *buf0_logical = 0;
    if(viv_alloc_contiguous(0x8000, &buf0_physical, &buf0_logical, NULL)!=0)
    {
        fprintf(stderr, "Error allocating host memory\n");
        exit(1);
    }
    printf("Allocated buffer: phys=%08x log=%08x\n", (uint32_t)buf0_physical, (uint32_t)buf0_logical);

    /* allocate main render target */
    gcuVIDMEM_NODE_PTR rt_node = 0;
    if(viv_alloc_linear_vidmem(0x70000, 0x40, gcvSURF_RENDER_TARGET, gcvPOOL_DEFAULT, &rt_node)!=0)
    {
        fprintf(stderr, "Error allocating render target buffer memory\n");
        exit(1);
    }
    printf("Allocated render target node: node=%08x\n", (uint32_t)rt_node);
    
    viv_addr_t rt_physical = 0; /* ADDR_A */
    void *rt_logical = 0;
    if(viv_lock_vidmem(rt_node, &rt_physical, &rt_logical)!=0)
    {
        fprintf(stderr, "Error locking render target memory\n");
        exit(1);
    }
    printf("Locked render target: phys=%08x log=%08x\n", (uint32_t)rt_physical, (uint32_t)rt_logical);
    memset(rt_logical, 0xff, 0x70000); /* clear previous result just in case, test that clearing works */

    /* allocate tile status for main render target */
    gcuVIDMEM_NODE_PTR rt_ts_node = 0;
    if(viv_alloc_linear_vidmem(0x700, 0x40, gcvSURF_TILE_STATUS, gcvPOOL_DEFAULT, &rt_ts_node)!=0)
    {
        fprintf(stderr, "Error allocating render target tile status memory\n");
        exit(1);
    }
    printf("Allocated render target tile status node: node=%08x\n", (uint32_t)rt_ts_node);
    
    viv_addr_t rt_ts_physical = 0; /* ADDR_B */
    void *rt_ts_logical = 0;
    if(viv_lock_vidmem(rt_ts_node, &rt_ts_physical, &rt_ts_logical)!=0)
    {
        fprintf(stderr, "Error locking render target memory\n");
        exit(1);
    }
    printf("Locked render target ts: phys=%08x log=%08x\n", (uint32_t)rt_ts_physical, (uint32_t)rt_ts_logical);

    /* allocate depth for main render target */
    gcuVIDMEM_NODE_PTR z_node = 0;
    if(viv_alloc_linear_vidmem(0x38000, 0x40, gcvSURF_DEPTH, gcvPOOL_DEFAULT, &z_node)!=0)
    {
        fprintf(stderr, "Error allocating depth memory\n");
        exit(1);
    }
    printf("Allocated depth node: node=%08x\n", (uint32_t)z_node);
    
    viv_addr_t z_physical = 0; /* ADDR_C */
    void *z_logical = 0;
    if(viv_lock_vidmem(z_node, &z_physical, &z_logical)!=0)
    {
        fprintf(stderr, "Error locking depth target memory\n");
        exit(1);
    }
    printf("Locked depth target: phys=%08x log=%08x\n", (uint32_t)z_physical, (uint32_t)z_logical);

    /* allocate depth ts for main render target */
    gcuVIDMEM_NODE_PTR z_ts_node = 0;
    if(viv_alloc_linear_vidmem(0x400, 0x40, gcvSURF_TILE_STATUS, gcvPOOL_DEFAULT, &z_ts_node)!=0)
    {
        fprintf(stderr, "Error allocating depth memory\n");
        exit(1);
    }
    printf("Allocated depth ts node: node=%08x\n", (uint32_t)z_ts_node);
    
    viv_addr_t z_ts_physical = 0; /* ADDR_D */
    void *z_ts_logical = 0;
    if(viv_lock_vidmem(z_ts_node, &z_ts_physical, &z_ts_logical)!=0)
    {
        fprintf(stderr, "Error locking depth target ts memory\n");
        exit(1);
    }
    printf("Locked depth ts target: phys=%08x log=%08x\n", (uint32_t)z_ts_physical, (uint32_t)z_ts_logical);

    /* allocate vertex buffer */
    gcuVIDMEM_NODE_PTR vtx_node = 0;
    if(viv_alloc_linear_vidmem(0x60000, 0x40, gcvSURF_VERTEX, gcvPOOL_DEFAULT, &vtx_node)!=0)
    {
        fprintf(stderr, "Error allocating vertex memory\n");
        exit(1);
    }
    printf("Allocated vertex node: node=%08x\n", (uint32_t)vtx_node);
    
    viv_addr_t vtx_physical = 0; /* ADDR_E */
    void *vtx_logical = 0;
    if(viv_lock_vidmem(vtx_node, &vtx_physical, &vtx_logical)!=0)
    {
        fprintf(stderr, "Error locking vertex memory\n");
        exit(1);
    }
    printf("Locked vertex memory: phys=%08x log=%08x\n", (uint32_t)vtx_physical, (uint32_t)vtx_logical);

    /* allocate aux render target */
    gcuVIDMEM_NODE_PTR aux_rt_node = 0;
    if(viv_alloc_linear_vidmem(0x4000, 0x40, gcvSURF_RENDER_TARGET, gcvPOOL_SYSTEM /*why?*/, &aux_rt_node)!=0)
    {
        fprintf(stderr, "Error allocating aux render target buffer memory\n");
        exit(1);
    }
    printf("Allocated aux render target node: node=%08x\n", (uint32_t)aux_rt_node);
    
    viv_addr_t aux_rt_physical = 0; /* ADDR_F */
    void *aux_rt_logical = 0;
    if(viv_lock_vidmem(aux_rt_node, &aux_rt_physical, &aux_rt_logical)!=0)
    {
        fprintf(stderr, "Error locking aux render target memory\n");
        exit(1);
    }
    printf("Locked aux render target: phys=%08x log=%08x\n", (uint32_t)aux_rt_physical, (uint32_t)aux_rt_logical);

    /* allocate tile status for aux render target */
    gcuVIDMEM_NODE_PTR aux_rt_ts_node = 0;
    if(viv_alloc_linear_vidmem(0x100, 0x40, gcvSURF_TILE_STATUS, gcvPOOL_DEFAULT, &aux_rt_ts_node)!=0)
    {
        fprintf(stderr, "Error allocating aux render target tile status memory\n");
        exit(1);
    }
    printf("Allocated aux render target tile status node: node=%08x\n", (uint32_t)aux_rt_ts_node);
    
    viv_addr_t aux_rt_ts_physical = 0; /* ADDR_G */
    void *aux_rt_ts_logical = 0;
    if(viv_lock_vidmem(aux_rt_ts_node, &aux_rt_ts_physical, &aux_rt_ts_logical)!=0)
    {
        fprintf(stderr, "Error locking aux ts render target memory\n");
        exit(1);
    }
    printf("Locked aux render target ts: phys=%08x log=%08x\n", (uint32_t)aux_rt_ts_physical, (uint32_t)aux_rt_ts_logical);

    /* Phew, now we got all the memory we need.
     * Write interleaved attribute vertex stream.
     * Unlike the GL example we only do this once, not every time glDrawArrays is called, the same would be accomplished
     * from GL by using a vertex buffer object.
     */
    for(int vert=0; vert<NUM_VERTICES; ++vert)
    {
        int src_idx = vert * COMPONENTS_PER_VERTEX;
        int dest_idx = vert * COMPONENTS_PER_VERTEX * 3;
        for(int comp=0; comp<COMPONENTS_PER_VERTEX; ++comp)
        {
            ((float*)vtx_logical)[dest_idx+comp+0] = vVertices[src_idx + comp]; /* 0 */
            ((float*)vtx_logical)[dest_idx+comp+3] = vNormals[src_idx + comp]; /* 1 */
            ((float*)vtx_logical)[dest_idx+comp+6] = vColors[src_idx + comp]; /* 2 */
        }
    }
    /*
    for(int idx=0; idx<NUM_VERTICES*3*3; ++idx)
    {
        printf("%i %f\n", idx, ((float*)vtx_logical)[idx]);
    }*/

    /* Load the command buffer and send the commit command. */
    /* First build context state map */
    size_t stateCount = 0x1d00;
    uint32_t *contextMap = malloc(stateCount * 4);
    memset(contextMap, 0, stateCount*4);
    for(int idx=0; idx<sizeof(contextbuf_addr)/sizeof(address_index_t); ++idx)
    {
        contextMap[contextbuf_addr[idx].address / 4] = contextbuf_addr[idx].index;
    }

    struct _gcoCMDBUF commandBuffer = {
        .object = {
            .type = gcvOBJ_COMMANDBUFFER
        },
        //.os = (_gcoOS*)0xbf7488,
        //.hardware = (_gcoHARDWARE*)0x402694e0,
        .physical = (void*)buf0_physical,
        .logical = (void*)buf0_logical,
        .bytes = 0x8000,
        .startOffset = 0x0,
        //.offset = 0xac0,
        //.free = 0x7520,
        //.hintTable = (unsigned int*)0x0, // Used when gcdSECURE
        //.hintIndex = (unsigned int*)0x58,  // Used when gcdSECURE
        //.hintCommit = (unsigned int*)0xffffffff // Used when gcdSECURE
    };
    struct _gcoCONTEXT contextBuffer = {
        .object = {
            .type = gcvOBJ_CONTEXT
        },
        //.os = (_gcoOS*)0xbf7488,
        //.hardware = (_gcoHARDWARE*)0x402694e0,
        .id = 0x0, // Actual ID will be returned here
        .map = contextMap,
        .stateCount = stateCount,
        //.hint = (unsigned char*)0x0, // Used when gcdSECURE
        //.hintValue = 2, // Used when gcdSECURE
        //.hintCount = 0xca, // Used when gcdSECURE
        .buffer = contextbuf,
        .pipe3DIndex = 0x2d6, // XXX should not be hardcoded
        .pipe2DIndex = 0x106e,
        .linkIndex = 0x1076,
        .inUseIndex = 0x1078,
        .bufferSize = 0x41e4,
        .bytes = 0x0, // Number of bytes at physical, logical
        .physical = (void*)0x0,
        .logical = (void*)0x0,
        .link = (void*)0x0, // Logical address of link
        .initialPipe = 0x1,
        .entryPipe = 0x0,
        .currentPipe = 0x0,
        .postCommit = 1,
        .inUse = (int*)0x0, // Logical address of inUse
        .lastAddress = 0xffffffff, // Not used by kernel
        .lastSize = 0x2, // Not used by kernel
        .lastIndex = 0x106a, // Not used by kernel
        .lastFixed = 0, // Not used by kernel
        //.hintArray = (unsigned int*)0x0, // Used when gcdSECURE
        //.hintIndex = (unsigned int*)0x0  // Used when gcdSECURE
    };
    struct _gcoCMDBUF *cmdPtr = &commandBuffer;
    
    commandBuffer.free = commandBuffer.bytes - 0x8; /* Always keep 0x8 at end of buffer for kernel driver */
    commandBuffer.startOffset = 0;
    commandBuffer.offset = commandBuffer.startOffset + 8*4;
#ifdef CMD_COMPARE
    memset(is_padding, 0, 0x8000/4); /* just for debugging / comparing */
#endif

    /* XXX how important is the ordering? I suppose we could group states (except the flushes, kickers, semaphores etc)
     * and simply submit them at once. Especially for consecutive states and masked stated this could be a big win
     * in DMA command buffer size. */

    /* Build first command buffer */
    etna_set_state(cmdPtr, VIV_STATE_GL_VERTEX_ELEMENT_CONFIG, 0x1);
    etna_set_state(cmdPtr, VIV_STATE_RA_CONTROL, 0x1);

    etna_set_state(cmdPtr, VIV_STATE_PA_W_CLIP_LIMIT, 0x34000001);
    etna_set_state(cmdPtr, VIV_STATE_PA_SYSTEM_MODE, 0x11);
    etna_set_state(cmdPtr, VIV_STATE_PA_CONFIG, VIV_MASKED_BIT(VIV_STATE_PA_CONFIG_UNK22, 0));
    etna_set_state(cmdPtr, VIV_STATE_SE_LAST_PIXEL_ENABLE, 0x0);
    etna_set_state(cmdPtr, VIV_STATE_GL_FLUSH_CACHE, VIV_STATE_GL_FLUSH_CACHE_COLOR);
    etna_set_state(cmdPtr, VIV_STATE_PE_COLOR_FORMAT, 
            VIV_MASKED_BIT(VIV_STATE_PE_COLOR_FORMAT_UNK16, 0));
    etna_set_state(cmdPtr, VIV_STATE_PE_ALPHA_CONFIG, /* can & all these together */
            VIV_MASKED_BIT(VIV_STATE_PE_ALPHA_CONFIG_BLEND_ENABLE_COLOR, 0) &
            VIV_MASKED_BIT(VIV_STATE_PE_ALPHA_CONFIG_BLEND_ENABLE_ALPHA, 0));
    etna_set_state(cmdPtr, VIV_STATE_PE_ALPHA_CONFIG,
            VIV_MASKED(VIV_STATE_PE_ALPHA_CONFIG_SRC_FUNC_COLOR, BLEND_FUNC_ONE) &
            VIV_MASKED(VIV_STATE_PE_ALPHA_CONFIG_SRC_FUNC_ALPHA, BLEND_FUNC_ONE));
    etna_set_state(cmdPtr, VIV_STATE_PE_ALPHA_CONFIG,
            VIV_MASKED(VIV_STATE_PE_ALPHA_CONFIG_DST_FUNC_COLOR, BLEND_FUNC_ZERO) &
            VIV_MASKED(VIV_STATE_PE_ALPHA_CONFIG_DST_FUNC_ALPHA, BLEND_FUNC_ZERO));
    etna_set_state(cmdPtr, VIV_STATE_PE_ALPHA_CONFIG,
            VIV_MASKED(VIV_STATE_PE_ALPHA_CONFIG_EQ_COLOR, BLEND_EQ_ADD) &
            VIV_MASKED(VIV_STATE_PE_ALPHA_CONFIG_EQ_ALPHA, BLEND_EQ_ADD));
    etna_set_state(cmdPtr, VIV_STATE_PE_ALPHA_BLEND_COLOR, 
            (0 << VIV_STATE_PE_ALPHA_BLEND_COLOR_B__SHIFT) | 
            (0 << VIV_STATE_PE_ALPHA_BLEND_COLOR_G__SHIFT) | 
            (0 << VIV_STATE_PE_ALPHA_BLEND_COLOR_R__SHIFT) | 
            (0 << VIV_STATE_PE_ALPHA_BLEND_COLOR_A__SHIFT));
    
    etna_set_state(cmdPtr, VIV_STATE_PE_ALPHA_OP, VIV_MASKED_BIT(VIV_STATE_PE_ALPHA_OP_ALPHA_TEST, 0));
    etna_set_state(cmdPtr, VIV_STATE_PA_CONFIG, VIV_MASKED_INL(VIV_STATE_PA_CONFIG_CULL_FACE_MODE, OFF));
    etna_set_state(cmdPtr, VIV_STATE_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIV_STATE_PE_DEPTH_CONFIG_WRITE_ENABLE, 0));
    etna_set_state(cmdPtr, VIV_STATE_PE_STENCIL_CONFIG, VIV_MASKED(VIV_STATE_PE_STENCIL_CONFIG_REF_FRONT, 0));
    etna_set_state(cmdPtr, VIV_STATE_PE_STENCIL_OP, VIV_MASKED(VIV_STATE_PE_STENCIL_OP_FUNC_FRONT, COMPARE_FUNC_ALWAYS));
    etna_set_state(cmdPtr, VIV_STATE_PE_STENCIL_OP, VIV_MASKED(VIV_STATE_PE_STENCIL_OP_FUNC_BACK, COMPARE_FUNC_ALWAYS));
    etna_set_state(cmdPtr, VIV_STATE_PE_STENCIL_CONFIG, VIV_MASKED(VIV_STATE_PE_STENCIL_CONFIG_MASK_FRONT, 0xff));
    etna_set_state(cmdPtr, VIV_STATE_PE_STENCIL_CONFIG, VIV_MASKED(VIV_STATE_PE_STENCIL_CONFIG_WRITE_MASK, 0xff));
    etna_set_state(cmdPtr, VIV_STATE_PE_STENCIL_OP, VIV_MASKED(VIV_STATE_PE_STENCIL_OP_FAIL_FRONT, STENCIL_OP_KEEP));
    etna_set_state(cmdPtr, VIV_STATE_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIV_STATE_PE_DEPTH_CONFIG_UNK16, 0));
    etna_set_state(cmdPtr, VIV_STATE_PE_STENCIL_OP, VIV_MASKED(VIV_STATE_PE_STENCIL_OP_FAIL_BACK, STENCIL_OP_KEEP));
    etna_set_state(cmdPtr, VIV_STATE_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIV_STATE_PE_DEPTH_CONFIG_UNK16, 0));
    etna_set_state(cmdPtr, VIV_STATE_PE_STENCIL_OP, VIV_MASKED(VIV_STATE_PE_STENCIL_OP_DEPTH_FAIL_FRONT, STENCIL_OP_KEEP));
    etna_set_state(cmdPtr, VIV_STATE_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIV_STATE_PE_DEPTH_CONFIG_UNK16, 0));
    etna_set_state(cmdPtr, VIV_STATE_PE_STENCIL_OP, VIV_MASKED(VIV_STATE_PE_STENCIL_OP_DEPTH_FAIL_BACK, STENCIL_OP_KEEP));
    etna_set_state(cmdPtr, VIV_STATE_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIV_STATE_PE_DEPTH_CONFIG_UNK16, 0));
    etna_set_state(cmdPtr, VIV_STATE_PE_STENCIL_OP, VIV_MASKED(VIV_STATE_PE_STENCIL_OP_PASS_FRONT, STENCIL_OP_KEEP));
    etna_set_state(cmdPtr, VIV_STATE_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIV_STATE_PE_DEPTH_CONFIG_UNK16, 0));
    etna_set_state(cmdPtr, VIV_STATE_PE_STENCIL_OP, VIV_MASKED(VIV_STATE_PE_STENCIL_OP_PASS_BACK, STENCIL_OP_KEEP));
    etna_set_state(cmdPtr, VIV_STATE_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIV_STATE_PE_DEPTH_CONFIG_UNK16, 0));
    etna_set_state(cmdPtr, VIV_STATE_PE_COLOR_FORMAT, VIV_MASKED(VIV_STATE_PE_COLOR_FORMAT_COMPONENTS, 0xf));

#if 0
    /* set bits at once -- faster? */
    etna_set_state(cmdPtr, VIV_STATE_PE_ALPHA_CONFIG,
             // VIV_STATE_PE_ALPHA_CONFIG_BLEND_ENABLE_COLOR |
             // VIV_STATE_PE_ALPHA_CONFIG_BLEND_ENABLE_ALPHA_MASK |
             (BLEND_FUNC_ONE << VIV_STATE_PE_ALPHA_CONFIG_SRC_FUNC_COLOR__SHIFT) |
             (BLEND_FUNC_ONE << VIV_STATE_PE_ALPHA_CONFIG_SRC_FUNC_ALPHA__SHIFT) |
             (BLEND_FUNC_ZERO << VIV_STATE_PE_ALPHA_CONFIG_DST_FUNC_COLOR__SHIFT) |
             (BLEND_FUNC_ZERO << VIV_STATE_PE_ALPHA_CONFIG_DST_FUNC_ALPHA__SHIFT) |
             (BLEND_EQ_ADD << VIV_STATE_PE_ALPHA_CONFIG_EQ_COLOR__SHIFT) |
             (BLEND_EQ_ADD << VIV_STATE_PE_ALPHA_CONFIG_EQ_ALPHA__SHIFT));
    etna_set_state(cmdPtr, VIV_STATE_PE_ALPHA_OP, ~(VIV_STATE_PE_ALPHA_OP_ALPHA_TEST | VIV_STATE_PE_ALPHA_OP_ALPHA_TEST_MASK));

    etna_set_state(cmdPtr, VIV_STATE_PA_CONFIG, 
            (~(VIV_STATE_PA_CONFIG_CULL_FACE_MODE_MASK | VIV_STATE_PA_CONFIG_CULL_FACE_MODE__MASK)) |
            VIV_STATE_PA_CONFIG_CULL_FACE_MODE_OFF);
    etna_set_state(cmdPtr, VIV_STATE_PE_DEPTH_CONFIG, 
            ~(VIV_STATE_PE_DEPTH_CONFIG_WRITE_ENABLE | VIV_STATE_PE_DEPTH_CONFIG_WRITE_ENABLE_MASK |
              VIV_STATE_PE_DEPTH_CONFIG_UNK16 | VIV_STATE_PE_DEPTH_CONFIG_UNK16_MASK));
    etna_set_state(cmdPtr, VIV_STATE_PE_STENCIL_CONFIG, 
            VIV_STATE_PE_STENCIL_CONFIG_MODE_DISABLED | 
            (0 << VIV_STATE_PE_STENCIL_CONFIG_REF_FRONT__SHIFT) | 
            (0 << VIV_STATE_PE_STENCIL_CONFIG_MASK_FRONT__SHIFT) |
            (0 << VIV_STATE_PE_STENCIL_CONFIG_WRITE_MASK__SHIFT));
    etna_set_state(cmdPtr, VIV_STATE_PE_STENCIL_OP, 
            (COMPARE_FUNC_ALWAYS << VIV_STATE_PE_STENCIL_OP_FUNC_FRONT__SHIFT)|
            (STENCIL_OP_KEEP << VIV_STATE_PE_STENCIL_OP_PASS_FRONT__SHIFT)|
            (STENCIL_OP_KEEP << VIV_STATE_PE_STENCIL_OP_FAIL_FRONT__SHIFT)|
            (STENCIL_OP_KEEP << VIV_STATE_PE_STENCIL_OP_DEPTH_FAIL_FRONT__SHIFT)|
            (COMPARE_FUNC_ALWAYS << VIV_STATE_PE_STENCIL_OP_FUNC_BACK__SHIFT) |
            (STENCIL_OP_KEEP << VIV_STATE_PE_STENCIL_OP_PASS_BACK__SHIFT)|
            (STENCIL_OP_KEEP << VIV_STATE_PE_STENCIL_OP_FAIL_BACK__SHIFT)|
            (STENCIL_OP_KEEP << VIV_STATE_PE_STENCIL_OP_DEPTH_FAIL_BACK__SHIFT));
#endif 
    etna_set_state(cmdPtr, VIV_STATE_SE_DEPTH_SCALE, 0x0);
    etna_set_state(cmdPtr, VIV_STATE_SE_DEPTH_BIAS, 0x0);
    
    etna_set_state(cmdPtr, VIV_STATE_PA_CONFIG, VIV_MASKED_INL(VIV_STATE_PA_CONFIG_FILL_MODE, SOLID));
    etna_set_state(cmdPtr, VIV_STATE_PA_CONFIG, VIV_MASKED_INL(VIV_STATE_PA_CONFIG_SHADE_MODEL, SMOOTH));
    etna_set_state(cmdPtr, VIV_STATE_PE_COLOR_FORMAT, 
            VIV_MASKED(VIV_STATE_PE_COLOR_FORMAT_FORMAT, RS_FORMAT_X8R8G8B8) &
            VIV_MASKED_BIT(VIV_STATE_PE_COLOR_FORMAT_UNK20, 1));

    etna_set_state(cmdPtr, VIV_STATE_PE_COLOR_ADDR, rt_physical); /* ADDR_A */
    etna_set_state(cmdPtr, VIV_STATE_PE_COLOR_STRIDE, 0x700); 
    etna_set_state(cmdPtr, VIV_STATE_GL_MULTI_SAMPLE_CONFIG, 
            VIV_MASKED(VIV_STATE_GL_MULTI_SAMPLE_CONFIG_UNK0, 0x0) &
            VIV_MASKED(VIV_STATE_GL_MULTI_SAMPLE_CONFIG_UNK4, 0xf) &
            VIV_MASKED(VIV_STATE_GL_MULTI_SAMPLE_CONFIG_UNK12, 0x0) &
            VIV_MASKED(VIV_STATE_GL_MULTI_SAMPLE_CONFIG_UNK16, 0x0)
            ); 
    etna_set_state(cmdPtr, VIV_STATE_GL_FLUSH_CACHE, VIV_STATE_GL_FLUSH_CACHE_COLOR);
    etna_set_state(cmdPtr, VIV_STATE_PE_COLOR_FORMAT, VIV_MASKED_BIT(VIV_STATE_PE_COLOR_FORMAT_UNK16, 1));
    etna_set_state(cmdPtr, VIV_STATE_GL_FLUSH_CACHE, VIV_STATE_GL_FLUSH_CACHE_COLOR);
    etna_set_state(cmdPtr, VIV_STATE_TS_COLOR_CLEAR_VALUE, 0);
    etna_set_state(cmdPtr, VIV_STATE_TS_COLOR_STATUS_BASE, rt_ts_physical); /* ADDR_B */
    etna_set_state(cmdPtr, VIV_STATE_TS_COLOR_SURFACE_BASE, rt_physical); /* ADDR_A */
    etna_set_state(cmdPtr, VIV_STATE_TS_MEM_CONFIG, VIV_STATE_TS_MEM_CONFIG_COLOR_FAST_CLEAR); /* ADDR_A */

    etna_set_state(cmdPtr, VIV_STATE_PE_DEPTH_CONFIG, 
            VIV_MASKED_BIT(VIV_STATE_PE_DEPTH_CONFIG_UNK4, 0) &
            VIV_MASKED_BIT(VIV_STATE_PE_DEPTH_CONFIG_UNK26, 1)
            );
    etna_set_state(cmdPtr, VIV_STATE_PE_DEPTH_ADDR, z_physical); /* ADDR_C */
    etna_set_state(cmdPtr, VIV_STATE_PE_DEPTH_STRIDE, 0x380);
    etna_set_state(cmdPtr, VIV_STATE_PE_STENCIL_CONFIG, VIV_MASKED_INL(VIV_STATE_PE_STENCIL_CONFIG_MODE, DISABLED));
    etna_set_state(cmdPtr, VIV_STATE_PE_HDEPTH_CONTROL, VIV_STATE_PE_HDEPTH_CONTROL_FORMAT_DISABLED);
    etna_set_state_f32(cmdPtr, VIV_STATE_PE_DEPTH_NORMALIZE, 65535.0);
    etna_set_state(cmdPtr, VIV_STATE_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIV_STATE_PE_DEPTH_CONFIG_UNK16, 0));
    etna_set_state(cmdPtr, VIV_STATE_GL_FLUSH_CACHE, VIV_STATE_GL_FLUSH_CACHE_DEPTH);

    etna_set_state(cmdPtr, VIV_STATE_TS_DEPTH_CLEAR_VALUE, 0xffffffff);
    etna_set_state(cmdPtr, VIV_STATE_TS_DEPTH_STATUS_BASE, z_ts_physical); /* ADDR_D */
    etna_set_state(cmdPtr, VIV_STATE_TS_DEPTH_SURFACE_BASE, z_physical); /* ADDR_C */
    etna_set_state(cmdPtr, VIV_STATE_TS_MEM_CONFIG, 
            VIV_STATE_TS_MEM_CONFIG_DEPTH_FAST_CLEAR |
            VIV_STATE_TS_MEM_CONFIG_COLOR_FAST_CLEAR |
            VIV_STATE_TS_MEM_CONFIG_DEPTH_16BPP | 
            VIV_STATE_TS_MEM_CONFIG_DEPTH_COMPRESSION);
    etna_set_state(cmdPtr, VIV_STATE_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIV_STATE_PE_DEPTH_CONFIG_UNK16, 1)); /* flip-flopping once again */

    /* Warm up RS on aux render target */
    etna_warm_up_rs(cmdPtr, aux_rt_physical, aux_rt_ts_physical);

    /* Phew, now that's one hell of a setup; the serious rendering starts now */
    etna_set_state(cmdPtr, VIV_STATE_TS_COLOR_STATUS_BASE, rt_ts_physical); /* ADDR_B */
    etna_set_state(cmdPtr, VIV_STATE_TS_COLOR_SURFACE_BASE, rt_physical); /* ADDR_A */

    /* ... or so we thought */
    etna_warm_up_rs(cmdPtr, aux_rt_physical, aux_rt_ts_physical);

    /* maybe now? */
    etna_set_state(cmdPtr, VIV_STATE_TS_COLOR_STATUS_BASE, rt_ts_physical); /* ADDR_B */
    etna_set_state(cmdPtr, VIV_STATE_TS_COLOR_SURFACE_BASE, rt_physical); /* ADDR_A */
    etna_set_state(cmdPtr, VIV_STATE_GL_FLUSH_CACHE, VIV_STATE_GL_FLUSH_CACHE_COLOR | VIV_STATE_GL_FLUSH_CACHE_DEPTH);
   
    /* nope, not really... */ 
    etna_warm_up_rs(cmdPtr, aux_rt_physical, aux_rt_ts_physical);
    etna_set_state(cmdPtr, VIV_STATE_TS_COLOR_STATUS_BASE, rt_ts_physical); /* ADDR_B */
    etna_set_state(cmdPtr, VIV_STATE_TS_COLOR_SURFACE_BASE, rt_physical); /* ADDR_A */

    /* semaphore time */
    etna_set_state(cmdPtr, VIV_STATE_GL_SEMAPHORE_TOKEN, 
            (SYNC_RECIPIENT_RA<<VIV_STATE_GL_SEMAPHORE_TOKEN_FROM__SHIFT)|
            (SYNC_RECIPIENT_PE<<VIV_STATE_GL_SEMAPHORE_TOKEN_TO__SHIFT)
            );
    etna_set_state(cmdPtr, VIV_STATE_GL_STALL_TOKEN, 
            (SYNC_RECIPIENT_RA<<VIV_STATE_GL_STALL_TOKEN_FROM__SHIFT)|
            (SYNC_RECIPIENT_PE<<VIV_STATE_GL_STALL_TOKEN_TO__SHIFT)
            );

    /* Set up the resolve to clear tile status for main render target */
    etna_set_state(cmdPtr, VIV_STATE_RS_CONFIG,
            (RS_FORMAT_A8R8G8B8 << VIV_STATE_RS_CONFIG_SOURCE_FORMAT__SHIFT) |
            (RS_FORMAT_A8R8G8B8 << VIV_STATE_RS_CONFIG_DEST_FORMAT__SHIFT)
            );
    etna_set_state_multi(cmdPtr, VIV_STATE_RS_DITHER(0), 2, (uint32_t[]){0xffffffff, 0xffffffff});
    etna_set_state(cmdPtr, VIV_STATE_RS_DEST_ADDR, rt_ts_physical); /* ADDR_B */
    etna_set_state(cmdPtr, VIV_STATE_RS_DEST_STRIDE, 0x40);
    etna_set_state(cmdPtr, VIV_STATE_RS_WINDOW_SIZE, 
            (28 << VIV_STATE_RS_WINDOW_SIZE_HEIGHT__SHIFT) |
            (16 << VIV_STATE_RS_WINDOW_SIZE_WIDTH__SHIFT));
    etna_set_state(cmdPtr, VIV_STATE_RS_FILL_VALUE(0), 0x55555555);
    etna_set_state(cmdPtr, VIV_STATE_RS_CLEAR_CONTROL, 
            VIV_STATE_RS_CLEAR_CONTROL_MODE_ENABLED |
            (0xffff << VIV_STATE_RS_CLEAR_CONTROL_BITS__SHIFT));
    etna_set_state(cmdPtr, VIV_STATE_RS_EXTRA_CONFIG, 
            0); /* no AA, no endian switch */
    etna_set_state(cmdPtr, VIV_STATE_RS_KICKER, 
            0xbeebbeeb);
    
    etna_set_state(cmdPtr, VIV_STATE_TS_COLOR_CLEAR_VALUE, 0xff7f7f7f);
    etna_set_state(cmdPtr, VIV_STATE_GL_FLUSH_CACHE, VIV_STATE_GL_FLUSH_CACHE_COLOR);
    etna_set_state(cmdPtr, VIV_STATE_TS_COLOR_CLEAR_VALUE, 0xff7f7f7f);
    etna_set_state(cmdPtr, VIV_STATE_TS_COLOR_STATUS_BASE, rt_ts_physical); /* ADDR_B */
    etna_set_state(cmdPtr, VIV_STATE_TS_COLOR_SURFACE_BASE, rt_physical); /* ADDR_A */
    etna_set_state(cmdPtr, VIV_STATE_TS_MEM_CONFIG, 
            VIV_STATE_TS_MEM_CONFIG_DEPTH_FAST_CLEAR |
            VIV_STATE_TS_MEM_CONFIG_COLOR_FAST_CLEAR |
            VIV_STATE_TS_MEM_CONFIG_DEPTH_16BPP | 
            VIV_STATE_TS_MEM_CONFIG_DEPTH_COMPRESSION);
    etna_set_state(cmdPtr, VIV_STATE_PA_CONFIG, VIV_MASKED_INL(VIV_STATE_PA_CONFIG_CULL_FACE_MODE, CCW));
    etna_set_state(cmdPtr, VIV_STATE_GL_FLUSH_CACHE, VIV_STATE_GL_FLUSH_CACHE_COLOR | VIV_STATE_GL_FLUSH_CACHE_DEPTH);

    etna_set_state(cmdPtr, VIV_STATE_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIV_STATE_PE_DEPTH_CONFIG_WRITE_ENABLE, 0));
    etna_set_state(cmdPtr, VIV_STATE_PE_DEPTH_CONFIG, VIV_MASKED(VIV_STATE_PE_DEPTH_CONFIG_UNK0, 4));
    etna_set_state(cmdPtr, VIV_STATE_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIV_STATE_PE_DEPTH_CONFIG_WRITE_ENABLE, 0));
    etna_set_state(cmdPtr, VIV_STATE_PE_DEPTH_CONFIG, VIV_MASKED(VIV_STATE_PE_DEPTH_CONFIG_DEPTH_FUNC, COMPARE_FUNC_ALWAYS));
    etna_set_state(cmdPtr, VIV_STATE_PE_DEPTH_CONFIG, VIV_MASKED(VIV_STATE_PE_DEPTH_CONFIG_UNK0, 5));
    etna_set_state_f32(cmdPtr, VIV_STATE_PE_DEPTH_NEAR, 0.0);
    etna_set_state_f32(cmdPtr, VIV_STATE_PE_DEPTH_FAR, 1.0);
    etna_set_state_f32(cmdPtr, VIV_STATE_PE_DEPTH_NORMALIZE, 65535.0);

    /* set up primitive assembly */
    etna_set_state_f32(cmdPtr, VIV_STATE_PA_VIEWPORT_OFFSET_Z, 0.0);
    etna_set_state_f32(cmdPtr, VIV_STATE_PA_VIEWPORT_SCALE_Z, 1.0);
    etna_set_state(cmdPtr, VIV_STATE_PE_DEPTH_CONFIG, VIV_MASKED_BIT(VIV_STATE_PE_DEPTH_CONFIG_UNK20, 0));
    etna_set_state_fixp(cmdPtr, VIV_STATE_PA_VIEWPORT_OFFSET_X, 200 << 16);
    etna_set_state_fixp(cmdPtr, VIV_STATE_PA_VIEWPORT_OFFSET_Y, 120 << 16);
    etna_set_state_fixp(cmdPtr, VIV_STATE_PA_VIEWPORT_SCALE_X, 200 << 16);
    etna_set_state_fixp(cmdPtr, VIV_STATE_PA_VIEWPORT_SCALE_Y, 120 << 16);
    etna_set_state_fixp(cmdPtr, VIV_STATE_SE_SCISSOR_LEFT, 0);
    etna_set_state_fixp(cmdPtr, VIV_STATE_SE_SCISSOR_TOP, 0);
    etna_set_state_fixp(cmdPtr, VIV_STATE_SE_SCISSOR_RIGHT, (400 << 16) | 5);
    etna_set_state_fixp(cmdPtr, VIV_STATE_SE_SCISSOR_BOTTOM, (240 << 16) | 5);

    /* shader setup */
    etna_set_state(cmdPtr, VIV_STATE_VS_END_PC, 0x18);
    etna_set_state_multi(cmdPtr, VIV_STATE_VS_INPUT_COUNT, 3, (uint32_t[]){
            /* VIV_STATE_VS_INPUT_COUNT */ (1<<8) | 3,
            /* VIV_STATE_VS_TEMP_REGISTER_CONTROL */ 6 << VIV_STATE_VS_TEMP_REGISTER_CONTROL_NUM_TEMPS__SHIFT,
            /* VIV_STATE_VS_OUTPUT(0) */ 4});
    etna_set_state(cmdPtr, VIV_STATE_VS_START_PC, 0x0);
    etna_set_state_f32(cmdPtr, VIV_STATE_VS_UNIFORMS(45), 0.5); /* u11.y */
    etna_set_state_f32(cmdPtr, VIV_STATE_VS_UNIFORMS(44), 1.0); /* u11.x */
    etna_set_state_f32(cmdPtr, VIV_STATE_VS_UNIFORMS(27), 0.0); /* u6.w */
    etna_set_state_f32(cmdPtr, VIV_STATE_VS_UNIFORMS(23), 20.0); /* u5.w */
    etna_set_state_f32(cmdPtr, VIV_STATE_VS_UNIFORMS(19), 2.0); /* u4.w */

    /* Now load the shader itself */
    uint32_t vs[] = {
        0x01831009, 0x00000000, 0x00000000, 0x203fc048,
        0x02031009, 0x00000000, 0x00000000, 0x203fc058,
        0x07841003, 0x39000800, 0x00000050, 0x00000000,
        0x07841002, 0x39001800, 0x00aa0050, 0x00390048,
        0x07841002, 0x39002800, 0x01540050, 0x00390048,
        0x07841002, 0x39003800, 0x01fe0050, 0x00390048,
        0x03851003, 0x29004800, 0x000000d0, 0x00000000,
        0x03851002, 0x29005800, 0x00aa00d0, 0x00290058,
        0x03811002, 0x29006800, 0x015400d0, 0x00290058,
        0x07851003, 0x39007800, 0x00000050, 0x00000000,
        0x07851002, 0x39008800, 0x00aa0050, 0x00390058,
        0x07851002, 0x39009800, 0x01540050, 0x00390058,
        0x07801002, 0x3900a800, 0x01fe0050, 0x00390058,
        0x0401100c, 0x00000000, 0x00000000, 0x003fc008,
        0x03801002, 0x69000800, 0x01fe00c0, 0x00290038,
        0x03831005, 0x29000800, 0x01480040, 0x00000000,
        0x0383100d, 0x00000000, 0x00000000, 0x00000038,
        0x03801003, 0x29000800, 0x014801c0, 0x00000000,
        0x00801005, 0x29001800, 0x01480040, 0x00000000,
        0x0080108f, 0x3fc06800, 0x00000050, 0x203fc068,
        0x03801003, 0x00000800, 0x01480140, 0x00000000,
        0x04001009, 0x00000000, 0x00000000, 0x200000b8,
        0x02041001, 0x2a804800, 0x00000000, 0x003fc048,
        0x02041003, 0x2a804800, 0x00aa05c0, 0x00000002,
    };
    uint32_t ps[] = {
        0x00000000, 0x00000000, 0x00000000, 0x00000000
    };

    etna_set_state_multi(cmdPtr, VIV_STATE_VS_INST_MEM(0), sizeof(vs)/4, vs);
    etna_set_state(cmdPtr, VIV_STATE_RA_CONTROL, 0x1);
    etna_set_state_multi(cmdPtr, VIV_STATE_PS_END_PC, 2, (uint32_t[]){
            /* VIV_STATE_PS_END_PC */ 0x1,
            /* VIV_STATE_PS_OUTPUT_REG */ 0x1});
    etna_set_state(cmdPtr, VIV_STATE_PS_START_PC, 0x0);
    etna_set_state(cmdPtr, VIV_STATE_PA_SHADER_ATTRIBUTES(0), 0x200);
    etna_set_state(cmdPtr, VIV_STATE_GL_PS_VARYING_NUM_COMPONENTS,  /* one varying, with four components */
            (4 << VIV_STATE_GL_VS_VARYING_NUM_COMPONENTS_VAR0__SHIFT)
            );
    etna_set_state_multi(cmdPtr, VIV_STATE_GL_PS_VARYING_COMPONENT_USE(0), 2, (uint32_t[]){ /* one varying, with four components */
            (VARYING_COMPONENT_USE_USED << VIV_STATE_GL_PS_VARYING_COMPONENT_USE_COMP0__SHIFT) |
            (VARYING_COMPONENT_USE_USED << VIV_STATE_GL_PS_VARYING_COMPONENT_USE_COMP1__SHIFT) |
            (VARYING_COMPONENT_USE_USED << VIV_STATE_GL_PS_VARYING_COMPONENT_USE_COMP2__SHIFT) |
            (VARYING_COMPONENT_USE_USED << VIV_STATE_GL_PS_VARYING_COMPONENT_USE_COMP3__SHIFT)
            , 0
            });
    etna_set_state_multi(cmdPtr, VIV_STATE_PS_INST_MEM(0), sizeof(ps)/4, ps);
    etna_set_state(cmdPtr, VIV_STATE_PS_INPUT_COUNT, (31<<8)|2);
    etna_set_state(cmdPtr, VIV_STATE_PS_TEMP_REGISTER_CONTROL, 
            (2 << VIV_STATE_PS_TEMP_REGISTER_CONTROL_NUM_TEMPS__SHIFT));
    etna_set_state(cmdPtr, VIV_STATE_PS_CONTROL, 
            VIV_STATE_PS_CONTROL_UNK1
            );
    etna_set_state(cmdPtr, VIV_STATE_PA_ATTRIBUTE_ELEMENT_COUNT, 0x100);
    etna_set_state(cmdPtr, VIV_STATE_GL_VS_VARYING_NUM_COMPONENTS,  /* one varying, with four components */
            (4 << VIV_STATE_GL_VS_VARYING_NUM_COMPONENTS_VAR0__SHIFT)
            );
    etna_set_state(cmdPtr, VIV_STATE_VS_LOAD_BALANCING, 0xf3f0582);
    etna_set_state(cmdPtr, VIV_STATE_VS_OUTPUT_COUNT, 2);
    etna_set_state(cmdPtr, VIV_STATE_PA_CONFIG, VIV_MASKED_BIT(VIV_STATE_PA_CONFIG_POINT_SIZE_ENABLE, 0));
    etna_set_state_multi(cmdPtr, VIV_STATE_VS_UNIFORMS(0), 16, (uint32_t[]){ /* matrix */
        0x3fbf00b4, /*   VS.UNIFORMS[0] := 1.492209 (u0.x) */
        0x3fa8f7a3, /*   VS.UNIFORMS[1] := 1.320057 (u0.y) */
        0xc01d7d33, /*   VS.UNIFORMS[2] := -2.460767 (u0.z) */
        0xbf1d7d33, /*   VS.UNIFORMS[3] := -0.615192 (u0.w) */
        0x3e86b73c, /*   VS.UNIFORMS[4] := 0.263117 (u1.x) */
        0x403303b5, /*   VS.UNIFORMS[5] := 2.797101 (u1.y) */
        0x401c0ad2, /*   VS.UNIFORMS[6] := 2.438160 (u1.z) */
        0x3f1c0ad2, /*   VS.UNIFORMS[7] := 0.609540 (u1.w) */
        0xbfc1f304, /*   VS.UNIFORMS[8] := -1.515229 (u2.x) */
        0x3fe49248, /*   VS.UNIFORMS[9] := 1.785714 (u2.y) */
        0xbfffffff, /*   VS.UNIFORMS[10] := -2.000000 (u2.z) */
        0xbeffffff, /*   VS.UNIFORMS[11] := -0.500000 (u2.w) */
        0x00000000, /*   VS.UNIFORMS[12] := 0.000000 (u3.x) */
        0x00000000, /*   VS.UNIFORMS[13] := 0.000000 (u3.y) */
        0x40000000, /*   VS.UNIFORMS[14] := 2.000000 (u3.z) */
        0x41000000  /*   VS.UNIFORMS[15] := 8.000000 (u3.w) */});
    etna_set_state_multi(cmdPtr, VIV_STATE_VS_UNIFORMS(16), 3, (uint32_t[]){0x3f3244ed, 0x3ebd3e50, 0x3f1d7d33}); /* u4.xyz */
    etna_set_state_multi(cmdPtr, VIV_STATE_VS_UNIFORMS(20), 3, (uint32_t[]){0x3dfb782d, 0x3f487f08, 0xbf1c0ad2}); /* u5.xyz */
    etna_set_state_multi(cmdPtr, VIV_STATE_VS_UNIFORMS(24), 3, (uint32_t[]){0xbf3504f3, 0x3effffff, 0x3effffff}); /* u6.xyz */
    etna_set_state_multi(cmdPtr, VIV_STATE_VS_UNIFORMS(28), 16, (uint32_t[]){
        0x3f3244ed, /*   VS.UNIFORMS[28] := 0.696364 (u7.x) */
        0x3ebd3e50, /*   VS.UNIFORMS[29] := 0.369616 (u7.y) */
        0x3f1d7d33, /*   VS.UNIFORMS[30] := 0.615192 (u7.z) */
        0x00000000, /*   VS.UNIFORMS[31] := 0.000000 (u7.w) */
        0x3dfb782d, /*   VS.UNIFORMS[32] := 0.122788 (u8.x) */
        0x3f487f08, /*   VS.UNIFORMS[33] := 0.783188 (u8.y) */
        0xbf1c0ad2, /*   VS.UNIFORMS[34] := -0.609540 (u8.z) */
        0x00000000, /*   VS.UNIFORMS[35] := 0.000000 (u8.w) */
        0xbf3504f3, /*   VS.UNIFORMS[36] := -0.707107 (u9.x) */
        0x3effffff, /*   VS.UNIFORMS[37] := 0.500000 (u9.y) */
        0x3effffff, /*   VS.UNIFORMS[38] := 0.500000 (u9.z) */
        0x00000000, /*   VS.UNIFORMS[39] := 0.000000 (u9.w) */
        0x00000000, /*   VS.UNIFORMS[40] := 0.000000 (u10.x) */
        0x00000000, /*   VS.UNIFORMS[41] := 0.000000 (u10.y) */
        0xc1000000, /*   VS.UNIFORMS[42] := -8.000000 (u10.z) */
        0x3f800000, /*   VS.UNIFORMS[43] := 1.000000 (u10.w) */});
    etna_set_state(cmdPtr, VIV_STATE_FE_VERTEX_STREAM_BASE_ADDR, vtx_physical); /* ADDR_E */
    etna_set_state(cmdPtr, VIV_STATE_FE_VERTEX_STREAM_CONTROL, 
            0x24 << VIV_STATE_FE_VERTEX_STREAM_CONTROL_VERTEX_STRIDE__SHIFT);
    etna_set_state(cmdPtr, VIV_STATE_FE_VERTEX_ELEMENT_CONFIG(0), 
            VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_TYPE_FLOAT |
            (ENDIAN_MODE_NO_SWAP << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_ENDIAN__SHIFT) |
            (0 << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_STREAM__SHIFT) |
            (3 <<VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_NUM__SHIFT) |
            VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_NORMALIZE_OFF |
            (0x0 << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_START__SHIFT) |
            (0xc << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_END__SHIFT));
    etna_set_state(cmdPtr, VIV_STATE_FE_VERTEX_ELEMENT_CONFIG(1), 
            VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_TYPE_FLOAT |
            (ENDIAN_MODE_NO_SWAP << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_ENDIAN__SHIFT) |
            (0 << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_STREAM__SHIFT) |
            (3 <<VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_NUM__SHIFT) |
            VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_NORMALIZE_OFF |
            (0xc << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_START__SHIFT) |
            (0x18 << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_END__SHIFT));
    etna_set_state(cmdPtr, VIV_STATE_FE_VERTEX_ELEMENT_CONFIG(2), 
            VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_TYPE_FLOAT |
            (ENDIAN_MODE_NO_SWAP << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_ENDIAN__SHIFT) |
            VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_NONCONSECUTIVE |
            (0 << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_STREAM__SHIFT) |
            (3 <<VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_NUM__SHIFT) |
            VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_NORMALIZE_OFF |
            (0x18 << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_START__SHIFT) |
            (0x24 << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_END__SHIFT));
    etna_set_state(cmdPtr, VIV_STATE_VS_INPUT(0), 0x20100);
    etna_set_state(cmdPtr, VIV_STATE_PA_CONFIG, VIV_MASKED_BIT(VIV_STATE_PA_CONFIG_POINT_SPRITE_ENABLE, 0));
    etna_draw_primitives(cmdPtr, PRIMITIVE_TYPE_TRIANGLE_STRIP, 0, 2);

    for(int prim=0; prim<5; ++prim) /* this part is repeated 5 times */
    {
        etna_set_state(cmdPtr, VIV_STATE_PS_INPUT_COUNT, (31<<8)|2);
        etna_set_state(cmdPtr, VIV_STATE_PS_TEMP_REGISTER_CONTROL, 
                (2 << VIV_STATE_PS_TEMP_REGISTER_CONTROL_NUM_TEMPS__SHIFT));
        etna_set_state(cmdPtr, VIV_STATE_PS_CONTROL, 
                VIV_STATE_PS_CONTROL_UNK1
                );
        etna_set_state(cmdPtr, VIV_STATE_PA_ATTRIBUTE_ELEMENT_COUNT, 0x100);
        etna_set_state(cmdPtr, VIV_STATE_GL_VS_VARYING_NUM_COMPONENTS,  /* one varying, with four components */
                (4 << VIV_STATE_GL_VS_VARYING_NUM_COMPONENTS_VAR0__SHIFT)
                );
        etna_set_state(cmdPtr, VIV_STATE_VS_LOAD_BALANCING, 0xf3f0582);
        etna_set_state(cmdPtr, VIV_STATE_VS_OUTPUT_COUNT, 2);
        etna_set_state(cmdPtr, VIV_STATE_PA_CONFIG, VIV_MASKED_BIT(VIV_STATE_PA_CONFIG_POINT_SIZE_ENABLE, 0));
        etna_set_state(cmdPtr, VIV_STATE_FE_VERTEX_STREAM_BASE_ADDR, vtx_physical); /* ADDR_E */
        etna_set_state(cmdPtr, VIV_STATE_FE_VERTEX_STREAM_CONTROL, 
                0x24 << VIV_STATE_FE_VERTEX_STREAM_CONTROL_VERTEX_STRIDE__SHIFT);
        etna_set_state(cmdPtr, VIV_STATE_FE_VERTEX_ELEMENT_CONFIG(0), 
                VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_TYPE_FLOAT |
                (ENDIAN_MODE_NO_SWAP << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_ENDIAN__SHIFT) |
                (0 << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_STREAM__SHIFT) |
                (3 <<VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_NUM__SHIFT) |
                VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_NORMALIZE_OFF |
                (0x0 << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_START__SHIFT) |
                (0xc << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_END__SHIFT));
        etna_set_state(cmdPtr, VIV_STATE_FE_VERTEX_ELEMENT_CONFIG(1), 
                VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_TYPE_FLOAT |
                (ENDIAN_MODE_NO_SWAP << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_ENDIAN__SHIFT) |
                (0 << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_STREAM__SHIFT) |
                (3 <<VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_NUM__SHIFT) |
                VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_NORMALIZE_OFF |
                (0xc << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_START__SHIFT) |
                (0x18 << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_END__SHIFT));
        etna_set_state(cmdPtr, VIV_STATE_FE_VERTEX_ELEMENT_CONFIG(2), 
                VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_TYPE_FLOAT |
                (ENDIAN_MODE_NO_SWAP << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_ENDIAN__SHIFT) |
                VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_NONCONSECUTIVE |
                (0 << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_STREAM__SHIFT) |
                (3 <<VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_NUM__SHIFT) |
                VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_NORMALIZE_OFF |
                (0x18 << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_START__SHIFT) |
                (0x24 << VIV_STATE_FE_VERTEX_ELEMENT_CONFIG_END__SHIFT));
        etna_set_state(cmdPtr, VIV_STATE_VS_INPUT(0), 0x20100);
        etna_set_state(cmdPtr, VIV_STATE_PA_CONFIG, VIV_MASKED_BIT(VIV_STATE_PA_CONFIG_POINT_SPRITE_ENABLE, 0));
        etna_draw_primitives(cmdPtr, PRIMITIVE_TYPE_TRIANGLE_STRIP, 4 + prim*4, 2);
    }
    etna_set_state(cmdPtr, VIV_STATE_GL_FLUSH_CACHE, VIV_STATE_GL_FLUSH_CACHE_COLOR | VIV_STATE_GL_FLUSH_CACHE_DEPTH);
#ifdef CMD_COMPARE
    /* Set addresses in first command buffer */
    cmdbuf1[0x57] = cmdbuf1[0x67] = cmdbuf1[0x9f] = cmdbuf1[0xbb] = cmdbuf1[0xd9] = cmdbuf1[0xfb] = rt_physical; /* ADDR_A */
    cmdbuf1[0x65] = cmdbuf1[0x9d] = cmdbuf1[0xb9] = cmdbuf1[0xd7] = cmdbuf1[0xe5] = cmdbuf1[0xf9] = rt_ts_physical; /* ADDR_B */
    cmdbuf1[0x6d] = cmdbuf1[0x7f] = z_physical; /* ADDR_C */
    cmdbuf1[0x7d] = z_ts_physical; /* ADDR_D */
    cmdbuf1[0x87] = cmdbuf1[0xa3] = cmdbuf1[0xc1] = aux_rt_ts_physical; /* ADDR_G */
    cmdbuf1[0x89] = cmdbuf1[0x8f] = cmdbuf1[0x93] 
        = cmdbuf1[0xa5] = cmdbuf1[0xab] = cmdbuf1[0xaf] 
        = cmdbuf1[0xc3] = cmdbuf1[0xc9] = cmdbuf1[0xcd] = aux_rt_physical; /* ADDR_F */
    cmdbuf1[0x1f3] = cmdbuf1[0x215] = cmdbuf1[0x237] 
        = cmdbuf1[0x259] = cmdbuf1[0x27b] = cmdbuf1[0x29d] = vtx_physical; /* ADDR_E */
    
    /* Count differences between generated and stored command buffer */
    int diff = 0;
    for(int idx=8; idx<sizeof(cmdbuf1)/4; ++idx)
    {
        uint32_t cmdbuf_word = cmdbuf1[idx];
        uint32_t my_word = *(uint32_t*)((size_t)cmdPtr->logical + cmdPtr->startOffset + idx*4);
        printf("/*%08x*/ %08x ref:%08x ", idx, my_word, cmdbuf_word);
        if(is_padding[cmdPtr->startOffset/4 + idx])
        {
            printf("PAD");
        } else if(cmdbuf_word != my_word)
        {
            diff += 1;
            printf("DIFF");
        }
        printf("\n");
    }
    printf("Number of differences: %i\n", diff);
    uint32_t size_cmd = sizeof(cmdbuf1)/4;
    uint32_t size_my = (cmdPtr->offset - cmdPtr->startOffset)/4;
    printf("Sizes: %i %i\n", size_cmd, size_my);
    /* Must exactly match */
    if(diff != 0 || size_cmd != size_my)
        exit(1);
#endif

    /* Submit first command buffer */
    commandBuffer.free = commandBuffer.bytes - commandBuffer.offset - 0x8;
    printf("[1] startOffset=%08x, offset=%08x, free=%08x\n", (uint32_t)commandBuffer.startOffset, (uint32_t)commandBuffer.offset, (uint32_t)commandBuffer.free);
    if(viv_commit(&commandBuffer, &contextBuffer) != 0)
    {
        fprintf(stderr, "Error committing first command buffer\n");
        exit(1);
    }

    /* After the first COMMIT, allocate contiguous memory for context and set
     * bytes, physical, logical, link, inUse */
    printf("Context assigned index: %i\n", (uint32_t)contextBuffer.id);
    viv_addr_t cbuf0_physical = 0;
    void *cbuf0_logical = 0;
    size_t cbuf0_bytes = 0;
    if(viv_alloc_contiguous(contextBuffer.bufferSize, &cbuf0_physical, &cbuf0_logical, &cbuf0_bytes)!=0)
    {
        fprintf(stderr, "Error allocating contiguous host memory for context\n");
        exit(1);
    }
    printf("Allocated buffer (size 0x%x) for context: phys=%08x log=%08x\n", (int)cbuf0_bytes, (int)cbuf0_physical, (int)cbuf0_logical);
    contextBuffer.bytes = cbuf0_bytes; /* actual size of buffer */
    contextBuffer.physical = (void*)cbuf0_physical;
    contextBuffer.logical = cbuf0_logical;
    contextBuffer.link = ((uint32_t*)cbuf0_logical) + contextBuffer.linkIndex;
    contextBuffer.inUse = (gctBOOL*)(((uint32_t*)cbuf0_logical) + contextBuffer.inUseIndex);

    /* Submit second command buffer, with updated context.
     * Second command buffer fills the background.
     */
    cmdbuf2[0x1d] = cmdbuf2[0x1f] = rt_physical;
    commandBuffer.startOffset = commandBuffer.offset + 0x18; /* Make space for LINK */
    memcpy((void*)((size_t)commandBuffer.logical + commandBuffer.startOffset), cmdbuf2, sizeof(cmdbuf2));
    commandBuffer.offset = commandBuffer.startOffset + sizeof(cmdbuf2);
    commandBuffer.free -= sizeof(cmdbuf2) + 0x18;
    printf("[2] startOffset=%08x, offset=%08x, free=%08x\n", (uint32_t)commandBuffer.startOffset, (uint32_t)commandBuffer.offset, (uint32_t)commandBuffer.free);
    if(viv_commit(&commandBuffer, &contextBuffer) != 0)
    {
        fprintf(stderr, "Error committing second command buffer\n");
        exit(1);
    }

    /* Submit third command buffer, with updated context
     * Third command buffer does some cache flush trick?
     * It can be left out without any visible harm.
     **/
    cmdbuf3[0x9] = aux_rt_ts_physical;
    cmdbuf3[0xb] = cmdbuf3[0x11] = cmdbuf3[0x15] = aux_rt_physical;
    cmdbuf3[0x1f] = rt_ts_physical;
    cmdbuf3[0x21] = rt_physical;
    commandBuffer.startOffset = commandBuffer.offset + 0x18;
    memcpy((void*)((size_t)commandBuffer.logical + commandBuffer.startOffset), cmdbuf3, sizeof(cmdbuf3));
    commandBuffer.offset = commandBuffer.startOffset + sizeof(cmdbuf3);
    commandBuffer.free -= sizeof(cmdbuf3) + 0x18;
    printf("[3] startOffset=%08x, offset=%08x, free=%08x\n", (uint32_t)commandBuffer.startOffset, (uint32_t)commandBuffer.offset, (uint32_t)commandBuffer.free);
    if(viv_commit(&commandBuffer, &contextBuffer) != 0)
    {
        fprintf(stderr, "Error committing third command buffer\n");
        exit(1);
    }

    /* Submit event queue with SIGNAL, fromWhere=gcvKERNEL_PIXEL (wait for pixel engine to finish) */
    int sig_id = 0;
    if(viv_user_signal_create(0, &sig_id) != 0) /* automatic resetting signal */
    {
        fprintf(stderr, "Cannot create user signal\n");
        exit(1);
    }
    printf("Created user signal %i\n", sig_id);
    if(viv_event_queue_signal(sig_id, gcvKERNEL_PIXEL) != 0)
    {
        fprintf(stderr, "Cannot queue GPU signal\n");
        exit(1);
    }

    /* Wait for signal */
    if(viv_user_signal_wait(sig_id, SIG_WAIT_INDEFINITE) != 0)
    {
        fprintf(stderr, "Cannot wait for signal\n");
        exit(1);
    }

    /* Allocate video memory for BITMAP, lock */
    gcuVIDMEM_NODE_PTR bmp_node = 0;
    if(viv_alloc_linear_vidmem(0x5dc00, 0x40, gcvSURF_BITMAP, gcvPOOL_DEFAULT, &bmp_node)!=0)
    {
        fprintf(stderr, "Error allocating bitmap status memory\n");
        exit(1);
    }
    printf("Allocated bitmap node: node=%08x\n", (uint32_t)bmp_node);
    
    viv_addr_t bmp_physical = 0;
    void *bmp_logical = 0;
    if(viv_lock_vidmem(bmp_node, &bmp_physical, &bmp_logical)!=0)
    {
        fprintf(stderr, "Error locking bmp memory\n");
        exit(1);
    }
    memset(bmp_logical, 0xff, 0x5dc00); /* clear previous result */
    printf("Locked bmp: phys=%08x log=%08x\n", (uint32_t)bmp_physical, (uint32_t)bmp_logical);

    /* Submit fourth command buffer, updating context.
     * Fourth command buffer copies render result to bitmap, detiling along the way. 
     */
    cmdbuf4[0x19] = rt_physical;
    cmdbuf4[0x1b] = bmp_physical;
    commandBuffer.startOffset = commandBuffer.offset + 0x18;
    memcpy((void*)((size_t)commandBuffer.logical + commandBuffer.startOffset), cmdbuf4, sizeof(cmdbuf4));
    commandBuffer.offset = commandBuffer.startOffset + sizeof(cmdbuf4);
    commandBuffer.free -= sizeof(cmdbuf4) + 0x18;
    printf("[4] startOffset=%08x, offset=%08x, free=%08x\n", (uint32_t)commandBuffer.startOffset, (uint32_t)commandBuffer.offset, (uint32_t)commandBuffer.free);
    if(viv_commit(&commandBuffer, &contextBuffer) != 0)
    {
        fprintf(stderr, "Error committing fourth command buffer\n");
        exit(1);
    }

    /* Submit event queue with SIGNAL, fromWhere=gcvKERNEL_PIXEL */
    if(viv_event_queue_signal(sig_id, gcvKERNEL_PIXEL) != 0)
    {
        fprintf(stderr, "Cannot queue GPU signal\n");
        exit(1);
    }

    /* Wait for signal */
    if(viv_user_signal_wait(sig_id, SIG_WAIT_INDEFINITE) != 0)
    {
        fprintf(stderr, "Cannot wait for signal\n");
        exit(1);
    }
    bmp_dump32(bmp_logical, 400, 240, false, "/mnt/sdcard/replay.bmp");
    /* Unlock video memory */
    if(viv_unlock_vidmem(bmp_node, gcvSURF_BITMAP, 1) != 0)
    {
        fprintf(stderr, "Cannot unlock vidmem\n");
        exit(1);
    }
    /*
    for(int x=0; x<0x700; ++x)
    {
        uint32_t value = ((uint32_t*)rt_ts_logical)[x];
        printf("Sample ts: %x %08x\n", x*4, value);
    }*/
    printf("Contextbuffer used %i\n", *contextBuffer.inUse);

    viv_close();
    return 0;
}

