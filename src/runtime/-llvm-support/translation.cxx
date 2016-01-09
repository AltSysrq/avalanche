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
#include <map>
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
#include <llvm/IR/Intrinsics.h>
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
#include "../avalanche/struct.h"
#include "../avalanche/exception.h"
AVA_END_DECLS
#include "../../bsd.h"
#include "../-internal-defs.h"

#include "driver-iface.hxx"
#include "exception-abi.hxx"
#include "ir-types.hxx"
#include "translation.hxx"

AVA_BEGIN_FILE_PRIVATE

struct ava_xcode_translation_context {
  ava_xcode_translation_context(
    llvm::LLVMContext& llvm_context,
    llvm::Module& module,
    ava_string filename,
    ava_string module_name,
    ava_string package_prefix,
    bool full_debug);

  llvm::LLVMContext& llvm_context;
  llvm::Module& module;
  std::string module_name;
  std::string package_prefix;
  llvm::DIBuilder dib;
  const ava::ir_types types;
  const ava::driver_iface di;
  const ava::exception_abi ea;

  llvm::GlobalVariable* string_type;
  llvm::GlobalVariable* integer_type;
  llvm::GlobalVariable* pointer_pointer_impl;
  llvm::GlobalVariable* pointer_prototype_tag;
  llvm::Constant* empty_string, * empty_string_value;
  llvm::Constant* pointer_prototype_header;

  llvm::DIFile* di_file;
  std::map<std::string,llvm::DIFile*> di_files;
  llvm::DICompileUnit* di_compile_unit;

  llvm::DIBasicType* di_void;
  llvm::DIBasicType* di_signed_c_byte, * di_unsigned_c_byte;
  llvm::DIBasicType* di_signed_c_short, * di_unsigned_c_short;
  llvm::DIBasicType* di_signed_c_int, * di_unsigned_c_int;
  llvm::DIBasicType* di_signed_c_long, * di_unsigned_c_long;
  llvm::DIBasicType* di_signed_c_llong, * di_unsigned_c_llong;
  llvm::DIBasicType* di_signed_ava_byte, * di_unsigned_ava_byte;
  llvm::DIBasicType* di_signed_ava_short, * di_unsigned_ava_short;
  llvm::DIBasicType* di_signed_ava_int, * di_unsigned_ava_int;
  llvm::DIBasicType* di_signed_ava_long, * di_unsigned_ava_long;
  llvm::DIBasicType* di_signed_ava_integer, * di_unsigned_ava_integer;
  llvm::DIBasicType* di_signed_c_size, * di_unsigned_c_size;
  llvm::DIBasicType* di_signed_c_intptr, * di_unsigned_c_intptr;
  llvm::DIBasicType* di_signed_c_atomic, * di_unsigned_c_atomic;
  llvm::DIBasicType* di_c_float, * di_c_double, * di_c_ldouble, * di_ava_real;
  llvm::DIDerivedType* di_c_string, * di_general_pointer;

  llvm::DICompositeType* di_ava_value;
  llvm::DIDerivedType* di_ava_function_ptr;
  llvm::DICompositeType* di_ava_function_parameter, * di_ava_fat_list_value;
  llvm::DISubroutineType* di_subroutine_inline_args[AVA_CC_AVA_MAX_INLINE_ARGS+1];
  llvm::DISubroutineType* di_subroutine_array_args;
  std::vector<llvm::DebugLoc> di_global_location;
  std::vector<llvm::DIFile*> di_global_files;
  std::vector<unsigned> di_global_lines;

  std::map<size_t,llvm::GlobalVariable*> global_vars;
  std::map<size_t,llvm::Function*> global_funs;
  /* Maps from global indices to types for structs declared with decl-sxt. Both
   * structs and unions are entered into the structs map; for unions, this just
   * acts as a container for the sub-types, to determine required alignment,
   * and simplifies some of the code. Unions are also entered into the unions
   * map, which holds arrays of size equal to the largest member in the
   * corresponding struct and with a type appropriate to force LLVM to consider
   * it to have the required alignment, since there's strangely no other way to
   * express this.
   */
  std::map<size_t,llvm::StructType*> global_sxt_structs;
  std::map<size_t,llvm::ArrayType*> global_sxt_unions;

  llvm::DIFile* get_di_file(const std::string& name) noexcept;
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

static bool ava_xcode_to_ir_declare_sxt(
  ava_xcode_translation_context& context,
  size_t ix, const ava_struct* sxt,
  std::string& error) noexcept;
static llvm::StructType* ava_xcode_to_ir_make_struct_type(
  ava_xcode_translation_context& context,
  const ava_struct* sxt,
  std::string& error) noexcept;
static llvm::Type* ava_xcode_to_ir_make_struct_member(
  ava_xcode_translation_context& context,
  ava_string struct_name,
  const ava_struct_field* field,
  std::string& error) noexcept;
static llvm::ArrayType* ava_xcode_to_ir_make_union_type(
  ava_xcode_translation_context& context,
  llvm::StructType* sxt) noexcept;

static llvm::Type* translate_marshalling_type(
  const ava_xcode_translation_context& context,
  const ava_c_marshalling_type& type) noexcept;

static llvm::Metadata* translate_marshalling_debug_type(
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
  llvm::DIFile* in_file) noexcept;

struct ava_xcode_fun_xlate_info {
  ava_xcode_translation_context& context;
  llvm::IRBuilder<true>& irb;
  const ava_xcode_function* xfun;
  const std::vector<llvm::BasicBlock*>& basic_blocks;
  /**
   * Landing pads which have been generated.
   *
   * The key is (destination_block,source_ce_height). destination_block is the
   * index of the basic block which gains control upon an exception being
   * thrown. source_ce_height is the value of current_exception at the throw
   * point.
   *
   * This model is necessary to ensure we call __cxa_end_catch() the correct
   * number of times.
   */
  std::map<std::pair<signed,signed>,llvm::BasicBlock*>& landing_pads;
  size_t this_basic_block;
  const std::vector<llvm::Value*>& caught_exceptions;
  const std::vector<llvm::Value*>& regs;
  llvm::Value*const* tmplists;
  llvm::Value* data_array_base;

  ava_xcode_fun_xlate_info(
    ava_xcode_translation_context& context_,
    llvm::IRBuilder<true>& irb_,
    const ava_xcode_function* xfun_,
    const std::vector<llvm::BasicBlock*>& basic_blocks_,
    std::map<std::pair<signed,signed>,llvm::BasicBlock*>& landing_pads_,
    size_t this_basic_block_,
    const std::vector<llvm::Value*>& caught_exceptions_,
    const std::vector<llvm::Value*>& regs_,
    llvm::Value*const* tmplists_,
    llvm::Value* data_array_base_)
  : context(context_), irb(irb_), xfun(xfun_),
    basic_blocks(basic_blocks_),
    landing_pads(landing_pads_),
    this_basic_block(this_basic_block_),
    caught_exceptions(caught_exceptions_),
    regs(regs_),
    tmplists(tmplists_),
    data_array_base(data_array_base_)
  { }

  void copy_args_to_regs(
    const ava_pcg_fun* pcfun,
    llvm::Function* dst) noexcept;

  bool translate_instruction(
    const ava_pcode_exe* exe,
    const ava_xcode_global* container,
    const ava_xcode_global_list* xcode,
    llvm::DISubprogram* scope) noexcept;

  void store_register(
    ava_pcode_register dst,
    llvm::Value* src,
    const ava_pcg_fun* pcfun) noexcept;

  void store_strangelet(
    ava_pcode_register dst,
    llvm::Value* src,
    const ava_pcg_fun* pcfun) noexcept;

  llvm::Value* load_register(
    ava_pcode_register src,
    const ava_pcg_fun* pcfun,
    llvm::Value* tmplist) noexcept;

  llvm::Value* load_strangelet(
    ava_pcode_register src,
    const ava_pcg_fun* pcfun) noexcept;

  llvm::Value* convert_register(
    llvm::Value* src,
    ava_pcode_register_type dst_type,
    ava_pcode_register_type src_type,
    llvm::Value* tmplist) noexcept;

  llvm::Type* get_sxt_type(
    const ava_xcode_global_list* xcode,
    size_t struct_ix) noexcept;

  llvm::Type* get_sxt_field_type(
    size_t struct_ix, size_t field_ix) noexcept;

  const ava_struct* get_sxt_def(
    const ava_xcode_global_list* xcode,
    size_t struct_ix) noexcept;

  const ava_struct_field* get_sxt_field_def(
    const ava_xcode_global_list* xcode,
    size_t struct_ix, size_t field_ix) noexcept;

  llvm::Value* get_sxt_field_ptr(
    llvm::Value* base,
    const ava_xcode_global_list* xcode,
    size_t struct_ix,
    size_t field_ix) noexcept;

  llvm::AtomicOrdering get_llvm_ordering(
    ava_pcode_memory_order pco) noexcept;

  void figure_cas_orderings(
    llvm::AtomicOrdering* succ_dst,
    llvm::AtomicOrdering* fail_dst,
    ava_pcode_memory_order succ_src,
    ava_pcode_memory_order fail_src) noexcept;

  llvm::Value* expand_to_ava_integer(
    llvm::Value* base, const ava_struct_field* fdef) noexcept;

  llvm::Value* invoke_s(
    const ava_xcode_global* target,
    size_t target_ix,
    llvm::Value*const* args,
    bool args_in_data_array) noexcept;

  llvm::Value* marshal_from(
    const ava_c_marshalling_type& marshal,
    llvm::Value* orig) noexcept;
  llvm::Value* marshal_to(
    const ava_c_marshalling_type& marshal,
    llvm::Value* raw) noexcept;

  llvm::Value* create_call_or_invoke(
    llvm::Value* callee,
    llvm::ArrayRef<llvm::Value*> args) noexcept;

  void do_return(llvm::Value* value,
                 const ava_pcg_fun* pcfun) noexcept;
};

AVA_END_FILE_PRIVATE

std::string ava::get_init_fun_name(
  const std::string& package, const std::string& module
) {
  ava_demangled_name n;

  n.scheme = ava_nms_ava;
  n.name = ava_string_of_cstring(
    get_unmangled_init_fun_name(package, module).c_str());
  return ava_string_to_cstring(ava_name_mangle(n));
}

std::string ava::get_unmangled_init_fun_name(
  const std::string& package, const std::string& module
) {
  return package + module + "(init)";
}

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
  ava_string package_prefix,
  llvm::LLVMContext& llvm_context,
  std::string& error)
const noexcept {
  ava_string full_module_name = ava_strcat(package_prefix, module_name);

  std::unique_ptr<llvm::Module> module(
    new llvm::Module(llvm::StringRef(ava_string_to_cstring(full_module_name)),
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
    llvm_context, *module, filename, module_name, package_prefix, full_debug);
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

  return module;
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
    llvm::parseIR(buffer->getMemBufferRef(), diagnostic, llvm_context));

  if (!driver_module.get()) {
    error = diagnostic.getMessage();
    return false;
  }

  /* returns *true* on error */
  if (dst.linkInModule(driver_module.get())) {
    /* As of 3.7, it's no longer possible to find out *why* this failed.
     * Thanks, LLVM.
     */
    error = "Linking driver failed, but LLVM won't tell us why.";
    return false;
  }

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
    if (fun.getName() != "main" && !fun.empty() && !fun.isIntrinsic()) {
      fun.addFnAttr(llvm::Attribute::AlwaysInline);
      fun.setUnnamedAddr(true);
      fun.setVisibility(llvm::GlobalValue::DefaultVisibility);
      fun.setLinkage(llvm::GlobalValue::PrivateLinkage);
      /* Clang when compiling C sets "nounwind" on all the functions it emits,
       * even though there's nothing stopping C functions from calling things
       * that do unwind.
       */
      fun.removeFnAttr(llvm::Attribute::NoUnwind);
    }

    declared_symbols.insert(fun.getName());
  }
}

static void make_global_locations(
  ava_xcode_translation_context& context,
  const ava_xcode_global_list* xcode)
noexcept {
  llvm::DebugLoc dl = llvm::DebugLoc::get(0, 0, context.di_file);
  llvm::DIFile* file = context.di_file;
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
  llvm::DIFile* di_file = context.di_global_files[ix];
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
      di_file, ava_string_to_cstring(v->name.name),
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
      di_file, ava_string_to_cstring(v->name.name),
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

  case ava_pcgt_decl_sxt: {
    const ava_pcg_decl_sxt* v = (const ava_pcg_decl_sxt*)pcode;

    return ava_xcode_to_ir_declare_sxt(
      context, ix, v->def, error);
  } break;

  case ava_pcgt_load_pkg:
  case ava_pcgt_load_mod:
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
  ava_string filename, ava_string module_name_, ava_string package_prefix_,
  bool full_debug)
