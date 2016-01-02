/*-
 * Copyright (c) 2015, 2016, Jason Lingle
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
#ifndef AVA__INTERNAL_INCLUDE
#error "Don't include avalanche/function.h directly; just include avalanche.h"
#endif

#ifndef AVA_RUNTIME_FUNCTION_H_
#define AVA_RUNTIME_FUNCTION_H_

#include "defs.h"
#include "string.h"
#include "value.h"
#include "pointer.h"

/**
 * @file
 *
 * Provides facilities for invoking functions from Avalanche, both statically
 * and dynamically. This is used for calling both C and Avalanche functions.
 *
 * A function specification is a list of at least three elements. The first
 * element specifies the address. The second is the calling convention.
 * Following that is the return type (if the calling convention requires it)
 * and the argument specifications. Each argument specification is itself a
 * list that begins with the marshalling specification if required by the
 * calling convention and is followed by the binding specification for the
 * argument.
 *
 * The address is simply the numeric value of the address of the native (C)
 * function to invoke.
 *
 * The marshalling-specification is described detail further down.
 *
 * The binding specification indicates how any given list of parameters is
 * translated to function-level arguments.
 *
 * The number of arguments taken by a function is derived from the length of
 * this list; it is equal to the length of the list minus two or three,
 * depending on whether the marshalling specification specifies a return type.
 *
 * Terminology note: "Argument" refers to a value passed into a function as the
 * function receives it; "parameter" refers to a single value at the call site.
 * For example, variadic arguments bundle zero or more parameters into exactly
 * one argument; optional arguments take zero or one parameters. The number of
 * arguments a function takes is always fixed, but many functions can take a
 * variable number of parameters.
 *
 * ---
 *
 * BINDING SPECIFICATION
 *
 * The binding specification is a sequence of lists of the same length as the
 * number of arguments described by the marshalling specification. The legal
 * forms of a binding element are:
 *
 * - `implicit value`. The given value is _always_ passed as this argument for
 *   the function. (This is used in the implementation of closures and
 *   similar.)
 *
 * - `pos`. The argument is mandatory and derives its value based on absolute
 *   position within the parameter list.
 *
 * - `empty`. Exactly like `pos`, but the resulting value is required to be a
 *   static empty value. Note that unlike other places where a dynamic
 *   parameter will cause binding to fail with ava_fbs_unknown, empty arguments
 *   will fail with ava_fbs_impossible since there is never any reason to pass
 *   a non-constant value in. A spread parameter will still result in
 *   ava_fbs_unpack in order to support constructs like forwarding.
 *
 * - `pos default`. The argument is optional. If it is specified in the call,
 *   it gains its value based on absolute position within the parameter list;
 *   otherwise, its value is the given default.
 *
 * - `varargs`. Zero or more parameters, based on absolute position, are packed
 *   into a list and passed as a single argument.
 *
 * - `named name`. The argument is mandatory, but passed by the given name
 *   (which conventionally begins with a hyphen). Order of all named arguments
 *   is irrelevant for any contiguous sequence of such arguments.
 *
 * - `named name default`. Like `named name`, but the argument is optional,
 *   defaulting to the given value.
 *
 * - `bool name`. Like `named`, but the argument is optional. If not specified,
 *   defaults to "false". If specified, it is given the value "true". Though it
 *   is not named "named", `bool` arguments are considered named arguments.
 *
 * Named parameters are expressed in Avalanche as the name of the parameter
 * followed by (if not `bool`) its value. A named parameter may not be
 * specified more than once, either in the binding specification or the
 * function invocation.
 *
 * Unlike Tcl, Avalanche permits no ambiguity regarding the use of named
 * parameters; there is nothing special about arguments beginning with a
 * hyphen, and thus no reason to need to worry about (or support, for that
 * matter) the "--" argument.
 *
 * Argument types other than `implicit value` and `pos` are said to be
 * _variable-shaped_. All variably-shaped arguments must be contiguous. No
 * variably-shaped arguments may follow a `varargs` argument.
 *
 * The invocation parameter list is bound to arguments as follows:
 *
 * - `implicit value` arguments are given their value and do not otherwise play
 *   into this process.
 *
 * - Parameters are directly bound to arguments left-to-right starting from the
 *   first parameter and first argument until a non-`pos` argument is
 *   encountered. An error occurs if insufficient parameters are given for this
 *   to complete.
 *
 * - If the previous step did not bind all the arguments, parameters are
 *   directly bound to arguments right-to-left starting from the last parameter
 *   and the last argument until a non-`pos` argument is encountered. An error
 *   occurs if insufficient parameters are given for this to complete.
 *
 * - The remaining arguments, now only variably-shaped, are inspected
 *   left-to-right until all parameters or all arguments have been consumed.
 *
 *   - If the left-most argument is a `pos value`, it is passed the value of
 *     the left-most parameter, and both are consumed.
 *
 *   - If the left-most argument is a `varargs`, all remaining parameters are
 *     packed into a list and passed to that argument. This consumes that
 *     argument and all remaining parameters.
 *
 *   - If the left-most argument is any of the named parameter types, the value
 *     of the left-most parameter is inspected. Its value is compared against
 *     the name of every named argument from the left-most argument to (but
 *     obviously not including) the first argument in the remaining list. If no
 *     name matches, an error occurs.
 *
 *     - If a `named` argument matches, the next parameter is bound to that
 *       argument, and the matching argument and both parameters are consumed.
 *
 *     - If a `bool` argument matches, the matching argument is bound to
 *       "true", and the argument and parameter are consumed.
 *
 * - Any reamining arguments are bound to their defaults and consumed.
 *
 *   - `pos default` and `named name default` are bound to `default`.
 *   - `bool` is bound to "false".
 *   - `varargs` is bound to the empty string.
 *
 * - An error occurs if any parameters were left unconsumed by this process.
 *
 * Examples:
 *
 * Avalanche-style puts:
 *
 *   Spec: [[bool -n] [named -o stdout] pos]
 *
 *   Bindings:
 *     [foo] -> (false,stdout,foo)
 *     [-n foo] -> (true,stdout,foo)
 *     [-o stderr foo] -> (false,stderr,foo)
 *     [-o stderr -n foo] -> (true,stderr,foo)
 *     [-n -o stderr foo] -> (true,stderr,foo)
 *     [-n] -> (false,stdout,-n)
 *     [-o stderr -n] -> (false,stderr,-n)
 *     [-n -o] -> (true,stdout,-o)
 *     [foo bar] -> error ("foo" cannot be bound)
 *     [-o stderr] -> error ("-o" needs an argument, but "stderr" bound to 3rd)
 *     [-n -n] -> (true,stdout,-n)
 *     [-n -n -n] -> error (the second "-n" doesn't bind to any argument)
 *
 * Tcl puts:
 *
 *   Spec: [[bool -nonewline] [pos stdout] pos]
 *
 *   Bindings:
 *     [foo] -> (false,stdout,foo)
 *     [stderr foo] -> error (stderr cannot be bound to the named argument)
 *     [-n stderr foo] -> (true,stderr,foo)
 *     [-n foo] (true,stderr,foo)
 *
 *   Note how Tcl's ambiguity is avoided: Given a statement like `puts $x foo`,
 *   there's no question as to how "$x" ends up being interpreted --- it always
 *   binds to a named argument. (Not that that's useful in this particular
 *   case, but that's just an API design issue.) Tcl side-steps the issue in
 *   this particular case because channel names never begin with a hyphen.
 *
 * Contrived varargs:
 *
 *   Spec: [pos varargs pos]
 *
 *   Bindings:
 *     [foo bar] -> (foo,[],bar)
 *     [foo baz bar] -> (foo,[baz],bar)
 *     [foo baz quux bar] -> (foo,[baz quux],bar)
 *
 * ---
 *
 * MARSHALLING SPECIFICATION
 *
 * The calling convention and marshalling specification describes how arguments
 * are actually passed to the native function.
 *
 * In the "ava" calling convention, there is no return type, and arguments do
 * not have marshalling specifications. Internally, the ava calling convention
 * works as follows:
 *
 * - All arguments are ava_values, and the return value is an ava_value.
 *
 * - If the number of arguments is less than or equal to 8, all arguments are
 *   passed as native arguments using the native C calling convention.
 *
 * - If there are more than 8 arguments, they are packed into a flat array.
 *   The function is passed a pointer to this array using the native C calling
 *   convention. The array is not guaranteed to remain valid after the callee
 *   returns, but the callee may modify the array arbitrarily.
 *
 * The "c" calling convention requires a return type, and every argument's
 * specification is prefixed with the type of that argument. Permissible types
 * for the return type and argument types are:
 *
 * - `void`. For the return type, indicates that the function always returns
 *   the empty string. For an argument type, indicates that the argument is not
 *   actually passed to the native function, and that it must be the empty
 *   string.
 *
 * - `byte`, `short`, `int`, `long`, `llong`, `float`, `double`, `ldouble`,
 *   `size`. Correspond to the C primitive types `char`, `short`, `int`,
 *   `long`, `long long`, `float`, `double`, `long double`, and `size_t`,
 *   respectively. The integer types are signed; they can be prefixed with `u`
 *   to make them unsigned. The types corresponding to char are called "byte"
 *   to avoid any expectation that they are interpreted as actual characters.
 *
 * - `ava_ubyte`, `ava_sbyte`, `ava_ushort`, `ava_sshort`, `ava_uint`,
 *   `ava_sint`, `ava_ulong`, `ava_slong`, `ava_integer`, and `ava_real` to
 *   refer to the Avalanche-defined types of the same name.
 *
 * - `string`. Arguments with this type are converted to C strings and passed
 *   as an (assumed const) pointer. Return values are interpreted as C strings
 *   and converted back to values.
 *
 * - `X*` or `X&`, for any `X`. Indicates a pointer with (Avalanche) type X.
 *   There is obviously no checking that `X` is in any way related to the
 *   pointer type the native function actually takes.
 *
 * Examples:
 *
 * - fopen: `[c FILE* [string pos]]`
 * - fclose: `[c int [FILE* pos]]`
 * - qsort: `[c void [* pos] [size pos] [size pos] [& pos]]`
 * - rand: `[c uint [void pos]]`
 *
 * The "this" and "msstd" calling conventions are syntactically equivalent to
 * the "c" calling convention. They correspond to the "thiscall" and "stdcall"
 * native calling conventions on platforms that have them. If unsupported, both
 * behave like "c".
 */

