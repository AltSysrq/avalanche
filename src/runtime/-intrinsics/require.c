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

#define AVA__INTERNAL_INCLUDE 1
#include "../avalanche/defs.h"
#include "../avalanche/alloc.h"
#include "../avalanche/string.h"
#include "../avalanche/value.h"
#include "../avalanche/list.h"
#include "../avalanche/function.h"
#include "../avalanche/macsub.h"
#include "../avalanche/symbol.h"
#include "../avalanche/symtab.h"
#include "../avalanche/macro-arg.h"
#include "../avalanche/pcode.h"
#include "../avalanche/code-gen.h"
#include "../avalanche/errors.h"
#include "../avalanche/module-cache.h"
#include "../avalanche/compenv.h"
#include "../avalanche/pcode-linker.h"
#include "user-macro.h"
#include "require.h"

typedef struct {
  ava_ast_node header;
  ava_list_value targets;
} ava_intr_req;

typedef struct {
  ava_ast_node header;

  const ava_pcode_global* target;
  ava_symbol* symbol;
  ava_bool defined;
} ava_intr_require_import;

static ava_string ava_intr_reqmod_to_string(const ava_intr_req* node);
static void ava_intr_reqmod_cg_discard(
  ava_intr_req* node, ava_codegen_context* context);

static ava_string ava_intr_reqpkg_to_string(const ava_intr_req* node);
static void ava_intr_reqpkg_cg_discard(
  ava_intr_req* node, ava_codegen_context* context);

static ava_string ava_intr_require_import_to_string(
  const ava_intr_require_import* node);
static void ava_intr_require_import_cg_define(
  ava_intr_require_import* node, ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_reqmod_vtable = {
  .name = "reqmod",
  .to_string = (ava_ast_node_to_string_f)ava_intr_reqmod_to_string,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_intr_reqmod_cg_discard,
};
static const ava_ast_node_vtable ava_intr_reqpkg_vtable = {
  .name = "reqpkg",
  .to_string = (ava_ast_node_to_string_f)ava_intr_reqpkg_to_string,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_intr_reqpkg_cg_discard,
};
static const ava_ast_node_vtable ava_intr_require_import_vtable = {
  .name = "req*-import",
  .to_string = (ava_ast_node_to_string_f)ava_intr_require_import_to_string,
  .cg_define = (ava_ast_node_cg_define_f)ava_intr_require_import_cg_define,
};

static ava_bool ava_intr_req_is_valid_name(
  ava_string name, ava_bool permit_slash);

static void ava_intr_req_do_import(
  ava_macsub_context* context, ava_string name,
  const ava_compile_location* location, ava_bool is_package);
static const ava_pcode_global_list* ava_intr_req_compile_module(
  ava_macsub_context* context, ava_string name,
  const ava_compile_location* location);

static const ava_symbol_other_type ava_intr_req_sentinel_type = {
  .name = "require sentinel",
};

ava_macro_subst_result ava_intr_reqpkg_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  const ava_parse_unit* name_unit;
  ava_string name;
  ava_list_value targets;
  ava_intr_req* this;

  targets = ava_empty_list();

  AVA_MACRO_ARG_PARSE {
    AVA_MACRO_ARG_FROM_RIGHT_BEGIN {
      AVA_MACRO_ARG_FOR_REST {
        AVA_MACRO_ARG_CURRENT_UNIT(name_unit, "package name");
        AVA_MACRO_ARG_BAREWORD(name, "package name");

        if (!ava_intr_req_is_valid_name(name, ava_false)) {
          ava_macsub_record_error(
            context, ava_error_illegal_package_name(
              &name_unit->location, name));
          ava_macsub_panic(context);
        } else {
          targets = ava_list_append(targets, ava_value_of_string(name));
          ava_intr_req_do_import(context, name, &name_unit->location, ava_true);
        }
      }
    }
  }

  this = AVA_NEW(ava_intr_req);
  this->header.v = &ava_intr_reqpkg_vtable;
  this->header.location = provoker->location;
  this->header.context = context;
  this->targets = targets;
  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = (ava_ast_node*)this },
  };
}

ava_macro_subst_result ava_intr_reqmod_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  const ava_parse_unit* name_unit;
  ava_string name;
  ava_list_value targets;
  ava_intr_req* this;

  targets = ava_empty_list();

  AVA_MACRO_ARG_PARSE {
    AVA_MACRO_ARG_FROM_RIGHT_BEGIN {
      AVA_MACRO_ARG_FOR_REST {
        AVA_MACRO_ARG_CURRENT_UNIT(name_unit, "module name");
        AVA_MACRO_ARG_BAREWORD(name, "module name");

        if (!ava_intr_req_is_valid_name(name, ava_true)) {
          ava_macsub_record_error(
            context, ava_error_illegal_module_name(
              &name_unit->location, name));
          ava_macsub_panic(context);
        } else {
          targets = ava_list_append(targets, ava_value_of_string(name));
          ava_intr_req_do_import(context, name, &name_unit->location, ava_false);
        }
      }
    }
  }

  this = AVA_NEW(ava_intr_req);
  this->header.v = &ava_intr_reqmod_vtable;
  this->header.location = provoker->location;
  this->header.context = context;
  this->targets = targets;
  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = (ava_ast_node*)this },
  };
}