: llvm_context(llvm_context_),
  module(module_),
  module_name(ava_string_to_cstring(module_name_)),
  package_prefix(ava_string_to_cstring(package_prefix_)),
  dib(module_),
  types(llvm_context_, module_),
  di(module_),
  ea(module_, types)
{
  string_type = module.getGlobalVariable("ava_string_type");
  integer_type = module.getGlobalVariable("ava_integer_type");

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

  const llvm::DataLayout dl = module.getDataLayout();
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

  di_void = nullptr;
#define DI_INT(signedness, type)                                \
  di_##signedness##_##type = dib.createBasicType(               \
    #signedness "_" #type, dl.getTypeSizeInBits(types.type),    \
    dl.getABITypeAlignment(types.type),                         \
    llvm::dwarf::DW_ATE_##signedness)
#define DI_INTS(type) DI_INT(signed, type); DI_INT(unsigned, type)
  DI_INTS(c_byte);
  DI_INTS(c_short);
  DI_INTS(c_int);
  DI_INTS(c_long);
  DI_INTS(c_llong);
  DI_INTS(ava_byte);
  DI_INTS(ava_short);
  DI_INTS(ava_int);
  DI_INTS(ava_long);
  DI_INTS(ava_integer);
  DI_INTS(c_size);
  DI_INTS(c_intptr);
  DI_INTS(c_atomic);
#undef DI_INTS
#undef DI_INT
#define DI_FLOAT(type)                          \
  di_##type = dib.createBasicType(              \
    #type, dl.getTypeSizeInBits(types.type),    \
    dl.getABITypeAlignment(types.type),         \
    llvm::dwarf::DW_ATE_float)
  DI_FLOAT(c_float);
  DI_FLOAT(c_double);
  DI_FLOAT(c_ldouble);
  DI_FLOAT(ava_real);
#undef DI_FLOAT
  di_c_string = dib.createPointerType(
    dib.createBasicType(
      "char", 1, 1, llvm::dwarf::DW_ATE_unsigned_char),
    dl.getTypeSizeInBits(types.general_pointer),
    dl.getABITypeAlignment(types.general_pointer),
    "c_string");
  di_general_pointer = di_c_string;

  llvm::DISubrange* ava_value_subscript = dib.getOrCreateSubrange(
    0, types.ava_value->getNumElements());
  llvm::DINodeArray ava_value_subscript_array = dib.getOrCreateArray(
    ava_value_subscript);
  di_ava_value = dib.createVectorType(
    dl.getTypeSizeInBits(types.ava_value),
    dl.getABITypeAlignment(types.ava_value),
    di_unsigned_ava_long, ava_value_subscript_array);
  di_ava_function_ptr = dib.createPointerType(
    dib.createUnspecifiedType("ava_function"),
    dl.getPointerSizeInBits(),
    dl.getPointerABIAlignment());
  di_ava_fat_list_value = dib.createStructType(
    /* Not actually defined here, but there's no real reason to track it as
     * such.
     */
    di_compile_unit, "ava_fat_list_value", di_file, 0,
    dl.getTypeSizeInBits(types.ava_fat_list_value),
    dl.getABITypeAlignment(types.ava_fat_list_value),
    0, nullptr,
    dib.getOrCreateArray({
      dib.createPointerType(dib.createUnspecifiedType("ava_list_trait"),
                            dl.getPointerSizeInBits(),
                            dl.getPointerABIAlignment()),
      di_ava_value
    }));
  llvm::DICompositeType* di_ava_function_parameter_type =
    dib.createEnumerationType(
      di_compile_unit, "ava_function_parameter_type", di_file, 0,
      dl.getTypeSizeInBits(types.c_int),
      dl.getABITypeAlignment(types.c_int),
      dib.getOrCreateArray(
        { dib.createEnumerator("ava_fpt_static", ava_fpt_static),
          dib.createEnumerator("ava_fpt_dynamic", ava_fpt_dynamic),
          dib.createEnumerator("ava_fpt_spread", ava_fpt_spread) }),
      dib.createBasicType("int", dl.getTypeSizeInBits(types.c_int),
                          dl.getABITypeAlignment(types.c_int),
                          llvm::dwarf::DW_ATE_unsigned));
  di_ava_function_parameter = dib.createStructType(
    di_compile_unit, "ava_function_parameter", di_file, 0,
    dl.getTypeSizeInBits(types.ava_function_parameter),
    dl.getABITypeAlignment(types.ava_function_parameter),
    0, nullptr,
    dib.getOrCreateArray({ di_ava_function_parameter_type, di_ava_value }));

  llvm::Metadata* di_inline_args[AVA_CC_AVA_MAX_INLINE_ARGS+1] = {
    di_ava_value,
    di_ava_value, di_ava_value, di_ava_value, di_ava_value,
    di_ava_value, di_ava_value, di_ava_value, di_ava_value,
  };
  for (unsigned i = 0; i <= AVA_CC_AVA_MAX_INLINE_ARGS; ++i)
    di_subroutine_inline_args[i] = dib.createSubroutineType(
      di_file, dib.getOrCreateTypeArray(
        llvm::ArrayRef<llvm::Metadata*>(di_inline_args, i + 1)));
  di_subroutine_array_args = dib.createSubroutineType(
    di_file, dib.getOrCreateTypeArray(
      { di_ava_value, di_unsigned_c_size, dib.createPointerType(
          di_ava_value, dl.getPointerSizeInBits()) }));
}

llvm::DIFile* ava_xcode_translation_context::get_di_file(
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
   *
   * If the function is not published, it is never accessed by name and may
   * coexist with other functions of the same same.
   */
  fun = llvm::GlobalValue::InternalLinkage == linkage? nullptr :
    context.module.getFunction(mangled_name);
  if (!fun) {
    fun = llvm::Function::Create(
      fun_type, linkage,
      mangled_name, &context.module);

    if (will_be_defined)
      fun->setUnnamedAddr(true);
  } else {
    /* TODO This probably shouldn't be an assert, but a normal error */
    assert(!will_be_defined);
  }
  context.global_funs[ix] = fun;

  /* Global constant ava_function referencing this function.
   *
   * In theory, the below would be nice.
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
   *
   * However, LinkOnceODR doesn't work well if there's a possibility of PIC and
   * non-PIC code being mixed, since PIC will expect the global to be a pointer
   * to the actual value, whereas non-PIC will interpret the global directly.
   *
   * Therefore, just make all of them internal for now.
   */
  llvm::GlobalValue::LinkageTypes funvar_linkage =
    /* Logic described above
    llvm::GlobalValue::InternalLinkage == linkage?
    llvm::GlobalValue::InternalLinkage :
    llvm::GlobalValue::LinkOnceODRLinkage;
    */
    llvm::GlobalValue::InternalLinkage;

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

static bool ava_xcode_to_ir_declare_sxt(
  ava_xcode_translation_context& context,
  size_t ix, const ava_struct* sxt,
  std::string& error)
noexcept {
  llvm::StructType* struct_type;
  llvm::ArrayType* union_type;

  if (!(struct_type = ava_xcode_to_ir_make_struct_type(
          context, sxt, error)))
    return false;

  context.global_sxt_structs[ix] = struct_type;

  if (sxt->is_union) {
    union_type = ava_xcode_to_ir_make_union_type(
      context, struct_type);
    context.global_sxt_unions[ix] = union_type;
  }

  return true;
}

static llvm::StructType* ava_xcode_to_ir_make_struct_type(
  ava_xcode_translation_context& context,
  const ava_struct* sxt,
  std::string& error)
noexcept {
  llvm::Type* members[sxt->num_fields + !!sxt->parent];
  size_t i;

  if (sxt->parent) {
    if (!(members[0] = ava_xcode_to_ir_make_struct_type(
            context, sxt->parent, error)))
      return nullptr;
  }

  for (i = 0; i < sxt->num_fields; ++i) {
    if (!(members[i + !!sxt->parent] =
          ava_xcode_to_ir_make_struct_member(
            context, sxt->name, sxt->fields + i, error)))
      return nullptr;
  }

  return llvm::StructType::get(
    context.llvm_context,
    llvm::ArrayRef<llvm::Type*>(members, ava_lenof(members)));
}

static llvm::Type* ava_xcode_to_ir_make_struct_member(
  ava_xcode_translation_context& context,
  ava_string struct_name,
  const ava_struct_field* field,
  std::string& error)
noexcept {
  switch (field->type) {
  case ava_sft_int: {
    if (AVA_STRUCT_NATURAL_ALIGNMENT != field->v.vint.alignment &&
        AVA_STRUCT_NATIVE_ALIGNMENT != field->v.vint.alignment) {
      error = ava_string_to_cstring(
        ava_error_native_field_with_unnatural_alignment_unsupported(
          struct_name, field->name));
      return nullptr;
    }

    switch (field->v.vint.size) {
    case ava_sis_ava_integer:
      return context.types.ava_integer;
    case ava_sis_word:
      return context.types.c_atomic;
    case ava_sis_byte:
      return context.types.ava_byte;
    case ava_sis_short:
      return context.types.ava_short;
    case ava_sis_int:
      return context.types.ava_int;
    case ava_sis_long:
      return context.types.ava_long;
    case ava_sis_c_short:
      return context.types.c_short;
    case ava_sis_c_int:
      return context.types.c_int;
    case ava_sis_c_long:
      return context.types.c_long;
    case ava_sis_c_llong:
      return context.types.c_llong;
    case ava_sis_c_size:
      return context.types.c_size;
    case ava_sis_c_intptr:
      return context.types.c_intptr;
    }

    /* unreachable */
    abort();
  } break;

  case ava_sft_real: {
    if (AVA_STRUCT_NATURAL_ALIGNMENT != field->v.vreal.alignment &&
        AVA_STRUCT_NATIVE_ALIGNMENT != field->v.vreal.alignment) {
      error = ava_string_to_cstring(
        ava_error_native_field_with_unnatural_alignment_unsupported(
          struct_name, field->name));
      return nullptr;
    }

    switch (field->v.vreal.size) {
    case ava_srs_ava_real: return context.types.ava_real;
    case ava_srs_single: return context.types.c_float;
    case ava_srs_double: return context.types.c_double;
    case ava_srs_extended: return context.types.c_ldouble;
    }

    /* unreachable */
    abort();
  } break;

  case ava_sft_ptr: {
    return context.types.general_pointer;
  } break;

  case ava_sft_hybrid: {
    /* ava_string is the canonical hybrid type in the C API */
    return context.types.ava_string;
  } break;

  case ava_sft_value: {
    return context.types.ava_value;
  } break;

  case ava_sft_compose:
  case ava_sft_array:
  case ava_sft_tail: {
    llvm::StructType* struct_type = ava_xcode_to_ir_make_struct_type(
      context, field->v.vcompose.member, error);
    if (!struct_type) return nullptr;

    llvm::Type* inner;
    if (field->v.vcompose.member->is_union)
      inner = ava_xcode_to_ir_make_union_type(
        context, struct_type);
    else
      inner = struct_type;

    return llvm::ArrayType::get(inner, field->v.vcompose.array_length);
  } break;
  }

  /* unreachable */
  abort();
}

static llvm::ArrayType* ava_xcode_to_ir_make_union_type(
  ava_xcode_translation_context& context,
  llvm::StructType* sxt)
noexcept {
  size_t max_size = 0, max_align = 1, i, n, size, align;
  llvm::Type* member, * elt;
  const llvm::DataLayout& layout(context.module.getDataLayout());

  n = sxt->getStructNumElements();
  for (i = 0; i < n; ++i) {
    member = sxt->getStructElementType(i);
    size = layout.getTypeAllocSize(member);
    align = layout.getABITypeAlignment(member);

    if (size > max_size) max_size = size;
    if (align > max_align) max_align = align;
  }

  max_size = (max_size + max_align - 1) / max_align * max_align;
  /* Go hunting for a type which happens to have this alignment, since LLVM
   * doesn't provide any way to say "array of N bytes with alignment X".
   *
   * Clang accomplishes this by adding its own padding in the containing
   * struct...
   */
#define TYPE(type)                                                      \
  if (max_align == layout.getABITypeAlignment(context.types.type))      \
    elt = context.types.type
  TYPE(ava_byte);
  else TYPE(ava_short);
  else TYPE(ava_int);
  else TYPE(general_pointer);
  else TYPE(ava_long);
  else TYPE(c_atomic);
  else TYPE(ava_real);
  else TYPE(ava_value);
  else abort();
#undef TYPE

  assert(0 == max_size % layout.getTypeAllocSize(elt));

  return llvm::ArrayType::get(elt, max_size / layout.getTypeAllocSize(elt));
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
  case ava_cmpt_strange: return context.types.general_pointer;
  case ava_cmpt_pointer: return context.types.general_pointer;
  }

  /* Unreachable */
  abort();
}

