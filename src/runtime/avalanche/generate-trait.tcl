#! /usr/bin/env tclsh8.5
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

package require Tcl 8.5

# DSL for definining and implementing traits.
#
# A trait definition is placed into its own file, typically with the ".trait"
# extension. That filename is then passed to this file in order to expand it
# into the appropriate code.
#
# A trait definition file looks like the following:
#
#   doc { this is a trait }
#   trait prefix widget {
#     doc {docs...}
#     method return_type method_name {
#       arg2_type arg2_name arg3_type arg3_name
#     }
#     method SELF add {SELF a2} AVA_PURE
#     method void void_method {type name}
#     # ...
#   }
#
# The prefix specifies the name prefix applied to all top-level symbols
# generated; widget specifies the base name of the actual trait.
#
# method defines a method with a chosen name and return type on the trait with
# some arguments. The first argument is always implicitly the "this" parameter.
# For both return types and argument types, the pseudo-type SELF may be used to
# stand in for the primary input type (since separate versions of each method
# are generated which take a generic ava_value and a format-safe
# prefix_widget_value).
#
# GCC-style attributes may be affixed to a method by putting them in a list
# after the arguments.
#
# The above example results in the following output:
#
#   /* The format-safe value for this trait */
#   /**
#    * this is a trait
#    */
#   typedef struct { ava_value v; } prefix_widget_value;
#
#   /* The attribute tag for the trait itself */
#   extern const ava_attribute_tag prefix_widget_trait_tag;
#
#   typedef struct prefix_widget_trait_s prefix_widget_trait;
#
#   /* A format-safe value with the trait pre-extracted */
#   typedef struct {
#     /* The vtable for this value/trait pair */
#     const prefix_widget_trait*restrict v;
#     /* The content of this value */
#     prefix_widget_value c;
#   } prefix_fat_widget_value;
#
#   /* The trait itself, indicating the method implementations for a value */
#   struct prefix_widget_trait_s {
#     ava_attribute header;
#     /* Method pointers */
#     /**
#      * docs...
#      */
#     return_value (*method_name)(
#       prefix_widget_value this,
#       arg2_type arg2_name,
#       arg3_type arg3_name);
#     prefix_widget_value (*add)(
#       prefix_widget_value this,
#       prefix_widget_value a2);
#     void (*void_method)(
#       prefix_widget_value this,
#       arg2_type arg2_name);
#   };
#
#   /* Generic value->normal value conversion routine.
#    * This must be coded in the implementation by hand.
#    */
#   prefix_widget_value prefix_widget_value_of(ava_value value) AVA_PURE;
#   /* Augmented value->fat normal value conversion routine. */
#   prefix_fat_widget_value prefix_fat_widget_value_of(
#     ava_value value) AVA_PURE;
#
#   /* Top-level calls for each method. There are versions that take plain
#    * ava_values and formatted values for each.
#    */
#   /**
#    * docs...
#    */
#   #define prefix_widget_my_method(this, arg1_name, ar2_name)          \
#     _Generic((this),                                                  \
#              ava_value:               prefix_widget_my_method_v,      \
#              prefix_widget_value:     prefix_widget_my_method_f)      \
#       (this, arg1_name, arg2_name)
#   /* These functions are also implemented; snipped here */
#   static inline return_type prefix_widget_my_method_v(
#     ava_value this, arg1_type arg1_name, arg2_type arg2_name);
#   static inline return_type prefix_widget_my_method_f(
#     prefix_widget_value this, arg1_type arg1_name, ar2_type arg2_name);
#   #define prefix_widget_add(this, a2)                                 \
#     _Generic((this),                                                  \
#              ava_value:               prefix_widget_add_v,            \
#              prefix_widget_value:     prefix_widget_add_f)            \
#       (this, a2)
#   static inline ava_value prefix_widget_add_v(
#     ava_value this, ava_value a2) AVA_PURE;
#   static inline prefix_widget_value prefix_widget_add_v(
#     prefix_widget_value this, prefix_value a2) AVA_PURE;
#   /* void_method snipped */
#
#   #define PREFIX_WIDGET_DEFIMPL(name,chain)                               \
#     static ava_value name##_widget_my_method(                             \
#       prefix_widget_value this, arg1_type arg1_name, arg2_type arg2_name) \
#     static prefix_widget_value name##_widget_add(                         \
#       prefix_widget_value this, prefix_widget_value a2);                  \
#     static void name##_widget_void_method(                                \
#       prefix_widget_value this, type name);                               \
#     static const prefix_widget_trait name##_widget_impl = {               \
#       .header = { .next = (chain), .tag = &prefix_widget_trait_tag },     \
#       .my_method = name##_widget_my_method,                               \
#       .add = name##_widget_add,                                           \
#       .void_method = name##_widget_void_method,                           \
#     };
#
#   #define PREFIX_WIDGET_DEFIMPL_PUBLIC() \
#     /* same as DEFIMPL, but the _impl variable is not static */
#
# Note that PREFIX_WIDGET_DEFIMPL() can be used by implementors of the trait to
# generate via the preprocessor most of the boilerplate surrounding the
# implementation.

set doc {}

proc trait {prefix name body} {
  set ::prefix $prefix
  set ::name $name
  set ::methods {}
  set ::trait_doc $::doc
  set ::doc {}
  eval $body
}

proc method {return_type name args {attributes {}}} {
  lappend ::methods [list return_type $return_type \
                          mname $name \
                          args $args \
                          attributes $attributes \
                          mdoc $::doc]
  set ::docs {}
}

