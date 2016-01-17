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

#define AVA__INTERNAL_INCLUDE 1
#include "../avalanche/defs.h"
#include "../avalanche/alloc.h"
#include "../avalanche/string.h"
#include "../avalanche/value.h"
#include "../avalanche/map.h"
#include "../avalanche/struct.h"
#include "../avalanche/pcode.h"
#include "../avalanche/macsub.h"
#include "../avalanche/code-gen.h"
#include "../avalanche/symbol.h"
#include "../avalanche/symtab.h"
#include "../avalanche/macro-arg.h"
#include "../avalanche/pointer.h"
#include "../avalanche/errors.h"
#include "../avalanche/exception.h"
#include "funmac.h"
#include "structdef.h"

typedef struct {
  ava_ast_node header;

  const ava_struct* def;
  ava_symbol* struct_sym;
  ava_bool is_definition;
  ava_bool defined;
} ava_intr_structdef;

static ava_macro_subst_result ava_intr_struct_subst_impl(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool is_union);

static ava_compile_error* ava_intr_structdef_look_struct_up(
  const ava_struct** dst,
  const ava_macsub_context* context,
  ava_string name,
  const ava_compile_location* location);

static void ava_intr_structdef_read_adjectives(
  ava_macsub_context* context,
  ava_string macro_name,
  const ava_parse_unit** unit,
  const ava_parse_unit* end_unit,
  ava_bool* signedness,
  unsigned char* alignment,
  ava_struct_byte_order* byte_order);

static const ava_pointer_prototype* ava_intr_structdef_parse_prototype(
  ava_macsub_context* context,
  const ava_parse_unit* unit);

static void ava_intr_structdef_flip_struct(
  /* const ava_struct** */ void* sxt);

static ava_string ava_intr_structdef_to_string(
  const ava_intr_structdef* node);
static void ava_intr_structdef_cg_define(
  ava_intr_structdef* node, ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_structdef_vtable = {
  .name = "struct or union definition",
  .to_string = (ava_ast_node_to_string_f)ava_intr_structdef_to_string,
  .cg_define = (ava_ast_node_cg_define_f)ava_intr_structdef_cg_define,
  .cg_discard = (ava_ast_node_cg_discard_f)ava_intr_structdef_cg_define,/*sic*/
};

static ava_string ava_intr_structdef_to_string(
  const ava_intr_structdef* node
) {
  return ava_to_string(ava_value_of_struct(node->def));
}

static void ava_intr_structdef_cg_define(
  ava_intr_structdef* node, ava_codegen_context* context
) {
  if (node->defined) return;

  node->defined = ava_true;
  node->struct_sym->pcode_index =
    AVA_PCGB(decl_sxt, node->is_definition, node->def);
  ava_codegen_export(context, node->struct_sym);
}

ava_macro_subst_result ava_intr_struct_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  return ava_intr_struct_subst_impl(
    self, context, statement, provoker, ava_false);
}

ava_macro_subst_result ava_intr_union_subst(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  return ava_intr_struct_subst_impl(
    self, context, statement, provoker, ava_true);
}