static llvm::Metadata* translate_marshalling_debug_type(
  const ava_xcode_translation_context& context,
  const ava_c_marshalling_type& type)
noexcept {
  switch (type.primitive_type) {
  case ava_cmpt_void: return context.di_void;
  case ava_cmpt_byte: return context.di_signed_c_byte;
  case ava_cmpt_ubyte: return context.di_unsigned_c_byte;
  case ava_cmpt_short: return context.di_signed_c_short;
  case ava_cmpt_ushort: return context.di_unsigned_c_short;
  case ava_cmpt_int: return context.di_signed_c_int;
  case ava_cmpt_uint: return context.di_unsigned_c_int;
  case ava_cmpt_long: return context.di_signed_c_long;
  case ava_cmpt_ulong: return context.di_unsigned_c_long;
  case ava_cmpt_llong: return context.di_signed_c_llong;
  case ava_cmpt_ullong: return context.di_unsigned_c_llong;
  case ava_cmpt_ava_sbyte: return context.di_signed_ava_byte;
  case ava_cmpt_ava_ubyte: return context.di_unsigned_ava_byte;
  case ava_cmpt_ava_sshort: return context.di_signed_ava_short;
  case ava_cmpt_ava_ushort: return context.di_unsigned_ava_short;
  case ava_cmpt_ava_sint: return context.di_signed_ava_int;
  case ava_cmpt_ava_uint: return context.di_unsigned_ava_int;
  case ava_cmpt_ava_slong: return context.di_signed_ava_long;
  case ava_cmpt_ava_ulong: return context.di_unsigned_ava_long;
  case ava_cmpt_ava_integer: return context.di_signed_ava_integer;
  case ava_cmpt_size: return context.di_unsigned_c_size;
  case ava_cmpt_float: return context.di_c_float;
  case ava_cmpt_double: return context.di_c_double;
  case ava_cmpt_ldouble: return context.di_c_ldouble;
  case ava_cmpt_ava_real: return context.di_ava_real;
  case ava_cmpt_string: return context.di_c_string;
  case ava_cmpt_strange: return context.di_general_pointer;
  case ava_cmpt_pointer: return context.di_general_pointer;
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
    size_t size = (ava_strlen(string) +
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
      llvm::ConstantInt::get(context.types.c_size, ava_strlen(string)),
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

static bool is_normalised_integer(ava_value value) {
  try {
    return ava_value_equal(
      value, ava_value_of_integer(ava_integer_of_value(value, 0)));
  } catch (const ava_exception& ex) {
    if (&ava_format_exception == ex.type)
      return false;
    else
      throw;
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
    /* Until we actually have type inferrence, encode everything as a string
     * unless it is already a normalised integer.
     */
    llvm::Constant* vals[2];
    if (is_normalised_integer(value)) {
      vals[0] = llvm::ConstantExpr::getPtrToInt(
        context.integer_type, context.types.ava_long);
      vals[1] = llvm::ConstantInt::get(
        context.types.ava_long, ava_integer_of_value(value, 0));
    } else {
      ava_string string_value = ava_to_string(value);
      vals[0] = llvm::ConstantExpr::getPtrToInt(
        context.string_type, context.types.ava_long);
      vals[1] = get_ava_string_constant(context, string_value);
    }
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
      ava::get_init_fun_name(context.package_prefix, context.module_name),
      &context.module);
  }
}

static void build_init_function(
  ava_xcode_translation_context& context,
  llvm::Function* init_function,
  const ava_xcode_global_list* xcode)
noexcept {
  llvm::DISubprogram* di_fun = context.dib.createFunction(
    context.di_compile_unit,
    ava::get_unmangled_init_fun_name(
      context.package_prefix, context.module_name),
    init_function->getName(),
    context.di_file, 0,
    context.dib.createSubroutineType(
      context.di_file, context.dib.getOrCreateTypeArray({NULL})),
    false, true, 0, 0, true, init_function);

  llvm::BasicBlock* test_block = llvm::BasicBlock::Create(
    context.llvm_context, "", init_function);
  llvm::BasicBlock* already_init_block = llvm::BasicBlock::Create(
    context.llvm_context, "", init_function);
  llvm::BasicBlock* block = llvm::BasicBlock::Create(
    context.llvm_context, "", init_function);
  llvm::IRBuilder<true> irb(test_block);

  /* While we don't care that much about source locations in the initialisation
   * function, we still need to set *some* locations, as LLVM produces invalid
   * debug info (and sometimes crashes) when inlining is enabled and code
   * without source locations calls a function with source locations.
   */
  irb.SetCurrentDebugLocation(llvm::DebugLoc::get(0, 0, di_fun));

  llvm::GlobalVariable* module_already_init =
    new llvm::GlobalVariable(
      context.module, irb.getInt1Ty(), false,
      llvm::GlobalValue::PrivateLinkage,
      irb.getFalse());
  irb.CreateCondBr(
    irb.CreateLoad(module_already_init),
    already_init_block, block);

  irb.SetInsertPoint(already_init_block);
  irb.CreateRetVoid();

  irb.SetInsertPoint(block);
  irb.CreateStore(irb.getTrue(), module_already_init);

  /* Initialise other packages */
  llvm::FunctionType* init_fun_type = llvm::FunctionType::get(
    llvm::Type::getVoidTy(context.llvm_context), false);
  for (size_t i = 0; i < xcode->length; ++i) {
    if (ava_pcgt_load_pkg == xcode->elts[i].pc->type) {
      const ava_pcg_load_pkg* v = (const ava_pcg_load_pkg*)xcode->elts[i].pc;
      irb.CreateCall(
        context.module.getOrInsertFunction(
          ava::get_init_fun_name(
            ava_string_to_cstring(
              ava_strcat(
                v->name, AVA_ASCII9_STRING(":"))), ""),
          init_fun_type));
    }
  }
  for (size_t i = 0; i < xcode->length; ++i) {
    if (ava_pcgt_load_mod == xcode->elts[i].pc->type) {
      const ava_pcg_load_mod* v = (const ava_pcg_load_mod*)xcode->elts[i].pc;
      irb.CreateCall(
        context.module.getOrInsertFunction(
          ava::get_init_fun_name(
            context.package_prefix,
            ava_string_to_cstring(v->name)),
          init_fun_type));
    }
  }

  /* Initialise all globals */
  for (size_t i = 0; i < xcode->length; ++i) {
    switch (xcode->elts[i].pc->type) {
    case ava_pcgt_ext_var: {
      const ava_pcg_ext_var* v = (const ava_pcg_ext_var*)xcode->elts[i].pc;
      ava_string mangled_name = ava_name_mangle(v->name);

      irb.CreateCall(context.di.g_ext_var,
                     { context.global_vars[i],
                       get_ava_string_constant(
                         context, mangled_name) });
    } break;

    case ava_pcgt_ext_fun: {
      const ava_pcg_ext_fun* f = (const ava_pcg_ext_fun*)xcode->elts[i].pc;
      ava_string mangled_name = ava_name_mangle(f->name);

      irb.CreateCall(
        ava_cc_ava == f->prototype->calling_convention?
        context.di.g_ext_fun_ava : context.di.g_ext_fun_other,
        { context.global_vars[i],
          get_ava_string_constant(context, mangled_name) });
    } break;

    case ava_pcgt_var: {
      const ava_pcg_var* v = (const ava_pcg_var*)xcode->elts[i].pc;
      ava_string mangled_name = ava_name_mangle(v->name);

      irb.CreateCall(
        context.di.g_var,
        { context.global_vars[i],
          get_ava_string_constant(context, mangled_name),
          llvm::ConstantInt::get(context.types.ava_bool, v->publish) });
    } break;

    case ava_pcgt_fun: {
      const ava_pcg_fun* f = (const ava_pcg_fun*)xcode->elts[i].pc;
      ava_string mangled_name = ava_name_mangle(f->name);

      irb.CreateCall(
        context.di.g_fun_ava,
        { context.global_vars[i],
          get_ava_string_constant(context, mangled_name),
          llvm::ConstantInt::get(context.types.ava_bool, f->publish) });
    } break;

    case ava_pcgt_src_pos: {
      const ava_pcg_src_pos* p = (const ava_pcg_src_pos*)xcode->elts[i].pc;
      irb.SetCurrentDebugLocation(
        llvm::DebugLoc::get(p->start_line, p->start_column, di_fun));
    } break;

    case ava_pcgt_decl_sxt:
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
  llvm::DebugLoc global_init_loc,
  llvm::DIFile* in_file)
noexcept {
  const ava_pcg_fun* pcfun = (const ava_pcg_fun*)xcode->pc;
  const ava_xcode_function* xfun = xcode->fun;
  llvm::DISubroutineType* subroutine_type;

  if (ava_cc_ava == pcfun->prototype->calling_convention) {
    subroutine_type =
      pcfun->prototype->num_args <= AVA_CC_AVA_MAX_INLINE_ARGS?
      context.di_subroutine_inline_args[pcfun->prototype->num_args] :
      context.di_subroutine_array_args;
  } else {
    llvm::Metadata* types[1 + pcfun->prototype->num_args];
    /* Need to count types separately since "void" entries aren't actually
     * physical arguments.
     */
    size_t num_types = 1;
    types[0] = translate_marshalling_debug_type(
      context, pcfun->prototype->c_return_type);
    for (size_t i = 0; i < pcfun->prototype->num_args; ++i)
      if (ava_cmpt_void != pcfun->prototype->args[i].marshal.primitive_type)
        types[num_types++] = translate_marshalling_debug_type(
          context, pcfun->prototype->args[i].marshal);

    subroutine_type = context.dib.createSubroutineType(
      context.di_file, context.dib.getOrCreateTypeArray(
        llvm::ArrayRef<llvm::Metadata*>(types, num_types)));
  }

  llvm::DISubprogram* di_fun = context.dib.createFunction(
    in_file, ava_string_to_cstring(pcfun->name.name),
    dst->getName(), in_file, global_init_loc.getLine(),
    subroutine_type, !pcfun->publish, true, global_init_loc.getLine(),
    0, true, dst);
  llvm::DebugLoc init_loc = llvm::DebugLoc::get(
    global_init_loc.getLine(), global_init_loc.getCol(), di_fun);

  llvm::BasicBlock* init_block;
  /* Extra basic block at end which just does "ret ''" so that the translation
   * code can always assume it can fall through to something.
   */
  std::vector<llvm::BasicBlock*> basic_blocks(xfun->num_blocks + 1);
  std::map<std::pair<signed,signed>,llvm::BasicBlock*> landing_pads;
  std::vector<llvm::Value*> regs(xfun->reg_type_off[ava_prt_function+1]);
  /* Since ava_fat_list_value isn't first-class in the C ABI, we need some
   * temporary space we can make pointers to.
   */
  llvm::Value* tmplists[3];
  std::vector<llvm::Value*> caught_exceptions(xfun->num_caught_exceptions);
  char reg_name[16];
  const char* reg_names[xfun->reg_type_off[ava_prt_var+1]];

  dst->setPersonalityFn(context.ea.personality_fn);

  init_block = llvm::BasicBlock::Create(context.llvm_context, "init", dst);
  for (size_t i = 0; i <= xfun->num_blocks; ++i)
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

    llvm::DILocalVariable* di_var = context.dib.createLocalVariable(
      i < pcfun->prototype->num_args?
      llvm::dwarf::DW_TAG_arg_variable :
      llvm::dwarf::DW_TAG_auto_variable,
      di_fun, reg_names[i],
      in_file, init_loc.getLine(), context.di_ava_value, true,
      0, i < pcfun->prototype->num_args? i + 1 : 0);
    context.dib.insertDeclare(
      regs[i], di_var, context.dib.createExpression(),
      init_loc.get(), init_block);
  }
#define DEFREG(prefix, i, reg_type, llvm_type, debug_type) do {         \
    std::snprintf(reg_name, sizeof(reg_name), "(" #prefix "%d)", (int)i); \
    regs[i] = irb.CreateAlloca(                                         \
      context.types.llvm_type, nullptr, reg_name);                      \
  } while (0)
#define DEFREGS(prefix, reg_type, llvm_type, debug_type) do {    \
    for (size_t i = xfun->reg_type_off[reg_type];                \
         i < xfun->reg_type_off[reg_type+1]; ++i) {              \
      DEFREG(prefix, i, reg_type, llvm_type, debug_type);        \
    }                                                            \
  } while (0)
  DEFREGS(d, ava_prt_data, ava_value, di_ava_value);
  DEFREGS(i, ava_prt_int, ava_integer, di_signed_ava_integer);
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
      regs[i] = irb.CreateConstGEP1_32(
        parm_reg_base, i - xfun->reg_type_off[ava_prt_parm]);
    }
  }

  /* Allocate temporary list slots */
  for (size_t i = 0; i < 3; ++i)
    tmplists[i] = irb.CreateAlloca(context.types.ava_fat_list_value);

  /* Allocate caught-exception stack */
  for (size_t i = 0; i < xfun->num_caught_exceptions; ++i) {
    caught_exceptions[i] = irb.CreateAlloca(context.types.ava_exception);
  }

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

  ava_xcode_fun_xlate_info xi(
    context, irb, xfun, basic_blocks, landing_pads, 0,
    caught_exceptions, regs, tmplists, data_array_base);

  /* Fall through to the first block, or off the end of the function if it's
   * empty.
   */
  if (xfun->num_blocks > 0) {
    irb.CreateBr(basic_blocks[0]);
  } else {
    xi.do_return(context.empty_string_value, pcfun);
  }

  /* Initialise the final fall-through block */
  irb.SetInsertPoint(basic_blocks[xfun->num_blocks]);
  xi.do_return(context.empty_string_value, pcfun);

  for (size_t block_ix = 0; block_ix < xfun->num_blocks; ++block_ix) {
    xi.this_basic_block = block_ix;

    const ava_xcode_basic_block* block = xfun->blocks[block_ix];
    irb.SetInsertPoint(basic_blocks[block_ix]);

    if (0 == block_ix)
      /* Initialise arguments since we're in the first block.
       * We don't do this in the initialisation block above, since we don't
       * want to risk the true entry block with all the allocas getting split
       * (eg, by maybe-throwing conversions) since that would prevent LLVM from
       * raising the allocas into LLVM registers.
       */
      xi.copy_args_to_regs(pcfun, dst);

    if (!block->exception_stack) {
      /* Basic block is trivially unreachable */
      irb.CreateUnreachable();
      continue;
    }

    /* We used to add explicit stores of "undefined" to each register not
     * considered inialised at this point and immediately before leaving each
     * basic block, on the grounds that the instructions are ultimately free
     * and do nothing but give the optimiser more information.
     *
     * However, the sheer number of these that are produced for longer
     * functions bogs LLVM down, and essentially makes compilation of most
     * functions O(n**2) (assuming number of registers and number of basic
     * blocks are directly correlated, which they often are).
     *
     * Given how simplistic our evaluation of register initialisation is, it's
     * probably reasonable to expect that LLVM could come to the same
     * conclusions itself.
     */

    bool has_terminal = false;
    for (size_t instr_ix = 0; instr_ix < block->length; ++instr_ix) {
      const ava_pcode_exe* exe = block->elts[instr_ix];
      has_terminal |= xi.translate_instruction(
        exe, xcode, xcode_list, di_fun);
    }

    if (!has_terminal) {
      /* Fell off the end of the block */
      /* We also used to explicitly deinitialise registers here; see above for
       * why we no longer do this.
       */
      if (block_ix + 1 < xfun->num_blocks)
        irb.CreateBr(basic_blocks[block_ix + 1]);
      else
        xi.do_return(context.empty_string_value, pcfun);
    }
  }
}

