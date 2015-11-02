/*-
 * Copyright (c) 2015, Jason Lingle
 *
 * Permission to  use, copy,  modify, and/or distribute  this software  for any
 * purpose  with or  without fee  is hereby  granted, provided  that the  above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE  IS PROVIDED "AS  IS" AND  THE AUTHOR DISCLAIMS  ALL WARRANTIES
 * WITH  REGARD   TO  THIS  SOFTWARE   INCLUDING  ALL  IMPLIED   WARRANTIES  OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT  SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL,  DIRECT,   INDIRECT,  OR  CONSEQUENTIAL  DAMAGES   OR  ANY  DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF  CONTRACT, NEGLIGENCE  OR OTHER  TORTIOUS ACTION,  ARISING OUT  OF OR  IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/value.h"
#include "avalanche/list.h"
#include "avalanche/map.h"
#include "avalanche/pcode.h"
#include "avalanche/errors.h"
#include "avalanche/pcode-validation.h"

static void ava_xcode_unknown_location(
  ava_compile_location* dst);
static void ava_xcode_make_global_location(
  ava_compile_location* dst,
  const ava_pcg_src_pos* pos,
  ava_map_value sources);
static void ava_xcode_see_global(
  ava_compile_location* location,
  const ava_pcode_global* exe,
  ava_map_value sources);
static void ava_xcode_make_exe_location(
  ava_compile_location* dst,
  const ava_pcx_src_pos* pos,
  ava_map_value sources);
static void ava_xcode_see_exe(
  ava_compile_location* location,
  const ava_pcode_exe* exe,
  ava_map_value sources);
static void ava_xcode_globals_from_pcode(
  ava_xcode_global_list* dst,
  const ava_pcode_global_list* pcode,
  ava_compile_error_list* errors,
  ava_map_value sources);
static ava_bool ava_xcode_validate_global_xrefs(
  const ava_xcode_global_list* dst,
  ava_compile_error_list* errors,
  ava_map_value sources);
static ava_bool ava_xcode_validate_fun_global_xrefs(
  const ava_xcode_function* fun,
  const ava_xcode_global_list* xcode,
  ava_compile_error_list* errors,
  ava_map_value sources);

static ava_xcode_function* ava_xcode_structure_function(
  const ava_pcg_fun* pcode,
  ava_compile_error_list* errors,
  ava_map_value sources);

static ava_bool ava_xcode_check_block_break(
  const ava_pcode_exe* instr,
  ava_map_value* label_to_block_ix,
  ava_list_value* block_lengths,
  size_t* block_length,
  ava_bool* next_instr_starts_block,
  const ava_compile_location* location,
  ava_compile_error_list* errors);
static ava_bool ava_xcode_update_reg_height(
  const ava_pcode_exe* instr,
  size_t num_registers[ava_prt_function+1],
  size_t reg_height[ava_prt_function+1],
  const ava_compile_location* location,
  ava_compile_error_list* errors);
static ava_bool ava_xcode_check_registers_exist(
  const ava_pcode_exe* instr,
  const size_t reg_height[ava_prt_function+1],
  const ava_compile_location* location,
  ava_compile_error_list* errors);
static ava_xcode_function* ava_xcode_to_basic_blocks(
  const ava_pcg_fun* pcode,
  size_t num_registers[ava_prt_function+1],
  ava_list_value block_lengths);
static ava_bool ava_xcode_link_blocks(
  ava_xcode_function* fun,
  ava_map_value label_to_block_ix,
  ava_compile_error_list* errors,
  ava_map_value sources);
static void ava_xcode_rename_registers(
  ava_xcode_function* fun,
  const size_t* num_registers);
static void ava_xcode_init_phi(ava_xcode_function* fun, size_t num_args);
static void ava_xcode_propagate_phi(ava_xcode_function* fun);
static ava_bool ava_xcode_propagate_phi_hop(
  ava_xcode_function* fun, const ava_xcode_basic_block* from,
  ava_sint to_ix);
static void ava_xcode_check_phi(
  const ava_xcode_function* fun,
  ava_list_value vars,
  ava_compile_error_list* errors,
  ava_map_value sources);
static void ava_xcode_check_reg_init(
  const ava_ulong* init,
  ava_pcode_register reg,
  ava_list_value vars,
  const ava_compile_location* location,
  ava_compile_error_list* errors);

ava_xcode_global_list* ava_xcode_from_pcode(
  const ava_pcode_global_list* pcode,
  ava_compile_error_list* errors,
  ava_map_value sources
) {
  size_t num_globals;
  ava_xcode_global_list* ret;
  const ava_pcode_global* global;

  num_globals = 0;
  TAILQ_FOREACH(global, pcode, next)
    ++num_globals;

  ret = ava_alloc(sizeof(ava_xcode_global_list) +
                  num_globals * sizeof(ava_xcode_global));
  ret->length = num_globals;
  ava_xcode_globals_from_pcode(ret, pcode, errors, sources);
  if (!ava_xcode_validate_global_xrefs(ret, errors, sources))
    /* Return early so any later passes (if we add any) can assume global refs
     * make sense.
     */
    return ret;

  return ret;
}

