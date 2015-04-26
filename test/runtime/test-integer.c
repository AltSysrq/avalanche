/*-
 * Copyright (c) 2015 Jason Lingle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.c"

#include <stdlib.h>

#define AVA__INTERNAL_INCLUDE 1
#include "runtime/avalanche/integer.h"

defsuite(integer);

static const char* int_to_str(ava_integer i) {
  return ava_string_to_cstring(
    ava_to_string(
      ava_value_of_integer(i)));
}

static ava_integer str_to_int(const char* s, ava_integer d) {
  return ava_integer_of_value(
    ava_value_of_string(
      ava_string_of_cstring(s)), d);
}

static int str_is_int(const char* s) {
  return ava_string_is_integer(ava_string_of_cstring(s));
}

deftest(integer_zero_to_string) {
  ck_assert_str_eq("0", int_to_str(0));
}

deftest(integer_positive_one_to_string) {
  ck_assert_str_eq("1", int_to_str(1));
}

deftest(integer_negative_one_to_string) {
  ck_assert_str_eq("-1", int_to_str(-1));
}

deftest(integer_max_to_string) {
  ck_assert_str_eq("9223372036854775807",
                   int_to_str(0x7FFFFFFFFFFFFFFFLL));
}

deftest(integer_min_to_string) {
  ck_assert_str_eq("-9223372036854775808",
                   int_to_str(-0x8000000000000000LL));
}

deftest(weighs_nothing) {
  ck_assert_int_eq(0, ava_value_weight(ava_value_of_integer(5)));
}

deftest(empty_string_to_default_integer) {
  ck_assert_int_eq(42LL, str_to_int("", 42));
}

deftest(whitespace_string_to_default_integer) {
  ck_assert_int_eq(42LL, str_to_int("  \t\r\n", 42));
}

deftest(decimal_zero_to_integer) {
  ck_assert_int_eq(0LL, str_to_int("0", 42));
}

deftest(decimal_one_to_integer) {
  ck_assert_int_eq(1LL, str_to_int("1", 42));
}

deftest(decimal_one_with_leading_plus_to_integer) {
  ck_assert_int_eq(1LL, str_to_int("+1", 42));
}

deftest(decimal_negative_one_to_integer) {
  ck_assert_int_eq(-1LL, str_to_int("-1", 42));
}

deftest(decimal_max_to_integer) {
  ck_assert_int_eq(9223372036854775807LL,
                   str_to_int("9223372036854775807", 42));
}

deftest(decimal_min_to_integer) {
  /* GCC thinks -9223372036854775808 is less than the minimum value of signed
   * long long and complains about interpreting it unsigned. Just give it
   * unsigned for now and cast it.
   */
  ck_assert_int_eq((ava_integer)9223372036854775808ULL,
                   str_to_int("-9223372036854775808", 42));
}

deftest(decimal_unsigned_max_to_integer) {
  ck_assert_int_eq((ava_integer)0xFFFFFFFFFFFFFFFFULL,
                   str_to_int("18446744073709551615", 42));
}

deftest(decimal_to_integer_overflow_by_one) {
  ava_exception_handler handler;
  ava_try (handler) {
    str_to_int("18446744073709551616", 42);
    ck_abort_msg("no exception thrown");
  } ava_catch (handler, ava_format_exception) {
    /* ok */
  } ava_catch_all {
    ava_rethrow(&handler);
  }
}

deftest(decimal_to_integer_overflow_by_ten) {
  ava_exception_handler handler;
  ava_try (handler) {
    str_to_int("18446744073709551625", 42);
    ck_abort_msg("no exception thrown");
  } ava_catch (handler, ava_format_exception) {
    /* ok */
  } ava_catch_all {
    ava_rethrow(&handler);
  }
}

deftest(binary_zero_to_integer) {
  ck_assert_int_eq(0, str_to_int("b0", 64));
}

deftest(binary_one_to_integer) {
  ck_assert_int_eq(1, str_to_int("b1", 42));
}

deftest(binary_one_with_leading_plus_to_integer) {
  ck_assert_int_eq(1, str_to_int("+b1", 42));
}

deftest(binary_one_with_zero_prefix_to_integer) {
  ck_assert_int_eq(1, str_to_int("0b1", 42));
}

deftest(binary_one_with_capital_radix_to_integer) {
  ck_assert_int_eq(1, str_to_int("B1", 42));
}

deftest(binary_one_with_zero_prefix_and_capital_radix_to_integer) {
  ck_assert_int_eq(1, str_to_int("0B1", 42));
}