static ava_string to_ava_string(llvm::StringRef ref) {
  return ava_string_of_bytes(ref.data(), ref.size());
}

#define INVOKE(callee, ...)                             \
  create_call_or_invoke((callee), { __VA_ARGS__ })

void ava_xcode_fun_xlate_info::copy_args_to_regs(
  const ava_pcg_fun* pcfun,
  llvm::Function* dst)
noexcept {
  size_t i, arg;
  llvm::Value* value;

  for (i = 0, arg = 0; i < pcfun->prototype->num_args; ++i) {
    if (ava_cc_ava != pcfun->prototype->calling_convention) {
      if (ava_cmpt_void == pcfun->prototype->args[i].marshal.primitive_type) {
        value = marshal_from(pcfun->prototype->args[i].marshal, nullptr);
      } else {
        value = marshal_from(pcfun->prototype->args[i].marshal,
                             get_argument(dst, arg++));
      }
    } else if (pcfun->prototype->num_args <= AVA_CC_AVA_MAX_INLINE_ARGS) {
      value = get_argument(dst, arg++);
    } else {
      value = irb.CreateLoad(irb.CreateConstGEP1_32(get_argument(dst, 1), i));
    }

    irb.CreateStore(value, regs[i]);
  }
}

bool ava_xcode_fun_xlate_info::translate_instruction(
  const ava_pcode_exe* exe,
  const ava_xcode_global* container,
  const ava_xcode_global_list* xcode,
  llvm::DISubprogram* scope)