/**
 * The maximum number of arguments passed in-line to a function using the "ava"
 * calling convention.
 */
#define AVA_CC_AVA_MAX_INLINE_ARGS 8

/**
 * Special value used to indicate that a parameter index points to no parameter
 * at all.
 */
#define AVA_FUNCTION_NO_PARAMETER (~(size_t)0)

typedef struct ava_function_s ava_function;

/**
 * Indicates the calling convention used by a function.
 */
typedef enum {
  /**
   * The ava calling convention. Up to AVA_CC_AVA_MAX_INLINE_ARGS arguments are
   * passed as ava_value arguments; if there are more, the function instead
   * takes a size_t indicating argument count, followed by
   * `ava_value*restrict`, which is an array of arguments passed to the
   * function. In the latter case, the array is considered clobbered by the
   * call. The function returns ava_value.
   */
  ava_cc_ava = 0,
  /**
   * The C calling convention. Values are passed to and returned from the
   * function using C pritive types.
   */
  ava_cc_c,
  /**
   * Like ava_cc_c, but uses the thiscall calling convention if such a thing
   * exists on the platform.
   */
  ava_cc_this,
  /**
   * Like ava_cc_c, but uses the stdcall calling convention if such a thing
   * exists on the platform.
   */
  ava_cc_msstd
} ava_calling_convention;

