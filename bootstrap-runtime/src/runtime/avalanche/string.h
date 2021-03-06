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
#ifndef AVA__INTERNAL_INCLUDE
#error "Don't include avalanche/string.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_STRING_H_
#define AVA_RUNTIME_STRING_H_

#include <string.h>

#include "defs.h"

/******************** STRING HANDLING ********************/

/**
 * The required alignment of data wrapped in ava_strings.
 */
#define AVA_STRING_ALIGNMENT 8
/**
 * A variable attribute that forces the variable to be aligned to an
 * AVA_STRING_ALIGNMENT-byte boundary.
 */
#define AVA_STR_ALIGN AVA_ALIGN(AVA_STRING_ALIGNMENT)

/**
 * Type suitable for being passed to ava_string_to_cstring_buff().
 *
 * It is guaranteed to be an array type, and may be initialised with `{0}`. No
 * other properties are guaranteed to client code.
 */
typedef ava_ulong ava_str_tmpbuff[2] AVA_STR_ALIGN;

/**
 * An ASCII string of up to 9 characters packed into a 64-bit integer. The
 * zeroth bit is always 1. Bits 57..63 are the first character, bits 50..56 the
 * second, and so on. The string ends at the 9th character or the first NUL
 * character, whichever occurs first. All bits between the last character and
 * the zeroth bit (exclusive) are zero.
 *
 * This format permits extremely efficient handling of short printable strings,
 * which are extremely common.
 */
typedef ava_ulong ava_ascii9_string;
/**
 * An arbitrary byte string of any number of characters. The exact internal
 * format is unspecified.
 */
typedef struct ava_twine_s ava_twine;

/**
 * The primary Avalanche string type.
 *
 * The encoding of the string can be identified by testing bit 0 of the ascii9
 * field; if it is zero, the string is a twine or absent. If it is 1, the
 * string is an ascii9 string.
 *
 * A string is said to be "absent" if the ascii9 field identifies the string as
 * a twine and the twine field is NULL.
 */
typedef union {
  ava_ascii9_string ascii9;
  const ava_twine*restrict twine;
} ava_string;

#define _AVA_IS_ASCII9_CHAR(ch)                                 \
  (((unsigned char)(ch)) > 0 && ((unsigned char)(ch)) < 128)
#define _AVA_ASCII9_ENCODE_CHAR(ch,ix)                  \
  (((ava_ascii9_string)((ch) & 0x7F)) << (57 - (ix)*7))
/* The somewhat odd-looking index in this macro works around an issue in Clang
 * where, if any errors are found in the compilation unit, it proceeds to
 * produce a warning for *every* *single* character this macro sets to zero if
 * ix is allowed to go beyond the end.
 */
#define _AVA_STRCHR(str,ix) \
  ((ix) < sizeof(str)? (str)[(ix) * ((ix) < sizeof(str))] : 0)
#define _AVA_ASCII9_ENCODE_STRCHR(str,ix)       \
  _AVA_ASCII9_ENCODE_CHAR(_AVA_STRCHR((str), (ix)), (ix))
#define _AVA_ASCII9_ENCODE_STR(str) (           \
    1                                           \
    | _AVA_ASCII9_ENCODE_STRCHR((str), 0)       \
    | _AVA_ASCII9_ENCODE_STRCHR((str), 1)       \
    | _AVA_ASCII9_ENCODE_STRCHR((str), 2)       \
    | _AVA_ASCII9_ENCODE_STRCHR((str), 3)       \
    | _AVA_ASCII9_ENCODE_STRCHR((str), 4)       \
    | _AVA_ASCII9_ENCODE_STRCHR((str), 5)       \
    | _AVA_ASCII9_ENCODE_STRCHR((str), 6)       \
    | _AVA_ASCII9_ENCODE_STRCHR((str), 7)       \
    | _AVA_ASCII9_ENCODE_STRCHR((str), 8)       \
    )
/**
 * Reduces to an integer constant representing the given character sequence as
 * an ASCII9 string.
 *
 * Note that you need to pass each character in individually, since indexing a
 * string literal doesn't count as a constant expression in ISO C.
 *
 * The result is undefined if the string implied is not a legal ASCII9 string.
 */