static inline ava_bool is_word(char ch) {
  return
    (ch >= 'a' && ch <= 'z') ||
    (ch >= 'A' && ch <= 'Z') ||
    (ch >= '0' && ch <= '9') ||
    (ch == '_');
}

static inline ava_bool is_delim(char ch, ava_bool permit_slash) {
  return ch == '.' || ch == '-' ||
    (permit_slash && ch == '/');
}

static ava_bool ava_intr_req_is_valid_name(ava_string str, ava_bool permit_slash) {
  ava_str_tmpbuff tmp;
  const char* s;

  s = ava_string_to_cstring_buff(tmp, str);
  for (;;) {
    if (!is_word(*s)) return ava_false;
    while (is_word(*s)) ++s;

    if (!*s) break;

    if (!is_delim(*s, permit_slash)) return ava_false;
    ++s;
  }

  return ava_true;
}

static ava_string ava_intr_reqpkg_to_string(const ava_intr_req* node) {
  return ava_strcat(
    AVA_ASCII9_STRING("reqpkg "),
    ava_to_string(node->targets.v));
}

static void ava_intr_reqpkg_cg_discard(
  ava_intr_req* node, ava_codegen_context* context
) {
  size_t i, len;

  len = ava_list_length(node->targets);
  for (i = 0; i < len; ++i)
    AVA_PCGB(load_pkg, ava_to_string(ava_list_index(node->targets, i)));
}

static ava_string ava_intr_reqmod_to_string(const ava_intr_req* node) {
  return ava_strcat(
    AVA_ASCII9_STRING("reqmod "),
    ava_to_string(node->targets.v));
}

static void ava_intr_reqmod_cg_discard(
  ava_intr_req* node, ava_codegen_context* context
) {
  size_t i, len;

  len = ava_list_length(node->targets);
  for (i = 0; i < len; ++i)
    AVA_PCGB(load_mod, ava_to_string(ava_list_index(node->targets, i)));
}

static ava_string ava_intr_require_import_to_string(
  const ava_intr_require_import* node
) {
  AVA_STATIC_STRING(prefix, "<import of ");

  return ava_strcat(
    ava_strcat(prefix, node->symbol->full_name),
    AVA_ASCII9_STRING(">"));
}

static void ava_intr_require_import_cg_define(
  ava_intr_require_import* node, ava_codegen_context* context
) {
  if (node->defined) return;
  node->defined = ava_true;

  switch (node->target->type) {
  case ava_pcgt_ext_fun: {
    const ava_pcg_ext_fun* f = (const ava_pcg_ext_fun*)node->target;
    node->symbol->pcode_index = AVA_PCGB(ext_fun, f->name, f->prototype);
  } break;

  case ava_pcgt_ext_var: {
    const ava_pcg_ext_var* v = (const ava_pcg_ext_var*)node->target;
    node->symbol->pcode_index = AVA_PCGB(ext_var, v->name);
  } break;

  case ava_pcgt_decl_sxt: {
    const ava_pcg_decl_sxt* v = (const ava_pcg_decl_sxt*)node->target;
    node->symbol->pcode_index =
      AVA_PCGB(decl_sxt, ava_false, v->def);
  } break;

  default: abort();
  }
}

static void ava_intr_req_do_import(
  ava_macsub_context* context, ava_string name,
  const ava_compile_location* location, ava_bool is_package
) {
  ava_compenv* compenv;
  const ava_pcode_global_list* module;
  ava_string cache_error;

  compenv = ava_macsub_get_compenv(context);

  /* Try to get the package/module from the cache */
  module = ava_module_cache_get(
    is_package? &compenv->package_cache : &compenv->module_cache,
    name, &cache_error);
  if (ava_string_is_present(cache_error)) {
    ava_macsub_record_error(
      context, ava_error_failed_reading_cached_module(
        location, name, cache_error));
    ava_macsub_panic(context);
    return;
  }

  ava_macsub_insert_module(context, module, name, location, is_package);
}

