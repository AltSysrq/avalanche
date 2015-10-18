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
#include <cstdio>
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
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Support/Dwarf.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

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
#include "../../bsd.h"

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
  llvm::DICompositeType di_subroutine_inline_args[AVA_CC_AVA_MAX_INLINE_ARGS+1];
  llvm::DICompositeType di_subroutine_array_args;
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

static llvm::Constant* wrap_ava_string(
  const ava_xcode_translation_context& context,
  llvm::Constant* bare_string) noexcept;

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

static void build_user_function(
  ava_xcode_translation_context& context,
  llvm::Function* dst,
  const ava_xcode_global* xcode,
  const ava_xcode_global_list* xcode_list,
  llvm::DebugLoc init_loc,
  llvm::DIFile in_file) noexcept;

static void explicitly_deinitialise_registers(
  llvm::IRBuilder<true>& irb,
  const ava_xcode_function* xfun,
  const ava_ulong* phi,
  llvm::Value*const* regs) noexcept;

static bool translate_instruction(
  ava_xcode_translation_context& context,
  llvm::IRBuilder<true>& irb,
  const ava_pcode_exe* exe,
  const ava_xcode_global* container,
  const ava_xcode_global_list* xcode,
  llvm::BasicBlock*const* basic_blocks,
  size_t this_basic_block,
  llvm::Value*const* regs,
  llvm::Value*const* tmplists,
  llvm::Value* data_array_base,
  llvm::DISubprogram scope) noexcept;

static void store_register(
  ava_xcode_translation_context& context,
  llvm::IRBuilder<true>& irb,
  ava_pcode_register dst,
  llvm::Value* src,
  const ava_pcg_fun* pcfun,
  llvm::Value*const* regs) noexcept;

static llvm::Value* load_register(
  ava_xcode_translation_context& context,
  llvm::IRBuilder<true>& irb,
  ava_pcode_register src,
  const ava_pcg_fun* pcfun,
  llvm::Value*const* regs,
  llvm::Value* tmplist) noexcept;

static llvm::Value* convert_register(
  ava_xcode_translation_context& context,
  llvm::IRBuilder<true>& irb,
  llvm::Value* src,
  ava_pcode_register_type dst_type,
  ava_pcode_register_type src_type,
  llvm::Value* tmplist) noexcept;

static llvm::Value* invoke_s(
  ava_xcode_translation_context& context,
  llvm::IRBuilder<true>& irb,
  const ava_xcode_global* xfun,
  size_t fun_ix,
  llvm::Value*const* args,
  llvm::Value* data_array_base,
  bool args_in_data_array) noexcept;

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

  /* If we don't add this, llc and friends "helpfully" decide it's version 0
   * and "upgrade" the debug info by discarding all of it.
   */
  module->addModuleFlag(llvm::Module::Warning, "Debug Info Version",
                        /* Not relevant here, but it's of interest that this
                         * constant is in an *enumeration*, despite not
                         * enumerating anything.
                         */
                        llvm::DEBUG_METADATA_VERSION);
  module->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 4);

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

  for (size_t i = 0; i < xcode->length; ++i) {
    if (ava_pcgt_fun == xcode->elts[i].pc->type) {
      build_user_function(
        context, context.global_funs[i], xcode->elts + i, xcode,
        context.di_global_location[i], context.di_global_files[i]);
    }
  }

  context.dib.finalize();

  {
    std::string errors;
    llvm::raw_string_ostream error_stream(errors);
    /* returns *true* on "not valid" */
    if (llvm::verifyModule(*module, &error_stream)) {
      module->dump();
      errx(EX_SOFTWARE, "Generated invalid IR: %s", errors.c_str());
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

  llvm::Value* di_inline_args[AVA_CC_AVA_MAX_INLINE_ARGS+1] = {
    di_ava_value,
    di_ava_value, di_ava_value, di_ava_value, di_ava_value,
    di_ava_value, di_ava_value, di_ava_value, di_ava_value,
  };
  for (unsigned i = 0; i <= AVA_CC_AVA_MAX_INLINE_ARGS; ++i)
    di_subroutine_inline_args[i] = dib.createSubroutineType(
      di_file, dib.getOrCreateArray(
        llvm::ArrayRef<llvm::Value*>(di_inline_args, i + 1)));
  di_subroutine_array_args = dib.createSubroutineType(
    di_file, dib.getOrCreateArray(
      { di_ava_value, di_size, dib.createPointerType(
          di_ava_value, dl->getPointerSizeInBits()) }));
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
      wrap_ava_string(
        context,
        get_ava_string_constant(context, prototype->args[i].binding.name)),
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
    /* Bitcast the arrayness away */
    llvm::ConstantExpr::getBitCast(
      argspecs_var, context.types.ava_function->getElementType(
        ava::ir_types::ff_args)),
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
      wrap_ava_string(
        context, get_ava_string_constant(context, prototype->tag)),
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
      /* Bitcast arrayness away */
      llvm::ConstantExpr::getBitCast(
        strglob, context.types.ava_twine->getElementType(0)),
      llvm::ConstantInt::get(context.types.c_size, ava_string_length(string)),
      twinetailconst,
      nullptr);
    llvm::GlobalVariable* twinevar = new llvm::GlobalVariable(
      context.module, context.types.ava_twine, true,
      llvm::GlobalVariable::LinkOnceODRLinkage,
      twineconst);
    twinevar->setUnnamedAddr(true);

    return llvm::ConstantExpr::getPtrToInt(twinevar, context.types.ava_string);
  }
}

