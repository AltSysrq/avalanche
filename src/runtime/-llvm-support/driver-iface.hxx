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
#ifndef AVA_RUNTIME__LLVM_SUPPORT_DRIVER_IFACE_HXX_
#define AVA_RUNTIME__LLVM_SUPPORT_DRIVER_IFACE_HXX_

#include <string>

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>

namespace ava {
  /**
   * Provides access to functions provided by drivers the code generator must
   * interact with.
   *
   * Interface fields beginning with g_ are ISA interfaces for
   * ava_pcode_globals. Interface fields beginning with x_ are ISA interfaces
   * for ava_pcode_exes. Note that there isn't a 1:1 correspondence between ISA
   * interfaces and P-Code instructions. In particular, instructions which have
   * no effects or alter control flow do not have interfaces at all; the
   * various register movement instructions are decomposed into load, store,
   * and convert; static invocation is performed directly by the code generator
   * since it cannot be expressed efficiently in C.
   *
   * All ISA interfaces correspond to functions whose name is of the form
   * `ava_isa_INTERFACE$`.
   */
  struct driver_iface {
    /**
     * Inspects the given module and extracts the driver interfaces from it.
     *
     * If an error occurs, the error member is set to a non-empty string.
     */
    driver_iface(const llvm::Module& module) noexcept;

    typedef llvm::Function* F;

    /**
     * If non-empty, an error was detected in a driver, rendering a mandatory
     * interface point unavailable.
     */
    std::string error;

    /**
     * Implements runtime semantics of the ext-var global type.
     *
     * Signature: void (const ava_value* src, ava_string name)
     *
     * src is a pointer to the global variable; name is its mangled name.
     */
    F g_ext_var;
    /**
     * Implements runtime semantics of the ext-fun global type for functions
     * with the Avalanche calling convention.
     *
     * Signature: void (const ava_function* fun, ava_string name)
     *
     * fun is a pointer to the global describing this function. name is the
     * mangled name from which the function was resolved.
     */
    F g_ext_fun_ava;
    /**
     * Implements runtime semantics of the ext-fun global type for functions
     * with non-Avalanche calling conventions.
     *
     * Signature: void (ava_function* fun, ava_string name)
     *
     * fun is a pointer to the global describing this function. name is the
     * mangled name from which the function was resolved.
     *
     * This call must initialise the FFI on fun, and do so in a way that will
     * not interfere with other threads already using it (ie, if another module
     * already initialised the shared location).
     */
    F g_ext_fun_other;
    /**
     * Implements runtime semantics of the var global type.
     *
     * Signature: void (ava_value* var, ava_string name, ava_bool publish)
     *
     * var is a pointer to the variable, which has already been initialised to
     * the empty string. name is its mangled name. publish corresponds to the
     * publish field of the P-Code.
     */
    F g_var;
    /**
     * Implements runtime semantics of the fun global type. (All fun types
     * refer to functions with the Avalanche calling convention.)
     *
     * Signature: void (const ava_function* fun, ava_string name,
     *                  ava_bool publish)
     *
     * fun is a pointer to the global describing this function. name is its
     * mangled name. publish corresponds to the publish field of the P-Code.
     */
    F g_fun_ava;

