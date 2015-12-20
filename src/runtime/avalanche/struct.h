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
#error "Don't include avalanche/struct.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_STRUCT_H_
#define AVA_RUNTIME_STRUCT_H_

#include "string.h"
#include "value.h"

struct ava_pointer_prototype_s;

/**
 * Abstractly, a struct describes a tuple with named entries, where each entry
 * is constrained to a sub-range of some normalised format.
 *
 * Structs are used for two purposes:
 * - Describing simple binary formats. This is referred to as "binary mode"
 *   below.
 * - Interacting with software on the underlying platform. This is referred to
 *   as "native mode" below.
 *
 * To the latter end, the struct system was designed to accomidate three
 * particular types of underlying platform:
 *
 * - Memory-oriented systems like C. Values are contiguous regions of raw
 *   memory. Field access is performed by adding an offset to a pointer and
 *   reading the memory at that location. Types are unchecked and need not be
 *   declared. Struct extension is expressed by making the parent struct the
 *   first composite member of the child. Struct composition is directly
 *   supported, as are immediate arrays and immediate variable-length arrays.
 *   Pointers are word-sized integers and thus can be extracted either to true
 *   values or to strangelets.
 *
 * - Object-oriented systems like the JVM. Values are references to named
 *   tuples. Field access is performed by using a platform-specific method to
 *   request the platform access a field within a given reference. Types are
 *   checked and must be declared (i.e., for a struct to exist, it must have a
 *   physical class). Struct extension is directly expressed to the platform.
 *   Struct composition does not exist, nor do true arrays, but they can be
 *   approximated through additional references. Pointers are not directly
 *   accessible and therefore can only be extracted to strangelets.
 *
 * - Map-oriented systems like Javascript. Values are maps. Field access is
 *   performed by indexing the map using the field's name. Types do not exist.
 *   Struct extension is implemented by adding the child's fields to the map
 *   represting the parent. Neither struct composition nor arrays exist, though
 *   they can be approximated via additional references and platform lists,
 *   respectively. Pointers are not directly accessible and therefore can only
 *   be extracted to strangelets.
 *
 * Note that by its very nature the use of structs to interact with the
 * underlying platform is inherently unsafe. Such usage is not intended for use
 * in every-day Avalanche code, but only for writing code to bridge the gap
 * between Avalanche and what lies under it.
 *
 * Padding used to fulfill alignment restrictions in binary mode always follows
 * the item being padded. For example, a one-byte element followed by an
 * element with four-byte alignment is represented as:
 * - one-byte element
 * - three bytes of padding
 * - element with alignment 4
 *
 * Within the context of Avalanche, a "struct" always refers to the descriptor,
 * and not to a value formatted by it. A string in the binary format of a
 * struct is termed a "binary string"; a strangelet accessed via a struct is
 * termed a "struct-bound strangelet" when it is necessary to distinguish it
 * from other types of strangelets.
 *
 * An ava_struct is itself a value. It is represented as a list of lists. The
 * first list is the struct descriptor; each list thereafter describes one
 * field.
 *
 * The struct descriptor is a list of two or three elements. The first element
 * is either the keyword "struct" or "union", which dictates the is_union field
 * of ava_struct. Next is the name of the struct (ava_struct.name). If the next
 * element is present, it is interpreted as an ava_struct describing the parent
 * struct. Unions cannot extend other structs or be themselves extended since
 * the meaning in binary form is unclear and they may not be representable in
 * certain native backends.
 *
 * Each field descriptor begins with a keyword indicating the type and ends
 * with the field name. Elements between are particular to each type and
 * described in the documentation for those types themselves.
 */
typedef struct ava_struct_s ava_struct;

/**
 * Special value for the `alignment` fields of a struct field which indicates
 * that the field is to use its natural alignment.
 *
 * In native mode, this describes the alignment typically used by the ABI (if
 * alignment is even meaningful). In binary mode, uses the defined "natural
 * alignment" of the field type.
 */
#define AVA_STRUCT_NATURAL_ALIGNMENT 14

/**
 * Special value for the `alignment` fields of a struct field which indicates
 * that the field is to use the native alignment (rather than natural
 * alignment) even in binary mode.
 *
 * If the platform does not define alignment, equivalent to
 * AVA_STRUCT_NATURAL_ALIGNMENT.
 */
#define AVA_STRUCT_NATIVE_ALIGNMENT 15

