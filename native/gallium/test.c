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

/* Beginnings of TGSI->Vivante conversion -- WIP */
/* usage: ./test tgsi_testdata/cube_companion_frag.tgsi */

/* What should the compiler return (see etna_shader_program)?
 *  1) instruction data
 *  2) input-to-temporary mapping (fixed for ps)
 *      *) in case of ps, semantic -> varying id mapping
 *      *) for each varying: number of components used
 *  3) temporary-to-output mapping
 *  4) for each input/output: possible semantic (position, color, glpointcoord, ...)
 *  5) immediates base offset, immediate data
 *  6) used texture units (and possibly the TGSI_TEXTURE_* type); not needed to configure the hw, but useful
 *       for error checking
 *  7) enough information to add the z=(z+w)/2.0 necessary for older chips
 *
 *  Empty shaders are not allowed, should always at least generate a NOP. Also if there is a label
 *  at the end of the shader, an extra NOP should be generated as jump target.
 *
 * TODO
 * * Point texture coordinate
 */
#include "tgsi/tgsi_text.h"
#include "tgsi/tgsi_iterate.h"
#include "tgsi/tgsi_dump.h"
#include "tgsi/tgsi_parse.h"
#include "tgsi/tgsi_strings.h"
#include "pipe/p_shader_tokens.h"
#include "util/u_memory.h"
#include "util/u_math.h"

#include "etna_util.h"
#include "etna_asm.h"
#include "isa.xml.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* load a text file into memory; simple utility function, should not stay here */
static char *
load_text_file(const char *file_name)
{
    char *text = NULL;
    size_t size;
    size_t total_read = 0;
    FILE *fp = fopen(file_name, "rb");

    if (!fp) {
        return NULL;
    }

    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    text = (char *) malloc(size + 1);
    if (text != NULL) {
        do {
            size_t bytes = fread(text + total_read,
                         1, size - total_read, fp);
            if (bytes < size - total_read) {
                free(text);
                text = NULL;
                break;
            }

            if (bytes == 0) {
                break;
            }

            total_read += bytes;
        } while (total_read < size);

        text[total_read] = '\0';
    }

    fclose(fp);

    return text;
}

/* XXX some of these such as ETNA_MAX_LABELS are pretty arbitrary limits, may be better to switch
 * to dynamic allocation at some point. 
 */
#define ETNA_MAX_TEMPS (64)
#define ETNA_MAX_TOKENS (1024)
#define ETNA_MAX_IMM (1024)  /* max const+imm in 32-bit words */
#define ETNA_MAX_DECL (2048)  /* max declarations */
#define ETNA_MAX_DEPTH (32)
#define ETNA_MAX_LABELS (64)
#define ETNA_MAX_INSTRUCTIONS (1024)
#define ETNA_NUM_VARYINGS 16

struct etna_native_reg
{
    unsigned valid:1;
    unsigned is_tex:1; /* is texture unit, overrides rgroup */
    unsigned rgroup:3;
    unsigned id:9;
};

struct etna_reg_desc
{
    enum tgsi_file_type file;
    int idx;
    bool active;
    int first_use; /* instruction id of first use (scope begin) */
    int last_use;  /* instruction id of last use (scope end) */

    struct etna_native_reg native; /* native register to map to */
    unsigned rgroup:3; /* INST_RGROUP */
    unsigned usage_mask:4; /* usage, per channel */
    bool has_semantic;
    struct tgsi_declaration_semantic semantic;
    struct tgsi_declaration_interp interp;
};

/* a label */
struct etna_compile_label
{
    int inst_idx;
};

enum etna_compile_frame_type {
    ETNA_COMPILE_FRAME_IF, /* IF/ELSE/ENDIF */
};
/* nesting scope frame (LOOP, IF, ...) during compilation
 */
struct etna_compile_frame
{
    enum etna_compile_frame_type type;
    struct etna_compile_label *lbl_else;
    struct etna_compile_label *lbl_endif;
};

/* scratch area for compiling shader */
struct etna_compile_data
{
    uint processor; /* TGSI_PROCESSOR_... */

    struct etna_reg_desc *file[TGSI_FILE_COUNT];
    uint file_size[TGSI_FILE_COUNT];
    struct etna_reg_desc decl[ETNA_MAX_DECL];
    uint total_decls;
    bool dead_inst[ETNA_MAX_TOKENS]; /* mark dead instructions */
    uint32_t imm_data[ETNA_MAX_IMM];
    uint32_t imm_base; /* base of immediates (in 32 bit units) */
    uint32_t imm_size; /* size of immediates (in 32 bit units) */
    uint32_t next_free_native;
   
    /* conditionals */
    struct etna_compile_frame frame_stack[ETNA_MAX_DEPTH];
    int frame_sp;
    struct etna_compile_label *lbl_usage[ETNA_MAX_INSTRUCTIONS]; /* label usage reference, per instruction */
    struct etna_compile_label labels[ETNA_MAX_LABELS]; /* XXX use subheap allocation */
    int num_labels;

    /* code generation */
    int inst_ptr; /* current instruction pointer */
    uint32_t code[ETNA_MAX_INSTRUCTIONS*4];

    /* PS only */
    int num_varyings;
    struct tgsi_declaration_semantic varying_semantic[ETNA_NUM_VARYINGS];
    uint32_t varying_pa_attributes[ETNA_NUM_VARYINGS]; /* PA_SHADER_ATTRIBUTES */
    int varying_num_components[ETNA_NUM_VARYINGS];

    /* device profile */
    uint32_t vertex_tex_base; /* base of vertex texture units */
};

/** Register allocation **/
enum reg_sort_order
{
    FIRST_USE_ASC,
    FIRST_USE_DESC,
    LAST_USE_ASC,
    LAST_USE_DESC
};

struct sort_rec
{
    struct etna_reg_desc *ptr;
    int key;
};

static int sort_rec_compar(const struct sort_rec *a, const struct sort_rec *b)
{
    if(a->key < b->key) return -1;
    if(a->key > b->key) return 1;
    return 0;
}

/* create an index on a register set based on certain criteria. */
static int sort_registers(
        struct sort_rec *sorted,
        struct etna_reg_desc *regs,
        int count,
        enum reg_sort_order so)
{
    /* pre-populate keys from active registers */
    int ptr = 0;
    for(int idx=0; idx<count; ++idx)
    {
        /* only interested in active registers now; will only assign inactive ones if no
         * space in active ones */
        if(regs[idx].active)
        {
            sorted[ptr].ptr = &regs[idx];
            switch(so)
            {
            case FIRST_USE_ASC:  sorted[ptr].key = regs[idx].first_use; break;
            case LAST_USE_ASC:   sorted[ptr].key = regs[idx].last_use; break;
            case FIRST_USE_DESC: sorted[ptr].key = -regs[idx].first_use; break;
            case LAST_USE_DESC:  sorted[ptr].key = -regs[idx].last_use; break;
            }
            ptr++;
        }
    }