noexcept {
  const ava_pcg_fun* pcfun = (const ava_pcg_fun*)container->pc;
  const llvm::DataLayout& layout(context.module.getDataLayout());

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
    store_register(p->dst, src, pcfun);
  } return false;

  case ava_pcxt_ld_imm_i: {
    const ava_pcx_ld_imm_i* p = (const ava_pcx_ld_imm_i*)exe;

    llvm::Value* src = llvm::ConstantInt::get(
      context.types.ava_integer, p->src);
    store_register(p->dst, src, pcfun);
  } return false;

  case ava_pcxt_ld_glob: {
    const ava_pcx_ld_glob* p = (const ava_pcx_ld_glob*)exe;

    llvm::Value* src = irb.CreateCall(
      ava_pcode_global_is_fun(xcode->elts[p->src].pc)?
      context.di.x_load_glob_fun : context.di.x_load_glob_var,
      { context.global_vars[p->src],
        get_ava_string_constant(
          context, to_ava_string(
            context.global_vars[p->src]->getName())) });
    store_register(p->dst, src, pcfun);
  } return false;

  case ava_pcxt_ld_reg_s: {
    const ava_pcx_ld_reg_s* p = (const ava_pcx_ld_reg_s*)exe;

    llvm::Value* src = load_register(
      p->src, pcfun, tmplists[0]);
    store_register(p->dst, src, pcfun);
  } return false;

  case ava_pcxt_ld_reg_u: {
    const ava_pcx_ld_reg_u* p = (const ava_pcx_ld_reg_u*)exe;

    llvm::Value* raw_src = load_register(
       p->src, pcfun, tmplists[0]);
    llvm::Value* src = convert_register(
      raw_src, p->dst.type, p->src.type, tmplists[0]);
    store_register(p->dst, src, pcfun);
  } return false;

  case ava_pcxt_ld_reg_d: {
    const ava_pcx_ld_reg_d* p = (const ava_pcx_ld_reg_d*)exe;

    llvm::Value* raw_src = load_register(
      p->src, pcfun, tmplists[0]);
    llvm::Value* src = convert_register(
      raw_src, p->dst.type, p->src.type, tmplists[0]);
    store_register(p->dst, src, pcfun);
  } return false;

  case ava_pcxt_ld_parm: {
    const ava_pcx_ld_parm* p = (const ava_pcx_ld_parm*)exe;

    llvm::Value* src = load_register(
      p->src, pcfun, nullptr);
    irb.CreateCall(
      context.di.x_store_p,
      { regs[p->dst.index], src,
        llvm::ConstantInt::get(context.types.ava_function_parameter_type,
                               p->spread? ava_fpt_spread : ava_fpt_static),
        llvm::ConstantInt::get(context.types.c_size, p->dst.index) });
  } return false;

  case ava_pcxt_set_glob: {
    const ava_pcx_set_glob* p = (const ava_pcx_set_glob*)exe;

    llvm::Value* src = load_register(
      p->src, pcfun, nullptr);
    irb.CreateCall(
      context.di.x_store_glob_var,
      { context.global_vars[p->dst],
        src, get_ava_string_constant(
          context,
          to_ava_string(context.global_vars[p->dst]->getName())) });
  } return false;

  case ava_pcxt_lempty: {
    const ava_pcx_lempty* p = (const ava_pcx_lempty*)exe;

    irb.CreateCall(context.di.x_lempty, tmplists[0]);
    store_register(p->dst, tmplists[0], pcfun);
  } return false;

  case ava_pcxt_lappend: {
    const ava_pcx_lappend* p = (const ava_pcx_lappend*)exe;

    llvm::Value* lsrc = load_register(
      p->lsrc, pcfun, tmplists[0]);
    llvm::Value* esrc = load_register(
      p->esrc, pcfun, nullptr);
    INVOKE(
      context.di.x_lappend,
      tmplists[1], lsrc, esrc);
    store_register(p->dst, tmplists[1], pcfun);
  } return false;

  case ava_pcxt_lcat: {
    const ava_pcx_lcat* p = (const ava_pcx_lcat*)exe;

    llvm::Value* left = load_register(
      p->left, pcfun, tmplists[0]);
    llvm::Value* right = load_register(
      p->right, pcfun, tmplists[1]);
    INVOKE(
      context.di.x_lcat,
      tmplists[2], left, right);
    store_register(p->dst, tmplists[2], pcfun);
  } return false;

  case ava_pcxt_lhead: {
    const ava_pcx_lhead* p = (const ava_pcx_lhead*)exe;

    llvm::Value* src = load_register(
      p->src, pcfun, tmplists[0]);
    llvm::Value* val = INVOKE(context.di.x_lhead, src);
    store_register(p->dst, val, pcfun);
  } return false;

  case ava_pcxt_lbehead: {
    const ava_pcx_lbehead* p = (const ava_pcx_lbehead*)exe;

    llvm::Value* src = load_register(
      p->src, pcfun, tmplists[0]);
    INVOKE(context.di.x_lbehead, tmplists[1], src);
    store_register(p->dst, tmplists[1], pcfun);
  } return false;

  case ava_pcxt_lflatten: {
    const ava_pcx_lflatten* p = (const ava_pcx_lflatten*)exe;

    llvm::Value* src = load_register(
      p->src, pcfun, tmplists[0]);
    INVOKE(context.di.x_lflatten, tmplists[1], src);
    store_register(p->dst, tmplists[1], pcfun);
  } return false;

  case ava_pcxt_lindex: {
    const ava_pcx_lindex* p = (const ava_pcx_lindex*)exe;

    llvm::Value* src = load_register(
      p->src, pcfun, tmplists[0]);
    llvm::Value* ix = load_register(
      p->ix, pcfun, nullptr);
    llvm::Value* val = INVOKE(
      context.di.x_lindex, src, ix,
      get_ava_string_constant(context, p->extype),
      get_ava_string_constant(context, p->exmessage));
    store_register(p->dst, val, pcfun);
  } return false;

  case ava_pcxt_llength: {
    const ava_pcx_llength* p = (const ava_pcx_llength*)exe;

    llvm::Value* src = load_register(
      p->src, pcfun, tmplists[0]);
    llvm::Value* val = irb.CreateCall(context.di.x_llength, src);
    store_register(p->dst, val, pcfun);
  } return false;

  case ava_pcxt_iadd_imm: {
    const ava_pcx_iadd_imm* p = (const ava_pcx_iadd_imm*)exe;

    llvm::Value* left = load_register(
      p->src, pcfun, nullptr);
    llvm::Value* right = llvm::ConstantInt::get(
      context.types.ava_integer, p->incr);
    llvm::Value* val = irb.CreateCall(context.di.x_iadd, { left, right });
    store_register(p->dst, val, pcfun);
  } return false;

  case ava_pcxt_icmp: {
    const ava_pcx_icmp* p = (const ava_pcx_icmp*)exe;

    llvm::Value* left = load_register(
      p->left, pcfun, nullptr);
    llvm::Value* right = load_register(
      p->right, pcfun, nullptr);
    llvm::Value* val = irb.CreateCall(context.di.x_icmp, { left, right });
    store_register(p->dst, val, pcfun);
  } return false;

  case ava_pcxt_bool: {
    const ava_pcx_bool* p = (const ava_pcx_bool*)exe;

    llvm::Value* src = load_register(
      p->src, pcfun, nullptr);
    llvm::Value* val = irb.CreateCall(context.di.x_bool, src);

    store_register(p->dst, val, pcfun);
  } return false;

  case ava_pcxt_invoke_ss: {
    const ava_pcx_invoke_ss* p = (const ava_pcx_invoke_ss*)exe;

    llvm::Value* args[p->nargs];
    for (size_t i = 0; i < (size_t)p->nargs; ++i) {
      ava_pcode_register reg;
      reg.type = ava_prt_data;
      reg.index = p->base + i;
      args[i] = load_register(reg, pcfun, nullptr);
    }

    llvm::Value* ret = invoke_s(
      xcode->elts + p->fun, p->fun, args, false);

    store_register(p->dst, ret, pcfun);
  } return false;

  case ava_pcxt_invoke_sd: {
    const ava_pcx_invoke_sd* p = (const ava_pcx_invoke_sd*)exe;

    INVOKE(
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
      xcode->elts + p->fun, p->fun, args, true);

    store_register(p->dst, ret, pcfun);
  } return false;

  case ava_pcxt_invoke_dd: {
    const ava_pcx_invoke_dd* p = (const ava_pcx_invoke_dd*)exe;

    llvm::Value* target = load_register(
      p->fun, pcfun, nullptr);
    llvm::Value* ret = INVOKE(
      context.di.x_invoke_dd,
      target, regs[p->base],
      llvm::ConstantInt::get(context.types.c_size, p->nparms));

    store_register(p->dst, ret, pcfun);
  } return false;

  case ava_pcxt_partial: {
    const ava_pcx_partial* p = (const ava_pcx_partial*)exe;

    llvm::Value* src = load_register(
      p->src, pcfun, nullptr);
    for (size_t i = 0; i < (size_t)p->nargs; ++i) {
      ava_pcode_register reg;
      reg.type = ava_prt_data;
      reg.index = p->base + i;
      llvm::Value* val = load_register(
        reg, pcfun, nullptr);
      irb.CreateStore(
        val, irb.CreateConstGEP1_32(data_array_base, i));
    }

    llvm::Value* val = INVOKE(
      context.di.x_partial,
      src, data_array_base,
      llvm::ConstantInt::get(context.types.c_size, p->nargs));

    store_register(p->dst, val, pcfun);
  } return false;

  case ava_pcxt_ret: {
    const ava_pcx_ret* p = (const ava_pcx_ret*)exe;

    llvm::Value* src = load_register(
      p->return_value, pcfun, nullptr);
    do_return(src, pcfun);
  } return true;

  case ava_pcxt_branch: {
    const ava_pcx_branch* p = (const ava_pcx_branch*)exe;

    llvm::Value* key = load_register(
      p->key, pcfun, nullptr);
    llvm::Value* value = llvm::ConstantInt::get(
      context.types.ava_integer, p->value);
    llvm::Value* test;
    if (p->invert)
      test = irb.CreateICmpNE(key, value);
    else
      test = irb.CreateICmpEQ(key, value);

    irb.CreateCondBr(test, basic_blocks[p->target],
                     basic_blocks[this_basic_block+1]);
  } return true;

  case ava_pcxt_goto: {
    const ava_pcx_goto* p = (const ava_pcx_goto*)exe;

    irb.CreateBr(basic_blocks[p->target]);
  } return true;

  case ava_pcxt_try: {
  } return false;

  case ava_pcxt_yrt: {
    const ava_xcode_basic_block* bb = xfun->blocks[this_basic_block];
    if (bb->exception_stack->current_exception !=
        bb->exception_stack->next->current_exception) {
      context.ea.drop(irb, context.di);
    }
  } return false;

  case ava_pcxt_rethrow: {
    INVOKE(context.ea.cxa_rethrow);
    irb.CreateUnreachable();
  } return true;

  case ava_pcxt_throw: {
    const ava_pcx_throw* p = (const ava_pcx_throw*)exe;

    llvm::Value* type = llvm::ConstantInt::get(
      context.types.ava_integer, p->ex_type);
    llvm::Value* val = load_register(
      p->ex_value, pcfun, nullptr);
    INVOKE(context.di.x_throw, type, val);
    irb.CreateUnreachable();
  } return true;

  case ava_pcxt_ex_type: {
    const ava_pcx_ex_type* p = (const ava_pcx_ex_type*)exe;
    const ava_xcode_basic_block* bb = xfun->blocks[this_basic_block];

    llvm::Value* type = irb.CreateCall(
      context.di.x_ex_type,
      caught_exceptions[bb->exception_stack->current_exception]);
    store_register(p->dst, type, pcfun);
  } return false;

  case ava_pcxt_ex_value: {
    const ava_pcx_ex_value* p = (const ava_pcx_ex_value*)exe;
    const ava_xcode_basic_block* bb = xfun->blocks[this_basic_block];

    llvm::Value* type = irb.CreateCall(
      context.di.x_ex_value,
      caught_exceptions[bb->exception_stack->current_exception]);
    store_register(p->dst, type, pcfun);
  } return false;

  case ava_pcxt_cpu_pause: {
    irb.CreateCall(context.di.x_cpu_pause);
  } return false;

  case ava_pcxt_S_new_s: {
    const ava_pcx_S_new_s* p = (const ava_pcx_S_new_s*)exe;
    llvm::Type* type = get_sxt_type(xcode, p->sxt);
    llvm::Value* allocated = irb.CreateAlloca(type);
    if (p->zero_init) {
      irb.CreateStore(llvm::ConstantAggregateZero::get(type), allocated);
    }
    store_strangelet(p->dst, allocated, pcfun);
  } return false;

  case ava_pcxt_S_new_sa: {
    const ava_pcx_S_new_sa* p = (const ava_pcx_S_new_sa*)exe;
    llvm::Type* type = get_sxt_type(xcode, p->sxt);
    llvm::Value* length = load_register(p->length, pcfun, nullptr);
    llvm::Value* allocated = irb.CreateAlloca(type, length);
    if (p->zero_init) {
      llvm::Constant* struct_size = llvm::ConstantInt::get(
        context.types.ava_integer, layout.getTypeAllocSize(type));
      irb.CreateMemSet(
        allocated, llvm::ConstantInt::get(
          context.types.ava_byte, 0),
        irb.CreateNUWMul(length, struct_size),
        layout.getABITypeAlignment(type));
    }
    store_strangelet(p->dst, allocated, pcfun);
  } return false;

  case ava_pcxt_S_new_st: {
    const ava_pcx_S_new_st* p = (const ava_pcx_S_new_st*)exe;
    llvm::Type* type = get_sxt_type(xcode, p->sxt);
    llvm::Type* struct_type = context.global_sxt_structs[p->sxt];
    llvm::Type* tail_type = struct_type->getStructElementType(
      struct_type->getStructNumElements() - 1)
      ->getArrayElementType();

    llvm::Value* length = load_register(p->tail_length, pcfun, nullptr);
    llvm::Value* base_size = llvm::ConstantInt::get(
      context.types.ava_integer, layout.getTypeAllocSize(type));
    llvm::Value* tail_elt_size = llvm::ConstantInt::get(
      context.types.ava_integer, layout.getTypeAllocSize(tail_type));
    llvm::Value* total_size = irb.CreateNUWAdd(
      base_size, irb.CreateNUWMul(tail_elt_size, length));
    llvm::Value* allocated = irb.CreateAlloca(
      context.types.ava_byte, total_size);
    if (p->zero_init) {
      irb.CreateMemSet(
        allocated, llvm::ConstantInt::get(
          context.types.ava_byte, 0), total_size,
        layout.getABITypeAlignment(type));
    }
    store_strangelet(p->dst, allocated, pcfun);
  } return false;

  case ava_pcxt_S_new_h: {
    const ava_pcx_S_new_h* p = (const ava_pcx_S_new_h*)exe;
    llvm::Type* type = get_sxt_type(xcode, p->sxt);
    llvm::Value* size = llvm::ConstantInt::get(
      context.types.c_size, layout.getTypeAllocSize(type));
    llvm::Value* allocated = INVOKE(
      context.di.x_new,
      size,
      llvm::ConstantInt::get(context.types.ava_bool, p->atomic),
      llvm::ConstantInt::get(context.types.ava_bool, p->precise),
      llvm::ConstantInt::get(context.types.ava_bool, p->zero_init));
    store_strangelet(p->dst, allocated, pcfun);
  } return false;

  case ava_pcxt_S_new_ha: {
    const ava_pcx_S_new_ha* p = (const ava_pcx_S_new_ha*)exe;
    llvm::Type* type = get_sxt_type(xcode, p->sxt);
    llvm::Value* struct_size = llvm::ConstantInt::get(
      context.types.c_size, layout.getTypeAllocSize(type));
    llvm::Value* length =
      irb.CreateTrunc(load_register(p->length, pcfun, nullptr),
                      context.types.c_size);
    llvm::Value* total_size = irb.CreateNUWMul(struct_size, length);
    llvm::Value* allocated = INVOKE(
      context.di.x_new,
      total_size,
      llvm::ConstantInt::get(context.types.ava_bool, p->atomic),
      llvm::ConstantInt::get(context.types.ava_bool, p->precise),
      llvm::ConstantInt::get(context.types.ava_bool, p->zero_init));
    store_strangelet(p->dst, allocated, pcfun);
  } return false;

  case ava_pcxt_S_new_ht: {
    const ava_pcx_S_new_ht* p = (const ava_pcx_S_new_ht*)exe;
    llvm::Type* type = get_sxt_type(xcode, p->sxt);
    llvm::Type* struct_type = context.global_sxt_structs[p->sxt];
    llvm::Type* tail_type = struct_type->getStructElementType(
      struct_type->getStructNumElements() - 1)
      ->getArrayElementType();

    llvm::Value* length = load_register(p->tail_length, pcfun, nullptr);
    llvm::Value* base_size = llvm::ConstantInt::get(
      context.types.ava_integer, layout.getTypeAllocSize(type));
    llvm::Value* tail_elt_size = llvm::ConstantInt::get(
      context.types.ava_integer, layout.getTypeAllocSize(tail_type));
    llvm::Value* total_size = irb.CreateNUWAdd(
      base_size, irb.CreateNUWMul(tail_elt_size, length));
    llvm::Value* allocated = INVOKE(
      context.di.x_new,
      total_size,
      llvm::ConstantInt::get(context.types.ava_bool, p->atomic),
      llvm::ConstantInt::get(context.types.ava_bool, p->precise),
      llvm::ConstantInt::get(context.types.ava_bool, p->zero_init));
    store_strangelet(p->dst, allocated, pcfun);
  } return false;

  case ava_pcxt_S_sizeof: {
    const ava_pcx_S_sizeof* p = (const ava_pcx_S_sizeof*)exe;
    llvm::Type* type = get_sxt_type(xcode, p->sxt);
    llvm::Value* size_val = llvm::ConstantInt::get(
      context.types.ava_integer, layout.getTypeAllocSize(type));
    store_register(p->dst, size_val, pcfun);
  } return false;

  case ava_pcxt_S_alignof: {
    const ava_pcx_S_alignof* p = (const ava_pcx_S_alignof*)exe;
    llvm::Type* type = get_sxt_type(xcode, p->sxt);
    llvm::Value* align_val = llvm::ConstantInt::get(
      context.types.ava_integer, layout.getABITypeAlignment(type));
    store_register(p->dst, align_val, pcfun);
  } return false;

  case ava_pcxt_S_get_sp: {
    const ava_pcx_S_get_sp* p = (const ava_pcx_S_get_sp*)exe;
    llvm::Function* stacksave = llvm::Intrinsic::getDeclaration(
      &context.module, llvm::Intrinsic::stacksave);
    llvm::Value* ptr = irb.CreateCall(stacksave);
    store_strangelet(p->dst, ptr, pcfun);
  } return false;

  case ava_pcxt_S_set_sp: {
    const ava_pcx_S_set_sp* p = (const ava_pcx_S_set_sp*)exe;
    llvm::Function* stackrestore = llvm::Intrinsic::getDeclaration(
      &context.module, llvm::Intrinsic::stackrestore);
    llvm::Value* ptr = load_strangelet(p->src, pcfun);
    irb.CreateCall(stackrestore, { ptr });
  } return false;

  case ava_pcxt_S_cpy: {
    const ava_pcx_S_cpy* p = (const ava_pcx_S_cpy*)exe;
    llvm::Type* type = get_sxt_type(xcode, p->sxt);
    llvm::Value* src_ptr = irb.CreateBitCast(
      load_strangelet(p->src, pcfun), type->getPointerTo());
    llvm::Value* dst_ptr = irb.CreateBitCast(
      load_strangelet(p->dst, pcfun), type->getPointerTo());
    llvm::Value* value = irb.CreateLoad(src_ptr);
    irb.CreateStore(value, dst_ptr);
  } return false;

  case ava_pcxt_S_cpy_a: {
    const ava_pcx_S_cpy_a* p = (const ava_pcx_S_cpy_a*)exe;
    llvm::Type* type = get_sxt_type(xcode, p->sxt);
    llvm::Value* struct_size = llvm::ConstantInt::get(
      context.types.ava_integer, layout.getTypeAllocSize(type));
    llvm::Value* count = load_register(p->count, pcfun, nullptr);
    llvm::Value* size = irb.CreateNUWMul(struct_size, count);

    llvm::Value* src_base = irb.CreateBitCast(
      load_strangelet(p->src, pcfun), type->getPointerTo());
    llvm::Value* src_off = load_register(p->src_off, pcfun, nullptr);
    llvm::Value* src = irb.CreateInBoundsGEP(src_base, { src_off });

    llvm::Value* dst_base = irb.CreateBitCast(
      load_strangelet(p->dst, pcfun), type->getPointerTo());
    llvm::Value* dst_off = load_register(p->dst_off, pcfun, nullptr);
    llvm::Value* dst = irb.CreateInBoundsGEP(dst_base, { dst_off });

    irb.CreateMemCpy(dst, src, size, layout.getABITypeAlignment(type));
  } return false;

  case ava_pcxt_S_cpy_t: {
    const ava_pcx_S_cpy_t* p = (const ava_pcx_S_cpy_t*)exe;
    llvm::Type* type = get_sxt_type(xcode, p->sxt);
    llvm::Type* tail_type = type->getStructElementType(
      type->getStructNumElements() - 1)
      ->getArrayElementType();
    llvm::Value* struct_size = llvm::ConstantInt::get(
      context.types.ava_integer, layout.getTypeAllocSize(type));
    llvm::Value* tail_size = llvm::ConstantInt::get(
      context.types.ava_integer, layout.getTypeAllocSize(tail_type));
    llvm::Value* tail_length = load_register(p->tail_length, pcfun, nullptr);
    llvm::Value* size = irb.CreateNUWAdd(
      struct_size, irb.CreateNUWMul(tail_size, tail_length));

    llvm::Value* src = load_strangelet(p->src, pcfun);
    llvm::Value* dst = load_strangelet(p->dst, pcfun);
    irb.CreateMemCpy(dst, src, size, layout.getABITypeAlignment(type));
  } return false;

  case ava_pcxt_S_i_ld: {
    const ava_pcx_S_i_ld* p = (const ava_pcx_S_i_ld*)exe;
    const ava_struct_field* fdef = get_sxt_field_def(xcode, p->sxt, p->field);
    llvm::Value* src = load_strangelet(p->src, pcfun);
    llvm::Value* field_ptr = get_sxt_field_ptr(src, xcode, p->sxt, p->field);
    llvm::Value* value = irb.CreateLoad(field_ptr, p->volatil);
    /* We support unnatural byte orders for integer fields here since some
     * network APIs insist on using big-endian integers instead of native
     * integers (*cough* Berkley sockets).
     */
    if (ava_sbo_native != fdef->v.vint.byte_order &&
        ava_sbo_preferred != fdef->v.vint.byte_order &&
        ava_sbo_little == fdef->v.vint.byte_order != layout.isLittleEndian())
      value = irb.CreateCall(llvm::Intrinsic::getDeclaration(
                               &context.module,
                               llvm::Intrinsic::bswap, { value->getType() }),
                             { value });
    value = expand_to_ava_integer(value, fdef);
    store_register(p->dst, value, pcfun);
  } return false;

  case ava_pcxt_S_i_st: {
    const ava_pcx_S_i_st* p = (const ava_pcx_S_i_st*)exe;
    const ava_struct_field* fdef = get_sxt_field_def(xcode, p->sxt, p->field);

    llvm::Value* value = load_register(p->src, pcfun, nullptr);
    value = irb.CreateTrunc(value, get_sxt_field_type(p->sxt, p->field));
    if (ava_sbo_native != fdef->v.vint.byte_order &&
        ava_sbo_preferred != fdef->v.vint.byte_order &&
        ava_sbo_little == fdef->v.vint.byte_order != layout.isLittleEndian())
      value = irb.CreateCall(llvm::Intrinsic::getDeclaration(
                               &context.module,
                               llvm::Intrinsic::bswap, { value->getType() }),
                             { value });

    llvm::Value* dst = load_strangelet(p->dst, pcfun);
    llvm::Value* field_ptr = get_sxt_field_ptr(dst, xcode, p->sxt, p->field);
    irb.CreateStore(value, field_ptr, p->volatil);
  } return false;

  case ava_pcxt_S_ia_ld: {
    const ava_pcx_S_ia_ld* p = (const ava_pcx_S_ia_ld*)exe;
    const ava_struct_field* fdef = get_sxt_field_def(xcode, p->sxt, p->field);

    llvm::AtomicOrdering order = get_llvm_ordering(p->order);
    switch (order) {
    case llvm::Release:
    case llvm::AcquireRelease:
      /* Not supported, promote to cstseq */
      order = llvm::SequentiallyConsistent;
      break;

    default: /* OK */ break;
    }

    llvm::Value* src = load_strangelet(p->src, pcfun);
    llvm::Value* field_ptr = get_sxt_field_ptr(src, xcode, p->sxt, p->field);
    llvm::LoadInst* loaded = irb.CreateLoad(field_ptr, p->volatil);
    loaded->setOrdering(order);
    loaded->setAlignment(layout.getABITypeAlignment(context.types.c_atomic));
    llvm::Value* value = expand_to_ava_integer(loaded, fdef);
    store_register(p->dst, value, pcfun);
  } return false;

  case ava_pcxt_S_ia_st: {
    const ava_pcx_S_ia_st* p = (const ava_pcx_S_ia_st*)exe;

    llvm::AtomicOrdering order = get_llvm_ordering(p->order);
    switch (order) {
    case llvm::Acquire:
    case llvm::AcquireRelease:
      /* Not supported, promote to cstseq */
      order = llvm::SequentiallyConsistent;
      break;

    default: /* OK */ break;
    }

    llvm::Value* value = load_register(p->src, pcfun, nullptr);
    value = irb.CreateTrunc(value, get_sxt_field_type(p->sxt, p->field));

    llvm::Value* dst = load_strangelet(p->dst, pcfun);
    llvm::Value* field_ptr = get_sxt_field_ptr(dst, xcode, p->sxt, p->field);
    llvm::StoreInst* store = irb.CreateStore(value, field_ptr, p->volatil);
    store->setOrdering(order);
    store->setAlignment(layout.getABITypeAlignment(context.types.c_atomic));
  } return false;

  case ava_pcxt_S_ia_cas: {
    const ava_pcx_S_ia_cas* p = (const ava_pcx_S_ia_cas*)exe;
    const ava_struct_field* fdef = get_sxt_field_def(xcode, p->sxt, p->field);
    llvm::Type* field_type = get_sxt_field_type(p->sxt, p->field);

    llvm::AtomicOrdering succ_order, fail_order;
    figure_cas_orderings(&succ_order, &fail_order,
                         p->success_order, p->failure_order);

    llvm::Value* from_val = load_register(p->from, pcfun, nullptr);
    from_val = irb.CreateTrunc(from_val, field_type);
    llvm::Value* to_val = load_register(p->to, pcfun, nullptr);
    to_val = irb.CreateTrunc(to_val, field_type);

    llvm::Value* dst = load_strangelet(p->dst, pcfun);
    llvm::Value* field_ptr = get_sxt_field_ptr(dst, xcode, p->sxt, p->field);
    llvm::AtomicCmpXchgInst* result = irb.CreateAtomicCmpXchg(
      field_ptr, from_val, to_val, succ_order, fail_order);
    result->setVolatile(p->volatil);
    result->setWeak(p->weak);

    llvm::Value* succeeded = irb.CreateZExt(
      irb.CreateExtractValue(result, { 1 }), context.types.ava_integer);
    store_register(p->success, succeeded, pcfun);

    llvm::Value* old = irb.CreateExtractValue(result, { 0 });
    old = expand_to_ava_integer(old, fdef);
    store_register(p->actual, old, pcfun);
  } return false;

  case ava_pcxt_S_ia_rmw: {
    const ava_pcx_S_ia_rmw* p = (const ava_pcx_S_ia_rmw*)exe;
    const ava_struct_field* fdef = get_sxt_field_def(xcode, p->sxt, p->field);
    llvm::Type* field_type = get_sxt_field_type(p->sxt, p->field);

    llvm::AtomicRMWInst::BinOp op;
    switch (p->op) {
    case ava_pro_xchg:  op = llvm::AtomicRMWInst::Xchg; break;
    case ava_pro_add:   op = llvm::AtomicRMWInst::Add;  break;
    case ava_pro_sub:   op = llvm::AtomicRMWInst::Sub;  break;
    case ava_pro_and:   op = llvm::AtomicRMWInst::And;  break;
    case ava_pro_nand:  op = llvm::AtomicRMWInst::Nand; break;
    case ava_pro_or:    op = llvm::AtomicRMWInst::Or;   break;
    case ava_pro_xor:   op = llvm::AtomicRMWInst::Xor;  break;
    case ava_pro_smax:  op = llvm::AtomicRMWInst::Max;  break;
    case ava_pro_smin:  op = llvm::AtomicRMWInst::Min;  break;
    case ava_pro_umax:  op = llvm::AtomicRMWInst::UMax; break;
    case ava_pro_umin:  op = llvm::AtomicRMWInst::UMin; break;
    }

    llvm::AtomicOrdering order = get_llvm_ordering(p->order);
    /* Undocumented, but LLVM requires the order to be at least monotonic */
    if (llvm::Unordered == order) order = llvm::Monotonic;

    llvm::Value* neu = load_register(p->src, pcfun, nullptr);
    neu = irb.CreateTrunc(neu, field_type);

    llvm::Value* dst = load_strangelet(p->dst, pcfun);
    llvm::Value* field_ptr = get_sxt_field_ptr(dst, xcode, p->sxt, p->field);
    llvm::AtomicRMWInst* rmw = irb.CreateAtomicRMW(op, field_ptr, neu, order);
    rmw->setVolatile(p->volatil);

    llvm::Value* result = expand_to_ava_integer(rmw, fdef);
    store_register(p->old, result, pcfun);
  } return false;

  case ava_pcxt_S_r_ld: {
    const ava_pcx_S_r_ld* p = (const ava_pcx_S_r_ld*)exe;
    const ava_struct_field* fdef = get_sxt_field_def(xcode, p->sxt, p->field);

    llvm::Value* src = load_strangelet(p->src, pcfun);
    llvm::Value* field_ptr = get_sxt_field_ptr(src, xcode, p->sxt, p->field);
    llvm::Value* loaded = irb.CreateLoad(field_ptr, p->volatil);
    /* No support for unnatural byte orders, since there's no precedent of any
     * API using them.
     */
    llvm::Function* cvt = nullptr;
    switch (fdef->v.vreal.size) {
    case ava_srs_single: cvt = context.di.marshal_from[ava_cmpt_float]; break;
    case ava_srs_double: cvt = context.di.marshal_from[ava_cmpt_double]; break;
    case ava_srs_ava_real: cvt = context.di.marshal_from[ava_cmpt_ava_real]; break;
    case ava_srs_extended: cvt = context.di.marshal_from[ava_cmpt_ldouble]; break;
    }
    llvm::Value* converted = irb.CreateCall(cvt, { loaded });
    store_register(p->dst, converted, pcfun);
  } return false;

  case ava_pcxt_S_r_st: {
    const ava_pcx_S_r_st* p = (const ava_pcx_S_r_st*)exe;
    const ava_struct_field* fdef = get_sxt_field_def(xcode, p->sxt, p->field);

    llvm::Value* src = load_register(p->src, pcfun, nullptr);
    llvm::Function* cvt = nullptr;
    switch (fdef->v.vreal.size) {
    case ava_srs_single: cvt = context.di.marshal_to[ava_cmpt_float]; break;
    case ava_srs_double: cvt = context.di.marshal_to[ava_cmpt_double]; break;
    case ava_srs_ava_real: cvt = context.di.marshal_to[ava_cmpt_ava_real]; break;
    case ava_srs_extended: cvt = context.di.marshal_to[ava_cmpt_ldouble]; break;
    }
    llvm::Value* converted = INVOKE(cvt, src);

    llvm::Value* dst = load_strangelet(p->dst, pcfun);
    llvm::Value* field_ptr = get_sxt_field_ptr(dst, xcode, p->sxt, p->field);
    irb.CreateStore(converted, field_ptr, p->volatil);
  } return false;

  case ava_pcxt_S_p_ld: {
    const ava_pcx_S_p_ld* p = (const ava_pcx_S_p_ld*)exe;
    const ava_struct_field* fdef = get_sxt_field_def(xcode, p->sxt, p->field);

    llvm::Value* src = load_strangelet(p->src, pcfun);
    llvm::Value* field_ptr = get_sxt_field_ptr(src, xcode, p->sxt, p->field);

    llvm::Value* ptr;
    if (ava_sft_ptr == fdef->type) {
      ptr = irb.CreateLoad(field_ptr, p->volatil);
    } else {
      assert(ava_sft_hybrid == fdef->type);
      ptr = irb.CreateLoad(field_ptr, p->volatil);
      ptr = irb.CreateTrunc(ptr, context.types.c_intptr);
      ptr = irb.CreateIntToPtr(ptr, context.types.general_pointer);
    }
    store_strangelet(p->dst, ptr, pcfun);
  } return false;

  case ava_pcxt_S_p_st: {
    const ava_pcx_S_p_st* p = (const ava_pcx_S_p_st*)exe;
    const ava_struct_field* fdef = get_sxt_field_def(xcode, p->sxt, p->field);

    llvm::Value* src = load_strangelet(p->src, pcfun);
    llvm::Value* dst = load_strangelet(p->dst, pcfun);
    llvm::Value* field_ptr = get_sxt_field_ptr(dst, xcode, p->sxt, p->field);

    if (ava_sft_ptr == fdef->type) {
      irb.CreateStore(src, field_ptr, p->volatil);
    } else {
      assert(ava_sft_hybrid == fdef->type);
      llvm::Value* h = irb.CreatePtrToInt(src, context.types.c_intptr);
      h = irb.CreateZExt(h, context.types.ava_string);
      irb.CreateStore(h, field_ptr, p->volatil);
    }
  } return false;

  case ava_pcxt_S_pa_ld: {
    const ava_pcx_S_pa_ld* p = (const ava_pcx_S_pa_ld*)exe;

    llvm::AtomicOrdering order = get_llvm_ordering(p->order);
    switch (order) {
    case llvm::Release:
    case llvm::AcquireRelease:
      /* Not supported, promote to cstseq */
      order = llvm::SequentiallyConsistent;
      break;

    default: /* OK */ break;
    }

    llvm::Value* src = load_strangelet(p->src, pcfun);
    llvm::Value* field_ptr = get_sxt_field_ptr(src, xcode, p->sxt, p->field);
    llvm::LoadInst* loaded = irb.CreateLoad(field_ptr, p->volatil);
    loaded->setOrdering(order);
    loaded->setAlignment(layout.getABITypeAlignment(
                           context.types.general_pointer));
    store_strangelet(p->dst, loaded, pcfun);
  } return false;

  case ava_pcxt_S_pa_st: {
    const ava_pcx_S_pa_st* p = (const ava_pcx_S_pa_st*)exe;

    llvm::AtomicOrdering order = get_llvm_ordering(p->order);
    switch (order) {
    case llvm::Acquire:
    case llvm::AcquireRelease:
      /* Not supported, promote to cstseq */
      order = llvm::SequentiallyConsistent;
      break;

    default: /* OK */ break;
    }

    llvm::Value* value = load_strangelet(p->src, pcfun);
    llvm::Value* dst = load_strangelet(p->dst, pcfun);
    llvm::Value* field_ptr = get_sxt_field_ptr(dst, xcode, p->sxt, p->field);
    llvm::StoreInst* store = irb.CreateStore(value, field_ptr, p->volatil);
    store->setOrdering(order);
    store->setAlignment(layout.getABITypeAlignment(
                          context.types.general_pointer));
  } return false;

  case ava_pcxt_S_pa_cas: {
    const ava_pcx_S_pa_cas* p = (const ava_pcx_S_pa_cas*)exe;

    llvm::AtomicOrdering succ_order, fail_order;
    figure_cas_orderings(&succ_order, &fail_order,
                         p->success_order, p->failure_order);

    llvm::Value* from_val = load_strangelet(p->from, pcfun);
    from_val = irb.CreatePtrToInt(from_val, context.types.c_intptr);
    llvm::Value* to_val = load_strangelet(p->to, pcfun);
    to_val = irb.CreatePtrToInt(to_val, context.types.c_intptr);

    llvm::Value* dst = load_strangelet(p->dst, pcfun);
    llvm::Value* field_ptr = get_sxt_field_ptr(dst, xcode, p->sxt, p->field);
    field_ptr = irb.CreateBitCast(
      field_ptr, context.types.c_intptr->getPointerTo());
    llvm::AtomicCmpXchgInst* result = irb.CreateAtomicCmpXchg(
      field_ptr, from_val, to_val, succ_order, fail_order);
    result->setVolatile(p->volatil);
    result->setWeak(p->weak);

    llvm::Value* succeeded = irb.CreateZExt(
      irb.CreateExtractValue(result, { 1 }), context.types.ava_integer);
    store_register(p->success, succeeded, pcfun);

    llvm::Value* old = irb.CreateExtractValue(result, { 0 });
    old = irb.CreateIntToPtr(old, context.types.general_pointer);
    store_strangelet(p->actual, old, pcfun);
  } return false;

  case ava_pcxt_S_pa_xch: {
    const ava_pcx_S_pa_xch* p = (const ava_pcx_S_pa_xch*)exe;

    llvm::Value* src = load_strangelet(p->src, pcfun);
    src = irb.CreatePtrToInt(src, context.types.c_intptr);
    llvm::Value* dst = load_strangelet(p->dst, pcfun);
    llvm::Value* field_ptr = irb.CreateBitCast(
      get_sxt_field_ptr(dst, xcode, p->sxt, p->field),
      context.types.c_intptr->getPointerTo());

    llvm::AtomicOrdering order = get_llvm_ordering(p->order);
    /* Undocumented, but unordered is not permitted */
    if (llvm::Unordered == order) order = llvm::Monotonic;

    llvm::AtomicRMWInst* xch = irb.CreateAtomicRMW(
      llvm::AtomicRMWInst::Xchg, field_ptr, src, order);
    xch->setVolatile(p->volatil);

    llvm::Value* result = irb.CreateIntToPtr(
      xch, context.types.general_pointer);
    store_strangelet(p->old, result, pcfun);
  } return false;

  case ava_pcxt_S_hi_ld: {
    const ava_pcx_S_hi_ld* p = (const ava_pcx_S_hi_ld*)exe;

    llvm::Value* src = load_strangelet(p->src, pcfun);
    llvm::Value* field_ptr = get_sxt_field_ptr(src, xcode, p->sxt, p->field);
    llvm::Value* loaded = irb.CreateLoad(field_ptr, p->volatil);
    store_register(p->dst, loaded, pcfun);
  } return false;

  case ava_pcxt_S_hi_st: {
    const ava_pcx_S_hi_st* p = (const ava_pcx_S_hi_st*)exe;

    llvm::Value* src = load_register(p->src, pcfun, nullptr);
    llvm::Value* dst = load_strangelet(p->dst, pcfun);
    llvm::Value* field_ptr = get_sxt_field_ptr(dst, xcode, p->sxt, p->field);
    irb.CreateStore(src, field_ptr, p->volatil);
  } return false;

  case ava_pcxt_S_hy_intp: {
    const ava_pcx_S_hy_intp* p = (const ava_pcx_S_hy_intp*)exe;

    llvm::Value* src = load_strangelet(p->src, pcfun);
    llvm::Value* field_ptr = get_sxt_field_ptr(src, xcode, p->sxt, p->field);
    llvm::Value* loaded = irb.CreateLoad(field_ptr, p->volatil);
    llvm::Value* result = irb.CreateAnd(
      loaded, llvm::ConstantInt::get(context.types.ava_integer, 1));
    store_register(p->dst, result, pcfun);
  } return false;

  case ava_pcxt_S_v_ld: {
    const ava_pcx_S_v_ld* p = (const ava_pcx_S_v_ld*)exe;

    llvm::Value* src = load_strangelet(p->src, pcfun);
    llvm::Value* field_ptr = get_sxt_field_ptr(src, xcode, p->sxt, p->field);
    llvm::Value* loaded = irb.CreateLoad(field_ptr, p->volatil);
    store_register(p->dst, loaded, pcfun);
  } return false;

  case ava_pcxt_S_v_st: {
    const ava_pcx_S_v_st* p = (const ava_pcx_S_v_st*)exe;

    llvm::Value* src = load_register(p->src, pcfun, nullptr);
    llvm::Value* dst = load_strangelet(p->dst, pcfun);
    llvm::Value* field_ptr = get_sxt_field_ptr(dst, xcode, p->sxt, p->field);
    irb.CreateStore(src, field_ptr, p->volatil);
  } return false;

  case ava_pcxt_S_gfp: {
    const ava_pcx_S_gfp* p = (const ava_pcx_S_gfp*)exe;

    llvm::Value* src = load_strangelet(p->src, pcfun);
    llvm::Value* result = get_sxt_field_ptr(src, xcode, p->sxt, p->field);
    store_strangelet(p->dst, result, pcfun);
  } return false;

  case ava_pcxt_S_gap: {
    const ava_pcx_S_gap* p = (const ava_pcx_S_gap*)exe;

    llvm::Value* base = load_strangelet(p->src, pcfun);
    base = irb.CreateBitCast(
      base, get_sxt_type(xcode, p->sxt)->getPointerTo());
    llvm::Value* index = load_register(p->index, pcfun, nullptr);
    llvm::Value* result = irb.CreateInBoundsGEP(base, { index });
    store_strangelet(p->dst, result, pcfun);
  } return false;

  case ava_pcxt_S_membar: {
    const ava_pcx_S_membar* p = (const ava_pcx_S_membar*)exe;

    llvm::AtomicOrdering order = get_llvm_ordering(p->order);
    /* Unordered and monotonic not allowed, promote to acqrel */
    if (llvm::Unordered == order || llvm::Monotonic == order)
      order = llvm::AcquireRelease;

    irb.CreateFence(order);
  } return false;

  }
}