void ava_macsub_insert_module(
  ava_macsub_context* context, const ava_pcode_global_list* module,
  ava_string name,
  const ava_compile_location* location, ava_bool is_package
) {
  AVA_STATIC_STRING(package_sentinel_prefix, "(ava_intr_reqpkg)");
  AVA_STATIC_STRING(module_sentinel_prefix, "(ava_intr_reqmod)");

  const ava_pcode_global* global, * target, ** global_array;
  size_t global_size, ix;
  ava_symbol* sentinel, * symbol;
  ava_intr_require_import* definer;
  ava_symtab* symtab;

  symtab = ava_macsub_get_symtab(context);

  /* Check whether this import has already happened.
   *
   * This is implemented by defining a sentinel symbol in the symtab which
   * can't conflict with anything else.
   */
  sentinel = AVA_NEW(ava_symbol);
  sentinel->type = ava_st_other;
  sentinel->level = 0;
  sentinel->visibility = ava_v_private;
  sentinel->full_name = ava_strcat(
    is_package? package_sentinel_prefix : module_sentinel_prefix,
    name);
  sentinel->v.other.type = &ava_intr_req_sentinel_type;
  if (ava_symtab_put(symtab, sentinel)) {
    ava_macsub_record_error(
      context, ava_error_package_or_module_rerequired(
        location, name));
    /* Probably won't lead to a bunch of errors later, don't panic */
    return;
  }

  if (!module) {
    if (is_package) {
      ava_macsub_record_error(
        context, ava_error_package_not_found(location, name));
      ava_macsub_panic(context);
      return;
    } else {
      module = ava_intr_req_compile_module(context, name, location);
      if (!module) {
        ava_macsub_record_error(
          context, ava_error_implicit_module_compile_failed(
            location, name));
        ava_macsub_panic(context);
        return;
      }
    }
  }

  /* Convert the module to an array so we can index it efficiently */
  global_size = 0;
  TAILQ_FOREACH(global, module, next)
    ++global_size;

  global_array = ava_alloc(sizeof(ava_pcode_global*) * global_size);
  ix = 0;
  TAILQ_FOREACH(global, module, next)
    global_array[ix++] = global;

  /* Define symbols for every export / macro */
  TAILQ_FOREACH(global, module, next) {
    symbol = NULL;

    switch (global->type) {
    case ava_pcgt_macro: {
      const ava_pcg_macro* m = (const ava_pcg_macro*)global;

      symbol = AVA_NEW(ava_symbol);
      symbol->type = m->type;
      symbol->level = 0;
      symbol->visibility = m->reexport? ava_v_public : ava_v_internal;
      symbol->full_name = m->name;
      symbol->v.macro.precedence = m->precedence;
      symbol->v.macro.macro_subst = ava_intr_user_macro_eval;
      symbol->v.macro.userdata = m->body;
    } break;

    case ava_pcgt_export: {
      const ava_pcg_export* x = (const ava_pcg_export*)global;

      assert(x->global >= 0 && (size_t)x->global < global_size);
      target = global_array[x->global];

      symbol = AVA_NEW(ava_symbol);
      switch (target->type) {
      case ava_pcgt_ext_fun: {
        const ava_pcg_ext_fun* f = (const ava_pcg_ext_fun*)target;

        symbol->type = ava_st_global_function;
        symbol->v.var.fun = *f->prototype;
        symbol->v.var.name = f->name;
      } break;

      case ava_pcgt_ext_var: {
        const ava_pcg_ext_var* v = (const ava_pcg_ext_var*)target;

        symbol->type = ava_st_global_variable;
        symbol->v.var.name = v->name;
      } break;

      case ava_pcgt_decl_sxt: {
        const ava_pcg_decl_sxt* s = (const ava_pcg_decl_sxt*)target;

        symbol->type = ava_st_struct;
        symbol->v.sxt.def = s->def;
      } break;

      default: abort();
      }
      symbol->level = 0;
      symbol->visibility = x->reexport? ava_v_public : ava_v_internal;
      symbol->full_name = x->effective_name;
      symbol->v.var.is_mutable = ava_false;

      definer = AVA_NEW(ava_intr_require_import);
      definer->header.v = &ava_intr_require_import_vtable;
      definer->header.location = *location;
      definer->header.context = context;
      definer->target = target;
      definer->symbol = symbol;
      definer->defined = ava_false;
      symbol->definer = (ava_ast_node*)definer;
    } break;

    default: break;
    }

    if (symbol) {
      if (ava_symtab_put(symtab, symbol)) {
        ava_macsub_record_error(
          context, ava_error_require_symbol_clash(
            location, symbol->full_name));
      }
    }
  }
}

static const ava_pcode_global_list* ava_intr_req_compile_module(
  ava_macsub_context* context, ava_string name,
  const ava_compile_location* location
) {
  ava_compenv* compenv;
  ava_pcode_global_list* full_pcode, * interface;

  compenv = ava_macsub_get_compenv(context);

  if (!ava_compenv_compile_file(
        &full_pcode, NULL, compenv,
        ava_strcat(name, AVA_ASCII9_STRING(".ava")),
        ava_macsub_get_errors(context), location))
    return NULL;

  interface = ava_pcode_to_interface(full_pcode);
  ava_module_cache_put(&compenv->module_cache, name, interface);
  return interface;
}