    qsort(sorted, ptr, sizeof(struct sort_rec), (int (*) (const void *, const void *))sort_rec_compar);
    return ptr;
}

/* Allocate a new, unused, native temp register */
static struct etna_native_reg alloc_new_native_reg(struct etna_compile_data *cd)
{
    assert(cd->next_free_native < ETNA_MAX_TEMPS);
    int rv = cd->next_free_native;
    cd->next_free_native++;
    return (struct etna_native_reg){ .valid=1, .rgroup=INST_RGROUP_TEMP, .id=rv };
}

/* assign TEMPs to native registers */
static void assign_temporaries_to_native(struct etna_compile_data *cd, struct etna_reg_desc *temps, int num_temps)
{
    for(int idx=0; idx<num_temps; ++idx)
    {
        temps[idx].native = alloc_new_native_reg(cd);
    }
}

/* assign inputs and outputs to temporaries 
 * Gallium assumes that the hardware has separate registers for taking input and output,
 * however Vivante GPUs use temporaries both for passing in inputs and passing back outputs.
 * Try to re-use temporary registers where possible.
 */
static void assign_inouts_to_temporaries(struct etna_compile_data *cd, uint file)
{
    bool mode_inputs = (file == TGSI_FILE_INPUT);
    int inout_ptr = 0, num_inouts;
    int temp_ptr = 0, num_temps;
    struct sort_rec inout_order[ETNA_MAX_TEMPS];
    struct sort_rec temps_order[ETNA_MAX_TEMPS];
    num_inouts = sort_registers(inout_order, 
            cd->file[file], cd->file_size[file],
            mode_inputs ? LAST_USE_ASC : FIRST_USE_ASC);
    num_temps = sort_registers(temps_order, 
            cd->file[TGSI_FILE_TEMPORARY], cd->file_size[TGSI_FILE_TEMPORARY],
            mode_inputs ? FIRST_USE_ASC : LAST_USE_ASC);

    while(inout_ptr < num_inouts && temp_ptr < num_temps)
    {
        struct etna_reg_desc *inout = inout_order[inout_ptr].ptr;
        struct etna_reg_desc *temp = temps_order[temp_ptr].ptr;
        if(!inout->active || inout->native.valid) /* Skip if already a native register assigned */
        {
            inout_ptr++;
            continue;
        }
        /* last usage of this input is before or in same instruction of first use of temporary? */
        if(mode_inputs ? (inout->last_use <= temp->first_use) :
                         (inout->first_use >= temp->last_use))
        {
            /* assign it and advance to next input */
            inout->native = temp->native;
            inout_ptr++;
        }
        temp_ptr++;
    }
    /* if we couldn't reuse current ones, allocate new temporaries
     */
    for(inout_ptr=0; inout_ptr<num_inouts; ++inout_ptr)
    {
        struct etna_reg_desc *inout = inout_order[inout_ptr].ptr;
        if(inout->active && !inout->native.valid)
        {
            inout->native = alloc_new_native_reg(cd);
        }
    }
}

/* Allocate an immediate with a certain value and return the index. If 
 * there is already an immediate with that value, return that.
 */
static struct etna_inst_src alloc_imm_u32(struct etna_compile_data *cd, uint32_t value)
{
    int idx;
    for(idx = 0; idx<cd->imm_size; ++idx)
    {
        if(cd->imm_data[idx] == value)
            break;
    }
    if(idx == cd->imm_size) /* allocate new immediate */
    {
        assert(cd->imm_size < ETNA_MAX_IMM);
        idx = cd->imm_size++;
        cd->imm_data[idx] = value;
    }

    /* swizzle so that component with value is returned in all components */
    idx += cd->imm_base;
    struct etna_inst_src imm_src = {
        .use = 1,
        .rgroup = INST_RGROUP_UNIFORM_0,
        .reg = idx/4,
        .swiz = INST_SWIZ_X(idx & 3) | INST_SWIZ_Y(idx & 3) | 
                INST_SWIZ_Z(idx & 3) | INST_SWIZ_W(idx & 3)
    };
    return imm_src;
}

static struct etna_inst_src alloc_imm_f32(struct etna_compile_data *cd, uint32_t value)
{
    return alloc_imm_u32(cd, etna_f32_to_u32(value));
}

/* Pass -- check register file declarations and immediates */
static void etna_compile_parse_declarations(struct etna_compile_data *cd, const struct tgsi_token *tokens)
{
    struct tgsi_parse_context ctx = {};
    unsigned status = TGSI_PARSE_OK;
    status = tgsi_parse_init(&ctx, tokens);
    assert(status == TGSI_PARSE_OK);

    cd->processor = ctx.FullHeader.Processor.Processor;

    while(!tgsi_parse_end_of_tokens(&ctx))
    {
        tgsi_parse_token(&ctx);
        const struct tgsi_full_declaration *decl = 0;
        const struct tgsi_full_immediate *imm = 0;
        switch(ctx.FullToken.Token.Type)
        {
        case TGSI_TOKEN_TYPE_DECLARATION:
            decl = &ctx.FullToken.FullDeclaration;
            cd->file_size[decl->Declaration.File] = MAX2(cd->file_size[decl->Declaration.File], decl->Range.Last+1);
            break;
        case TGSI_TOKEN_TYPE_IMMEDIATE: /* immediates are handled differently from other files; they are not declared 
                                           explicitly, and always add four components */
            imm = &ctx.FullToken.FullImmediate;
            assert(cd->imm_size <= (ETNA_MAX_IMM-4));
            for(int i=0; i<4; ++i)
            {
                cd->imm_data[cd->imm_size++] = imm->u[i].Uint;
            }
            cd->file_size[TGSI_FILE_IMMEDIATE] = cd->imm_size / 4;
            break;
        }
    }
    tgsi_parse_free(&ctx);
}

static void etna_assign_decls(struct etna_compile_data *cd)
{
    uint idx=0;
    for(int x=0; x<TGSI_FILE_COUNT; ++x)
    {
        cd->file[x] = &cd->decl[idx];
        for(int sub=0; sub<cd->file_size[x]; ++sub)
        {
            cd->decl[idx].file = x;
            cd->decl[idx].idx = sub;
            idx++;
        }
    }
    cd->total_decls = idx;
}

