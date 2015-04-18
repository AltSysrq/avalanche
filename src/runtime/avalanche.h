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
#define AVA_MALLOC __attribute__((malloc))
#define AVA_PURE __attribute__((pure))
#else
#define AVA_MALLOC
#define AVA_PURE
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

/******************** CORE TYPES ********************/

/**
 * Avalanche's value system is in many ways similar to that of Tcl: A value has
 * a string value and/or a logical (e.g., parsed) value, which has an
 * associated logical type.
 *
 * Unless otherwise noted, all types are intended to be used in an
 * allocate-write-freeze manner. Allocations are managed by the Boehm GC
 * (accessed from the ava_alloc() family of functions above), and thus pointers
 * can be expected to remain valid as long as they are held. Value objects
 * (i.e., the physical-logical value pair) are generally private to the holder,
 * and copied to other usage sites, rather than borrowed via pointers.
 *
 * The above underscores Avalance's differences to Tcl's approach. While the
 * lack of reference counting prevents implicit optimisation of operations to
 * destructive variants, the true immutability provides free thread-safety, and
 * most performance issues can be overcome with persistent data structures.
 * Similarly, the non-sharing use of value pairs makes it impossible for, eg,
 * the contents of a list to obtain their logical values, it also permits free
 * thread-safety and eliminates the possibility of shimmering due to
 * coincidental sharing.
 */

/**
 * The logical (parsed) content of a value. Common numeric types are provided
 * inline; anything else must be behind a pointer.
 */
typedef union {
  signed int            sint;
  unsigned int          uint;
  signed long long      slong;
  unsigned long long    ulong;
  double                real;
  const void*           ptr;
} ava_logical_value;

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

/**
 * Describes a type of logical value.
 */
typedef struct {
  /**
   * sizeof(ava_logical_type), for forwards-compatibility.
   */
  size_t size;
  /**
   * Produces the string representation of this logical value.
   *
   * This MUST produce a string for any valid logical value, and MUST be
   * idempotent. Obviously, any function(s) which parses a string into this
   * logical type MUST be able to reverse this operation perfectly.
   */
  ava_string (*to_string)(ava_logical_value);
} ava_logical_type;

/**
 * The primary type for Avalanche values.
 */
typedef struct {
  /**
   * The physical content of this value, if generated; otherwise, absent.
   *
   * Most operations on logical content return values with only logical
   * content; the physical content is generated on-demand.
   */
  ava_string physical;
  /**
   * If logical_type is non-NULL, the logical content of this value.
   */
  ava_logical_value logical;
  /**
   * If non-NULL, indentifies the type stored in the logical value. NULL
   * indicates there is no logical content.
   */
  const ava_logical_type*restrict logical_type;
} ava_value;

/**
 * Indicates the evaluation status of a particular operation.
 *
 * As a pointer, NULL is the only normal status; the default behaviour for any
 * other status is to stop further evaluation and continue propagating it up
 * the stack, except that ava_return_code_return is converted to
 * ava_return_code_ok before such propagation.
 *
 * Values of this type are generally expected to be statically allocated.
 */
typedef struct {
  /**
   * A human-readable message used to describe the return code should it
   * propagate up to a native boundary. (Eg, "internal error", "evaluation
   * failed".)
   */
  const ava_string description;
  /**
   * Arbitrary additional properties to associate with this return code. The
   * value is expected to be a dict.
   */
  const ava_value properties;
} ava_return_code_info;

/**
 * Standard return code indicating evaluation failure, that is, falseness.
 */
extern const ava_return_code_info ava_rc_fail;
/**
 * Standard return code indicating an error on the caller's part. Errors
 * indicate user programming errors and should generally not be handled.
 *
 * Error values are normally a two-tuple of a message and a stack trace.
 */
extern const ava_return_code_info ava_rc_error;
/**
 * Standard return code indicating a problem occurred in the internal runtime
 * implementation. Internal errors should generally not be handled.
 *
 * Note that the process is still expected to be in a functional state upon an
 * internal error; abnormal conditions terminate the process instead of
 * producing a return value.
 *
 * Internal error values are normally a two-tuple of a message and a stack
 * trace.
 */