#define AVA_ASCII9(...) _AVA_ASCII9(__VA_ARGS__, 0, 0, 0, 0, 0, 0, 0, 0, 0)
#define _AVA_ASCII9(a,b,c,d,e,f,g,h,i,...) (    \
    1                                           \
    | _AVA_ASCII9_ENCODE_CHAR((a),0)            \
    | _AVA_ASCII9_ENCODE_CHAR((b),1)            \
    | _AVA_ASCII9_ENCODE_CHAR((c),2)            \
    | _AVA_ASCII9_ENCODE_CHAR((d),3)            \
    | _AVA_ASCII9_ENCODE_CHAR((e),4)            \
    | _AVA_ASCII9_ENCODE_CHAR((f),5)            \
    | _AVA_ASCII9_ENCODE_CHAR((g),6)            \
    | _AVA_ASCII9_ENCODE_CHAR((h),7)            \
    | _AVA_ASCII9_ENCODE_CHAR((i),8)            \
    )

/**
 * This is an internal structure.
 */
typedef union {
  /**
   * For a slice, the offset within the body at which the slice begins.
   */
  size_t offset;
  /**
   * For a concat, the right string.
   */
  ava_string string;
} ava_twine_tail_other;

/**
 * This is an internal structure. It is only present in the header to permit
 * static intialisation. While the structure is partially documented here,
 * there is no guarantee that the documented semantics will be preserved.
 *
 * A twine is a lazy tree of string operations which is forced into a flat
 * array (NUL-terminated for convenience) when required or when the memory
 * overhead exceeds a certain threshold.
 */
struct ava_twine_s {
  /**
   * The main body of this string.
   *
   * This is not always a pointer; it is declared as such to permit
   * AVA_STATIC_STRING() to assign a pointer to it. When it does contain a
   * pointer, the pointer must have 8-byte alignment.
   */
  const void*restrict body;
  /**
   * The length of this twine.
   */
  size_t length;

  /**
   * Extra data not useful for forced twine nodes. Forced nodes may store
   * string data here.
   */
  struct {
    /**
     * The overhead, in bytes, of maintaining the twine in unforced form.
     */
    size_t overhead;
    /**
     * The "other" piece of data needed for the current form of this twine
     * node.
     */
    ava_twine_tail_other other;
  } tail;
};

/**
 * The empty string.
 */
#define AVA_EMPTY_STRING ((ava_string) { .ascii9 = 1 })

/**
 * Equivalent to AVA_EMPTY_STRING, but is guaranteed to be considered a
 * constant initialiser.
 *
 * (Some gcc versions, like 4.9.2, don't think the
 * compound-literal-in-parentheses is a constant expression.)
 */
#define AVA_EMPTY_STRING_INIT { .ascii9 = 1 }

/**
 * The absent string.
 */
#define AVA_ABSENT_STRING ((ava_string) { .ascii9 = 0 })

/**
 * Equivalent to AVA_ABSENT_STRING, but is guaranteed to be considered a
 * constant initialiser.
 *
 * (Some gcc versions, like 4.9.2, don't think the
 * compound-literal-in-parentheses is a constant expression.)
 */
#define AVA_ABSENT_STRING_INIT { .ascii9 = 0 }

/**
 * Expands to a static initialiser for an ava_string containing the given
 * constant string.
 *
 * Note that this is not an expression; it defines a static constant with the
 * chosen name.
 */
#define AVA_STATIC_STRING(name, text)                   \
  static const char name##__dat[                        \
    (sizeof(text) + sizeof(ava_ulong)-1) /              \
    sizeof(ava_ulong) * sizeof(ava_ulong)]              \
  AVA_STR_ALIGN = text;                                 \
                                                        \
  static const ava_twine name##__twine = {              \
    .body = name##__dat,                                \
    .length = sizeof(text) - 1,                         \
    .tail = { 0, { 0 } },                               \
  };                                                    \
  static const ava_string name = {                      \
    .twine = &name##__twine                             \
  }

/**
 * Expands to an initialiser for an ava_string containing the given constant
 * string, which must not exceed 9 characters in length, and may not contain
 * non-ASCII characters or NUL characters.
 *
 * This can safely be used in any context.
 */