/* Pass -- check usage of temporaries, inputs, outputs */
static void etna_compile_pass_check_usage(struct etna_compile_data *cd, const struct tgsi_token *tokens)
{
    struct tgsi_parse_context ctx = {};
    unsigned status = TGSI_PARSE_OK;
    status = tgsi_parse_init(&ctx, tokens);
    assert(status == TGSI_PARSE_OK);

    for(int idx=0; idx<cd->total_decls; ++idx)
    {
        cd->decl[idx].active = false;
        cd->decl[idx].first_use = cd->decl[idx].last_use = -1;
    }

    int inst_idx = 0;
    while(!tgsi_parse_end_of_tokens(&ctx))
    {
        tgsi_parse_token(&ctx);
        /* find out max register #s used 
         * for every register mark first and last instruction id where it's used
         * this allows finding slots that can be used as input and output registers
         *
         * XXX in the case of loops this needs special care, as the last usage of a register
         * inside a loop means it can still be used on next loop iteration. The register can
         * only be declared "free" after the loop finishes.
         * Same for inputs: the first usage of a register inside a loop doesn't mean that the register
         * won't have been overwritten in previous iteration. The register can only be declared free before the loop
         * starts.
         * The proper way would be to do full dominator / post-dominator analysis (especially with more complicated
         * control flow such as direct branch instructions) but not for now...
         */
        const struct tgsi_full_declaration *decl = 0;
        const struct tgsi_full_instruction *inst = 0;
        switch(ctx.FullToken.Token.Type)
        {
        case TGSI_TOKEN_TYPE_DECLARATION:
            decl = &ctx.FullToken.FullDeclaration;
            for(int idx=decl->Range.First; idx<=decl->Range.Last; ++idx)
            {
                cd->file[decl->Declaration.File][idx].usage_mask = decl->Declaration.UsageMask;
                cd->file[decl->Declaration.File][idx].has_semantic = decl->Declaration.Semantic;
                cd->file[decl->Declaration.File][idx].semantic = decl->Semantic;
                cd->file[decl->Declaration.File][idx].interp = decl->Interp;
            }
            break;
        case TGSI_TOKEN_TYPE_INSTRUCTION:
            /* iterate over operands */
            inst = &ctx.FullToken.FullInstruction;
            //printf("instruction: opcode=%i num_src=%i num_dest=%i\n", inst->Instruction.Opcode,
            //        inst->Instruction.NumSrcRegs, inst->Instruction.NumDstRegs);
            /* iterate over destination registers */
            for(int idx=0; idx<inst->Instruction.NumDstRegs; ++idx)
            {
                struct etna_reg_desc *reg_desc = &cd->file[inst->Dst[idx].Register.File][inst->Dst[idx].Register.Index];
                if(reg_desc->first_use == -1)
                    reg_desc->first_use = inst_idx;
                reg_desc->last_use = inst_idx;
                reg_desc->active = true;
            }
            /* iterate over source registers */
            for(int idx=0; idx<inst->Instruction.NumSrcRegs; ++idx)
            {
                struct etna_reg_desc *reg_desc = &cd->file[inst->Src[idx].Register.File][inst->Src[idx].Register.Index];
                if(reg_desc->first_use == -1)
                    reg_desc->first_use = inst_idx;
                reg_desc->last_use = inst_idx;
                reg_desc->active = true;
            }
            inst_idx += 1;
            break;
        default:
            break;
        }
    }
    tgsi_parse_free(&ctx);
}

/* Pass -- optimize outputs 
 * Mesa tends to generate code like this at the end if their shaders
 *   MOV OUT[1], TEMP[2]
 *   MOV OUT[0], TEMP[0]
 *   MOV OUT[2], TEMP[1]
 * Recognize if 
 * a) there is only a single assignment to an output register and
 * b) the temporary is not used after that
 * Also recognize direct assignment of IN to OUT (passthrough)
 **/
static void etna_compile_pass_optimize_outputs(struct etna_compile_data *cd, const struct tgsi_token *tokens)
{
    struct tgsi_parse_context ctx = {};
    unsigned status = TGSI_PARSE_OK;
    status = tgsi_parse_init(&ctx, tokens);
    assert(status == TGSI_PARSE_OK);

    int inst_idx = 0;
    while(!tgsi_parse_end_of_tokens(&ctx))
    {
        tgsi_parse_token(&ctx);
        const struct tgsi_full_instruction *inst = 0;
        switch(ctx.FullToken.Token.Type)
        {
        case TGSI_TOKEN_TYPE_INSTRUCTION:
            /* iterate over operands */
            inst = &ctx.FullToken.FullInstruction;
            switch(inst->Instruction.Opcode)
            {
            case TGSI_OPCODE_MOV: 
                if(inst->Dst[0].Register.File == TGSI_FILE_OUTPUT &&
                   inst->Src[0].Register.File == TGSI_FILE_TEMPORARY)
                {
                    uint out_idx = inst->Dst[0].Register.Index;
                    uint reg_idx = inst->Src[0].Register.Index;
                    if(cd->file[TGSI_FILE_TEMPORARY][reg_idx].last_use == inst_idx)
                    {
                        /* this is the last use of the temp, good */
                        cd->file[TGSI_FILE_OUTPUT][out_idx].native = cd->file[TGSI_FILE_TEMPORARY][reg_idx].native;
                        /* prevent temp from being re-used for the rest of the shader */
                        cd->file[TGSI_FILE_TEMPORARY][reg_idx].last_use = ETNA_MAX_TOKENS;
                        /* mark this MOV instruction as a no-op */
                        cd->dead_inst[inst_idx] = true;
                    }
                }
                /* direct assignment of input to output -- 
                 * allocate a new register, and associate both input and output to it */
                if(inst->Dst[0].Register.File == TGSI_FILE_OUTPUT &&
                   inst->Src[0].Register.File == TGSI_FILE_INPUT)
                {
                    uint out_idx = inst->Dst[0].Register.Index;
                    uint in_idx = inst->Src[0].Register.Index;

                    cd->file[TGSI_FILE_OUTPUT][out_idx].native = cd->file[TGSI_FILE_INPUT][in_idx].native = 
                        alloc_new_native_reg(cd);
                    /* mark this MOV instruction as a no-op */
                    cd->dead_inst[inst_idx] = true;
                }
                break;
            default: ;
            }
            inst_idx += 1;
            break;
        }
    }
    tgsi_parse_free(&ctx);
}

/* emit instruction and append to program */
static void emit_inst(struct etna_compile_data *cd, const struct etna_inst *inst)
{
    assert(cd->inst_ptr <= ETNA_MAX_INSTRUCTIONS);
    etna_assemble(&cd->code[cd->inst_ptr*4], inst);
    cd->inst_ptr ++;
}

