Avalanche Native ABI
====================

# Introduction

This document describes the ABI of native Avalanche targets, insofar as it
concerns code generators. Anything which is fully abstracted by a functional
interface without performance loss is not described here. Functions tied to the
ABI are not described in detail here, but rather in the documentation for the
headers that declare them.

The Avalanche ABI is designed primarily with 64-bit architectures in mind. It
is expected to be functional on 32-bit architectures as well, but will be much
less performant.

"Value" here is always used in its most general sense; values as usually meant
in Avalanche jargon are "standard values". Intel terminology is used for
integer sizes; "byte" is an 8-bit integer, "word" is 16-bit, "dword" 32-bit,
and "qword" 64-bit. Bits are numbered starting at 0 for the least significant
bit.

"Natural alignment" of a value means whatever alignment the conventional ABI
for that platform specifies for a value of that type.

# Standard Values

The "standard value" is the immediate-physical type (IPT) most commonly used by
Avalanche code, and describes a dynamically-typed, user-visible value.

A Standard Value is a value at least 64 bits wide. (If pointers are wider than
64 bits, it should be the size of a pointer. No such platforms currently exist
and no effort is going to expended to support them unless one arrives.) It is
layed out as follows:

```
        /-fp-sign
        |/exponent-\/---------------mantissa------------------\
        |---------------------floating-point------------------|
 /-ch0-\|-ch1-\/-ch2-\/-ch3-\/-ch4-\/-ch5-\/-ch6-\/-ch7-\/-ch8+\
 6666555555555544444444443333333333222222222211111111110000000000
 3210987654321098765432109876543210987654321098765432109876543210
 \----------------------integer/pointer---------------------/||||
                                        type-----------------+/||
     uniqueness/floating-point-discriminator-------------------/|
               integer/special-discriminator--------------------/
```

The type and discriminators play together to determine how a standard value is
to be interpreted.

## ASCII9 String

An ASCII9 string is distinguished by the integer/special discriminator being
set and all `ch*` septets either being non-zero or all being zero after the
first one which is zero. It encodes a string of between zero and nine non-NUL
ASCII characters in the 9 `ch*` septet fields, starting with `ch0` and
terminating on the first septet which is zero or after `ch8`, whichever comes
first.

The only other type which is encoded with the integer/special discriminator set
is floating-point. This is arranged such that only `ch0` and the floating-point
discriminator need be examined to differentiate between the two. An ASCII9
string can be identified by detecting that bit 0 is set and the lower 9 bits of
the value when rotated left by 7 bits is not 0x18.

GNU superopt-2.5 produced the following i386 code to perform this test on a
32-bit variant of the same data-format:

```
  roll $7,%eax
  shll $24,%eax
  sbbl $1,%eax
  adcl $1,%eax
  shrl $31,%eax
```

This was verified to be correct via a brute-force search of the entire 32-bit
integer space.

Translated to 64-bit and readable syntax:

```
  rol rax, 7
  shl rax, 56
  sbb rax, 1
  adc rax, 1
  shr rax, 63
```

Translated to C (assuming rol is a macro/inline function):

```
  val = rol(val, 7);
  carry = (val >> 8) & 1;
  val <<= 56;
  addend = -1 - carry;
  carry = (val + addend) > val;
  val += addend;
  val += 1 + carry;
  val >>= 63;
```

(This was also verified as a 32-bit version by brute force over the whole
problem space.)

However, neither GCC5 nor Clang3.7 seems able to translate it back to the
original assembly code. Raw LLVM IR translated from the C gets closer, but is
still twice the size. Using i33/i65 instead of i32/i64 results in a minor
improvement. (The problem seems to lie in the fact that LLVM doesn't realise it
can get the carry from the left-shift from the actual carry bit and fold it
into an `adc`, so instead it does a shift-right and and-1 as stated literally
in the code.)

## Floating-Point

A floating-point standard value is distinguished by the integer/special
discriminator being set, the floating-point discriminator being set, and `ch0`
being equal to zero. The floating-point value itself is encoded in the 55 bits
from bit 2 through bit 56. It is an ordinary IEEE 754 binary64
(double-precision) floating-point value with the lower 9 bits removed (and
zeroed when the floating-point value is expanded to 64 bits). That is, it
consists of a sign bit, 11 exponent bits, and 43 mantissa bits.

Floating-point standard values can be identified by rotating the value left by
7 bits and testing whether the lowest 9 bits are equal to 0x18. Equivalently,

```
  is_float = (val << 7 >> 7) == (val | 3);
```

## Integer

An integer is identified by having a clear integer/special discriminator and a
type of 0. The uniqueness bit is always 0. The upper 60 bits encode a
2's-compliment 60-bit integer which is sign-extended when extracted to a 64-bit
machine register.

