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
#ifndef AVA_PLATFORM_NATIVE_ABI_H_
#define AVA_PLATFORM_NATIVE_ABI_H_

#include <stdint.h>

#include "defs.h"

AVA_BEGIN_DECLS

/**
 * @file
 *
 * Provides definitions for operating with Avalanche's native ABI and defines
 * public functions provided by the native runtime which are required for
 * operating with the ABI.
 */

/**
 * Type for 32-bit uninterpreted fields.
 *
 * Use of this type implies by convention that the use site does not impart any
 * interpretation to the bits beyond it being a bit-vector, but is simply
 * moving them around as a unit.
 */
typedef uint32_t ava_dword;
/**
 * Type for 64-bit uninterpreted fields.
 *
 * Use of this type implies by convention that the use site does not impart any
 * interpretation to the bits beyond it being a bit-vector, but is simply
 * moving them around as a unit.
 */
typedef uint64_t ava_qword;
/**
 * Type for Standard Values.
 */
typedef ava_qword ava_stdval;

/**
 * The number of low bits in a Standard Value reserved for flags.
 */
#define AVA_STDV_FLAGS 4
/**
 * The number of bits to arithmetic-right-shift a Standard Value containing an
 * inline integer to derive the actual integer value.
 */
#define AVA_STDV_INT_RSHIFT AVA_STDV_FLAGS
/**
 * The number of bits to left-shift a Standard Value containing an inline real
 * after masking it with AVA_STDV_FLT to extract the embedded double.
 */
#define AVA_STDV_FLOAT_LSHIFT 7
/**
 * The alignment for pointers stored in Standard Values and generally
 * guaranteed by the allocator.
 */
#define AVA_STDALIGN (1 << AVA_STDV_FLAGS)
/**
 * Attribute for C types or values indicating to align them according to
 * AVA_STDALIGN.
 */
#define AVA_ALIGNED __attribute__((__aligned__(AVA_STDALIGN)))

/**
 * Bitmask for the integer/special discriminator bit in a Standard Value.
 */
#define AVA_STDV_ISD ((ava_stdval)0x0000000000000001uLL)
/**
 * Bitmask for the uniqueness bit in a Standard Value.
 */
#define AVA_STDV_UNQ ((ava_stdval)0x0000000000000002uLL)
/**
 * Bitmask for the floating-point discriminator bit in a Standard Value.
 */
#define AVA_STDV_FPD ((ava_stdval)0x0000000000000002uLL)
/**
 * Bitmask for the type of an integer/pointer-based Standard Value.
 */
#define AVA_STDV_TYP ((ava_stdval)0x000000000000000CuLL)
/**
 * Bitmask for the integer payload of inline integer Standard Values.
 */
#define AVA_STDV_INT ((ava_stdval)0xFFFFFFFFFFFFFFF0uLL)
/**
 * Bitmasks for the 9 ch* fields of a Standard Value.
 */
#define AVA_STDV_CH0 ((ava_stdval)0xFE00000000000000uLL)
#define AVA_STDV_CH1 ((ava_stdval)0x01FC000000000000uLL)
#define AVA_STDV_CH2 ((ava_stdval)0x0003F80000000000uLL)
#define AVA_STDV_CH3 ((ava_stdval)0x000007F000000000uLL)
#define AVA_STDV_CH4 ((ava_stdval)0x0000000F70000000uLL)
#define AVA_STDV_CH5 ((ava_stdval)0x000000001FC00000uLL)
#define AVA_STDV_CH6 ((ava_stdval)0x00000000003F8000uLL)
#define AVA_STDV_CH7 ((ava_stdval)0x0000000000007F00uLL)
#define AVA_STDV_CH8 ((ava_stdval)0x00000000000000F7uLL)
/**
 * Bitmask for a floating-point value embedded in a Standard Value.
 */
#define AVA_STDV_FLT ((ava_stdval)0x01FFFFFFFFFFFFF6uLL)
/**
 * Pre-shifted constant (ie, covered by AVA_STDV_TYP) identifying integers.
 */
#define AVA_TYPE_INT ((ava_stdval)0x0000000000000000uLL)
/**
 * Pre-shifted constant (ie, covered by AVA_STDV_TYP) identifying out-of-line
 * strings.
 */
#define AVA_TYPE_STR ((ava_stdval)0x0000000000000004uLL)
/**
 * Pre-shifted constant (ie, covered by AVA_STDV_TYP) identifying lists.
 */