/**
 * Describes the size of an integer.
 *
 * Integers are truncated to the specified bit-width when stored into such a
 * struct field and are expanded on read.
 *
 * The natural alignment of an integer is equal to its size.
 *
 * The string representation of each integer type is the text of the entry name
 * here after "ava_sis_", with underscores replaced with hyphens.
 */
typedef enum {
  /**
   * An integer type equivalent to the general Avalanche integer
   * representation.
   */
  ava_sis_ava_integer = 0,
  /**
   * The natural integer type on the underlying platform (even in binary mode)
   * which supports atomic operations.
   *
   * If there is no such concept, equivalent to ava_sis_ava_integer.
   */
  ava_sis_word,
  /**
   * An 8-bit integer.
   */
  ava_sis_byte,
  /**
   * A 16-bit integer.
   */
  ava_sis_short,
  /**
   * A 32-bit integer.
   */
  ava_sis_int,
  /**
   * A 64-bit integer.
   */
  ava_sis_long,
  /**
   * If the platform defines a "short" integer type, an integer of that size,
   * even in binary mode. Otherwise equivalent to ava_sis_short.
   */
  ava_sis_c_short,
  /**
   * If the platform defines an "int" integer type, an integer of that size,
   * even in binary mode. Otherwise equivalent to ava_sis_int.
   */
  ava_sis_c_int,
  /**
   * If the platform defines a "long" integer type, an integer of that size,
   * even in binary mode. Otherwise equivalent to ava_sis_long.
   */
  ava_sis_c_long,
  /**
   * If the platform defines a "long long" integer type, an integer of that
   * size, even in binary mode. Otherwise equivalent to ava_sis_long.
   */
  ava_sis_c_llong,
  /**
   * If the platform defines a "size_t" integer type, or has a particular
   * integer type used to describe the length of an array, an integer of that
   * size, even in binary mode. Otherwise equivalent to ava_sis_long.
   */
  ava_sis_c_size,
  /**
   * If pointers are directly accessible, an integer of the same size as a
   * machine pointer. Otherwise equivalent to ava_sis_long.
   */
  ava_sis_c_intptr
} ava_struct_int_size;

/**
 * Describes the size and precision of a floating-point field.
 *
 * Values are truncated or expanded appropriately upon field read and write.
 * Note that use of floating-points larger than ava_real is of rather limited
 * utility since the extra precision gets discarded; the only real use is to
 * interoperate with native APIs that use such fields in structs.
 *
 * The natural alignment of of a real is 4 bytes in binary mode.
 *
 * Floating-point values are assumed to be in IEEE 754 unless stated otherwise.
 *
 * The string representation of this enum is the text in the each entry's name
 * after "ava_srs_", with underscores replaced by hyphens.
 */
typedef enum {
  /**
   * A floating-point value of the same precision as ava_real.
   *
   * Equivalent to ava_srs_double in binary mode.
   */
  ava_srs_ava_real = 0,
  /**
   * The smallest supported floating-point size of at least 32 bits.
   *
   * In binary mode, must be exactly 32 bits.
   */
  ava_srs_single,
  /**
   * The smallest supported floating-point size of at least 64 bits.
   *
   * In binary mode, must be exactly 64 bits.
   */
  ava_srs_double,
  /**
   * The largest supported floating-point size in practical use on the
   * platform. This is not necessarily an IEEE 754 floating-point type.
   *
   * Binary mode matches native mode.
   */
  ava_srs_extended
} ava_struct_real_size;

/**
 * Indicates the high-level type of a field.
 *
 * The string representation of the field type is the text after "ava_sft_" in
 * each entry in the enum.
 */
