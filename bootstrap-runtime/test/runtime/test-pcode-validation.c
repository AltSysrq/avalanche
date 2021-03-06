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
#include "test.c"

#include <string.h>

#define AVA__INTERNAL_INCLUDE 1
#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/string.h"
#include "runtime/avalanche/list.h"
#include "runtime/avalanche/map.h"
#include "runtime/avalanche/pcode.h"
#include "runtime/avalanche/errors.h"
#include "runtime/avalanche/pcode-validation.h"

/*
  Tests for low-level aspects of the P-Code to X-Code transformation.

  Things which can be adequately tested by compiling Avalanche code to P-Code
  are generally not included here; the main purpose of these tests is to test
  situations that should never occur when the P-Code generator functions
  correctly.
 */

defsuite(pcode_validation);

static ava_xcode_global_list* make_xcode(
  const char* pcode_str,
  ava_compile_error_list* errors
) {
  ava_pcode_global_list* pcode = ava_pcode_global_list_of_string(
    ava_string_of_cstring(pcode_str));
  return ava_xcode_from_pcode(pcode, errors, ava_empty_map());
}

static ava_xcode_global_list* make_xcode_ok(const char* pcode_str) {
  ava_compile_error_list errors;
  ava_xcode_global_list* ret;

  TAILQ_INIT(&errors);
  ret = make_xcode(pcode_str, &errors);
  if (!TAILQ_EMPTY(&errors)) {
    ck_abort_msg("P-Code rejected unexpectly.\n%s",
                 ava_string_to_cstring(
                   ava_error_list_to_string(&errors, 50, ava_false)));
  }
  ck_assert_ptr_ne(NULL, ret);

  return ret;
}

static ava_xcode_function* make_xcode_fun(const char* pcode_str) {
  ava_xcode_global_list* xc;

  xc = make_xcode_ok(pcode_str);
  ck_assert_int_eq(1, xc->length);
  ck_assert_ptr_ne(NULL, xc->elts[0].fun);
  return xc->elts[0].fun;
}

static void xcode_fail_with(
  const char* message,
  const char* pcode_str
) {
  ava_pcode_global_list* pcode;
  ava_compile_error_list errors;
  const char* text;

  TAILQ_INIT(&errors);

  pcode = ava_pcode_global_list_of_string(
    ava_string_of_cstring(pcode_str));
  (void)ava_xcode_from_pcode(pcode, &errors, ava_empty_map());

  if (TAILQ_EMPTY(&errors)) {
    ck_abort_msg("P-Code unexpectedly accepted.");
  } else {
    text = ava_string_to_cstring(
      ava_error_list_to_string(&errors, 50, ava_false));
    if (!strstr(text, message))
      ck_abort_msg(
        "P-Code rejected, but expected message \"%s\" not found.\n%s",
        message, text);
  }
}