/**
 * Indicates a native type used by the C calling convention.
 */
typedef enum {
  /**
   * C type: void
   *
   * Return: The function returns the empty string.
   * Argument: No argument is actually passed, but the bound parameter must be
   * the empty string.
   */
  ava_cmpt_void = 0,
  /** C type: signed char */
  ava_cmpt_byte,
  /** C type: signed short */
  ava_cmpt_short,
  /** C type: signed int */
  ava_cmpt_int,
  /** C type: signed long */
  ava_cmpt_long,
  /** C type: signed long long */
  ava_cmpt_llong,
  /** C type: unsigned char */
  ava_cmpt_ubyte,
  /** C type: unsigned short */
  ava_cmpt_ushort,
  /** C type: unsigned int */
  ava_cmpt_uint,
  /** C type: unsigned long */
  ava_cmpt_ulong,
  /** C type: unsigned long long */
  ava_cmpt_ullong,
  /** C type: ava_sbyte */
  ava_cmpt_ava_sbyte,
  /** C type: ava_sshort */
  ava_cmpt_ava_sshort,
  /** C type: ava_sint */
  ava_cmpt_ava_sint,
  /** C type: ava_slong */
  ava_cmpt_ava_slong,
  /** C type: ava_ubyte */
  ava_cmpt_ava_ubyte,
  /** C type: ava_ushort */
  ava_cmpt_ava_ushort,
  /** C type: ava_uint */
  ava_cmpt_ava_uint,
  /** C type: ava_ulong */
  ava_cmpt_ava_ulong,
  /** C type: ava_integer */
  ava_cmpt_ava_integer,
  /** C type: size_t */
  ava_cmpt_size,
  /** C type: float */
  ava_cmpt_float,
  /** C type: double */
  ava_cmpt_double,
  /** C type: long double */
  ava_cmpt_ldouble,
  /** C type: ava_real */
  ava_cmpt_ava_real,
  /** C type: const char*, interpreted as NTBS */
  ava_cmpt_string,
  /**
   * C type: void*
   *
   * Note that this type requires the pointer_proto on the
   * ava_c_marshalling_type to be set appropriately.
   */
  ava_cmpt_pointer
} ava_c_marshalling_primitive_type;

