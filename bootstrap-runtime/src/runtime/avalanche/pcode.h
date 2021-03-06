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
#error "Don't include avalanche/pcode.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_PCODE_H_
#define AVA_RUNTIME_PCODE_H_

#include "defs.h"
#include "string.h"
#include "integer.h"
#include "list.h"
#include "name-mangle.h"
#include "function.h"
#include "macsub.h"

struct ava_struct_s;

/**
   @file

   Avalanche's post-macro-substitution AST is transformed into P-Code, whose
   interface is defined in this file. The P-Code is designed to be both easy to
   transpile to low-level languages like C and to interpret. P-Code objects are
   the primary output for compilation of modules and for linking of modules
   into packages; similarly, they are what the compiler reads in when code
   loads another module or package.

   At top-level, a P-Code object consists of a series of global declarations,
   which may some global name. No executable code is found at global scope;
   rather, the code generator wraps the top-level code in the input file into
   its own function.

   All executable code lives within global functions; there is no such thing as
   nested functions at the P-Code level, so that downstream transpilers and
   interpreters need not implement closures.

   Executable code is defined in terms of a typed-register-stack machine. That
   is, instructions operate upon directly addressed registers; registers are
   pushed into and popped out of existence as necessary. This provides the same
   ease of code generation as a typical stack machine, while allowing a
   downstream register-based code-generator to operate efficiently.

   The virtual machine only exposes three types of mutable locations:
   - Registers private to a function
   - Variables private to a function
   - Global variables

   Global variables are thus the only first-class mutable location recognised
   by the virtual machine which can be observed by multiple threads. The
   virtual machine defines no memory model around global variables. Instead,
   during module initialisation, reading a global variable from a thread other
   than the one initialising the module containing that global variable is
   considered to have undefined behaviour; similarly, mutating a global
   variable after the containing module has initialised or from a thread other
   than the one initialising is considered to have undefined behaviour.

   Registers in the VM are notated by a single letter indicating their type,
   followed by their 0-based index specific to that type. Variables are also
   notated the same way, though they are not, strictly speaking, registers. The
   register types are:
   - Variable, an ava_value.
   - Data, also an ava_value (but anonymous)
   - Integer
   - List
   - Parameter
   - Function
   For example, "i2" denotes the third integer register.

   Registers and variables are required to be initialised before they are used;
   the P-Code is considered invalid if this is not fulfilled.

   The register stacks are _lexically_ scoped, rather than dynamically as with
   a traditional stack machine. For example, the instruction sequence
   ```
     push i 3
     goto out
     pop
     label out
   ```
   pops the three integer registers off the stack when the goto is executed,
   even though the pop instruction itself wouldn't be executed according to
   flow control.

   Conventions for notating P-Code instructions are mostly based on the
   assembly codes of the Intel 8080 and x86, except that "label" is its own
   pseudo-instruction instead of having dedicated syntax, and no commas are
   placed between operands.

   A P-Code object is formatted as an ava_list of global declarations, each of
   which is itself an ava_list, where the first element identifies the type of
   declaration. The meaning and format of the other elements vary by type.

   P-Code objects may describe implementations or interfaces. In the former
   case, they include function and global variable definitions and any
   supporting declarations they need. Interfaces only declare what external
   consumers of the module or package may need. Furthermore, a P-Code object
   may represent a single module, a whole package, or even a whole application,
   the larger forms created by linking (see ava_pcode_linker) one or more
   P-Code objects of the immediately preceding form.

   The following table shows the useful combinations of the above, and their
   conventional names and file extensions:

                Module                  Package         Application
   Impl         .avam                   .avap           .avax
                "Module"                "Fat Package"   "Application"
   Iface        .avami                  .avapi          -
                "[Module] Interface"    "Package"       -

   For interoperation with the underlying platform, the P-Code VM also supports
   direct interaction with strangelets. By nature, the operation of strangelets
   is somewhat vague and entirely unsafe, and so generally should only be found
   in low-level code interacting directly with the platform. Strangelets
   present a second vector for mutation to be observed across threads. Unlike
   with global variables, this is permitted. The P-Code VM's memory model
   matches that of LLVM.

   - In the absence of anything else, memory reads are unordered and
     non-atomic. Note that this means simply reading and writing to an
     unsynchronised thread-shared ava_value field is unsafe and produces
     undefined behaviour.

   - Volatile affects the optimiser *only*; volatile operations cannot be
     reordered with respect to other volatile operations, split, coalesced, or
     deleted. However, there is nothing stopping the underlying architecture
     from doing the same, and thus there are no particular guarantees about
     cross-threaded behaviour. Volatiles are therefore of extremely limited
     utility, especially since their main use --- asynchronous signal handlers
     --- is not available to Avalanche code.

   - Certain field types can be atomically manipulated. Each atomic operation
     is given a memory ordering, which is interpreted at least as strongly as
     LLVM defines them:
     http://llvm.org/docs/LangRef.html#memory-model-for-concurrent-operations

 */