#define VERB(...) " \\{" __VA_ARGS__ "\\} "
#define FUN_FOO "fun false \"ava foo\" "
#define ONE_ARG " \"ava pos\" "
#define NO_VAR " \\{\\{\\}\\} "
#define INSTR(type,blkix,iix)                           \
  ((const ava_pcx_##type*)assert_type_is(               \
    fun->blocks[blkix]->elts[iix], ava_pcxt_##type))

static const ava_pcode_exe* assert_type_is(
  const ava_pcode_exe* instr,
  ava_pcode_exe_type type
) {
  ck_assert_int_eq(instr->type, type);
  return instr;
}

deftest(trivial_function) {
  ava_xcode_function* fun;

  fun = make_xcode_fun(
    VERB(FUN_FOO ONE_ARG NO_VAR VERB()));
  ck_assert_int_eq(0, fun->num_blocks);
}

deftest(identity_function) {
  ava_xcode_function* fun;

  fun = make_xcode_fun(
    VERB(FUN_FOO ONE_ARG VERB("x") VERB(
           VERB("ret v0"))));
  ck_assert_int_eq(1, fun->num_blocks);
  ck_assert_ptr_ne(NULL, fun->blocks[0]);
  ck_assert_int_eq(1, fun->blocks[0]->length);
  ck_assert_int_eq(ava_pcxt_ret, fun->blocks[0]->elts[0]->type);
}

deftest(simple_reg_rename) {
  ava_xcode_function* fun;

  fun = make_xcode_fun(
    VERB(FUN_FOO ONE_ARG VERB("x") VERB(
           VERB("push d 1")             /* 0,0 */
           VERB("push i 1")
           VERB("push l 1")
           VERB("push p 1")
           VERB("push f 1")
           VERB("ld-reg-s d0 v0")       /* 0,5 */
           VERB("ld-reg-d i0 d0")       /* 1,0 ! */
           VERB("ld-reg-d l0 d0")       /* 2,0 ! */
           VERB("ld-parm p0 d0 false")  /* 2,1 */
           VERB("ld-reg-d f0 d0")       /* 3,0 ! */
           VERB("invoke-dd d0 f0 0 1")  /* 4,0 ! */
           VERB("ret d0")
           VERB("pop f 1")
           VERB("pop p 1")
           VERB("pop l 1")
           VERB("pop i 1")              /* 4,5 */
           VERB("pop d 1"))));

  ck_assert_int_eq(0, INSTR(ld_reg_s, 0, 5)->src.index);
  ck_assert_int_eq(1, INSTR(ld_reg_s, 0, 5)->dst.index);
  ck_assert_int_eq(1, INSTR(ld_reg_d, 1, 0)->src.index);
  ck_assert_int_eq(2, INSTR(ld_reg_d, 1, 0)->dst.index);
  ck_assert_int_eq(1, INSTR(ld_reg_d, 2, 0)->src.index);
  ck_assert_int_eq(3, INSTR(ld_reg_d, 2, 0)->dst.index);
  ck_assert_int_eq(1, INSTR(ld_parm, 2, 1)->src.index);
  ck_assert_int_eq(4, INSTR(ld_parm, 2, 1)->dst.index);
  ck_assert_int_eq(1, INSTR(ld_reg_d, 3, 0)->src.index);
  ck_assert_int_eq(5, INSTR(ld_reg_d, 3, 0)->dst.index);
  ck_assert_int_eq(5, INSTR(invoke_dd, 4, 0)->fun.index);
  ck_assert_int_eq(4, INSTR(invoke_dd, 4, 0)->base);
  ck_assert_int_eq(1, INSTR(invoke_dd, 4, 0)->dst.index);
  ck_assert_int_eq(1, INSTR(ret, 4, 1)->return_value.index);
}

deftest(sectioned_reg_rename) {
  ava_xcode_function* fun;

  fun = make_xcode_fun(
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push d 1")
           VERB("ld-imm-vd d0 foo")
           VERB("pop d 1")
           VERB("push d 1")
           VERB("ld-imm-vd d0 bar")
           VERB("pop d 1"))));

  ck_assert_int_eq(1, INSTR(ld_imm_vd, 0, 1)->dst.index);
  ck_assert_int_eq(2, INSTR(ld_imm_vd, 0, 4)->dst.index);
}

deftest(loop_initialisation) {
  (void)make_xcode_fun(
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push i 1")
           VERB("ld-imm-i i0 42")
           VERB("label 1")
           VERB("branch i0 42 false 1")
           VERB("pop i 1"))));
}

deftest(dupe_label) {
  xcode_fail_with(
    "X9000",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("label 1")
           VERB("label 1"))));
}

deftest(pop_underflow) {
  xcode_fail_with(
    "X9001",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("pop d 1"))));
}

deftest(reg_nxread) {
  xcode_fail_with(
    "X9002",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push d 1")
           VERB("pop d 1")
           VERB("ret d0"))));
}