/**
 * Describes a return type or argument type for a function using the C calling
 * convention.
 */
typedef struct {
  /**
   * The primitive type of the value.
   */
  ava_c_marshalling_primitive_type primitive_type;
  /**
   * If primitive_type == ava_cmpt_pointer, the prototype for the pointer. For
   * arguments, this indicates how the pointer is decoded and checked; for
   * return values, this prototype is used on the constructed pointer.
   *
   * NULL if primitive_type != ava_cmpt_pointer.
   */
  const ava_pointer_prototype* pointer_proto;
} ava_c_marshalling_type;

/**
 * Indicates the binding method used for a particular argument.
 */
typedef enum {
  /**
   * The argument implicitly uses the value in the binding, and never consumes
   * a parameter.
   */
  ava_abt_implicit = 0,
  /**
   * The argument binds to exactly one parameter by position.
   */
  ava_abt_pos,
  /**
   * The argument binds to exactly one parameter by position, and that
   * parameter is required to be the empty string. Binding fails with
   * ava_fbs_impossible if the parameter that would be bound to an empty
   * argument is dynamic.
   */
  ava_abt_empty,
  /**
   * The argument binds to at most one parameter by position, otherwise using
   * the default in the binding.
   */
  ava_abt_pos_default,
  /**
   * The argument binds to zero or more parameters by position, collecting them
   * into a single list.
   */
  ava_abt_varargs,
  /**
   * The argument binds to two consecutive parameters, where the first
   * specifies the name given in the binding, and the second is the value.
   */
  ava_abt_named,
  /**
   * The argument binds to zero or two consecutive parameters, where the first
   * specifies the name given in the binding, and the second in the value. If
   * no parameters are bound, the default in the binding is used instead.
   */
  ava_abt_named_default,
  /**
   * The argument binds to at most one parameter, which specifies the name in
   * the binding. If bound, "true" is passed as the value, otherwise "false" is.
   */
  ava_abt_bool
} ava_argument_binding_type;