/* convert destination operand */
static struct etna_inst_dst convert_dst(struct etna_compile_data *cd, const struct tgsi_full_dst_register *in)
{
    struct etna_inst_dst rv = {
        .use = 1,
        .comps = in->Register.WriteMask,
    };
    struct etna_native_reg native_reg = cd->file[in->Register.File][in->Register.Index].native;
    assert(native_reg.valid);
    assert(!native_reg.is_tex && native_reg.rgroup == INST_RGROUP_TEMP); /* can only assign to temporaries */
    rv.reg = native_reg.id;
    return rv;
}

/* convert texture operand */
static struct etna_inst_tex convert_tex(struct etna_compile_data *cd, const struct tgsi_full_src_register *in, const struct tgsi_instruction_texture *tex)
{
    struct etna_inst_tex rv = {
        // XXX .amode, .swiz
    };
    struct etna_native_reg native_reg = cd->file[in->Register.File][in->Register.Index].native;
    assert(native_reg.is_tex);
    assert(native_reg.valid);
    rv.id = native_reg.id;
    return rv;
}

/* convert source operand */
static struct etna_inst_src convert_src(struct etna_compile_data *cd, const struct tgsi_full_src_register *in)
{
    struct etna_inst_src rv = {
        .use = 1,
        .swiz = INST_SWIZ_X(in->Register.SwizzleX) | INST_SWIZ_Y(in->Register.SwizzleY) |
                INST_SWIZ_Z(in->Register.SwizzleZ) | INST_SWIZ_W(in->Register.SwizzleW),
        .neg = in->Register.Negate,
        .abs = in->Register.Absolute,
        // XXX .amode
    };
    struct etna_native_reg native_reg = cd->file[in->Register.File][in->Register.Index].native;
    assert(native_reg.valid);
    assert(!native_reg.is_tex);
    rv.rgroup = native_reg.rgroup;
    rv.reg = native_reg.id;
    return rv;
}

/* create a new label */
static struct etna_compile_label *alloc_new_label(struct etna_compile_data *cd)
{
    assert(cd->num_labels < ETNA_MAX_LABELS);
    struct etna_compile_label *rv = &cd->labels[cd->num_labels++];
    rv->inst_idx = -1; /* start by point to no specific instruction */
    return rv;
}

/* place label at current instruction pointer */
static void label_place(struct etna_compile_data *cd, struct etna_compile_label *label)
{
    label->inst_idx = cd->inst_ptr;
}

/* mark label use at current instruction.
 * target of the label will be filled in in the marked instruction's src2.imm slot as soon
 * as the value becomes known.
 */
static void label_mark_use(struct etna_compile_data *cd, struct etna_compile_label *label)
{
    assert(cd->inst_ptr < ETNA_MAX_INSTRUCTIONS);
    cd->lbl_usage[cd->inst_ptr] = label;
}

