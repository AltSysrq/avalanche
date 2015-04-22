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
#ifndef AVA_AVALANCHE_H_
#define AVA_AVALANCHE_H_

#if defined(__GNUC__) || defined(__clang__)
#define AVA_MALLOC __attribute__((__malloc__))
#define AVA_PURE __attribute__((__pure__))
#define AVA_CONSTFUN __attribute__((__const__))
#else
#define AVA_MALLOC
#define AVA_PURE
#define AVA_CONSTFUN
#endif

#ifdef __cplusplus
#define AVA_BEGIN_DECLS extern "C" {
#define AVA_END_DECLS }
#else
#define AVA_BEGIN_DECLS
#define AVA_END_DECLS
#endif

AVA_BEGIN_DECLS

/**
 * The maximum depth of a rope. This value is not useful to clients; it merely
 * determines the structures of some values.
 *
 * This puts an effective maximum string size of fib(64-2) = 3.7TB. (Where
 * fib(0) = 0, fib(1) = 1.)
 */
#define AVA_MAX_ROPE_DEPTH 64

/******************** MEMORY MANAGEMENT ********************/
/**
 * Initialises the Avalanche heap. This should be called once, at the start of
 * the process.
 */
void ava_heap_init(void);
/**
 * Allocates and returns a block of memory of at least the given size. The
 * memory is initialised to zeroes.
 *
 * There is no way to explicitly free this memory; it will be released
 * automatically when no GC-visible pointers remain.
 *
 * If memory allocation fails, the process is aborted.
 */
void* ava_alloc(size_t) AVA_MALLOC;
/**
 * Allocates and returns a block of memory of at least the given size. The
 * memory is initialised to zeroes.
 *
 * The caller may not store any pointers in this memory.
 *
 * There is no way to explicitly free this memory; it will be released
 * automatically when no GC-visible pointers remain.
 *
 * If memory allocation fails, the process is aborted.
 */
void* ava_alloc_atomic(size_t) AVA_MALLOC;
/**
 * Allocates and returns a block of memory of at least the given size.
 *
 * This memory must be explicitly freed with ava_free_unmanaged(). Its chief
 * difference from a plain malloc() is that the GC is aware of this memory, so
 * it may contain pointers to managed memory.
 *
 * If memory allocation fails, the process is aborted.
 */
void* ava_alloc_unmanaged(size_t) AVA_MALLOC;
/**
 * Frees the given memory allocated from ava_alloc_unmanaged().
 */
void ava_free_unmanaged(void*);
/**
 * Performs an ava_alloc() with the given size, then copies that many bytes
 * from the given source pointer into the new memory before returning it.
 *
 * If memory allocation fails, the process is aborted.
 */
void* ava_clone(const void*restrict, size_t) AVA_MALLOC;
/**
 * Performs an ava_alloc_atomic() with the given size, then copies that many
 * bytes from the given source pointer into the new memory before returning it.
 *
 * If memory allocation fails, the process is aborted.
 */
void* ava_clone_atomic(const void*restrict, size_t) AVA_MALLOC;
/**
 * Syntax sugar for calling ava_alloc() with the size of the selected type and
 * casting it to a pointer to that type.
 */
#define AVA_NEW(type) ((type*)ava_alloc(sizeof(type)))


/******************** STRING HANDLING ********************/

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
typedef unsigned long long ava_ascii9_string;
/**
 * An arbitrary byte string of any number of characters. The exact internal
 * format is undefined.
 */
typedef struct ava_rope_s ava_rope;

/**
 * The primary Avalanche string type.
 *
 * The encoding of the string can be identified by testing bit 0 of the ascii9
 * field; if it is zero, the string is a rope or absent. If it is 1, the string
 * is an ascii9 string.
 *
 * A string is said to be "absent" if the ascii9 field identifies the string as
 * a rope and the rope field is NULL.
 */
typedef union {
  ava_ascii9_string ascii9;
  const ava_rope*restrict rope;
} ava_string;

