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
#error "Don't include avalanche/pcode-validation.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_PCODE_VALIDATION_H_
#define AVA_RUNTIME_PCODE_VALIDATION_H_

/**
 * @file
 *
 * Provides facilities for validating P-Code, as well as transforming P-Code
 * into an extended form termed X-Code, which is not serialisable but permits
 * interesting static analysis.
 *
 * The execution model of X-Code is mostly the same as P-Code, except for the
 * following:
 *
 * - There are no register stacks. Instead, each function gets a fixed-size
 *   register file. Different instances of the same register in P-Code become
 *   different X-Code registers.
 *
 * - Functions are decomposed into basic blocks, and jumps target block indices
 *   rather than labels.
 */

#include "pcode.h"
#include "errors.h"
#include "map.h"

/**
 * A basic block is a sequence of instructions which are necessarily executed
 * in sequence and unconditionally.
 *
 * X-Code basic blocks are very similar to those in LLVM, except that it is
 * permissible to "fall off the end" of a basic block and continue into the
 * linearly next basic block. It is also permissible to jump to the zeroth
 * basic block.
 *
 * LLVM-style "phi nodes" (which represent the transfer of state across basic
 * blocks) are implicit; registers are implicitly transfered if they exist in
 * both source and destination (ie, no renaming occurs).
 */
typedef struct {
  /**
   * Tracks the phi-node information for this basic block. Each variable is a
   * bitset keyed by register index plus type offset.
   *
   * phi_iinit tracks whether each register is definitely initialised upon
   * entry to the block. Initially, all blocks have all 1s here for all
   * registers that exist upon entry, except for the initial block, which has
   * all 0s except for the argument registers.
   *
   * phi_oinit tracks whether each register is definitely initialised upon exit
   * from the block. Initially, all blocks have all 0s here except for
   * registers they initialise themselves.
   *
   * Eventually, the phi_iinit of a block is equal to the bitwise AND of all
   * phi_oinit of all basic blocks that may continue to it.
   *
   * phi_effect tracks which registers are affected by this basic block.
   * Changes from phi_iinit propagate to phi_oinit except for registers whose
   * bit is set in phi_effect; ie,
   *   phi_oinit = (phi_iinit &~ phi_effect) | (phi_oinit & phi_effect)
   *
   * A bit in phi_effect may be set where phi_oinit is clear. This indicates
   * that the block *destroys* the register. Registers are destroyed by not
   * existing or, in the case of P-registers, being passed to a function.
   *
   * phi_iexist and phi_oexist simply track whether each register exists upon
   * entry and exit of the basic block.
   */
  ava_ulong* phi_iinit, * phi_oinit, * phi_effect, * phi_iexist, * phi_oexist;

  /**
   * The indices of the basic blocks to which execution may continue after this
   * basic block. A value of -1 indicates an empty slot.
   *
   * If the final block can fall of the end, this is also indicated as -1.
   */
  ava_sint next[2];

  /**
   * The number of instructions in this basic block.
   */
  size_t length;
  /**
   * The instructions in this basic block.
   */
  const ava_pcode_exe* elts[];
} ava_xcode_basic_block;

/**
 * An X-Code function is comprised of a fixed set of registers and some number
 * of basic blocks.
 *
 * Execution begins with the zeroth basic block. Should execution fall off the
 * end of the final block, the empty string is returned.
 *
 * Note that the companion ava_pcg_fun object is generally required for
 * additional context, such as the number of arguments.
 */
typedef struct {
  /**
   * Indicates the offset of each register type, ie, the minimum index of that
   * type after all registers have been uniquified. ava_prt_var is always
   * offset 0. The number of registers of a type can be found with
   * reg_type_off[type+1] - reg_type_off[type].
   */
  ava_pcode_register_index reg_type_off[ava_prt_function+2];

  /**
   * The number of ulongs in each phi bitset.
   */
  size_t phi_length;
  /**
   * The number of basic blocks in this function.
   */
  size_t num_blocks;
  /**
   * The basic blocks in this function.
   */
  ava_xcode_basic_block* blocks[];
} ava_xcode_function;

/**
 * A pcode global element associated with a possible X-Code function.
 */
typedef struct {
  const ava_pcode_global* pc;
  ava_xcode_function* fun;
} ava_xcode_global;

/**
 * A global P-Code list, with associated X-Code functions and flattened into an
 * array for simpler access.
 */
typedef struct ava_xcode_global_list_s {
  /**
   * The number of elements in this list.
   */
  size_t length;
  /**
   * The elements in this list.
   */
  ava_xcode_global elts[];
} ava_xcode_global_list;

/**
 * Converts the given P-Code to X-Code and validates it.
 *
 * If no values are added to errors by the time this function returns, the
 * X-Code is known to be free of internal consistency issues. This means that
 * all global references point to locations that do exist and which make sense
 * for the referent, that only existent registers are referenced and all
 * registers are initialised before use, and that jump targets actually exist.
 *
 * @param pcode The P-Code to restructure and validate.
 * @param errors List to which any errors encountered are added.
 * @param sources Map from filename to full source, in order to produce
 * ava_compile_locations with a present source. If an entry is missing, the
 * error will have an absent source.
 * @return The X-Code produced by structuring the P-Code. A non-NULL return
 * value does not guarantee success, but merely indicates that the P-Code vas
 * sufficiently valid to produce such a structure. If any errors are emitted,
 * entries within the X-Code globals or anything it references may be NULL even
 * if this is not permitted on a valid object.
 */
ava_xcode_global_list* ava_xcode_from_pcode(
  const ava_pcode_global_list* pcode,
  ava_compile_error_list* errors,
  ava_map_value sources);

/**
 * Returns a bit corresponding to the given register within an xcode phi
 * bitset.
 */
static inline ava_bool ava_xcode_phi_get(
  const ava_ulong* phi, size_t ix
) {
  return (phi[ix / 64] >> (ix % 64)) & 1;
}

/**
 * Sets a bit corresponding to the given register within an xcode phi bitset.
 */
static inline void ava_xcode_phi_set(
  ava_ulong* phi, size_t ix, ava_bool val
) {
  phi[ix / 64] &= ~(1uLL << (ix % 64));
  phi[ix / 64] |= ((ava_ulong)val) << (ix % 64);
}

#endif /* AVA_RUNTIME_PCODE_VALIDATION_H_ */
