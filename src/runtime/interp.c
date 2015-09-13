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

/* GNU requires _GNU_SOURCE for RTLD_DEFAULT */
#define _GNU_SOURCE 1

#include <stdlib.h>
#include <string.h>

/* TODO: Not that portable */
#include <dlfcn.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/value.h"
#include "avalanche/list.h"
#include "avalanche/function.h"
#include "avalanche/name-mangle.h"
#include "avalanche/pcode.h"
#include "avalanche/interp.h"

/*
  The current interpreter is a "simplest thing that works" implementation, not
  intended to be fast or completely correct, so that we can get simple programs
  running and start writing tests in Avalanche sooner.

  It simply interprets P-Code directly. This adds a lot of overhead, and means
  that global accesses, gotos, and so forth, have linear runtime instead of
  constant.

  The register stacks are not implemented. Each function invocation gets 256 of
  each register; if this overflows, the result is unpredictable.

  load-pkg and load-mod do nothing.

  External globals are resolved using dlsym()/dlfunc() upon access instead of
  during initialisation. Publish is not supported. When the interpreter returns
  from ava_interp_exec(), all globals are forgotten. Only the first 256 global
  P-Code statements may declare global variables; if any others do, the result
  is undefined.

  There's generally no error checking. Anything unexpected behaves erratically
  or crashes the process.
 */

static ava_value ava_interp_run_function(
  const ava_pcode_global_list* pcode,
  const ava_pcode_exe_list* function,
  ava_value* args,
  size_t nargs,
  ava_value* global_vars);
static const ava_pcode_global* ava_interp_get_global(
  const ava_pcode_global_list* pcode,
  ava_uint index);
static void ava_interp_get_global_function(
  ava_function* dst,
  const ava_pcode_global* fun);
static ava_value* ava_interp_get_global_var_ptr(
  const ava_pcode_global* var,
  ava_value* global_vars,
  ava_uint index);

void ava_interp_exec(const ava_pcode_global_list* pcode) {
  const ava_pcode_global* statement;
  ava_value global_vars[256];
  ava_value empty;
  unsigned i;

  for (i = 0; i < 256; ++i)
    global_vars[i] = ava_value_of_string(AVA_EMPTY_STRING);

  i = 0;
  TAILQ_FOREACH(statement, pcode, next) {
    switch (statement->type) {
    case ava_pcgt_init: {
      const ava_pcg_init* elt = (const ava_pcg_init*)statement;
      const ava_pcg_fun* fun = (const ava_pcg_fun*)
        ava_interp_get_global(pcode, elt->fun);

      empty = ava_value_of_string(AVA_EMPTY_STRING);
      ava_interp_run_function(
        pcode, fun->body, &empty, 1, global_vars);
    } break;

    default: break;
    }
  }
}