deftest(binary_nagitve_one_to_integer) {
  ck_assert_int_eq(-1LL, str_to_int("-b1", 42));
}

deftest(binary_fourty_two_to_integer) {
  ck_assert_int_eq(42, str_to_int("b101010", 64));
}

deftest(binary_max_to_integer) {
  ck_assert_int_eq(
    0x7FFFFFFFFFFFFFFFLL,
    str_to_int(
      "b0111111111111111111111111111111111111111111111111111111111111111",
      42));
}

deftest(binary_min_to_integer) {
  ck_assert_int_eq(
    -0x8000000000000000LL,
    str_to_int(
      "b1000000000000000000000000000000000000000000000000000000000000000",
      42));
}

deftest(binary_unsigned_max_to_integer) {
  ck_assert_int_eq(
    0xFFFFFFFFFFFFFFFFLL,
    str_to_int(
      "b1111111111111111111111111111111111111111111111111111111111111111",
      42));
}

deftest(octal_zero_to_integer) {
  ck_assert_int_eq(0, str_to_int("o0", 42));
}

deftest(octal_one_to_integer) {
  ck_assert_int_eq(1, str_to_int("o1", 42));
}

deftest(octal_one_with_zero_prefix_to_integer) {
  ck_assert_int_eq(1, str_to_int("0o1", 42));
}

deftest(octal_one_with_capital_radix_to_integer) {
  ck_assert_int_eq(1, str_to_int("O1", 42));
}

deftest(octal_one_with_zero_prefix_and_capital_radix_to_integer) {
  /* If anyone ever actually *does* this.. */
  ck_assert_int_eq(1, str_to_int("0O1", 42));
}

deftest(octal_one_with_leading_plus_to_integer) {
  ck_assert_int_eq(1, str_to_int("+o1", 42));
}

deftest(octal_negative_one_to_integer) {
  ck_assert_int_eq(-1, str_to_int("-o1", 42));
}

deftest(octal_fourty_two_to_integer) {
  ck_assert_int_eq(042, str_to_int("o42", 0));
}

deftest(octal_max_to_integer) {
  ck_assert_int_eq(0x7FFFFFFFFFFFFFFFLL,
                   str_to_int("o777777777777777777777", 42));
}

deftest(octal_min_to_integer) {
  ck_assert_int_eq(0x8000000000000000LL,
                   str_to_int("-o1000000000000000000000", 42));
}

deftest(octal_unsigned_max_to_integer) {
  ck_assert_int_eq(0xFFFFFFFFFFFFFFFFLL,
                   str_to_int("o1777777777777777777777", 42));
}

deftest(octal_overflow) {
  ava_exception_handler handler;
  ava_try (handler) {
    str_to_int("o2000000000000000000000", 42);
    ck_abort_msg("no nexception thrown");
  } ava_catch (handler, ava_format_exception) {
    /* ok */
  } ava_catch_all {
    ava_rethrow(&handler);
  }
}

deftest(hex_zero_to_integer) {
  ck_assert_int_eq(0, str_to_int("x0", 42));
}

deftest(hex_one_to_integer) {
  ck_assert_int_eq(1, str_to_int("x1", 42));
}

deftest(hex_one_with_zero_prefix_to_integer) {
  ck_assert_int_eq(1, str_to_int("0x1", 42));
}

deftest(hex_one_with_capital_radix_to_integer) {
  ck_assert_int_eq(1, str_to_int("X1", 42));
}

deftest(hex_one_with_zero_prefix_and_captial_radix_to_integer) {
  ck_assert_int_eq(1, str_to_int("0X1", 42));
}

deftest(hex_one_with_leading_plus_to_integer) {
  ck_assert_int_eq(1, str_to_int("+x1", 42));
}

deftest(hex_lowercase_ten_to_integer) {
  ck_assert_int_eq(10, str_to_int("xa", 42));
}

deftest(hex_uppercase_ten_to_integer) {
  ck_assert_int_eq(10, str_to_int("xA", 42));
}

deftest(hex_negative_one_to_integer) {
  ck_assert_int_eq(-1LL, str_to_int("-x1", 42));
}

deftest(hex_fourty_two_to_integer) {
  ck_assert_int_eq(0x42LL, str_to_int("x42", 42));
}

deftest(hex_deadbeef_to_integer) {
  ck_assert_int_eq(0xDEADBEEFLL, str_to_int("xDEADBEEF", 42));
}

deftest(hex_max_to_integer) {
  ck_assert_int_eq(0x7FFFFFFFFFFFFFFFLL,
                   str_to_int("x7FFFFFFFFFFFFFFF", 42));
}