#define _AVA_IS_ASCII9_CHAR(ch) ((ch) > 0 && (ch) < 128)
#define _AVA_ASCII9_ENCODE_CHAR(ch,ix)                  \
  (((ava_ascii9_string)((ch) & 0x7F)) << (57 - (ix)*7))
#define _AVA_STRCHR(str,ix) ((ix) < sizeof(str)? (str)[ix] : 0)
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
 * This is an internal structure. It is only present in the header to permit
 * static intialisation. While the structure is documented, there is no
 * guarantee that the documented semantics will be preserved.
 *
 * A rope is a string data structure which supports very efficient
 * concatenation (very common in Tcl and Avalanche) and splitting at the cost
 * of reduced direct-indexing performance. It is represented as a tree of
 * Concats, which always have two children but no text, and Leaves, which have
 * no children but do have text.
 *
 * See also http://citeseer.ist.psu.edu/viewdoc/download?doi=10.1.1.14.9450&rep=rep1&type=pdf
 */
struct ava_rope_s {
  /**
   * The length of the string represented by this rope node.
   *
   * The "weight" described in some contexts can be obtained by looking at the
   * length of v.concat_left.
   */
  size_t length;
  /**
   * The size, in terms of external memory usage, of this rope node. ASCII9
   * leaves have a size of 0; flat array leaves have a size equal to the size
   * of the backing array, if allocated on the heap. Concats have a size equal
   * to the sum of the sizes of their children.
   *
   * This value is used to choose between pointing into the original array or
   * copying to a new, smaller array on substring operations.
   */
  size_t external_size;
  /**
   * The depth of this node. Leaves have depth zero; Concats have a depth one
   * greater than the maximum depth of either branch.
   */
  unsigned depth;
  /**
   * The number of children, direct and indirect, of this node. Leaves have 0
   * descendants.
   */
  unsigned descendants;
  /**
   * The value of this node. If concat_right is greater than 1, it is a Concat;
   * otherwise, it is a Leaf. ASCII9 and array-based leaves can be
   * distinguished by bit0 of concat_right, a 1 indicating ASCII9 and a 0
   * indicating a flat array. (The zero bit of leaf9 cannot be used for this
   * purpose, sine leafv does not have any particular alignment.)
   */
  union {
    /**
     * Leaves with 9 or fewer non-NUL ASCII characters are usually packed into
     * an ascii9 string to save space and allocations.
     */
    ava_ascii9_string leaf9;
    /**
     * Leaves that cannot be packed into an ASCII9 string are instead allocated
     * as a flat array. This array are not NUL-terminated. The number of
     * characters is equal to the length of the rope node.
     *
     * This field contains the node's value if concat_right is NULL and bit 0
     * of v.leaf9 is clear.
     */
    const char*restrict leafv;
    /**
     * The left branch of a Concat.
     *
     * The node is a Concat iff concat_right is non-NULL.
     */
    const ava_rope*restrict concat_left;
  } v;
  /**
   * If non-NULL, this node is a Concat, and this value points to its right
   * branch.
   */
  const ava_rope*restrict concat_right;
};

/**
 * The empty string.
 */
#define AVA_EMPTY_STRING ((ava_string) { .ascii9 = 1 })

/**
 * The absent string.
 */
#define AVA_ABSENT_STRING ((ava_string) { .ascii9 = 0 })

/**
 * Produces an ava_string containing the given rope.
 */
static inline ava_string ava_string_of_rope(const ava_rope*restrict rope) {
  ava_string str;

  str.ascii9 = 0;
  str.rope = rope;
  return str;
}

/**
 * Expands to a static initialiser for an ava_string containing the given
 * constant string.
 *
 * Note that this is not an expression; it defines a static constant with the
 * chosen name.
 */
