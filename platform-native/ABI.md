Avalanche Native ABI
====================

# Introduction

This document describes the ABI of native Avalanche targets, insofar as it
concerns code generators. Anything which is fully abstracted by a functional
interface without performance loss is not described here. Functions tied to the
ABI are not described in detail here, but rather in the documentation for the
headers that declare them. Other references to headers also imply more detailes
documentation there, which is not generally repeated here.

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
bits can be cleared to derive a pointer to the `ava_ool_string` structure
defined in abi.h.

Note that this allows the data array to be treated as a C-string (though
obviously embedded NULs will cause that string to be effectively truncated).

Standard values with their uniqueness bit set, referencing out-of-line strings,
may be used to modify the string data in-place without coordination of the
runtime and without needing to use any atomic operations. String data,
including that beyond the index given by length, may be accessed through a
standard value at any time without the use of atomic operations.

The memory backing a string is assumed to not alias anything else.

## List

A list is identified by having a clear integer/special discriminator and a type
of 2. The uniqueness bit is meaningful. The lower 4 bits can be cleared to
derive a pointer to the `ava_list` sturcture defined in abi.h.

Mutating a list to reference a single-strand-heap-allocated value which could
have been allocated after the list requires informing the memory manager about
the mutation in certain circumstances (see Memory Management). The garbage
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

Bit 0 of the type field indicates whether the object is
single-strand-heap-allocated. The actual type pointer is found by clearing this
bit.

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

Mutating an object (in certain circumstances) to point to a
single-strand-heap-allocated value which may have been allocated after the
object itself requires informing the memory manager about this operation or
require copying the allocation containing the pointer to another heap. This is
described under Memory Management.

## Pointer-Based / Heap-Allocated Values

The out-of-line string, list, and object standard value types are considered
pointer-based values, and can be converted to pointers to real memory by
zeroing the lower 4 bits if it is known the value is initialised. Reading the
zeroth bit of the intptr_t behind this pointer indicates whether the memory
backing the value is single-strand-heap-allocated.

Expression to test whether a standard value is pointer-based:

```
  is_ptr_based = !(val & 1) && ((val >> 2) & 3);
```

Recording whether an object is single-strand-heap-allocated is necessary to
permit constants to be compiled into the executable, as such objects do not
exist within the garbage collector's memory layout, and to correctly handle
multi-stranded regions in general.

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
allocation (but not one-past-the-end). A raw pointer type also includes the
type of memory allocation it points to, ie, one of the three pointer-based
standard value types.

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

The base address of the page holding a single-strand-heap-allocated object can
be derived by clearing the lower 12 bits. Every page of every managed heap
begins with a qword called the heap-graph table, and then a 64-bit card table
(encoded as a qword). See the `ava_page_header` struct in abi.h for more
details.

## Garbage-Collection Write Barriers

For the below, assume `PTR` is a value known to point to a
single-strand-heap-allocated object, `FLD` is a pointer offset from `PTR` by
some amount that leaves it within `PTR`'s allocation, and `VAL` is some other
value. `page(X)` notates base address of pointer `X`. Note that `page(PTR)`
could be different from `page(FLD)`.

If `FLD` is to be set to `VAL`, and `VAL` could possibly be a
single-strand-heap-allocated value which was allocated *after* `PTR`, the
following must happen iff `VAL` is actually a single-strand-heap-allocated
object:

- The heap-graph table of `page(VAL)` is ORed into the heap-graph table of
  `page(PTR)` (leaving `page(VAL)` untouched).

- The `n`th bit of `page(VAL)`'s card table must be set, where `n` is equal to
  `(FLD >> 6) & 63`. Note that this means for an allocation spanning pages, a
  bit in the card table may refer to multiple locations.

These operations are performed without atomic operations, since they only
affect single-strand heaps which by definition are never mutated by more than
one thread at a time.

Since it is not permitted for a non-single-strand-heap-allocated object to
point to a single-strand-heap-allocated object, mutating code must also check
for the case where `PTR` is not single-strand-heap-allocated and `VAL` is; in
this case, the object behind `PTR` must first be copied into a single-strand
heap.