static ava_macro_subst_result ava_intr_struct_subst_impl(
  const struct ava_symbol_s* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool is_union
) {
  const ava_parse_unit* name_unit = NULL, * parent_unit = NULL,
                      * body_unit = NULL, * kw_unit = NULL,
                      * field_name_unit, * unit, * type_unit, * len_unit;
  const ava_parse_statement* field_stmt;
  ava_string name, parent = AVA_ABSENT_STRING, kw, str, type;
  size_t num_fields, i, len;
  ava_struct* sxt;
  const ava_struct* parent_sxt;
  ava_compile_error* error;
  ava_map_value field_names_seen;
  ava_bool has_prototype;
  ava_integer array_length;
  ava_exception caught;
  ava_intr_structdef* node;
  ava_symbol* sym;

  AVA_MACRO_ARG_PARSE {
    AVA_MACRO_ARG_FROM_RIGHT_END {
      AVA_MACRO_ARG_BLOCK(body_unit, "body");
    }
    AVA_MACRO_ARG_FROM_RIGHT_BEGIN {
      AVA_MACRO_ARG_CURRENT_UNIT(name_unit, "name");
      AVA_MACRO_ARG_BAREWORD(name, "name");
      if (AVA_MACRO_ARG_HAS_ARG()) {
        AVA_MACRO_ARG_CURRENT_UNIT(kw_unit, "extends keyword");
        AVA_MACRO_ARG_BAREWORD(kw, "extends keyword");
        if (!ava_string_equal(AVA_ASCII9_STRING("extends"), kw)) {
          return ava_macsub_error_result(
            context, ava_error_bad_macro_keyword(
              &kw_unit->location,
              self->full_name, kw, AVA_ASCII9_STRING("extends")));
        }
        AVA_MACRO_ARG_CURRENT_UNIT(parent_unit, "parent");
        AVA_MACRO_ARG_BAREWORD(parent, "parent");
      }
    }
  }

  if (ava_string_is_present(parent) && is_union)
    return ava_macsub_error_result(
      context, ava_error_structdef_union_with_parent(
        &kw_unit->location, name));

  if (ava_string_is_present(parent)) {
    if ((error = ava_intr_structdef_look_struct_up(
           &parent_sxt, context, parent, &parent_unit->location)))
      return ava_macsub_error_result(context, error);

    if (parent_sxt->is_union)
      return ava_macsub_error_result(
        context, ava_error_structdef_parent_is_union(
          &parent_unit->location, name, parent));

    if (!parent_sxt->is_composable)
      return ava_macsub_error_result(
        context, ava_error_structdef_parent_is_noncomposable(
          &parent_unit->location, name, parent));
  } else {
    parent_sxt = NULL;
  }

  num_fields = 0;
  TAILQ_FOREACH(field_stmt, &body_unit->v.statements, next)
    if (!TAILQ_EMPTY(&field_stmt->units))
      ++num_fields;

  sxt = AVA_NEWA(ava_struct, fields, num_fields);
  sxt->name = name;
  sxt->parent = parent_sxt;
  sxt->is_union = is_union;
  sxt->num_fields = num_fields;

  /* Initialise all the field names so we can safely let other code see it.
   * Zero-initialised fields just describe a bunch of integers, but the name
   * needs to be something reasonably valid.
   */
  for (i = 0; i < num_fields; ++i)
    sxt->fields[i].name = AVA_EMPTY_STRING;

  /* Define the symbol now so that pointers below can reference it. */
  sym = AVA_NEW(ava_symbol);
  sym->type = ava_st_struct;
  sym->visibility = *(const ava_visibility*)self->v.macro.userdata;
  sym->level = ava_macsub_get_level(context);
  sym->full_name = ava_macsub_apply_prefix(context, name);
  sym->v.sxt.def = sxt;
  ava_macsub_put_symbol(context, sym, &name_unit->location);

  i = 0;
  field_names_seen = ava_empty_map();
  TAILQ_FOREACH(field_stmt, &body_unit->v.statements, next) {
    if (TAILQ_EMPTY(&field_stmt->units)) continue;

    field_name_unit = TAILQ_LAST(
      &field_stmt->units, ava_parse_unit_list_s);
    if (ava_put_bareword != field_name_unit->type) {
      AVA_STATIC_STRING(field_name_str, "field name");
      return ava_macsub_error_result(
        context, ava_error_macro_arg_must_be_bareword(
          &field_name_unit->location, field_name_str));
    }

    sxt->fields[i].name = field_name_unit->v.string;
    if (AVA_MAP_CURSOR_NONE != ava_map_find(
          field_names_seen, ava_value_of_string(sxt->fields[i].name)))
      return ava_macsub_error_result(
        context, ava_error_structdef_duplicate_field_name(
          &field_name_unit->location, name, sxt->fields[i].name));

    field_names_seen = ava_map_add(
      field_names_seen, ava_value_of_string(sxt->fields[i].name),
      ava_empty_map().v);

    if (TAILQ_FIRST(&field_stmt->units) == field_name_unit)
      return ava_macsub_error_result(
        context, ava_error_structdef_typeless_field(
          &field_name_unit->location));

    has_prototype = ava_false;
    TAILQ_FOREACH(unit, &field_stmt->units, next) {
      if (field_name_unit == unit) break;

      if (ava_put_bareword == unit->type) {
        str = unit->v.string;
        len = ava_strlen(str);
        if (len > 0 &&
            ('*' == ava_string_index(str, len-1) ||
             '&' == ava_string_index(str, len-1))) {
          has_prototype = ava_true;
          break;
        }
      }
    }

    type_unit = TAILQ_FIRST(&field_stmt->units);
    if (ava_put_bareword != type_unit->type)
      return ava_macsub_error_result(
        context, ava_error_macro_arg_must_be_bareword(
          &type_unit->location, AVA_ASCII9_STRING("type")));
    type = type_unit->v.string;
    unit = TAILQ_NEXT(type_unit, next);

    switch (ava_string_to_ascii9(type)) {
#define INTCASE(sis, ...)                                       \
    case AVA_ASCII9(__VA_ARGS__):                               \
      sxt->fields[i].type = ava_sft_int;                        \
      sxt->fields[i].v.vint.size = ava_sis_##sis;               \
      sxt->fields[i].v.vint.byte_order = ava_sbo_preferred;     \
      sxt->fields[i].v.vint.alignment =                         \
        AVA_STRUCT_NATURAL_ALIGNMENT;                           \
      sxt->fields[i].v.vint.sign_extend = ava_false;            \
      ava_intr_structdef_read_adjectives(                       \
        context, self->full_name, &unit, field_name_unit,       \
        &sxt->fields[i].v.vint.sign_extend,                     \
        &sxt->fields[i].v.vint.alignment,                       \
        &sxt->fields[i].v.vint.byte_order);                     \
      break

      /*                 1   2   3   4   5   6   7   8   9 */
    INTCASE(ava_integer,'i','n','t','e','g','e','r');
    INTCASE(byte,       'b','y','t','e');
    INTCASE(short,      's','h','o','r','t');
    INTCASE(int,        'i','n','t');
    INTCASE(long,       'l','o','n','g');
    INTCASE(c_short,    'c','-','s','h','o','r','t');
    INTCASE(c_int,      'c','-','i','n','t');
    INTCASE(c_long,     'c','-','l','o','n','g');
    INTCASE(c_llong,    'c','-','l','l','o','n','g');
    INTCASE(c_size,     'c','-','s','i','z','e');
    INTCASE(c_intptr,   'c','-','i','n','t','p','t','r');
    INTCASE(word,       'w','o','r','d');
#undef INTCASE

    case AVA_ASCII9('a','t','o','m','i','c'):
      if (has_prototype) {
        sxt->fields[i].type = ava_sft_ptr;
        sxt->fields[i].v.vptr.is_atomic = ava_true;
        sxt->fields[i].v.vptr.prot = ava_intr_structdef_parse_prototype(
          context, unit);
        unit = TAILQ_NEXT(unit, next);
      } else {
        sxt->fields[i].type = ava_sft_int;
        sxt->fields[i].v.vint.size = ava_sis_word;
        sxt->fields[i].v.vint.byte_order = ava_sbo_preferred;
        sxt->fields[i].v.vint.alignment = AVA_STRUCT_NATURAL_ALIGNMENT;
        sxt->fields[i].v.vint.sign_extend = ava_false;
        sxt->fields[i].v.vint.is_atomic = ava_true;
        ava_intr_structdef_read_adjectives(
          context, self->full_name, &unit, field_name_unit,
          &sxt->fields[i].v.vint.sign_extend, NULL, NULL);
      }
      break;

#define REALCASE(srs, ...)                                      \
    case AVA_ASCII9(__VA_ARGS__):                               \
      sxt->fields[i].type = ava_sft_real;                       \
      sxt->fields[i].v.vreal.size = ava_srs_##srs;              \
      sxt->fields[i].v.vreal.byte_order = ava_sbo_preferred;    \
      sxt->fields[i].v.vreal.alignment =                        \
        AVA_STRUCT_NATURAL_ALIGNMENT;                           \
      ava_intr_structdef_read_adjectives(                       \
        context, self->full_name, &unit, field_name_unit, NULL, \
        &sxt->fields[i].v.vreal.alignment,                      \
        &sxt->fields[i].v.vreal.byte_order);                    \
      break
    REALCASE(ava_real,          'r','e','a','l');
    REALCASE(single,            's','i','n','g','l','e');
    REALCASE(double,            'd','o','u','b','l','e');
    REALCASE(extended,          'e','x','t','e','n','d','e','d');
#undef REALCASE

    case AVA_ASCII9('v','a','l','u','e'):
      sxt->fields[i].type = ava_sft_value;
      break;

    case AVA_ASCII9('h','y','b','r','i','d'):
      if (unit == field_name_unit) {
        AVA_STATIC_STRING(hybrid_str, "pointer prototype after \"hybrid\"");
        return ava_macsub_error_result(
          context, ava_error_macro_arg_missing(
            &type_unit->location, self->full_name, hybrid_str));
      }

      sxt->fields[i].type = ava_sft_hybrid;
      sxt->fields[i].v.vptr.prot = ava_intr_structdef_parse_prototype(
        context, unit);
      unit = TAILQ_NEXT(unit, next);
      break;

    case AVA_ASCII9('s','t','r','u','c','t'): {
      AVA_STATIC_STRING(expected_str, "struct name (after \"struct\")");
      if (unit == field_name_unit)
        return ava_macsub_error_result(
          context, ava_error_macro_arg_missing(
            &type_unit->location, self->full_name, expected_str));
      if (ava_put_bareword != unit->type)
        return ava_macsub_error_result(
          context, ava_error_macro_arg_must_be_bareword(
            &unit->location, expected_str));

      if ((error = ava_intr_structdef_look_struct_up(
             &sxt->fields[i].v.vcompose.member, context,
             unit->v.string, &unit->location)))
        return ava_macsub_error_result(context, error);

      if (sxt == sxt->fields[i].v.vcompose.member) {
        /* Defensively reset the field to a "safe" type in case something tries
         * to stringify the struct.
         */
        sxt->fields[i].type = ava_sft_value;
        return ava_macsub_error_result(
          context, ava_error_structdef_compose_self(
            &unit->location, name));
      }

      if (!sxt->fields[i].v.vcompose.member->is_composable)
        return ava_macsub_error_result(
          context, ava_error_structdef_composed_noncomposable(
            &unit->location, sxt->fields[i].name,
            sxt->fields[i].v.vcompose.member->name));

      unit = TAILQ_NEXT(unit, next);
      /* Check for array specifier */
      if (unit == field_name_unit ||
          ava_put_semiliteral != unit->type) {
        /* No array specifier, so it's a simple compose */
        sxt->fields[i].type = ava_sft_compose;
        sxt->fields[i].v.vcompose.array_length = 1;
      } else {
        /* Array specifier */
        len_unit = TAILQ_FIRST(&unit->v.units);
        if (!len_unit) {
          /* No length, it's a tail */
          if (i+1 != num_fields)
            return ava_macsub_error_result(
              context, ava_error_structdef_tail_not_at_end(
                &field_name_unit->location, field_name_unit->v.string));
          if (is_union)
            return ava_macsub_error_result(
              context, ava_error_structdef_tail_in_union(
                &field_name_unit->location, field_name_unit->v.string));

          sxt->fields[i].type = ava_sft_tail;
          sxt->fields[i].v.vcompose.array_length = 0;
        } else if (TAILQ_NEXT(len_unit, next)) {
          return ava_macsub_error_result(
            context, ava_error_structdef_extra_tokens_in_array_length(
              &TAILQ_NEXT(len_unit, next)->location));
        } else if (ava_put_bareword != len_unit->type) {
          AVA_STATIC_STRING(array_length_str, "array length");
          return ava_macsub_error_result(
            context, ava_error_macro_arg_must_be_bareword(
              &len_unit->location, array_length_str));
        } else if (!ava_integer_try_parse(
                     &array_length, len_unit->v.string, -1) ||
                   array_length < 0 ||
                   array_length != (ava_integer)(size_t)array_length) {
          return ava_macsub_error_result(
            context, ava_error_structdef_invalid_array_length(
              &len_unit->location, sxt->fields[i].name, len_unit->v.string));
        } else {
          sxt->fields[i].type = ava_sft_array;
          sxt->fields[i].v.vcompose.array_length = array_length;
        }

        unit = TAILQ_NEXT(unit, next);
      }
    } break;

    default:
      if (!has_prototype)
        return ava_macsub_error_result(
          context, ava_error_structdef_invalid_type(&type_unit->location));

      sxt->fields[i].type = ava_sft_ptr;
      sxt->fields[i].v.vptr.prot = ava_intr_structdef_parse_prototype(
        context, type_unit);
      break;
    } /* end switch */

    if (unit != field_name_unit)
      return ava_macsub_error_result(
        context, ava_error_structdef_garbage_after_type(
          &unit->location));

    ++i;
  }

  /* Parsed all the fields. Flip through a string so that all the extra data
   * gets initialised and to run full validation.
   */
  if (ava_catch(&caught, ava_intr_structdef_flip_struct, &sxt)) {
    if (&ava_format_exception == caught.type) {
      return ava_macsub_error_result(
        context, ava_error_structdef_invalid(
          &provoker->location, ava_to_string(
            ava_exception_get_value(&caught))));
    } else {
      ava_rethrow(caught);
    }
  }
  sym->v.sxt.def = sxt;

  node = AVA_NEW(ava_intr_structdef);
  node->header.v = &ava_intr_structdef_vtable;
  node->header.context = context;
  node->header.location = provoker->location;
  node->def = sxt;
  node->defined = ava_false;
  node->is_definition = ava_true;
  node->struct_sym = sym;

  sym->definer = (ava_ast_node*)node;

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = (ava_ast_node*)node }
  };
}