Integers larger than 60 bits are represented as objects and handled by runtime
library calls.

## Out-of-Line String

An out-of-line string is identified by having a clear integer/special
discriminator and a type of 1. The uniqueness bit is meaningful. The lower 4
bits can be cleared to derive a pointer to the following structure:

```
  struct out_of_line_string {
    intptr_t capacity;
    intptr_t length;
    char data[capacity];
  };
```

The capacity is always a multiple of 8, and the total allocation size of the
out-of-line string a multiple of 16. length is always strictly less than
capacity. All bytes with an index greater than or equal to length are zero.

Bit 0 of capacity is used to indicate whether the out-of-line string is
gc-heap-allocated.

Note that this allows the data array to be treated as a C-string (though
obviously embedded NULs will cause that string to be effectively truncated).

Standard values with their uniqueness bit set, referencing out-of-line strings,
may be used to modify the string data in-place without coordination of the
runtime and without needing to use any atomic operations. String data,
including that beyond the index given by length, may be accessed through a
standard value at any time without the use of atomic operations.

The garbage collector is permitted to adjust the capacity of an out-of-line
string at safepoints.

The memory backing a string is assumed to not alias anything else.

## List

A list is identified by having a clear integer/special discriminator and a type
of 2. The uniqueness bit is meaningful. The lower 4 bits can be cleared to
derive a pointer to the following structure:

```
  struct list {
    intptr_t capacity;
    intptr_t length;
    standard_value data[capacity];
  };
```

The total allocation size of the list is always a multiple of 16. Note that
this means that a 64-bit pointer implies that capacity will always be even,
while a 32-bit pointer implies that it will always be odd. The low bit of
capacity is therefore used to indicate whether the value is gc-heap-allocated.
length is always less than or equal to capacity.

The first length elements in data are the contents of the list. Entries beyond
length have undefined content. Values may be extracted from a list via a
standard value at any time without the use of atomic operations. A list may be
mutated in-place via a standard value with the uniqueness bit set at any time
and without coordination with the runtime or atomic operations.

The memory backing a list is assumed to not alias anything else.

Mutating a list to reference a gc-heap-allocated value which could have been
allocated after the list requires informing the memory manager about the
mutation in certain circumstances (see Memory Management). The garbage
collector is permitted to adjust the capacity of a list at safepoints.

## Object

An object is identified by having a clear integer/special discriminator and a
type of 3. The uniqueness bit is meaningful. The lower 4 bits can be cleared to
derive a pointer to the following structure:

```
  struct object {
    const object_type* type;
    /* fields */
  };

  struct object_type {
    intptr_t object_size;
    const char* type_name;
    const memory_layout* layout;
    standard function(regional top standard) stringify;
    object_method_definition methods[/*NULL-tierminated*/];
  };

  struct object_method_reference {
    const object_method_id* id;
    function impl; /* function type is method-specific */
  };

  struct object_method_id {
    const char* name;
  };

  /* struct memory_layout is described under Memory Management */
```

At an ABI level, an object is simply a fixed-length array of standard values
and an associated type. The uniqueness bit indicates whether the object may be
*semantically* modified in-place; certain object implementations may memoise
operations and thus are permitted certain classes of in-place mutation even
when non-unique.

Bit 0 of the type field indicates whether the object is gc-heap-allocated. The
actual type pointer is found by clearing this bit.

The type of an object determines its size. The size is always a multiple of 16
bytes. The type name is only used for debugging purposes. The type also
provides first-class access to the method to convert the object to a string
(the notation for the function type is discussed later). Following that is a
table of auxilliary methods the object type implements. A method is identified
by the pointer to its id; the name on the id is used for debugging purposes.

In general, it is assumed that objects are only mutated when the reference is
unique. Thus object pointers are assumed to never alias. Accesses to locations
that may be mutated even under non-uniqueness require volatile memory
operations to prevent the optimiser from making incorrect assumptions.
Similarly, there is no defined behaviour for in-place mutation under
multithreaded access, except for operations specifically designed for such
situations.

Mutating an object (in certain circumstances) to point to a gc-heap-allocated
value which may have been allocated after the object itself requires informing
the memory manager about this operation. This is described under Memory
Management.

## Pointer-Based / Heap-Allocated Values

The out-of-line string, list, and object standard value types are considered
pointer-based values, and can be converted to pointers to real memory by
zeroing the lower 4 bits if it is known the value is initialised. Reading the
zeroth bit of the intptr_t behind this pointer indicates whether the memory
backing the value is gc-heap-allocated.

Expression to test whether a standard value is pointer-based:

```
  is_ptr_based = !(val & 1) && ((val >> 2) & 3);
```