static ava_value ava_interp_run_function(
  const ava_pcode_global_list* pcode,
  const ava_pcode_exe_list* body,
  ava_value* args,
  size_t nargs,
  ava_value* global_vars
) {
  ava_value vars[256], data[256];
  ava_integer ints[256];
  const ava_function* funs[256];
  ava_list_value lists[256];
  ava_function_parameter parms[256] AVA_UNUSED;
  const ava_pcode_global* global;
  const ava_pcode_exe* instr;

  memcpy(vars, args, nargs * sizeof(ava_value));

  TAILQ_FOREACH(instr, body, next) {
    switch (instr->type) {
    case ava_pcxt_src_file: break;
    case ava_pcxt_src_line: break;
    case ava_pcxt_push: break;
    case ava_pcxt_pop: break;

    case ava_pcxt_ld_imm_vd: {
      const ava_pcx_ld_imm_vd* ld = (const ava_pcx_ld_imm_vd*)instr;
      switch (ld->dst.type) {
      case ava_prt_var:
        vars[ld->dst.index] = ava_value_of_string(ld->src);
        break;
      case ava_prt_data:
        data[ld->dst.index] = ava_value_of_string(ld->src);
        break;
      default: abort();
      }
    } break;

    case ava_pcxt_ld_imm_i: {
      const ava_pcx_ld_imm_i* ld = (const ava_pcx_ld_imm_i*)instr;
      ints[ld->dst.index] = ld->src;
    } break;

    case ava_pcxt_ld_glob: {
      const ava_pcx_ld_glob* ld = (const ava_pcx_ld_glob*)instr;
      ava_value src;

      global = ava_interp_get_global(pcode, ld->src);
      switch (global->type) {
      case ava_pcgt_ext_var:
      case ava_pcgt_var:
        src = *ava_interp_get_global_var_ptr(global, global_vars, ld->src);
        break;

      case ava_pcgt_ext_fun:
      case ava_pcgt_fun: {
        ava_function tmp;
        ava_interp_get_global_function(&tmp, global);
        src = ava_value_of_function(AVA_CLONE(tmp));
      } break;

      default: abort();
      }

      switch (ld->dst.type) {
      case ava_prt_var:
        vars[ld->dst.index] = src;
        break;
      case ava_prt_data:
        data[ld->dst.index] = src;
        break;
      default: abort();
      }
    } break;

    case ava_pcxt_ld_reg: {
      const ava_pcx_ld_reg* ld = (const ava_pcx_ld_reg*)instr;
      switch (ld->dst.type) {
      case ava_prt_var: switch (ld->src.type) {
        case ava_prt_var: vars[ld->dst.index] = vars[ld->src.index]; break;
        case ava_prt_data: vars[ld->dst.index] = data[ld->src.index]; break;
        case ava_prt_int:
          vars[ld->dst.index] = ava_value_of_integer(ints[ld->src.index]);
          break;
        case ava_prt_function:
          vars[ld->dst.index] = ava_value_of_function(funs[ld->src.index]);
          break;
        case ava_prt_list:
          vars[ld->dst.index] = lists[ld->src.index].v;
          break;
        default: abort();
        } break;
      case ava_prt_data: switch (ld->src.type) {
        case ava_prt_var: data[ld->dst.index] = vars[ld->src.index]; break;
        case ava_prt_data: data[ld->dst.index] = data[ld->src.index]; break;
        case ava_prt_int:
          data[ld->dst.index] = ava_value_of_integer(ints[ld->src.index]);
          break;
        case ava_prt_function:
          data[ld->dst.index] = ava_value_of_function(funs[ld->src.index]);
          break;
        case ava_prt_list:
          data[ld->dst.index] = lists[ld->src.index].v;
          break;
        default: abort();
        } break;
      case ava_prt_int: switch (ld->src.type) {
        case ava_prt_var:
          ints[ld->dst.index] = ava_integer_of_value(
            vars[ld->src.index], 0);
          break;
        case ava_prt_data:
          ints[ld->dst.index] = ava_integer_of_value(
            data[ld->src.index], 0);
          break;
        case ava_prt_int: ints[ld->dst.index] = ints[ld->src.index]; break;
        default: abort();
        } break;
      case ava_prt_function: switch (ld->src.type) {
        case ava_prt_var:
          funs[ld->dst.index] = ava_function_of_value(
            vars[ld->src.index]);
          break;
        case ava_prt_data:
          funs[ld->dst.index] = ava_function_of_value(
            data[ld->src.index]);
          break;
        case ava_prt_function:
          funs[ld->dst.index] = funs[ld->src.index];
          break;
        default: abort();
        } break;
      case ava_prt_list: switch (ld->src.type) {
        case ava_prt_var:
          lists[ld->dst.index] = ava_list_value_of(vars[ld->src.index]);
          break;
        case ava_prt_data:
          lists[ld->dst.index] = ava_list_value_of(data[ld->src.index]);
          break;
        case ava_prt_function: funs[ld->dst.index] =
            funs[ld->src.index];
          break;
        default: abort();
        } break;
      default: abort();
      }
    } break;

    case ava_pcxt_ld_parm: {
      const ava_pcx_ld_parm* ld = (const ava_pcx_ld_parm*)instr;
      ava_value src;

      switch (ld->src.type) {
      case ava_prt_var:  src = vars[ld->src.index]; break;
      case ava_prt_data: src = data[ld->src.index]; break;
      default: abort();
      }

      parms[ld->dst.index].value = src;
      parms[ld->dst.index].type = ld->spread?
        ava_fpt_spread : ava_fpt_static;
    } break;

    case ava_pcxt_set_glob: {
      const ava_pcx_set_glob* set = (const ava_pcx_set_glob*)instr;
      ava_value src;
      switch (set->src.type) {
      case ava_prt_var:  src = vars[set->src.index]; break;
      case ava_prt_data: src = data[set->src.index]; break;
      default: abort();
      }
      *ava_interp_get_global_var_ptr(
        ava_interp_get_global(pcode, set->dst),
        global_vars, set->dst) = src;
    } break;

    case ava_pcxt_invoke_ss: {
      const ava_pcx_invoke_ss* inv = (const ava_pcx_invoke_ss*)instr;
      ava_function fun;
      ava_value ret;
      ava_interp_get_global_function(
        &fun, ava_interp_get_global(pcode, inv->fun));
      ret = ava_function_invoke(&fun, data + inv->base);
      switch (inv->dst.type) {
      case ava_prt_var:  vars[inv->dst.index] = ret; break;
      case ava_prt_data: data[inv->dst.index] = ret; break;
      default: abort();
      }
    } break;

    case ava_pcxt_invoke_sd:
    case ava_pcxt_invoke_dd:
      /* TODO */
      abort();

    case ava_pcxt_ret: {
      const ava_pcx_ret* ret = (const ava_pcx_ret*)instr;
      switch (ret->return_value.type) {
      case ava_prt_var:  return vars[ret->return_value.index];
      case ava_prt_data: return data[ret->return_value.index];
      default: abort();
      }
    } break;

    case ava_pcxt_goto: {
      const ava_pcx_goto* go = (const ava_pcx_goto*)instr;
      TAILQ_FOREACH(instr, body, next) {
        if (ava_pcxt_label == instr->type &&
            !ava_strcmp(go->target, ((const ava_pcx_label*)instr)->name))
          break;
      }
    } break;

    case ava_pcxt_goto_c: {
      const ava_pcx_goto_c* go = (const ava_pcx_goto_c*)instr;

      if (!ints[go->condition.index])
        break;

      TAILQ_FOREACH(instr, body, next) {
        if (ava_pcxt_label == instr->type &&
            !ava_strcmp(go->target, ((const ava_pcx_label*)instr)->name))
          break;
      }
    } break;

    case ava_pcxt_label: break;
    }
  }

  return ava_value_of_string(AVA_EMPTY_STRING);
}