static void ava_xcode_make_global_location(
  ava_compile_location* dst,
  const ava_pcg_src_pos* pos,
  ava_map_value sources
) {
  ava_map_cursor cursor;

  dst->line_offset = pos->line_offset;
  dst->start_line = pos->start_line;
  dst->end_line = pos->end_line;
  dst->start_column = pos->start_column;
  dst->end_column = pos->end_column;
  dst->filename = pos->filename;

  cursor = ava_map_find(sources, ava_value_of_string(pos->filename));
  if (AVA_MAP_CURSOR_NONE == cursor) {
    dst->source = AVA_ABSENT_STRING;
  } else {
    dst->source = ava_to_string(ava_map_get(sources, cursor));
  }
}

static void ava_xcode_see_global(
  ava_compile_location* location,
  const ava_pcode_global* exe,
  ava_map_value sources
) {
  if (ava_pcgt_src_pos == exe->type)
    ava_xcode_make_global_location(
      location, (const ava_pcg_src_pos*)exe, sources);
}

static void ava_xcode_make_exe_location(
  ava_compile_location* dst,
  const ava_pcx_src_pos* pos,
  ava_map_value sources
) {
  ava_map_cursor cursor;

  dst->line_offset = pos->line_offset;
  dst->start_line = pos->start_line;
  dst->end_line = pos->end_line;
  dst->start_column = pos->start_column;
  dst->end_column = pos->end_column;
  dst->filename = pos->filename;

  cursor = ava_map_find(sources, ava_value_of_string(pos->filename));
  if (AVA_MAP_CURSOR_NONE == cursor) {
    dst->source = AVA_ABSENT_STRING;
  } else {
    dst->source = ava_to_string(ava_map_get(sources, cursor));
  }
}

static void ava_xcode_see_exe(
  ava_compile_location* location,
  const ava_pcode_exe* exe,
  ava_map_value sources
) {
  if (ava_pcxt_src_pos == exe->type)
    ava_xcode_make_exe_location(
      location, (const ava_pcx_src_pos*)exe, sources);
}

static void ava_xcode_unknown_location(ava_compile_location* dst) {
  memset(dst, 0, sizeof(*dst));
  dst->filename = AVA_ASCII9_STRING("<unknown>");
  dst->source = AVA_ABSENT_STRING;
}

static void ava_xcode_globals_from_pcode(
  ava_xcode_global_list* dst,
  const ava_pcode_global_list* pcode,
  ava_compile_error_list* errors,
  ava_map_value sources
) {
  ava_compile_location location;
  size_t i;
  const ava_pcode_global* global;

  ava_xcode_unknown_location(&location);
  i = 0;
  TAILQ_FOREACH(global, pcode, next) {
    dst->elts[i].pc = global;
    switch (global->type) {
    case ava_pcgt_src_pos:
      ava_xcode_make_global_location(
        &location, (const ava_pcg_src_pos*)global, sources);
      break;

    case ava_pcgt_fun:
      dst->elts[i].fun = ava_xcode_structure_function(
        (const ava_pcg_fun*)global, errors, sources);
      break;

    default: break;
    }

    ++i;
  }
}

