/*-
 * Copyright (c) 2015, 2016, Jason Lingle
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
#include <assert.h>

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
#include "../avalanche/pcode.h"
#include "../avalanche/code-gen.h"
#include "../avalanche/exception.h"
#include "user-macro.h"

typedef struct {
  ava_ast_node header;
  ava_symbol* symbol;
  ava_bool defined;
} ava_intr_user_macro;

static ava_pcode_macro_list* ava_intr_user_macro_make_body(
  ava_macsub_context* context,
  const ava_parse_unit* first,
  ava_visibility visibility);
static void ava_intr_user_macro_body_translate_unit(
  ava_macsub_context* context,
  ava_pcm_builder* builder,
  const ava_parse_unit* unit,
  ava_visibility visibility);
static void ava_intr_user_macro_body_translate_bareword(
  ava_macsub_context* context,
  ava_pcm_builder* builder,
  const ava_parse_unit* unit,
  ava_visibility visibility,
  ava_bool is_expander);
static void ava_intr_user_macro_body_translate_splice(
  ava_macsub_context* context,
  ava_pcm_builder* builder,
  const ava_compile_location* location,
  signed direction,
  ava_string tail);
static ava_bool ava_intr_user_macro_parse_offset(
  ava_uint* begin, ava_uint* end, ava_string tail);

static ava_parse_statement* ava_intr_user_macro_clone_units(
  const ava_parse_unit* begin_inclusive, const ava_parse_unit* end_exclusive);
static ava_integer ava_intr_user_macro_statement_length(
  const ava_parse_statement* statement);

static ava_string ava_intr_user_macro_to_string(
  const ava_intr_user_macro* node);
static void ava_intr_user_macro_cg_define(
  ava_intr_user_macro* node, ava_codegen_context* context);

static const ava_ast_node_vtable ava_intr_user_macro_vtable = {
  .name = "macro definition",
  .to_string = (ava_ast_node_to_string_f)ava_intr_user_macro_to_string,
  /* sic */
  .cg_discard = (ava_ast_node_cg_discard_f)ava_intr_user_macro_cg_define,
  .cg_define = (ava_ast_node_cg_define_f)ava_intr_user_macro_cg_define,
};

typedef struct {
  ava_string str;
  ava_integer ret;
} ava_intr_user_macro_cvt_prec_data;

static void ava_intr_user_macro_cvt_prec(void* d) {
  ava_intr_user_macro_cvt_prec_data* data = d;

  data->ret = ava_integer_of_value(
    ava_value_of_string(data->str), -1);
}

