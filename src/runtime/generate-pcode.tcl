#! /usr/bin/env tclsh8.6
#-
# Copyright (c) 2015 Jason Lingle
#
# Permission to  use, copy,  modify, and/or distribute  this software  for any
# purpose  with or  without fee  is hereby  granted, provided  that the  above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE  IS PROVIDED "AS  IS" AND  THE AUTHOR DISCLAIMS  ALL WARRANTIES
# WITH  REGARD   TO  THIS  SOFTWARE   INCLUDING  ALL  IMPLIED   WARRANTIES  OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT  SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL,  DIRECT,   INDIRECT,  OR  CONSEQUENTIAL  DAMAGES   OR  ANY  DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
# OF  CONTRACT, NEGLIGENCE  OR OTHER  TORTIOUS ACTION,  ARISING OUT  OF OR  IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# Generates definitions and boilerplate implementation of P-Code.
#
# Specifically, can be used to output two files:
# - avalanche/gen-pcode.h, containing enums, structs, and builder function
#   dclarations.
# - gen-pcode.c, containing builder function implementations, to_string
#   implementations, and of_string implementations. The generated code
#   does not permit construction of values outside their local constraints.
#
# Usage:
#   generate-pcode.tcl {header|impl} <pcode-defs.tcl >output.c

set context global
set structs {}

proc struct {args} {
  struct-$::context {*}$args
}

proc attr {args} {
  attr-$::context {*}$args
}

proc prop {args} {
  prop-$::context {*}$args
}

proc struct-global {name mne body} {
  global current_struct context structs

  set current_struct {}
  dict set current_struct name $name
  dict set current_struct mne $mne
  dict set current_struct attrs {}
  dict set current_struct props {}
  dict set current_struct elts {}
  set context struct
  eval $body
  set context global

  lappend structs $current_struct
}

proc attr-struct {name} {
  dict lappend ::current_struct attrs $name
}

proc prop-struct {type name} {
  dict set ::current_struct props $name $type
}

proc parent {name mne} {
  dict set ::current_struct parent [list $name $mne]
}

proc elt {name body} {
  global current_struct context current_elt

  if {[string length $name] > 9} {
    error "Element name $name is longer than 9 chars"
  }

  set current_elt {}
  dict set current_elt fields {}
  dict set current_elt constraints {}
  dict set current_elt attrs {}
  set context elt
  eval $body
  set context struct
  dict set current_struct elts $name $current_elt
}

proc attr-elt {name} {
  dict lappend ::current_elt attrs $name
}

proc int {args} {
  add-field int {*}$args
}

proc str {args} {
  add-field str {*}$args
}

proc demangled-name {args} {
  add-field demangled-name {*}$args
}

proc function {args} {
  add-field function {*}$args
}

proc bool {args} {
  add-field bool {*}$args
}

proc ava-list {args} {
  add-field list {*}$args
}

proc register-type {types name args} {
  add-field register-type $name {*}$args
  add-register-type-constraint $types $name
}

set REGISTER_MNE {
  v ava_prt_var
  d ava_prt_data
  i ava_prt_int
  f ava_prt_function
  l ava_prt_list
  p ava_prt_parm
}

proc register {types name args} {
  add-field register $name {*}$args
  add-register-type-constraint $types $name.type
}

proc add-register-type-constraint {types name} {
  set constr {}
  set name [string map {- _} $name]
  for {set i 0} {$i < [string length $types]} {incr i} {
    set t [dict get $::REGISTER_MNE [string index $types $i]]
    if {$constr ne {}} {
      append constr " || "
    }
    append constr "@.$name == $t"
  }
  constraint $constr
}

proc struct-elt {name mne args} {
  add-field [list $name $mne] {*}$args
}

proc add-field {type name {body {}}} {
  global current_elt context current_field

  set current_field {}
  dict set current_field name $name
  dict set current_field type $type
  dict set current_field props {}
  set context field
  eval $body
  set context elt
  dict lappend current_elt fields $current_field
}

proc constraint {str} {
  dict lappend ::current_elt constraints $str
}

proc prop-field {name} {
  dict lappend ::current_field props $name
}

eval [read stdin]

proc putm {template args} {
  set repl {}
  foreach {from to} $args {
    lappend repl $from [string map {- _} $to]
    lappend repl %$from $to
  }
  lappend repl "\\{" "{" "\\}" "}" "\\(" "(" "\\)" ")"
  puts [string map $repl $template]
}

proc is-substruct {type} {
  expr {2 == [llength $type]}
}