static ava_xcode_function* ava_xcode_structure_function(
  const ava_pcg_fun* pcode,
  ava_compile_error_list* errors,
  ava_map_value sources
) {
  ava_xcode_function* fun;
  ava_compile_location location;
  ava_map_value label_to_block_ix;
  ava_list_value block_lengths;
  size_t num_registers[ava_prt_function+1], reg_height[ava_prt_function+1];
  const ava_pcode_exe* instr;
  ava_bool next_instr_starts_block;
  size_t block_length, i;

#define DIE(error) do { ava_compile_error_add(errors, error); \
                        return NULL; } while (0)

  /* First pass, determine how many registers there are total, determine basic
   * block indices and their lengths, and check that only live registers are
   * accessed.
   *
   * Basic blocks not initiated by label instructions are given a "label" of
   * the empty string.
   */
  memset(num_registers, 0, sizeof(num_registers));
  memset(reg_height, 0, sizeof(reg_height));
  num_registers[ava_prt_var] = ava_list_length(pcode->vars);
  reg_height[ava_prt_var] = ava_list_length(pcode->vars);
  label_to_block_ix = ava_empty_map();
  block_lengths = ava_empty_list();
  next_instr_starts_block = ava_true;
  ava_xcode_unknown_location(&location);
  block_length = 0;
  TAILQ_FOREACH(instr, pcode->body, next) {
    ava_xcode_see_exe(&location, instr, sources);

    if (!ava_xcode_check_block_break(
          instr, &label_to_block_ix, &block_lengths, &block_length,
          &next_instr_starts_block, &location, errors))
      return NULL;

    ++block_length;

    if (!ava_xcode_update_reg_height(
          instr, num_registers, reg_height, &location, errors))
      return NULL;

    if (!ava_xcode_check_registers_exist(
          instr, reg_height, &location, errors))
      return NULL;

    next_instr_starts_block = ava_pcode_exe_is_terminal(instr);
  }

  for (i = ava_prt_data; i <= ava_prt_function; ++i) {
    if (reg_height[i] > 0) {
      DIE(ava_error_xcode_unbalanced_push(
            &location, ava_string_concat(
              ava_string_of_char("vdilpf"[i]),
              ava_string_concat(
                AVA_ASCII9_STRING(" "),
                ava_to_string(ava_value_of_integer(reg_height[i]))))));
    }
  }

  if (ava_map_npairs(label_to_block_ix) > 0)
    block_lengths = ava_list_append(
      block_lengths, ava_value_of_integer(block_length));

  assert(ava_list_length(block_lengths) == ava_map_npairs(label_to_block_ix));

  fun = ava_xcode_to_basic_blocks(pcode, num_registers, block_lengths);
  assert(fun);

  if (!ava_xcode_link_blocks(fun, label_to_block_ix, errors, sources))
    return NULL;

  ava_xcode_rename_registers(fun, num_registers);
  ava_xcode_init_phi(fun, pcode->prototype->num_args);
  ava_xcode_propagate_phi(fun);
  ava_xcode_check_phi(fun, pcode->vars, errors, sources);

  return fun;
#undef DIE
}

#define DIE(error) do { ava_compile_error_add(errors, error); \
                        return ava_false; } while (0)
static ava_bool ava_xcode_check_block_break(
  const ava_pcode_exe* instr,
  ava_map_value* label_to_block_ix,
  ava_list_value* block_lengths,
  size_t* block_length,
  ava_bool* next_instr_starts_block,
  const ava_compile_location* location,
  ava_compile_error_list* errors
) {
  ava_value label;
  ava_map_cursor cursor;

  if (ava_pcxt_label == instr->type) {
    label = ava_value_of_integer(
      ((const ava_pcx_label*)instr)->name);
    cursor = ava_map_find(*label_to_block_ix, label);
    if (AVA_MAP_CURSOR_NONE != cursor)
      DIE(ava_error_xcode_dupe_label(location, label));

    *next_instr_starts_block = ava_true;
  } else {
    label = ava_value_of_string(AVA_EMPTY_STRING);
  }

  if (*next_instr_starts_block) {
    if (ava_map_npairs(*label_to_block_ix) > 0)
      *block_lengths = ava_list_append(
        *block_lengths, ava_value_of_integer(*block_length));
    *label_to_block_ix = ava_map_add(
      *label_to_block_ix, label,
      ava_value_of_integer(ava_map_npairs(*label_to_block_ix)));
    *block_length = 0;
  }

  return ava_true;
}

