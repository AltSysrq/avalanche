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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cstdlib>
#include <list>
#include <vector>
#include <memory>
#include <utility>
#include <set>
#include <string>

#include <llvm/IR/Attributes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Linker/Linker.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>

#define AVA__INTERNAL_INCLUDE 1
#include "../avalanche/defs.h"
AVA_BEGIN_DECLS
#include "../avalanche/string.h"
#include "../avalanche/value.h"
#include "../avalanche/list.h"
#include "../avalanche/pcode.h"
#include "../avalanche/pcode-validation.h"
#include "../avalanche/errors.h"
#include "../avalanche/name-mangle.h"
AVA_END_DECLS

#include "ir-types.hxx"
#include "translation.hxx"

AVA_BEGIN_FILE_PRIVATE

struct ava_xcode_translation_context {
  ava_xcode_translation_context(llvm::LLVMContext& llvm_context,
                                llvm::Module& module);

  llvm::LLVMContext& llvm_context;
  llvm::Module& module;
  const ava::ir_types types;
  llvm::GlobalVariable* string_type;
  llvm::GlobalVariable* pointer_pointer_impl;
  llvm::GlobalVariable* pointer_prototype_tag;
  llvm::Constant* empty_string, * empty_string_value;
  llvm::Constant* pointer_prototype_header;

  std::map<size_t,llvm::GlobalVariable*> global_vars;
  std::map<size_t,llvm::Function*> global_funs;
};

static bool ava_xcode_to_ir_load_driver(
  llvm::Linker& dst,
  std::set<std::string>& declared_symbols,
  const std::vector<char>& driver_bitcode,
  llvm::LLVMContext& llvm_context,
  std::string& error) noexcept;

static void ava_xcode_handle_driver_functions(
  llvm::Module& module,
  std::set<std::string>& declared_symbols) noexcept;

static bool ava_xcode_to_ir_declare_global(
  ava_xcode_translation_context& context,
  std::set<std::string>& declared_symbols,
  const ava_pcode_global* pcode,
  size_t ix,
  std::string& error) noexcept;

static bool ava_xcode_to_ir_declare_fun(
  ava_xcode_translation_context& context,
  std::set<std::string>& declared_symbols,
  const ava_function* prototype,
  const ava_demangled_name& name,
  llvm::GlobalValue::LinkageTypes linkage,
  bool will_be_defined, size_t ix,
  std::string& error) noexcept;

static llvm::Type* translate_marshalling_type(
  const ava_xcode_translation_context& context,
  const ava_c_marshalling_type& type) noexcept;

static llvm::Constant* get_pointer_prototype_constant(
  const ava_xcode_translation_context& context,
  const ava_pointer_prototype* prototype) noexcept;

static llvm::Constant* get_ava_string_constant(
  const ava_xcode_translation_context& context,
  ava_string string) noexcept;

static llvm::Constant* get_ava_value_constant(
  const ava_xcode_translation_context& context,
  ava_value value) noexcept;

static llvm::Constant* get_marshal_value(
  const ava_xcode_translation_context& context,
  const ava_c_marshalling_type& type) noexcept;

AVA_END_FILE_PRIVATE

void ava::xcode_to_ir_translator::add_driver(
  const char* data, size_t size)
noexcept {
  drivers.push_back(std::vector<char>(data, data + size));
}

std::unique_ptr<llvm::Module> ava::xcode_to_ir_translator::translate(
  const ava_xcode_global_list* xcode,
  const char* module_name,
  llvm::LLVMContext& llvm_context,
  std::string& error)
const noexcept {
  std::unique_ptr<llvm::Module> module(
    new llvm::Module(llvm::StringRef(module_name),
                     llvm_context));
  std::unique_ptr<llvm::Module> error_ret(nullptr);
  std::set<std::string> declared_symbols;
  llvm::Linker linker(module.get());

  for (const auto& driver: drivers) {
    if (!ava_xcode_to_ir_load_driver(
          linker, declared_symbols, driver, llvm_context, error)) {
      return error_ret;
    }
  }

  ava_xcode_handle_driver_functions(*module, declared_symbols);
  ava_xcode_translation_context context(llvm_context, *module);

  for (size_t i = 0; i < xcode->length; ++i) {
    if (!ava_xcode_to_ir_declare_global(
          context, declared_symbols, xcode->elts[i].pc, i, error)) {
      return error_ret;
    }
  }

  return std::move(module);
}

