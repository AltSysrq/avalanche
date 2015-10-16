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
#include <cstring>
#include <list>
#include <vector>
#include <memory>
#include <utility>
#include <set>
#include <string>

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DebugLoc.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Support/Dwarf.h>
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

#include "driver-iface.hxx"
#include "ir-types.hxx"
#include "translation.hxx"

AVA_BEGIN_FILE_PRIVATE

struct ava_xcode_translation_context {
  ava_xcode_translation_context(
    llvm::LLVMContext& llvm_context,
    llvm::Module& module,
    ava_string filename,
    ava_string module_name,
    bool full_debug);

  llvm::LLVMContext& llvm_context;
  llvm::Module& module;
  std::string module_name;
  llvm::DIBuilder dib;
  const ava::ir_types types;
  const ava::driver_iface di;

  llvm::GlobalVariable* string_type;
  llvm::GlobalVariable* pointer_pointer_impl;
  llvm::GlobalVariable* pointer_prototype_tag;
  llvm::Constant* empty_string, * empty_string_value;
  llvm::Constant* pointer_prototype_header;

  llvm::DIFile di_file;
  std::map<std::string,llvm::DIFile> di_files;
  llvm::DICompileUnit di_compile_unit;
  llvm::DIBasicType di_size, di_ava_ulong, di_ava_integer;
  llvm::DICompositeType di_ava_value;
  llvm::DIDerivedType di_ava_function_ptr;
  llvm::DICompositeType di_ava_function_parameter, di_ava_fat_list_value;
  std::vector<llvm::DebugLoc> di_global_location;
  std::vector<llvm::DIFile> di_global_files;
  std::vector<unsigned> di_global_lines;

  std::map<size_t,llvm::GlobalVariable*> global_vars;
  std::map<size_t,llvm::Function*> global_funs;