#define AVA_TYPE_LST ((ava_stdval)0x0000000000000008uLL)
/**
 * Pre-shifted constant (ie, covered by AVA_STDV_TYP) identifying objects.
 */
#define AVA_TYPE_OBJ ((ava_stdval)0x000000000000000CuLL)

/**
 * The size of a page of Avalanche heap.
 */
#define AVA_PAGE_SIZE ((uintptr_t)4096)
/**
 * The bitmask to apply to a pointer to the head of an allocation in a managed
 * heap to derive the heap header.
 */
#define AVA_PAGE_HEADER_MASK (~(AVA_PAGE_SIZE-(uintptr_t)1))
/**
 * The bitmask to apply to a pointer to a mutable location in a managed heap to
 * derive its offset within a page.
 */
#define AVA_PAGE_OFFSET_MASK (AVA_PAGE_SIZE-(uintptr_t)1)
/**
 * The shift to apply to a page offset (see AVA_PAGE_OFFSET_MASK) to derive the
 * index of the bit in the page's card table to set in response to mutating an
 * address.
 */
#define AVA_OFFSET_CARDTABLE_SHIFT 6

/**
 * Mask for a memory layout field which reveals the type (an
 * ava_immediate_physical_type).
 */
#define AVA_MEMLAYOUT_TYPE              ((unsigned char)0x07)
/**
 * Mask for a memory layout field for the intent_mutate bit.
 *
 * This only makes sense on memory layout fields of type ava_ipt_stdval. When
 * set, it indicates that, if the value points to a unique allocation, the
 * holder of that value intends to take advantage of the uniqueness, and so the
 * garbage collector shall not take any action that would clear the uniqueness
 * bit.
 */
#define AVA_MEMLAYOUT_INTENT_MUTATE     ((unsigned char)0x08)
/**
 * Mask for a memory layout field for the weak bit.
 *
 * This only makes sense on ava_ipt_stdval, ava_ipt_ptr_precise, and
 * ava_ipt_ptr_imprecise fields. When set, the garbage collector is not
 * required to retain the memory backing the pointer. Furthermore, if the field
 * contains a pointer which after collection does not point to anything, the
 * pointer is reset to `ava_gc_broken_weak_pointer`.
 */
#define AVA_MEMLAYOUT_WEAK              ((unsigned char)0x10)

/**
 * Tag used for the lower two bits of an `ava_stack_map*` passed into a
 * function to indicate that its parent heap should be the local heap of the
 * caller.
 */
#define AVA_HEAP_HANDLE_INHERIT_LOCAL 0
/**
 * Tag used for the lower two bits of an `ava_stack_map*` passed into a
 * function to indicate that its parent heap should be the parent heap of the
 * caller.
 */
#define AVA_HEAP_HANDLE_INHERIT_PARENT 1
/**
 * Tag used for the lower two bits of an `ava_stack_map*` passed into a
 * function to indicate that its parent heap should be the global heap.
 */
#define AVA_HEAP_HANDLE_GLOBAL_PARENT 2

typedef struct ava_memory_layout_s ava_memory_layout;
typedef struct ava_page_header_s ava_page_header;
typedef struct ava_ool_string_s ava_ool_string;
typedef struct ava_list_s ava_list;
typedef struct ava_object_method_id_s ava_object_method_id;
typedef struct ava_object_method_reference_s  ava_object_method_reference;
typedef struct ava_object_type_s ava_object_type;
typedef struct ava_object_s ava_object;
typedef struct ava_gc_heap_s ava_gc_heap;
typedef struct ava_stack_map_s ava_stack_map;
typedef struct ava_static_map_s ava_static_map;

/**
 * Defines an immediate-physical type.
 */
typedef enum {
  /**
   * Describes a value holding an ava_stdval.
   */
  ava_ipt_stdval = 0,
  /**
   * Describes a value of the dword IPT, ie, 32 uninterpreted bits.
   */
  ava_ipt_dword,
  /**
   * Describes a value of the qword IPT, ie, 64 uninterpreted bits.
   */
  ava_ipt_qword,
  /**
   * Describes a value which is always either a pointer to the head of an
   * allocation or points to nothing. The value is sized and aligned as per
   * native pointers.
   */
  ava_ipt_ptr_precise,
  /**
   * Describes a value which is always a pointer to some address within an
   * allocation or points to nothing. The value is sized and aligned as per
   * native pointers.
   */
  ava_ipt_ptr_imprecise
} ava_immediate_physical_type;