ava_macro_subst_result ava_intr_user_macro_subst(
  const ava_symbol* self,
  ava_macsub_context* context,
  const ava_parse_statement* statement,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  AVA_STATIC_STRING(out_of_range, "Out of legal range.");
  ava_exception ex;
  const ava_parse_unit* name_unit = NULL, * type_unit, * precedence_unit;
  const ava_parse_unit* definition_begin = NULL;
  ava_string name, type_str, precedence_str;
  ava_symbol_type type = -1;
  ava_integer precedence = -1;
  ava_pcode_macro_list* body;
  ava_ast_node* this;
  ava_visibility visibility;

  visibility = *(const ava_visibility*)self->v.macro.userdata;

  AVA_MACRO_ARG_PARSE {
    AVA_MACRO_ARG_FROM_RIGHT_BEGIN {
      AVA_MACRO_ARG_CURRENT_UNIT(name_unit, "name");
      AVA_MACRO_ARG_BAREWORD(name, "name");
      AVA_MACRO_ARG_CURRENT_UNIT(type_unit, "type");
      AVA_MACRO_ARG_BAREWORD(type_str, "type");

      switch (ava_string_to_ascii9(type_str)) {
      case AVA_ASCII9('e','x','p','a','n','d'):
        type = ava_st_expander_macro;
        break;

      case AVA_ASCII9('c','o','n','t','r','o','l'):
        type = ava_st_control_macro;
        break;

      case AVA_ASCII9('o','p'):
        type = ava_st_operator_macro;
        break;

      case AVA_ASCII9('f','u','n'):
        type = ava_st_function_macro;
        break;

      default:
        return ava_macsub_error_result(
          context, ava_error_bad_macro_type(
            &type_unit->location, type_str));
      }

      if (ava_st_operator_macro == type) {
        ava_intr_user_macro_cvt_prec_data data;
        AVA_MACRO_ARG_CURRENT_UNIT(precedence_unit, "precedence");
        AVA_MACRO_ARG_BAREWORD(precedence_str, "precedence");

        data.str = precedence_str;
        if (ava_catch(&ex, ava_intr_user_macro_cvt_prec, &data)) {
          if (&ava_format_exception == ex.type) {
            return ava_macsub_error_result(
              context, ava_error_bad_macro_precedence(
                &precedence_unit->location, precedence_str,
                ava_to_string(ava_exception_get_value(&ex))));
          } else {
            ava_rethrow(ex);
          }
        }
        precedence = data.ret;

        if (precedence < 1 || precedence > AVA_MAX_OPERATOR_MACRO_PRECEDENCE)
          return ava_macsub_error_result(
            context, ava_error_bad_macro_precedence(
              &precedence_unit->location, precedence_str, out_of_range));
      } else {
        precedence = 0;
      }

      AVA_MACRO_ARG_UNIT(definition_begin, "macro definition");
      AVA_MACRO_ARG_FOR_REST {
        AVA_MACRO_ARG_CONSUME();
      }
    }
  }

  assert(-1 != precedence);
  assert(-1 != (signed)type);
  assert(definition_begin);
  assert(name_unit);

  body = ava_intr_user_macro_make_body(context, definition_begin,
                                       visibility);
  this = ava_intr_user_macro_put(
    context, type, visibility, name, precedence, body,
    &provoker->location, &name_unit->location);

  return (ava_macro_subst_result) {
    .status = ava_mss_done,
    .v = { .node = this },
  };
}

ava_ast_node* ava_intr_user_macro_put(
  ava_macsub_context* context, ava_symbol_type type,
  ava_visibility visibility, ava_string name,
  int precedence, ava_pcode_macro_list* body,
  const ava_compile_location* main_location,
  const ava_compile_location* name_location
) {
  ava_intr_user_macro* this;
  ava_symbol* symbol;

  this = AVA_NEW(ava_intr_user_macro);
  symbol = AVA_NEW(ava_symbol);

  symbol->type = type;
  symbol->level = ava_macsub_get_level(context);
  symbol->visibility = visibility;
  symbol->definer = (ava_ast_node*)this;
  symbol->full_name = ava_macsub_apply_prefix(context, name);
  symbol->v.macro.precedence = precedence;
  symbol->v.macro.macro_subst = ava_intr_user_macro_eval;
  symbol->v.macro.userdata = body;

  this->header.v = &ava_intr_user_macro_vtable;
  this->header.location = *main_location;
  this->header.context = context;
  this->symbol = symbol;

  ava_macsub_put_symbol(context, symbol, name_location);

  return (ava_ast_node*)this;
}

static ava_pcode_macro_list* ava_intr_user_macro_make_body(
  ava_macsub_context* context,
  const ava_parse_unit* unit,
  ava_visibility visibility
) {
  ava_pcm_builder* builder;

  builder = ava_pcm_builder_new(NULL);
  for (; unit; unit = TAILQ_NEXT(unit, next)) {
    ava_intr_user_macro_body_translate_unit(context, builder, unit,
                                            visibility);
    ava_pcmb_append(builder);
  }

  return ava_pcm_builder_get(builder);
}