  llvm::DIFile get_di_file(const std::string& name) noexcept;
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

static void make_global_locations(
  ava_xcode_translation_context& context,
  const ava_xcode_global_list* xcode) noexcept;

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

static llvm::Function* create_init_function(
  ava_xcode_translation_context& context) noexcept;

static void build_init_function(
  ava_xcode_translation_context& context,
  llvm::Function* init_function,
  const ava_xcode_global_list* xcode) noexcept;

AVA_END_FILE_PRIVATE

ava::xcode_to_ir_translator::xcode_to_ir_translator(void)
: full_debug(true)
{ }

void ava::xcode_to_ir_translator::add_driver(
  const char* data, size_t size)
noexcept {
  drivers.push_back(std::vector<char>(data, data + size));
}

std::unique_ptr<llvm::Module> ava::xcode_to_ir_translator::translate(
  const ava_xcode_global_list* xcode,
  ava_string filename,
  ava_string module_name,
  llvm::LLVMContext& llvm_context,
  std::string& error)
const noexcept {
  std::unique_ptr<llvm::Module> module(
    new llvm::Module(llvm::StringRef(ava_string_to_cstring(module_name)),
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

  for (size_t i = 0; !ava_string_is_present(filename) && i < xcode->length; ++i)
    if (ava_pcgt_src_pos == xcode->elts[i].pc->type)
      filename = ((const ava_pcg_src_pos*)xcode->elts[i].pc)->filename;

  if (!ava_string_is_present(filename))
    filename = AVA_ASCII9_STRING("<unknown>");

  ava_xcode_handle_driver_functions(*module, declared_symbols);
  ava_xcode_translation_context context(
    llvm_context, *module, filename, module_name, full_debug);
  if (!context.di.error.empty()) {
    error = context.di.error;
    return error_ret;
  }

  make_global_locations(context, xcode);

  for (size_t i = 0; i < xcode->length; ++i) {
    if (!ava_xcode_to_ir_declare_global(
          context, declared_symbols, xcode->elts[i].pc, i, error)) {
      return error_ret;
    }
  }

  llvm::Function* module_init_function = create_init_function(context);
  build_init_function(context, module_init_function, xcode);

  context.dib.finalize();

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

static void make_global_locations(
  ava_xcode_translation_context& context,
  const ava_xcode_global_list* xcode)
noexcept {
  llvm::DebugLoc dl = llvm::DebugLoc::get(0, 0, context.di_file);
  llvm::DIFile file = context.di_file;
  unsigned line = 0;

  for (size_t i = 0; i < xcode->length; ++i) {
    if (ava_pcgt_src_pos == xcode->elts[i].pc->type) {
      const ava_pcg_src_pos* pos = (const ava_pcg_src_pos*)xcode->elts[i].pc;
      file = context.get_di_file(ava_string_to_cstring(pos->filename));
      dl = llvm::DebugLoc::get(
        pos->start_line, pos->start_column, file);
    }
    context.di_global_location.push_back(dl);
    context.di_global_files.push_back(file);
    context.di_global_lines.push_back(line);
  }
}

static bool ava_xcode_to_ir_declare_global(
  ava_xcode_translation_context& context,
  std::set<std::string>& declared_symbols,
  const ava_pcode_global* pcode,
  size_t ix,
  std::string& error)
noexcept {
  llvm::DIFile di_file = context.di_global_files[ix];
  unsigned di_line = context.di_global_lines[ix];

  switch (pcode->type) {
  case ava_pcgt_ext_var: {
    const ava_pcg_ext_var* v = (ava_pcg_ext_var*)pcode;
    llvm::GlobalVariable* var = new llvm::GlobalVariable(
      context.module, context.types.ava_value,
      true, llvm::GlobalValue::ExternalLinkage,
      nullptr, ava_string_to_cstring(ava_name_mangle(v->name)));
    context.global_vars[ix] = var;
    context.dib.createGlobalVariable(
      ava_string_to_cstring(v->name.name),
      var->getName(), di_file, di_line,
      context.di_ava_value, false, var);
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
    context.dib.createGlobalVariable(
      ava_string_to_cstring(v->name.name),
      var->getName(), di_file, di_line,
      context.di_ava_value, !v->publish, var);
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
  llvm::Module& module_,
  ava_string filename, ava_string module_name, bool full_debug)
: llvm_context(llvm_context_),
  module(module_),
  module_name(ava_string_to_cstring(module_name)),
  dib(module_),
  types(llvm_context_, module_),
  di(module_)
{
  string_type = new llvm::GlobalVariable(
    module,
    types.ava_attribute, true, llvm::GlobalValue::ExternalLinkage,
    nullptr, "ava_string_type");

  empty_string = llvm::ConstantInt::get(types.ava_string, 1);
  empty_string_value = llvm::ConstantVector::get(
    { llvm::ConstantExpr::getPtrToInt(string_type, types.ava_long),
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

  const llvm::DataLayout* dl = module.getDataLayout();
  const char* basename = ava_string_to_cstring(filename);
  /* TODO Get the actual dirname */
  const char* dirname = ".";
  di_compile_unit = dib.createCompileUnit(
    /* *(const unsigned short*)"AV" | 0x8000
     * Ie, something in the DW_LANG_*_user range
     */
    0xD541, basename, dirname,
    PACKAGE_STRING,
    true, "", 0, llvm::StringRef(),
    full_debug? llvm::DIBuilder::FullDebug : llvm::DIBuilder::LineTablesOnly,
    true);
  di_file = dib.createFile(basename, dirname);
  di_files[basename] = di_file;
  di_size = dib.createBasicType(
    "size_t", dl->getTypeSizeInBits(types.c_size),
    dl->getABITypeAlignment(types.c_size),
    llvm::dwarf::DW_ATE_unsigned);
  di_ava_integer = dib.createBasicType(
    "ava_integer", dl->getTypeSizeInBits(types.ava_integer),
    dl->getABITypeAlignment(types.ava_integer),
    llvm::dwarf::DW_ATE_signed);
  di_ava_ulong = dib.createBasicType(
    "ava_ulong", dl->getTypeSizeInBits(types.ava_long),
    dl->getABITypeAlignment(types.ava_long),
    llvm::dwarf::DW_ATE_unsigned);
  llvm::Value* ava_value_subscript = dib.getOrCreateSubrange(
    0, types.ava_value->getNumElements());
  llvm::DIArray ava_value_subscript_array = dib.getOrCreateArray(
    ava_value_subscript);
  di_ava_value = dib.createVectorType(
    dl->getTypeSizeInBits(types.ava_value),
    dl->getABITypeAlignment(types.ava_value),
    di_ava_ulong, ava_value_subscript_array);
  di_ava_function_ptr = dib.createPointerType(
    dib.createUnspecifiedType("ava_function"),
    dl->getPointerSizeInBits(),
    dl->getPointerABIAlignment());
  di_ava_fat_list_value = dib.createStructType(
    /* Not actually defined here, but there's no real reason to track it as
     * such.
     */
    di_compile_unit, "ava_fat_list_value", di_file, 0,
    dl->getTypeSizeInBits(types.ava_fat_list_value),
    dl->getABITypeAlignment(types.ava_fat_list_value),
    0, llvm::DIType(),
    dib.getOrCreateArray({
      dib.createPointerType(dib.createUnspecifiedType("ava_list_trait"),
                            dl->getPointerSizeInBits(),
                            dl->getPointerABIAlignment()),
      di_ava_value
    }));
  llvm::DICompositeType di_ava_function_parameter_type =
    dib.createEnumerationType(
      di_compile_unit, "ava_function_parameter_type", di_file, 0,
      dl->getTypeSizeInBits(types.c_int),
      dl->getABITypeAlignment(types.c_int),
      dib.getOrCreateArray(
        { dib.createEnumerator("ava_fpt_static", ava_fpt_static),
          dib.createEnumerator("ava_fpt_dynamic", ava_fpt_dynamic),
          dib.createEnumerator("ava_fpt_spread", ava_fpt_spread) }),
      dib.createBasicType("int", dl->getTypeSizeInBits(types.c_int),
                          dl->getABITypeAlignment(types.c_int),
                          llvm::dwarf::DW_ATE_unsigned));
  di_ava_function_parameter = dib.createStructType(
    di_compile_unit, "ava_function_parameter", di_file, 0,
    dl->getTypeSizeInBits(types.ava_function_parameter),
    dl->getABITypeAlignment(types.ava_function_parameter),
    0, llvm::DIType(),
    dib.getOrCreateArray({ di_ava_function_parameter_type, di_ava_value }));
}

llvm::DIFile ava_xcode_translation_context::get_di_file(
  const std::string& filename)
noexcept {
  auto it = di_files.find(filename);
  if (di_files.end() != it)
    return it->second;

  return di_files[filename] = dib.createFile(filename, ".");
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

    llvm::Constant* twinetailconst =
      /* TODO: String constants of length 0 to 15 can be crammed into this
       * space.
       */
      llvm::ConstantStruct::get(
        context.types.ava_twine_tail,
        llvm::ConstantInt::get(context.types.c_size, 0),
        llvm::ConstantInt::get(context.types.ava_string, 0),
        nullptr);
    llvm::Constant* twineconst = llvm::ConstantStruct::get(
      context.types.ava_twine,
      strglob,
      llvm::ConstantInt::get(context.types.c_size, ava_string_length(string)),
      twinetailconst,
      nullptr);
    llvm::GlobalVariable* ret = new llvm::GlobalVariable(
      context.module, context.types.ava_string, true,
      llvm::GlobalVariable::LinkOnceODRLinkage,
      llvm::ConstantExpr::getPtrToInt(twineconst, context.types.ava_string));
    ret->setUnnamedAddr(true);

    return llvm::ConstantExpr::getBitCast(ret, context.types.ava_string);
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

static llvm::Function* create_init_function(
  ava_xcode_translation_context& context) noexcept
{
  if (context.di.program_entry) {
    context.di.program_entry->setLinkage(
      llvm::GlobalValue::InternalLinkage);
    return context.di.program_entry;
  } else {
    return llvm::Function::Create(
      llvm::FunctionType::get(
        llvm::Type::getVoidTy(context.llvm_context), false),
      llvm::GlobalValue::ExternalLinkage,
      context.module_name, &context.module);
  }
}

static void build_init_function(
  ava_xcode_translation_context& context,
  llvm::Function* init_function,
  const ava_xcode_global_list* xcode)
noexcept {
  llvm::BasicBlock* block = llvm::BasicBlock::Create(
    context.llvm_context, "", init_function);
  llvm::IRBuilder<true> irb(block);
  context.dib.createFunction(
    context.di_compile_unit,
    context.module_name,
    init_function->getName(),
    context.di_file, 0,
    context.dib.createSubroutineType(context.di_file,
                                     context.dib.getOrCreateArray({})),
    false, true, 0, 0, true, init_function);

  for (size_t i = 0; i < xcode->length; ++i) {
    irb.SetCurrentDebugLocation(context.di_global_location[i]);

    switch (xcode->elts[i].pc->type) {
    case ava_pcgt_ext_var: {
      const ava_pcg_ext_var* v = (const ava_pcg_ext_var*)xcode->elts[i].pc;
      ava_string mangled_name = ava_name_mangle(v->name);
      mangled_name = ava_string_concat(mangled_name,
                                       AVA_ASCII9_STRING("_"));

      irb.CreateCall2(context.di.g_ext_var, context.global_vars[i],
                      get_ava_string_constant(
                        context, mangled_name));
    } break;

    case ava_pcgt_ext_fun: {
      const ava_pcg_ext_fun* f = (const ava_pcg_ext_fun*)xcode->elts[i].pc;
      ava_string mangled_name = ava_name_mangle(f->name);

      irb.CreateCall2(
        ava_cc_ava == f->prototype->calling_convention?
        context.di.g_ext_fun_ava : context.di.g_ext_fun_other,
        context.global_vars[i],
        get_ava_string_constant(context, mangled_name));
    } break;

    case ava_pcgt_var: {
      const ava_pcg_var* v = (const ava_pcg_var*)xcode->elts[i].pc;
      ava_string mangled_name = ava_name_mangle(v->name);

      irb.CreateCall3(
        context.di.g_var,
        context.global_vars[i],
        get_ava_string_constant(context, mangled_name),
        llvm::ConstantInt::get(context.types.ava_bool, v->publish));
    } break;

    case ava_pcgt_fun: {
      const ava_pcg_fun* f = (const ava_pcg_fun*)xcode->elts[i].pc;
      ava_string mangled_name = ava_name_mangle(f->name);

      irb.CreateCall3(
        context.di.g_fun_ava,
        context.global_vars[i],
        get_ava_string_constant(context, mangled_name),
        llvm::ConstantInt::get(context.types.ava_bool, f->publish));
    } break;

    case ava_pcgt_src_pos:
    case ava_pcgt_export:
    case ava_pcgt_macro:
    case ava_pcgt_load_pkg:
    case ava_pcgt_load_mod:
    case ava_pcgt_init:
      break;
    }
  }

  irb.CreateRetVoid();
}

AVA_END_FILE_PRIVATE