typedef enum {
  /**
   * Used for all primitive integer types.
   */
  ava_sft_int = 0,
  /**
   * Used for all floating-point types.
   */
  ava_sft_real,
  /**
   * Indicates a field which always holds a raw pointer or an arbitrary invalid
   * pointer-sized value.
   *
   * The tag of the pointer is implicit in the field and has no representation
   * in the actual value.
   *
   * Pointers always use native byte-order and alignment. They are supported in
   * binary mode, but always use the native interpretations (64-bit
   * little-endian if there is no native interpretation).
   */
  ava_sft_ptr,
  /**
   * Indicates a field which either stores a pointer (as with ava_sft_ptr)
   * zero-extended to sizeof(ava_integer), or an ava_integer with bit 0 set.
   *
   * This is mainly useful for constructs like ava_string.
   *
   * As with pointers, these always use native byte-order and alignment (the
   * greater of the two unioned values), even in binary mode.
   *
   * A zero-initialised hybrid is considered to contain a null pointer.
   */
  ava_sft_hybrid,
  /**
   * Indicates a field which holds a raw ava_value.
   *
   * Whether this makes any sense in binary mode is entirely
   * platform-dependent.
   *
   * To be clear, this does not hold the actual value. For example, in the C
   * ABI it is simply a double-quadword in native byte-order and with 16-byte
   * alignment.
   */
  ava_sft_value,
  /**
   * Indicates a field whose structure is defined by another struct.
   *
   * That is, the sub-structure is always present and fundamentally associated
   * with its container, its identity intrinsically tied to the container. This
   * is equivalent to embedding one struct in another in C.
   *
   * In binary mode, the struct's binary string is included in-line with the
   * natural alignment of that struct.
   *
   * The sub-struct must be composable.
   */
  ava_sft_compose,
  /**
   * Indicates a field whose structure is defined by another struct, repeated a
   * fixed number of times.
   *
   * An array is roughly equivalent to a compose field repeated $length times,
   * except that it is possible to use dynamic indices to access the elements.
   *
   * All notes from the ava_sft_compose type apply to array as well.
   */
  ava_sft_array,
  /**
   * Like array, but the length is not fixed but is rather determined by the
   * total size of the object, the tail continuing on till the end.
   *
   * Note that no facility is provided to determine the run-time length of a
   * tail array in native mode, since this is impossible to determine in
   * memory-oriented systems.
   *
   * This must be the final element in a struct, and makes the struct
   * non-composable. It cannot be used in a union, since this is both difficult
   * to support and can be more flexibly represented as an array of union
   * instances.
   *
   * Other than length not being meaningful and the restrictions above, this
   * otherwise behaves the same as ava_sft_array.
   */
  ava_sft_tail
} ava_struct_field_type;

/**
 * Describes the byte-order of an integer or floating-point type.
 *
 * The string representation of this enum is the text after "ava_sbo_" in each
 * entry.
 */
typedef enum {
  /**
   * Indicates to use the "preferred" byte-order for the field. In native mode,
   * this is the native byte-order, if there is such a thing. In binary mode,
   * always little-endian.
   */
  ava_sbo_preferred = 0,
  /**
   * Forces a value to be encoded little-endian (least significant byte first).
   */
  ava_sbo_little,
  /**
   * Forces a value to be encoded big-endian (most significant byte first).
   */
  ava_sbo_big,
  /**
   * Indicates to always use the native byte-order for the field, even in
   * binary mode. If there is no native byte-order, indicates little-endian.
   */
  ava_sbo_native
} ava_struct_byte_order;

/**
 * Describes a single field within a struct.
 */
