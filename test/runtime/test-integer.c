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
#include "test.c"

#include <stdlib.h>

#include "runtime/avalanche/exception.h"
#include "runtime/avalanche/integer.h"
#include "runtime/-integer-fast-dec.h"

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

static ava_integer str_to_dec_fast(const char* s) {
  ava_string str = ava_string_of_cstring(s);
  ck_assert(str.ascii9 & 1);
  return ava_integer_parse_dec_fast(str.ascii9, ava_string_length(str));
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

deftest(dec_fast_zero) {
  ck_assert_int_eq(0, str_to_dec_fast("0"));
}

deftest(dec_fast_negative_zero) {
  ck_assert_int_eq(0, str_to_dec_fast("-0"));
}

deftest(dec_fast_negative_one) {
  ck_assert_int_eq(-1LL, str_to_dec_fast("-1"));
}

deftest(dec_fast_all_digits) {
  char str[2];
  unsigned i;

  str[1] = 0;
  for (i = 0; i < 10; ++i) {
    str[0] = i + '0';
    ck_assert_int_eq(i, str_to_dec_fast(str));
  }
}

deftest(dec_fast_two_digit_positive) {
  ck_assert_int_eq(42, str_to_dec_fast("42"));
}

deftest(dec_fast_two_digit_negative) {
  ck_assert_int_eq(-42LL, str_to_dec_fast("-42"));
}

deftest(dec_fast_three_digit_positve) {
  ck_assert_int_eq(123, str_to_dec_fast("123"));
}

deftest(dec_fast_three_digit_negative) {
  ck_assert_int_eq(-123LL, str_to_dec_fast("-123"));
}

deftest(dec_fast_four_digit_positive) {
  ck_assert_int_eq(1234, str_to_dec_fast("1234"));
}

deftest(dec_fast_four_digit_negative) {
  ck_assert_int_eq(-1234LL, str_to_dec_fast("-1234"));
}

deftest(dec_fast_five_digit_positive) {
  ck_assert_int_eq(12345, str_to_dec_fast("12345"));
}

deftest(dec_fast_five_digit_negative) {
  ck_assert_int_eq(-12345LL, str_to_dec_fast("-12345"));
}

deftest(dec_fast_six_digit_positive) {
  ck_assert_int_eq(123456, str_to_dec_fast("123456"));
}

deftest(dec_fast_six_digit_negative) {
  ck_assert_int_eq(-123456LL, str_to_dec_fast("-123456"));
}

deftest(dec_fast_seven_digit_positive) {
  ck_assert_int_eq(1234567, str_to_dec_fast("1234567"));
}

deftest(dec_fast_seven_digit_negative) {
  ck_assert_int_eq(-1234567LL, str_to_dec_fast("-1234567"));
}

deftest(dec_fast_eight_digit_positive) {
  ck_assert_int_eq(12345678, str_to_dec_fast("12345678"));
}

deftest(dec_fast_eight_digit_negative) {
  ck_assert_int_eq(-12345678LL, str_to_dec_fast("-12345678"));
}

deftest(dec_fast_nine_digit_positive) {
  ck_assert_int_eq(123456789LL, str_to_dec_fast("123456789"));
}

/* No nine-digit negative, since that would be 10 chars long */

deftest(dec_fast_max_value) {
  ck_assert_int_eq(999999999LL, str_to_dec_fast("999999999"));
}

deftest(dec_fast_min_value) {
  ck_assert_int_eq(-99999999LL, str_to_dec_fast("-99999999"));
}

deftest(dec_fast_leading_zeroes) {
  ck_assert_int_eq(1, str_to_dec_fast("000000001"));
}

deftest(dec_fast_rejects_isolated_hyphen) {
  ck_assert_int_eq(PARSE_DEC_FAST_ERROR,
                   str_to_dec_fast("-"));
}

deftest(dec_fast_rejects_nondigit_nonhyphen_at_start) {
  char str[3];
  unsigned i;

  str[1] = '0';
  str[2] = 0;

  for (i = 1; i < 128; ++i) {
    if (i != '-' && (i < '0' || i > '9')) {
      str[0] = i;
      ck_assert_int_eq(PARSE_DEC_FAST_ERROR,
                       str_to_dec_fast(str));
    }
  }
}

deftest(dec_fast_rejects_all_nondigits_in_middle) {
  char str[3];
  unsigned i;

  str[0] = '0';
  str[2] = 0;

  for (i = 1; i < 128; ++i) {
    if (i < '0' || i > '9') {
      str[1] = i;
      ck_assert_int_eq(PARSE_DEC_FAST_ERROR,
                       str_to_dec_fast(str));
    }
  }
}

deftest(dec_fast_rejects_repeated_hyphen) {
  ck_assert_int_eq(PARSE_DEC_FAST_ERROR, str_to_dec_fast("--0"));
}
