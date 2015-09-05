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

# Generates the files avalanche/gen-errors.h and gen-errors.c.
# Usage:
#   generate-errors.tcl {header|impl} >output.c

set defs {
  serror R0001 integer_overflow {{ava_string input}} {
    msg "Integer flows over: %input%"
    explanation {
      An attempt was made to perform integer arithmetic on a value which cannot
      be represented as an integer in Avalanche.

      Even if syntactically valid, integers are rejected if they would overflow
      both a signed and unsigned 64-bit integer. This error thus indicates that
      an attempt was made to use an integer greater than
      18,446,744,073,709,551,615 or less than -9,223,372,036,854,775,808.
    }
  }

  serror R0002 bad_pcode {{ava_string message}} {
    msg "Invalid P-Code: %message%"
    explanation {
      An attempt was made to parse a string as P-Code, but it is not a valid
      P-Code object.

      Since programmers and users do not normally input P-Code by hand, this
      error indicates a bug in whatever generated the P-Code string.
    }
  }

  serror R0003 bad_ffi {{ava_string message}} {
    msg "Preparing FFI for function failed: %message%"
    explanation {
      ffi_prep_cif() returned failure when preparing the FFI data for a
      function.

      This most likely indicates that the function references a calling
      convention or other concept that does not actually exist on the host
      platform.
    }
  }

  serror R0004 too_few_elements_to_be_a_function {} {
    msg "List has too few elements to be a function."
    explanation {
      An attempt was made to interpret a non-function value as a function.

      Specifically, the value was a valid list, but was too short to possibly
      be a function.
    }
  }

  serror R0005 unknown_calling_convention {{ava_string cc}} {
    msg "Unknown calling convention: %cc%"
    explanation {
      While attempting to interpret a value as a function, the given
      unrecognised calling convention was found.

      Most likely, this indicates that the value is not actually a valid
      function anyway.
    }
  }

  serror R0006 function_has_no_args {} {
    msg "Function value has no arguments."
    explanation {
      The function interpretation of a value implied the function takes no
      arguments.

      All functions in Avalanche must take at least one argument and at least
      one non-implicit parameter. This function indicated it took no argument
      at all. Most likely, this was the result of incorrectly truncating a real
      function value.
    }
  }

  serror R0007 function_argspec_too_short {{ava_value argspec}} {
    msg "Function argument specification too short: [%argspec%]"
    explanation {
      The function interpretation of a value has an argument specification that
      is too short.

      This may be due to a value that isn't actually a function at all. It may
      also be the result of forgetting to include the type of an argument if
      the function uses a non-Avalance calling convention.
    }
  }

  serror R0008 function_null_pointer {} {
    msg "Function would have NULL address."
    explanation {
      An attempt was made to interpret a value as a function, but the address
      of that function would have been NULL.
    }
  }

  serror R0009 unknown_marshalling_type {{ava_string type}} {
    msg "Unknown marshalling type: %type%"
    explanation {
      An attempt was made to interpret a value as a function, but it references
      a type the type marshaller does not know about.

      Marshalling types are only present on functions with non-Avalanche
      calling conventions. Most likely, this error indicates that a marshalling
      type was forgotten in a function prototype, or that a type was
      misspelled.
    }
  }

  serror R0010 function_argspec_missing_binding {} {
    msg "Missing binding in argument specification."
    explanation {
      The binding specification of a an argument in a function object was not
      present at all.

      This most likely means that the value is not actually a function, or that
      a hand-coded prototype for a non-Avalance function inadvertently omitted
      the binding specification for the argument, including only the type.
    }
  }

  serror R0011 function_argspec_implicit_length {} {
    msg "implicit binding not followed by exactly one value."
    explanation {
      A function argument specification declares an argument as "implicit", but
      fails to specify the value for it, or incorrectly specifies more than one
      value.
    }
  }

  serror R0012 function_argspec_pos_length {} {
    msg "pos binding followed by more than one value."
    explanation {
      A function argument specification declares an argument as "pos", but
      includes more than one value thereafter.
    }
  }

  serror R0013 function_argspec_varargs_length {} {
    msg "varargs binding followed by garbage."
    explanation {
      A function argument specification declares an argument as "varargs", but
      has additional values beyond the "varargs" word.
    }
  }

  serror R0014 function_argspec_named_length {} {
    msg "named binding has no values, or followed by garbage."
    explanation {
      A function argument specification declares an argument as "named", but
      either omits the name entirely, or has more than two values following
      "named".
    }
  }

  serror R0015 function_argspec_bool_length {} {
    msg "bool binding has no name, or followed by garbage."
    explanation {
      A function argument specification declares an argument as "bool", but
      either omits the name entirely, or has extraneous values after the name.
    }
  }

  serror R0016 function_argspec_unknown_binding {{ava_string binding}} {
    msg "Unknown argument binding type: %binding%"
    explanation {
      A function argument specification indicates the given binding type, which
      is not known.

      Most likely, the value being interpreted as a function is not actually a
      function. It is also possible the prototype incorrectly includes
      marshalling types on an Avalanche function, which would indicate
      incorrect function manipulation elsewhere in the program.
    }
  }

  serror R0017 function_var_shape_following_varargs {} {
    msg "Variably-shaped argument found after variadic argument."
    explanation {
      A function declares itself to take a variably-shaped argument at some
      point following a variadic argument.

      Variably-shaped arguments are those which may consume varying numbers of
      parameters depending on context, such as optional arguments or variadic
      arguments. Such arguments are not permitted at any point beyond a
      variadic argument, since it would make parameter binding ambiguous.
    }
  }

  serror R0018 function_var_shaped_must_be_contiguous {} {
    msg "Non-contiguous variably-shaped arguments in function."
    explanation {
      A function declares itself to take more than one variably-shaped
      argument, but one or more fixed-shaped arguments occur therebetween.

      Variably-shaped arguments are those which may consume varying numbers of
      parameters depending on context, such as optional arguments or variadic
      arguments. Such arguments may not be interrupted by fixed-shape
      arguments, since it would make parameter binding ambiguous.
    }
  }

  serror R0019 function_has_no_explicit_parms {} {
    msg "Function has no explicit parameters."
    explanation {
      An attempt was made to create a function with no explicit parameters.

      Since it is impossible to call a function in Avalance with no parameters,
      it is required that all functions take at least one, even if they have
      implicitly-bound arguments.
    }
  }

  serror R0020 function_named_arg_declared_more_than_once {{ava_string arg}} {
    msg "Named argument declared more than once: %arg%"
    explanation {
      A function declares the given named argument more than one time.
    }
  }

  serror R0021 function_missing_value_for_named_argument {{ava_string arg}} {
    msg "Missing value for named argument: %arg%"
    explanation {
      The parameters passed to a function appear to intend to bind a value to
      the indicated named argument, but no further parameters occur to do so.

      If the parameter giving the argument name occurs in the middle of the
      parameter list, most likely insufficient parameters were passed to the
      right of it, causing the value for the named argument to be used for a
      different, positional argument.
    }
  }

  serror R0022 function_unknown_named_argument {{ava_string name}} {
    msg "No match for named argument: %name%"
    explanation {
      The indicated parameter is positioned in a location that implies it is
      giving the name of a named argument, but there is no named argument with
      that name, or that named argument has already been given.
    }
  }

  serror R0023 function_too_many_parms \
      {{ava_integer first} {ava_integer last}} {
    msg "Too many parameters to function; unbound: %first%..%last%"
    explanation {
      An attempt was made to call a function with more parameters than can be
      bound to its arguments.
    }
  }

  serror R0024 function_unbound_argument {{ava_string unbound}} {
    msg "No parameter bound to mandatory argument: %unbound%"
    explanation {
      An attempt was made to call a function with insufficient parameters, such
      that the indicated mandatory argument could not be bound to any
      parameter.
    }
  }

  serror R0025 non_empty_string_to_void_arg {} {
    msg "Non-empty string passed to void argument."
    explanation {
      Native functions which accept no arguments generally take a single "void"
      argument so that they can be called from Avalanche. This argument is
      required to be the empty string, but some other value was passed in.
    }
  }

  serror R0026 not_an_integer {{ava_string value}} {
    msg "Not an integer: %value%"
    explanation {
      An attempt was made to interpret the given value as an integer, but it
      was not parsable as such.
    }
  }

  serror R0027 integer_trailing_garbage {{ava_string value}} {
    msg "Trailing garbage at end of integer: %value%"
    explanation {
      An attempt was made to interpret the given value as an integer, but it
      was not parsable as such.
    }
  }

  serror R0028 odd_length_list_to_map {} {
    msg "Attempt to use list of odd length as map."
    explanation {
      An attempt was made to interpret a value which was a valid list as a map,
      but the list has odd length.

      Maps in Avalanche are defined as lists of alternating keys and values.
      Thus, a list of odd length would leave a key at the end with no value,
      and therefore is not considered a valid map.
    }
  }

  serror R0029 list_of_non_two_length_as_pointer {} {
    msg "Use of list of non-2 length as pointer."
    explanation {
      Avalanche pointers are defined to be a list of exactly two elements. An
      attempt was made to interpret a list with a different number of elements
      as a pointer.
    }
  }

  serror R0030 bad_pointer_prototype {} {
    msg "Bad pointer prototype."
    explanation {
      A pointer prototype (the first element of a pointer value) must end with
      "*" or "&". A value was used as a pointer where this was not the case;
      eg, the prototype was empty or ended with some other character.
    }
  }

  serror R0031 bad_pointer_constness {} {
    msg "Use of const pointer in non-const context."
    explanation {
      A const pointer (with a "&" prototype) was passed to a function which
      only accepts non-const ("*") pointers.
    }
  }

  serror R0032 bad_pointer_type {{ava_string provided} {ava_string expected}} {
    msg "Use of pointer type %provided%  where %expected% was expected."
    explanation {
      A pointer with a strong type was passed to a function which expects a
      different strong pointer type.
    }
  }

  serror R0033 unexpected_token_parsing_list {
    {ava_integer index} {ava_string token}
  } {
    msg "Unexpected token parsing list at index %index%: %token%"
    explanation {
      An illegal token was encountered while parsing a list.

      While lists follow the same lexical rules as Avalanche in general, only a
      subset of token types are actually meaningful within a list.
    }
  }

  serror R0034 invalid_list_syntax {
    {ava_integer index} {ava_string message}
  } {
    msg "Illegal list syntax at index %index%: %message%"
    explanation {
      An attempt to interpret a value as a list failed because the value
      contains lexical errors.
    }
  }

  serror L4001 unclosed_string_literal {} {
    msg "Unclosed string literal."
    explanation {
      The end of the input file was encountered without the indicated string
      literal having been closed.
    }
  }

  serror L4002 unclosed_verbatim {{ava_integer depth}} {
    msg "Unclosed verbatim literal (nested %depth% levels at eof)."
    explanation {
      The end of the input file was encountered before the indicated verbatim
      had been fully closed.
    }
  }

  serror L4003 invalid_backslash_sequence {} {
    msg "Invalid backslash sequence."
    explanation {
      An illegal backslash escape sequence was encountered at the indicated
      location.
    }
  }

  serror L4004 lone_backslash_at_eof {} {
    msg "Lone backslash at end of input."
    explanation {
      An isolated backslash was found as the very last byte of the input file.
    }
  }

  serror L4005 lex_illegal_characters {
    {ava_integer count} {ava_string plural} {ava_string hex}
  } {
    msg "Encountered %count% illegal character%plural%: %hex%"
    explanation {
      A sequence of one or more illegal characters was encountered in the input.

      Avalanche forbids all ASCII control characters other than newline ("\n"),
      carraige-return ("\r"), and horizontal tab ("\t") from occurring
      literally within and string parsed according to the lexical rules. To
      encode such characters in the string, use a backslash escape sequence,
      such as "\x7f" for the ASCII DEL character.

      This error may also indicate that something other than an Avalance
      lexical string, like a binary file, was input.
    }
  }

  serror L4006 token_must_be_separated_from_previous {} {
    msg "Missing whitespace separation before this token."
    explanation {
      Many tokens are required to be separated from what precedes them by
      whitespace.

      For example, the input "foo"bar is rejected under this rule, as is
      foo"bar". Instead, "foo" bar and foo "bar" should be written.

      For some inputs, this error may indicate a string that was incorrectly or
      accidentally closed.
    }
  }

  cerror C5001 invalid_function_prototype {{ava_value reason}} {
    msg "Invalid funciton prototype: %reason%"
    explanation {
      An invalid function prototype was passed to an intrinsic macro.

      More details can be found in the nested error message.

      This error indicates that the macro did not correctly validate its
      input correctly and is probably a bug.
    }
  }

  cerror C5002 ambiguous_function {{ava_string name}} {
    msg "Function name is ambiguous: %name%"
    explanation {
      The compiler was not able to determine what function to call because more
      than one symbol matches the name.

      Ambiguous symbols result when more than one import produces the same
      symbol name. Note that functions share the same namespace as variables
      and macros, so the conflicting symbols are not necessarily functions.

      This issue can be resolved by using a qualified name for the function, or
      adjusting imports so that names cannot conflict, for example by ensuring
      that each import has a distinct prefix on the imported symbols.
    }
  }

  cerror C5003 no_such_function {{ava_string name}} {
    msg "No such function: %name%"
    explanation {
      The indicated identifier is followed by one or more expressions, implying
      it is a function, but there is no symbol at all with that name.

      Most likely, a function or macro name was misspelled, an import
      forgotten, or an intended macro left out.
    }
  }

  cerror C5004 not_a_function {{ava_string name}} {
    msg "Not a function: %name%"
    explanation {
      The indicated identifier is followed by one or more expressions, implying
      it is a function, but the symbol it references is not a function.

      If the intent was to invoke a function stored in a variable, the variable
      must be explicitly dereferenced by prefixing it with a dollar sign.
    }
  }

  cerror C5005 empty_sequence_as_lvalue {} {
    msg "Empty expression cannot be used as lvalue"
    explanation {
      An attempt was made to assign to or update an empty expression, which is
      not meaningful.
    }
  }

  cerror C5006 multi_sequence_as_lvalue {} {
    msg "Sequence of statements cannot be used as lvalue"
    explanation {
      An attempt was made to assign to or update a sequence of statements
      containing more than one statement.
    }
  }

  cerror C5007 lstring_missing_left_expr {} {
    msg "Missing expression before L- or LR-String."
    explanation {
      L-Strings and LR-Strings imply concatenation with the expression to their
      left, but there is nothing to the left of this string.

      If it looks like there is something to the left of the string, there may
      be confusion over macro precedence; try adding parentheses around the
      expression to the left, or around the whole expression consisting of this
      string and its concatenatees.
    }
  }

  cerror C5008 rstring_missing_right_expr {} {
    msg "Missing expression after R- or LR-String."
    explanation {
      R-Strings and LR-Strings imply concatenation with the expression to their
      right, but there is nothing to the right of this string.

      If it looks like there is something to the right of the string, there may
      be confusion over macro precedence; try adding parentheses around the
      expression to the right, or around the whole expression consisting of
      this string and its concatenatees.
    }
  }

  cerror C5009 string_as_lvalue {} {
    msg "String literal cannot be used as lvalue"
    explanation {
      An attempt was made to assign to or update a string literal, which is not
      a meaningful operation.
    }
  }

  cerror C5010 extra_macro_args_left {{ava_string macro}} {
    msg "Extra arguments to left of %macro%"
    explanation {
      More arguments were passed on the left of an operator macro than the
      macro actually takes.

      Consult the documentation of the macro to find out more about what
      arguments it accepts.
    }
  }

  cerror C5011 extra_macro_args_right {{ava_string macro}} {
    msg "Extra arguments to right of %macro%"
    explanation {
      More arguments were passed on the right of a macro than the macro
      actually takes.

      Consult the documentation of the macro to find out more about what
      arguments it accepts.
    }
  }

  cerror C5012 macro_arg_missing {{ava_string macro} {ava_string name}} {
    msg "Missing argument for macro %macro%: %name%"
    explanation {
      An argument required by a macro was not given.

      The last consumed argument before the missing argument is indicated, or
      the macro itself if no arguments on that side were provided.

      Consult the documentation of the macro to find out more about what
      arguments it accepts.
    }
  }

  cerror C5013 macro_arg_must_be_bareword {{ava_string name}} {
    msg "Not a bareword, for argument %name%"
    explanation {
      The indicated argument is required to be a bareword, but something else
      was given.
    }
  }

  cerror C5014 macro_arg_must_be_stringoid {{ava_string name}} {
    msg "Not a bareword, A-string, or verbatim, for argument %name%"
    explanation {
      The indicated argument is required to be a bareword or string literal,
      but something else was given.
    }
  }

  cerror C5015 macro_arg_must_be_literal {{ava_string name}} {
    msg "Not a literal, for argument %name%"
    explanation {
      The indicated argument is required to be a literal expression, but a
      non-literal was given.

      A literal is any string literal, a bareword, or a semiliteral containing
      only other literals.
    }
  }

  cerror C5016 non_private_definition_in_nested_scope {} {
    msg "Non-private definition at non-global scope."
    explanation {
      Defitions outside of global scope (e.g., within a function) must always
      be private, since it is impossible for code outside of the scope to
      reference such definitions by name anyway.
    }
  }

  cerror C5017 symbol_redefined {{ava_string symbol}} {
    msg "Redefinition of symbol: %symbol%"
    explanation {
      An attempt was made to define the given symbol, but another definition of
      the same name already exists.
    }
  }

  cerror C5018 symbol_redefined_import {{ava_string symbol}} {
    msg "Redefinition of symbol's simple name: %symbol%"
    explanation {
      An attempt was made to define the given symbol, but an automaic import in
      effect resulted in a name collision with another symbol.
    }
  }

  cerror C5019 apply_imports_produced_conflict {} {
    msg "Name conflict caused by later definitions."
    explanation {
      The imports in effect at this location caused a name conflict with names
      defined later in the same input.
    }
  }

  cerror C5020 ambiguous_bareword {} {
    msg "Bareword is ambiguous."
    explanation {
      The compiler was not able to determine whether the indicated token was a
      macro because more than one symbol matches the name.

      Ambiguous symbols result when more than one import produces the same
      symbol name. Note that macros share the same namespace as variables
      and functions, so the conflicting symbols are not necessarily macros.

      This issue can be resolved by using a qualified name for the function, or
      adjusting imports so that names cannot conflict, for example by ensuring
      that each import has a distinct prefix on the imported symbols.
    }
  }

  cerror C5021 not_an_lvalue {{ava_string node}} {
    msg "%node% cannot be used as an lvalue."
    explanation {
      An attempt was made to assign to or update something that never supports
      such an operation.
    }
  }

  cerror C5022 does_not_produce_a_value {{ava_string node}} {
    msg "%node% does not produce a value."
    explanation {
      An attempt was made to use something which does not produce a value, such
      as a function definition or an import, in an expression.

      Such definitions must occur as their own statements.
    }
  }

  cerror C5023 is_pure_but_would_discard {{ava_string node}} {
    msg "%node% is pure, but value would be discarded."
    explanation {
      The given element is pure, but the value it produces is not used for
      anything.

      Pure elements have no effect of their own, so discarding the resulting
      value is equivalent to deleting the element from the source entirely. For
      example, an expression consting of a lone bareword is pure, since it
      produces the bareword's value but has no observable effect.

      This error most commonly results if a statement was accidentally split in
      two (eg, a backslash escaping a newline forgotten) or an attempt was made
      to call a function without parameters.
    }
  }

  serror C5024 parse_unexpected_token {{ava_string token}} {
    msg "Unexpected token: %token%"
    explanation {
      The indicated token was encountered in a context where it was not
      expected.
    }
  }

  serror C5025 parse_unexpected_eof {} {
    msg "Unexpected end-of-input."
    explanation {
      The parser encountered the end of the input file while there were still
      unbalanced parentheses, brackets, or braces.
    }
  }

  serror C5026 empty_variable_name {} {
    msg "Empty variable name."
    explanation {
      The containing token appears to involve reading from a variable whose
      name is the empty string.

      This most likely indicates that a dollar sign was forgotten. For example,
      the token $foo$bar$ will produce this error at the very end, whereas the
      intended behaviour would be expressed with $foo$$bar$.
    }
  }
}