/**
 * Describes the layout of a stack_map or a non-intrinsic object type.
 *
 * Such memory is described in terms of a sequence of fields. Fields are
 * arranged contiguously in memory, except that padding may be inserted before
 * a field to give it its native ABI alignment. Padding bytes are always
 * considered uninitialised and may change values spontaneously unless
 * otherwise noted. Allocated memory beyond the final field but before the end
 * of the allocation is considered padding for this purpose.
 */
struct ava_memory_layout_s {
  /**
   * The number of fields described in this layout.
   */
  uintptr_t num_fields;
  /**
   * The fields in this layout. Length is dictated by num_fields.
   */
  unsigned char fields[/*num_fields*/];
};

/**
 * Header found at the base address of every managed heap containing the head
 * of an object allocation.
 */
struct ava_page_header_s {
  /**
   * Used by the garbage collector to track which heaps may reference which
   * other heaps directly or indirectly. Full details of how this is used are
   * found in the documentation for the garbage collector internals. ABI.md
   * describes how clients must maintain this field, in the "Garbage-Collection
   * Write Barriers" section.
   */
  ava_qword heap_graph;
  /**
   * Tracks what locations within the page may have been modified to contain
   * pointers to later allocations. Full details of how this is used are found
   * in the documentation for the garbage collector internals. ABI.md describes
   * how clients must maintain this field, in the "Garbage-Collection Write
   * Barriers" section.
   */
  ava_qword card_table;
};

/**
 * A stack map for a single call frame. This struct is the first element of the
 * struct which contains the safepoint-preserved variables of a single call
 * frame.
 *
 * This is initialised via ava_gc_push_local_heap() and should generally be
 * considered opaque. However, understanding of some of its fields is required
 * anyway, so that is documented here.
 *
 * Note that pointers to this structure also double as heap handles.
 * Furthermore, when passed into a function, the lower two bits are set to one
 * of the AVA_HEAP_HANDLE_* constants to indicate how the parent heap is to be
 * set.
 */
struct ava_stack_map_s {
  /**
   * Describes the layout of fields immediately following the ava_stack_map
   * structure.
   */
  const ava_memory_layout* layout;
  /**
   * The stack map of the caller of the current frame. NULL if this is a root
   * function.
   */
  const ava_stack_map* parent;
  /**
   * The heap into which allocations which are known not to escape the current
   * call frame are made.
   */
  ava_gc_heap* local_heap;
  /**
   * The heap into which allocations which may escape the current call frame
   * via the return value are made.
   */
  ava_gc_heap* parent_heap;
  /**
   * The heap into which allocations which may escape the current call frame
   * via other methods are made.
   */
  ava_gc_heap* global_heap;
};

/**
 * A static map describes a location in static memory (or other non-heap
 * non-stack memory) that may contain pointers into a managed heap. Static maps
 * must be registered with the heap with the ava_gc_add_static() function. That
 * function is responsible for initialising this structure; callers need not do
 * it themselves.
 *
 * The memory described by a static map immediately follows the static map
 * itself; essentially, this structure is the first member of a structure
 * containing the pointers themselves.
 */
struct ava_static_map_s {
  /**
   * The layout of the memory in this static map.
   */
  const ava_memory_layout* layout;
  /**
   * The next static map registered with the same heap.
   */
  ava_static_map* next;
};

/**
 * Structure type for out-of-line strings.
 *
 * Strings are always assumed to alias with nothing else. Memory within the
 * string may safely be read without the use of atomic operations. If a unique
 * reference to the string is held, the string may safely be modified in-place
 * without the use of atomic operations.
 *
 * Strings are not scanned by the garbage collector for pointers, and therefore
 * may also be used as variably-sized arrays of arbitrary scalar data.
 */
