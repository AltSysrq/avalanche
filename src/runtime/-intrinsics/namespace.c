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

#include <stdlib.h>

#define AVA__INTERNAL_INCLUDE 1
#include "../avalanche/defs.h"
#include "../avalanche/alloc.h"
#include "../avalanche/string.h"
#include "../avalanche/value.h"
#include "../avalanche/symbol-table.h"
#include "../avalanche/macsub.h"
#include "../avalanche/symbol.h"
#include "../avalanche/macro-arg.h"
#include "../avalanche/pcode.h"
#include "../avalanche/code-gen.h"
#include "../avalanche/errors.h"
#include "namespace.h"

typedef struct {
  ava_ast_node header;
  ava_string self_name;
  const ava_symbol* old_symbol;
  ava_symbol* new_symbol;
  ava_bool defined;
} ava_intr_alias;

static ava_string ava_intr_import_to_string(const ava_ast_node* node);
static void ava_intr_import_cg_discard(ava_ast_node* node, ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_import_vtable = {
  .name = "import",
  .to_string = ava_intr_import_to_string,
  .cg_discard = ava_intr_import_cg_discard,
};

static ava_string ava_intr_alias_to_string(const ava_intr_alias* node);
static void ava_intr_alias_cg_define(
  ava_intr_alias* node, ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_alias_vtable = {
  .name = "alias",
  .to_string = (ava_ast_node_to_string_f)ava_intr_alias_to_string,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_intr_alias_cg_define, /* sic */
  .cg_define = (ava_ast_node_cg_define_f)ava_intr_alias_cg_define,
};

ava_macro_subst_result ava_intr_namespace_subst(
  const ava_symbol* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  ava_string name;
  const ava_parse_unit* explicit_body = NULL;
  ava_macsub_context* child_context;
  ava_parse_statement* body;
  ava_ast_node* result;

  AVA_MACRO_ARG_PARSE {
    AVA_MACRO_ARG_FROM_RIGHT_BEGIN {
      AVA_MACRO_ARG_BAREWORD(name, "namespace name");
      if (AVA_MACRO_ARG_HAS_ARG()) {
        AVA_MACRO_ARG_BLOCK(explicit_body, "body");
      }
    }
  }

  name = ava_string_concat(name, AVA_ASCII9_STRING("."));
  child_context = ava_macsub_context_push_minor(context, name);
  switch (ava_symbol_table_import(
            ava_macsub_get_current_symbol_table(child_context),
            ava_macsub_apply_prefix(context, name),
            AVA_EMPTY_STRING,
            ava_true, ava_true)) {
  case ava_stis_ok:
  case ava_stis_no_symbols_imported: break;
  case ava_stis_redefined_strong_local:
    /* TODO: Rethink the symbol table, unclear whether this should ever happen
     * or what should be done in that case.
     */
    abort();
  }

  if (explicit_body) {
    body = TAILQ_FIRST(&explicit_body->v.statements);
  } else {
    body = TAILQ_NEXT(statement, next);
    *consumed_other_statements = ava_true;
  }

  result = ava_macsub_run_from(child_context, &provoker->location, body,
                               ava_isrp_void);
  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = result },
  };
}

ava_macro_subst_result ava_intr_import_subst(
  const ava_symbol* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  char tmp[AVA_STR_TMPSZ];
  const ava_parse_unit* source_unit;
  ava_string source, dest = AVA_EMPTY_STRING;
  size_t source_len, dest_len, i;
  const char* source_str;
  ava_ast_node* result;

  AVA_MACRO_ARG_PARSE {
    AVA_MACRO_ARG_FROM_RIGHT_BEGIN {
      AVA_MACRO_ARG_CURRENT_UNIT(source_unit, "source");
      AVA_MACRO_ARG_BAREWORD(source, "source");
      if (AVA_MACRO_ARG_HAS_ARG()) {
        AVA_MACRO_ARG_BAREWORD(dest, "dest");
      }
    }
  }

  source_len = ava_string_length(source);
  if (source_len && '.' != ava_string_index(source, source_len-1) &&
      ':' != ava_string_index(source, source_len-1)) {
    source = ava_string_concat(source, AVA_ASCII9_STRING("."));
    ++source_len;
  }

  dest_len = ava_string_length(dest);
  if (dest_len) {
    if (!ava_strcmp(AVA_ASCII9_STRING("*"), dest)) {
      dest = AVA_EMPTY_STRING;
      dest_len = 0;
    } else if (dest_len && '.' != ava_string_index(dest, dest_len-1) &&
        ':' != ava_string_index(dest, dest_len-1)) {
      dest = ava_string_concat(dest, AVA_ASCII9_STRING("."));
      ++dest_len;
    }
  } else {
    source_str = ava_string_to_cstring_buff(tmp, source);
    for (i = source_len - 2 /* exclude trailing . or : */; i < source_len; --i)
      if (':' == source_str[i] || '.' == source_str[i])
        break;

    if (i >= source_len)
      return ava_macsub_error_result(
        context, ava_error_import_explicit_dest_required(
          &source_unit->location, source));

    dest = ava_string_of_cstring(source_str + i + 1);
  }

  switch (ava_symbol_table_import(ava_macsub_get_current_symbol_table(context),
                                  source, dest, ava_false, ava_false)) {
  case ava_stis_ok: break;
  case ava_stis_no_symbols_imported:
    return ava_macsub_error_result(
      context, ava_error_import_imported_nothing(
        &source_unit->location, source));

  case ava_stis_redefined_strong_local:
    /* Not strong, should be impossible */
    abort();
  }

  result = AVA_NEW(ava_ast_node);
  result->v = &ava_intr_import_vtable;
  result->context = context;
  result->location = provoker->location;
  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = result },
  };
}