AVA_BEGIN_FILE_PRIVATE

static bool ava_xcode_to_ir_load_driver(
  llvm::Linker& dst,
  std::set<std::string>& declared_symbols,
  const std::vector<char>& driver_bitcode,
  llvm::LLVMContext& llvm_context,
  std::string& error)
noexcept {
  llvm::SMDiagnostic diagnostic;
  std::unique_ptr<llvm::MemoryBuffer> buffer(
    llvm::MemoryBuffer::getMemBuffer(
      llvm::StringRef(&driver_bitcode[0], driver_bitcode.size()),
      "", false));

  std::unique_ptr<llvm::Module> driver_module(
    llvm::ParseIR(buffer.get(), diagnostic, llvm_context));

  if (!driver_module.get()) {
    error = diagnostic.getMessage();
    return false;
  }

  /* returns *true* on error */
  if (dst.linkInModule(driver_module.get(), llvm::Linker::DestroySource,
                       &error))
    return false;

  return true;
}

static void ava_xcode_handle_driver_functions(
  llvm::Module& module,
  std::set<std::string>& declared_symbols)
noexcept {
  /* Adjust linkage, visibility, and attributes of all functions that aren't
   * "main".
   */
  for (auto& fun: module) {
    if (fun.getName() != "main" && !fun.empty()) {
      fun.addFnAttr(llvm::Attribute::AlwaysInline);
      fun.setUnnamedAddr(true);
      fun.setVisibility(llvm::GlobalValue::DefaultVisibility);
      fun.setLinkage(llvm::GlobalValue::PrivateLinkage);
    }

    declared_symbols.insert(fun.getName());
  }
}

static bool ava_xcode_to_ir_declare_global(
  ava_xcode_translation_context& context,
  std::set<std::string>& declared_symbols,
  const ava_pcode_global* pcode,
  size_t ix,
  std::string& error)
noexcept {
  switch (pcode->type) {
  case ava_pcgt_ext_var: {
    const ava_pcg_ext_var* v = (ava_pcg_ext_var*)pcode;
    llvm::GlobalVariable* var = new llvm::GlobalVariable(
      context.module, context.types.ava_value,
      true, llvm::GlobalValue::ExternalLinkage,
      nullptr, ava_string_to_cstring(ava_name_mangle(v->name)));
    context.global_vars[ix] = var;
  } break;

  case ava_pcgt_var: {
    const ava_pcg_var* v = (ava_pcg_var*)pcode;
    llvm::GlobalVariable* var = new llvm::GlobalVariable(
      context.module, context.types.ava_value,
      false,
      v->publish? llvm::GlobalValue::ExternalLinkage :
                  llvm::GlobalValue::InternalLinkage,
      context.empty_string_value,
      ava_string_to_cstring(ava_name_mangle(v->name)));
    context.global_vars[ix] = var;
  } break;

  case ava_pcgt_ext_fun: {
    const ava_pcg_ext_fun* v = (ava_pcg_ext_fun*)pcode;

    return ava_xcode_to_ir_declare_fun(
      context, declared_symbols,
      v->prototype, v->name, llvm::GlobalValue::ExternalLinkage,
      false, ix, error);
  } break;

  case ava_pcgt_fun: {
    const ava_pcg_fun* v = (ava_pcg_fun*)pcode;

    return ava_xcode_to_ir_declare_fun(
      context, declared_symbols,
      v->prototype, v->name,
      v->publish? llvm::GlobalValue::ExternalLinkage :
                  llvm::GlobalValue::InternalLinkage,
      true, ix, error);
  } break;

  case ava_pcgt_load_pkg: {
    /* TODO */
    abort();
  } break;

  case ava_pcgt_load_mod: {
    /* TODO */
    abort();
  } break;

  case ava_pcgt_src_pos:
  case ava_pcgt_export:
  case ava_pcgt_macro:
  case ava_pcgt_init:
    break;
  }

  return true;
}