deftest(reg_nxwrite) {
  xcode_fail_with(
    "X9002",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push d 1")
           VERB("pop d 1")
           VERB("ld-imm-vd d0 foo"))));
}

deftest(reg_nxdrange) {
  xcode_fail_with(
    "X9002",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push d 1")
           VERB("invoke-ss d0 0 0 2")
           VERB("pop d 1"))));
}

deftest(reg_nxprange) {
  xcode_fail_with(
    "X9002",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push p 1")
           VERB("push d 1")
           VERB("invoke-sd d0 0 0 2")
           VERB("pop d 1")
           VERB("pop p 1"))));
}

deftest(jump_nxlabel) {
  xcode_fail_with(
    "X9003",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("goto 0")
           VERB("label 1"))));
}

deftest(local_uninit_reg) {
  xcode_fail_with(
    "X9004",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push d 1")
           VERB("ld-reg-s d0 d0")
           VERB("pop d 1"))));
}

deftest(block_fallthrough_uninit_reg) {
  xcode_fail_with(
    "X9004",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push d 1")
           VERB("goto 1")
           VERB("label 1")
           VERB("ld-reg-s d0 d0")
           VERB("pop d 1"))));
}

deftest(maybe_uninit_reg) {
  xcode_fail_with(
    "X9004",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push i 2")
           VERB("ld-imm-i i0 42")
           VERB("branch i0 42 false 1")
           VERB("ld-imm-i i1 0")
           VERB("label 1")
           VERB("ld-reg-s i0 i1")
           VERB("pop i 2"))));
}

deftest(uninit_var) {
  xcode_fail_with(
    "X9005",
    VERB(FUN_FOO ONE_ARG VERB("foo bar") VERB(
           VERB("ret v1"))));
}

deftest(missing_pop) {
  xcode_fail_with(
    "X9006",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push d 1"))));
}

deftest(global_global_fun_oob_ref) {
  xcode_fail_with(
    "X9007",
    VERB("init 42"));
}

deftest(global_global_entity_oob_ref) {
  xcode_fail_with(
    "X9007",
    VERB("export 42 true foo"));
}

deftest(global_global_fun_nonfun_ref) {
  xcode_fail_with(
    "X9008",
    VERB("init 0"));
}

deftest(global_global_entity_nonentity_ref) {
  xcode_fail_with(
    "X9008",
    VERB("export 0 true foo"));
}

deftest(global_init_extfun_ref) {
  xcode_fail_with(
    "X9008",
    VERB("ext-fun" VERB("ava foo") VERB("ava pos"))
    VERB("init 0"));
}

deftest(global_bss_oob_ref) {
  xcode_fail_with(
    "X9007",
    VERB("S-bss 99 true [ava foo] false"));
}

deftest(global_bss_ref_non_sxt) {
  xcode_fail_with(
    "X9008",
    VERB("S-bss 0 true [ava foo] false"));
}

deftest(global_bss_t_ref_non_tail) {
  xcode_fail_with(
    "X9008",
    VERB("S-bss-t 1 true [ava foo] false 42")
    VERB("decl-sxt true [[struct foo] [value x]]"));
}

deftest(global_bss_t_ref_empty) {
  xcode_fail_with(
    "X9008",
    VERB("S-bss-t 1 true [ava foo] false 42")
    VERB("decl-sxt true [[struct foo]]"));
}

deftest(global_init_bad_arg_count) {
  xcode_fail_with(
    "X9008",
    VERB(FUN_FOO VERB("ava pos pos") VERB("foo bar") VERB())
    VERB("init 0"));
}

deftest(local_global_var_oob_ref) {
  xcode_fail_with(
    "X9007",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("set-glob 42 v0"))));
}

deftest(local_global_fun_oob_ref) {
  xcode_fail_with(
    "X9007",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push d 1")
           VERB("ld-imm-vd d0 foo")
           VERB("invoke-ss d0 42 0 1")
           VERB("pop d 1"))));
}