static void ava_intr_user_macro_body_translate_unit(
  ava_macsub_context* context,
  ava_pcm_builder* builder,
  const ava_parse_unit* unit,
  ava_visibility visibility
) {
  const ava_parse_unit* child;
  const ava_parse_statement* statement;

  switch (unit->type) {
  case ava_put_bareword:
    ava_intr_user_macro_body_translate_bareword(context, builder, unit,
                                                visibility, ava_false);
    break;

  case ava_put_expander:
    ava_intr_user_macro_body_translate_bareword(context, builder, unit,
                                                visibility, ava_true);
    break;

  case ava_put_astring:
    ava_pcmb_astring(builder, unit->v.string);
    break;

  case ava_put_lstring:
    ava_pcmb_lstring(builder, unit->v.string);
    break;

  case ava_put_rstring:
    ava_pcmb_rstring(builder, unit->v.string);
    break;

  case ava_put_lrstring:
    ava_pcmb_lrstring(builder, unit->v.string);
    break;

  case ava_put_verbatim:
    ava_pcmb_verbatim(builder, unit->v.string);
    break;

  case ava_put_spread:
    ava_intr_user_macro_body_translate_unit(
      context, builder, unit->v.unit, visibility);
    ava_pcmb_spread(builder);
    break;

  case ava_put_substitution:
  case ava_put_block:
    if (ava_put_substitution == unit->type)
      ava_pcmb_subst(builder);
    else
      ava_pcmb_block(builder);

    TAILQ_FOREACH(statement, &unit->v.statements, next) {
      ava_pcmb_statement(builder);
      TAILQ_FOREACH(child, &statement->units, next) {
        ava_intr_user_macro_body_translate_unit(context, builder, child,
                                                visibility);
        ava_pcmb_append(builder);
      }
      ava_pcmb_append(builder);
    }
    break;

  case ava_put_semiliteral:
    ava_pcmb_semilit(builder);

    TAILQ_FOREACH(child, &unit->v.units, next) {
      ava_intr_user_macro_body_translate_unit(context, builder, child,
                                              visibility);
      ava_pcmb_append(builder);
    }
    break;
  }
}

static void ava_intr_user_macro_body_translate_bareword(
  ava_macsub_context* context,
  ava_pcm_builder* builder,
  const ava_parse_unit* unit,
  ava_visibility visibility,
  ava_bool is_expander
) {
  const ava_symbol* symbol, ** results;
  ava_string value, chopped;
  char sigil;
  size_t strlen, num_results;

  if (is_expander)
    assert(ava_put_expander == unit->type);
  else
    assert(ava_put_bareword == unit->type);

  value = unit->v.string;
  strlen = ava_strlen(value);
  sigil = strlen? ava_string_index(value, 0) : 0;
  chopped = strlen? ava_string_slice(value, 1, strlen) : value;

  switch (sigil) {
  case '!':
    if (strlen < 2) {
      ava_macsub_record_error(
        context, ava_error_empty_bareword_in_macro_definition(
          &unit->location, value));
      goto error;
    }
    if (is_expander)
      ava_pcmb_expander(builder, chopped);
    else
      ava_pcmb_bareword(builder, chopped);
    break;

  case '#':
    if (1 == strlen || '#' != ava_string_index(value, strlen-1)) {
      ava_macsub_record_error(
        context, ava_error_bad_macro_hash_bareword(
          &unit->location, value));
      goto error;
    }
    if (is_expander)
      ava_pcmb_expander(builder, value);
    else
      ava_pcmb_bareword(builder, value);
    break;

  case '$':
    /* Only can get this way because the parser decided it should */
    assert(1 == strlen);
    assert(!is_expander);
    ava_pcmb_bareword(builder, value);
    break;

  case '%':
    if (strlen < 2) {
      ava_macsub_record_error(
        context, ava_error_empty_bareword_in_macro_definition(
          &unit->location, value));
      goto error;
    }

    num_results = ava_symtab_get(
      &results, ava_macsub_get_symtab(context), chopped);

    if (0 == num_results) {
      ava_macsub_record_error(
        context, ava_error_macro_resolved_bareword_not_found(
          &unit->location, chopped));
      goto error;
    } else if (num_results > 1) {
      ava_macsub_record_error(
        context, ava_error_macro_resolved_bareword_ambiguous(
          &unit->location, chopped));
      goto error;
    }

    symbol = results[0];
    if (symbol->visibility < visibility) {
      ava_macsub_record_error(
        context, ava_error_macro_resolved_bareword_invisible(
          &unit->location, symbol->full_name));
      /* Continue anyway */
    }

    if (is_expander)
      ava_pcmb_expander(builder, symbol->full_name);
    else
      ava_pcmb_bareword(builder, symbol->full_name);
    break;

  case '?':
    if (is_expander) goto bad_sigil;
    /* There's strictly no reason to require this, but if '?' is an operator,
     * the result of forgetting to say '%?' would be extremely confusing.
     */
    if (strlen < 2) {
      ava_macsub_record_error(
        context, ava_error_empty_bareword_in_macro_definition(
          &unit->location, value));
      goto error;
    }
    ava_pcmb_gensym(builder, chopped);
    break;

  case '<':
  case '>':
    if (is_expander) goto bad_sigil;

    ava_intr_user_macro_body_translate_splice(
      context, builder, &unit->location,
      ('>' == sigil)? +1 : -1, chopped);
    break;

  default:
  bad_sigil:
    ava_macsub_record_error(
      context, ava_error_bad_macro_bareword_sigil(
        &unit->location, value));
    goto error;
  }

  return;

  error:
  ava_pcmb_die(builder);
}