ava_xcode_translation_context::ava_xcode_translation_context(
  llvm::LLVMContext& llvm_context_,
  llvm::Module& module_)
: llvm_context(llvm_context_),
  module(module_),
  types(llvm_context_, module_)
{
  string_type = new llvm::GlobalVariable(
    module,
    types.ava_attribute, true, llvm::GlobalValue::ExternalLinkage,
    nullptr, "ava_string_type");

  empty_string = llvm::ConstantInt::get(types.ava_string, 1);
  empty_string_value = llvm::ConstantVector::get(
    { llvm::ConstantExpr::getPtrToInt(string_type, types.ava_attribute),
      empty_string });

  pointer_prototype_tag = new llvm::GlobalVariable(
    module, types.ava_attribute_tag, true, llvm::GlobalValue::ExternalLinkage,
    nullptr, "ava_pointer_prototype_tag");
  pointer_pointer_impl = new llvm::GlobalVariable(
    module, types.ava_attribute, true, llvm::GlobalVariable::ExternalLinkage,
    nullptr, "ava_pointer_pointer_impl");
  pointer_prototype_header = llvm::ConstantStruct::get(
    types.ava_attribute,
    pointer_prototype_tag,
    llvm::ConstantExpr::getBitCast(
      pointer_pointer_impl, types.ava_attribute->getElementType(1)),
    nullptr);
}

static bool ava_xcode_to_ir_declare_fun(
  ava_xcode_translation_context& context,
  std::set<std::string>& declared_symbols,
  const ava_function* prototype,
  const ava_demangled_name& name,
  llvm::GlobalValue::LinkageTypes linkage,
  bool will_be_defined,
  size_t ix,
  std::string& error)
noexcept {
  llvm::Function* fun;
  llvm::StringRef mangled_name = ava_string_to_cstring(ava_name_mangle(name));

  llvm::Type* return_type = ava_cc_ava == prototype->calling_convention?
    context.types.ava_value :
    translate_marshalling_type(context, prototype->c_return_type);

  llvm::Type* arg_types[prototype->num_args];
  size_t actual_arg_count;

  if (ava_cc_ava == prototype->calling_convention) {
    if (prototype->num_args <= AVA_CC_AVA_MAX_INLINE_ARGS) {
      actual_arg_count = prototype->num_args;
      for (size_t i = 0; i < prototype->num_args; ++i)
        arg_types[i] = context.types.ava_value;
    } else {
      actual_arg_count = 2;
      arg_types[0] = context.types.c_size;
      arg_types[1] = llvm::PointerType::getUnqual(context.types.ava_value);
    }
  } else {
    actual_arg_count = 0;
    for (size_t i = 0; i < prototype->num_args; ++i) {
      if (ava_cmpt_void != prototype->args[i].marshal.primitive_type) {
        arg_types[actual_arg_count++] =
          translate_marshalling_type(
            context, prototype->args[i].marshal);
      }
    }
  }

  llvm::FunctionType* fun_type = llvm::FunctionType::get(
    return_type, llvm::ArrayRef<llvm::Type*>(arg_types, actual_arg_count),
    false);

  /* Only declare the function itself if not already defined. (A driver may
   * have defined it already.)
   */
  fun = context.module.getFunction(mangled_name);
  if (!fun) {
    fun = llvm::Function::Create(
      fun_type, linkage,
      mangled_name, &context.module);

    if (will_be_defined)
      fun->setUnnamedAddr(true);
  } else {
    assert(!will_be_defined);
  }
  context.global_funs[ix] = fun;

  /* Global constant ava_function referencing this function.
   *
   * For functions with internal linkage, the constant also has internal
   * linkage.
   *
   * For external-linked functions with the Avalanche calling convention, the
   * constant is a true constant, and we use linkonce_odr, since there is no
   * FFI to initialise.
   *
   * For other external-linked functions with other calling conventions, we
   * also make the global linkonce_odr. However, it is necessary to initialise
   * the FFI, and there is no definitive owner of the function since the
   * function lives in an Avalanche-unaware library. Thus, each module
   * reinitialises the FFI, taking care to do it in a way that doesn't
   * interfere with concurrent usages of that data.
   */
  llvm::GlobalValue::LinkageTypes funvar_linkage =
    llvm::GlobalValue::InternalLinkage == linkage?
    llvm::GlobalValue::InternalLinkage :
    llvm::GlobalValue::LinkOnceODRLinkage;

  llvm::Constant* argspecs[prototype->num_args];
  for (size_t i = 0; i < prototype->num_args; ++i) {
    llvm::Constant* marshal_value = get_marshal_value(
      context, prototype->args[i].marshal);
    llvm::Constant* binding_value = llvm::ConstantStruct::get(
      context.types.ava_argument_binding,
      llvm::ConstantInt::get(context.types.c_int,
                             prototype->args[i].binding.type),
      get_ava_string_constant(context, prototype->args[i].binding.name),
      get_ava_value_constant(context, prototype->args[i].binding.value),
      nullptr);
    argspecs[i] = llvm::ConstantStruct::get(
      context.types.ava_argument_spec,
      marshal_value, binding_value,
      nullptr);
  }
  llvm::Constant* argspecs_value = llvm::ConstantArray::get(
    llvm::ArrayType::get(context.types.ava_argument_spec,
                         prototype->num_args),
    llvm::ArrayRef<llvm::Constant*>(argspecs, prototype->num_args));
  llvm::GlobalVariable* argspecs_var = new llvm::GlobalVariable(
    context.module, argspecs_value->getType(), true, funvar_linkage,
    argspecs_value);
  argspecs_var->setUnnamedAddr(true);

  llvm::Constant* funvar_init_value = llvm::ConstantStruct::get(
    context.types.ava_function,
    llvm::ConstantExpr::getBitCast(
      fun, context.types.ava_function->getElementType(
        ava::ir_types::ff_address)),
    llvm::ConstantInt::get(context.types.c_int, prototype->calling_convention),
    get_marshal_value(context, prototype->c_return_type),
    llvm::ConstantInt::get(context.types.c_size, prototype->num_args),
    argspecs_var,
    llvm::ConstantAggregateZero::get(
      context.types.ava_function->getElementType(
        ava::ir_types::ff_ffi)),
    nullptr);
  llvm::GlobalVariable* funvar = new llvm::GlobalVariable(
    context.module, context.types.ava_function,
    ava_cc_ava == prototype->calling_convention,
    funvar_linkage, funvar_init_value);
  funvar->setUnnamedAddr(true);
  context.global_vars[ix] = funvar;

  return true;
}