#define AVA_ASCII9_STRING(text) ((ava_string) { \
      .ascii9 = _AVA_ASCII9_ENCODE_STR(text)    \
    })

/**
 * Like AVA_ASCII9_STRING(), but the result is a legal static initialiser, and
 * each character must be passed in separately.
 */
#define AVA_ASCII9_INIT(...) { .ascii9 = AVA_ASCII9(__VA_ARGS__) }

/**
 * Returns whether the given string is considered present.
 */
static inline ava_bool ava_string_is_present(ava_string str) AVA_CONSTFUN;
static inline ava_bool ava_string_is_present(ava_string str) {
  return !!str.ascii9;
}

/**
 * Returns whether the given string is empty.
 */
static inline ava_bool ava_string_is_empty(ava_string str) AVA_PURE;
static inline ava_bool ava_string_is_empty(ava_string str) {
  return 1 == str.ascii9 ||
    (0 == (str.ascii9 & 1) && 0 == str.twine->length);
}

/**
 * Returns an ava_string whose contents are that of the given NUL-terminated
 * string. The ava_string will hold no references to the original string, so it
 * need not be managed by the GC or be immutable.
 *
 * Equivalent to ava_string_of_bytes(str, strlen(str)).
 */
ava_string ava_string_of_cstring(const char* str) AVA_PURE;
/**
 * Returns an ava_string whose contents is the one character given.
 */
ava_string ava_string_of_char(char) AVA_PURE;
/**
 * Returns an ava_string whose contents are that of the given byte-buffer,
 * which need not be NUL-terminated and may contain NUL characters, and whose
 * length is the specified buffer size. The ava_string will not reference the
 * buffer after creation, so the buffer need not be GC-managed or immutable.
 */
ava_string ava_string_of_bytes(const char*, size_t) AVA_PURE;
/**
 * Returns a GC-managed C string which holds the string contents of the given
 * ava_string. No special handling of embedded NUL characters occurs.
 *
 * When a C string is needed only temporarily (eg, to be passed to printf),
 * prefer ava_string_to_cstring_buff().
 *
 * The return value is guaranteed to be 8-byte-aligned. Furthermore, the return
 * value is guaranteed to have a readable length which is a multiple of 8, with
 * all bytes beyond the end of the string initialised to 0. Ie, it is safe to
 * cast the `const char*` to a `const ava_ulong*` and consume the data that way.
 */
const char* ava_string_to_cstring(ava_string str) AVA_PURE;
/**
 * Returns a pointer to a NUL-terminated string with the same contents as the
 * given string.
 *
 * Unlike ava_string_to_cstring(), the returned value is not guaranteed to be
 * GC-managed; rather, its lifetime is bound to that of the tmp buffer.
 *
 * This never requires an extra heap allocation.
 *
 * The result meets all guarantees provided by ava_string_to_cstring().
 *
 * @param tmp A scratch buffer to use if necessary to back the returned string.
 * @param str The string to convert to a C String.
 */
const char* ava_string_to_cstring_buff(ava_str_tmpbuff tmp,
                                       ava_string str) AVA_PURE;
/**
 * Copies the characters in str, starting at start, inclusive, and ending at
 * end, exclusive, into the given byte buffer.
 *
 * Behaviour is undefined if start > end, or if either is greater than the
 * length of the string.
 */
void ava_string_to_bytes(void*restrict dst, ava_string str,
                         size_t start, size_t end);

/**
 * Returns the length, in bytes, of the given string.
 *
 * Complexity: O(1)
 */
size_t ava_strlen(ava_string str) AVA_PURE;
/**
 * Returns the character at the given index within the given string.
 *
 * Behaviour is undefined if the index is greater than or equal to the length
 * of the string.
 *
 * Complexity: Amortised voluminous O(1)
 */
char ava_string_index(ava_string str, size_t ix) AVA_PURE;
/**
 * Returns the contents of the given string between the begin index, inclusive,
 * and the end index, exclusive.
 *
 * Behaviour is undefined if begin > end or if either index is greater than the
 * length of the string.
 *
 * Complexity: Amortized O(1)
 */
ava_string ava_string_slice(ava_string, size_t begin, size_t end) AVA_PURE;
/**
 * Equivalent to ava_string_slice(str, 0, end), but more efficient.
 *
 * Complexity: Amortised O(1)
 */