void ava_xcode_fun_xlate_info::store_register(
  ava_pcode_register dst,
  llvm::Value* src,
  const ava_pcg_fun* pcfun)
noexcept {
  switch (dst.type) {
  case ava_prt_var:
    irb.CreateCall(
      context.di.x_store_v,
      { regs[dst.index], src,
        get_ava_string_constant(
          context, ava_to_string(ava_list_index_f(pcfun->vars, dst.index))) });
    break;

  case ava_prt_data:
    irb.CreateCall(
      context.di.x_store_d,
      { regs[dst.index], src,
        llvm::ConstantInt::get(context.types.c_size, dst.index) });
    break;

  case ava_prt_int:
    irb.CreateCall(
      context.di.x_store_i,
      { regs[dst.index], src,
        llvm::ConstantInt::get(context.types.c_size, dst.index) });
    break;

  case ava_prt_list:
    irb.CreateCall(
      context.di.x_store_l,
      { regs[dst.index], src,
        llvm::ConstantInt::get(context.types.c_size, dst.index) });
    break;

  case ava_prt_function:
    irb.CreateCall(
      context.di.x_store_f,
      { regs[dst.index], src,
        llvm::ConstantInt::get(context.types.c_size, dst.index) });
    break;

  case ava_prt_parm: abort();
  }
}

