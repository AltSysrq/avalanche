/*
  Simple test program which compiles stdin to LLVM IR and dumps the resulting
  module to stdout.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <memory>
#include <string>

#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>

#include "runtime/avalanche.h"
#include "bsd.h"
#define AVA__INTERNAL_INCLUDE 1
#include "runtime/-llvm-support/translation.hxx"
#include "runtime/-llvm-support/drivers.h"

static ava_value run(void* filename) {
  llvm::LLVMContext llvm_context;
  ava::xcode_to_ir_translator xlator;
  FILE* infile;
  ava_string source;
  char buff[4096];
  size_t nread;
  ava_compile_error_list errors;
  ava_xcode_global_list* xcode;
  ava_value ret = ava_empty_list().v;
  std::string error;

  TAILQ_INIT(&errors);

  infile = fopen((const char*)filename, "r");
  if (!infile) err(EX_NOINPUT, "fopen");

  source = AVA_EMPTY_STRING;
  do {
    nread = fread(buff, 1, sizeof(buff), infile);
    source = ava_string_concat(
      source, ava_string_of_bytes(buff, nread));
  } while (nread == sizeof(buff));
  fclose(infile);

  ava_compile_file(NULL, &xcode, &errors,
                   AVA_ASCII9_STRING("input:"),
                   ava_string_of_cstring((const char*)filename),
                   source);

  if (!TAILQ_EMPTY(&errors)) {
    warnx("Compilation failed.\n%s",
          ava_string_to_cstring(
            ava_error_list_to_string(&errors, 50, ava_true)));
    return ret;
  }

  xlator.add_driver(ava_driver_isa_unchecked_data,
                    ava_driver_isa_unchecked_size);
  xlator.add_driver(ava_driver_main_data, ava_driver_main_size);
  std::unique_ptr<llvm::Module> output = xlator.translate(
    xcode, ava_string_of_cstring((const char*)filename),
    AVA_ASCII9_STRING("input"), llvm_context, error);
  if (!output.get()) {
    warnx("Translation failed: %s", error.c_str());
    return ret;
  }

  output->dump();
  return ret;
}

int main(int argc, char** argv) {
  ava_invoke_in_context(run, argv[1]);
  return 0;
}