typedef struct ava_struct_field_s {
  /**
   * The general type of this field.
   */
  ava_struct_field_type type;
  /**
   * The name of this field.
   *
   * All fields in a struct (including its parent structs) must have unique
   * names.
   */
  ava_string name;
  /**
   * The byte offset of this field in binary mode.
   */
  size_t offset;

  /**
   * Type-specific data about this field.
   */
  union {
    /**
     * Information for ava_sft_int.
     *
     * These extra fields are encoded as list elements between the type and
     * name as follows:
     * - size: the string representation of the ava_struct_int_size entry.
     * - sign_extend: integer
     * - is_atomic: integer
     * - alignment: integer
     * - byte_order: the string representation of the ava_struct_byte_order
     *   entry.
     */
    struct {
      /**
       * The ava_struct_int_size identifying the size of this integer.
       */
      /* ava_struct_int_size */ unsigned size : 4;
      /**
       * Whether the integer should be size-extended when expanded to an
       * ava_integer.
       */
      unsigned sign_extend : 1;
      /**
       * Whether the integer needs to support atomic operations in native mode.
       * This requires alignment to be natural or native, byte_order to be
       * preferred or native, and size to be ava_sis_word.
       */
      unsigned is_atomic : 1;
      /**
       * Indicates the exponent (applied to 2) for the byte alignment of this
       * field.
       *
       * AVA_STRUCT_NATIVE_ALIGNMENT and AVA_STRUCT_NATURAL_ALIGNMENT are
       * special values. Note that this means alignments greater than 2**13
       * (8192 bytes) are not supported.
       */
      unsigned alignment : 4;
      /**
       * The byte (ava_struct_byte_order) order for this field.
       *
       * This value is not meaningful in native mode if the platform does not
       * expose byte-order.
       */
      /* ava_struct_byte_order */ unsigned byte_order : 2;
    } vint;
    /**
     * Information for ava_sft_real.
     *
     * The extra fields are encoded between the type and name of a field
     * descriptor as follows:
     * - size: The string representation of ava_struct_real_size.
     * - alignment: integer
     * - byte_order: The string representation of ava_struct_byte_order.
     */
    struct {
      /**
       * The size (ava_struct_real_size) of this field.
       */
      /* ava_struct_real_size */ unsigned size : 2;
      /**
       * The alignment of this field.
       *
       * @see ava_struct_field.v.vint.alignment
       */
      unsigned alignment : 4;
      /**
       * The byte-order for this field.
       *
       * This value is not meaningful in native mode if the platform does not
       * expose byte-order.
       */
      /* ava_struct_byte_order */ unsigned byte_order : 2;
    } vreal;
    /**
     * Information for ava_sft_ptr and ava_sft_hybrid.
     *
     * For ava_sft_ptr, the extra fields are encoded between the type and name
     * of the field descriptor as follows:
     * - prot: the tag of the prototype
     * - is_atomic: integer
     *
     * For ava_sft_hybrid, prot is placed between thet ype and name of the
     * field descriptor as its tag.
     */
    struct {
      /**
       * The pointer prototype to use for pointers read from this field.
       *
       * If the platform needs to declare pointer types, the type in the
       * pointer tag identifies the name of the struct to declare.
       */
      const struct ava_pointer_prototype_s* prot;
      /**
       * Whether this field needs to support atomic operations. This is only
       * honoured for ava_sft_ptr.
       */
      ava_bool is_atomic;
    } vptr;
    /**
     * Information for ava_sft_compose, ava_sft_array, and ava_sft_tail.
     *
     * For ava_sft_compose and ava_sft_tail, the value representation of member
     * is placed between the type and name of the field descriptor. For
     * ava_sft_array, the array_length is included as an additional element
     * after member's element as an integer.
     */
    struct {
      /**
       * The composed struct.
       */
      const ava_struct* member;
      /**
       * For ava_sft_array, the number of members in the array.
       *
       * This always indicates the "natural length" of the field, including for
       * ava_sft_compose (where it is always 1) and ava_sft_tail (where it is
       * always 0).
       */
      size_t array_length;
    } vcompose;
  } v;
} ava_struct_field;

struct ava_struct_s {
  /**
   * The name of this struct.
   *
   * If the platform requires physical manifestations of types to be created,
   * this identifies that manifestation (eg, a class on the JVM).
   */
  ava_string name;
  /**
   * The parent struct of this struct, if any.
   *
   * Regardless of platform, it is always guarenteed to be meaningful to
   * interpret a struct's value as if it were an instance of its parent struct.
   *
   * In binary mode, the parent is placed before all fields in the child
   * struct.
   */
  const ava_struct* parent;

  /**
   * The total size, in bytes, of this struct in binary mode.
   */
  size_t size;
  /**
   * The alignment, in bytes, of this struct in binary mode.
   */
  unsigned alignment;
  /**
   * Whether it is legal to compose this struct inside another.
   */
  ava_bool is_composable;
  /**
   * Whether this struct is actually a union. In a union, all fields are placed
   * at the starting offset rather than one after the other.
   *
   * Note that in a union there is no way to tell which field is actually being
   * used. The effect of reinterpreting a union with one field populated as
   * another field is unspecified, but never results in a runtime error, even
   * in native mode.
   */
  ava_bool is_union;

  /**
   * The length of the fields array.
   */
  size_t num_fields;
  /**
   * The fields in this structure.
   */
  ava_struct_field fields[/*num_fields*/];
};

/**
 * Embeds the given struct within an ava_value.
 */
ava_value ava_value_of_struct(const ava_struct* sxt) AVA_CONSTFUN;
/**
 * Returns the ava_struct embedded within the given value, or converts it if it
 * is not currently a struct.
 *
 * @throw ava_format_exception if the value is not a valid struct.
 */
const ava_struct* ava_struct_of_value(ava_value val);

#endif /* AVA_RUNTIME_STRUCT_H_ */