/* Pass -- compile instructions */
static void etna_compile_pass_generate_code(struct etna_compile_data *cd, const struct tgsi_token *tokens)
{
    struct tgsi_parse_context ctx = {};
    unsigned status = TGSI_PARSE_OK;
    status = tgsi_parse_init(&ctx, tokens);
    assert(status == TGSI_PARSE_OK);

    int inst_idx = 0;
    while(!tgsi_parse_end_of_tokens(&ctx))
    {
        tgsi_parse_token(&ctx);
        const struct tgsi_full_instruction *inst = 0;
        switch(ctx.FullToken.Token.Type)
        {
        case TGSI_TOKEN_TYPE_INSTRUCTION:
            /* iterate over operands */
            inst = &ctx.FullToken.FullInstruction;
            if(cd->dead_inst[inst_idx]) /* skip dead instructions */
            {
                inst_idx++;
                continue;
            }
            assert(inst->Instruction.Saturate != TGSI_SAT_MINUS_PLUS_ONE);
            int sat = (inst->Instruction.Saturate == TGSI_SAT_ZERO_ONE);
            switch(inst->Instruction.Opcode)
            {
            case TGSI_OPCODE_ARL: assert(0); break;
            case TGSI_OPCODE_MOV: 
                emit_inst(cd, &(struct etna_inst) {
                        .opcode = INST_OPCODE_MOV,
                        .sat = sat,
                        .dst = convert_dst(cd, &inst->Dst[0]),
                        .src[2] = convert_src(cd, &inst->Src[0]), 
                        });
                break;
            case TGSI_OPCODE_LIT: assert(0); break;
            case TGSI_OPCODE_RCP: 
                emit_inst(cd, &(struct etna_inst) {
                        .opcode = INST_OPCODE_RCP,
                        .sat = sat,
                        .dst = convert_dst(cd, &inst->Dst[0]),
                        .src[2] = convert_src(cd, &inst->Src[0]), 
                        });
                break;
            case TGSI_OPCODE_RSQ: 
                emit_inst(cd, &(struct etna_inst) {
                        .opcode = INST_OPCODE_RSQ,
                        .sat = sat,
                        .dst = convert_dst(cd, &inst->Dst[0]),
                        .src[2] = convert_src(cd, &inst->Src[0]), 
                        });
                break;
            case TGSI_OPCODE_EXP: assert(0); break;
            case TGSI_OPCODE_LOG: assert(0); break;
            case TGSI_OPCODE_MUL: 
                emit_inst(cd, &(struct etna_inst) {
                        .opcode = INST_OPCODE_MUL,
                        .sat = sat,
                        .dst = convert_dst(cd, &inst->Dst[0]),
                        .src[0] = convert_src(cd, &inst->Src[0]),
                        .src[1] = convert_src(cd, &inst->Src[1]), 
                        });
                break;
            case TGSI_OPCODE_ADD: 
                emit_inst(cd, &(struct etna_inst) {
                        .opcode = INST_OPCODE_ADD,
                        .sat = sat,
                        .dst = convert_dst(cd, &inst->Dst[0]),
                        .src[0] = convert_src(cd, &inst->Src[0]),
                        .src[2] = convert_src(cd, &inst->Src[1]), 
                        });
                break;
            case TGSI_OPCODE_DP3: 
                emit_inst(cd, &(struct etna_inst) {
                        .opcode = INST_OPCODE_DP3,
                        .sat = sat,
                        .dst = convert_dst(cd, &inst->Dst[0]),
                        .src[0] = convert_src(cd, &inst->Src[0]),
                        .src[1] = convert_src(cd, &inst->Src[1]), 
                        });
                break;
            case TGSI_OPCODE_DP4: 
                emit_inst(cd, &(struct etna_inst) {
                        .opcode = INST_OPCODE_DP4,
                        .sat = sat,
                        .dst = convert_dst(cd, &inst->Dst[0]),
                        .src[0] = convert_src(cd, &inst->Src[0]),
                        .src[1] = convert_src(cd, &inst->Src[1]), 
                        });
                break;
            case TGSI_OPCODE_DST: assert(0); break; /* XXX INST_OPCODE_DST */
            case TGSI_OPCODE_MIN: 
                emit_inst(cd, &(struct etna_inst) {
                        .opcode = INST_OPCODE_SELECT,
                        .cond = INST_CONDITION_GT,
                        .sat = sat,
                        .dst = convert_dst(cd, &inst->Dst[0]),
                        .src[0] = convert_src(cd, &inst->Src[0]),
                        .src[1] = convert_src(cd, &inst->Src[1]), 
                        .src[2] = convert_src(cd, &inst->Src[0]), 
                        });
                break;
            case TGSI_OPCODE_MAX: 
                emit_inst(cd, &(struct etna_inst) {
                        .opcode = INST_OPCODE_SELECT,
                        .cond = INST_CONDITION_LT,
                        .sat = sat,
                        .dst = convert_dst(cd, &inst->Dst[0]),
                        .src[0] = convert_src(cd, &inst->Src[0]),
                        .src[1] = convert_src(cd, &inst->Src[1]), 
                        .src[2] = convert_src(cd, &inst->Src[0]), 
                        });
                break;
            case TGSI_OPCODE_SLT: 
            case TGSI_OPCODE_SGE: 
            case TGSI_OPCODE_SEQ:
            case TGSI_OPCODE_SGT:
            case TGSI_OPCODE_SLE:
            case TGSI_OPCODE_SNE:
            case TGSI_OPCODE_STR: {
                uint cond = 0;
                switch(inst->Instruction.Opcode)
                {
                case TGSI_OPCODE_SLT: cond = INST_CONDITION_LT; break;
                case TGSI_OPCODE_SGE: cond = INST_CONDITION_GE; break;
                case TGSI_OPCODE_SEQ: cond = INST_CONDITION_EQ; break;
                case TGSI_OPCODE_SGT: cond = INST_CONDITION_GT; break;
                case TGSI_OPCODE_SLE: cond = INST_CONDITION_LE; break;
                case TGSI_OPCODE_SNE: cond = INST_CONDITION_NE; break;
                case TGSI_OPCODE_STR: cond = INST_CONDITION_TRUE; break;
                }
                emit_inst(cd, &(struct etna_inst) {
                        .opcode = INST_OPCODE_SET,
                        .cond = cond,
                        .sat = sat,
                        .dst = convert_dst(cd, &inst->Dst[0]),
                        .src[0] = convert_src(cd, &inst->Src[0]),
                        .src[1] = convert_src(cd, &inst->Src[1]), 
                        });
                } break;
            case TGSI_OPCODE_MAD: 
                emit_inst(cd, &(struct etna_inst) {
                        .opcode = INST_OPCODE_MAD,
                        .sat = (inst->Instruction.Saturate == TGSI_SAT_ZERO_ONE),
                        .dst = convert_dst(cd, &inst->Dst[0]),
                        .src[0] = convert_src(cd, &inst->Src[0]),
                        .src[1] = convert_src(cd, &inst->Src[1]), 
                        .src[2] = convert_src(cd, &inst->Src[2]), 
                        });
                break;
            case TGSI_OPCODE_SFL: assert(0); break; /* SET to 0 */
            case TGSI_OPCODE_SUB: assert(0); break; /* XXX INST_OPCODE_ADD + negate? */
            case TGSI_OPCODE_LRP: assert(0); break;
            case TGSI_OPCODE_CND: assert(0); break;
            case TGSI_OPCODE_SQRT: assert(0); break; /* XXX INST_OPCODE_SQRT if HAS_SQRT_TRIG */
            case TGSI_OPCODE_DP2A: assert(0); break;
            case TGSI_OPCODE_FRC: assert(0); break;
            case TGSI_OPCODE_CLAMP: assert(0); break;
            case TGSI_OPCODE_FLR: assert(0); break;
            case TGSI_OPCODE_ROUND: assert(0); break;
            case TGSI_OPCODE_EX2: assert(0); break;
            case TGSI_OPCODE_LG2: assert(0); break;
            case TGSI_OPCODE_POW: assert(0); break;
            case TGSI_OPCODE_XPD: assert(0); break;
            case TGSI_OPCODE_ABS: assert(0); break;
            case TGSI_OPCODE_RCC: assert(0); break;
            case TGSI_OPCODE_DPH: assert(0); break;
            case TGSI_OPCODE_COS: assert(0); break; /* XXX INST_OPCODE_COS */
            case TGSI_OPCODE_SIN: assert(0); break; /* XXX INST_OPCODE_SIN */
            case TGSI_OPCODE_DDX: assert(0); break; /* XXX INST_OPCODE_DSX */
            case TGSI_OPCODE_DDY: assert(0); break; /* XXX INST_OPCODE_DSY */
            case TGSI_OPCODE_KILP: assert(0); break; /* XXX INST_OPCODE_TEXKILL */
            case TGSI_OPCODE_PK2H: assert(0); break;
            case TGSI_OPCODE_PK2US: assert(0); break;
            case TGSI_OPCODE_PK4B: assert(0); break;
            case TGSI_OPCODE_PK4UB: assert(0); break;
            case TGSI_OPCODE_RFL: assert(0); break;
            case TGSI_OPCODE_TEX: 
                emit_inst(cd, &(struct etna_inst) {
                        .opcode = INST_OPCODE_TEXLD,
                        .sat = (inst->Instruction.Saturate == TGSI_SAT_ZERO_ONE),
                        .dst = convert_dst(cd, &inst->Dst[0]),
                        .tex = convert_tex(cd, &inst->Src[1], &inst->Texture),
                        .src[0] = convert_src(cd, &inst->Src[0]),
                        });
                break;
            case TGSI_OPCODE_TXD: assert(0); break;
            case TGSI_OPCODE_TXP: assert(0); break;
            case TGSI_OPCODE_UP2H: assert(0); break;
            case TGSI_OPCODE_UP2US: assert(0); break;
            case TGSI_OPCODE_UP4B: assert(0); break;
            case TGSI_OPCODE_UP4UB: assert(0); break;
            case TGSI_OPCODE_X2D: assert(0); break;
            case TGSI_OPCODE_ARA: assert(0); break;
            case TGSI_OPCODE_ARR: assert(0); break;
            case TGSI_OPCODE_BRA: assert(0); break;
            case TGSI_OPCODE_CAL: assert(0); break; /* CALL */
            case TGSI_OPCODE_RET: assert(0); break;
            case TGSI_OPCODE_SSG: assert(0); break;
            case TGSI_OPCODE_CMP: assert(0); break;
            case TGSI_OPCODE_SCS: assert(0); break;
            case TGSI_OPCODE_TXB: assert(0); break;
            case TGSI_OPCODE_NRM: assert(0); break;
            case TGSI_OPCODE_DIV: assert(0); break;
            case TGSI_OPCODE_DP2: assert(0); break;
            case TGSI_OPCODE_TXL: assert(0); break;
            case TGSI_OPCODE_BRK: assert(0); break; /* break from loop */
            case TGSI_OPCODE_IF:  {
                struct etna_compile_frame *f = &cd->frame_stack[cd->frame_sp++];
                /* push IF to stack */
                f->type = ETNA_COMPILE_FRAME_IF;
                /* create "else" label */
                f->lbl_else = alloc_new_label(cd);
                f->lbl_endif = NULL;
                /* mark position in instruction stream of label reference so that it can be filled in in next pass */
                label_mark_use(cd, f->lbl_else);
                /* create conditional branch to label if src0 EQ 0 */
                emit_inst(cd, &(struct etna_inst) {
                        .opcode = INST_OPCODE_BRANCH,
                        .cond = INST_CONDITION_EQ,
                        .src[0] = convert_src(cd, &inst->Src[0]),
                        .src[1] = alloc_imm_f32(cd, 0.0f),
                        /* imm is filled in later */
                        });
                } break;
            case TGSI_OPCODE_ELSE: {
                assert(cd->frame_sp>0);
                struct etna_compile_frame *f = &cd->frame_stack[cd->frame_sp-1];
                assert(f->type == ETNA_COMPILE_FRAME_IF);
                /* create "endif" label, and branch to endif label */
                f->lbl_endif = alloc_new_label(cd);
                label_mark_use(cd, f->lbl_endif);
                emit_inst(cd, &(struct etna_inst) {
                        .opcode = INST_OPCODE_BRANCH,
                        .cond = INST_CONDITION_TRUE,
                        /* imm is filled in later */
                        });
                /* mark "else" label at this position in instruction stream */
                label_place(cd, f->lbl_else);
                } break;
            case TGSI_OPCODE_ENDIF: {
                assert(cd->frame_sp>0);
                struct etna_compile_frame *f = &cd->frame_stack[--cd->frame_sp];
                assert(f->type == ETNA_COMPILE_FRAME_IF);
                /* assign "endif" or "else" (if no ELSE) label to current position in instruction stream, pop IF */
                if(f->lbl_endif != NULL)
                    label_place(cd, f->lbl_endif);
                else
                    label_place(cd, f->lbl_else);
                } break;
            case TGSI_OPCODE_PUSHA: assert(0); break;
            case TGSI_OPCODE_POPA: assert(0); break;
            case TGSI_OPCODE_CEIL: assert(0); break;
            case TGSI_OPCODE_I2F: assert(0); break;
            case TGSI_OPCODE_NOT: assert(0); break;
            case TGSI_OPCODE_TRUNC: assert(0); break;
            case TGSI_OPCODE_SHL: assert(0); break;
            case TGSI_OPCODE_AND: assert(0); break;
            case TGSI_OPCODE_OR: assert(0); break;
            case TGSI_OPCODE_MOD: assert(0); break;
            case TGSI_OPCODE_XOR: assert(0); break;
            case TGSI_OPCODE_SAD: assert(0); break;
            case TGSI_OPCODE_TXF: assert(0); break;
            case TGSI_OPCODE_TXQ: assert(0); break;
            case TGSI_OPCODE_CONT: assert(0); break;
            case TGSI_OPCODE_EMIT: assert(0); break;
            case TGSI_OPCODE_ENDPRIM: assert(0); break;
            case TGSI_OPCODE_BGNLOOP: assert(0); break;
            case TGSI_OPCODE_BGNSUB: assert(0); break;
            case TGSI_OPCODE_ENDLOOP: assert(0); break;
            case TGSI_OPCODE_ENDSUB: assert(0); break;
            case TGSI_OPCODE_TXQ_LZ: assert(0); break;
            case TGSI_OPCODE_NOP: assert(0); break;
            case TGSI_OPCODE_NRM4: assert(0); break;
            case TGSI_OPCODE_CALLNZ: assert(0); break;
            case TGSI_OPCODE_IFC: assert(0); break;
            case TGSI_OPCODE_BREAKC: assert(0); break;
            case TGSI_OPCODE_KIL: assert(0); break;
            case TGSI_OPCODE_END: /* Nothing to do */ break;
            case TGSI_OPCODE_F2I: assert(0); break;
            case TGSI_OPCODE_IDIV: assert(0); break;
            case TGSI_OPCODE_IMAX: assert(0); break;
            case TGSI_OPCODE_IMIN: assert(0); break;
            case TGSI_OPCODE_INEG: assert(0); break;
            case TGSI_OPCODE_ISGE: assert(0); break;
            case TGSI_OPCODE_ISHR: assert(0); break;
            case TGSI_OPCODE_ISLT: assert(0); break;
            case TGSI_OPCODE_F2U: assert(0); break;
            case TGSI_OPCODE_U2F: assert(0); break;
            case TGSI_OPCODE_UADD: assert(0); break;
            case TGSI_OPCODE_UDIV: assert(0); break;
            case TGSI_OPCODE_UMAD: assert(0); break;
            case TGSI_OPCODE_UMAX: assert(0); break;
            case TGSI_OPCODE_UMIN: assert(0); break;
            case TGSI_OPCODE_UMOD: assert(0); break;
            case TGSI_OPCODE_UMUL: assert(0); break;
            case TGSI_OPCODE_USEQ: assert(0); break;
            case TGSI_OPCODE_USGE: assert(0); break;
            case TGSI_OPCODE_USHR: assert(0); break;
            case TGSI_OPCODE_USLT: assert(0); break;
            case TGSI_OPCODE_USNE: assert(0); break;
            case TGSI_OPCODE_SWITCH: assert(0); break;
            case TGSI_OPCODE_CASE: assert(0); break;
            case TGSI_OPCODE_DEFAULT: assert(0); break;
            case TGSI_OPCODE_ENDSWITCH: assert(0); break;
            case TGSI_OPCODE_SAMPLE: assert(0); break;
            case TGSI_OPCODE_SAMPLE_I: assert(0); break;
            case TGSI_OPCODE_SAMPLE_I_MS: assert(0); break;
            case TGSI_OPCODE_SAMPLE_B: assert(0); break;
            case TGSI_OPCODE_SAMPLE_C: assert(0); break;
            case TGSI_OPCODE_SAMPLE_C_LZ: assert(0); break;
            case TGSI_OPCODE_SAMPLE_D: assert(0); break;
            case TGSI_OPCODE_SAMPLE_L: assert(0); break;
            case TGSI_OPCODE_GATHER4: assert(0); break;
            case TGSI_OPCODE_SVIEWINFO: assert(0); break;
            case TGSI_OPCODE_SAMPLE_POS: assert(0); break;
            case TGSI_OPCODE_SAMPLE_INFO: assert(0); break;
            case TGSI_OPCODE_UARL: assert(0); break;
            case TGSI_OPCODE_UCMP: assert(0); break;
            case TGSI_OPCODE_IABS: assert(0); break;
            case TGSI_OPCODE_ISSG: assert(0); break;
            case TGSI_OPCODE_LOAD: assert(0); break;
            case TGSI_OPCODE_STORE: assert(0); break;
            case TGSI_OPCODE_MFENCE: assert(0); break;
            case TGSI_OPCODE_LFENCE: assert(0); break;
            case TGSI_OPCODE_SFENCE: assert(0); break;
            case TGSI_OPCODE_BARRIER: assert(0); break;
            case TGSI_OPCODE_ATOMUADD: assert(0); break;
            case TGSI_OPCODE_ATOMXCHG: assert(0); break;
            case TGSI_OPCODE_ATOMCAS: assert(0); break;
            case TGSI_OPCODE_ATOMAND: assert(0); break;
            case TGSI_OPCODE_ATOMOR: assert(0); break;
            case TGSI_OPCODE_ATOMXOR: assert(0); break;
            case TGSI_OPCODE_ATOMUMIN: assert(0); break;
            case TGSI_OPCODE_ATOMUMAX: assert(0); break;
            case TGSI_OPCODE_ATOMIMIN: assert(0); break;
            case TGSI_OPCODE_ATOMIMAX: assert(0); break;
            case TGSI_OPCODE_TEX2: assert(0); break;
            case TGSI_OPCODE_TXB2: assert(0); break;
            case TGSI_OPCODE_TXL2: assert(0); break;
            default:
                printf("Unhandled instruction %i\n", inst->Instruction.Opcode);
            }
            inst_idx += 1;
            break;
        }
    }
    tgsi_parse_free(&ctx);
}