static ava_bool ava_xcode_update_reg_height(
  const ava_pcode_exe* instr,
  size_t num_registers[ava_prt_function+1],
  size_t reg_height[ava_prt_function+1],
  const ava_compile_location* location,
  ava_compile_error_list* errors
) {
  if (ava_pcxt_push == instr->type) {
    const ava_pcx_push* push = (const ava_pcx_push*)instr;
    num_registers[push->register_type] += push->count;
    reg_height[push->register_type] += push->count;
  } else if (ava_pcxt_pop == instr->type) {
    const ava_pcx_pop* pop = (const ava_pcx_pop*)instr;
    if ((size_t)pop->count > reg_height[pop->register_type])
      DIE(ava_error_xcode_reg_underflow(location));

    reg_height[pop->register_type] -= pop->count;
  }

  return ava_true;
}

static ava_bool ava_xcode_check_registers_exist(
  const ava_pcode_exe* instr,
  const size_t reg_height[ava_prt_function+1],
  const ava_compile_location* location,
  ava_compile_error_list* errors
) {
  ava_integer sbase, scount;
  ava_ulong ubase, ucount;
  ava_uint i;
  ava_pcode_register reg;
  ava_pcode_register_type type;

  for (i = 0; ava_pcode_exe_get_reg_read(&reg, instr, i); ++i)
    if (reg.index >= reg_height[reg.type])
      DIE(ava_error_xcode_reg_nxaccess(location));

  for (i = 0; ava_pcode_exe_get_reg_write(&reg, instr, i); ++i)
    if (reg.index >= reg_height[reg.type])
      DIE(ava_error_xcode_reg_nxaccess(location));

  if (ava_pcode_exe_is_special_reg_read_d(instr) ||
      ava_pcode_exe_is_special_reg_read_p(instr)) {
    type = ava_pcode_exe_is_special_reg_read_d(instr)?
      ava_prt_data : ava_prt_parm;

    if (!ava_pcode_exe_get_reg_read_base(&sbase, instr, 0)) abort();
    if (!ava_pcode_exe_get_reg_read_count(&scount, instr, 0)) abort();
    ubase = sbase;
    ucount = scount;

    if (ucount + ubase < ubase ||
        ucount + ubase > reg_height[type])
      DIE(ava_error_xcode_reg_nxaccess(location));
  }

  return ava_true;
}

static ava_xcode_function* ava_xcode_to_basic_blocks(
  const ava_pcg_fun* pcode,
  size_t num_registers[ava_prt_function+1],
  ava_list_value block_lengths
) {
  ava_xcode_function* fun;
  ava_xcode_basic_block* block;
  size_t num_blocks, num_instrs, block_ix, instr_ix;
  size_t total_registers, phi_length;
  size_t i;
  ava_ulong* phi;
  const ava_pcode_exe* instr;

  num_blocks = ava_list_length(block_lengths);
  fun = ava_alloc(sizeof(ava_xcode_function) +
                  sizeof(ava_xcode_basic_block*) * num_blocks);
  fun->num_blocks = num_blocks;

  total_registers = 0;
  fun->reg_type_off[0] = 0;
  for (i = 0; i < ava_prt_function+1; ++i) {
    total_registers += num_registers[i];
    fun->reg_type_off[i+1] = fun->reg_type_off[i] + num_registers[i];
  }
  fun->phi_length = phi_length = (total_registers + 63) / 64;

  instr = TAILQ_FIRST(pcode->body);
  for (block_ix = 0; block_ix < num_blocks; ++block_ix) {
    num_instrs = ava_integer_of_value(
      ava_list_index(block_lengths, block_ix), 0);

    block = ava_alloc(sizeof(ava_xcode_basic_block) +
                      sizeof(ava_pcode_exe*) * num_instrs);
    block->length = num_instrs;
    phi = ava_alloc_atomic(sizeof(ava_ulong) * phi_length * 5);
    block->phi_iinit  = phi + phi_length * 0;
    block->phi_oinit  = phi + phi_length * 1;
    block->phi_effect = phi + phi_length * 2;
    block->phi_iexist = phi + phi_length * 3;
    block->phi_oexist = phi + phi_length * 4;

    for (instr_ix = 0; instr_ix < num_instrs; ++instr_ix) {
      block->elts[instr_ix] = instr;
      instr = TAILQ_NEXT(instr, next);
    }

    fun->blocks[block_ix] = block;
  }

  return fun;
}