## Memory Layout

The garbage collector needs to be informed of how certain regions of memory are
interpreted, such as the stack, static storage, and allocated objects of a
non-intrinsic type; this is accomplished with a _memory layout_ table, which is
described under the `ava_immediate_physical_type` enum in abi.h.

## Stack Maps and Safepoints

Functions which could result in a call into the memory management system must
mantain a _stack map_ which allows the garbage collector to manipulate live
variables that live on the call stack. The format of a frame of the stack map
is described in the `ava_stack_map` structure in abi.h.

Upon entry, a function initialises its stack map itself as described on
`ava_stack_map` in abi.h.

The lower two bits of the `stack_map` pointer (a heap handle) passed into
`ava_gc_push_local_heap` as the caller's stack map are used to tag what the
parent heap of the resulting heap handle should be:

- 00: The parent heap is the local heap of the parent handle.
- 01: The parent heap is the parent heap of the parent handle.
- 10: The parent heap is the global heap of the parent handle.

This is encoded this way because these bits are set by the caller of a
function, which knows what escape scope the callee's return value will have.

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
may point into a managed heap. They must be registered with the garbage
collector before any pointers into a heap are written into them. They are
described in more detail with the `ava_static_map` structure in abi.h.

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
`ava_gc_suspend_strand` and `ava_gc_resume_strand` before and after that point.
Both of these are void functions taking a pointer to the caller's stack map as
their only argument. Between these calls, no allocations may be performed using
the suspended stack map.

## Heaps and Heap Handles

Avalanche takes an unusual approach in that it uses a hierarchy of heaps,
rather than a single managed heap.

 Every heap has exactly one parent (except a root heap, which has no parent)
and any number of children. Heaps may contain pointers to other heaps with the
following restrictions:

- A child heap may reference memory in any of its direct or indirect parents
  freely, without notifying the garbage collector, except for the allocation
  order restriction described below.

- A parent heap may reference memory in any of its direct or indirect childern,
  provided it notifies the garbage collector by maintaining the heap graph and
  card table. However, in doing so, the object that contains the reference must
  not itself outlive the child heap it references and, if the child heap is
  single-strand, any use of the allocation in the parent heap must be only by
  the owner of the child heap.

- No heap may reference memory in another heap which is not a direct or
  indirect parent or child.

- No multi-strand heap may reference memory in a single-strand heap.

Additionally, allocations are _ordered_. Even within a single heap, an
allocation may only freely reference other allocations that occurred _before_
it freely. If this is not necessarily the case, the mutator must update the
heap graph and card table.

Any heap is considered either single-strand or multi-strand. A
single-strand heap may only be used by a single strand of execution and marks
its allocations as gc-heap-allocated. A multi-strand heap permits sharing
across strands at the cost of less efficient allocation and garbage collection.
A multi-strand heap must have a multi-strand parent.

A heap handle is a reference to a "local heap", a "parent heap", and a "global
heap". It is represented simply as a pointer to a stack map. Each handle has
its own parent handle, which is independent the parent of any of its heaps. A
strand is assumed to have only one active handle at a time. Whenever a handle
is passed to the memory manager, any handles not in the handle chain are
assumed abandoned and no longer referencable from that strand, which may also
result in destruction of heaps.

Typically, the local heap of a handle is a heap allocated specifically for that
call frame, while the parent heap is chosen by the caller of a function and
used for the function's return value. The global heap is usually the root heap.

## Interaction with the Threading Model

The memory management system is integrated with the threading model, which is
described in more detail in THREADING.md. As far as the ABI goes, code
generators and other low-level code must account for two things:

- Any call into the GC which may rendezvous with a world-stop may cause the
  current strand to be suspended.

- While a strand is suspended, the execution stack may be relocated to another
  memory region. It is therefore not safe to expose pointers into the stack
  to code outside of the current strand except to subsystems expecting to cope
  with relocated strands (eg, the memory manager).