deftest(hex_min_to_integer) {
  ck_assert_int_eq(0x8000000000000000LL,
                   str_to_int("-x8000000000000000", 42));
}

deftest(hex_unsigned_max_to_integer) {
  ck_assert_int_eq(0xFFFFFFFFFFFFFFFFLL,
                   str_to_int("xFFFFFFFFFFFFFFFF", 42));
}

deftest(hex_overflow) {
  ava_exception_handler handler;
  ava_try (handler) {
    str_to_int("x10000000000000000", 42);
    ck_abort_msg("no exception thrown");
  } ava_catch (handler, ava_format_exception) {
    /* ok */
  } ava_catch_all {
    ava_rethrow(&handler);
  }
}

deftest(hex_leading_zeros_dont_overflow) {
  ck_assert_int_eq(
    1,
    str_to_int("x0000000000000000000000000000000000000000000000000000000000001",
               42));
}

deftest(leading_garbage) {
  ava_exception_handler handler;
  ava_try (handler) {
    str_to_int("~ 0", 42);
    ck_abort_msg("no exception thrown");
  } ava_catch (handler, ava_format_exception) {
    /* ok */
  } ava_catch_all {
    ava_rethrow(&handler);
  }
}

deftest(trailing_garbage) {
  ava_exception_handler handler;
  ava_try (handler) {
    str_to_int("0 x", 42);
    ck_abort_msg("no exception thrown");
  } ava_catch (handler, ava_format_exception) {
    /* ok */
  } ava_catch_all {
    ava_rethrow(&handler);
  }
}

deftest(surrounding_whitespace) {
  ck_assert_int_eq(42, str_to_int(" \t\r\n42\n\r\t ", 5));
}

deftest(isolated_radix_mark) {
  ava_exception_handler handler;
  ava_try (handler) {
    str_to_int("x", 42);
    ck_abort_msg("no exception thrown");
  } ava_catch (handler, ava_format_exception) {
    /* ok */
  } ava_catch_all {
    ava_rethrow(&handler);
  }
}

deftest(oversized_string_to_integer) {
  ava_exception_handler handler;
  ava_try (handler) {
    str_to_int(
      "000000000000000000000000000000000000000000000000000000000000000000", 42);
    ck_abort_msg("no exception thrown");
  } ava_catch (handler, ava_format_exception) {
    /* ok */
  } ava_catch_all {
    ava_rethrow(&handler);
  }
}

deftest(truthy_to_integer_one) {
  ck_assert_int_eq(1LL, str_to_int("true", 42));
  ck_assert_int_eq(1LL, str_to_int("tRuE", 42));
  ck_assert_int_eq(1LL, str_to_int("on", 42));
  ck_assert_int_eq(1LL, str_to_int("yes", 42));
}

deftest(falsey_to_integer_zero) {
  ck_assert_int_eq(0LL, str_to_int("false", 42));
  ck_assert_int_eq(0LL, str_to_int("fAlSe", 42));
  ck_assert_int_eq(0LL, str_to_int("off", 42));
  ck_assert_int_eq(0LL, str_to_int("no", 42));
  ck_assert_int_eq(0LL, str_to_int("null", 42));
  ck_assert_int_eq(0LL, str_to_int("NULL", 42));
}

deftest(truthy_is_integer) {
  ck_assert(str_is_int("true"));
  ck_assert(str_is_int("tRuE"));
  ck_assert(str_is_int("on"));
  ck_assert(str_is_int("yes"));
}

deftest(falsey_is_integer) {
  ck_assert(str_is_int("false"));
  ck_assert(str_is_int("fAlSe"));
  ck_assert(str_is_int("off"));
  ck_assert(str_is_int("no"));
  ck_assert(str_is_int("null"));
  ck_assert(str_is_int("NULL"));
}

deftest(empty_string_is_integer) {
  ck_assert(str_is_int(""));
}

deftest(whitespace_string_is_integer) {
  ck_assert(str_is_int(" \t\r\n"));
}

deftest(literals_are_integers) {
  ck_assert(str_is_int("+012349"));
  ck_assert(str_is_int("-b101"));
  ck_assert(str_is_int("+0o123"));
  ck_assert(str_is_int("XDeadBeef"));
}

deftest(invalid_literal_is_not_integer) {
  ck_assert(!str_is_int("0o012345678"));
}

deftest(literal_surrounded_by_whitespace_is_integer) {
  ck_assert(str_is_int(" \r\t\n123\n\t\r "));
}

deftest(oversized_string_is_not_integer) {
  ck_assert(
    !str_is_int(
      "000000000000000000000000000000000000000000000000000000000000000000"));
}