static ava_bool ava_xcode_link_blocks(
  ava_xcode_function* fun,
  ava_map_value label_to_block_ix,
  ava_compile_error_list* errors,
  ava_map_value sources
) {
  ava_compile_location location;
  size_t i, j, relinked;
  ava_integer jump_target;
  ava_xcode_basic_block* block;
  const ava_pcode_exe* instr;
  ava_map_cursor cursor;

  ava_xcode_unknown_location(&location);

  for (i = 0; i < fun->num_blocks; ++i) {
    block = fun->blocks[i];
    assert(0 != block->length);

    /* Scan through all the instructions to maintain the location */
    for (j = 0; j < block->length; ++j)
      ava_xcode_see_exe(&location, block->elts[j], sources);

    instr = block->elts[block->length-1];
    if (!ava_pcode_exe_is_terminal(instr)) {
      /* Block simply falls through to next */
      block->next[0] = i + 1 < fun->num_blocks? (ava_sint)(i + 1) : -1;
      block->next[1] = -1;
    } else {
      if (ava_pcode_exe_get_jump_target(&jump_target, instr, 0)) {
        cursor = ava_map_find(
          label_to_block_ix, ava_value_of_integer(jump_target));
        if (AVA_MAP_CURSOR_NONE == cursor)
          DIE(ava_error_xcode_jump_nxlabel(
                &location, ava_value_of_integer(jump_target)));

        relinked = ava_integer_of_value(
          ava_map_get(label_to_block_ix, cursor), 0);
        block->elts[block->length-1] =
          ava_pcode_exe_with_jump_target(instr, 0, relinked);
        block->next[0] = relinked;
      } else {
        block->next[0] = -1;
      }

      if (ava_pcode_exe_is_terminal_no_fallthrough(instr) ||
          i+1 == fun->num_blocks) {
        block->next[1] = -1;
      } else {
        block->next[1] = i + 1;
      }
    }
  }

  return ava_true;
}

static void ava_xcode_rename_registers(
  ava_xcode_function* fun,
  const size_t* num_registers
) {
  ava_pcode_register_index next_name[ava_prt_function+1];
  ava_pcode_register_index reg_height[ava_prt_function+1];
  ava_pcode_register_index
    effective_v[num_registers[ava_prt_var]],
    effective_d[num_registers[ava_prt_data]],
    effective_i[num_registers[ava_prt_int]],
    effective_l[num_registers[ava_prt_list]],
    effective_p[num_registers[ava_prt_parm]],
    effective_f[num_registers[ava_prt_function]];
  ava_pcode_register_index*const effective_names[ava_prt_function+1] = {
    effective_v, effective_d, effective_i,
    effective_l, effective_p, effective_f
  };
  size_t i, block_ix, instr_ix;
  ava_integer base;
  ava_pcode_register reg;
  ava_xcode_basic_block* block;
  const ava_pcode_exe* instr;

  memcpy(next_name, fun->reg_type_off, sizeof(next_name));
  memset(reg_height, 0, sizeof(reg_height));
  reg_height[ava_prt_var] = num_registers[ava_prt_var];
  for (i = 0; i < num_registers[ava_prt_var]; ++i)
    effective_names[ava_prt_var][i] = i;

  for (block_ix = 0; block_ix < fun->num_blocks; ++block_ix) {
    block = fun->blocks[block_ix];

    memset(block->phi_iexist, 0, fun->phi_length * sizeof(ava_ulong));
    for (reg.type = ava_prt_var; reg.type <= ava_prt_function; ++reg.type) {
      for (reg.index = 0; reg.index < reg_height[reg.type]; ++reg.index) {
        ava_xcode_phi_set(
          block->phi_iexist, effective_names[reg.type][reg.index], ava_true);
      }
    }

    for (instr_ix = 0; instr_ix < block->length; ++instr_ix) {
      instr = block->elts[instr_ix];

      if (ava_pcxt_push == instr->type) {
        const ava_pcx_push* push = (const ava_pcx_push*)instr;
        for (i = 0; i < (size_t)push->count; ++i) {
          effective_names[push->register_type][
            reg_height[push->register_type]++] =
            next_name[push->register_type]++;
        }
      } else if (ava_pcxt_pop == instr->type) {
        const ava_pcx_pop* pop = (const ava_pcx_pop*)instr;
        reg_height[pop->register_type] -= pop->count;
      }

      for (i = 0; ava_pcode_exe_get_reg_read(&reg, instr, i); ++i) {
        reg.index = effective_names[reg.type][reg.index];
        instr = ava_pcode_exe_with_reg_read(instr, i, reg);
      }
      for (i = 0; ava_pcode_exe_get_reg_write(&reg, instr, i); ++i) {
        reg.index = effective_names[reg.type][reg.index];
        instr = ava_pcode_exe_with_reg_write(instr, i, reg);
      }
      if (ava_pcode_exe_is_special_reg_read_d(instr)) {
        if (!ava_pcode_exe_get_reg_read_base(&base, instr, 0)) abort();
        base = effective_names[ava_prt_data][base];
        instr = ava_pcode_exe_with_reg_read_base(instr, 0, base);
      }
      if (ava_pcode_exe_is_special_reg_read_p(instr)) {
        if (!ava_pcode_exe_get_reg_read_base(&base, instr, 0)) abort();
        base = effective_names[ava_prt_parm][base];
        instr = ava_pcode_exe_with_reg_read_base(instr, 0, base);
      }

      block->elts[instr_ix] = instr;
    }

    memset(block->phi_oexist, 0, fun->phi_length * sizeof(ava_ulong));
    for (reg.type = ava_prt_var; reg.type <= ava_prt_function; ++reg.type) {
      for (reg.index = 0; reg.index < reg_height[reg.type]; ++reg.index) {
        ava_xcode_phi_set(
          block->phi_oexist, effective_names[reg.type][reg.index], ava_true);
      }
    }
  }
}

