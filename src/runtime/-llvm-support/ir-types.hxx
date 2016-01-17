/*-
 * Copyright (c) 2015, 2016, Jason Lingle
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
#ifndef AVA_RUNTIME__LLVM_SUPPORT_IR_TYPES_HXX_
#define AVA_RUNTIME__LLVM_SUPPORT_IR_TYPES_HXX_

#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>

namespace ava {
  /**
   * Determines the common types (both avalanche and native) to be used for
   * generating code within the given module.
   *
   * The module must already have the ISA driver loaded into it, since it is
   * consulted to learn about the target C ABI (most importantly size_t, which
   * cannot otherwise be determined).
   */
  struct ir_types {
    ir_types(llvm::LLVMContext& context,
             const llvm::Module& layout) noexcept;

    llvm::Type* c_void;
    llvm::IntegerType* c_byte;
    llvm::IntegerType* c_short;
    llvm::IntegerType* c_int;
    llvm::IntegerType* c_long;
    llvm::IntegerType* c_llong;
    llvm::IntegerType* c_size;
    llvm::IntegerType* c_atomic;
    llvm::IntegerType* c_intptr;
    llvm::Type* c_float;
    llvm::Type* c_double;
    llvm::Type* c_ldouble;
    llvm::PointerType* c_string;
    llvm::IntegerType* ava_bool;
    llvm::IntegerType* ava_byte;
    llvm::IntegerType* ava_short;
    llvm::IntegerType* ava_int;
    llvm::IntegerType* ava_long;
    llvm::IntegerType* ava_integer;
    llvm::Type* ava_real;
    /**
     * The way ava_string appears in most places.
     */
    llvm::Type* ava_string;
    /**
     * The way ava_string appears as a struct member.
     *
     * For some reason this is different than ava_string.
     */
    llvm::StructType* ava_string_wrapped;
    llvm::StructType* ava_twine;
    llvm::StructType* ava_twine_tail;
    llvm::StructType* ava_twine_tail_other;
    llvm::VectorType* ava_value;
    llvm::IntegerType* ava_function_parameter_type;
    llvm::StructType* ava_function_parameter;
    llvm::StructType* ava_attribute;
    llvm::StructType* ava_attribute_tag;
    llvm::StructType* ava_fat_list_value;

    llvm::StructType* ava_function;
    llvm::PointerType* ava_function_ptr;
    llvm::StructType* ava_argument_spec;
    llvm::StructType* ava_argument_binding;
    llvm::StructType* ava_c_marshalling_type;
    llvm::StructType* ava_pointer_prototype;
    llvm::StructType* ava_exception;

    llvm::PointerType* general_pointer;
    llvm::StructType* opaque;

    enum {
      ff_address = 0, ff_calling_convention, ff_c_return_type,
      ff_num_args, ff_args, ff_ffi
    };

    enum {
      asf_marshal = 0, asf_binding
    };

    enum {
      abf_type = 0, abf_name, abf_value
    };

    enum {
      cmtf_primitive_type = 0, cmtf_pointer_proto
    };

    enum {
      ppf_header = 0, ppf_tag, ppf_is_const
    };

    enum {
      aaf_tag = 0, aaf_next
    };

    enum {
      tf_body = 0, tf_length, tf_tail
    };

    enum {
      ttf_overhead = 0, ttf_other
    };
  };
}

#endif /* AVA_RUNTIME__LLVM_SUPPORT_IR_TYPES_HXX_ */