ava_string ava_string_trunc(ava_string str, size_t end) AVA_PURE;
/**
 * Equivalent to ava_string_slice(str, begin, ava_strlen(str)), but more
 * efficient.
 *
 * Complexity: Amortised O(1)
 */
ava_string ava_string_behead(ava_string str, size_t begin) AVA_PURE;
/**
 * Produces a string containing the values of both strings concatenated.
 *
 * Complexity: Amortized O(1)
 */
ava_string ava_strcat(ava_string left, ava_string right) AVA_PURE;

/**
 * Returns a 32-bit hash of the given ASCII9 string.
 *
 * Any two invocations of this function with the same input within the same
 * process will return the same result. Different inputs will usually produce
 * different results. Hash values are effectively randomly distributed through
 * a large subset of the 32-bit integer space.
 *
 * Note that this hash function is entirely unrelated to ava_value_hash(), and
 * will return different values for the same string.
 *
 * This function is far less collision-resistent than ava_value_hash(); care
 * should be taken when using it. If the whole result cannot be used, the lower
 * bits should be preferred.
 */
/* Note: Implemented in value.c since it uses the same initialisation vector as
 * ava_value_hash().
 */
ava_uint ava_ascii9_hash(ava_ascii9_string str) AVA_CONSTFUN;

/**
 * Compares two strings, like strcmp().
 *
 * This function operates on unsigned bytes in the string regardless of the
 * signedness of the platform's char type, such that 'a\x80' is always ordered
 * after 'ab'.
 *
 * If one string is a prefix of the other, the shorter one is considered
 * lexicographically first.
 *
 * @return Zero if the two strings are exactly equal; a negative integer if a
 * is lexicographically before b; a positive integer if a is lexicographically
 * before a.
 */
signed ava_strcmp(ava_string a, ava_string b) AVA_PURE;

/**
 * Returns whether small is a prefix of big.
 */
ava_bool ava_string_starts_with(ava_string big, ava_string small) AVA_PURE;
/**
 * Returns whether the two given strings are equal.
 *
 * Essentially the same as testing whether ava_strcmp() is zero, but faster
 * when the strings are not equal.
 */
ava_bool ava_string_equal(ava_string a, ava_string b) AVA_PURE;

/**
 * Returns an ava_ascii9_string representing the given string if it is possible
 * to represent the string in that format; otherwise, returns 0.
 *
 * This makes it possible to switch over small ASCII strings with AVA_ASCII9().
 */
ava_ascii9_string ava_string_to_ascii9(ava_string str) AVA_PURE;

/**
 * Returns the index of the first character which is equal in the two given
 * ASCII9 strings, treating each string as if it were exactly 9 characters long
 * (padded at right with NULs). If no characters match, returns -1.
 *
 * This is intended to be used for ava_strchr_ascii(), where haystack is the
 * input string and needle is the target character saturating an ASCII9 string.
 */
ssize_t ava_ascii9_index_of_match(ava_ascii9_string haystack,
                                  ava_ascii9_string needle) AVA_PURE;

/**
 * Returns the first index of the character needle within string haystack, or
 * -1 if there is no such character.
 *
 * If needle is a non-NUL ASCII literal chacter, use ava_strchr_ascii()
 * instead.
 */
ssize_t ava_strchr(ava_string haystack, char needle) AVA_PURE;

/**
 * Returns whether the given string is an ASCII9 string.
 */
static inline ava_bool ava_string_is_ascii9(ava_string str) {
  return str.ascii9 & 1;
}

/**
 * Like ava_strchr(), but faster when needle is a character literal.
 *
 * needle MUST be a valid ASCII9 charatcer; ie, non-NUL and within the 7-bit
 * ASCII range.
 */
static inline ssize_t ava_strchr_ascii(ava_string haystack, char needle) {
  if (ava_string_is_ascii9(haystack)) {
    return ava_ascii9_index_of_match(
      haystack.ascii9,
      AVA_ASCII9(needle, needle, needle,
                 needle, needle, needle,
                 needle, needle, needle));
  } else {
    return ava_strchr(haystack, needle);
  }
}

#endif /* AVA_RUNTIME_STRING_H_ */