static llvm::Type* translate_marshalling_type(
  const ava_xcode_translation_context& context,
  const ava_c_marshalling_type& type)
noexcept {
  switch (type.primitive_type) {
  case ava_cmpt_void: return context.types.c_void;
  case ava_cmpt_byte:
  case ava_cmpt_ubyte: return context.types.c_byte;
  case ava_cmpt_short:
  case ava_cmpt_ushort: return context.types.c_short;
  case ava_cmpt_int:
  case ava_cmpt_uint: return context.types.c_int;
  case ava_cmpt_long:
  case ava_cmpt_ulong: return context.types.c_long;
  case ava_cmpt_llong:
  case ava_cmpt_ullong: return context.types.c_llong;
  case ava_cmpt_ava_sbyte:
  case ava_cmpt_ava_ubyte: return context.types.ava_byte;
  case ava_cmpt_ava_sshort:
  case ava_cmpt_ava_ushort: return context.types.ava_short;
  case ava_cmpt_ava_sint:
  case ava_cmpt_ava_uint: return context.types.ava_int;
  case ava_cmpt_ava_slong:
  case ava_cmpt_ava_ulong: return context.types.ava_long;
  case ava_cmpt_ava_integer: return context.types.ava_integer;
  case ava_cmpt_size: return context.types.c_size;
  case ava_cmpt_float: return context.types.c_float;
  case ava_cmpt_double: return context.types.c_double;
  case ava_cmpt_ldouble: return context.types.c_ldouble;
  case ava_cmpt_ava_real: return context.types.ava_real;
  case ava_cmpt_string: return context.types.c_string;
  case ava_cmpt_pointer: return context.types.general_pointer;
  }

  /* Unreachable */
  abort();
}