set SIMPLE_TYPES {
  int ava_integer
  str ava_string
  register ava_pcode_register
  register-type ava_pcode_register_type
  demangled-name ava_demangled_name
  function {const ava_function*}
  list ava_list_value
  bool ava_bool
}

proc ctype-field {type} {
  if {[is-substruct $type]} {
    lassign $type name mne
    return "ava_pcode_${name}_list*"
  } else {
    return [dict get $::SIMPLE_TYPES $type]
  }
}

proc ctype-builder {type} {
  if {[is-substruct $type]} {
    lassign $type name mne
    return "ava_pc${mne}_builder**"
  } else {
    return [dict get $::SIMPLE_TYPES $type]
  }
}

proc xname {str} {
  set accum "AVA_ASCII9("
  for {set i 0} {$i < [string length $str]} {incr i} {
    if {$i > 0} {
      append accum ,
    }
    append accum "'[string index $str $i]'"
  }
  append accum ")"
  return $accum
}

proc gen-header {} {
  puts {
    /* This file was generated by generate-pcode.tcl.
     * It is only intended to be included from pcode.h.
     */
#ifndef AVA__IN_PCODE_H_
#error "Don't include " __FILE__ " directly."
#endif

  }

  # Define enums
  foreach struct $::structs {
    set sname [dict get $struct name]
    set smne [dict get $struct mne]
    set comma {}
    putm {
      typedef enum \{
    }
    dict for {ename elt} [dict get $struct elts] {
      putm ", ava_pcMNEt_ENAME" \
          MNE $smne ENAME $ename , $comma
      set comma ,
    }
    putm {
      \} ava_pcode_NAME_type;
    } NAME $sname
  }

  # Declare builder types, header structs, lists, and high-level functions
  foreach struct $::structs {
    set sname [dict get $struct name]
    set smne [dict get $struct mne]

    putm {
      typedef struct ava_pcMNE_builder_s ava_pcMNE_builder;
      typedef struct ava_pcode_NAME_s {
        ava_pcode_NAME_type type;
        TAILQ_ENTRY(ava_pcode_NAME_s) next;
      } ava_pcode_NAME;
      typedef TAILQ_HEAD(ava_pcode_NAME_list_s,ava_pcode_NAME_s)
        ava_pcode_NAME_list;
      ava_pcode_NAME_list* ava_pcMNE_builder_get\(
        const ava_pcMNE_builder* builder\);
      ava_string ava_pcode_NAME_list_to_string\(
        const ava_pcode_NAME_list* list,
        ava_uint indentation_level\);
      ava_pcode_NAME_list* ava_pcode_NAME_list_of_string\(
        ava_string str\);
      ava_pcode_NAME* ava_pcode_NAME_clone\(
        const ava_pcode_NAME* elt\);
    } MNE [dict get $struct mne] NAME [dict get $struct name]

    if {[dict exists $struct parent]} {
      lassign [dict get $struct parent] pname pmne
      putm {
        ava_pcPMNE_builder* ava_pcCMNE_builder_get_parent\(
          const ava_pcCMNE_builder* builder\);
        ava_pcCMNE_builder* ava_pcCMNE_builder_new\(
          ava_pcPMNE_builder* parent\);
      } PMNE $pmne CMNE $smne
    } else {
      putm {
        ava_pcMNE_builder* ava_pcMNE_builder_new(void);
      } MNE $smne
    }
  }

  # Define element structs and builder functions
  foreach struct $::structs {
    set smne [dict get $struct mne]
    set sname [dict get $struct name]
    dict for {ename elt} [dict get $struct elts] {
      putm {
        typedef struct \{
          ava_pcode_SNAME header;
      } SNAME $sname
      foreach field [dict get $elt fields] {
        putm "TYPE NAME;" \
            TYPE [ctype-field [dict get $field type]] \
            NAME [dict get $field name]
      }
      putm {
        \} ava_pcMNE_ENAME;
        ava_uint ava_pcMNEb_ENAME\(
          ava_pcMNE_builder* builder
      } MNE $smne SNAME $sname ENAME $ename
      foreach field [dict get $elt fields] {
        putm ", TYPE NAME" \
            TYPE [ctype-builder [dict get $field type]] \
            NAME [dict get $field name]
      }
      putm {
        \);
      }
    }
  }

  # Attributes
  foreach struct $::structs {
    set sname [dict get $struct name]
    foreach attr [dict get $struct attrs] {
      putm {
        ava_bool ava_pcode_NAME_is_ATTR\(
          const ava_pcode_NAME* elt\);
      } NAME $sname ATTR $attr
    }
  }

  # Properties
  foreach struct $::structs {
    set sname [dict get $struct name]
    dict for {pname ptype} [dict get $struct props] {
      putm {
        ava_bool ava_pcode_NAME_get_PROP\(
          TYPE* dst,
          const ava_pcode_NAME* elt,
          ava_uint index\);
        void ava_pcode_NAME_set_PROP\(
          ava_pcode_NAME* elt,
          ava_uint index,
          TYPE value\);
        ava_pcode_NAME* ava_pcode_NAME_with_PROP\(
          const ava_pcode_NAME* elt,
          ava_uint index,
          TYPE value\);
      } NAME $sname PROP $pname TYPE [ctype-field $ptype]
    }
  }
}