static void ava_intr_user_macro_body_translate_splice(
  ava_macsub_context* context,
  ava_pcm_builder* builder,
  const ava_compile_location* location,
  signed direction,
  ava_string tail
) {
  enum { star, plus, singular } plurality;
  ava_uint begin, end;
  size_t strlen = ava_strlen(tail);
  char plurality_ch;

  /* TODO: Indicate context */

  if (strlen > 0) {
    plurality_ch = ava_string_index(tail, strlen-1);
    if ('+' == plurality_ch) {
      plurality = plus;
      tail = ava_string_slice(tail, 0, strlen-1);
      --strlen;
    } else if ('*' == plurality_ch) {
      plurality = star;
      tail = ava_string_slice(tail, 0, strlen-1);
      --strlen;
    } else {
      plurality = singular;
    }
  } else {
    plurality = plus;
  }

  if (0 == strlen) {
    begin = ~0u;
    end = ~0u;
  } else {
    if (!ava_intr_user_macro_parse_offset(&begin, &end, tail)) {
      ava_macsub_record_error(
        context, ava_error_bad_macro_slice_offset(location, tail));
      goto error;
    }

    if (singular == plurality) {
      /* Implicit plurality is plus if both endpoints given */
      if (~begin && ~end) {
        plurality = plus;
      }
    }
  }

  (direction < 0? ava_pcmb_left : ava_pcmb_right)(builder);
  if (~begin && begin)
    (direction < 0? ava_pcmb_curtail : ava_pcmb_behead)(builder, begin);
  if (~end)
    (direction < 0? ava_pcmb_behead : ava_pcmb_curtail)(builder, end);
  if (singular == plurality) {
    /* left, begin only: rightmost
     * left, end only: leftmost
     * right, begin only: leftmost
     * right, end only: rightmost
     */
    ((direction < 0) ^ (!!~begin)? ava_pcmb_head : ava_pcmb_tail)(builder, 1);
    ava_pcmb_singular(builder);
  } else if (plus == plurality) {
    ava_pcmb_nonempty(builder);
  }
  return;

  error:
  ava_pcmb_bareword(builder, AVA_EMPTY_STRING);
}