/**
 * The binding specification for a single argument.
 */
typedef struct {
  /**
   * Indicates how this argument is bound.
   */
  ava_argument_binding_type type;
  /**
   * The name of a named argument.
   */
  ava_string name;
  /**
   * The default of an optional argument; the value of an implicit argument.
   */
  ava_value value;
} ava_argument_binding;

/**
 * Specifies the marshalling and binding for a single argument.
 */
typedef struct {
  ava_c_marshalling_type marshal;
  ava_argument_binding binding;
} ava_argument_spec;

/**
 * Fully specifies a callable function.
 *
 * Note that functions not using the ava calling convention cannot be
 * dynamically invoked until ava_function_init_ffi() is called on it, so care
 * must be taken when statically declaring functions.
 */
struct ava_function_s {
  /**
   * The address of the function to call.
   *
   * Do not attempt to call this directly.
   */
  void (*address)();

  /**
   * The calling convention used by this function.
   */
  ava_calling_convention calling_convention;
  /**
   * If this function uses the C calling convention, the return type;
   * otherwise, meaningless.
   */
  ava_c_marshalling_type c_return_type;
  /**
   * The number of arguments passed to the function. Always at least 1.
   */
  size_t num_args;
  /**
   * An array of length num_args specifying the information for each argument.
   */
  const ava_argument_spec* args;

  /**
   * Extra data needed to invoke functions with the C calling convention
   * dynamically.
   *
   * Initialise with ava_function_init_ffi() if this ava_function is statically
   * declared. Values produced by the function API always have an initialised
   * FFI unless otherwise noted.
   */
  char ffi[AVA_SIZEOF_FFI_CIF];
};

/**
 * Indicates ways in which an argument can be bound to a value.
 */
typedef enum {
  /**
   * The argument takes its value from the parameter at
   * ava_function_bound_argument.v.parameter_index. The value is used as-is; if
   * the parameter is spread, it must be converted to a normalised list, but is
   * still passed as one value.
   */
  ava_fbat_parameter,
  /**
   * The argument takes its value from the list produced by the variadic
   * collection.
   *
   * The length of the collection, in terms of parameters (not elements), can
   * be found in ava_function_bound_argument.v.collection_size.
   *
   * There will be at most one bound argument with this type.
   */
  ava_fbat_collect,
  /**
   * The argument takes its value from ava_function_bound_argument.v.value.
   */
  ava_fbat_implicit
} ava_function_bound_argument_type;

/**
 * Describes how a function argument is bound to a value.
 */
typedef struct {
  /**
   * The manner in which this argument is bound.
   */
  ava_function_bound_argument_type type;
  /**
   * The index of the parameter which triggered this binding, or
   * AVA_FUNCTION_NO_PARAMETER if the argument is not bound to any parameter.
   *
   * This is distinct from parameter_index in that it (a) is set for all
   * arguments which are bound as a result of parameters, and (b) for named
   * arguments, it references the parameter that specified the name (including
   * for bool) rather than the value.
   *
   * For varargs, this references the first parameter used as input to the
   * collection, if any.
   */
  size_t trigger_parameter_index;
  /**
   * Type-specific information.
   */
  union {
    /**
     * For ava_fbat_implicit, the exact value to pass to the call.
     */
    ava_value value;
    /**
     * For ava_fbat_collect, the number of parameters that comprise the
     * variadic collection.
     */
    size_t collection_size;
    /**
     * For ava_fbat_parameter, the index of the parameter bound to this
     * argument.
     */
    size_t parameter_index;
  } v;
} ava_function_bound_argument;

