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
    msg "Use of pointer type %provided% where %expected% was expected."
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

  serror R0035 extract_element_from_empty_list {} {
    msg "Attempt to extract elements from empty list."
    explanation {
      The user code implies that some list should be non-empty, but it is not.
      This error only arises from relatively low-level situations, such as
      using the spread operator where a function name would normally occur.
    }
  }

  serror R0036 bad_list_multiplicity {} {
    msg "Bad list element multiplicity."
    explanation {
      An "each" clause of a loop expected a list whose length was an even
      multiple of some integer, but it was not and the end of the list was
      reached.

      For example, the clause
        each a b c in $list
      requires $list to be a list whose length is an even multiple of 3. If it
      is not, upon reaching the end of the list, there will not be sufficient
      elements to assign to c and possibly b, and this exception will be
      thrown.
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
      input and is probably a bug.
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

  cerror C5020 ambiguous_bareword {} {
    msg "Bareword is ambiguous."
    explanation {
      The compiler was not able to determine whether the indicated token was a
      macro because more than one symbol matches the name, at least one of
      which was a potentially-matching macro.

      Ambiguous symbols result when more than one import produces the same
      symbol name. Note that macros share the same namespace as variables
      and functions, so the conflicting symbols are not necessarily macros.

      This issue can be resolved by using a qualified name for the function, or
      adjusting imports so that names cannot conflict, for example by ensuring
      that each import has a distinct prefix on the imported symbols.
    }
  }

  cerror C5032 ambiguous_var {{ava_string name}} {
    msg "Variable name is ambiguous: %name%"
    explanation {
      The compiler was not able to determine what variable to access because
      more than one symbol matches the name.

      Ambiguous symbols result when more than one import produces the same
      symbol name. Note that variables share the same namespace as functions
      and macros, so the conflicting symbols are not necessarily variables.

      This issue can be resolved by using a qualified name for the variable, or
      adjusting imports so that names cannot conflict, for example by ensuring
      that each import has a distinct prefix on the imported symbols.
    }
  }

  cerror C5040 macro_resolved_bareword_ambiguous {{ava_string name}} {
    msg "Symbol is ambiguous: %name%"
    explanation {
      The containing macro requested the given name be resolved to a
      fully-qualified name at definition time, but the given name is ambiguous.
    }
  }

  cerror C5050 ambiguous_alias {{ava_string name}} {
    msg "Symbol to be aliased is ambiguous: %name%"
    explanation {
      The source name (right-hand-side of an alias) cannot be resolved to a
      single symbol.

      Ambiguous symbols result when more than one import produces the same
      symbol name. Note that variables share the same namespace as functions
      and macros, so the conflicting symbols are not necessarily variables.

      This issue can be resolved by using a qualified name for the variable, or
      adjusting imports so that names cannot conflict, for example by ensuring
      that each import has a distinct prefix on the imported symbols.
    }
  }

  cerror C5102 ambiguous_label {{ava_string name}} {
    msg "Label reference is ambiguous: %name%"
    explanation {
      The given name used as a label reference could refer to more than one
      symbol.

      Labels occupy the same namespace as other symbols, so the conflicting
      symbols are not necessarily labels.
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

  cerror C5046 macro_arg_must_be_block {{ava_string name}} {
    msg "Not a block, for argument %name%"
    explanation {
      The indicated argument is required to be a brace-enclosed block, but
      something else was found.
    }
  }

  cerror C5072 macro_arg_must_be_substitution {{ava_string name}} {
    msg "Not a substitution, for argument %name%"
    explanation {
      The indicated argument is required to be a parenthesis-enclosed
      substitution, but something else was found.
    }
  }

  cerror C5075 macro_arg_must_be_substitution_or_block {{ava_string name}} {
    msg "Not a substitution or block, for argument %name%"
    explanation {
      The indicated argument is required to be a parenthesis-enclosed
      substitution or a brace-enclosed block, but something else was found.
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

  # No longer a possible error after the symtab rewrite
  # cerror C5018 symbol_redefined_import {{ava_string symbol}} {
  #   msg "Redefinition of symbol's simple name: %symbol%"
  #   explanation {
  #     An attempt was made to define the given symbol, but an automaic import in
  #     effect resulted in a name collision with another symbol.
  #   }
  # }

  # No longer a possible error after the symtab rewrite
  # cerror C5019 apply_imports_produced_conflict {} {
  #   msg "Name conflict caused by later definitions."
  #   explanation {
  #     The imports in effect at this location caused a name conflict with names
  #     defined later in the same input.
  #   }
  # }

  # C5020 ambiguous_bareword is grouped with the other ambiguous_* errors

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

  cerror C5027 assignment_to_function {{ava_string name}} {
    msg "Assignment to function %name%"
    explanation {
      The indicated location appears to assign a new value to a function, which
      is not a permissible operation.

      The intent may have been to introduce a new variable. Most commonly, this
      may happen at global scope if an import made an external function
      available with the same name. The name conflict must be resolved either
      by changing the import or renaming one of the conflicting symbols.
    }
  }

  cerror C5028 assignment_to_macro {{ava_string name}} {
    msg "Assignment to macro %name%"
    explanation {
      The indicated location appears to assign a new value to a macro, which is
      not a meaningful operation.

      The intent may have been to introduce a new variable. Most commonly, this
      may happen at global scope if an import made an external macro available
      with the same name. The name conflict must be resolved either by changing
      the import or renaming one of the conflicting symbols.
    }
  }

  cerror C5029 assignment_to_readonly_var {{ava_string name}} {
    msg "Assignment to read-only variable %name%"
    explanation {
      The indicated location appears to assign a new value to a variable, but
      the variable is marked read-only.

      The intent may have been to introduce a new variable. Most commonly, this
      may happen at global scope if an import made an external variable
      available with the same name. The name conflict must be resolved either
      by changing the import or renaming one of the conflicting symbols.
    }
  }

  cerror C5030 assignment_to_closed_var {{ava_string name}} {
    msg "Assignment to closed-over variable %name% from nested function."
    explanation {
      A variable assignment references a local variable from an enclosing
      function within a nested function as its assignee.

      Nested functions in Avalanche carry snapshots of the local variables from
      their enclosing function(s), which do not reference the original
      variables themselves. Updates to these captured variables would not be
      reflected within the enclosing function, other nested functions, or even
      later invocations of the same nested function. To prevent such confusion,
      assignment to the captured variables is forbidden.

      If the intent is to create a new, independent variable, a distinct name
      must be used.

      In order to use the variable as a shared, mutable location, a box must be
      assigned to the variable and the box itself updated.
    }
  }

  cerror C5031 assignment_to_var_read {} {
    msg "Assignment to read of variable."
    explanation {
      The left-hand-side of an assignment is a variable read rather than the
      name of a variable.

      An assignment of the form $var = value would imply setting a variable
      somehow referenced by the expansion of $var to the chosen value (which
      would be similar to "variable variables" in PHP or the set $var value
      form in Tcl), rather than setting the "var" variable itself.

      The correct form of assignment is simply var = value, with no dollar
      sign.
    }
  }

  cerror C5099 assignment_to_other {{ava_string name} {ava_string type}} {
    msg "Assigment to symbol %name% of type %type%"
    explanation {
      The indicated location appears to assign a new value to something that is
      not a variable.
    }
  }

  # cerror C5032 ambiguous_var is group with the other ambiguous_* errors

  cerror C5033 no_such_var {{ava_string name}} {
    msg "No such variable: %name%"
    explanation {
      No symbol with the indicated name could be found.

      Most likely, the name is misspelled or there is a misplaced dollar sign.
    }
  }

  cerror C5034 use_of_macro_as_var {{ava_string name}} {
    msg "Variable read refers to macro %name%"
    explanation {
      The name in the indicated variable read refers to a macro rather than an
      actual variable or function.
    }
  }

  cerror C5100 use_of_other_as_var {{ava_string name} {ava_string type}} {
    msg "Variable read refers to non-variable %name% of type %type%"
    explanation {
      The name in the indicated variable read refers to something that is not a
      variable or function.
    }
  }

  cerror C5035 bad_macro_type {{ava_string type}} {
    msg "Bad macro type: %type%"
    explanation {
      The type of a macro must be one of "control", "op" (operator), or "fun"
      (function).
    }
  }

  cerror C5036 bad_macro_precedence {
    {ava_string precedence} {ava_string reason}
  } {
    msg "Bad macro precedence: %precedence%: %reason%"
    explanation {
      The precedence of an operator macro must be a valid integer between 1 and
      40, inclusive.
    }
  }

  cerror C5037 empty_bareword_in_macro_definition {{ava_string bareword}} {
    msg "Name or bareword is effectively empty: %bareword%"
    explanation {
      Barewords and variable names within a macro definition must be preceded
      by a sigil indicating how the bareword is to be interpreted. In most
      cases, these sigils are stripped; this would leave the indicated name
      empty, which is not legal.

      This error most commonly occurs if the actual sigil was forgotten. For
      example, use "%!" to refer to the logical-not operator from a macro.
    }
  }

  cerror C5038 bad_macro_hash_bareword {{ava_string bareword}} {
    msg "Bad hash-prefixed macro bareword: %bareword%"
    explanation {
      The hash ("#") sigil at the beginning of a bareword or variable name in a
      macro is reserved for passing names of special intrinsics through the
      macro, and thus must be at least two characters long and also end with
      another hash character.

      Most likely, the actual sigil was forgotten and this bareword merely
      happens to begin with a hash.
    }
  }

  cerror C5039 macro_resolved_bareword_not_found {{ava_string bareword}} {
    msg "No such symbol: %bareword%"
    explanation {
      The containing macro requested the given bareword to be resolved to a
      fully-qualified name at definition time, but the given name could not be
      found.
    }
  }

  # cerror C5040 macro_resolved_bareword_ambiguous with other ambiguous errors

  cerror C5041 macro_resolved_bareword_invisible {{ava_string symbol}} {
    msg "Symbol has tighter visibility than macro: %symbol%"
    explanation {
      The containing macro directly references the given symbol by
      fully-qualified name, but that symbol has less visibility than the macro.

      Avalanche macros are only partially hygenic; even if a symbol is given
      with its fully-qualified name or is expanded to such at definition time,
      the client code must itself still be able to reference all symbols
      mentioned in the macro.

      Because the indicated symbol has less visibility that the macro, an error
      would result when any client code within the macro's visibility but
      outside the symbol's visibility attempted to use the macro, since the
      client code would not be able to directly reference the symbol.

      Note that this check does not cover all cases. For example, it is
      possible to sneak such references through the macro definition macro by
      combining the fully-qualified name with the verbatim ("!") sigil or by
      disguising the name as an intrinsic. However, doing so will not prevent
      the same problems from arising.
    }
  }

  cerror C5042 bad_macro_bareword_sigil {{ava_string bareword}} {
    msg "Missing or invalid sigil on bareword on macro: %bareword%"
    explanation {
      Barewords and variable names within macros are required to be prefixed
      with a sigil to indicate how they are to be substituted into the calling
      context.

      For variable substitution, the sigil comes after the dollar sign; eg,
      "$?some-var".

      Quick reference:
      "!" --- drop the sigil, then substitute verbatim.
      "%" --- drop the sigil, substitute the fully-qualified name the result
              resolves to in the context of the macro definition.
      "?" --- generate a unique name per macro expansion.
      "<" --- splice arguments to the left of the macro in.
      ">" --- splice arguments to the right of the macro in.
      See the manual for full details.

      For literals which are actually supposed to be plain values, use
      quotation in preference to the "!" sigil. For example, write
        "1" %+ "1"
      rather than
        !1 %+ !1
    }
  }

  cerror C5043 bad_macro_slice_offset {{ava_string tail}} {
    msg "Bad offset for macro slice: %tail%"
    explanation {
      The offset string in a macro splice was not valid.

      Offset strings match the expression
        [0-9]*(-[0-9]+)?
      rather than being arbitrary integers, and cannot simply be the string
      "-".
    }
  }

  cerror C5044 user_macro_execution_error {{ava_string message}} {
    msg "Macro execution error: %message%"
    explanation {
      Execution of a user macro resulted in an error that is supposed to be
      impossible. This error indicates that there is a bug either in the macro
      evaluation code or the code that generated the P-Code containing this
      macro.
    }
  }

  cerror C5045 user_macro_not_enough_args {
    {ava_string macro} {ava_string context}
  } {
    msg "Near this location, in macro %macro%; %context% missing."
    explanation {
      A user macro required certain arguments to be present, but they could not
      be found. Either the use site is missing arguments, or there is a problem
      in the user macro itself.
    }
  }

  # cerror C5046 macro_arg_must_be_block is with other must_be errors.

  cerror C5047 import_explicit_dest_required {{ava_string source}} {
    msg "\"%source%\" is a simple name; must specify import dest explicitly."
    explanation {
      If the destination namespace on an import is omitted, defaults to the
      last conventional namespace part of the source namespace. In this case,
      that would result in the destination being essentially the same as the
      source (possibly making a useless whange from ":" to ".").

      Because of this, the destination namespace must be given for the import
      to be meaningful.
    }
  }

  cerror C5048 import_imported_nothing {{ava_string source}} {
    msg "Import has no effect because nothing starts with \"%source%\""
    explanation {
      The indicated import statement doesn't actually import anything. Most
      likely, the source was misspelled, or the code to make the expected
      symbols exist was forgotten.
    }
  }

  cerror C5049 bad_macro_keyword {
    {ava_string macro} {ava_string actual} {ava_string expected}
  } {
    msg "Bad keyword %actual% in %macro% syntax; expected %expected%"
    explanation {
      The indicated macro requires one of a particular set of barewords in the
      given location, but either a different bareword, or something other than
      a bareword, was found there.
    }
  }

  # cerror C5050 amibguous_alias with other ambiguous_* errors

  cerror C5051 alias_target_not_found {{ava_string name}} {
    msg "Alias target not found: %name%"
    explanation {
      The source name (right-hand-side of an alias) cannot be resolved to a
      any symbol. Most likely, the name was misspelled or the code to make the
      symbol visible with that name was forgotten.
    }
  }

  cerror C5052 alias_more_visible_than_target {
    {ava_string source} {ava_string dest}
  } {
    msg "Alias has greater visibility than target: %source% -> %dest%"
    explanation {
      The indicated alias attempts to assign a wider visibility to the new
      symbol than the symbol it is an alias of.

      Since visibility may be closely tied to linking on the underlying
      platform, it is not possible for the alias to expose the symbol to code
      which cannot see it under the original name.
    }
  }

  cerror C5053 import_ambiguous {
    {ava_string a} {ava_string b}
  } {
    msg "Import is ambiguous; options include %a% and %b%"
    explanation {
      An import is only permitted to replace one unique prefix from
      fully-qualified names. If the source of an import is not itself found as
      a prefix to any fully-qualified names, it instead compounds with another
      existing import. In this case, there was more than one such import at the
      same priority level.

      To resolve this, use the fully-qualified name in your import.
    }
  }

  cerror C5054 macro_expanded_to_nothing {{ava_string macro}} {
    msg "Macro %macro% expanded to nothing."
    explanation {
      The indicated macro "expanded" into the empty statement. This is almost
      certainly unintentional, and so is flagged as an error. If intentional,
      the macro should be adjusted to expand into some non-empty form that has
      the same effect.
    }
  }

  cerror C5055 use_of_invalid_macro {{ava_string macro}} {
    msg "Use of invalid macro %macro%"
    explanation {
      There is an error in the indicated macro's definition, so the macro
      cannot be expanded. Usages of this macro are flagged with this error to
      indicate that no further macro substitution could be performed on the
      expression.
    }
  }

  cerror C5056 would_discard_semilit {} {
    msg "Value of semiliteral is discarded."
    explanation {
      The indicated semiliteral expression's result is not used.

      While construction of the semiliteral is not necessarily pure (i.e.,
      evaluating its elements could have side-effects), there would still be no
      reason to wrap the contents in a semiliteral, so this is almost certainly
      a programming error.
    }
  }

  serror C5057 parse_isolated_spread {} {
    msg "Spread operator followed by nothing."
    explanation {
      The spread operator ("\*") must be immediately followed by the expression
      it is to spread.

      In the indicated location, it is either at the end of its statement or
      expression, or is incorrectly followed by a line break. Conventionally,
      the operator is directly attached to the thing it spreads, in which case
      this error would not occur.
    }
  }

  cerror C5058 spread_cannot_be_used_here {} {
    msg "Spread operator cannot be used here."
    explanation {
      The spread ("\*") operator only makes sense in certain contexts; it is an
      error to use it anywhere else.
    }
  }

  cerror C5059 function_without_body {} {
    msg "Missing function body."
    explanation {
      A function body consists of either a single block (brace-enclosed) or an
      "=" bareword followed by an arbitrary non-empty expression. No such
      definition was found near the indicated location.

      Possible causes include forgetting spaces around "=", or placing the "="
      at the end of the line without a backslash or opening parenthesis to
      allow the statement to continue ont o the next line.
    }
  }

  cerror C5060 garbage_after_function_body {} {
    msg "Unexpected tokens after brace-form function body."
    explanation {
      A brace-enclosed block alone defines the whole function body; thus, it is
      an error for anything else to follow it, as found in the indicated
      location.
    }
  }

  cerror C5061 defun_without_args {} {
    msg "Function declares no arguments."
    explanation {
      All Avalanche functions are required to take at least one argument, since
      it is awkward to impossible to invoke functions with no parameters.

      If the function doesn't take any meaningful inputs, give the function a
      single argument "()".

      This error could also occur if the code tries to name a positional
      argument "=", since the "=" token begins the function body.
    }
  }

  cerror C5062 defun_nonempty_empty {} {
    msg "Unexpeceted tokens inside \"empty\" argument. (Forgot \"=\"?)"
    explanation {
      A function argument declared by parentheses indicates a nominal, empty
      argument, and the space between the parentheses must be empty.

      Most likely, this was actually intended to be part of the expression-form
      function body, but the actual "=" was forgotten.
    }
  }

  cerror C5063 defun_optional_empty {} {
    msg "Optional argument doesn't declare anything. (Forgot \"=\"?)"
    explanation {
      A function argument declaration enclosed in brackets indicates an optional
      argument. The indicated location has brackets enclosing nothing, which is
      not meaningful.

      Most likely, this was actually intended to be part of the expression-form
      function body, but the actual "=" was forgotten. It is also possible that
      the empty argument "()" was intended.
    }
  }

  cerror C5064 defun_extra_tokens_after_default {} {
    msg "Extraneous tokens after argument default."
    explanation {
      A defaulted argument consists of either a single token identifying the
      argument itself, or a token identifying the argument followed by a
      literal indicating the default value. The indicated token lies beyond the
      default and therefore cannot be ascribed any meaning.
    }
  }

  cerror C5065 defun_invalid_arg {} {
    msg "Invalid argument declaration. (Forgot \"=\"?)"
    explanation {
      An argument declaration for a function takes one of the following forms:
      () --- nominal positional argument that must always be empty
      name --- mandatory positional argument
      -name --- mandatory named argument
      [name] --- optional positional argument, default empty string
      [name default] --- optional positional argument with given default
      [-name] --- optional named argument, default empty string
      [-name default] --- optional named argument with given default
      \*name --- variadic arguments

      If the indicated token isn't expected to be a function argument
      declaration, most likely the "=" of the expression-form function body was
      forgotten.
    }
  }

  cerror C5066 defun_no_explicit_args {} {
    msg "Function declares only optional arguments."
    explanation {
      The indicated function only declares itself to take optional arguments;
      ie, it has only bracket-enclosed argument declarations.

      Since it is awkward to invoke functions without any parameters, functions
      are required to take at least one argument. If all the arguments are
      truly intended to be optional, conventionally a "()" argument is added to
      the end of the argument list.
    }
  }

  cerror C5067 defun_varargs_name_must_be_simple {} {
    msg "Invalid variadic argument name."
    explanation {
      A variadic argument must have a single bareword as its name. This name
      may not begin with a hyphen, since that would imply it is a named
      argument.
    }
  }

  cerror C5068 defun_varargs_in_optional {} {
    msg "Variadic argument cannot be enclosed in brackets."
    explanation {
      Variadic arguments cannot be defaulted (or otherwise made "optional" by
      enclosing them in brackets) because they are necessarily bound to the
      empty list when no parameters are passed to them.

      In most cases, the correct fix is to write "\*foo" instead of "[\*foo]".
      There is no equivalent to "[\*foo default]".
    }
  }

  cerror C5069 defun_discontiguous_varshape {} {
    msg "Variably-shaped arguments is discontiguous from others."
    explanation {
      All "variably-shaped" arguments (optional arguments, named arguments, and
      variadic arguments) must occur in one contiguous group within a
      function's argument list. Were this not the case, the binding of
      parameters to arguments could be ambiguous.
    }
  }

  cerror C5070 defun_varshape_after_varargs {} {
    msg "Variably-shaped argument after variadic argument."
    explanation {
      A "variably-shaped" argument (optional argument, named argument, or
      variadic argument) cannot occur anywhere after a variadic argument in a
      function's argument list, as this could make the binding of parameters to
      arguments ambiguous.
    }
  }

  cerror C5071 ret_at_global_scope {} {
    msg "\"ret\" at global scope."
    explanation {
      A "ret" (return) statement cannot be used at global scope, since there is
      no containing function from which to return.
    }
  }

  # C5072 macro_arg_must_be_substitution group with other must-be errors

  cerror C5073 if_required_else_omitted {} {
    msg "\"else\" required before this point."
    explanation {
      Uses of the "if" macro require the "else" keyword before the final result
      for the sake of clarity whenever the statement has more than one
      conditional clause or when using statement form (i.e., with blocks
      enclosed in braces).
    }
  }

  cerror C5074 if_inconsistent_result_form {} {
    msg "Inconsistent expression/statement-form results in if."
    explanation {
      All result arguments for an "if" usage must be blocks (brace-enclosed, no
      value returned) or substitutions (parenthesis-enclosed, result returned).
      The expected form for a single "if" usage is decided by the first result
      argument; this error is raised if another result block differs from the
      first result argument.
    }
  }

  # cerror C5075 macro_arg_must_be_substitution_or_block with other must-bes

  cerror C5076 statement_form_does_not_produce_a_value {} {
    msg "Statement-form construct does not produce a value."
    explanation {
      Control structures generally come in two forms: statement-form and
      expression-form, identified by result bodies being brace-enclosed blocks
      or parenthesis-enclosed substitutions, respectively. The indicated
      construct is in statement form, and thus cannot be used as an expression.

      For example, the following is invalid due to this error:
        foo = if ($answer) { 42 } else { 56 }
      The above would be correctly written
        foo = if ($answer) (42) (56)
    }
  }

  cerror C5077 expression_form_discarded {} {
    msg "Statement-form construct does not produce a value."
    explanation {
      Control structures generally come in two forms: statement-form and
      expression-form, identified by result bodies being brace-enclosed blocks
      or parenthesis-enclosed substitutions, respectively. The indicated
      construct is in expression form, but its result is discarded. This is
      almost certainly unintended, at the very least indicating disaggreement
      between implied intent and actuality.

      For example, the following is invalid due to this error:
        if ($control) (do-something ())
      The above would be correctly written
        if ($control) { do-something () }
    }
  }

  cerror C5078 loop_each_without_in {} {
    msg "\"each\" clause of loop missing \"in\" keyword."
    explanation {
      At the indicated location, the end of the containing statement had been
      encountered before the requisite "in" word.

      The general syntax for the "each" loop clause is

        each lvalue ... in value

      If "in" occurs on a separate line, it may be necessary to escape the line
      separators with a backslash or to surround the expression in parentheses.
    }
  }

  cerror C5079 loop_each_without_lvalues {} {
    msg "\"each\" clause has no lvalues."
    explanation {
      The "in" keyword was found immediately after the start of an "each"
      clause within a loop.

      This would imply iteratively taking zero elements from the list, which is
      not a useful operation.
    }
  }

  cerror C5080 loop_each_without_list {} {
    msg "Missing value after \"in\" of \"each\" loop clause."
    explanation {
      The "in" keyword was found at the end of the containing statement of an
      "each" clause within a loop.

      If the expression for the list occurs on a separate line, it may be
      necessary to escape the line separators with a backslash ,to surround
      the loop as a whole with parentheses, or to place the opening parenthesis
      for the list expression on the same line as "in".
    }
  }

  cerror C5081 loop_for_without_init {} {
    msg "Missing initialiser block for \"for\" loop clause."
    explanation {
      The "for" keyword was found at the end of the statement containing a
      "loop" macro.
    }
  }

  cerror C5082 loop_for_init_not_block {} {
    msg "Unexpected non-block for \"for\" clause initialiser."
    explanation {
      The initialiser clause of a "for" clause in a loop must be a
      brace-surrounded block.
    }
  }

  cerror C5083 loop_for_without_cond {} {
    msg "Missing conditional expression for \"for\" loop clause."
    explanation {
      The condition expression (second argument) of a "for" clause of a loop
      was not found.

      If the condition is on another line, the line separators may need to be
      escaped with backslashes, or the whole loop enclosed in parentheses.
    }
  }

  cerror C5084 loop_for_cond_not_subst {} {
    msg "Unexpected non-substitution for \"for\" clause condition."
    explanation {
      The condition expresison of a "for" clause must be a
      parenthesis-surrounded substitution.
    }
  }

  cerror C5085 loop_for_without_update {} {
    msg "Missing update block for \"for\" loop clause."
    explanation {
      The update block (third argument) of a "for" clause of a loop was not
      found.

      If the update is on another line, the line separators may need to be
      escaped with backslashes, or the whole loop enclosed in parentheses.
    }
  }

  cerror C5086 loop_for_update_not_block {} {
    msg "Unexpected non-block for \"for\" clause update."
    explanation {
      The update block of a "for" clause bust be a brace-enclosed block.
    }
  }

  cerror C5087 loop_while_without_cond {{ava_string name}} {
    msg "Missing condition expression for \"%name%\" loop clause."
    explanation {
      The "while" and "until" loop clauses must be followed by a single
      expression providing the loop condition.
    }
  }

  cerror C5088 loop_while_cond_not_subst {{ava_string name}} {
    msg "Unexpected non-substitution for \"%name%\" clause condition."
    explanation {
      The condition of a "while" or "until" clause must be a single
      parentheses-enclosed expression.
    }
  }

  cerror C5089 loop_do_without_body {{ava_string type}} {
    msg "Missing body block for \"%type%\" loop clause."
    explanation {
      The specified loop clause must be followed by a single block providing
      the loop body.
    }
  }

  cerror C5090 loop_do_body_not_block_or_subst {{ava_string type}} {
    msg "Invalid body of \"%type%\" clause."
    explanation {
      The body of the specified clause must be a single brace-enclosed block or
      parenthesis-enclosed expression.
    }
  }

  cerror C5091 loop_collect_without_value {} {
    msg "Missing value expression for \"collect\" loop clause."
    explanation {
      The "collect" loop clause must be followed by a single expression
      providing the collection value.

      See also "collecting", which takes no arguments and operates on the
      implicit loop value.
    }
  }

  cerror C5092 bad_loop_clause_id {{ava_string id}} {
    msg "Invalid loop clause type: %id%"
    explanation {
      The indicated bareword appears where a loop clause type was expected, but
      it is not a known type of loop clause.

      Quick reference:
        each lvalue... in list
        for {init} (condition) {update}
        while (condition)
        until (condition)
        do { body }
        do (body)
        { body }
        collect value
        collecting
        else { body }
        else (body)
    }
  }

  cerror C5093 loop_garbage_after_else {} {
    msg "Unexpected tokens after \"else\" loop clause."
    explanation {
      The "else" clause of a loop must be the final clause, but something was
      found beyond it.

      If the indicated item was intended to be part of the body of the loop's
      else clause, the whole body must be surrounded in parentheses.
    }
  }

  cerror C5094 bad_loopctl_flag {
    {ava_string macro} {ava_string flag}
  } {
    msg "Unknown flag \"%flag\% to macro %macro%"
    explanation {
      All barewords beginning with a hyphen immediately after the "break" and
      "continue" macros are reserved as flags; the indicated bareword looks
      like such a flag, but is not a known flag.

      The flag may have been misspelled. If the bareword is actually supposed
      to be part of the expression, wrap the expression in parentheses.
    }
  }

  cerror C5095 loopctl_expression_but_suppressed {} {
    msg "Unexpected expression after \"-\" flag."
    explanation {
      The "-" flag was given to the loop control macro preceding the indicated
      location, but it appears an expression follows anyway.

      The "-" flag to the loop control macros indicates to suppress changing
      any iteration or accumulation values, and so no expression would be
      evaluated. Because of this, it is an error to include an expression
      anyway.

      If the "-" was supposed to be part of the expression, wrap the expression
      as a whole in parentheses.
    }
  }

  cerror C5096 loopctl_outside_of_loop {} {
    msg "Loop control cannot be used outside of loop."
    explanation {
      The indicated loop control macro is not within a loop, and therefore
      cannot do anything meaningful.

      Note that loop control cannot cross functions, so this error will also be
      produced if the loop control macro is used within a function or lambda
      nested inside a loop, for example.
    }
  }

  cerror C5097 loopctl_suppressed {} {
    msg "Loop control cannot be used here."
    explanation {
      While the indicated loop control macro does occur within a loop, it is
      not permitted to be used in the indicated location.
    }
  }

  cerror C5098 loopctl_flag_more_than_once {
    {ava_string macro} {ava_string flag}
  } {
    msg "Flag %flag% passed more than once to macro %macro%"
    explanation {
      The indicated flag occurs more than once in the invocation of the
      preceding macro, but there is no defined meaning to passing that flag
      more than once.

      If the second flag was intended to be part of the following expression,
      wrap the whole expression in parentheses.
    }
  }

  # cerror C5099 assignment_to_other with other assignment_to errors
  # cerror C5100 use_of_other_as_var with other use_of_*_as_var errors

  cerror C5101 no_such_label {{ava_string name}} {
    msg "No such label: %name%"
    explanation {
      The given goto target does not match any label visible in the current
      scope.

      Presumably either this reference or its definition was mispelled.
    }
  }

  # cerror C5102 ambiguous_label with other ambiguous errors

  cerror C5103 use_of_other_as_label {
    {ava_string name} {ava_string type}
  } {
    msg "Use of %type% %name% as label."
    explanation {
      The given bareword was expected to resolve to a reference to a label, but
      something else was found instead.
    }
  }

  cerror C5104 use_of_label_in_enclosing_scope {} {
    msg "Use of label from enclosing scope."
    explanation {
      The referenced label is defined in an outer function but used from an
      inner function.

      Labels cannot be used for non-local (i.e., cross-function) control flow.
      Use exception handling for that.
    }
  }

  cerror C5105 use_of_inaccessible_label {} {
    msg "Label is not accessible from this point."
    explanation {
      While the given label is defined in the same scope as this reference, it
      is not legal to reference it from the indicated location.

      Label references may only occur within the structure that defines them,
      even though their symbols follow normal rules of lexical visibility.
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
