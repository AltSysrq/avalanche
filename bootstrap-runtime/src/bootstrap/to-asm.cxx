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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <memory>
#include <string>

#include <llvm/ADT/Triple.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/CodeGen/LinkAllAsmWriterComponents.h>
#include <llvm/CodeGen/LinkAllCodegenComponents.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/MCTargetOptions.h>
#include <llvm/Pass.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

#define AVA__INTERNAL_INCLUDE 1
#include "../runtime/avalanche/defs.h"
AVA_BEGIN_DECLS
#include "../runtime/avalanche/defs.h"
#include "../runtime/avalanche/string.h"
#include "../runtime/avalanche/value.h"
#include "../runtime/avalanche/context.h"
#include "../runtime/avalanche/errors.h"
#include "../runtime/avalanche/pcode.h"
#include "../runtime/avalanche/pcode-validation.h"
#include "../runtime/avalanche/map.h"
AVA_END_DECLS
#include "../runtime/-llvm-support/translation.hxx"
#include "../runtime/-llvm-support/optimisation.hxx"
#include "../../../common/bsd.h"

#include "common.h"

/*

  Avalanche-to-Assembly Translator

  Usage: to-asm drivers... pcode-file

  Each listed driver file is read and added to the translation context in
  sequence. This program does not provide the ISA dirver itself, so the ISA
  driver must be passed on the command-line. The final argument is interpreted
  as a P-Code file and translated to assembly. Assembler output is written to
  stdout.

  The native code is optimised at the maximum level.

  This program assumes it is operating on at least a whole library. The package
  prefix is derived from the pcode-file, and the module name is the empty string.

 */

static llvm::LLVMContext llvm_context;

static void slurp_driver(const char** data_dst, size_t* size_dest,
                         const char* infile);
static ava_string derive_package_prefix(ava_string infile);
static void dump_assembly(llvm::Module& module);

static ava_value main_impl(void* arg) {
  unsigned argc = ((const main_data*)arg)->argc;
  const char*const* argv = ((const main_data*)arg)->argv;

  unsigned i;
  ava::xcode_to_ir_translator xlator;
  std::unique_ptr<llvm::Module> module;
  std::string xlate_error;
  const char* driver_data;
  size_t driver_size;
  ava_string pcode_file;
  const ava_pcode_global_list* pcode;
  const ava_xcode_global_list* xcode;
  ava_compile_error_list errors;

  if (argc < 3)
    errx(EX_USAGE, "Usage: %s <driver>... <pcode-file>", argv[0]);

  for (i = 1; i < argc - 1; ++i) {
    slurp_driver(&driver_data, &driver_size, argv[i]);
    xlator.add_driver(driver_data, driver_size);
  }

  pcode_file = ava_string_of_cstring(argv[argc-1]);
  pcode = slurp(pcode_file);
  TAILQ_INIT(&errors);
  xcode = ava_xcode_from_pcode(pcode, &errors, ava_empty_map());
  if (!TAILQ_EMPTY(&errors))
    errx(EX_DATAERR, "Input P-Code is invalid.\n%s",
         ava_string_to_cstring(
           ava_error_list_to_string(&errors, 50, ava_false)));

  module = xlator.translate(
    xcode, AVA_ABSENT_STRING, AVA_EMPTY_STRING,
    derive_package_prefix(pcode_file), llvm_context, xlate_error);
  if (!module)
    errx(EX_DATAERR, "Translation failed: %s", xlate_error.c_str());

  ava::optimise_module(*module, 3);
  dump_assembly(*module);

  return ava_value_of_string(AVA_EMPTY_STRING);
}

static void slurp_driver(const char** data_dst, size_t* size_dst,
                         const char* infile) {
  ava_string text;

  text = slurp_file(ava_string_of_cstring(infile));

  *data_dst = ava_string_to_cstring(text);
  *size_dst = ava_strlen(text);
}

static ava_string derive_package_prefix(ava_string infile) {
  const char* base, * dot;

  base = ava_string_to_cstring(infile);
  dot = strrchr(base, '.');
  if (!dot)
    errx(EX_USAGE, "Bad input filename: %s", base);

  return ava_strcat(ava_string_slice(infile, 0, dot - base),
                           AVA_ASCII9_STRING(":"));
}

static void dump_assembly(llvm::Module& module) {
  std::string error;

  /* See also LLVM's llc.cpp, since their docs don't actually mention how to do
   * simple, common things.
   */

  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  llvm::Triple triple(module.getTargetTriple());
  const llvm::Target* target = llvm::TargetRegistry::lookupTarget(
    "", triple, error);
  if (!target)
    errx(EX_SOFTWARE, "Couldn't find target for triple %s: %s",
         module.getTargetTriple().c_str(), error.c_str());

  llvm::TargetOptions options;
  options.DisableIntegratedAS = true;
  options.MCOptions.AsmVerbose = true;
  options.MCOptions.MCNoExecStack = true;

  std::unique_ptr<llvm::TargetMachine> target_machine(
    target->createTargetMachine(triple.getTriple(), "", "", options,
                                llvm::Reloc::PIC_,
                                llvm::CodeModel::Default,
                                llvm::CodeGenOpt::Aggressive));

  llvm::legacy::PassManager pm;
  llvm::TargetLibraryInfoImpl tlii(triple);
  pm.add(new llvm::TargetLibraryInfoWrapperPass(tlii));

  /* We need to new this because as of LLVM 3.7 (or 3.6?) the AsmPrinter thinks
   * it owns it and deletes it.
   */
  llvm::raw_fd_ostream* stdout = new llvm::raw_fd_ostream(1, false, false);
  target_machine->addPassesToEmitFile(
    pm, *stdout, llvm::TargetMachine::CGFT_AssemblyFile, false,
    nullptr, nullptr);
  pm.run(module);
}