proc gen-impl {} {
  puts -nonewline {
    /* This file was generated by generate-pcode.tcl */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <assert.h>

#define AVA__INTERNAL_INCLUDE 1
#include "avalanche/defs.h"
#include "avalanche/alloc.h"
#include "avalanche/string.h"
#include "avalanche/integer.h"
#include "avalanche/function.h"
#include "avalanche/list.h"
#include "avalanche/exception.h"
#include "avalanche/name-mangle.h"
#include "avalanche/symbol.h"
#include "avalanche/pcode.h"

#include "-pcode-conversions.h"
  }

  # Builders
  foreach struct $::structs {
    set sname [dict get $struct name]
    set smne [dict get $struct mne]
    putm {
      struct ava_pcMNE_builder_s {
        void* parent;
        ava_pcode_NAME_list* list;
        ava_uint index;
      };
      ava_pcode_NAME_list* ava_pcMNE_builder_get\(
        const ava_pcMNE_builder* builder
      \) {
        return builder->list;
      }
    } NAME $sname MNE $smne

    if {[dict exists $struct parent]} {
      lassign [dict get $struct parent] pname pmne
      putm {
        ava_pcPMNE_builder* ava_pcCMNE_builder_get_parent\(
          const ava_pcCMNE_builder* builder
        \) {
          return builder->parent;
        }
        ava_pcCMNE_builder* ava_pcCMNE_builder_new\(
          ava_pcPMNE_builder* parent
        \) {
          ava_pcCMNE_builder* child = AVA_NEW(ava_pcCMNE_builder);
          child->parent = parent;
          child->list = AVA_NEW(ava_pcode_CNAME_list);
          TAILQ_INIT(child->list);
          return child;
        }
      } PNAME $pname PMNE $pmne CNAME $sname CMNE $smne
    } else {
      putm {
        ava_pcMNE_builder* ava_pcMNE_builder_new(void) {
          ava_pcMNE_builder* builder = AVA_NEW(ava_pcMNE_builder);
          builder->list = AVA_NEW(ava_pcode_NAME_list);
          TAILQ_INIT(builder->list);
          return builder;
        }
      } NAME $sname MNE $smne
    }
  }

  foreach struct $::structs {
    set sname [dict get $struct name]
    set smne [dict get $struct mne]

    # Builder functions
    dict for {ename elt} [dict get $struct elts] {
      putm {
        ava_uint ava_pcMNEb_ELT\(
          ava_pcMNE_builder* builder
      } MNE $smne ELT $ename

      foreach field [dict get $elt fields] {
        putm ", TYPE NAME" \
            TYPE [ctype-builder [dict get $field type]] \
            NAME [dict get $field name]
      }
      putm {
        \) \{
          ava_pcMNE_ELT* elt = AVA_NEW(ava_pcMNE_ELT);
          elt->header.type = ava_pcMNEt_ELT;
      } MNE $smne ELT $ename
      foreach field [dict get $elt fields] {
        set ftype [dict get $field type]
        if {[is-substruct $ftype]} {
          lassign $ftype cname cmne
          putm {
            *NAME = ava_pcCMNE_builder_new(builder);
            elt->NAME = (*NAME)->list;
          } NAME [dict get $field name] CMNE $cmne
        } else {
          putm {
            elt->NAME = NAME;
          } NAME [dict get $field name]
        }
      }
      # Check constraints
      foreach constraint [dict get $elt constraints] {
        set constraint [string map {@ (*elt)} $constraint]
        putm {
          if (!(%CONSTRAINT)) {
            FORMAT_ERROR("Illegal %ELT");
          }
        } ELT $ename CONSTRAINT $constraint
      }
      putm {
          TAILQ_INSERT_TAIL(builder->list, (ava_pcode_NAME*)elt, next);
          return builder->index++;
        \}
      } NAME $sname
    }

    # The _clone() function
    putm {
      ava_pcode_NAME* ava_pcode_NAME_clone\(
        const ava_pcode_NAME* elt
      \) \{
        switch (elt->type) \{
    } NAME $sname

    dict for {ename elt} [dict get $struct elts] {
      putm {
        case ava_pcSMNEt_ENAME:
          return ava_clone(elt, sizeof(ava_pcSMNE_ENAME));
      } SMNE $smne ENAME $ename
    }

    putm {
        default: abort();
        \}
      \}
    }

    # Attributes
    foreach attr [dict get $struct attrs] {
      putm {
        ava_bool ava_pcode_NAME_is_ATTR\(
          const ava_pcode_NAME* elt
        \) \{
          switch (elt->type) \{
      } NAME $sname ATTR $attr
      dict for {ename elt} [dict get $struct elts] {
        if {$attr in [dict get $elt attrs]} {
          putm {
          case ava_pcMNEt_ELT:
          } MNE $smne ELT $ename
        }
      }
      putm {
            return ava_true;
          default:
            return ava_false;
          \}
        \}
      }
    }

    # Properties
    dict for {pname ptype} [dict get $struct props] {
      putm {
        ava_bool ava_pcode_NAME_get_PROP\(
          TYPE* dst,
          const ava_pcode_NAME* elt,
          ava_uint index
        \) \{
          switch (elt->type) \{
      } NAME $sname PROP $pname TYPE [ctype-field $ptype]
      dict for {ename elt} [dict get $struct elts] {
        putm {
          case ava_pcMNEt_ELT:
            switch (index) \{
        } MNE $smne ELT $ename
        set ix 0
        foreach field [dict get $elt fields] {
          if {$pname in [dict get $field props]} {
            putm {
            case IX: *dst = ((ava_pcMNE_ELT*)elt)->FIELD; return ava_true;
            } IX $ix FIELD [dict get $field name] MNE $smne ELT $ename
            incr ix
          }
        }
        putm {
            default: return ava_false;
            \}
        }
      }
      putm {
          default: /* unreachable */ abort();
          \}
        \}
      }

      putm {
        ava_pcode_NAME* ava_pcode_NAME_with_PROP\(
          const ava_pcode_NAME* elt,
          ava_uint index,
          TYPE value
        \) \{
          switch (elt->type) \{
      } NAME $sname PROP $pname TYPE [ctype-field $ptype]

      dict for {ename elt} [dict get $struct elts] {
        putm {
          case ava_pcMNEt_ELT: \{
            ava_pcMNE_ELT* new;
            new = ava_clone(elt, sizeof(*new));
            switch (index) \{
        } MNE $smne ELT $ename
        set ix 0
        foreach field [dict get $elt fields] {
          if {$pname in [dict get $field props]} {
            putm {
            case IX: new->FIELD = value; break;
            } IX $ix FIELD [dict get $field name]
            incr ix
          }
        }
        putm {
            default: abort();
            \}
            return (ava_pcode_NAME*)new;
          \}
        } NAME $sname
      }

      putm {
          default: abort();
          \}
        \}
      }

      putm {
        void ava_pcode_NAME_set_PROP\(
          ava_pcode_NAME* elt,
          ava_uint index,
          TYPE value
        \) \{
          switch (elt->type) \{
      } NAME $sname PROP $pname TYPE [ctype-field $ptype]

      dict for {ename elt} [dict get $struct elts] {
        putm {
          case ava_pcMNEt_ELT: \{
            ava_pcMNE_ELT* celt AVA_UNUSED;
            celt = (ava_pcMNE_ELT*)elt;
            switch (index) \{
        } MNE $smne ELT $ename
        set ix 0
        foreach field [dict get $elt fields] {
          if {$pname in [dict get $field props]} {
            putm {
            case IX: celt->FIELD = value; break;
            } IX $ix FIELD [dict get $field name]
            incr ix
          }
        }
        putm {
            default: abort();
            \}
          \} break;
        } NAME $sname
      }

      putm {
          default: abort();
          \}
        \}
      }
    } ;# end properties

    # to_string
    putm {
      ava_string ava_pcode_NAME_list_to_string\(
        const ava_pcode_NAME_list* list,
        ava_uint indent
      \) \{
        ava_string accum = AVA_EMPTY_STRING, elt_string;
        const ava_pcode_NAME* velt;
        TAILQ_FOREACH(velt, list, next) \{
          switch (velt->type) \{
    } NAME $sname
    dict for {ename elt} [dict get $struct elts] {
      putm {
        case ava_pcMNEt_ELT: \{
          const ava_pcMNE_ELT* elt AVA_UNUSED;
          elt = (const ava_pcMNE_ELT*)velt;
          elt_string = AVA_ASCII9_STRING("%ELT");
      } MNE $smne ELT $ename
      foreach field [dict get $elt fields] {
        set ftype [dict get $field type]
        if {[is-substruct $ftype]} {
          lassign $ftype cname cmne
          putm {
            elt_string = ava_string_concat\(
              elt_string, AVA_ASCII9_STRING(" \\""\{\n")\);
            elt_string = ava_string_concat\(
              elt_string, ava_pcode_CNAME_list_to_string\(
                elt->FIELD, indent+1\)\);
            elt_string = ava_string_concat\(
              elt_string, apply_indent(AVA_ASCII9_STRING("\\""\}"), indent)\);
          } CNAME $cname FIELD [dict get $field name]
        } else {
          putm {
            elt_string = ava_string_concat\(
              elt_string, AVA_ASCII9_STRING(" ")\);
            elt_string = ava_string_concat\(
              elt_string, ava_list_escape\(
                ava_value_of_string\(
                  ava_pcode_TYPE_to_string(elt->FIELD)\)\)\);
          } TYPE $ftype FIELD [dict get $field name]
        }
      }
      putm {
        break;
        \}
      }
    }
    putm {
          default: /* unreachable */ abort();
          \} /* switch (velt->type) */
          accum = ava_string_concat\(
            accum, apply_indent\(
              ava_pcode_elt_escape(elt_string), indent\)\);
          accum = ava_string_concat(accum, AVA_ASCII9_STRING("\n"));
        \} /* end foreach */
        return accum;
      \}
    }

    # of_string
    if {[dict exists $struct parent]} {
      set parent NULL
    } else {
      set parent {}
    }
    putm {
      ava_pcode_NAME_list* ava_pcode_NAME_list_of_string(ava_string str) \{
        ava_pcMNE_builder* builder = ava_pcMNE_builder_new(PARENT);
        ava_list_value src = ava_list_value_of(ava_value_of_string(str));
        ava_list_value eltl;
        size_t elt_ix, nelts = ava_list_length(src);
        void* dummy AVA_UNUSED;
        for (elt_ix = 0; elt_ix < nelts; ++elt_ix) \{
          eltl = ava_list_value_of(ava_list_index(src, elt_ix));
          if (0 == ava_list_length(eltl)) {
            FORMAT_ERROR("Empty element in NAME list");
          }
          switch (ava_string_to_ascii9(ava_to_string\(
                    ava_list_index(eltl, 0))\)) \{
    } NAME $sname MNE $smne PARENT $parent
    dict for {ename elt} [dict get $struct elts] {
      putm {
        case %XELT:
          if (NFIELDS+1 != ava_list_length(eltl))
            FORMAT_ERROR("%ELT takes exactly %NFIELDS fields");
          ava_pcMNEb_ELT\(builder
      } XELT [xname $ename] ELT $ename MNE $smne \
          NFIELDS [llength [dict get $elt fields]]
      set field_ix 1
      foreach field [dict get $elt fields] {
        set ftype [dict get $field type]
        if {[is-substruct $ftype]} {
          putm {, (void*)&dummy }
        } else {
          putm {
            , ava_pcode_parse_TYPE(ava_list_index(eltl, FIX))
          } TYPE $ftype FIX $field_ix
        }
        incr field_ix
      }
      putm {
        \);
      }
      # Read any sub-structures in
      set field_ix 1
      foreach field [dict get $elt fields] {
        set ftype [dict get $field type]
        if {[is-substruct $ftype]} {
          lassign $ftype cname cmne
          putm {
            ((ava_pcMNE_ELT*)TAILQ_LAST\(
             builder->list, ava_pcode_NAME_list_s)\)->FIELD =
              ava_pcode_CNAME_list_of_string\(
                ava_to_string(ava_list_index(eltl, FIX))\);
          } FIELD [dict get $field name] CNAME $cname \
              MNE $smne NAME $sname ELT $ename FIX $field_ix
        }
        incr field_ix
      }
      putm {
        break;
      }
    }
    putm {
          default:
            FORMAT_ERROR("Unknown element type in NAME list");
          \} /* end switch eltl[0] */
        \} /* end foreach */
        return builder->list;
      \} /* end of_string */
    } NAME $sname
  }
}

gen-[lindex $argv 0]