proc ncode {code} {
  string range $code 1 end
}

proc type {parm} {
  lindex $parm 0
}

proc name {parm} {
  lindex $parm 1
}

proc serror-decl {name parms} {
  puts "ava_string ava_error_${name}("
  set first true
  foreach parm $parms {
    if {!$first} {
      puts ,
    }
    set first false
    puts -nonewline "  [type $parm] [name $parm]"
  }
  if {$first} {
    puts -nonewline void
  }
  puts -nonewline ")"
}

proc cerror-decl {name parms} {
  puts "ava_compile_error* ava_error_${name}("
  puts -nonewline "  const ava_compile_location* location"
  foreach parm $parms {
    puts ,
    puts -nonewline "  [type $parm] [name $parm]"
  }
  puts -nonewline ")"
}

proc is-ascii9 {segment} {
  expr {[string is ascii $segment] && [string length $segment] <= 9}
}

proc quote {segment} {
  set segment [string map {"\\" "\\\\" "\"" "\\\""} $segment]
  return "\"$segment\""
}

proc ava_string-to-string {var} {
  return $var
}

proc ava_integer-to-string {var} {
  return "ava_to_string(ava_value_of_integer($var))"
}

proc ava_value-to-string {var} {
  return "ava_to_string($var)"
}

proc build-string {dst code fmt parms} {
  set fmt "$code: $fmt"

  set types {}
  foreach parm $parms {
    dict set types [name $parm] [type $parm]
  }
  puts "  $dst = AVA_EMPTY_STRING;"
  set literal true
  foreach segment [split $fmt %] {
    if {$literal} {
      if {{} eq $segment} {
        # Nothing to do
      } elseif {[is-ascii9 $segment]} {
        puts "  $dst = ava_string_concat("
        puts "    $dst, AVA_ASCII9_STRING([quote $segment]));"
      } else {
        puts "  { AVA_STATIC_STRING(_segment, [quote $segment]);"
        puts "    $dst = ava_string_concat($dst, _segment); }"
      }
    } else {
      puts "  $dst = ava_string_concat("
      puts "    $dst,"
      puts "    [[dict get $types $segment]-to-string $segment]);"
    }
    set literal [expr {!$literal}]
  }
}

