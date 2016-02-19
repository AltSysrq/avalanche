/*
  Simple test program which compiles stdin to LLVM IR and dumps the resulting
  module (in bitcode) to stdout.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <memory>
#include <string>
#include <iostream>

#include <llvm/Bitcode/BitcodeWriterPass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>

#include "runtime/avalanche.h"
#include "../../common/bsd.h"
#define AVA__INTERNAL_INCLUDE 1
#include "runtime/-llvm-support/translation.hxx"
#include "runtime/-llvm-support/optimisation.hxx"
#include "runtime/-llvm-support/drivers.h"

static ava_value run(void* filename) {
  llvm::LLVMContext llvm_context;
  ava::xcode_to_ir_translator xlator;
  ava_compile_error_list errors;
  ava_xcode_global_list* xcode;
  ava_value ret = ava_empty_list().v;
  std::string error;
  ava_compenv* compenv;

  TAILQ_INIT(&errors);

  compenv = ava_compenv_new(AVA_ASCII9_STRING("input:"));
  ava_compenv_use_simple_source_reader(compenv, AVA_EMPTY_STRING);
  ava_compenv_use_standard_macsub(compenv);
  ava_compenv_compile_file(NULL, &xcode, compenv,
                           ava_string_of_cstring((const char*)filename),
                           &errors, NULL);

  if (!TAILQ_EMPTY(&errors)) {
    warnx("Compilation failed.\n%s",
          ava_string_to_cstring(
            ava_error_list_to_string(&errors, 50, ava_true)));
    return ret;
  }

  xlator.add_driver(ava_driver_isa_unchecked_data,
                    ava_driver_isa_unchecked_size);
  xlator.add_driver(ava_driver_avast_unchecked_data,
                    ava_driver_avast_unchecked_size);
  xlator.add_driver(ava_driver_main_data, ava_driver_main_size);
  std::unique_ptr<llvm::Module> output = xlator.translate(
    xcode, ava_string_of_cstring((const char*)filename),
    AVA_ASCII9_STRING("input"), AVA_ASCII9_STRING("input"),
    llvm_context, error);
  if (!output.get()) {
    warnx("Translation failed: %s", error.c_str());
    return ret;
  }

  ava::optimise_module(*output, 3);

  {
    llvm::raw_fd_ostream bcout(1, false, false);
    llvm::ModulePassManager passManager;
    passManager.addPass(llvm::BitcodeWriterPass(bcout));
    passManager.run(*output);
  }

  return ret;
}

int main(int argc, char** argv) {
  ava_invoke_in_context(run, argv[1]);
  return 0;
}