static ava_compile_error* ava_intr_structdef_look_struct_up(
  const ava_struct** dst,
  const ava_macsub_context* context,
  ava_string name,
  const ava_compile_location* location
) {
  const ava_symbol** results;
  size_t num_results;

  num_results = ava_symtab_get(
    &results, ava_macsub_get_symtab(context), name);

  if (!num_results)
    return ava_error_no_such_struct(location, name);

  if (num_results > 1)
    return ava_error_ambiguous_struct(
      location, name, num_results,
      results[0]->full_name, results[1]->full_name);

  if (ava_st_struct != results[0]->type)
    return ava_error_symbol_not_a_struct(
      location,
      results[0]->full_name, ava_symbol_type_name(results[0]));

  *dst = results[0]->v.sxt.def;
  return NULL;
}

static void ava_intr_structdef_read_adjectives(
  ava_macsub_context* context,
  ava_string macro_name,
  const ava_parse_unit** unitptr,
  const ava_parse_unit* end_unit,
  ava_bool* signedness,
  unsigned char* alignment,
  ava_struct_byte_order* byte_order
) {
  AVA_STATIC_STRING(adjective_str, "type modifier keyword");
  ava_integer align_value;

#define unit (*unitptr)
  while (unit != end_unit) {
    if (ava_put_bareword != unit->type) {
      ava_macsub_record_error(
        context, ava_error_macro_arg_must_be_bareword(
          &unit->location, adjective_str));
      return;
    }

    switch (ava_string_to_ascii9(unit->v.string)) {
#define SIGNEDNESS(value, ...)                  \
    case AVA_ASCII9(__VA_ARGS__):               \
      if (!signedness) goto unexpected;         \
      *signedness = value;                      \
      signedness = NULL;                        \
      unit = TAILQ_NEXT(unit, next);            \
      break

    SIGNEDNESS(ava_true,         's','i','g','n','e','d');
    SIGNEDNESS(ava_false,'u','n','s','i','g','n','e','d');
#undef SIGNEDNESS

#define BYTEORDER(value, ...)                   \
    case AVA_ASCII9(__VA_ARGS__):               \
      if (!byte_order) goto unexpected;         \
      *byte_order = ava_sbo_##value;            \
      byte_order = NULL;                        \
      unit = TAILQ_NEXT(unit, next);            \
      break
    BYTEORDER(preferred,        'p','r','e','f','e','r','r','e','d');
    BYTEORDER(native,           'n','a','t','i','v','e');
    BYTEORDER(little,           'l','i','t','t','l','e');
    BYTEORDER(big,              'b','i','g');
#undef BYTEORDER

    case AVA_ASCII9('a','l','i','g','n'):
      if (!alignment) goto unexpected;

      if (TAILQ_NEXT(unit, next) == end_unit) {
        AVA_STATIC_STRING(alignment_str, "alignment");
        ava_macsub_record_error(
          context, ava_error_macro_arg_missing(
            &unit->location, macro_name, alignment_str));
        return;
      }

      unit = TAILQ_NEXT(unit, next);
      if (ava_put_bareword != unit->type) {
        AVA_STATIC_STRING(alignment_str, "alignment");
        ava_macsub_record_error(
          context, ava_error_macro_arg_must_be_bareword(
            &unit->location, alignment_str));
      }

      if (ava_string_equal(AVA_ASCII9_STRING("native"),
                           unit->v.string)) {
        *alignment = AVA_STRUCT_NATIVE_ALIGNMENT;
      } else if (ava_string_equal(
                   AVA_ASCII9_STRING("natural"), unit->v.string)) {
        *alignment = AVA_STRUCT_NATURAL_ALIGNMENT;
      } else if (!ava_integer_try_parse(&align_value, unit->v.string, -1)) {
        ava_macsub_record_error(
          context, ava_error_structdef_invalid_alignment(
            &unit->location, unit->v.string));
        return;
      } else {
        switch (align_value) {
#define CASE(n) case 1<<n: *alignment = n; break
          CASE(0); CASE(1); CASE(2); CASE(3);
          CASE(4); CASE(5); CASE(6); CASE(7);
          CASE(8); CASE(9); CASE(10); CASE(11);
          CASE(12); CASE(13);
        default:
          ava_macsub_record_error(
            context, ava_error_structdef_invalid_alignment(
              &unit->location, unit->v.string));
          return;
        }
      }
      alignment = NULL;
      unit = TAILQ_NEXT(unit, next);
      break;

    default:
    unexpected:
      ava_macsub_record_error(
        context, ava_error_structdef_unexpected_type_modifier(
          &unit->location, unit->v.string));
      return;
    }
  }
#undef unit
}