void ava_xcode_fun_xlate_info::store_strangelet(
  ava_pcode_register dst,
  llvm::Value* src,
  const ava_pcg_fun* pcfun)
noexcept {
  llvm::Value* cast = irb.CreateBitCast(src, context.types.general_pointer);
  llvm::Value* strangelet = irb.CreateCall(
    context.di.strangelet_of_pointer, { cast });
  store_register(dst, strangelet, pcfun);
}

llvm::Value* ava_xcode_fun_xlate_info::load_register(
  ava_pcode_register src,
  const ava_pcg_fun* pcfun,
  llvm::Value* tmplist)
noexcept {
  switch (src.type) {
  case ava_prt_var:
    return irb.CreateCall(
      context.di.x_load_v,
      { regs[src.index],
        get_ava_string_constant(
          context, ava_to_string(
            ava_list_index_f(pcfun->vars, src.index))) });

  case ava_prt_data:
    return irb.CreateCall(
      context.di.x_load_d,
      { regs[src.index],
        llvm::ConstantInt::get(context.types.c_size, src.index) });

  case ava_prt_int:
    return irb.CreateCall(
      context.di.x_load_i,
      { regs[src.index],
        llvm::ConstantInt::get(context.types.c_size, src.index) });

  case ava_prt_function:
    return irb.CreateCall(
      context.di.x_load_f,
      { regs[src.index],
        llvm::ConstantInt::get(context.types.c_size, src.index) });

  case ava_prt_list:
    irb.CreateCall(
      context.di.x_load_l,
      { tmplist, regs[src.index],
        llvm::ConstantInt::get(context.types.c_size, src.index) });
    return tmplist;

  case ava_prt_parm: abort();
  }
}