static const ava_pcode_global* ava_interp_get_global(
  const ava_pcode_global_list* pcode,
  ava_uint target
) {
  const ava_pcode_global* global;
  ava_uint index = 0;

  TAILQ_FOREACH(global, pcode, next)
    if (target == index++)
      return global;

  abort();
}

static ava_value* ava_interp_get_global_var_ptr(
  const ava_pcode_global* global,
  ava_value* global_vars,
  ava_uint index
) {
  switch (global->type) {
  case ava_pcgt_ext_var:
    return dlsym(RTLD_DEFAULT, ava_string_to_cstring(
                   ava_name_mangle(
                     ((const ava_pcg_ext_var*)global)->name)));
  case ava_pcgt_var:
    return global_vars + index;

  default: abort();
  }
}

static void ava_interp_get_global_function(
  ava_function* dst,
  const ava_pcode_global* global
) {
  switch (global->type) {
  case ava_pcgt_ext_fun: {
    const ava_pcg_ext_fun* f = (const ava_pcg_ext_fun*)global;
    *dst = *f->prototype;
#ifdef HAVE_DLFUNC
    dst->address = dlfunc(RTLD_DEFAULT, ava_string_to_cstring(
                            ava_name_mangle(f->name)));
#else
    dst->address = (void(*)())dlsym(RTLD_DEFAULT, ava_string_to_cstring(
                                        ava_name_mangle(f->name)));
#endif
  } break;
  case ava_pcgt_fun: {
    /* TODO */
    abort();
  } break;
  default: abort();
  }
}