/**
 * Return value from ava_function_bind().
 */
typedef enum {
  /**
   * All arguments were bound with the information given.
   */
  ava_fbs_bound,
  /**
   * Not all arguments were bound, but there is also insufficient information
   * to determine whether binding can succeed.
   */
  ava_fbs_unknown,
  /**
   * Not all arguments are bound because it is known to be impossible to do so.
   *
   * For example, too many paramaters were passed to the function, or a
   * constant not corresponding to any named parameter was placed where a named
   * parameter was expected.
   */
  ava_fbs_impossible,
  /**
   * Arguments could not be bound because a spread parameter spans non-variadic
   * arguments. The spread parameters will need to be unpacked to flat arrays
   * of static parameters and the binding retried.
   */
  ava_fbs_unpack
} ava_function_bind_status;

/**
 * Indicates the type of an ava_function_parameter.
 */
typedef enum {
  /**
   * The parameter is passed as a single value, and that value is known at the
   * time of binding.
   */
  ava_fpt_static = 0,
  /**
   * The parameter is passed as a single value, but that value is not known at
   * the time of binding.
   */
  ava_fpt_dynamic,
  /**
   * The parameter is to be interpreted as a list and every element passed as a
   * separate argument. The value need not be known at binding time.
   */
  ava_fpt_spread
} ava_function_parameter_type;

/**
 * A single parameter passed to a function.
 */
typedef struct {
  /**
   * The type of this parameter.
   */
  ava_function_parameter_type type;
  /**
   * If the type implies that the value is known at binding-time, the actual
   * value of this parameter.
   */
  ava_value value;
} ava_function_parameter;

/**
 * Wraps the given function in an ava_value.
 */
ava_value ava_value_of_function(const ava_function* fun);
/**
 * Converts the given value into a function.
 *
 * @throw ava_format_exception if val if not parsable as a function or
 * describes an invalid function.
 * @throw ava_internal_exception if constructing the FFI data fails.
 */
const ava_function* ava_function_of_value(ava_value val);

/**
 * Returns whether the given function is valid.
 *
 * All functions produced by ava_function_of_value() are valid; this is
 * intended for callers which construct ava_functions themselves.
 *
 * This call assumes that all fields are correctly initialised; eg, it does not
 * attempt to detect invalid calling conventions. Rather, it simply tests that
 * constraints such as positioning of variably-shaped arguments are followed.
 *
 * @param msg If the function is invalid, set to a message indicating why.
 * @param fun The function to check for validity.
 * @return Whether fun is a valid function.
 */
ava_bool ava_function_is_valid(ava_string*restrict msg,
                               const ava_function* fun);

/**
 * Initialises the ffi field of the given function.
 *
 * This must be called on a statically-initialised function before it can be
 * invoked. It is not necessary to call this on functions using the ava calling
 * convention, though doing so is not an error.
 *
 * @throw ava_internal_exception if constructing the FFI data fails.
 */
void ava_function_init_ffi(ava_function* fun);

/**
 * Attempts to bind parameters to arguments of the given function.
 *
 * If this call fails, the contents of the output arrays are undefined.
 *
 * @param fun The function to bind.
 * @param num_parameters The number of parameters being passed to the function.
 * @param parms Array of parameters passed to the function of length
 * num_parameters.
 * @param bound_args Array of arguments in which the resulting bindings will be
 * stored. Length is the number of arguments taken by fun.
 * @param variadic_collection Output array of parameter indices for packing a
 * variadic argument. The length of the output is defined by collection_size on
 * the parameter with a "collection" binding. The argument is composed by
 * list-appending (for normal parameters) or list-concatting (for spread
 * parameters) the identified parameters from left to right, starting from the
 * empty list.
 * @param message If ava_fbs_impossible is returned, set to an error message.
 * @return The status of the binding.
 */