extern const ava_return_code_info ava_rc_internal_error;
/**
 * Standard return code indicating a user-initiated exception.
 *
 * Exception values are normally a four-tuple of the exception type, a message,
 * a stack trace, and a dict of additional properties.
 */
extern const ava_return_code_info ava_rc_except;
/**
 * Standard return code indicating the called code wishes to terminate the loop
 * calling it.
 */
extern const ava_return_code_info ava_rc_break;
/**
 * Standard return code indicating the called code wishes to terminate the
 * current iteration of the loop calling it and begin the next iteration.
 */
extern const ava_return_code_info ava_rc_continue;
/**
 * Standard return code indicating the called code wishes its caller to return.
 */
extern const ava_return_code_info ava_return_code_return;

/**
 * Convenience for `const ava_return_code_info*`.
 */
typedef const ava_return_code_info* ava_return_code;

/**
 * Describes the call chain above a particular function call.
 *
 * Stack traces are not normally heap-allocated, and so usually have more
 * restricted lifetimes.
 */
typedef struct ava_stack_trace_s {
  /**
   * A human-readable indication of the location in code of this particular
   * stack trace element, excluding line number.
   */
  ava_string location;
  /**
   * The line number associated with the location, or -1 if unavailable / not
   * applicable.
   */
  signed line_number;
  /**
   * The stack frame above this one on the stack. The pointee is not guaranteed
   * to have a lifetime that exceeds that of this stack trace element.
   */
  const struct ava_stack_trace_s*restrict next;
} ava_stack_trace;

/**
 * A fixed-size, immutable array of values.
 *
 * Besides being an internal implementation of small lists, arrays are also
 * used as general-purpose tuples within the Avalance Runtime API.
 */
typedef struct {
  /**
   * The number of elements in values.
   */
  unsigned length;
  /**
   * The values in this array. The array is not guaranteed to have a lifetime
   * exceeding the lifetime of the ava_array.
   */
  const ava_value*restrict values;
} ava_array;

/**
 * Function pointer type for directly Avalanche-callable functions.
 *
 * @param return_value An uninitialised value which the function is to populate
 * with its return value. The function MUST populate the value in all
 * code-paths, if it ever returns. The return_value pointer is only guaranteed
 * to be valid for the duration of the function call.
 * @param arguments The arguments being passed to the function. This pointer is
 * only guaranteed to be valid for the duration of the function call.
 * @param context The context value associated with the function. This is used
 * for, eg, closures. The exact format is generally up to the function.
 * @param trace The stack trace used to reach this call. The pointer is only
 * guaranteed to be valid for the duration of the function call.
 */
typedef ava_return_code (*ava_function)(
  ava_value*restrict return_value,
  const ava_array*restrict arguments,
  ava_value context,
  const ava_stack_trace*restrict trace);

/******************** STRING HANDLING ********************/

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
 * Complexity: Amortized O(1)
 */
ava_string ava_string_concat(ava_string, ava_string) AVA_PURE;

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
 * tracks its offset within the rope; for leaves, this is simply the
 * byte/character offset. For Concats, 0 indicates the left branch, and 1 the
 * right branch. (We can't derive the information by looking at the children,
 * because a Concat could have the same rope for both children.)
 *
 * Out-of-bounds iterators are represented by setting the oob flag in addition
 * to placing the iterator at the nearest valid index to the out-of-bounds
 * location.
 *
 * ASCII9 strings are converted into a single rope node stored within the
 * iterator.
 */
typedef struct {
  struct {
    const ava_rope* rope;
    size_t offset;
  } stack[AVA_MAX_ROPE_DEPTH];
  unsigned top;
  size_t real_index, logical_index;
  int oob;
  ava_rope tmprope;
} ava_string_iterator;

/**
 * Initialises the given iterator to point at the given index within the given
 * string.
 *
 * Behaviour is undefined if index >= length(str).
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
 */
void ava_string_iterator_move(ava_string_iterator* it,
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
 * Returns the current logical index of the given string iterator.
 *
 * This will be beyond the string boundaries for invalid iterators.
 */
size_t ava_string_iterator_index(const ava_string_iterator*) AVA_PURE;

AVA_END_DECLS

#endif /* AVA_AVALANCHE_H_ */