/**
 * Identifies the type of a P-Code register.
 *
 * The register type both defines what data can be stored in and what
 * operations can be performed upon the register, as well as identifying the
 * register set in use. I.e., the register type acts together with the register
 * index to identify the register, such that i0 and d0 are different registers.
 *
 * Register types are usually notated in prose by the first letter of the type;
 * eg, a D-register for ava_prt_data, or an F-register for ava_prt_function.
 */
typedef enum {
  /**
   * A variable pseudo-register. Holds an ava_value.
   *
   * Unlike real registers, variables cannot be created or destroyed.
   *
   * Unless otherwise noted, all operations that can operate on data registers
   * can also operate on variables.
   *
   * Global variables are not variables in this sense.
   */
  ava_prt_var = 0,
  /**
   * A register holding an ava_value.
   *
   * D-registers are primarily used for holding intermediate computation
   * results, serving a similar purpose to the data stack in normal stack
   * machines.
   */
  ava_prt_data,
  /**
   * A register holding an ava_integer.
   *
   * I-registers are almost exclusively used for flow control and support
   * little in the way of arithmetic operations. (Optimisation of operations on
   * ava_values is up to the downstream implementation of the P-Code.)
   */
  ava_prt_int,
  /**
   * A register holding an ava_list.
   *
   * L-registers are used to implement the spread operator, variadic arguments,
   * and similar operations and support very few operations. As with
   * I-registers, any type-based optimisations are up to the lower level.
   */
  ava_prt_list,
  /**
   * A register holding an ava_function_parameter.
   *
   * P-registers are used when constructing calls to functions with dynamic
   * parameter binding.
   */
  ava_prt_parm,
  /**
   * A register holding an ava_function.
   *
   * F-registers are used as targets for dynamic function invocation and
   * binding closures.
   */
  ava_prt_function
} ava_pcode_register_type;

/**
 * Type used for indexing registers.
 */
typedef ava_uint ava_pcode_register_index;

/**
 * Fully identifies a P-Code register.
 */
typedef struct ava_pcode_register_s {
  /**
   * The type of this register.
   */
  ava_pcode_register_type type;
  /**
   * The index of this register within the given type.
   */
  ava_pcode_register_index index;
} ava_pcode_register;

/**
 * Identifies a type of exception visible to P-Code.
 */
typedef enum {
  /** @see ava_user_exception */
  ava_pet_user_exception = 0,
  /** @see ava_error_exception */
  ava_pet_error_exception,
  /** @see ava_undefined_behaviour_exception */
  ava_pet_undefined_behaviour_exception,
  /** @see ava_format_exception */
  ava_pet_format_exception,
  /**
   * Value used for other exception types.
   *
   * This is not a permissible thrown exception type, but is simply used as a
   * stand-in when indicating to P-Code what exception type has been caught.
   */
  ava_pet_other_exception
} ava_pcode_exception_type;