/* Allocate CONST and IMM to native ETNA_RGROUP_UNIFORM(x).
 * CONST must be consecutive as const buffers are supposed to be consecutive, and before IMM, as this is
 * more convenient because is possible for the compilation process itself to generate extra 
 * immediates for constants such as pi, one, zero. 
 */
static void assign_constants_and_immediates(struct etna_compile_data *cd)
{
    for(int idx=0; idx<cd->file_size[TGSI_FILE_CONSTANT]; ++idx)
    {
        cd->file[TGSI_FILE_CONSTANT][idx].native.valid = 1;
        cd->file[TGSI_FILE_CONSTANT][idx].native.rgroup = INST_RGROUP_UNIFORM_0;
        cd->file[TGSI_FILE_CONSTANT][idx].native.id = idx;
    }
    /* immediates start after the constants */
    cd->imm_base = cd->file_size[TGSI_FILE_CONSTANT] * 4;
    for(int idx=0; idx<cd->file_size[TGSI_FILE_IMMEDIATE]; ++idx)
    {
        cd->file[TGSI_FILE_IMMEDIATE][idx].native.valid = 1;
        cd->file[TGSI_FILE_IMMEDIATE][idx].native.rgroup = INST_RGROUP_UNIFORM_0;
        cd->file[TGSI_FILE_IMMEDIATE][idx].native.id = cd->imm_base/4 + idx;
    }
    printf("imm base: %i size: %i\n", cd->imm_base, cd->imm_size);
}