static llvm::Constant* get_marshal_value(
  const ava_xcode_translation_context& context,
  const ava_c_marshalling_type& type)
noexcept {
  return llvm::ConstantStruct::get(
    context.types.ava_c_marshalling_type,
    llvm::ConstantInt::get(
      context.types.c_int, type.primitive_type),
    (ava_cmpt_pointer == type.primitive_type?
     get_pointer_prototype_constant(
       context, type.pointer_proto) :
       llvm::ConstantPointerNull::get(
         llvm::PointerType::getUnqual(context.types.ava_pointer_prototype))),
    nullptr);
}

static llvm::Constant* get_pointer_prototype_constant(
  const ava_xcode_translation_context& context,
  const ava_pointer_prototype* prototype)
noexcept {
  if (prototype) {
    llvm::Constant* val = llvm::ConstantStruct::get(
      context.types.ava_pointer_prototype,
      context.pointer_prototype_header,
      get_ava_string_constant(context, prototype->tag),
      llvm::ConstantInt::get(context.types.ava_bool, prototype->is_const),
      nullptr);
    llvm::GlobalVariable* var = new llvm::GlobalVariable(
      context.module, context.types.ava_pointer_prototype, true,
      llvm::GlobalValue::LinkOnceODRLinkage, val);
    var->setUnnamedAddr(true);
    return var;
  } else {
    return llvm::ConstantPointerNull::get(
      llvm::PointerType::getUnqual(context.types.ava_pointer_prototype));
  }
}

static llvm::Constant* get_ava_string_constant(
  const ava_xcode_translation_context& context,
  ava_string string)
noexcept {
  ava_ascii9_string ascii9 = 0;

  if (!ava_string_is_present(string) ||
      (ascii9 = ava_string_to_ascii9(string))) {
    return llvm::ConstantInt::get(
      context.types.ava_string, ascii9);
  } else {
    const char* data = ava_string_to_cstring(string);
    size_t size = (ava_string_length(string) +
                   AVA_STRING_ALIGNMENT /* +1 for NUL, -1 for realign */)
      / AVA_STRING_ALIGNMENT * AVA_STRING_ALIGNMENT;
    llvm::Constant* strconst = llvm::ConstantDataArray::get(
      context.llvm_context,
      llvm::ArrayRef<uint8_t>((const uint8_t*)data, size));
    llvm::GlobalVariable* strglob = new llvm::GlobalVariable(
      context.module, strconst->getType(), true,
      llvm::GlobalValue::LinkOnceODRLinkage, strconst);
    strglob->setUnnamedAddr(true);
    strglob->setAlignment(AVA_STRING_ALIGNMENT);

    llvm::Constant* twineconst = llvm::ConstantStruct::get(
      context.types.ava_twine,
      strglob,
      ava_string_length(string),
      /* TODO: String constants of length 0 to 15 can be crammed into this
       * space.
       */
      llvm::ConstantStruct::get(
        context.types.ava_twine_tail,
        0, llvm::ConstantInt::get(context.types.ava_string, 0),
        nullptr),
      nullptr);
    llvm::GlobalVariable* ret = new llvm::GlobalVariable(
      context.module, context.types.ava_string, true,
      llvm::GlobalVariable::LinkOnceODRLinkage,
      llvm::ConstantExpr::getPtrToInt(twineconst, context.types.ava_string));
    ret->setUnnamedAddr(true);

    return ret;
  }
}

static llvm::Constant* get_ava_value_constant(
  const ava_xcode_translation_context& context,
  ava_value value)
noexcept {
  if (!ava_value_attr(value)) {
    /* Unused field */
    return llvm::ConstantDataVector::getSplat(
      2, llvm::ConstantInt::get(context.types.ava_long, 0));
  } else {
    /* TODO: We should try to find a better type than plain string */
    ava_string string_value = ava_to_string(value);
    llvm::Constant* vals[2];
    vals[0] = llvm::ConstantExpr::getPtrToInt(
      context.string_type, context.types.ava_long);
    vals[1] = get_ava_string_constant(context, string_value);
    return llvm::ConstantVector::get(
      llvm::ArrayRef<llvm::Constant*>(vals, 2));
  }
}

AVA_END_FILE_PRIVATE