deftest(local_global_var_nonvar_ref) {
  xcode_fail_with(
    "X9008",
    VERB("init 1")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("set-glob 0 v0"))));
}

deftest(set_glob_on_ext_var) {
  xcode_fail_with(
    "X9008",
    VERB("ext-var [ava some-var]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("set-glob 0 v0"))));
}

deftest(local_global_fun_nonfun_ref) {
  xcode_fail_with(
    "X9008",
    VERB("init 1")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push d 1")
           VERB("ld-imm-vd d0 foo")
           VERB("invoke-ss d0 0 0 1")
           VERB("pop d 1"))));
}

deftest(invoke_ss_with_wrong_arg_count) {
  xcode_fail_with(
    "X9009",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push d 2")
           VERB("ld-imm-vd d0 foo")
           VERB("ld-imm-vd d1 bar")
           VERB("invoke-ss d0 0 0 2")
           VERB("pop d 2"))));
}

deftest(try_nxlabel) {
  xcode_fail_with(
    "X9003",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("try true 99")
           VERB("yrt"))));
}

deftest(unclosed_try_at_ret) {
  xcode_fail_with(
    "X9015",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("try true 1")
           VERB("ret v0")
           VERB("label 1"))));
}

deftest(unclosed_try_at_fall_off) {
  xcode_fail_with(
    "X9015",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("try true 1")
           VERB("goto 2")
           VERB("label 1")
           VERB("yrt")
           VERB("ret v0")
           VERB("label 2"))));
}

deftest(yrt_underflow) {
  xcode_fail_with(
    "X9014",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("yrt"))));
}

deftest(rethrow_without_exception) {
  xcode_fail_with(
    "X9016",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("try true 1")
           VERB("rethrow")
           VERB("yrt")
           VERB("ret v0")
           VERB("label 1")
           VERB("yrt"))));
}

deftest(exception_conflict_lp_vs_ce) {
  xcode_fail_with(
    "X9013",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("try true 1")
           VERB("label 1")
           VERB("ret v0"))));
}

deftest(exception_conflict_sibling_tries_same_lp) {
  xcode_fail_with(
    "X9013",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("try true 1")
           VERB("yrt")
           VERB("try true 1")
           VERB("yrt")
           VERB("ret v0")
           VERB("label 1")
           VERB("yrt"))));
}

deftest(exception_conflict_nested_tries_same_lp) {
  xcode_fail_with(
    "X9013",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("try true 1")
           VERB("try true 1")
           VERB("yrt")
           VERB("yrt")
           VERB("ret v0")
           VERB("label 1")
           VERB("yrt"))));
}

deftest(exception_conflict_colliding_tries) {
  xcode_fail_with(
    "X9013",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push i 1")
           VERB("ld-reg-d i0 v0")
           VERB("branch i0 0 false 1")
           VERB("try true 2")
           VERB("goto 3")
           VERB("label 1")
           VERB("try true 4")
           VERB("goto 3")
           VERB("label 3")
           VERB("yrt")
           VERB("ret v0")
           VERB("label 2")
           VERB("yrt")
           VERB("ret v0")
           VERB("label 4")
           VERB("yrt")
           VERB("ret v0")
           VERB("pop i 1"))));
}

deftest(exception_conflict_infinite_try) {
  xcode_fail_with(
    "X9013",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("label 0")
           VERB("try true 1")
           VERB("goto 0")
           VERB("label 1")
           VERB("yrt"))));
}

deftest(exception_conflict_infinite_catch) {
  xcode_fail_with(
    "X9013",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("label 0")
           VERB("try true 1")
           VERB("yrt")
           VERB("ret v0")
           VERB("label 1")
           VERB("goto 0"))));
}