/**
 * Describes a type of read-modify-write operation.
 *
 * These directly correspond to the LLVM atomicrmw operation.
 * http://llvm.org/docs/LangRef.html#i-atomicrmw
 */
typedef enum {
  /**
   * result = new
   */
  ava_pro_xchg,
  /**
   * result = old + new
   */
  ava_pro_add,
  /**
   * result = old - new
   */
  ava_pro_sub,
  /**
   * result = old & new
   */
  ava_pro_and,
  /**
   * result = ~(old & new)
   */
  ava_pro_nand,
  /**
   * result = old | new
   */
  ava_pro_or,
  /**
   * result = old ^ new
   */
  ava_pro_xor,
  /**
   * result = old > new? old : new (signed)
   */
  ava_pro_smax,
  /**
   * result = old < new? old : new (signed)
   */
  ava_pro_smin,
  /**
   * result = old > new? old : new (unsigned)
   */
  ava_pro_umax,
  /**
   * result = old < new? old : new (unsigned)
   */
  ava_pro_umin
} ava_pcode_rmw_op;

/**
 * Describes the minimum memory ordering guarantees for an atomic operation.
 *
 * These correspond at minimum to the options of the same name provided by
 * LLVM. http://llvm.org/docs/LangRef.html#atomic-memory-ordering-constraints
 *
 * Unlike with LLVM, all operations permit all orders everywhere; orders which
 * don't make sense in-context are simply promoted to the next order that does.
 */
typedef enum {
  ava_pmo_unordered,
  ava_pmo_monotonic,
  ava_pmo_acquire,
  ava_pmo_release,
  ava_pmo_acqrel,
  ava_pmo_seqcst
} ava_pcode_memory_order;

/**
 * Converts the given value to a register type.
 *
 * The value must be a single-character string.
 *
 * @throws ava_format_exception if value is not a legal register type.
 */
ava_pcode_register_type ava_pcode_parse_register_type(ava_value value);

/**
 * Converts the given register type to its string representation.
 */
ava_string ava_pcode_register_type_to_string(
  ava_pcode_register_type type) AVA_PURE;

/**
 * Converts the given value to a register identifier.
 *
 * This conversion is strict; no surrounding whitespace is permitted.
 *
 * @throws ava_format_exception if value is not syntactically a legal register
 * identifier.
 */
ava_pcode_register ava_pcode_parse_register(ava_value value);

/**
 * Converts the given register to its string representation.
 */
ava_string ava_pcode_register_to_string(ava_pcode_register reg) AVA_PURE;

/**
 * Converts the given value to a demangled name, according to the format
 * normally used in P-Code files.
 *
 * @throws ava_format_exception if value is not a syntactically valid
 * demangled-name.
 */
ava_demangled_name ava_pcode_parse_demangled_name(ava_value value);

/**
 * Converts the given demangled-name to its P-Code string representation.
 */
ava_string ava_pcode_demangled_name_to_string(ava_demangled_name name) AVA_PURE;

/**
 * Converts the given value to the ava_pcode_rmw_op it names.
 *
 * @throws ava_format_exception if value does not name any ava_pcode_rmw_op.
 */
ava_pcode_rmw_op ava_pcode_parse_rmw_op(ava_value value);

/**
 * Converts the given rmw-op to the string that names it.
 */
ava_string ava_pcode_rmw_op_to_string(ava_pcode_rmw_op op) AVA_PURE;

/**
 * Converts the given value to the ava_pcode_memory_order it names.
 *
 * @throws ava_format_exception if value does not name any
 * ava_pcode_memory_order.
 */
ava_pcode_memory_order ava_pcode_parse_memory_order(ava_value value);

/**
 * Converts the given memory-order to the string that names it.
 */
ava_string ava_pcode_memory_order_to_string(ava_pcode_memory_order order)
AVA_PURE;

#define AVA__IN_PCODE_H_ 1
#include "gen-pcode.h"
#undef AVA__IN_PCODE_H_

#endif /* AVA_RUNTIME_PCODE_H_ */