static void ava_xcode_init_phi(ava_xcode_function* fun, size_t num_args) {
  size_t block_ix, instr_ix, i;
  ava_integer base, count;
  ava_pcode_register reg;
  ava_xcode_basic_block* block;
  const ava_pcode_exe* instr;

  for (block_ix = 0; block_ix < fun->num_blocks; ++block_ix) {
    block = fun->blocks[block_ix];
    if (0 == block_ix) {
      memset(block->phi_iinit, 0, sizeof(ava_ulong) * fun->phi_length);
      for (i = 0; i < num_args; ++i)
        ava_xcode_phi_set(block->phi_iinit, i, ava_true);
    } else {
      memcpy(block->phi_iinit, block->phi_iexist,
             sizeof(ava_ulong) * fun->phi_length);
    }
    memcpy(block->phi_oinit, block->phi_iinit,
           sizeof(ava_ulong) * fun->phi_length);
    memset(block->phi_effect, 0, sizeof(ava_ulong) * fun->phi_length);

    for (instr_ix = 0; instr_ix < block->length; ++instr_ix) {
      instr = block->elts[instr_ix];

      for (i = 0; ava_pcode_exe_get_reg_write(&reg, instr, i); ++i) {
        ava_xcode_phi_set(block->phi_effect, reg.index, ava_true);
        ava_xcode_phi_set(block->phi_oinit, reg.index, ava_true);
      }

      /* Range-based P-register reads destroy the registers */
      if (ava_pcode_exe_is_special_reg_read_p(instr)) {
        if (!ava_pcode_exe_get_reg_read_base(&base, instr, 0)) abort();
        if (!ava_pcode_exe_get_reg_read_count(&count, instr, 0)) abort();

        for (i = 0; i < (size_t)count; ++i) {
          ava_xcode_phi_set(block->phi_effect, base + i, ava_true);
          ava_xcode_phi_set(block->phi_oinit, base + i, ava_false);
        }
      }
    }

    /* All regitsers that don't exist on exit are destroyed */
    for (i = 0; i < fun->phi_length; ++i) {
      block->phi_effect[i] |= ~block->phi_oexist[i];
      block->phi_oinit[i] &= block->phi_oexist[i];
    }
  }
}