struct ava_ool_string_s {
  /**
   * The capacity, in chars, of data. This is always a multiple of 8. If
   * offsetof(ava_ool_string,data) is 16, it is also always a multiple of 16;
   * otherwise, it is always a multiple of 16 plus eight. It is always greater
   * than or equal to length.
   *
   * The zeroth bit indicates whether this object is allocated in a
   * single-threaded managed heap. This bit must be zeroed before capacity is
   * used for comparisons.
   *
   * The capacity may be adjusted by the garbage collector at safepoints.
   */
  uintptr_t capacity;
  /**
   * The number of chars in data, including the terminating NUL. Note that
   * strings are permitted to contain NUL characters; the inclusion of a
   * terminating NUL permits them to be used as C strings cheaply (though with
   * no handling of the embedded NUL).
   */
  uintptr_t length;
  /**
   * The data for this string. The qword type indicates its allocation size and
   * alignment. Characters are stored in natural byte order, regardless of the
   * endianness of the system, so the qwords cannot as a whole be compared to
   * determine lexicographical ordering as perceived by user code.
   *
   * All chars at at indices beyond length which lie in a qword that contains
   * at least one initialised character are initialised to zero. qwords beyond
   * that limit are considered uninitialised and may change values
   * spontaneously. They become stabilised simply by increasing length to
   * include them.
   */
  ava_qword data[];
} AVA_ALIGNED;

/**
 * Offset to add to ANDed list capacity. See ava_list.capacity for more
 * details.
 */
#define AVA_LIST_CAPACITY_OFF (sizeof(uintptr_t) != sizeof(ava_stdval))

/**
 * Structure type for intrinsic lists.
 *
 * Lists are always assumed to alias with nothing else. Memory within the list
 * may safely be read without the use of atomic operations. If a unique
 * reference to the list is held, the list may safely be modified in-place
 * without the use of atomic operations.
 */
struct ava_list_s {
  /**
   * The capacity of the data array, in values. It is always greater than or
   * equal to length. This is always a multiple of two plus
   * AVA_LIST_CAPACITY_OFF.
   *
   * The zeroth bit is used to indicate whether this object is allocated in a
   * single-threaded managed heap. The actual capacity is found by setting bit
   * zero to AVA_LIST_CAPACITY_OFF.
   *
   * The capacity may be adjusted by the garbage collector at safepoints.
   */
  uintptr_t capacity;
  /**
   * The number of used values in the data array.
   */
  uintptr_t length;
  /**
   * The values in this list. Of the length indicated by capacity. Values at
   * indices equal to or greater than length are considered uninitialised and
   * may change spontaneously (but become stable simply by increasing length to
   * include them).
   */
  ava_stdval data[];
} AVA_ALIGNED;

/**
 * Identifies an object method.
 *
 * The data within a method id is not normally interpreted; the pointer value
 * itself is used as the key.
 */
struct ava_object_method_id_s {
  /**
   * A human-readable name for this method, used for debugging.
   */
  const char* name;
};

/**
 * Describes a method implemented by an object type.
 *
 * A reference with both fields NULL indicates the end of the method table.
 */
struct ava_object_method_reference_s {
  /**
   * The identity of the method implemented.
   */
  const ava_object_method_id* id;
  /**
   * Pointer to the method implementation. The actual type of the function is
   * depndent on the method.
   */
  void (*impl)();
};

/**
 * Describes the type of an object.
 */
struct ava_object_type_s {
  /**
   * The size of the object, in bytes. Need not be a multiple of the allocation
   * size; extra bytes beyond size are considered padding.
   */
  intptr_t size;
  /**
   * The layout of this object.
   *
   * This may be NULL, describing an atomic object size bytes wide. That is,
   * the first size bytes of the allocation are considered to be uninterpreted
   * non-padding values by the garbage collector, and thus have stable values
   * but may not contain managed pointers.
   */
  const ava_memory_layout* layout;
  /**
   * Converts this object into its string form.
   *
   * This must be a pure function; ie, given the same object, it must produce
   * the same string value. Furthermore, it must be reversable for any object
   * type which can be converted from other values.
   *
   * @param self A pointer to the object to stringify. May not have wider
   * escape scope than regional.
   * @return A standard value containing the string representation of this
   * object.
   */
  ava_stdval (*stringify)(const ava_object* self, uintptr_t _padding,
                          const ava_stack_map* caller_handle);
  /**
   * The human-readable name of this type, for debugging purposes.
   */
  const char* type_name;
  /**
   * Table of auxilliary methods implemented by this value.
   *
   * The array terminates on the first reference with a NULL value for either
   * field.
   */
  ava_object_method_reference methods[];
};

AVA_END_DECLS

#endif /* AVA_PLATFORM_NATIVE_ABI_H_ */