proc doc {text} {
  set ::doc $text
}

proc typesub {type self} {
  if {$type eq "SELF"} {
    return $self
  } else {
    return $type
  }
}

if {[llength $argv] != 1} {
  puts stderr "Usage: $argv0 infile"
  exit 64
}

source [lindex $argv 0]

set valtype "${prefix}_${name}_value"

puts "/**
$trait_doc

@see ${prefix}_${name}_trait
 */
typedef struct { ava_value v; } ${valtype};

/**
 * Tag for the ${prefix}_${name}_trait attribute
 *
 * @see ${prefix}_${name}_trait
 * @see ${valtype}
 */
extern const ava_attribute_tag ${prefix}_${name}_trait_tag;

/**
$trait_doc

@see ${valtype}
 */
typedef struct ${prefix}_${name}_trait_s ${prefix}_${name}_trait;

typedef struct {
  /**
   * The vtable for this value/trait pair.
   */
  const ${prefix}_${name}_trait*restrict v;
  /**
   * The content of this value.
   */
  ${valtype} c;
} ${prefix}_fat_${name}_value;

struct ${prefix}_${name}_trait_s {
  ava_attribute header;"

foreach meth $methods {
  dict with meth {
    puts "  /**
$mdoc
   */
   [typesub $return_type $valtype]
   (*$mname)(${valtype} _this"

    foreach {t n} $args {
      puts "    , [typesub $t $valtype] $n"
    }
    puts ");"
  }
}

puts "};

/**
 * Converts the given value into a normal value of ${name} format.
 *
 * @value The value to normalise.
 * @throws ava_format_exception if value does not conform to ${name} format.
 */
${valtype} ${valtype}_of(ava_value value) AVA_PURE;
/**
 * Converts the given value into a normal value of ${name} format.
 *
 * If the value is not already in a representation that permits efficient
 * handling as a ${name}, it will be converted to one that does. The returned
 * value includes a trait pointer that can be used to perform these operations.
 *
 * Note that the content of the returned value is not necessarily the same as
 * the one on input.
 *
 * @param value The value to normalise and for which to obtain a trait
 *              implementation.
 * @return A normalised value with a trait implementation.
 * @throws ava_format_exception if value does not conform to ${name} format.
 */
${prefix}_fat_${name}_value ${prefix}_fat_${name}_value_of(
  ava_value value) AVA_PURE;
"

foreach meth $methods {
  dict with meth {
    set argnames {_this}
    set arguse {(_this)}
    foreach {t n} $args {
      append argnames ",$n"
      append arguse ",($n)"
    }
    puts "
/**
$mdoc
 */
#define ${prefix}_${name}_${mname}($argnames) \\
              _Generic((_this), \\
                       ava_value: ${prefix}_${name}_${mname}_v, \\
                       ${valtype}: ${prefix}_${name}_${mname}_f, \\
                       /* Clang requires const versions to be */ \\
                       /* here too. */ \\
                       const ava_value: ${prefix}_${name}_${mname}_v, \\
                       const ${valtype}: ${prefix}_${name}_${mname}_f) \\
                ($arguse)
"
    foreach {sfx self} [list v ava_value f "${valtype}"] {
      if {$self ne "ava_value"} {
        set thissfx .v
      } else {
        set thissfx {}
      }
      puts "/**\n$mdoc\n */"
      puts "static inline [typesub $return_type $self] "
      puts "    ${prefix}_${name}_${mname}_${sfx}"
      puts "($self _this"
      foreach {t n} $args {
        puts ", [typesub $t $self] $n"
      }
      puts ") $attributes;"
      puts "static inline [typesub $return_type $self] "
      puts "    ${prefix}_${name}_${mname}_${sfx}"
      puts "($self _this"
      foreach {t n} $args {
        puts ", [typesub $t $self] $n"
      }
      puts ") {"
      puts "  ${prefix}_fat_${name}_value fat = "
      puts "    ${prefix}_fat_${name}_value_of(_this$thissfx);"
      if {"void" ne $return_type} {
        puts "  return"
      }
      puts "    fat.v->${mname}(fat.c"
      foreach {t n} $args {
        if {"SELF" ne $t || "ava_value" ne $self} {
          puts ", $n"
        } else {
          puts ", ${valtype}_of($n)"
        }
      }
      puts "  )"
      if {"SELF" eq $return_type && $self eq "ava_value"} {
        puts ".v"
      }
      puts ";"
      puts "}"
    }
  }
}

foreach {linkage suffix} {static "" "" _public} {
  puts "#define [string toupper "${prefix}_${name}_defimpl${suffix}"](name,chain) \\"
  foreach meth $methods {
    dict with meth {
      puts "static [typesub $return_type $valtype] name##_${name}_${mname}(\\"
      puts "  ${prefix}_${name}_value _this \\"
      foreach {t n} $args {
        puts "  , [typesub $t $valtype] $n \\"
      }
      puts ") $attributes; \\"
    }
  }
  puts "$linkage const ${prefix}_${name}_trait name##_${name}_impl = {\\"
  puts "  .header = { \\
    .next = (const ava_attribute*)(chain), \\
    .tag = &${prefix}_${name}_trait_tag },\\"
  foreach meth $methods {
    dict with meth {
      puts "  .${mname} = name##_${name}_${mname},\\"
    }
  }
  puts "};"
}