static ava_bool ava_intr_user_macro_parse_offset(
  ava_uint* begin_dst, ava_uint* end_dst, ava_string tail
) {
  ava_str_tmpbuff tmp;
  const char* s;
  ava_ulong begin = ~0uLL, end = ~0uLL;

  s = ava_string_to_cstring_buff(tmp, tail);
  assert(*s);

  if ('-' != *s) {
    begin = 0;
    while (*s && '-' != *s) {
      if (*s < '0' || *s > '9') return ava_false;

      begin *= 10;
      begin += *s - '0';
      if (begin >= 0xFFFFFFFFULL) return ava_false;

      ++s;
    }
  }

  if ('-' == *s) {
    ++s;
    if (!*s) return ava_false;
    end = 0;
    while (*s) {
      if (*s < '0' || *s > '9') return ava_false;

      end *= 10;
      end += *s - '0';
      if (end >= 0xFFFFFFFFULL) return ava_false;

      ++s;
    }
  }

  if (*s) return ava_false;

  if (!~begin && !~end)
    /* just "-" */
    return ava_false;

  *begin_dst = begin;
  *end_dst = end;
  return ava_true;
}

static ava_string ava_intr_user_macro_to_string(
  const ava_intr_user_macro* node
) {
  ava_string accum;

  accum = AVA_ASCII9_STRING("macro ");
  accum = ava_strcat(accum, node->symbol->full_name);
  switch (node->symbol->type) {
  case ava_st_control_macro:
    accum = ava_strcat(accum, AVA_ASCII9_STRING(" control "));
    break;

  case ava_st_operator_macro:
    accum = ava_strcat(accum, AVA_ASCII9_STRING(" op "));
    accum = ava_strcat(
      accum, ava_to_string(ava_value_of_integer(
                             node->symbol->v.macro.precedence)));
    accum = ava_strcat(accum, AVA_ASCII9_STRING(" "));
    break;

  case ava_st_function_macro:
    accum = ava_strcat(accum, AVA_ASCII9_STRING(" fun "));
    break;

  default: abort();
  }
  accum = ava_strcat(
    accum, ava_pcode_macro_list_to_string(
      node->symbol->v.macro.userdata, 1));
  return accum;
}

static void ava_intr_user_macro_cg_define(
  ava_intr_user_macro* node,
  ava_codegen_context* context
) {
  ava_pcm_builder* builder;
  ava_pcg_macro* macro;

  if (node->defined || ava_v_private == node->symbol->visibility)
    return;

  node->symbol->pcode_index =
    AVA_PCGB(macro, ava_v_public == node->symbol->visibility,
             node->symbol->full_name,
             node->symbol->type,
             node->symbol->v.macro.precedence,
             &builder);
  macro = (ava_pcg_macro*)
    TAILQ_LAST(ava_pcg_builder_get(
                 ava_pcx_builder_get_parent(
                   ava_codegen_get_builder(context))),
               ava_pcode_global_list_s);
  macro->body = (ava_pcode_macro_list*)node->symbol->v.macro.userdata;
  node->defined = ava_true;
}

static ava_parse_statement* ava_intr_user_macro_clone_units(
  const ava_parse_unit* begin_inclusive, const ava_parse_unit* end_exclusive
) {
  ava_parse_statement* statement;
  const ava_parse_unit* src;
  ava_parse_unit* unit;

  statement = AVA_NEW(ava_parse_statement);
  TAILQ_INIT(&statement->units);

  for (src = begin_inclusive; src != end_exclusive;
       src = TAILQ_NEXT(src, next)) {
    unit = AVA_CLONE(*src);
    TAILQ_INSERT_TAIL(&statement->units, unit, next);
  }

  return statement;
}

static ava_integer ava_intr_user_macro_statement_length(
  const ava_parse_statement* statement
) {
  const ava_parse_unit* unit;
  ava_integer count = 0;

  TAILQ_FOREACH(unit, &statement->units, next)
    ++count;

  return count;
}