/* Assign declared samplers to native texture units */
static void assign_texture_units(struct etna_compile_data *cd)
{
    uint tex_base = 0;
    if(cd->processor == TGSI_PROCESSOR_VERTEX) 
    {
        tex_base = cd->vertex_tex_base;
    }
    for(int idx=0; idx<cd->file_size[TGSI_FILE_SAMPLER]; ++idx)
    {
        cd->file[TGSI_FILE_SAMPLER][idx].native.valid = 1;
        cd->file[TGSI_FILE_SAMPLER][idx].native.is_tex = 1; // overrides rgroup
        cd->file[TGSI_FILE_SAMPLER][idx].native.id = tex_base + idx;
    }
}

/* additional pass to fill in branch targets */
static void etna_compile_fill_in_labels(struct etna_compile_data *cd)
{
    for(int idx=0; idx<cd->inst_ptr ; ++idx)
    {
        if(cd->lbl_usage[idx])
        {
            etna_assemble_set_imm(&cd->code[idx * 4], cd->lbl_usage[idx]->inst_idx);
        }
    }
}

/* compare two etna_native_reg structures, return true if equal */
static bool cmp_etna_native_reg(const struct etna_native_reg to, const struct etna_native_reg from)
{
    return to.valid == from.valid && to.is_tex == from.is_tex && to.rgroup == from.rgroup &&
           to.id == from.id;
}

/* go through all declarations and swap native registers to and from */
static void swap_native_registers(struct etna_compile_data *cd, const struct etna_native_reg to, const struct etna_native_reg from)
{
    if(cmp_etna_native_reg(from, to))
        return; /* Nothing to do */
    for(int idx=0; idx<cd->total_decls; ++idx)
    {
        if(cmp_etna_native_reg(cd->decl[idx].native, from))
        {
            cd->decl[idx].native = to;
        } else if(cmp_etna_native_reg(cd->decl[idx].native, to))
        {
            cd->decl[idx].native = from;
        }
    }
}

/* For PS we need to permute so that inputs are always in temporary 0..N-1.
 * XXX Semantic POS is always t0. If that semantic is not used, avoid t0.
 */
static void permute_ps_inputs(struct etna_compile_data *cd)
{
    /* Special inputs:
     * gl_FragCoord  VARYING_SLOT_POS   TGSI_SEMANTIC_POSITION
     * (*) gl_PointCoord VARYING_SLOT_PNTC  TGSI_SEMANTIC_GENERIC / TGSI_SEMANTIC_PCOORD
     *    *: if not PIPE_CAP_TGSI_TEXCOORD can replace any GENERIC, by setting a bit in rasterizer->sprite_coord_enable.
     *       If PIPE_CAP_TGSI_TEXCOORD set, a specific semantic will be used.
     * any other generic can be allocated to any input slot
     */
    uint native_idx = 1;
    for(int idx=0; idx<cd->file_size[TGSI_FILE_INPUT]; ++idx)
    {
        struct etna_reg_desc *reg = &cd->file[TGSI_FILE_INPUT][idx];
        uint input_id;
        assert(reg->has_semantic);
        if(reg->semantic.Name == TGSI_SEMANTIC_POSITION)
        {
            input_id = 0; 
        } else {
            input_id = native_idx++;
            cd->varying_semantic[input_id-1] = reg->semantic;
            if(reg->interp.Interpolate == TGSI_INTERPOLATE_LINEAR) /* XXX just a guess */
                cd->varying_pa_attributes[input_id-1] = 0x2f1; 
            else
                cd->varying_pa_attributes[input_id-1] = 0x200;
            cd->varying_num_components[input_id-1] = 4; /* XXX set based on used components mask */
        }
        swap_native_registers(cd, (struct etna_native_reg) {
                .valid = 1,
                .rgroup = INST_RGROUP_TEMP,
                .id = input_id
            }, cd->file[TGSI_FILE_INPUT][idx].native); 
    }
    cd->num_varyings = native_idx-1;
}