Recording whether an object is gc-heap-allocated is necessary to permit
constants to be compiled into the executable, as such objects do not exist
within the garbage collector's memory layout. Note that "gc-heap-allocated"
here as a somewhat narrower meaning that might be expected; it reflects whether
the object is contained is subject to GC write barriers, and thus may be clear
for some objects actually allocated on a managed heap.

Pointer-based standard values must point to the base address of their backing
allocation. The garbage collector is permitted to adjust any pointer-based
standard value to point to another memory address at any safepoint as long as
the new address implies the same program behaviour. Pointers into the managed
heap which do not point to the head of an allocation are assumed to point to
nothing and may be nulled. Pointers outside the managed heap are retained. The
collector is also permitted to re-set the uniqueness bit during a safepoint in
all circumstances and is permitted to clear it in certain circumstances (as
described in Memory Management).

# Raw Values

There are two immediate-physical types of values termed "raw values".

Raw integers are permitted to hold arbitrary bit patterns and are never
manipulated or otherwise considered by the garbage collector. While termed
"integers", they can conceivably contain other things; for example, they would
be the correct IPT for low-level handling of CUDA device pointers. (Should
someone hypothetically make Avalanche work with CUDA.)

Raw pointers are similar to raw integers that happen to be same size as machine
addresses, but are treated similarly to pointer-based standard values with
respect to memory management. They come in two flavours: Precise raw pointers
either point outside the managed heap or point to the base of an allocated
object. Imprecise raw pointers are allowed to point anywhere within an
allocation (but not one-past-the-end).

# Calling Convention

The "Avalanche calling convention" is a constrained form of the native C
calling convention of the host platform and is defined in terms of it.

An ava-cc function may return void or any of the defined immediate-physical
types. All arguments are of one of the defined IPTs. User functions usually
take and return standard values.

For functions taking two arguments or fewer, the arguments are passed purely
positionally. If it takes fewer than two, additional pointer-sized integers are
passed with undefined values to pad the argument count up to two. Following
those is a pointer to the caller's stack map.

For functions taking between three and eight arguments, the first two arguments
are passed normally, followed by a pointer to the caller's stack map, followed
by the other arguments.

Functions taking over eight arguments work as like functions with between three
and eight, except that a pointer-sized integer is passed in after the eighth
which contains the actual argument count.

The positioning of the stack map pointer is chosen so that it is always passed
to functions in the same register, and not in one of the more highly-contended
ones. This is RDX in the case of the System V AMD64 ABI, R8 for the Microsoft
AMD64 ABI, and x2 on AArch64 (though which register it is matters less there).
The argument-count argument for functions taking more than 8 arguments alows
constructing functions that can proxy to arbitrary other functions. It only
comes into use after 8 arguments so that it does not displace arguments that
would otherwise be passed in registers.

# Memory Management

## Memory Organisation