deftest(accepts_try_join) {
  (void)make_xcode_fun(
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push i 1")
           VERB("ld-reg-d i0 v0")
           VERB("branch i0 0 false 1")
           VERB("try true 2")
           VERB("yrt")
           VERB("goto 3")
           VERB("label 2")
           VERB("yrt")
           VERB("goto 3")
           VERB("label 1")
           VERB("try true 4")
           VERB("yrt")
           VERB("goto 3")
           VERB("label 4")
           VERB("yrt")
           VERB("label 3")
           VERB("pop i 1")
           VERB("ret v0"))));
}

deftest(try_not_phi_to_catch) {
  (void)make_xcode_fun(
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push d 1")
           VERB("try true 1")
           VERB("ld-imm-vd d0 foo")
           VERB("push i 1")
           VERB("ld-reg-d i0 d0")
           VERB("yrt")
           VERB("goto 2")
           VERB("label 1")
           VERB("yrt")
           VERB("label 2")
           VERB("ret d0")
           VERB("pop i 1")
           VERB("pop d 1"))));
}

deftest(landing_pad_jump_over_init_use_after_yrt) {
  xcode_fail_with(
    "X9004",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push i 1")
           VERB("try true 1")
           VERB("ld-reg-d i0 v0")
           VERB("yrt")
           VERB("ret v0")
           VERB("label 1")
           VERB("yrt")
           VERB("ld-reg-u v0 i0")
           VERB("ret v0")
           VERB("pop i 1"))));
}

deftest(reg_init_in_try_and_lp) {
  (void)make_xcode_fun(
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push i 1")
           VERB("try true 1")
           VERB("ld-reg-d i0 v0")
           VERB("yrt")
           VERB("goto 2")
           VERB("label 1")
           VERB("ld-imm-i i0 42")
           VERB("yrt")
           VERB("label 2")
           VERB("ld-reg-u v0 i0")
           VERB("ret v0")
           VERB("pop i 1"))));
}

deftest(yrt_at_end_of_function) {
  (void)make_xcode_fun(
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("try true 1")
           VERB("yrt")
           VERB("ret v0")
           VERB("label 1")
           VERB("yrt"))));
}

deftest(negative_struct_ref) {
  xcode_fail_with(
    "X9007",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("S-new-s v0 -1 true"))));
}

deftest(oob_struct_ref) {
  xcode_fail_with(
    "X9007",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("S-new-s v0 99 true"))));
}

deftest(struct_ref_to_non_struct) {
  xcode_fail_with(
    "X9008",
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("S-new-s v0 0 true"))));
}

#define STRUCT_FOO(body) VERB("decl-sxt true [[struct foo] " body "]")

deftest(tail_ref_to_struct_with_no_fields) {
  xcode_fail_with(
    "X9008",
    STRUCT_FOO("")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push i 1")
           VERB("ld-imm-i i0 0")
           VERB("S-new-st v0 0 i0 true")
           VERB("pop i 1"))));
}

deftest(tail_ref_to_struct_with_non_tail) {
  xcode_fail_with(
    "X9008",
    STRUCT_FOO("[value v]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push i 1")
           VERB("ld-imm-i i0 0")
           VERB("S-new-st v0 0 i0 true")
           VERB("pop i 1"))));
}

deftest(tail_ref_to_struct_with_tail) {
  (void)make_xcode_ok(
    STRUCT_FOO("[tail [[struct bar]] t]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push i 1")
           VERB("ld-imm-i i0 0")
           VERB("S-new-st v0 0 i0 true")
           VERB("pop i 1"))));
}