    /**
     * Reads a value from a variable register.
     *
     * Signature: ava_value (const ava_value* src, ava_string name)
     *
     * Returns *src.
     *
     * src is a pointer to the variable. name is the unmangled, fully-qualified
     * name of the variable.
     */
    F x_load_v;
    /**
     * Reads a value from a data register.
     *
     * Signature: ava_value (const ava_value* src, size_t ix)
     *
     * Returns *src
     *
     * src is a pointer to the data register. ix is the index of the data
     * register, after X-Code register renaming.
     */
    F x_load_d;
    /**
     * Reads a value from an int register.
     *
     * Signature: ava_integer (const ava_integer* src, size_t ix)
     *
     * Returns *src
     *
     * src is a pointer to the int register. ix is the index of the data
     * register, after X-Code register renaming.
     */
    F x_load_i;
    /**
     * Reads a value from a function register.
     *
     * Signature: const ava_function* (const ava_function*const* src, size_t ix)
     *
     * Returns *src
     *
     * src is a pointer to the function register. ix is the index of the
     * function register, after X-Code register renaming.
     */
    F x_load_f;
    /**
     * Reads a value from a list register.
     *
     * Signature: void (ava_fat_list_value* dst, const ava_fat_list_value* src,
     *                  size_t ix)
     *
     * Sets *dst = *src
     *
     * src is a pointer to the list register, dst is a pointer to the desired
     * location of the copy. dst may equal src. ix is the index of the list
     * register, after X-Code register renaming.
     */
    F x_load_l;
    /**
     * Reads a value from a global var or ext-var.
     *
     * Signature: ava_value (const ava_value* var, ava_string name)
     *
     * Returns *var
     *
     * var is a pointer to the variable. name is its mangled name.
     */
    F x_load_glob_var;
    /**
     * Reads a value from a global fun or ext-fun.
     *
     * Signature: ava_value (const ava_function* fun, ava_string name)
     *
     * Returns a value containing fun.
     *
     * fun is a pointer to the function descriptor. name is its mangled name.
     */
    F x_load_glob_fun;
    /**
     * Stores a value into a local variable.
     *
     * Signature: void (ava_value* dst, ava_value src, ava_string name)
     *
     * Sets *dst = src
     *
     * dst is a pointer to the local variable. src is the new value to write to
     * it. name its its fully-qualified unmangled name.
     */
    F x_store_v;
    /**
     * Stores a value into a data register.
     *
     * Signature: void (ava_value* dst, ava_value src, size_t ix)
     *
     * Sets *dst = src
     *
     * dst is a pointer to the data register. src is the new value to write to
     * it. ix is the index of the data register, after X-Code register
     * renaming.
     */
    F x_store_d;
    /**
     * Stores a value into an int register.
     *
     * Signature: void (ava_integer* dst, ava_integer src, size_t ix)
     *
     * Sets *dst = src
     *
     * dst is a pointer to the int register. src is the new value to write to
     * it. ix is the index of the int register, after X-Code register renaming.
     */
    F x_store_i;
    /**
     * Stores a value into a function register.
     *
     * Signature: void (const ava_function** dst, const ava_function* src,
     *                  size_t ix)
     *
     * Sets *dst = src
     *
     * dst is a pointer to the function register. src is the new value to write
     * to it. ix is the index of the function register, after X-Code register
     * renaming.
     */
    F x_store_f;
    /**
     * Stores a value into a list register.
     *
     * Signature: void (ava_fat_list_value* dst, const ava_fat_list_value* src,
     *                  size_t ix)
     *
     * Sets *dst = *src
     *
     * dst is a pointer to the list register. src is a pointer to the new value
     * to write to it. dst and src may be equal. ix is the index of the list
     * register, after X-Code register renaming.
     */
    F x_store_l;
    /**
     * Stores a value into a parm register.
     *
     * Signature: void (ava_function_parameter* dst,
     *                  ava_value val,
     *                  ava_function_parameter_type type,
     *                  size_t ix)
     *
     * Sets dst->value = val, dst->type = type
     *
     * dst is a pointer to the parm register. val is the new value to store in
     * the register. type is the parameter type, always one of ava_fpt_static
     * or ava_fpt_spread. ix is the index of the parm register, after X-Code
     * register renaming.
     */
    F x_store_p;
    /**
     * Stores a value into a global variable defined in this module.
     *
     * Signature: void (ava_value* dst, ava_value src, ava_string name)
     *
     * Sets *dst = src
     *
     * dst is a pointer to the global register. src is the new value to write
     * to it. name is the mangled name of the variable.
     */
    F x_store_glob_var;
    /**
     * Converts an integer to a value.
     *
     * Signature: ava_value (ava_integer i)
     */
    F x_conv_vi;
    /**
     * Converts a value to an integer, defaulting to 0 and allowing any
     * exceptions to propagate.
     *
     * Signature: ava_integer (ava_value v)
     */
    F x_conv_iv;
    /**
     * Converts a function to a value.
     *
     * Signature: ava_value (const ava_function* f)
     */
    F x_conv_vf;
    /**
     * Converts a value to a function, allowing any exceptions to propagate.
     *
     * Signature: const ava_function* (ava_value v)
     */
    F x_conv_fv;
    /**
     * Converts a list to a value.
     *
     * Signature: ava_value (const ava_fat_list_value* l)
     */
    F x_conv_vl;
    /**
     * Converts a value to a list, allowing any exceptions to propagate.
     *
     * Signature: void (ava_fat_list_value* dst, ava_value v)
     */
    F x_conv_lv;
    /**
     * Implements the lempty P-Code exe.
     *
     * Signature: void (ava_fat_list_value* dst)
     *
     * Sets *dst to the empty list.
     */
    F x_lempty;
    /**
     * Implements the lappend P-Code exe.
     *
     * Signature: void (ava_fat_list_value* dst, const ava_fat_list_value* src,
     *                  ava_value val)
     *
     * Appends val to *src and stores the result in *dst. src and dst may be
     * equal.
     */
    F x_lappend;
    /**
     * Implements the lcat P-Code exe.
     *
     * Signature: void (ava_fat_list_value* dst,
     *                  const ava_fat_list_value* left,
     *                  const ava_fat_list_value* right)
     *
     * Concatenates *left and *right and stores the result in *dst. dst, left,
     * and/or right may all be equal.
     */
    F x_lcat;
    /**
     * Implemetns the lhead P-Code exe.
     *
     * Signature: ava_value (const ava_fat_list_value* dst)
     *
     * The first element of dst is returned, barring exceptional cases.
     */
    F x_lhead;
    /**
     * Implements the lbehead P-Code exe.
     *
     * Signature: void (ava_fat_list_value* dst, const ava_fat_list_value* src)
     *
     * *dst is set to *src minus its first element, barring exceptional cases.
     * dst and src may be equal.
     */
    F x_lbehead;
    /**
     * Implements the lflatten P-Code exe.
     *
     * Signature: void (ava_fat_list_value* dst, const ava_fat_list_value* src)
     *
     * *dst is set to *src flattened. dst and src may be equal.
     */
    F x_lflatten;
    /**
     * Implements the lindex P-Code exe.
     *
     * Signature: ava_value (const ava_fat_list_value* list, ava_integer ix)
     *
     * The ixth value in *list is returned, barring exceptional cases.
     */
    F x_lindex;
    /**
     * Implements the llength P-Code exe.
     *
     * Signature: ava_integer (const ava_fat_list_value* list)
     *
     * Returns the length of *list
     */
    F x_llength;
    /**
     * Sums two integers.
     *
     * Signature: ava_integer (ava_integer a, ava_integer b)
     *
     * Returns a + b. The result of overflow is undefined.
     */
    F x_iadd;
    /**
     * Implements the icmp P-Code exe.
     *
     * Signature: ava_integer (ava_integer a, ava_integer b)
     */
    F x_icmp;
    /**
     * Implements the aaempty P-Code exe.
     *
     * Signature: void (ava_value v)
     */
    F x_aaempty;
    /**
     * Pre-static-invocation hook.
     *
     * Signature: void (const ava_function* f, ava_string name)
     *
     * f is the descriptor for the function about to be invoked. name is its
     * mangled name.
     *
     * This function is invoked immediately prior to the function call in an
     * invoke-ss or invoke-sd instruction and has no defined semantics.
     */
    F x_pre_invoke_s;
    /**
     * Post-static-invocation hook.
     *
     * Signature: void (const ava_function* f, ava_string name,
     *                  ava_value returned)
     *
     * f is the descriptor for the function that was invoked. name is its
     * mangled name. returned is the value it returned.
     *
     * This function is invoked immediately after a function call in an
     * invoke-ss or invoke-sd instruction and has no defined semantics. Note
     * that it is only called if the function actually returns normally.
     */
    F x_post_invoke_s;
    /**
     * Binds parameters to arguments for invoke-sd.
     *
     * Signature: void (ava_value*restrict args,
     *                  const ava_function*restrict fun,
     *                  const ava_function_parameter*restrict parms,
     *                  size_t num_parms)
     *
     * args is the destination array of values for arguments bound to the
     * function. fun is the descriptor of the funtion whose arguments are to be
     * bound. parms is an array of P-Registers being passed to the function.
     * num_parms is the length of the parms array.
     *
     * This call has the same semantics as ava_function_force_bind(). Any
     * exceptions are allowed to propagate.
     */
    F x_invoke_sd_bind;
    /**
     * Implements the invoke-dd P-Code exe.
     *
     * Signature: ava_value (const ava_function*restrict fun,
     *                       const ava_function_parameter*restrict parms,
     *                       size_t num_parms)
     *
     * fun is the function to invoke. parms is an array of P-Registers being
     * passed to the function. num_parms is the number of parameters being
     * passed to the function.
     *
     * Any exceptions are allowed to propagate.
     */
    F x_invoke_dd;
    /**
     * Implements the partial P-Code exe.
     *
     * Signature: const ava_function* (const ava_function*restrict fun,
     *                                 const ava_value* args,
     *                                 size_t count)
     *
     * fun is the function to apply partially. args is the array of D-registers
     * being bound to arguments. count is the length of the args array.
     */
    F x_partial;
    /**
     * Implements the bool P-Code exe.
     *
     * Signature: ava_integer (ava_integer i)
     *
     * Returns !!i
     */
    F x_bool;

    /**
     * If the module defines a `\program-entry`, a pointer to that function;
     * otherwise, NULL.
     *
     * Signature: void (void)
     *
     * If defined, the code generator should place its module initialisation
     * logic into this function instead of creating its own. Note that the
     * function will most likely have the incorrect linkage since the C code
     * must declare it extern.
     */
    F program_entry;
  };
}

#endif /* AVA_RUNTIME__LLVM_SUPPORT_DRIVER_IFACE_HXX_ */
