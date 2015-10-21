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

#include <string>

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>

#define AVA__INTERNAL_INCLUDE 1
#include "../avalanche/defs.h"
#include "../avalanche/function.h"
#include "driver-iface.hxx"

ava::driver_iface::driver_iface(const llvm::Module& module) noexcept {
#define ISA(target) do {                                                \
    if (!(target = module.getFunction("ava_isa_" #target "$"))) {       \
      error = "ABI function " #target " not found";                     \
    }                                                                   \
  } while (0)

  ISA(g_ext_var);
  ISA(g_ext_fun_ava);
  ISA(g_ext_fun_other);
  ISA(g_var);
  ISA(g_fun_ava);
  ISA(x_load_v);
  ISA(x_load_d);
  ISA(x_load_i);
  ISA(x_load_f);
  ISA(x_load_l);
  ISA(x_load_glob_var);
  ISA(x_load_glob_fun);
  ISA(x_store_v);
  ISA(x_store_d);
  ISA(x_store_i);
  ISA(x_store_f);
  ISA(x_store_l);
  ISA(x_store_p);
  ISA(x_store_glob_var);
  ISA(x_conv_vi);
  ISA(x_conv_iv);
  ISA(x_conv_vf);
  ISA(x_conv_fv);
  ISA(x_conv_vl);
  ISA(x_conv_lv);
  ISA(x_lempty);
  ISA(x_lappend);
  ISA(x_lcat);
  ISA(x_lhead);
  ISA(x_lbehead);
  ISA(x_lflatten);
  ISA(x_lindex);
  ISA(x_llength);
  ISA(x_iadd);
  ISA(x_icmp);
  ISA(x_aaempty);
  ISA(x_pre_invoke_s);
  ISA(x_post_invoke_s);
  ISA(x_invoke_sd_bind);
  ISA(x_invoke_dd);
  ISA(x_partial);
  ISA(x_bool);
#undef ISA

#define MAR(type) do {                                            \
    if (!(marshal_to[ava_cmpt_##type] = module.getFunction(       \
            "ava_isa_m_to_" #type "$"))) {                        \
      error = "ABI function ava_isa_m_to_" #type "$ not found";   \
    }                                                             \
    if (!(marshal_from[ava_cmpt_##type] = module.getFunction(     \
            "ava_isa_m_from_" #type "$"))) {                      \
      error = "ABI function ava_isa_m_from_" #type "$ not found"; \
    }                                                             \
  } while (0)
  MAR(void);
  MAR(byte);
  MAR(short);
  MAR(int);
  MAR(long);
  MAR(llong);
  MAR(ubyte);
  MAR(ushort);
  MAR(uint);
  MAR(ulong);
  MAR(ullong);
  MAR(ava_sbyte);
  MAR(ava_sshort);
  MAR(ava_sint);
  MAR(ava_slong);
  MAR(ava_ubyte);
  MAR(ava_ushort);
  MAR(ava_ulong);
  MAR(ava_integer);
  MAR(size);
  MAR(float);
  MAR(double);
  MAR(ldouble);
  MAR(ava_real);
  MAR(string);
  MAR(pointer);

  program_entry = module.getFunction("a$$5Cprogram_entry");
}
