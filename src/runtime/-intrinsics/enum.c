/*-
 * Copyright (c) 2016, Jason Lingle
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

#define AVA__INTERNAL_INCLUDE 1
#include "../avalanche/defs.h"
#include "../avalanche/alloc.h"
#include "../avalanche/string.h"
#include "../avalanche/value.h"
#include "../avalanche/integer.h"
#include "../avalanche/parser.h"
#include "../avalanche/macsub.h"
#include "../avalanche/symbol.h"
#include "../avalanche/symtab.h"
#include "../avalanche/macro-arg.h"
#include "../avalanche/errors.h"
#include "fundamental.h"
#include "user-macro.h"
#include "enum.h"

static ava_macro_subst_result ava_intr_enum_subst(
  const ava_symbol* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool is_bitenum);

AVA_DEF_MACRO_SUBST(ava_intr_seqenum_subst) {
  return ava_intr_enum_subst(self, context, statement, provoker, ava_false);
}

AVA_DEF_MACRO_SUBST(ava_intr_bitenum_subst) {
  return ava_intr_enum_subst(self, context, statement, provoker, ava_true);
}

static ava_macro_subst_result ava_intr_enum_subst(
  const ava_symbol* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool is_bitenum
) {
  ava_macsub_context* subcontext;
  const ava_parse_unit * body_unit;
  const ava_parse_unit* elt_name_unit, * equals_unit, * val_unit;
  const ava_ast_node* value_override;
  ava_intr_seq* accum;
  ava_parse_statement* stmt;
  ava_string ns_name = AVA_ABSENT_STRING;
  ava_integer value;
  ava_value new_value;
  ava_pcm_builder* builder;

  AVA_MACRO_ARG_PARSE {
    AVA_MACRO_ARG_FROM_RIGHT_END {
      AVA_MACRO_ARG_BLOCK(body_unit, "body");
      if (AVA_MACRO_ARG_HAS_ARG()) {
        AVA_MACRO_ARG_BAREWORD(ns_name, "namespace name");
      }
    }
  }

  if (ava_string_is_present(ns_name)) {
    ava_string absolutised, ambiguous;

    ns_name = ava_strcat(ns_name, AVA_ASCII9_STRING("."));
    subcontext = ava_macsub_context_push_minor(context, ns_name);
    ava_macsub_import(&absolutised, &ambiguous, subcontext,
                      ava_macsub_apply_prefix(context, ns_name),
                      AVA_EMPTY_STRING,
                      ava_true, ava_true);
  } else {
    subcontext = context;
  }

  accum = ava_intr_seq_new(subcontext, &body_unit->location, ava_isrp_void);
  value = is_bitenum;

  TAILQ_FOREACH(stmt, &body_unit->v.statements, next) {
    ava_macsub_expand_expanders(subcontext, &stmt->units);
    if (TAILQ_EMPTY(&stmt->units)) continue;

    elt_name_unit = TAILQ_FIRST(&stmt->units);
    if (ava_put_bareword != elt_name_unit->type)
      return ava_macsub_error_result(
        context, ava_error_macro_arg_must_be_bareword(
          &elt_name_unit->location, AVA_ASCII9_STRING("name")));

    equals_unit = TAILQ_NEXT(elt_name_unit, next);
    if (equals_unit) {
      if (ava_put_bareword != equals_unit->type)
        return ava_macsub_error_result(
          context, ava_error_macro_arg_must_be_bareword(
            &equals_unit->location, AVA_ASCII9_STRING("\"=\"")));

      if (!ava_string_equal(AVA_ASCII9_STRING("="), equals_unit->v.string))
        return ava_macsub_error_result(
          context, ava_error_bad_macro_keyword(
            &equals_unit->location,
            self->full_name, equals_unit->v.string, AVA_ASCII9_STRING("=")));

      val_unit = TAILQ_NEXT(equals_unit, next);
      if (!val_unit)
        return ava_macsub_error_result(
          context, ava_error_macro_arg_missing(
            &equals_unit->location,
            self->full_name, AVA_ASCII9_STRING("value")));

      value_override = ava_macsub_run_units(
        subcontext, val_unit,
        TAILQ_LAST(&stmt->units, ava_parse_unit_list_s));

      if (!ava_ast_node_get_constexpr(value_override, &new_value)) {
        ava_macsub_record_error(
          context, ava_error_macro_arg_not_constexpr(
            &val_unit->location, AVA_ASCII9_STRING("value")));
      } else if (!ava_integer_try_parse(
                   &value, ava_to_string(new_value), 0)) {
        ava_macsub_record_error(
          context, ava_error_macro_arg_not_an_integer(
            &value_override->location, AVA_ASCII9_STRING("value")));
      }
    }

    builder = ava_pcm_builder_new(NULL);
    ava_pcmb_verbatim(builder, ava_to_string(ava_value_of_integer(value)));
    ava_pcmb_append(builder);
    ava_intr_seq_add(
      accum,  ava_intr_user_macro_put(
        subcontext, ava_st_expander_macro,
        *(const ava_visibility*)self->v.macro.userdata,
        elt_name_unit->v.string, 0,
        ava_pcm_builder_get(builder),
        &elt_name_unit->location, &elt_name_unit->location));

    if (is_bitenum)
      value <<= 1;
    else
      value += 1;
  }

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v.node = ava_intr_seq_to_node(accum),
  };
}