ava_macro_subst_result ava_intr_user_macro_eval(
  const ava_symbol* self,
  ava_macsub_context* context,
  const ava_parse_statement* container,
  const ava_parse_unit* provoker,
  ava_bool* consumed_other_statements
) {
  typedef enum {
    set_unit, set_statement
  } stack_entry_type;

  typedef struct stack_entry_s {
    stack_entry_type type;
    union {
      ava_parse_unit* unit;
      ava_parse_statement* statement;
    } v;

    SLIST_ENTRY(stack_entry_s) next;
  } stack_entry;

  SLIST_HEAD(,stack_entry_s) stack;
  ava_parse_statement* statement;
  const ava_pcode_macro_list* instructions;
  const ava_pcode_macro* instr;
  const ava_parse_unit* left_begin, * left_end, * right_begin, * right_end;
  const ava_compile_location* last_location = &provoker->location;
  ava_string where = AVA_ASCII9_STRING("unknown");

#define DIE(message) do {                               \
    AVA_STATIC_STRING(_message, message);               \
    return ava_macsub_error_result(                     \
      context, ava_error_user_macro_execution_error(    \
        &provoker->location, _message));                \
  } while (0)

#define PUSH_STATEMENT(_statement) do {         \
    stack_entry* _e = AVA_NEW(stack_entry);     \
    _e->type = set_statement;                   \
    _e->v.statement = (_statement);             \
    SLIST_INSERT_HEAD(&stack, _e, next);        \
  } while (0)

#define PUSH_UNIT(_unit) do {                   \
    stack_entry* _e = AVA_NEW(stack_entry);     \
    _e->type = set_unit;                        \
    _e->v.unit = (_unit);                       \
    SLIST_INSERT_HEAD(&stack, _e, next);        \
  } while (0)

#define POP() do {                                      \
    SLIST_REMOVE_HEAD(&stack, next);                    \
  } while (0)

#define TOS(dst) do {                           \
    (dst) = SLIST_FIRST(&stack);                \
    if (!(dst)) DIE("Stack underflow.");        \
  } while (0)

#define TOS_STATEMENT(dst) do {                         \
    stack_entry* _e;                                    \
    TOS(_e);                                            \
    if (set_statement != _e->type)                      \
      DIE("Expected to pop statement, got unit");       \
    (dst) = _e->v.statement;                            \
  } while (0)

#define TOS_UNIT(dst) do {                              \
    stack_entry* _e;                                    \
    TOS(_e);                                            \
    if (set_unit != _e->type)                           \
      DIE("Expected to pop unit, got statement");       \
    (dst) = _e->v.unit;                                 \
  } while (0)

#define NEAR(unit) do {                                 \
    const ava_parse_unit* _unit = (unit);               \
    if (_unit) last_location = &_unit->location;        \
  } while (0)

#define MISSING_ARG_UNLESS(condition) do {              \
    if (!(condition)) {                                 \
      return ava_macsub_error_result(                   \
        context, ava_error_user_macro_not_enough_args(  \
          last_location, self->full_name, where));      \
    }                                                   \
  } while (0)