static llvm::Constant* wrap_ava_string(
  const ava_xcode_translation_context& context,
  llvm::Constant* bare_string)
noexcept {
  return llvm::ConstantStruct::get(
    context.types.ava_string_wrapped,
    bare_string, nullptr);
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
                                     context.dib.getOrCreateArray({NULL})),
    false, true, 0, 0, true, init_function);

  /* TODO: Initialise other packages / modules */

  /* Initialise all globals */
  for (size_t i = 0; i < xcode->length; ++i) {
    irb.SetCurrentDebugLocation(context.di_global_location[i]);

    switch (xcode->elts[i].pc->type) {
    case ava_pcgt_ext_var: {
      const ava_pcg_ext_var* v = (const ava_pcg_ext_var*)xcode->elts[i].pc;
      ava_string mangled_name = ava_name_mangle(v->name);

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

  for (size_t i = 0; i < xcode->length; ++i) {
    if (ava_pcgt_init == xcode->elts[i].pc->type) {
      const ava_pcg_init* init = (const ava_pcg_init*)xcode->elts[i].pc;
      irb.CreateCall(
        context.global_funs[init->fun], context.empty_string_value);
    }
  }

  irb.CreateRetVoid();
}

static llvm::Argument* get_argument(llvm::Function* fun,
                                    size_t ix) {
  for (llvm::Argument& arg: fun->getArgumentList())
    if (!ix--)
      return &arg;

  abort();
}

static void build_user_function(
  ava_xcode_translation_context& context,
  llvm::Function* dst,
  const ava_xcode_global* xcode,
  const ava_xcode_global_list* xcode_list,
  llvm::DebugLoc init_loc,
  llvm::DIFile in_file)
noexcept {
  const ava_pcg_fun* pcfun = (const ava_pcg_fun*)xcode->pc;
  const ava_xcode_function* xfun = xcode->fun;
  llvm::DICompositeType subroutine_type =
    pcfun->prototype->num_args <= AVA_CC_AVA_MAX_INLINE_ARGS?
    context.di_subroutine_inline_args[pcfun->prototype->num_args] :
    context.di_subroutine_array_args;
  llvm::DISubprogram di_fun = context.dib.createFunction(
    in_file, ava_string_to_cstring(pcfun->name.name),
    dst->getName(), in_file, init_loc.getLine(),
    subroutine_type, !pcfun->publish, true, init_loc.getLine(),
    0, true, dst);

  llvm::BasicBlock* init_block, * basic_blocks[xfun->num_blocks];
  llvm::Value* regs[xfun->reg_type_off[ava_prt_function+1]];
  /* Since ava_fat_list_value isn't first-class in the C ABI, we need some
   * temporary space we can make pointers to.
   */
  llvm::Value* tmplists[3];
  char reg_name[16];
  const char* reg_names[xfun->reg_type_off[ava_prt_function+1]];

  init_block = llvm::BasicBlock::Create(context.llvm_context, "init", dst);
  for (size_t i = 0; i < xfun->num_blocks; ++i)
    basic_blocks[i] = llvm::BasicBlock::Create(context.llvm_context, "", dst);

  llvm::IRBuilder<true> irb(init_block);
  irb.SetCurrentDebugLocation(init_loc);
  /* Allocate all registers.
   * P-Registers are never accessed individually, so allocate them all with one
   * alloca. For others, we want separate allocas so that alias analysis
   * doesn't think that passing a pointer to one somewhere could invalidate
   * others.
   */
  for (size_t i = xfun->reg_type_off[ava_prt_var];
       i < xfun->reg_type_off[ava_prt_var+1]; ++i) {
    reg_names[i] = ava_string_to_cstring(
      ava_to_string(ava_list_index_f(pcfun->vars, i)));
    if (0 == strlen(reg_names[i]))
      reg_names[i] = "(anonymous)";

    regs[i] = irb.CreateAlloca(context.types.ava_value, nullptr, reg_names[i]);
    /* Initialise from argument if applicable */
    if (i < pcfun->prototype->num_args) {
      if (pcfun->prototype->num_args <= AVA_CC_AVA_MAX_INLINE_ARGS) {
        irb.CreateStore(get_argument(dst, i), regs[i]);
      } else {
        irb.CreateStore(
          irb.CreateLoad(irb.CreateConstGEP1_32(get_argument(dst, 1), i)),
          regs[i]);
      }
    }

    llvm::DIVariable di_var = context.dib.createLocalVariable(
      i < pcfun->prototype->num_args?
      llvm::dwarf::DW_TAG_arg_variable :
      llvm::dwarf::DW_TAG_auto_variable,
      di_fun, reg_names[i],
      in_file, init_loc.getLine(), context.di_ava_value, true,
      0, i < pcfun->prototype->num_args? i + 1 : 0);
    context.dib.insertDeclare(regs[i], di_var, init_block)
      ->setDebugLoc(init_loc);
  }
#define DEFREG(prefix, i, reg_type, llvm_type, debug_type) do {         \
    std::snprintf(reg_name, sizeof(reg_name), "(" #prefix "%d)", (int)i); \
    reg_names[i] = strdup(reg_name);                                    \
    regs[i] = irb.CreateAlloca(                                         \
      context.types.llvm_type, nullptr, reg_names[i]);                  \
    llvm::DIVariable di_var = context.dib.createLocalVariable(          \
      llvm::dwarf::DW_TAG_auto_variable, di_fun,                        \
      reg_names[i], in_file, init_loc.getLine(), context.debug_type,    \
      false);                                                           \
    context.dib.insertDeclare(regs[i], di_var, init_block)              \
      ->setDebugLoc(init_loc);                                          \
  } while (0)
#define DEFREGS(prefix, reg_type, llvm_type, debug_type) do {    \
    for (size_t i = xfun->reg_type_off[reg_type];                \
         i < xfun->reg_type_off[reg_type+1]; ++i) {              \
      DEFREG(prefix, i, reg_type, llvm_type, debug_type);        \
    }                                                            \
  } while (0)
  DEFREGS(d, ava_prt_data, ava_value, di_ava_value);
  DEFREGS(i, ava_prt_int, ava_integer, di_ava_integer);
  DEFREGS(l, ava_prt_list, ava_fat_list_value, di_ava_fat_list_value);
  DEFREGS(f, ava_prt_function, ava_function_ptr, di_ava_function_ptr);
#undef DEFREGS
#undef DEFREG

  if (xfun->reg_type_off[ava_prt_parm+1] > xfun->reg_type_off[ava_prt_parm]) {
    llvm::AllocaInst* parm_reg_base = irb.CreateAlloca(
      context.types.ava_function_parameter,
      llvm::ConstantInt::get(
        context.types.c_size,
        xfun->reg_type_off[ava_prt_parm+1] - xfun->reg_type_off[ava_prt_parm]),
      "(p*)");
    for (size_t i = xfun->reg_type_off[ava_prt_parm];
         i < xfun->reg_type_off[ava_prt_parm+1]; ++i) {
      std::snprintf(reg_name, sizeof(reg_name), "(p%d)", (int)i);
      reg_names[i] = strdup(reg_name);
      regs[i] = irb.CreateConstGEP1_32(
        parm_reg_base, i - xfun->reg_type_off[ava_prt_parm]);
      llvm::DIVariable di_var = context.dib.createLocalVariable(
        llvm::dwarf::DW_TAG_auto_variable, di_fun,
        reg_names[i], in_file, init_loc.getLine(),
        context.di_ava_function_parameter, false);
      context.dib.insertDeclare(regs[i], di_var, init_block)
        ->setDebugLoc(init_loc);
    }
  }

  /* Allocate temporary list slots */
  for (size_t i = 0; i < 3; ++i)
    tmplists[i] = irb.CreateAlloca(context.types.ava_fat_list_value);

  /* Some instructions require us to supply an array of data registers
   * somewhere. Find the largest such array we may need, allocate it once, and
   * just copy things there whenever it may be necessary.
   */
  size_t max_data_array_length = 0;
  for (size_t block_ix = 0; block_ix < xfun->num_blocks; ++block_ix) {
    const ava_xcode_basic_block* block = xfun->blocks[block_ix];
    for (size_t instr_ix = 0; instr_ix < block->length; ++instr_ix) {
      const ava_pcode_exe* instr = block->elts[instr_ix];
      if (ava_pcode_exe_is_special_reg_read_d(instr)) {
        ava_integer count;
        if (!ava_pcode_exe_get_reg_read_count(&count, instr, 0))
          abort();

        if ((size_t)count > max_data_array_length)
          max_data_array_length = count;
      }

      /* invoke-sd needs space for its bound arguments */
      if (ava_pcxt_invoke_sd == instr->type) {
        const ava_pcx_invoke_sd* isd = (const ava_pcx_invoke_sd*)instr;
        const ava_pcode_global* callee = xcode_list->elts[isd->fun].pc;
        const ava_function* prototype;

        if (!ava_pcode_global_get_prototype(&prototype, callee, 0))
          abort();

        if (prototype->num_args > max_data_array_length)
          max_data_array_length = prototype->num_args;
      }
    }
  }
  llvm::AllocaInst* data_array_base = max_data_array_length == 0?
    nullptr :
    irb.CreateAlloca(context.types.ava_value,
                     llvm::ConstantInt::get(context.types.c_size,
                                            max_data_array_length),
                     "(d*)");

  /* Fall through to the first block, or off the end of the function if it's
   * empty.
   */
  if (xfun->num_blocks > 0) {
    irb.CreateBr(basic_blocks[0]);
  } else {
    irb.CreateRet(context.empty_string_value);
  }

  for (size_t block_ix = 0; block_ix < xfun->num_blocks; ++block_ix) {
    const ava_xcode_basic_block* block = xfun->blocks[block_ix];
    irb.SetInsertPoint(basic_blocks[block_ix]);
    /* Explicitly tell the backend that particular registers do not have any
     * particular value upon entry/exit to basic blocks.
     *
     * The optimiser could probably figure this out on its own, given how
     * simple our definite initialisation checks are, but it doesn't hurt to
     * add this.
     */
    explicitly_deinitialise_registers(irb, xfun, block->phi_iinit, regs);

    bool has_terminal = false;
    for (size_t instr_ix = 0; instr_ix < block->length; ++instr_ix) {
      const ava_pcode_exe* exe = block->elts[instr_ix];
      has_terminal |= translate_instruction(
        context, irb, exe, xcode, xcode_list, basic_blocks, block_ix,
        regs, tmplists, data_array_base, di_fun);
    }

    if (!has_terminal) {
      /* Fell off the end of the block */
      explicitly_deinitialise_registers(irb, xfun, block->phi_oinit, regs);
      if (block_ix + 1 < xfun->num_blocks)
        irb.CreateBr(basic_blocks[block_ix + 1]);
      else
        irb.CreateRet(context.empty_string_value);
    }
  }
}

static void explicitly_deinitialise_registers(
  llvm::IRBuilder<true>& irb,
  const ava_xcode_function* xfun,
  const ava_ulong* phi,
  llvm::Value*const* regs)
noexcept {
  for (size_t i = 0; i < xfun->reg_type_off[ava_prt_function+1]; ++i) {
    if (!ava_xcode_phi_get(phi, i)) {
      llvm::PointerType* reg_type =
        llvm::cast<llvm::PointerType>(regs[i]->getType());
      irb.CreateStore(
        llvm::UndefValue::get(reg_type->getElementType()), regs[i]);
    }
  }
}

static ava_string to_ava_string(llvm::StringRef ref) {
  return ava_string_of_bytes(ref.data(), ref.size());
}

static bool translate_instruction(
  ava_xcode_translation_context& context,
  llvm::IRBuilder<true>& irb,
  const ava_pcode_exe* exe,
  const ava_xcode_global* container,
  const ava_xcode_global_list* xcode,
  llvm::BasicBlock*const* basic_blocks,
  size_t this_basic_block,
  llvm::Value*const* regs,
  llvm::Value*const* tmplists,
  llvm::Value* data_array_base,
  llvm::DISubprogram scope)
noexcept {
  const ava_xcode_function* xfun = container->fun;
  const ava_pcg_fun* pcfun = (const ava_pcg_fun*)container->pc;

  switch (exe->type) {
  case ava_pcxt_src_pos: {
    const ava_pcx_src_pos* p = (const ava_pcx_src_pos*)exe;
    /* Assume in the same file */
    irb.SetCurrentDebugLocation(
      llvm::DebugLoc::get(p->start_line, p->start_column, scope));
  } return false;

  case ava_pcxt_push:
  case ava_pcxt_pop:
  case ava_pcxt_label:
    return false;

  case ava_pcxt_ld_imm_vd: {
    const ava_pcx_ld_imm_vd* p = (const ava_pcx_ld_imm_vd*)exe;

    llvm::Value* src = get_ava_value_constant(
      context, ava_value_of_string(p->src));
    store_register(context, irb, p->dst, src, pcfun, regs);
  } return false;

  case ava_pcxt_ld_imm_i: {
    const ava_pcx_ld_imm_i* p = (const ava_pcx_ld_imm_i*)exe;

    llvm::Value* src = llvm::ConstantInt::get(
      context.types.ava_integer, p->src);
    store_register(context, irb, p->dst, src, pcfun, regs);
  } return false;

  case ava_pcxt_ld_glob: {
    const ava_pcx_ld_glob* p = (const ava_pcx_ld_glob*)exe;

    llvm::Value* src = irb.CreateCall2(
      ava_pcode_global_is_fun(xcode->elts[p->src].pc)?
      context.di.x_load_glob_fun : context.di.x_load_glob_var,
      context.global_vars[p->src],
      get_ava_string_constant(
        context, to_ava_string(
          context.global_vars[p->src]->getName())));
    store_register(context, irb, p->dst, src, pcfun, regs);
  } return false;

  case ava_pcxt_ld_reg: {
    const ava_pcx_ld_reg* p = (const ava_pcx_ld_reg*)exe;

    llvm::Value* raw_src = load_register(
      context, irb, p->src, pcfun, regs, tmplists[0]);
    llvm::Value* src = convert_register(
      context, irb, raw_src, p->dst.type, p->src.type, tmplists[0]);
    store_register(context, irb, p->dst, src, pcfun, regs);
  } return false;

  case ava_pcxt_ld_parm: {
    const ava_pcx_ld_parm* p = (const ava_pcx_ld_parm*)exe;

    llvm::Value* src = load_register(
      context, irb, p->src, pcfun, regs, nullptr);
    irb.CreateCall4(
      context.di.x_store_p,
      regs[p->dst.index], src,
      llvm::ConstantInt::get(context.types.ava_function_parameter_type,
                             p->spread? ava_fpt_spread : ava_fpt_static),
      llvm::ConstantInt::get(context.types.c_size, p->dst.index));
  } return false;

  case ava_pcxt_set_glob: {
    const ava_pcx_set_glob* p = (const ava_pcx_set_glob*)exe;

    llvm::Value* src = load_register(
      context, irb, p->src, pcfun, regs, nullptr);
    irb.CreateCall3(
      context.di.x_store_glob_var,
      context.global_vars[p->dst],
      src, get_ava_string_constant(
        context,
        to_ava_string(context.global_vars[p->dst]->getName())));
  } return false;

  case ava_pcxt_lempty: {
    const ava_pcx_lempty* p = (const ava_pcx_lempty*)exe;

    irb.CreateCall(context.di.x_lempty, tmplists[0]);
    store_register(context, irb, p->dst, tmplists[0], pcfun, regs);
  } return false;

  case ava_pcxt_lappend: {
    const ava_pcx_lappend* p = (const ava_pcx_lappend*)exe;

    llvm::Value* lsrc = load_register(
      context, irb, p->lsrc, pcfun, regs, tmplists[0]);
    llvm::Value* esrc = load_register(
      context, irb, p->esrc, pcfun, regs, nullptr);
    irb.CreateCall3(
      context.di.x_lappend,
      tmplists[1], lsrc, esrc);
    store_register(context, irb, p->dst, tmplists[1], pcfun, regs);
  } return false;

  case ava_pcxt_lcat: {
    const ava_pcx_lcat* p = (const ava_pcx_lcat*)exe;

    llvm::Value* left = load_register(
      context, irb, p->left, pcfun, regs, tmplists[0]);
    llvm::Value* right = load_register(
      context, irb, p->right, pcfun, regs, tmplists[1]);
    irb.CreateCall3(
      context.di.x_lcat,
      tmplists[2], left, right);
    store_register(context, irb, p->dst, tmplists[2], pcfun, regs);
  } return false;

  case ava_pcxt_lhead: {
    const ava_pcx_lhead* p = (const ava_pcx_lhead*)exe;

    llvm::Value* src = load_register(
      context, irb, p->src, pcfun, regs, tmplists[0]);
    llvm::Value* val = irb.CreateCall(context.di.x_lhead, src);
    store_register(context, irb, p->dst, val, pcfun, regs);
  } return false;

  case ava_pcxt_lbehead: {
    const ava_pcx_lbehead* p = (const ava_pcx_lbehead*)exe;

    llvm::Value* src = load_register(
      context, irb, p->src, pcfun, regs, tmplists[0]);
    irb.CreateCall2(context.di.x_lbehead, tmplists[1], src);
    store_register(context, irb, p->dst, tmplists[1], pcfun, regs);
  } return false;

  case ava_pcxt_lflatten: {
    const ava_pcx_lflatten* p = (const ava_pcx_lflatten*)exe;

    llvm::Value* src = load_register(
      context, irb, p->src, pcfun, regs, tmplists[0]);
    irb.CreateCall2(context.di.x_lflatten, tmplists[1], src);
    store_register(context, irb, p->dst, tmplists[1], pcfun, regs);
  } return false;

  case ava_pcxt_lindex: {
    const ava_pcx_lindex* p = (const ava_pcx_lindex*)exe;

    llvm::Value* src = load_register(
      context, irb, p->src, pcfun, regs, tmplists[0]);
    llvm::Value* ix = load_register(
      context, irb, p->ix, pcfun, regs, nullptr);
    llvm::Value* val = irb.CreateCall4(
      context.di.x_lindex, src, ix,
      get_ava_string_constant(context, p->extype),
      get_ava_string_constant(context, p->exmessage));
    store_register(context, irb, p->dst, val, pcfun, regs);
  } return false;

  case ava_pcxt_llength: {
    const ava_pcx_llength* p = (const ava_pcx_llength*)exe;

    llvm::Value* src = load_register(
      context, irb, p->src, pcfun, regs, tmplists[0]);
    llvm::Value* val = irb.CreateCall(context.di.x_llength, src);
    store_register(context, irb, p->dst, val, pcfun, regs);
  } return false;

  case ava_pcxt_iadd_imm: {
    const ava_pcx_iadd_imm* p = (const ava_pcx_iadd_imm*)exe;

    llvm::Value* left = load_register(
      context, irb, p->src, pcfun, regs, nullptr);
    llvm::Value* right = llvm::ConstantInt::get(
      context.types.ava_integer, p->incr);
    llvm::Value* val = irb.CreateCall2(context.di.x_iadd, left, right);
    store_register(context, irb, p->dst, val, pcfun, regs);
  } return false;

  case ava_pcxt_icmp: {
    const ava_pcx_icmp* p = (const ava_pcx_icmp*)exe;

    llvm::Value* left = load_register(
      context, irb, p->left, pcfun, regs, nullptr);
    llvm::Value* right = load_register(
      context, irb, p->right, pcfun, regs, nullptr);
    llvm::Value* val = irb.CreateCall2(context.di.x_icmp, left, right);
    store_register(context, irb, p->dst, val, pcfun, regs);
  } return false;

  case ava_pcxt_bool: {
    const ava_pcx_bool* p = (const ava_pcx_bool*)exe;

    llvm::Value* src = load_register(
      context, irb, p->src, pcfun, regs, nullptr);
    llvm::Value* val = irb.CreateCall(context.di.x_bool, src);

    store_register(context, irb, p->dst, val, pcfun, regs);
  } return false;

  case ava_pcxt_aaempty: {
    const ava_pcx_aaempty* p = (const ava_pcx_aaempty*)exe;

    llvm::Value* src = load_register(
      context, irb, p->src, pcfun, regs, nullptr);
    irb.CreateCall(context.di.x_aaempty, src);
  } return false;

  case ava_pcxt_invoke_ss: {
    const ava_pcx_invoke_ss* p = (const ava_pcx_invoke_ss*)exe;

    llvm::Value* args[p->nargs];
    for (size_t i = 0; i < (size_t)p->nargs; ++i) {
      ava_pcode_register reg;
      reg.type = ava_prt_data;
      reg.index = p->base + i;
      args[i] = load_register(context, irb, reg, pcfun, regs, nullptr);
    }

    llvm::Value* ret = invoke_s(
      context, irb, xcode->elts + p->fun, p->fun,
      args, data_array_base, false);

    store_register(context, irb, p->dst, ret, pcfun, regs);
  } return false;

  case ava_pcxt_invoke_sd: {
    const ava_pcx_invoke_sd* p = (const ava_pcx_invoke_sd*)exe;

    irb.CreateCall4(
      context.di.x_invoke_sd_bind,
      data_array_base, context.global_vars[p->fun],
      regs[p->base],
      llvm::ConstantInt::get(context.types.c_size, p->nparms));

    const ava_pcode_global* target = xcode->elts[p->fun].pc;
    const ava_function* prototype;
    if (!ava_pcode_global_get_prototype(&prototype, target, 0))
      abort();
    size_t nargs = prototype->num_args;

    llvm::Value* args[nargs];
    for (size_t i = 0; i < nargs; ++i)
      args[i] = irb.CreateLoad(irb.CreateConstGEP1_32(data_array_base, i));

    llvm::Value* ret = invoke_s(
      context, irb, xcode->elts + p->fun, p->fun,
      args, data_array_base, true);

    store_register(context, irb, p->dst, ret, pcfun, regs);
  } return false;

  case ava_pcxt_invoke_dd: {
    const ava_pcx_invoke_dd* p = (const ava_pcx_invoke_dd*)exe;

    llvm::Value* target = load_register(
      context, irb, p->fun, pcfun, regs, nullptr);
    llvm::Value* ret = irb.CreateCall3(
      context.di.x_invoke_dd,
      target, regs[p->base],
      llvm::ConstantInt::get(context.types.c_size, p->nparms));

    store_register(context, irb, p->dst, ret, pcfun, regs);
  } return false;

  case ava_pcxt_partial: {
    const ava_pcx_partial* p = (const ava_pcx_partial*)exe;

    llvm::Value* src = load_register(
      context, irb, p->src, pcfun, regs, nullptr);
    for (size_t i = 0; i < (size_t)p->nargs; ++i) {
      ava_pcode_register reg;
      reg.type = ava_prt_data;
      reg.index = p->base + i;
      llvm::Value* val = load_register(
        context, irb, reg, pcfun, regs, nullptr);
      irb.CreateStore(
        val, irb.CreateConstGEP1_32(data_array_base, i));
    }

    llvm::Value* val = irb.CreateCall3(
      context.di.x_partial,
      src, data_array_base,
      llvm::ConstantInt::get(context.types.c_size, p->nargs));

    store_register(context, irb, p->dst, val, pcfun, regs);
  } return false;

  case ava_pcxt_ret: {
    const ava_pcx_ret* p = (const ava_pcx_ret*)exe;

    llvm::Value* src = load_register(
      context, irb, p->return_value, pcfun, regs, nullptr);
    explicitly_deinitialise_registers(
      irb, xfun, xfun->blocks[this_basic_block]->phi_oinit, regs);
    irb.CreateRet(src);
  } return true;

  case ava_pcxt_branch: {
    const ava_pcx_branch* p = (const ava_pcx_branch*)exe;

    llvm::Value* key = load_register(
      context, irb, p->key, pcfun, regs, nullptr);
    llvm::Value* value = llvm::ConstantInt::get(
      context.types.ava_integer, p->value);
    llvm::Value* test;
    if (p->invert)
      test = irb.CreateICmpNE(key, value);
    else
      test = irb.CreateICmpEQ(key, value);

    explicitly_deinitialise_registers(
      irb, xfun, xfun->blocks[this_basic_block]->phi_oinit, regs);

    llvm::BasicBlock* subsequent;
    if (this_basic_block + 1 < xfun->num_blocks) {
      subsequent = basic_blocks[this_basic_block + 1];
    } else {
      /* Conditional branch which falls off the end of the function if not
       * taken.
       *
       * Need to create an extra BasicBlock for it to fall into.
       */
      subsequent = llvm::BasicBlock::Create(
        context.llvm_context, "branch-fall-off", basic_blocks[0]->getParent());
      llvm::IRBuilder<true> sirb(subsequent);
      sirb.CreateRet(context.empty_string_value);
    }

    irb.CreateCondBr(test, basic_blocks[p->target], subsequent);
  } return true;

  case ava_pcxt_goto: {
    const ava_pcx_goto* p = (const ava_pcx_goto*)exe;

    explicitly_deinitialise_registers(
      irb, xfun, xfun->blocks[this_basic_block]->phi_oinit, regs);

    irb.CreateBr(basic_blocks[p->target]);
  } return true;
  }
}


static void store_register(
  ava_xcode_translation_context& context,
  llvm::IRBuilder<true>& irb,
  ava_pcode_register dst,
  llvm::Value* src,
  const ava_pcg_fun* pcfun,
  llvm::Value*const* regs)
noexcept {
  switch (dst.type) {
  case ava_prt_var:
    irb.CreateCall3(
      context.di.x_store_v, regs[dst.index], src,
      get_ava_string_constant(
        context, ava_to_string(ava_list_index_f(pcfun->vars, dst.index))));
    break;

  case ava_prt_data:
    irb.CreateCall3(
      context.di.x_store_d, regs[dst.index], src,
      llvm::ConstantInt::get(context.types.c_size, dst.index));
    break;

  case ava_prt_int:
    irb.CreateCall3(
      context.di.x_store_i, regs[dst.index], src,
      llvm::ConstantInt::get(context.types.c_size, dst.index));
    break;

  case ava_prt_list:
    irb.CreateCall3(
      context.di.x_store_l, regs[dst.index], src,
      llvm::ConstantInt::get(context.types.c_size, dst.index));
    break;

  case ava_prt_function:
    irb.CreateCall3(
      context.di.x_store_f, regs[dst.index], src,
      llvm::ConstantInt::get(context.types.c_size, dst.index));
    break;

  case ava_prt_parm: abort();
  }
}

static llvm::Value* load_register(
  ava_xcode_translation_context& context,
  llvm::IRBuilder<true>& irb,
  ava_pcode_register src,
  const ava_pcg_fun* pcfun,
  llvm::Value*const* regs,
  llvm::Value* tmplist)
noexcept {
  switch (src.type) {
  case ava_prt_var:
    return irb.CreateCall2(
      context.di.x_load_v, regs[src.index],
      get_ava_string_constant(
        context, ava_to_string(
          ava_list_index_f(pcfun->vars, src.index))));

  case ava_prt_data:
    return irb.CreateCall2(
      context.di.x_load_d, regs[src.index],
      llvm::ConstantInt::get(context.types.c_size, src.index));

  case ava_prt_int:
    return irb.CreateCall2(
      context.di.x_load_i, regs[src.index],
      llvm::ConstantInt::get(context.types.c_size, src.index));

  case ava_prt_function:
    return irb.CreateCall2(
      context.di.x_load_f, regs[src.index],
      llvm::ConstantInt::get(context.types.c_size, src.index));

  case ava_prt_list:
    irb.CreateCall3(
      context.di.x_load_l, tmplist, regs[src.index],
      llvm::ConstantInt::get(context.types.c_size, src.index));
    return tmplist;

  case ava_prt_parm: abort();
  }
}

static llvm::Value* convert_register(
  ava_xcode_translation_context& context,
  llvm::IRBuilder<true>& irb,
  llvm::Value* src,
  ava_pcode_register_type dst_type,
  ava_pcode_register_type src_type,
  llvm::Value* tmplist)
noexcept {
  if (dst_type == ava_prt_data || dst_type == ava_prt_var) {
    switch (src_type) {
    case ava_prt_data:
    case ava_prt_var:
      return src;

    case ava_prt_int:
      return irb.CreateCall(context.di.x_conv_vi, src);

    case ava_prt_list:
      return irb.CreateCall(context.di.x_conv_vl, src);

    case ava_prt_function:
      return irb.CreateCall(context.di.x_conv_vf, src);

    case ava_prt_parm: abort();
    }
  } else if (src_type == dst_type) {
    return src;
  } else {
    assert(ava_prt_data == src_type || ava_prt_var == src_type);

    switch (dst_type) {
    case ava_prt_data:
    case ava_prt_var:
      /* unreachable */
      abort();

    case ava_prt_int:
      return irb.CreateCall(context.di.x_conv_iv, src);

    case ava_prt_function:
      return irb.CreateCall(context.di.x_conv_fv, src);

    case ava_prt_list:
      irb.CreateCall2(context.di.x_conv_lv, tmplist, src);
      return tmplist;

    case ava_prt_parm: abort();
    }
  }
}

static llvm::Value* invoke_s(
  ava_xcode_translation_context& context,
  llvm::IRBuilder<true>& irb,
  const ava_xcode_global* xfun,
  size_t fun_ix,
  llvm::Value*const* args,
  llvm::Value* data_array_base,
  bool args_in_data_array)
noexcept {
  llvm::Value* ret;
  const ava_function* prot;
  llvm::Value* fun_mangled_name = get_ava_string_constant(
    context, to_ava_string(context.global_funs[fun_ix]->getName()));

  if (!ava_pcode_global_get_prototype(&prot, xfun->pc, 0))
    abort();

  if (ava_cc_ava == prot->calling_convention) {
    irb.CreateCall2(context.di.x_pre_invoke_s,
                    context.global_vars[fun_ix],
                    fun_mangled_name);
    if (prot->num_args <= AVA_CC_AVA_MAX_INLINE_ARGS) {
      ret = irb.CreateCall(context.global_funs[fun_ix],
                           llvm::ArrayRef<llvm::Value*>(args, prot->num_args));
    } else {
      if (!args_in_data_array) {
        for (size_t i = 0; i < prot->num_args; ++i) {
          irb.CreateStore(args[i], irb.CreateConstGEP1_32(data_array_base, i));
        }
      }
      ret = irb.CreateCall2(
        context.global_funs[fun_ix],
        llvm::ConstantInt::get(context.types.c_size, prot->num_args),
        data_array_base);
    }
  } else {
    llvm::Value* actual_args[prot->num_args];
    size_t actual_nargs = 0;

    for (size_t i = 0; i < prot->num_args; ++i) {
      switch (prot->args[i].marshal.primitive_type) {
      case ava_cmpt_void:
        irb.CreateCall(context.di.marshal_to[ava_cmpt_void], args[i]);
        break;

      case ava_cmpt_pointer:
        actual_args[actual_nargs++] = irb.CreateCall2(
          context.di.marshal_to[ava_cmpt_pointer],
          args[i], get_pointer_prototype_constant(
            context, prot->args[i].marshal.pointer_proto));
        break;

      default:
        actual_args[actual_nargs++] = irb.CreateCall(
          context.di.marshal_to[prot->args[i].marshal.primitive_type],
          args[i]);
        break;
      }
    }

    llvm::Value* native_return = irb.CreateCall(
      context.global_funs[fun_ix],
      llvm::ArrayRef<llvm::Value*>(actual_args, actual_nargs));

    switch (prot->c_return_type.primitive_type) {
    case ava_cmpt_void:
      ret = irb.CreateCall(context.di.marshal_from[ava_cmpt_void]);
      break;

    case ava_cmpt_pointer:
      ret = irb.CreateCall2(
        context.di.marshal_from[ava_cmpt_pointer],
        native_return,
        get_pointer_prototype_constant(
          context, prot->c_return_type.pointer_proto));
      break;

    default:
      ret = irb.CreateCall(
        context.di.marshal_from[prot->c_return_type.primitive_type],
        native_return);
      break;
    }
  }

  irb.CreateCall3(context.di.x_post_invoke_s,
                  context.global_vars[fun_ix],
                  fun_mangled_name, ret);

  return ret;
}

AVA_END_FILE_PRIVATE