static ava_string ava_intr_import_to_string(const ava_ast_node* node) {
  return AVA_ASCII9_STRING("<import>");
}

static void ava_intr_import_cg_discard(
  ava_ast_node* node, ava_codegen_context* context
) { }

ava_macro_subst_result ava_intr_alias_subst(
  const ava_symbol* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  const ava_parse_unit* source_unit, * dest_unit, * equals_unit;
  ava_string source_name, dest_name, equals;
  ava_symbol_table_get_result existing;
  const ava_symbol* old_symbol;
  ava_symbol* new_symbol;
  ava_intr_alias* this;

  AVA_MACRO_ARG_PARSE {
    AVA_MACRO_ARG_FROM_RIGHT_BEGIN {
      AVA_MACRO_ARG_CURRENT_UNIT(dest_unit, "new name");
      AVA_MACRO_ARG_BAREWORD(dest_name, "new name");
      AVA_MACRO_ARG_CURRENT_UNIT(equals_unit, "=");
      AVA_MACRO_ARG_BAREWORD(equals, "=");
      if (ava_strcmp(AVA_ASCII9_STRING("="), equals)) {
        return ava_macsub_error_result(
          context, ava_error_bad_macro_keyword(
            &equals_unit->location,
            self->full_name, equals, AVA_ASCII9_STRING("\"=\"")));
      }
      AVA_MACRO_ARG_CURRENT_UNIT(source_unit, "old name");
      AVA_MACRO_ARG_BAREWORD(source_name, "old name");
    }
  }

  existing = ava_symbol_table_get(
    ava_macsub_get_current_symbol_table(context), source_name);
  switch (existing.status) {
  case ava_stgs_ok: break;
  case ava_stgs_ambiguous_weak:
    return ava_macsub_error_result(
      context, ava_error_ambiguous_alias(
        &source_unit->location, source_name));

  case ava_stgs_not_found:
    return ava_macsub_error_result(
      context, ava_error_alias_target_not_found(
        &source_unit->location, source_name));
  }

  old_symbol = existing.symbol;
  new_symbol = AVA_CLONE(*old_symbol);
  new_symbol->visibility = *(const ava_visibility*)self->v.macro.userdata;
  new_symbol->full_name = ava_macsub_apply_prefix(context, dest_name);

  if (new_symbol->visibility > old_symbol->visibility) {
    ava_macsub_record_error(
      context, ava_error_alias_more_visible_than_target(
        &provoker->location, old_symbol->full_name, new_symbol->full_name));
    /* Continuing is less likely to introduce more errors */
  }

  this = AVA_NEW(ava_intr_alias);
  this->header.v = &ava_intr_alias_vtable;
  this->header.location = provoker->location;
  this->header.context = context;
  this->old_symbol = old_symbol;
  this->new_symbol = new_symbol;
  this->self_name = self->full_name;
  new_symbol->definer = (ava_ast_node*)this;

  ava_macsub_put_symbol(context, new_symbol, &dest_unit->location);

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = (ava_ast_node*)this },
  };
}

static ava_string ava_intr_alias_to_string(const ava_intr_alias* alias) {
  ava_string accum;

  accum = alias->self_name;
  accum = ava_string_concat(accum, AVA_ASCII9_STRING(" "));
  accum = ava_string_concat(accum, alias->new_symbol->full_name);
  accum = ava_string_concat(accum, AVA_ASCII9_STRING(" = "));
  accum = ava_string_concat(accum, alias->old_symbol->full_name);
  return accum;
}

static void ava_intr_alias_cg_define(
  ava_intr_alias* alias, ava_codegen_context* context
) {
  if (alias->defined) return;

  if (alias->old_symbol->definer) {
    ava_ast_node_cg_define(alias->old_symbol->definer, context);
    alias->new_symbol->pcode_index = alias->old_symbol->pcode_index;
    ava_codegen_export(context, alias->new_symbol);
  }

  alias->defined = ava_true;
}