static void ava_xcode_propagate_phi(ava_xcode_function* fun) {
  size_t block_ix, i;
  ava_xcode_basic_block* block;
  ava_bool again;

  again = ava_true;
  while (again) {
    again = ava_false;

    for (block_ix = 0; block_ix < fun->num_blocks; ++block_ix) {
      block = fun->blocks[block_ix];

      /* Propagate input deinitialisations to output */
      for (i = 0; i < fun->phi_length; ++i)
        block->phi_oinit[i] &= block->phi_effect[i] | block->phi_iinit[i];

      /* Propagate oinit to subsequent blocks.
       * The outer loop needs to run again if this changes a block ordered
       * before this block, or if it changes this block itself.
       */
      for (i = 0; i < 2; ++i)
        again |= (ava_xcode_propagate_phi_hop(fun, block, block->next[i]) &&
                  i <= block_ix);
    }
  }
}

static void ava_xcode_check_phi(
  const ava_xcode_function* fun,
  ava_list_value vars,
  ava_compile_error_list* errors,
  ava_map_value sources
) {
  ava_compile_location location;
  const ava_xcode_basic_block* block;
  const ava_pcode_exe* instr;
  size_t block_ix, instr_ix, i;
  ava_integer base, count;
  ava_pcode_register reg;
  ava_ulong init[fun->phi_length];

  ava_xcode_unknown_location(&location);

  for (block_ix = 0; block_ix < fun->num_blocks; ++block_ix) {
    block = fun->blocks[block_ix];
    memcpy(init, block->phi_iinit, sizeof(ava_ulong) * fun->phi_length);

    for (instr_ix = 0; instr_ix < block->length; ++instr_ix) {
      instr = block->elts[instr_ix];
      ava_xcode_see_exe(&location, instr, sources);

      for (i = 0; ava_pcode_exe_get_reg_read(&reg, instr, i); ++i) {
        ava_xcode_check_reg_init(init, reg, vars, &location, errors);
      }

      if (ava_pcode_exe_is_special_reg_read_d(instr)) {
        if (!ava_pcode_exe_get_reg_read_base(&base, instr, 0)) abort();
        if (!ava_pcode_exe_get_reg_read_count(&count, instr, 0)) abort();

        reg.type = ava_prt_data;
        for (i = 0; i < (size_t)count; ++i) {
          reg.index = base + i;
          ava_xcode_check_reg_init(init, reg, vars, &location, errors);
        }
      }

      if (ava_pcode_exe_is_special_reg_read_p(instr)) {
        if (!ava_pcode_exe_get_reg_read_base(&base, instr, 0)) abort();
        if (!ava_pcode_exe_get_reg_read_count(&count, instr, 0)) abort();

        reg.type = ava_prt_parm;
        for (i = 0; i < (size_t)count; ++i) {
          reg.index = base + i;
          ava_xcode_check_reg_init(init, reg, vars, &location, errors);
          ava_xcode_phi_set(init, reg.index, ava_false);
        }
      }

      for (i = 0; ava_pcode_exe_get_reg_write(&reg, instr, i); ++i)
        ava_xcode_phi_set(init, reg.index, ava_true);
    }
  }
}

static void ava_xcode_check_reg_init(
  const ava_ulong* init,
  ava_pcode_register reg,
  ava_list_value vars,
  const ava_compile_location* location,
  ava_compile_error_list* errors
) {
  if (!ava_xcode_phi_get(init, reg.index)) {
    if (ava_prt_var == reg.type) {
      ava_compile_error_add(
        errors, ava_error_xcode_uninit_var(
          location,
          ava_to_string(ava_list_index(vars, reg.index))));
    } else {
      ava_compile_error_add(
        errors, ava_error_xcode_uninit_reg(
          location,
          ava_string_concat(
            ava_string_of_char("vdilpf"[reg.type]),
            ava_to_string(ava_value_of_integer(reg.index)))));
    }
  }
}

static ava_bool ava_xcode_propagate_phi_hop(
  ava_xcode_function* fun, const ava_xcode_basic_block* from,
  ava_sint to_ix
) {
  ava_xcode_basic_block* to;
  size_t i;
  ava_ulong old, new;
  ava_bool changed;

  if (to_ix < 0) return ava_false;

  to = fun->blocks[to_ix];
  changed = ava_false;
  for (i = 0; i < fun->phi_length; ++i) {
    old = to->phi_iinit[i];
    new = old & from->phi_oinit[i];
    if (new != old) {
      changed = ava_true;
      to->phi_iinit[i] = new;
    }
  }

  return changed;
}