#define PUSH_STRINGOID(_type, _value) do {              \
    ava_parse_unit* _unit = AVA_NEW(ava_parse_unit);    \
    _unit->type = (_type);                              \
    _unit->location = provoker->location;               \
    _unit->v.string = (_value);                         \
    PUSH_UNIT(_unit);                                   \
  } while (0)

  SLIST_INIT(&stack);
  statement = AVA_NEW(ava_parse_statement);
  TAILQ_INIT(&statement->units);
  PUSH_STATEMENT(statement);

  left_begin = TAILQ_FIRST(&container->units);
  left_end = provoker;
  right_begin = TAILQ_NEXT(provoker, next);
  right_end = NULL;

  ava_macsub_gensym_seed(context, &provoker->location);

  instructions = self->v.macro.userdata;
  TAILQ_FOREACH(instr, instructions, next) {
    switch (instr->type) {
    case ava_pcmt_context: {
      const ava_pcm_context* c = (const ava_pcm_context*)instr;
      where = c->value;
    } break;

    case ava_pcmt_left: {
      NEAR(provoker);
      PUSH_STATEMENT(ava_intr_user_macro_clone_units(left_begin, left_end));
    } break;

    case ava_pcmt_right: {
      NEAR(provoker);
      PUSH_STATEMENT(ava_intr_user_macro_clone_units(right_begin, right_end));
    } break;

    case ava_pcmt_head: {
      const ava_pcm_head* h = (const ava_pcm_head*)instr;
      ava_parse_statement* s;
      ava_parse_unit* last;
      ava_integer count, length;

      TOS_STATEMENT(s);
      NEAR(TAILQ_LAST(&s->units, ava_parse_unit_list_s));
      length = ava_intr_user_macro_statement_length(s);
      count = length - h->count;
      MISSING_ARG_UNLESS(count >= 0);

      while (count--) {
        last = TAILQ_LAST(&s->units, ava_parse_unit_list_s);
        NEAR(last);
        TAILQ_REMOVE(&s->units, last, next);
      }
    } break;

    case ava_pcmt_behead: {
      const ava_pcm_behead* h = (const ava_pcm_behead*)instr;
      ava_parse_statement* s;
      ava_parse_unit* first;
      ava_integer count;

      TOS_STATEMENT(s);
      for (count = h->count; count; --count) {
        first = TAILQ_FIRST(&s->units);
        MISSING_ARG_UNLESS(first);
        NEAR(first);
        TAILQ_REMOVE(&s->units, first, next);
      }
    } break;

    case ava_pcmt_tail: {
      const ava_pcm_tail* h = (const ava_pcm_tail*)instr;
      ava_parse_statement* s;
      ava_parse_unit* first;
      ava_integer count, length;

      TOS_STATEMENT(s);
      NEAR(TAILQ_FIRST(&s->units));

      length = ava_intr_user_macro_statement_length(s);
      count = length - h->count;
      MISSING_ARG_UNLESS(count >= 0);

      while (count--) {
        first = TAILQ_FIRST(&s->units);
        NEAR(first);
        TAILQ_REMOVE(&s->units, first, next);
      }
    } break;

    case ava_pcmt_curtail: {
      const ava_pcm_curtail* h = (const ava_pcm_curtail*)instr;
      ava_parse_statement* s;
      ava_parse_unit* last;
      ava_integer count;

      TOS_STATEMENT(s);
      for (count = h->count; count; --count) {
        last = TAILQ_LAST(&s->units, ava_parse_unit_list_s);
        MISSING_ARG_UNLESS(last);
        NEAR(last);
        TAILQ_REMOVE(&s->units, last, next);
      }
    } break;

    case ava_pcmt_nonempty: {
      ava_parse_statement* s;

      TOS_STATEMENT(s);
      MISSING_ARG_UNLESS(!TAILQ_EMPTY(&s->units));
    } break;

    case ava_pcmt_singular: {
      ava_parse_statement* s;
      ava_parse_unit* unit;

      TOS_STATEMENT(s); POP();
      unit = TAILQ_FIRST(&s->units);
      MISSING_ARG_UNLESS(unit);
      /* The presence of more than one unit implies a bug in the P-Code. */
      if (TAILQ_NEXT(TAILQ_FIRST(&s->units), next))
        DIE("Singular statement contains more than one unit.");
      PUSH_UNIT(unit);
    } break;

    case ava_pcmt_append: {
      stack_entry* dst, * src;
      ava_parse_unit* unit, * nunit;

      TOS(src); POP();
      TOS(dst);

      if (set_statement == dst->type) {
        if (set_statement == src->type) {
          /* Concatenate */
          TAILQ_FOREACH_SAFE(unit, &src->v.statement->units, next, nunit) {
            TAILQ_REMOVE(&src->v.statement->units, unit, next);
            TAILQ_INSERT_TAIL(&dst->v.statement->units, unit, next);
          }
        } else {
          /* Add to statement */
          TAILQ_INSERT_TAIL(&dst->v.statement->units, src->v.unit, next);
        }
      } else {
        switch (dst->v.unit->type) {
        case ava_put_block:
        case ava_put_substitution:
          if (set_statement != src->type)
            DIE("Attempt to append unit to block or substitution.");
          TAILQ_INSERT_TAIL(&dst->v.unit->v.statements, src->v.statement, next);
          break;

        case ava_put_semiliteral:
          if (set_statement == src->type) {
            /* Concatenate statement contents into semiliteral */
            TAILQ_FOREACH_SAFE(unit, &src->v.statement->units, next, nunit) {
              TAILQ_REMOVE(&src->v.statement->units, unit, next);
              TAILQ_INSERT_TAIL(&dst->v.unit->v.units, unit, next);
            }
          } else {
            /* Append to semiliteral */
            TAILQ_INSERT_TAIL(&dst->v.unit->v.units, src->v.unit, next);
          }
          break;

        default:
          DIE("Attempt to append to non-container.");
        }
      }
    } break;

    case ava_pcmt_gensym: {
      const ava_pcm_gensym* g = (const ava_pcm_gensym*)instr;
      PUSH_STRINGOID(ava_put_bareword, ava_macsub_gensym(context, g->value));
    } break;

    case ava_pcmt_bareword: {
      const ava_pcm_bareword* b = (const ava_pcm_bareword*)instr;
      PUSH_STRINGOID(ava_put_bareword, b->value);
    } break;

    case ava_pcmt_expander: {
      const ava_pcm_expander* b = (const ava_pcm_expander*)instr;
      PUSH_STRINGOID(ava_put_expander, b->value);
    } break;

    case ava_pcmt_astring: {
      const ava_pcm_astring* s = (const ava_pcm_astring*)instr;
      PUSH_STRINGOID(ava_put_astring, s->value);
    } break;

    case ava_pcmt_lstring: {
      const ava_pcm_lstring* s = (const ava_pcm_lstring*)instr;
      PUSH_STRINGOID(ava_put_lstring, s->value);
    } break;

    case ava_pcmt_rstring: {
      const ava_pcm_rstring* s = (const ava_pcm_rstring*)instr;
      PUSH_STRINGOID(ava_put_rstring, s->value);
    } break;

    case ava_pcmt_lrstring: {
      const ava_pcm_lrstring* s = (const ava_pcm_lrstring*)instr;
      PUSH_STRINGOID(ava_put_lrstring, s->value);
    } break;

    case ava_pcmt_verbatim: {
      const ava_pcm_verbatim* s = (const ava_pcm_verbatim*)instr;
      PUSH_STRINGOID(ava_put_verbatim, s->value);
    } break;

    case ava_pcmt_subst:
    case ava_pcmt_block: {
      ava_parse_unit* block = AVA_NEW(ava_parse_unit);
      block->type = (ava_pcmt_block == instr->type?
                     ava_put_block : ava_put_substitution);
      block->location = provoker->location;
      TAILQ_INIT(&block->v.statements);
      PUSH_UNIT(block);
    } break;

    case ava_pcmt_semilit: {
      ava_parse_unit* semilit = AVA_NEW(ava_parse_unit);
      semilit->type = ava_put_semiliteral;
      semilit->location = provoker->location;
      TAILQ_INIT(&semilit->v.units);
      PUSH_UNIT(semilit);
    } break;

    case ava_pcmt_statement: {
      ava_parse_statement* s = AVA_NEW(ava_parse_statement);
      TAILQ_INIT(&s->units);
      PUSH_STATEMENT(s);
    } break;

    case ava_pcmt_spread: {
      ava_parse_unit* nested, * spread;

      TOS_UNIT(nested); POP();
      spread = AVA_NEW(ava_parse_unit);
      spread->type = ava_put_spread;
      spread->location = provoker->location;
      spread->v.unit = nested;
      PUSH_UNIT(spread);
    } break;

    case ava_pcmt_die: {
      return ava_macsub_error_result(
        context, ava_error_use_of_invalid_macro(
          &provoker->location, self->full_name));
    } break;
    }
  }

  TOS_STATEMENT(statement); POP();
  if (!SLIST_EMPTY(&stack))
    DIE("Execution terminated with more than one element on stack.");

  if (TAILQ_EMPTY(&statement->units))
    return ava_macsub_error_result(
      context, ava_error_macro_expanded_to_nothing(
        &provoker->location, self->full_name));

  return (ava_macro_subst_result) {
    .status = ava_mss_again,
    .v = { .statement = statement },
  };
}