void etna_compile(struct etna_compile_data *cd, const struct tgsi_token *tokens)
{
    /* Build a map from gallium register to native registers for files
     * CONST, SAMP, IMM, OUT, IN, TEMP. 
     * SAMP will map as-is for fragment shaders, there will be a +8 offset for vertex shaders.
     */
    /* Pass one -- check register file declarations and immediates */
    etna_compile_parse_declarations(cd, tokens);

    etna_assign_decls(cd);

    /* Pass two -- check usage of temporaries, inputs, outputs */
    etna_compile_pass_check_usage(cd, tokens);

    /* Assign native temp register to TEMPs */
    assign_temporaries_to_native(cd, cd->file[TGSI_FILE_TEMPORARY], cd->file_size[TGSI_FILE_TEMPORARY]);

    /* optimize outputs */
    etna_compile_pass_optimize_outputs(cd, tokens);

    /* XXX assign special inputs: gl_FrontFacing (VARYING_SLOT_FACE) */

    /* assign inputs: last usage of input should be <= first usage of temp */
    /*   potential optimization case:
     *     if single MOV TEMP[y], IN[x] before which temp y is not used, and after which IN[x]
     *     is not read, temp[y] can be used as input register as-is
     */
    /*   sort temporaries by first use
     *   sort inputs by last usage
     *   iterate over inputs, temporaries
     *     if last usage of input <= first usage of temp:
     *       assign input to temp
     *       advance input, temporary pointer
     *     else
     *       advance temporary pointer
     *
     *   potential problem: instruction with multiple inputs of which one is the temp and the other is the input;
     *      however, as the temp is not used before this, how would this make sense? uninitialized temporaries have an undefined
     *      value, so this would be ok
     */
    assign_inouts_to_temporaries(cd, TGSI_FILE_INPUT);

    /* assign outputs: first usage of output should be >= last usage of temp */
    /*   potential optimization case: 
     *      if single MOV OUT[x], TEMP[y] (with full write mask, or at least writing all components that are used in
     *        the shader) after which temp y is no longer used temp[y] can be used as output register as-is 
     *
     *   potential problem: instruction with multiple outputs of which one is the temp and the other is the output;
     *      however, as the temp is not used after this, how would this make sense? could just discard the output value
     */
    /*   sort temporaries by last use
     *   sort outputs by first usage
     *   iterate over outputs, temporaries
     *     if first usage of output >= last usage of temp:
     *       assign output to temp
     *       advance output, temporary pointer
     *     else
     *       advance temporary pointer
     */
    assign_inouts_to_temporaries(cd, TGSI_FILE_OUTPUT);
    
    assign_constants_and_immediates(cd);
    assign_texture_units(cd);
    
    /* XXX for PS we need to permute so that inputs are always in temporary 0..N-1.
     * There is no "switchboard" for varyings (AFAIK!). The output color, however, can be routed 
     * from an arbitrary temporary.
     */
    if(cd->processor == TGSI_PROCESSOR_FRAGMENT) 
    {
        permute_ps_inputs(cd);
    }

    /* list declarations */
    for(int x=0; x<cd->total_decls; ++x)
    {
        printf("%i: %s,%d first_use=%i last_use=%i native=%i usage_mask=%x has_semantic=%i semantic_name=%i semantic_idx=%i\n", x, tgsi_file_names[cd->decl[x].file], cd->decl[x].idx,
                cd->decl[x].first_use, cd->decl[x].last_use, cd->decl[x].native.valid?cd->decl[x].native.id:-1,
                cd->decl[x].usage_mask, 
                cd->decl[x].has_semantic,
                cd->decl[x].semantic.Name, cd->decl[x].semantic.Index);
    }

    /* pass 3: compile instructions
     */
    etna_compile_pass_generate_code(cd, tokens);

    etna_compile_fill_in_labels(cd);
}

extern const char *tgsi_swizzle_names[];

int main(int argc, char **argv)
{
    if(argc < 2)
    {
        fprintf(stderr, "Must pass shader source name as argument\n");
        exit(1);
    }
    const char *text = load_text_file(argv[1]);
    if(!text)
    {
        fprintf(stderr, "Unable to open %s\n", argv[1]);
        exit(1);
    }
    struct tgsi_token tokens[ETNA_MAX_TOKENS];
    if(tgsi_text_translate(text, tokens, Elements(tokens)))
    {
        /* tgsi_dump(tokens, 0); */
#if 0
        union {
            struct tgsi_header h;
            struct tgsi_token t;
        } hdr;

        hdr.t = tokens[0];
        printf("Header size: %i\n", hdr.h.HeaderSize);
        printf("Body size: %i\n", hdr.h.BodySize);
        int totalSize = hdr.h.HeaderSize + hdr.h.BodySize;

        for(int i=0; i<totalSize; ++i)
        {
            printf("%08x ", *((uint32_t*)&tokens[i]));
        }
        printf("\n");
#endif
        struct etna_compile_data cdata = {};
        cdata.vertex_tex_base = 8;
        etna_compile(&cdata, tokens);

        for(int x=0; x<cdata.inst_ptr; ++x)
        {
            printf("| %08x %08x %08x %08x\n", cdata.code[x*4+0], cdata.code[x*4+1], cdata.code[x*4+2], cdata.code[x*4+3]);
        }
        printf("immediates:\n");
        for(int idx=0; idx<cdata.imm_size; ++idx)
        {
            printf(" [%i].%s = %f (0x%08x)\n", (idx+cdata.imm_base)/4, tgsi_swizzle_names[idx%4], 
                    *((float*)&cdata.imm_data[idx]), cdata.imm_data[idx]);
        }
        printf("varyings:\n");
        for(int idx=0; idx<cdata.num_varyings; ++idx)
        {
            printf(" [%i] name=%i index=%i pa=%08x comps=%i\n", idx, cdata.varying_semantic[idx].Name, cdata.varying_semantic[idx].Index,
                    cdata.varying_pa_attributes[idx], cdata.varying_num_components[idx]);
        }

        int fd = creat("shader.bin", 0777);
        write(fd, cdata.code, cdata.inst_ptr*4*4);
        close(fd);

    } else {
        fprintf(stderr, "Unable to parse %s\n", argv[1]);
    }

    return 0;
}

