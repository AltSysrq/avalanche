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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cassert>

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/TypeBuilder.h>

#include "ir-types.hxx"

ava::ir_types::ir_types(llvm::LLVMContext& context,
                        const llvm::Module& module)
noexcept {
  const llvm::Function* abi_info_function;
  const llvm::StructType* abi_info;
  enum { abi_char = 0, abi_short, abi_int, abi_long,
         abi_llong, abi_size, abi_atomic, abi_intptr, abi_ldouble,
         abi_abool, abi_fpt, abi_fun, abi_argspec,
         abi_twine, abi_str, abi_parm, abi_fat_list,
         abi_exception };

  abi_info_function = module.getFunction("ava_c_abi_info_get$");
  assert(abi_info_function);
  abi_info = llvm::cast<const llvm::StructType>(
    llvm::cast<const llvm::PointerType>(
      abi_info_function->getArgumentList().front().getType())
    ->getElementType());

  c_void = llvm::TypeBuilder<void,true>::get(context);
  c_byte = llvm::cast<llvm::IntegerType>(abi_info->getElementType(abi_char));
  c_short = llvm::cast<llvm::IntegerType>(abi_info->getElementType(abi_short));
  c_int = llvm::cast<llvm::IntegerType>(abi_info->getElementType(abi_int));
  c_long = llvm::cast<llvm::IntegerType>(abi_info->getElementType(abi_long));
  c_llong = llvm::cast<llvm::IntegerType>(abi_info->getElementType(abi_llong));
  c_size = llvm::cast<llvm::IntegerType>(abi_info->getElementType(abi_size));
  c_atomic = llvm::cast<llvm::IntegerType>(abi_info->getElementType(abi_atomic));
  c_intptr = llvm::cast<llvm::IntegerType>(abi_info->getElementType(abi_intptr));
  c_float = llvm::TypeBuilder<llvm::types::ieee_float,true>::get(context);
  c_double = llvm::TypeBuilder<llvm::types::ieee_double,true>::get(context);
  c_ldouble = abi_info->getElementType(abi_ldouble);
  c_string = llvm::TypeBuilder<llvm::types::i<8>*,true>::get(context);
  ava_bool = llvm::cast<llvm::IntegerType>(abi_info->getElementType(abi_abool));
  ava_byte = llvm::TypeBuilder<llvm::types::i<8>,true>::get(context);
  ava_short = llvm::TypeBuilder<llvm::types::i<16>,true>::get(context);
  ava_int = llvm::TypeBuilder<llvm::types::i<32>,true>::get(context);
  ava_long = llvm::TypeBuilder<llvm::types::i<64>,true>::get(context);
  ava_integer = ava_long;
  ava_real = c_double;
  /* We may need to at some point somehow note that this could contain a
   * pointer.
   */
  ava_string = ava_long;
  ava_string_wrapped = llvm::cast<llvm::StructType>(
    abi_info->getElementType(abi_str));
  ava_twine = llvm::cast<llvm::StructType>(abi_info->getElementType(abi_twine));
  ava_twine_tail = llvm::cast<llvm::StructType>(
    ava_twine->getElementType(tf_tail));
  ava_twine_tail_other = llvm::cast<llvm::StructType>(
    ava_twine_tail->getElementType(ttf_other));
  ava_fat_list_value = llvm::cast<llvm::StructType>(
    abi_info->getElementType(abi_fat_list));

  ava_function_parameter_type =
    llvm::cast<llvm::IntegerType>(abi_info->getElementType(abi_fpt));
  ava_function_parameter = llvm::cast<llvm::StructType>(
    abi_info->getElementType(abi_parm));

  ava_function = llvm::cast<llvm::StructType>(
    abi_info->getElementType(abi_fun));
  ava_function_ptr = llvm::PointerType::getUnqual(ava_function);
  ava_argument_spec = llvm::cast<llvm::StructType>(
    abi_info->getElementType(abi_argspec));
  ava_argument_binding = llvm::cast<llvm::StructType>(
    ava_argument_spec->getElementType(asf_binding));
  ava_c_marshalling_type = llvm::cast<llvm::StructType>(
    ava_function->getElementType(ff_c_return_type));
  ava_pointer_prototype = llvm::cast<llvm::StructType>(
    llvm::cast<llvm::PointerType>(
      ava_c_marshalling_type->getElementType(cmtf_pointer_proto))
    ->getElementType());
  ava_attribute = llvm::cast<llvm::StructType>(
    ava_pointer_prototype->getElementType(ppf_header));
  ava_attribute_tag = llvm::cast<llvm::StructType>(
    llvm::cast<llvm::PointerType>(ava_attribute->getElementType(aaf_tag))
    ->getElementType());
  ava_value = llvm::cast<llvm::VectorType>(
    ava_argument_binding->getElementType(abf_value));

  ava_exception = llvm::cast<llvm::StructType>(
    abi_info->getElementType(abi_exception));

  general_pointer = llvm::TypeBuilder<llvm::types::i<8>*,true>::get(context);
  opaque = llvm::StructType::create(context, "S_ext_bss_opaque");
}