llvm::Value* ava_xcode_fun_xlate_info::load_strangelet(
  ava_pcode_register src,
  const ava_pcg_fun* pcfun)
noexcept {
  llvm::Value* strangelet = load_register(src, pcfun, nullptr);
  return irb.CreateCall(
    context.di.strangelet_to_pointer, { strangelet });
}

llvm::Value* ava_xcode_fun_xlate_info::convert_register(
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
      return INVOKE(context.di.x_conv_iv, src);

    case ava_prt_function:
      return INVOKE(context.di.x_conv_fv, src);

    case ava_prt_list:
      INVOKE(context.di.x_conv_lv, tmplist, src);
      return tmplist;

    case ava_prt_parm: abort();
    }
  }
}

llvm::Type* ava_xcode_fun_xlate_info::get_sxt_type(
  const ava_xcode_global_list* xcode,
  size_t struct_ix)
noexcept {
  const ava_struct* def;

  if (!ava_pcode_global_get_struct_def(&def, xcode->elts[struct_ix].pc, 0))
    abort();

  if (def->is_union)
    return context.global_sxt_unions[struct_ix];
  else
    return context.global_sxt_structs[struct_ix];
}

llvm::Type* ava_xcode_fun_xlate_info::get_sxt_field_type(
  size_t struct_ix, size_t field_ix)
noexcept {
  return context.global_sxt_structs[struct_ix]
    ->getStructElementType(field_ix);
}

const ava_struct* ava_xcode_fun_xlate_info::get_sxt_def(
  const ava_xcode_global_list* xcode,
  size_t struct_ix)
noexcept {
  const ava_struct* def;

  if (!ava_pcode_global_get_struct_def(&def, xcode->elts[struct_ix].pc, 0))
    abort();

  return def;
}

const ava_struct_field* ava_xcode_fun_xlate_info::get_sxt_field_def(
  const ava_xcode_global_list* xcode,
  size_t struct_ix,
  size_t field_ix)
noexcept {
  return get_sxt_def(xcode, struct_ix)->fields + field_ix;
}

llvm::Value* ava_xcode_fun_xlate_info::get_sxt_field_ptr(
  llvm::Value* base,
  const ava_xcode_global_list* xcode,
  size_t struct_ix,
  size_t field_ix)
noexcept {
  const ava_struct* def;

  if (!ava_pcode_global_get_struct_def(&def, xcode->elts[struct_ix].pc, 0))
    abort();

  if (def->is_union)
    return irb.CreateBitCast(
      base, context.global_sxt_structs[struct_ix]
      ->getStructElementType(field_ix)->getPointerTo());
  else
    return irb.CreateConstInBoundsGEP2_32(
      nullptr, irb.CreateBitCast(
        base, context.global_sxt_structs[struct_ix]->getPointerTo()),
      0, field_ix + !!def->parent);
}

llvm::Value* ava_xcode_fun_xlate_info::expand_to_ava_integer(
  llvm::Value* base, const ava_struct_field* fdef)
noexcept {
  if (fdef->v.vint.sign_extend)
    return irb.CreateSExt(base, context.types.ava_integer);
  else
    return irb.CreateZExt(base, context.types.ava_integer);
}

llvm::AtomicOrdering ava_xcode_fun_xlate_info::get_llvm_ordering(
  ava_pcode_memory_order pco)
noexcept {
  switch (pco) {
  case ava_pmo_unordered: return llvm::Unordered;
  case ava_pmo_monotonic: return llvm::Monotonic;
  case ava_pmo_acquire:   return llvm::Acquire;
  case ava_pmo_release:   return llvm::Release;
  case ava_pmo_acqrel:    return llvm::AcquireRelease;
  case ava_pmo_seqcst:    return llvm::SequentiallyConsistent;
  }

  /* unreachable */
  abort();
}

void ava_xcode_fun_xlate_info::figure_cas_orderings(
  llvm::AtomicOrdering* succ_dst, llvm::AtomicOrdering* fail_dst,
  ava_pcode_memory_order succ_src, ava_pcode_memory_order fail_src)
noexcept {
    llvm::AtomicOrdering fail_order = get_llvm_ordering(fail_src);
    switch (fail_order) {
    case llvm::Unordered:
      /* Not permitted, promote to Monotonic */
      fail_order = llvm::Monotonic;
      break;

    case llvm::Release:
    case llvm::AcquireRelease:
      /* Not permitted, promote to SequentiallyConsistent */
      fail_order = llvm::SequentiallyConsistent;
      break;

    default: /* OK */ break;
    }

    llvm::AtomicOrdering succ_order = get_llvm_ordering(succ_src);
    if (llvm::Unordered == succ_order)
      /* Not permitted, promote to Monotonic */
      succ_order = llvm::Monotonic;
    /* "the constraint on failure must be no stronger than that on success".
     * Failure is one of monotonic, acquire, or seqcst at this point. If it's
     * monotonic, the above clause is trivial, since success is already at
     * least monotonic. If it is seqcst, so must success be, for it is the
     * strongest order.
     *
     * For acquire, it seems debatable whether success may be set to release.
     * While acquire provides guarantees that release does not, the converse is
     * also true, so neither is stronger than the other.
     *
     * The actual assertion in the LLVM source is that (success >= failure),
     * based on the enum values, and this does in fact permit success of
     * release and failure of acquire. Thus, monotonic is the only thing that
     * need be promoted for success if failure is acquire.
     */
    if (llvm::SequentiallyConsistent == fail_order)
      succ_order = llvm::SequentiallyConsistent;
    else if (llvm::Acquire == fail_order && llvm::Monotonic == succ_order)
      succ_order = llvm::Release;

    *succ_dst = succ_order;
    *fail_dst = fail_order;
}

llvm::Value* ava_xcode_fun_xlate_info::invoke_s(
  const ava_xcode_global* target,
  size_t fun_ix,
  llvm::Value*const* args,
  bool args_in_data_array)
noexcept {
  llvm::Value* ret;
  const ava_function* prot;
  llvm::Value* fun_mangled_name = get_ava_string_constant(
    context, to_ava_string(context.global_funs[fun_ix]->getName()));

  if (!ava_pcode_global_get_prototype(&prot, target->pc, 0))
    abort();

  if (ava_cc_ava == prot->calling_convention) {
    irb.CreateCall(context.di.x_pre_invoke_s,
                   { context.global_vars[fun_ix],
                     fun_mangled_name });
    if (prot->num_args <= AVA_CC_AVA_MAX_INLINE_ARGS) {
      ret = INVOKE(context.global_funs[fun_ix],
                   llvm::ArrayRef<llvm::Value*>(args, prot->num_args));
    } else {
      if (!args_in_data_array) {
        for (size_t i = 0; i < prot->num_args; ++i) {
          irb.CreateStore(args[i], irb.CreateConstGEP1_32(data_array_base, i));
        }
      }
      ret = INVOKE(
        context.global_funs[fun_ix],
        llvm::ConstantInt::get(context.types.c_size, prot->num_args),
        data_array_base);
    }
  } else {
    llvm::Value* actual_args[prot->num_args];
    size_t actual_nargs = 0;

    for (size_t i = 0; i < prot->num_args; ++i) {
      if (ava_cmpt_void == prot->args[i].marshal.primitive_type)
        marshal_to(prot->args[i].marshal, args[i]);
      else
        actual_args[actual_nargs++] =
          marshal_to(prot->args[i].marshal, args[i]);
    }

    llvm::Value* native_return = INVOKE(
      context.global_funs[fun_ix],
      llvm::ArrayRef<llvm::Value*>(actual_args, actual_nargs));

    ret = marshal_from(
      prot->c_return_type,
      ava_cmpt_void == prot->c_return_type.primitive_type?
      nullptr : native_return);
  }

  irb.CreateCall(
    context.di.x_post_invoke_s,
    { context.global_vars[fun_ix], fun_mangled_name, ret });

  return ret;
}

static bool landing_pad_has_cleanup(const ava_xcode_exception_stack* s) {
  for (; s; s = s->next)
    if (s->landing_pad_is_cleanup)
      return true;

  return false;
}

static bool landing_pad_has_catch(const ava_xcode_exception_stack* s) {
  for (; s; s = s->next)
    if (!s->landing_pad_is_cleanup)
      return true;

  return false;
}

llvm::Value* ava_xcode_fun_xlate_info::marshal_from(
  const ava_c_marshalling_type& marshal,
  llvm::Value* raw)
noexcept {
  switch (marshal.primitive_type) {
  case ava_cmpt_void:
    return irb.CreateCall(context.di.marshal_from[ava_cmpt_void]);

  case ava_cmpt_pointer:
    return irb.CreateCall(
      context.di.marshal_from[ava_cmpt_pointer],
      { raw, get_pointer_prototype_constant(
          context, marshal.pointer_proto) });

  default:
    return irb.CreateCall(
      context.di.marshal_from[marshal.primitive_type], raw);
  }
}

llvm::Value* ava_xcode_fun_xlate_info::marshal_to(
  const ava_c_marshalling_type& marshal,
  llvm::Value* raw)
noexcept {
  switch (marshal.primitive_type) {
  case ava_cmpt_void:
    INVOKE(context.di.marshal_to[ava_cmpt_void], raw);
    return nullptr;

  case ava_cmpt_pointer:
    return INVOKE(
      context.di.marshal_to[ava_cmpt_pointer],
      raw, get_pointer_prototype_constant(
        context, marshal.pointer_proto));

  default:
    return INVOKE(
      context.di.marshal_to[marshal.primitive_type], raw);
  }
}

llvm::Value* ava_xcode_fun_xlate_info::create_call_or_invoke(
  llvm::Value* callee,
  llvm::ArrayRef<llvm::Value*> args)
noexcept {
  const ava_xcode_basic_block* bb, * dbb;
  signed lp_ix;
  llvm::BasicBlock* subsequent;
  llvm::Value* ret;
  std::pair<signed,signed> lp_key;

  bb = xfun->blocks[this_basic_block];
  lp_ix = bb->exception_stack->landing_pad;
  if (-1 == lp_ix && -1 == bb->exception_stack->current_exception) {
    ret = irb.CreateCall(callee, args);
  } else {
    lp_key = std::make_pair(bb->exception_stack->landing_pad,
                            bb->exception_stack->current_exception);
    if (!landing_pads[lp_key]) {
      if (-1 != lp_ix) {
        dbb = xfun->blocks[lp_ix];

        landing_pads[lp_key] = context.ea.create_landing_pad(
          irb.getCurrentDebugLocation(),
          basic_blocks[lp_ix],
          caught_exceptions[dbb->exception_stack->current_exception],
          /* Need to clean up all exceptions between that of the new target and
           * the location, inclusive.
           */
          1 + bb->exception_stack->current_exception -
          dbb->exception_stack->current_exception,
          bb->exception_stack->landing_pad_is_cleanup,
          landing_pad_has_cleanup(dbb->exception_stack),
          landing_pad_has_catch(dbb->exception_stack),
          context.di);
      } else {
        landing_pads[lp_key] = context.ea.create_cleanup(
          irb.GetInsertBlock(),
          irb.getCurrentDebugLocation(),
          1 + bb->exception_stack->current_exception,
          context.di);
      }
    }

    subsequent = llvm::BasicBlock::Create(
      context.llvm_context, "",
      basic_blocks[this_basic_block]->getParent(),
      irb.GetInsertBlock()->getNextNode());
    ret = irb.CreateInvoke(callee, subsequent, landing_pads[lp_key], args);

    irb.SetInsertPoint(subsequent);
  }

  return ret;
}

void ava_xcode_fun_xlate_info::do_return(
  llvm::Value* value,
  const ava_pcg_fun* pcfun)
noexcept {
  if (ava_cc_ava == pcfun->prototype->calling_convention) {
    irb.CreateRet(value);
  } else {
    value = marshal_to(pcfun->prototype->c_return_type, value);
    if (ava_cmpt_void == pcfun->prototype->c_return_type.primitive_type) {
      irb.CreateRetVoid();
    } else {
      irb.CreateRet(value);
    }
  }
}

AVA_END_FILE_PRIVATE