ava_function_bind_status ava_function_bind(
  const ava_function* fun,
  size_t num_parameters,
  const ava_function_parameter parms[ /* num_parameters */],
  ava_function_bound_argument bound_args[/* fun->num_args */],
  size_t variadic_collection[/* num_parameters */],
  ava_string* message);

/**
 * Applies the output of ava_function_bind() to a parameter list to produce an
 * array of values that can be passed to ava_function_invoke().
 *
 * @param num_args The number of arguments taken by the bound function.
 * @param arguments Output array that will hold the value of each argument.
 * @param parms The parameters passed to the function. All parameters must have
 * known values.
 * @param bound_args The bound_args output from ava_function_bind().
 * @param variadic_collection The variadic_collection output from
 * ava_function_bind().
 */
void ava_function_apply_bind(
  size_t num_args,
  ava_value arguments[ /* num_args */],
  const ava_function_parameter parms[],
  const ava_function_bound_argument bound_args[/* fun->num_args */],
  const size_t variadic_collection[]);

/**
 * Invokes the given function with the given list of arguments (not logical
 * parameters).
 *
 * @param fun The function to invoke.
 * @param argument The physical arguments to pass to the function. An array of
 * length fun->num_args. This array is destroyed by this call, and its contents
 * are undefined after return.
 * @return The return value of the function.
 * @see ava_function_bind_invoke()
 */
ava_value ava_function_invoke(
  const ava_function* fun,
  ava_value arguments[/* fun->num_args */]);

/**
 * Binds all parameters in the given function to arguments. No arguments may
 * have dynamic type. The parameters are unpacked if necessary. If binding
 * fails, an exception is thrown.
 *
 * @param arguments Array to which final arguments are written.
 * @param fun The function to bind.
 * @param num_parameters The number of parameters being passed to the function.
 * @param parms An array of length num_parameters containing the parameters
 * being passed. All values must be known.
 * @throw ava_error_exception if the parameters cannot be bound to the function.
 */
void ava_function_force_bind(
  ava_value arguments[ /* fun->num_args */],
  const ava_function* fun,
  size_t num_parameters,
  const ava_function_parameter parms[/* num_parameters */]);

/**
 * Dynamically binds and invokes the given function on the given list of
 * logical parameters.
 *
 * This is a convenience for ava_function_bind(), ava_function_apply_bind(),
 * ava_function_invoke(), and correctly handling ava_fbs_unpack.
 *
 * @param fun The function to invoke.
 * @param num_parameters The number of parameters being passed to the function.
 * @param parms An array of length num_parameters containing the parameters
 * being passed. All values must be known.
 * @return The return value of the function.
 * @throw ava_error_exception if the parameters cannot be bound to the function.
 */
ava_value ava_function_bind_invoke(
  const ava_function* fun,
  size_t num_parameters,
  const ava_function_parameter parms[/*num_parameters*/]);

/**
 * Performs in-place partial function application on the given function.
 *
 * The first num_args non-implicit arguments of fun are changed to implict
 * arguments whose values are derived from successive values in args.
 *
 * This is used by the `partial` P-Code instruction, which itself is used to
 * implement closures. The semantics of this function are not very useful in
 * general, since parameter binding is bypassed.
 *
 * @param argspecs The argspecs of the function to partially apply.
 * @param function_num_args The number of physical arguments taken by the
 * function.
 * @param input_num_args The number of arguments in the partial application.
 * @param args The array of arguments to partially apply.
 * @throws ava_internal_exception if fun does not have num_args non-implicit
 * arguments.
 */
void ava_function_partial(
  ava_argument_spec*restrict argspecs,
  size_t function_num_args,
  size_t input_num_args,
  const ava_value*restrict args);

#endif /* AVA_RUNTIME_FUNCTION_H_ */