static const ava_pointer_prototype* ava_intr_structdef_parse_prototype(
  ava_macsub_context* context,
  const ava_parse_unit* unit
) {
  AVA_STATIC_STRING(expected_str, "pointer prototype");

  const ava_pointer_prototype* prot;
  ava_pointer_prototype* ret;
  const ava_struct* resolved_sxt;
  ava_compile_error* error;

  if (ava_put_bareword != unit->type) {
    ava_macsub_record_error(
      context, ava_error_macro_arg_must_be_bareword(
        &unit->location, expected_str));
    return &ava_pointer_proto_mut_void;
  }

  prot = ava_pointer_prototype_parse(unit->v.string);
  if (!prot) {
    ava_macsub_record_error(
      context, ava_error_structdef_invalid_pointer_prototype(
        &unit->location, unit->v.string));
    return &ava_pointer_proto_mut_void;
  }

  if ((error = ava_intr_structdef_look_struct_up(
         &resolved_sxt, context, prot->tag, &unit->location))) {
    ava_macsub_record_error(context, error);
    return prot;
  }

  ret = AVA_NEW(ava_pointer_prototype);
  *ret = (ava_pointer_prototype)
    AVA_INIT_POINTER_PROTOTYPE(resolved_sxt->name, prot->is_const);
  return ret;
}

static void ava_intr_structdef_flip_struct(void* vptr) {
  const ava_struct** ptr = vptr;

  *ptr = ava_struct_of_value(
    ava_value_of_string(
      ava_to_string(
        ava_value_of_struct(*ptr))));
}