Avalanche heap memory is divided into 4096-byte pages (regardless of the
underlying platform's page size) and 16-byte blocks. This implies that all
allocations have 16-byte alignment, which is necessary to use the lowest 4 bits
for flags as done with Standard Values, and also permits somewhat more
efficient bulk copies on platforms like AMD64 with 16-byte-aligned register
instructions.

The base address of the page holding a gc-heap-allocated object can be derived
by clearing the lower 12 bits. Every page of every managed heap begins with a
qword called the heap-graph table, and then a 64-bit card table (encoded as a
qword).

## Garbage-Collection Write Barriers

For the below, asume `PTR` is a value known to point to a gc-heap-allocated
object, `FLD` is a pointer offset from `PTR` by some amount that leaves it
within `PTR`'s allocation, and `VAL` is some other value. `page(X)` notates
base address of pointer `X`. Note that `page(PTR)` could be different from
`page(FLD)`.

If `FLD` is to be set to `VAL`, and `VAL` could possibly be a gc-heap-allocated
value which was allocated *after* `PTR`, the following must happen iff `VAL` is
actually a gc-heap-allocated object:

- The heap-graph table of `page(VAL)` is ORed into the heap-graph table of
  `page(PTR)` (leaving `page(VAL)` untouched).

- The `n`th bit of `page(VAL)`'s card-table must be set, where `n` is equal to
  `(FLD >> 12) & 63`.

These operations are performed without atomic operations; if a memory location
is visible to multiple threads, it is not marked as gc-heap-allocated.

## Memory Layout

The garbage collector needs to be informed of how certain regions of memory are
interpreted, such as the stack, static storage, and allocated objects of a
non-intrinsic type; this is accomplished with a _memory layout_ table.

```
struct memory_layout {
  intptr_t num_fields;
  memory_layout_field fields[num_fields];
};

struct memory_layout_field {
  /* C bitfield notation used for convenience.
   * Bits assigned from least to most significant.
   */
  enum char {
    standard_value = 0,
    raw_integer_dword,
    raw_integer_qword,
    raw_pointer_precise,
    raw_pointer_imprecise
  } type : 3;
  char intent_mutate : 1;
  char weak : 1;
  char reserved : 3;
};
```

Fields in a memory region are allocated contiguously, except that padding is
inserted immediately before a field if necessary to achieve standard C ABI
alignment.

`standard_value` indicates a Standard Value as described previously.
`raw_integer_dword` and `raw_integer_qword` describe 32-bit and 64-bit fields,
respectively, which have no particular interpretation. `raw_pointer_precise`
and `raw_pointer_imprecise` describe the two flavours of raw pointers described
previously.

If `intent_mutate` is set (on a standard value), the garbage collector is not
permitted to take any action that would require clearing the uniqueness bit of
the value.

If `weak` is set (on a standard value or pointer IPT), the garbage collector
is not required to retain the pointee if there are no non-weak references to
it. If, after collection, it no longer points to a valid object, the pointer
is reset to point to the `ava_gc_broken_weak_pointer` symbol. If the field is a
standard value, it becomes non-unique and is tagged a second-class object.

The `reserved` bits should be zero.

## Stack Maps and Safepoints

Functions which could result in a call into the memory management system must
mantain a _stack map_ which allows the garbage collector to manipulate live
variables that live on the call stack. The format of a frame of the stack map
is described below.

```
  struct stack_map {
    const memory_layout* layout;
    const stack_map* callers_stack_map;
    void* parent_heap;
    void* local_heap;
    /* Fields described by layout */
  };
```

`callers_stack_map` is a pointer to the stack map of the calling function, if
there is one. This implies that functions necessarily pass their stack maps to
their callees. Only values which are live at a safepoint must actually be
written to the stackmap; temporaries need not be notated. Similarly, there is
no reason to add raw integers to the stack map, since the garbage collector
would simply ignore them.

Functions do not normally initialise a `stack_map` directly. Rather, they pass
the uninitialised `stack_map` along with `layout` and `callers_stack_map` to
`ava_gc_push_local_heap` which takes care of initialising the header fields.
This function returns the `stack_map` pointer itself. By using the return value
instead of the declared `stack_map` itself, the call can be declared pure,
allowing the optimiser to elide the call in functions or paths that do not need
it.

A safepoint in a function occurs whenever another function which could possibly
call into the memory management system or which is such a call itself is made.
At a safepoint, all values local to the function which could possibly be
pointers into the heap must be flushed to the stack map, and reloaded from the
stack map afterwards.

During a safepoint, the garbage collector is permitted to make the following
modifications:

- A pointer-based value which points into a heap may have its address altered
  to point to any equivalent structure.

- The uniqueness bit may be set on a pointer-based standard value.

- Unless tagged `intent_mutate`, a pointer-based standard value may have its
  uniqueness bit cleared.

- Pointer-based values tagged `weak` may be reset to point to a sentinal as
  described above.

- The capacity of a string or list behind a standard value may be adjusted.

Stack maps *must* be allocated on the actual stack; the garbage collector is
permitted to use knowledge of the growth direction of the stack to perform
meaningful pointer comparisons. (Eg, on AMD64, `callers_stack_map` is always at
a greater address than the stack map containing it.)

## Static Maps

Static maps inform the garbage collector about statically-allocated data that
may point into the managed heap. They must be registered with the garbage
collector before any pointers into the heap are written into them.

Static maps have a similar physical format to that of stack maps:

```
  struct static_map {
    const memory_layout* layout;
    const static_map* next;
    /* Fields described by layout */
  };
```

The `next` field is set by the memory manager when the static map is
registered.

## Voluntary Preemption and Suspension

Garbage-collecting the global heap requires stopping all threads of execution.
In order to ensure that any thread can be stopped at a safepoint in a
reasonably short amount of the time, the code generator should emit tests of
the thread-local pointer-sized variable `ava_gc_stop_the_world` and make a call
to `ava_gc_accept_world_stop`, a void function taking the stack map as its only
argument, if it is non-zero. This is only necessary if no other memory manager
functions would be called. The variable is thread-local to permit multiple
Avalanche contexts to exist within one process; it is in fact mutated by other
threads.

If a thread of execution makes a call to a non-trivial native function or does
something else that could block it from responding to world-stops, it must call
`ava_gc_suspend_thread` and `ava_gc_resume_thread` before and after that point.
Both of these are void functions taking a pointer to the caller's stack map as
their only argument. Between these calls, no allocations may be performed using
the suspended stack map.