static ava_bool ava_xcode_validate_global_xrefs(
  const ava_xcode_global_list* xcode,
  ava_compile_error_list* errors,
  ava_map_value sources
) {
  ava_compile_location location;
  size_t glob_ix, i;
  ava_integer ref;
  const ava_pcode_global* global, * target;

  ava_xcode_unknown_location(&location);
  for (glob_ix = 0; glob_ix < xcode->length; ++glob_ix) {
    global = xcode->elts[glob_ix].pc;
    ava_xcode_see_global(&location, global, sources);

    for (i = 0; ava_pcode_global_get_global_entity_ref(&ref, global, i); ++i) {
      if (ref < 0 || (size_t)ref >= xcode->length)
        DIE(ava_error_xcode_oob_global(&location, ref));

      target = xcode->elts[ref].pc;
      if (!ava_pcode_global_is_entity(target))
        DIE(ava_error_xcode_bad_xref(&location, ref));
    }

    for (i = 0; ava_pcode_global_get_global_fun_ref(&ref, global, i); ++i) {
      if (ref < 0 || (size_t)ref >= xcode->length)
        DIE(ava_error_xcode_oob_global(&location, ref));

      target = xcode->elts[ref].pc;
      if (!ava_pcode_global_is_fun(target))
        DIE(ava_error_xcode_bad_xref(&location, ref));
    }

    if (ava_pcode_global_is_needs_special_validation(global)) {
      switch (global->type) {
      case ava_pcgt_init: {
        const ava_pcg_init* init = (const ava_pcg_init*)global;
        const ava_pcg_fun* target_fun;

        target = xcode->elts[init->fun].pc;
        if (ava_pcgt_fun != target->type)
          DIE(ava_error_xcode_bad_xref(&location, init->fun));

        target_fun = (const ava_pcg_fun*)target;
        if (1 != target_fun->prototype->num_args)
          DIE(ava_error_xcode_bad_xref(&location, init->fun));
        if (ava_cc_ava != target_fun->prototype->calling_convention)
          DIE(ava_error_xcode_bad_xref(&location, init->fun));
      } break;

      default: abort();
      }
    }

    if (xcode->elts[glob_ix].fun) {
      if (!ava_xcode_validate_fun_global_xrefs(xcode->elts[glob_ix].fun,
                                               xcode, errors, sources))
        return ava_false;
    }
  }

  return ava_true;
}

static ava_bool ava_xcode_validate_fun_global_xrefs(
  const ava_xcode_function* fun,
  const ava_xcode_global_list* xcode,
  ava_compile_error_list* errors,
  ava_map_value sources
) {
  ava_compile_location location;
  size_t block_ix, instr_ix, i;
  const ava_xcode_basic_block* block;
  const ava_pcode_exe* instr;
  ava_integer ref, num_args;
  const ava_pcode_global* target;
  const ava_function* prototype;

  ava_xcode_unknown_location(&location);
  for (block_ix = 0; block_ix < fun->num_blocks; ++block_ix) {
    block = fun->blocks[block_ix];
    for (instr_ix = 0; instr_ix < block->length; ++instr_ix) {
      instr = block->elts[instr_ix];
      ava_xcode_see_exe(&location, instr, sources);

      for (i = 0; ava_pcode_exe_get_global_var_ref(&ref, instr, i); ++i) {
        if (ref < 0 || (size_t)ref >= xcode->length)
          DIE(ava_error_xcode_oob_global(&location, ref));

        target = xcode->elts[ref].pc;
        if (!ava_pcode_global_is_var(target))
          DIE(ava_error_xcode_bad_xref(&location, ref));
      }

      for (i = 0; ava_pcode_exe_get_global_fun_ref(&ref, instr, i); ++i) {
        if (ref < 0 || (size_t)ref >= xcode->length)
          DIE(ava_error_xcode_oob_global(&location, ref));

        target = xcode->elts[ref].pc;
        if (!ava_pcode_global_is_fun(target))
          DIE(ava_error_xcode_bad_xref(&location, ref));

        if (ava_pcode_exe_get_static_arg_count(&num_args, instr, 0)) {
          if (!ava_pcode_global_get_prototype(&prototype, target, 0)) abort();

          if ((size_t)num_args != prototype->num_args)
            DIE(ava_error_xcode_wrong_arg_count(
                  &location, prototype->num_args, num_args));
        }
      }
    }
  }

  return ava_true;
}