#define AVA_STATIC_STRING(name, text)           \
  static const ava_rope name##__rope = {        \
    .length = sizeof(text) - 1,                 \
    .depth = 0,                                 \
    .v = {                                      \
      .leafv = (text)                           \
    },                                          \
    .concat_right = NULL,                       \
  };                                            \
  static const ava_string name = {              \
    .rope = &name##__rope                       \
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
 * Returns whether the given string is considered present.
 */
static inline int ava_string_is_present(ava_string str) AVA_CONSTFUN;
static inline int ava_string_is_present(ava_string str) {
  return !!str.ascii9;
}

/**
 * Returns an ava_string whose contents are that of the given NUL-terminated
 * string. The C string MUST be immutable and managed by the GC; the ava_string
 * will maintain references into the original when beneficial.
 *
 * Equivalent to ava_string_of_shared_bytes(str, strlen(str)).
 */
ava_string ava_string_of_shared_cstring(const char* str) AVA_PURE;
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
 * length is the specified buffer size. The buffer MUST be immutable and
 * managed by the GC; the ava_string will maintain references into it when
 * beneficial.
 */
ava_string ava_string_of_shared_bytes(const char*, size_t) AVA_PURE;
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
 * The caller may mutate the returned C string if it so desires. No storage is
 * guaranteed to exist beyond the terminating NUL byte.
 */
char* ava_string_to_cstring(ava_string) AVA_PURE;
/**
 * Returns a C string which holds the string contents of the given ava_string.
 * No special handling of embedded NUL characters occurs.
 *
 * If the C string (including the terminating NUL character) fits within the
 * given buffer, the buffer is used to hold the string. Otherwise, a GC-managed
 * heap allocation is performed and used to hold the return value.
 *
 * The caller may mutate the returned C string if it so desires. No storage is
 * guaranteed to exist beyond the terminating NUL byte.
 *
 * @param buff A temporary buffer to use if possible.
 * @param sz The maximum number of bytes, including the terminating NUL
 * character, that may be written to buff.
 * @param str The string to convert.
 * @return A C string containing str's contents, which may or may not be the
 * input buffer.
 */
char* ava_string_to_cstring_buff(
  char* buff, size_t sz, ava_string str);
/**
 * Returns a C string which holds the string contents of the given ava_string.
 * No special handling of embedded NUL characters occurs.
 *
 * If (*buffptr) is non-NULL, that byte-buffer will be used to hold the return
 * value if the space needed is less than or equal to (*szptr). Otherwise, a
 * new buffer is allocated that is large enough to hold the string and is
 * filled with the string contents. In the latter case, (*buffptr) and (*szptr)
 * are updated for the new buffer. It is assumed that (*buffptr), when
 * non-NULL, is either automatic or GC-managed storage, and therefore does not
 * need explicit freeing.
 *
 * The caller may mutate the returned C string if it so desires. It is
 * guaranteed that the return value and (*buffptr) has at least (*szptr) bytes
 * of storage after this call returns.
 *
 * @param buffptr A pointer to a temporary buffer to use if possible. May be
 * NULL.
 * @param szptr A pointer to the size of (*buffptr); ignored if (*buffptr) is
 * NULL.
 * @param str The string to convert.
 * @return A C string containing str's contents, which is equal to the value of
 * (*buffptr) after return.
 */
char* ava_string_to_cstring_tmpbuff(char** buffptr, size_t* szptr,
                                    ava_string str);

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
size_t ava_string_length(ava_string) AVA_PURE;
/**
 * Returns the character at the given index within the given string.
 *
 * Behaviour is undefined if the index is greater than or equal to the length
 * of the string.
 *
 * Complexity: O(log(n))
 */
char ava_string_index(ava_string, size_t) AVA_PURE;
/**
 * Returns the contents of the given string between the begin index, inclusive,
 * and the end index, exclusive.
 *
 * Behaviour is undefined if begin > end or if either index is greater than the
 * length of the string.
 *
 * Complexity: Amortized O(log(n))
 */
ava_string ava_string_slice(ava_string, size_t begin, size_t end) AVA_PURE;
/**
 * Produces a string containing the values of both strings concatenated.
 *
 * Complexity: Amortized O(log(n))
 */
ava_string ava_string_concat(ava_string, ava_string) AVA_PURE;

/**
 * Optimises the given string for read access.
 *
 * This should only be done for newly-produced strings that are not expected to
 * be sharing memory with anything else; otherwise, this will likely be
 * counter-productive.
 *
 * Complexity: O(n)
 */
ava_string ava_string_optimise(ava_string) AVA_PURE;

/**
 * An iterator into an ava_string, allowing efficient linear access to string
 * elements.
 *
 * Iterators can safely be copied via assignment.
 *
 * Implementation details:
 *
 * The iterator is a stack of ropes, the top of which is a leaf. Every other
 * stack element is the direct parent of the one below it. Each stack element
 * tracks its offset within the rope. (This includes concats, since we can't
 * derive the information by looking at the children, because a Concat could
 * have the same rope for both children.)
 *
 * Out-of-bounds iterators are represented by moving the real_index to the
 * index nearest the requsted location, and logical_index to that location.
 *
 * ASCII9 strings are copied into the ascii9 field of the iterator.
 */
typedef struct {
  struct {
    const ava_rope*restrict rope;
    size_t offset;
  } stack[AVA_MAX_ROPE_DEPTH];
  unsigned top;
  size_t real_index, length;
  ssize_t logical_index;
  ava_ascii9_string ascii9;
} ava_string_iterator;

/**
 * Initialises the given iterator to point at the given index within the given
 * string.
 *
 * Complexity: O(log(n))
 */
void ava_string_iterator_place(ava_string_iterator* it,
                               ava_string str, size_t index);
/**
 * Adjusts the position of the given iterator within its string. If the
 * iterator would move off the end of the string, it becomes invalid, but moves
 * its logical index the specified amount anyway.
 *
 * Invalid iterators may be moved back into a valid range, at which point they
 * become valid again.
 *
 * @return Whether the iterator is still valid.
 */
int ava_string_iterator_move(ava_string_iterator* it,
                             ssize_t distance);
/**
 * Returns whether the given iterator is valid.
 */
int ava_string_iterator_valid(const ava_string_iterator*) AVA_PURE;
/**
 * Returns the character pointed to by the given string iterator.
 *
 * The return value is undefined if the iterator is invalid, but behaviour is
 * otherwise defined. (Ie, no particular value will be returned, but the
 * process won't segfault.)
 */
char ava_string_iterator_get(const ava_string_iterator*) AVA_PURE;
/**
 * Reads up to n bytes from the string backing iterator, starting at the
 * iterator's current position, into dst, returning the number of bytes read
 * and advancing the iterator's position.
 *
 * The number of bytes actually read may be less than n, if reading more would
 * have been just as expensive as calling this function twice.
 *
 * Returns 0 if the iterator is invalid.
 */
size_t ava_string_iterator_read(char*restrict dst, size_t n,
                                ava_string_iterator* iterator);
/**
 * Reads up to n bytes from the string backing iterator, starting at the
 * iterator's current position, into dst, returning the number of bytes read.
 * The iterator's position is not advanced.
 *
 * The number of bytes actually read may be less than n, if reading more would
 * have been just as expensive as calling this function twice.
 *
 * Returns 0 if the iterator is invalid.
 */
size_t ava_string_iterator_read_hold(char*restrict dst, size_t n,
                                     const ava_string_iterator* iterator);
/**
 * Provides direct, non-copying access to the contents of a string.
 *
 * dst is set to a pointer containing the next character(s) in the string
 * following the iterator. Should copying be necessary, the caller-supplied
 * tmpbuff will be used to hold the temporary data; therefore, the lifetime of
 * the output pointer is bounded by that of tmpbuff.
 *
 * If the iterator is invalid, dst is set to NULL and 0 is returned.
 *
 * @param dst Out-parameter to hold a pointer into the string. (*dst) becomes
 * invalid when tmpbuff is destroyed, or the next time the same tmpbuff is
 * passed into ava_string_iterator_access(). (*dst) may point into tmpbuff
 * rather than to its beginning.
 * @param iterator The iterator with which to access the string.
 * @param tmpbuff A location in which to store characters if they must be
 * copied; must be at least 9 characters wide.
 * @return The number of characters behind (*dst) which are part of the string.
 */
size_t ava_string_iterator_access(const char*restrict* dst,
                                  const ava_string_iterator* iterator,
                                  char*restrict tmpbuff);
/**
 * Returns the current logical index of the given string iterator.
 *
 * If the iterator is currently invalid, returns 0 if the logical pointer is
 * before the start of the string, or the length of the string if the logical
 * pointer is beyond the end of the string.
 */
size_t ava_string_iterator_index(const ava_string_iterator*) AVA_PURE;


/******************** LEXICAL ANALYSIS ********************/

/**
 * Opaque struct storing the state of the lexical analyser.
 */
typedef struct ava_lex_context_s ava_lex_context;

/**
 * Describes the type of a lexed token.
 */
typedef enum {
  /**
   * The token is a simple string without any quoting.
   */
  ava_ltt_bareword,
  /**
   * The token is an actual or logical line break.
   */
  ava_ltt_newline,
  /**
   * The token is a string enclosed in double-quotes.
   */
  ava_ltt_astring,
  /**
   * The token is a string initiated with a back-quote and terminated with a
   * double-quote.
   */
  ava_ltt_lstring,
  /**
   * The token is a string initiated with a double-quote an terminated with a
   * back-quote.
   */
  ava_ltt_rstring,
  /**
   * Tke token is a string enclosed in back-quotes.
   */
  ava_ltt_lrstring,
  /**
   * The token is a left-parenthesis starting a substitution.
   */
  ava_ltt_begin_substitution,
  /**
   * The token is a left-parenthesis starting a name subscript.
   */
  ava_ltt_begin_name_subscript,
  /**
   * The token is a right-parenthesis.
   */
  ava_ltt_close_paren,
  /**
   * The token is a left-bracket starting a semiliteral.
   */
  ava_ltt_begin_semiliteral,
  /**
   * The token is a left-bracket starting a numeric subscript.
   */
  ava_ltt_begin_numeric_subscript,
  /**
   * The token is a right-bracket.
   */
  ava_ltt_close_bracket,
  /**
   * The token is a left-brace starting a block.
   */
  ava_ltt_begin_block,
  /**
   * The token is a right-brace.
   */
  ava_ltt_close_brace,
  /**
   * The token is a string enclosed in \{...\}.
   */
  ava_ltt_verbatim,
  /**
   * Not an actual token type; used when no token could be extracted due to
   * end-of-input or error.
   */
  ava_ltt_none
} ava_lex_token_type;

/**
 * Output type from the lexical analyser.
 */
typedef struct {
  /**
   * The type of token encountered.
   */
  ava_lex_token_type type;
  /**
   * IF type is not ava_ltt_none, the string content of the token, after escape
   * sequence substitution. (Eg, the token
   *    "foo\x41"
   * would have a str value of
   *    fooA
   * ).
   *
   * Otherwise, this contains the error message if an error was encountered, or
   * empty string if end-of-file.
   */
  ava_string str;
  /**
   * The line and column numbers where the start of this result was found. Both
   * are 1-based.
   */
  size_t line, column;
  /**
   * The indices between which the raw token can be found.
   */
  size_t index_start, index_end;
  /**
   * The byte offset within the original string at which the line on which this
   * token is found begins.
   */
  size_t line_offset;
} ava_lex_result;

/**
 * Indicates whether the lexer successfully extracted a token.
 *
 * Zero is the only success code; any true value indicates that no token was
 * extracted.
 */
typedef enum {
  /**
   * Success.
   */
  ava_ls_ok = 0,
  /**
   * No error occurred, but the lexer encountered the end of the input string
   * before extracting any new tokens.
   */
  ava_ls_end_of_input,
  /**
   * An error in the lexical syntax was encountered.
   */
  ava_ls_error
} ava_lex_status;

/**
 * Returns whether the given token type can be treated as a simple string.
 *
 * This assumes the caller assigns no special semantics to barewords.
 */
int ava_lex_token_type_is_simple(ava_lex_token_type);
/**
 * Returns whether the given token type is any of the three
 * close-parenthesis-like token types.
 */
int ava_lex_token_type_is_close_paren(ava_lex_token_type);

/**
 * Creates a new lexical analyser that will tokenise the given string.
 *
 * @return The lexical analyser, managed on the garbage-collected heap.
 */
ava_lex_context* ava_lex_new(ava_string);
/**
 * Obtains the next token from the lexical analyser.
 *
 * @param result The result in which to store output information. All fields of
 * this value are written regardless of the return status.
 * @param analyser The lexical analyser. The state of the analyser will be
 * advanced to the next token.
 * @return The status of this lexing attempt. It is safe to continue using the
 * lexer even after it returns an error.
 */
ava_lex_status ava_lex_lex(ava_lex_result* result, ava_lex_context* analyser);

/******************** VALUE AND TYPE SYSTEM ********************/

/**
 * Avalanche's "everything is a string" type system, as in later Tcl versions,
 * is merely an illusion. In reality, higher-level representations of values
 * are maintained for the sake of performance.
 *
 * In Tcl, this was accomplished by heap-allocating values, each of which has
 * either or both of a string representation and a higher-level representation.
 * In Avalanche, values are actual immutable value types. They may have one or
 * two representations at any given time, but what those representations are is
 * determined by the dynamic type of the value.
 *
 * Core concepts:
 *
 * - Value. A value is an instance of ava_value. Values are almost always
 *   passed by value; this allows the C compiler to perform a large number of
 *   optimisations, in particular constant propagation of the dynamic type and
 *   dead store elimination. A value is associated with one or two
 *   representations and a type.
 *
 * - Type. A type defines (a) a set of permissible string values which is a
 *   subset the set of all strings; (b) a higher-level internal representation
 *   that may be more performant than raw strings; (c) a set of method
 *   implementations. The finite set of types in the process is determined at
 *   compile time; types can only be introduced by C code. Constructing a value
 *   of a particular type from another value is to "imbue" the value with that
 *   type.
 *
 * - Representation. The physical way a value is stored. Without knowledge of
 *   the particular type, the representation of a value is totally opaque.
 *   Certain types, such as ava_string_type, MAY make their representation
 *   public.
 *
 * An important thing to note is that values *always* preserve their native
 * string representation. If a string "0x01" is converted to integer 1, it must
 * still appear to be the string "0x01". Functions which return *new* values
 * may define themselves to produce _normalised_ values, in which case this
 * does not apply. (Eg, "0x0a"+"0" produces "10", not "0x0a".)
 */
typedef struct ava_value_s ava_value;

/**
 * A single representation of a value.
 *
 * The usage of this union is entirely up to the type on the value.
 */
typedef union {
  unsigned char         ubytes[8];
  signed char           sbytes[8];
  char                  chars[8];
  unsigned short        ushorts[4];
  signed short          sshorts[4];
  unsigned int          uint[2];
  signed int            sint[2];
  unsigned long long    ulong;
  signed long long      slong;
  const void*restrict   ptr;
  ava_string            str;
} ava_representation;

/**
 * Defines the type of a value and how its representation is interpreted.
 *
 * As previously described, types provide *performance*, not functionality.
 * This is formalised by several rules:
 *
 * - Every value of every type has a string representation which perfectly
 *   represents the value. to_type(to_string(value_of_type)) == value_of_type
 *
 * - Every value of every type provides sufficient information to reproduce the
 *   original string. This means that types supporting denormalised string
 *   representations must typically preserve the original string until passed
 *   through some explicitly normalising operation.
 *   to_string(to_type(string)) == string
 *
 * - The effect of performing any operation on a value of a type is exactly
 *   equivalent to having performed the same operation on the string
 *   representation. f(value_of_type) == f(to_string(value_of_type))
 */
typedef struct ava_value_type_s ava_value_type;

struct ava_value_s {
  /**
   * The one or two representations of this value, as controlled by the type.
   */
  ava_representation r1, r2;
  /**
   * The dynamic type of this value.
   */
  const ava_value_type*restrict type;
};

/**
 * An accelerator identifies a set of operations that can be performed on any
 * value, but which may be implemented more efficiently on particular types.
 *
 * An ava_accelerator itself contains no information; it is meerly a pointer
 * used to guarantee uniqueness.
 *
 * Use the AVA_DECLARE_ACCELERATOR and AVA_DEFINE_ACCELERATOR to define values
 * of this type.
 */
typedef struct {
  /**
   * sizeof(ava_accelerator)
   */
  size_t size;
} ava_accelerator;

/**
 * Declares the existence of a particular accelerator, appropriate for use in a
 * header file.
 */
#define AVA_DECLARE_ACCELERATOR(name) extern const ava_accelerator name
/**
 * Defines an accelerator; must be located in a source file.
 */
#define AVA_DEFINE_ACCELERATOR(name) \
  const ava_accelerator name = { .size = sizeof(ava_accelerator) }

struct ava_value_type_s {
  /**
   * sizeof(ava_value_type)
   */
  size_t size;

  /**
   * A human-readable name of this type, for diagnostic and debugging purposes.
   */
  const char* name;

  /**
   * Defines how to reproduce the string representation of a value of this
   * type.
   *
   * Do not call this directly; use ava_to_string() instead.
   *
   * If the underlying type can produce string fragments in chunks more
   * efficiently, it should implement string_chunk_iterator() and
   * iterate_string_chunk() instead and use ava_string_of_chunk_iterator
   * instead.
   *
   * This function is assumed to be pure.
   *
   * @param value The value which is to be converted to a string; guaranteed to
   * have a type equal to this type.
   * @return The string representation of the value.
   */
  ava_string (*to_string)(ava_value value);
  /**
   * Certain types, such as lists, can efficiently produce strings in chunks;
   * similarly, some APIs can perform equally well or better with a sequence of
   * smaller string chunks as with a monolithic string.
   *
   * string_chunk_iterator() begins iterating the string chunks in a value. It
   * returns an arbitrary representation object with which it can track its
   * state.
   *
   * iterate_string_chunk() can be called successively with a pointer to its
   * state to obtain each successive chunk in the value. The end of the
   * sequence is indicated by returning an absent string.
   *
   * If the underlying type more naturally produces a monolithic string,
   * implement to_string() instead, and use
   * ava_singleton_string_chunk_iterator() and
   * ava_iterate_singleton_string_chunk() for these fields instead.
   *
   * Do not call this function directly; use ava_string_chunk_iterator()
   * instead.
   *
   * @param value The value which is to be converted to a string in chunks.
   * @return The initial state of the iterator.
   */
  ava_representation (*string_chunk_iterator)(ava_value value);
  /**
   * Continues iteration over the chunks of a value.
   *
   * Do not call this function directly; use ava_iterate_string_chunk()
   * instead.
   *
   * @see string_chunk_iterator().
   * @param it A pointer to the current iterator state.
   * @param value The value being converted to a string in chunks.
   * @return The next chunk in the string, or an absent string to indicate that
   * iteration has completed.
   */
  ava_string (*iterate_string_chunk)(ava_representation*restrict it,
                                     ava_value value);

  /**
   * Queries whether the type supports the given accelerator.
   *
   * Do not call this function directly; use ava_query_accelerator() instead.
   *
   * If the type recognises the given accelerator, it MAY return a value
   * particular to that type; otherwise, it MUST return dfault.
   *
   * The exact semantics of dfault and the return value are entirely dependent
   * on the accelerator; typically, it will be a struct containing function
   * pointers and possibly some properties global to the type.
   *
   * Generally, dfault should provide an implementation of the operations that
   * operates on string values.
   *
   * The implementation may not inspect the contents of *accel, and may not do
   * anything whatsoever with dfault except return it.
   *
   * @see ava_noop_query_accelerator()
   * @param accel The accelerator being queried.
   * @param dfault The value to return if the type does not know about accel.
   * @return An accelerator-dependent value.
   */
  const void* (*query_accelerator)(const ava_accelerator* accel,
                                   const void* dfault);
};

/**
 * Converts the given value into a monolithic string.
 *
 * @see ava_value_type.to_string()
 */
static inline ava_string ava_to_string(ava_value value) AVA_PURE;
static inline ava_string ava_to_string(ava_value value) {
  return (*value.type->to_string)(value);
}

/**
 * Begins iterating string chunks in the given value.
 *
 * @see ava_value_type.string_chunk_iterator()
 */
static inline ava_representation ava_string_chunk_iterator(ava_value value) {
  return (*value.type->string_chunk_iterator)(value);
}

/**
 * Continues iterating string chunks in the given value.
 *
 * @see ava_value_type.iterate_string_chunk()
 */
static inline ava_string ava_iterate_string_chunk(
  ava_representation*restrict it, ava_value value
) {
  return (*value.type->iterate_string_chunk)(it, value);
}

/**
 * This is a workaround to hint to GCC that (*type->query_accelerator) is
 * const, since there doesn't seem to be any way to attach attributes to
 * function pointers.
 *
 * This is not considered part of the public API; do not call it yourself.
 */
static inline const void* ava___invoke_query_accelerator_const(
  const void* (*f)(const ava_accelerator*, const void*),
  const ava_accelerator* accel,
  const void* dfault
) AVA_CONSTFUN;
/**
 * Returns the implementation corresponding to the given accelerator of the
 * type associated with the given value.
 *
 * @see ava_value_type.query_accelerator()
 */
static inline const void* ava_query_accelerator(
  ava_value value,
  const ava_accelerator* accel,
  const void* dfault
) AVA_PURE;

static inline const void* ava___invoke_query_accelerator_const(
  const void* (*f)(const ava_accelerator*, const void*),
  const ava_accelerator* accel,
  const void* dfault
) {
  return (*f)(accel, dfault);
}

static inline const void* ava_query_accelerator(
  ava_value value,
  const ava_accelerator* accel,
  const void* dfault
) {
  return ava___invoke_query_accelerator_const(
    value.type->query_accelerator, accel, dfault);
}

/**
 * Constructs a string by using the chunk-iterator API on the given value.
 *
 * This is a good implementation of ava_value_type.to_string() for types that
 * implement string_chunk_iterator() and iterate_string_chunk() naturally.
 */
ava_string ava_string_of_chunk_iterator(ava_value value) AVA_PURE;

/**
 * Prepares an iterator for use with ava_iterate_singleton_string_chunk().
 *
 * This is a reasonable implementation of
 * ava_value_type.string_chunk_iterator() for types that only implement
 * to_string() naturally.
 */
ava_representation ava_singleton_string_chunk_iterator(ava_value value);

/**
 * Iterates a value as a single string chunk.
 *
 * This is a reasonable implementation of ava_value_type.iterate_string_chunk()
 * for types that only implement to_string() naturally. It must be used with
 * ava_singleton_string_chunk_iterator().
 */
ava_string ava_iterate_singleton_string_chunk(ava_representation* rep,
                                              ava_value value);

/**
 * Implementation for ava_value_type.query_accelerator() that always returns
 * the default.
 */
const void* ava_noop_query_accelerator(const ava_accelerator* accel,
                                       const void* dfault) AVA_CONSTFUN;

/**
 * The type for string values, the universal type.
 *
 * Representational semantics:
 *
 * - r1 has the str field set to the string value.
 *
 * - r2 may be used internally.
 *
 * ava_value_of_string() is the appropriate means to create a value holding a
 * string. ava_string_imbue() creates a string value from any value, and is
 * strongly preferred over ava_value_of_string(ava_to_string(value)).
 */
extern const ava_value_type ava_string_type;
/**
 * Returns a value which contains the given string, with a string type.
 */
ava_value ava_value_of_string(ava_string str) AVA_PURE;
/**
 * Returns a value which contains the string representation of the input value.
 */
ava_value ava_string_imbue(ava_value value) AVA_PURE;

AVA_END_DECLS

#endif /* AVA_AVALANCHE_H_ */