deftest(negative_struct_field_ref) {
  xcode_fail_with(
    "X9017",
    STRUCT_FOO("[value v]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("S-v-st v0 0 -1 v0 false"))));
}

deftest(oob_struct_field_ref) {
  xcode_fail_with(
    "X9017",
    STRUCT_FOO("[value v]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("S-v-st v0 0 99 v0 false"))));
}

deftest(int_ref_to_non_int_struct_field) {
  xcode_fail_with(
    "X9018",
    STRUCT_FOO("[value v]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push i 1")
           VERB("S-i-ld i0 v0 0 0 false")
           VERB("pop i 1"))));
}

deftest(real_ref_to_non_real_struct_field) {
  xcode_fail_with(
    "X9018",
    STRUCT_FOO("[value v]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("S-r-ld v0 v0 0 0 false"))));
}

deftest(value_ref_to_non_value_struct_field) {
  xcode_fail_with(
    "X9018",
    STRUCT_FOO("[hybrid FILE* v]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("S-v-ld v0 v0 0 0 false"))));
}

deftest(ph_ref_to_non_ph_struct_field) {
  xcode_fail_with(
    "X9018",
    STRUCT_FOO("[value v]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("S-p-ld v0 v0 0 0 false"))));
}

deftest(ph_ref_to_pointer_struct_field) {
  (void)make_xcode_ok(
    STRUCT_FOO("[ptr FILE* true v]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("S-p-ld v0 v0 0 0 false"))));
}

deftest(ph_ref_to_hybrid_struct_field) {
  (void)make_xcode_ok(
    STRUCT_FOO("[hybrid FILE* v]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("S-p-ld v0 v0 0 0 false"))));
}

deftest(hybrid_ref_to_non_hybrid_struct_field) {
  xcode_fail_with(
    "X9018",
    STRUCT_FOO("[ptr FILE* false v]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push i 1")
           VERB("S-hy-intp i0 v0 0 0 false")
           VERB("pop i 1"))));
}

deftest(composite_ref_to_noncomposite_struct_field) {
  xcode_fail_with(
    "X9018",
    STRUCT_FOO("[value v]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("S-gfp v0 v0 0 0"))));
}

deftest(composite_ref_to_compose_struct_field) {
  (void)make_xcode_ok(
    STRUCT_FOO("[compose [[struct bar]] v]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("S-gfp v0 v0 0 0"))));
}

deftest(composite_ref_to_array_struct_field) {
  (void)make_xcode_ok(
    STRUCT_FOO("[array [[struct bar]] 1 v]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("S-gfp v0 v0 0 0"))));
}

deftest(composite_ref_to_tail_struct_field) {
  (void)make_xcode_ok(
    STRUCT_FOO("[tail [[struct bar]] v]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("S-gfp v0 v0 0 0"))));
}

deftest(atomic_int_ref_to_non_int_struct_field) {
  xcode_fail_with(
    "X9018",
    STRUCT_FOO("[value v]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push i 1")
           VERB("S-ia-ld i0 v0 0 0 true seqcst")
           VERB("pop i 1"))));
}

deftest(atomic_int_ref_to_nonatomic_int_struct_field) {
  xcode_fail_with(
    "X9018",
    STRUCT_FOO("[int word true false 15 native v]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push i 1")
           VERB("S-ia-ld i0 v0 0 0 true seqcst")
           VERB("pop i 1"))));
}

deftest(atomic_int_ref_to_atomic_int_struct_field) {
  (void)make_xcode_ok(
    STRUCT_FOO("[int word true true 15 native v]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("push i 1")
           VERB("S-ia-ld i0 v0 0 0 true seqcst")
           VERB("pop i 1"))));
}

deftest(atomic_ptr_ref_to_nonptr_struct_field) {
  xcode_fail_with(
    "X9018",
    STRUCT_FOO("[hybrid FILE* v]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("S-pa-ld v0 v0 0 0 true seqcst"))));
}

deftest(atomic_ptr_ref_to_nonatomic_ptr_struct_field) {
  xcode_fail_with(
    "X9018",
    STRUCT_FOO("[ptr FILE* false v]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("S-pa-ld v0 v0 0 0 true seqcst"))));
}


deftest(atomic_ptr_ref_to_atomic_ptr_struct_field) {
  (void)make_xcode_ok(
    STRUCT_FOO("[ptr FILE* true v]")
    VERB(FUN_FOO ONE_ARG NO_VAR VERB(
           VERB("S-pa-ld v0 v0 0 0 true seqcst"))));
}