puts "/* This file was auto-generated by generate-errors.tcl */"
if {"header" eq [lindex $argv 0]} {
  proc error-common {code name parms spec} {
    puts "/**\n * @see ava_error_${name}()\n */"
    puts "#define AVA_ERROR_[string toupper $name] [ncode $code]"
    puts "/**\n[dict get $spec explanation]\n */"
  }

  proc serror {code name parms spec} {
    error-common $code $name $parms $spec
    serror-decl $name $parms
    puts ";"
  }

  proc cerror {code name parms spec} {
    error-common $code $name $parms $spec
    cerror-decl $name $parms
    puts ";"
  }
} else {
  puts "#ifdef HAVE_CONFIG_H"
  puts "#include <config.h>"
  puts "#endif"
  puts "#define AVA__INTERNAL_INCLUDE 1"
  puts "#include \"avalanche/defs.h\""
  puts "#include \"avalanche/alloc.h\""
  puts "#include \"avalanche/string.h\""
  puts "#include \"avalanche/integer.h\""
  puts "#include \"avalanche/errors.h\""

  proc serror {code name parms spec} {
    serror-decl $name $parms
    puts ""
    puts "{"
    puts "  ava_string _message;"
    build-string _message $code [dict get $spec msg] $parms
    puts "  return _message;"
    puts "}"
  }

  proc cerror {code name parms spec} {
    cerror-decl $name $parms
    puts ""
    puts "{"
    puts "  ava_compile_error* _error = AVA_NEW(ava_compile_error);"
    puts "  _error->location = *location;"
    build-string _error->message $code [dict get $spec msg] $parms
    puts "  return _error;"
    puts "}"
  }
}

eval $defs
