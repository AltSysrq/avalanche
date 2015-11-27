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

# Process this file with generate-pcode.tcl to convert to C

# Global declaration types.
#
# In P-Code, globals are always referenced by their index in the global list,
# rather than by name.
#
# The method in which a named reference to an external symbol is resolved is
# defined by the P-Code implementation.
#
# P-Code initialisation sequence:
# - All `load-pkg` statements executed.
# - All `load-mod` statements executed.
# - All global variables and functions initialised and published.
# - All `ext-var` and `ext-fun` statements complete.
# - All `init` statements are executed.
struct global g {
  # Whether this element is a valid target for global variable references.
  attr var
  # Whether this element is a valid target for global function references.
  attr fun
  # Whether this element is a valid target for global references that require
  # some physical entity to exist.
  attr entity
  # Whether the P-Code validator needs special handling for this element.
  attr needs-special-validation
  # Whether this element always participates in linking, despite not having a
  # "publish" property.
  attr effectively-published
  # Whether this element is a definition. Definitions participating in linkage
  # take precedence over declarations, and only one definition for a given name
  # may exist.
  attr linkage-definition
  # Property which is a reference, by absolute index, to an element with the
  # entity attribute in the same P-Code object.
  prop int global-entity-ref
  # Property which is a reference, by absolute index, to an element with the
  # fun attribute in the same P-Code object.
  prop int global-fun-ref
  # Property indicating that the element describes a function with the given
  # prototype.
  #
  # Always singular.
  prop function prototype
  # Property on elements with no semantics besides symbol exporting (ie, to the
  # Avalanche compiler). If false, such entries do not survive linking.
  #
  # Always singular.
  prop bool reexport
  # Property on elements which may or may not participate in linking, depending
  # on assigned visibility.
  #
  # Always singular.
  prop bool publish
  # Property on elements  which may participate in linking, indicating the name
  # that is used for linkage purposes.
  #
  # Always singular.
  prop demangled-name linkage-name

  # Records the source location in effect until the next declaration indicating
  # otherwise.
  elt src-pos {
    str filename
    int line-offset
    int start-line
    int end-line
    int start-column
    int end-column

    constraint {
      @.line_offset >= 0 &&
      @.start_line >= 0 &&
      @.end_line >= @.start_line &&
      @.start_column >= 0 &&
      @.end_column >= @.start_column
    }
  }

  # Declares a global variable defined by a different P-Code unit.
  #
  # Semantics: At any point before this P-Code unit's initialisation
  # function(s) are run, the name of this variable will be resolved to the
  # referenced global symbol. If resolution fails, this unit fails to load.
  # This resolution may occur before the program is executed.
  elt ext-var {
    attr var
    attr entity
    attr effectively-published
    demangled-name name {
      prop linkage-name
    }
  }

  # Declares a global function defined by a different P-Code unit.
  #
  # Semantics: Resolution of the name occurs as with ext-var. The resolved
  # symbol is assumed to be a function with the given prototype.
  elt ext-fun {
    attr var
    attr fun
    attr entity
    attr effectively-published
    demangled-name name {
      prop linkage-name
    }
    function prototype {
      prop prototype
    }
  }

  # Declares a global variable defined by this P-Code unit.
  #
  # Semantics: The global variable is initialised to the empty string at some
  # point before this P-Code's initialisation function(s) are executed. If
  # publish is true and another P-Code unit has a `var` or `fun` with an
  # equivalent name (equivalance being somewhat dependent on the underlying
  # implementation) and publish=true, this unit fails to load at some point
  # before the initialisation takes place, possibly before the program is
  # actually executed.
  #
  # If and only if publish is true, it is possible for any other P-Code unit
  # to use an `ext-var` statement to reference this `var` by name.
  # Otherwise, only this P-Code unit can reference this `var`, and only by
  # global index.
  elt var {
    attr var
    attr entity
    attr linkage-definition
    bool publish {
      prop publish
    }
    demangled-name name {
      prop linkage-name
    }
  }

  # Declares a global function defined by this P-Code unit.
  #
  # Semantics: All notes from `var` apply, except that it is initialised to
  # a function object which can be executed to execute body.
  elt fun {
    attr var
    attr fun
    attr entity
    attr linkage-definition
    bool publish {
      prop publish
    }
    demangled-name name {
      prop linkage-name
    }
    function prototype {
      prop prototype
    }
    # A list of variable names used by this function.
    #
    # Function arguments are bound to the first n entries of this list (which
    # must be at least n elements in size); other variables are internal to the
    # function and uninitialised upon entry.
    ava-list vars
    # The body of this function; a list of executable statements.
    #
    # When the function is called, control begins at the head of the zeroth
    # statement. If control transfers past the final statement in the function,
    # it implicitly returns the empty string.
    struct exe x body

    constraint {
      ava_list_length(@.vars) >= @.prototype->num_args
    }
    constraint {
      ava_cc_ava == @.prototype->calling_convention
    }
  }

  # Exports a symbol visible to Avalanche code referencing a global in this
  # P-Code unit.
  elt export {
    # The global which is being exported.
    int global {
      prop global-entity-ref
    }
    # If true, any P-Code unit created as a composite including this one
    # (ie, linkage of modules into a package) includes this export statement. A
    # value of true corresponds to public visibility; false to internal
    # visibility.
    bool reexport {
      prop reexport
    }
    # The name by which this global is exposed to Avalanche code.
    str effective-name
  }

  # Exports a global user macro.
  #
  # Since macros do not have physical manifestations, they are not declared and
  # exported separately; non-exported macros do not even appear in P-Code.
  elt macro {
    # If true, any P-Code unit created as a composite including this one
    # (ie, linkage of modules into a package) includes this macro statement. A
    # value of true corresponds to public visibility; false to internal
    # visibility.
    bool reexport {
      prop reexport
    }
    # The name by which this macro is exposed to Avalanche code.
    str name
    # The type of this macro.
    # One of ava_st_control_macro, ava_st_operator_macro, ava_st_function_macro
    int type
    # The precedence of this macro.
    # Must be 0 if type != ava_st_operator_macro.
    int precedence
    # The body of this macro.
    struct macro m body

    constraint {
      ava_st_control_macro == @.type ||
      ava_st_operator_macro == @.type ||
      ava_st_function_macro == @.type
    }
    constraint {
      (ava_st_operator_macro == @.type &&
       (@.precedence >= 1 &&
        @.precedence <= AVA_MAX_OPERATOR_MACRO_PRECEDENCE)) ||
      (ava_st_operator_macro != type &&
       @.precedence == 0)
    }
  }

  # Declares that this P-Code unit depends upon a given package.
  #
  # Semantics: Before module-dependency initialisation, all load-pkg statements
  # in the P-Code unit are executed serially in the order given. The named
  # package is loaded and initialised if this has not already occurred. The
  # manner in which packages are loaded is implementation-dependent. If loading
  # or initialising the package fails, initialising this P-Code unit fails at
  # some time before the completion of this statement, possibly before the
  # program is even executed.
  elt load-pkg {
    str name
  }

  # Declares that this P-Code unit depends upon a given module within the same
  # package.
  #
  # Semantics: Before this module's variable initialisation, all load-mod
  # statements in the P-Code unit are executed serially in the order given. All
  # remarks for `load-pkg` also apply to `load-mod`.
  elt load-mod {
    str name
  }

  # Declares a module-initialisation function.
  #
  # Semantics: After all implicit initialisation for this P-Code unit
  # completes, each init statement within the P-Code unit is executed serially
  # in the order given. The function referenced by each is invoked with one
  # parameter equal to the empty string.
  elt init {
    attr needs-special-validation
    int fun {
      prop global-fun-ref
      prop global-entity-ref
    }
  }
}

# A single executable instruction.
struct exe x {
  parent global g

  # Terminal instructions terminate basic blocks naturally.
  attr terminal
  # Terminal-no-fallthrough instructions cannot proceed to the
  # immediately-following instruction. All terminal-no-fallthrough should be
  # terminal.
  attr terminal-no-fallthrough
  # special-reg-read-d instructions read a range of D-registers identified by
  # the 0th reg-read-base and reg-read-count properties.
  attr special-reg-read-d
  # special-reg-read-p instructions read a range of P-registers identifiied by
  # the 0th reg-read-base and reg-read-count properties.
  attr special-reg-read-p
  # can-throw instructions terminate basic blocks at the instruction *preceding
  # them* (even if not marked terminal) and may branch to the current landing
  # pad, if any.
  #
  # The landing-pad branch logically happens *before* the instruction because,
  # if the instruction does throw, any registers written by the instruction are
  # not actually modified.
  attr can-throw
  # push-landing-pad instructions push an exception-handler onto the stack
  # which establishes their 0th landing-pad property as the landing pad for
  # can-throw instructions. Additinoally, they push a caught-exception when
  # transfering to their 0th landing-pad.
  #
  # All such instructions must be terminal.
  attr push-landing-pad
  # pop-exception instructions pop an item from the combined exception stack.
  #
  # All such instructions must be terminal.
  attr pop-exception
  # require-caught-exception instructions require at least one element to exist
  # on the caught-exception stack.
  attr require-caught-exception
  # require-empty-exception instructions require the combined exception stack
  # to be totally empty.
  attr require-empty-exception
  # Identifies fields which are registers that the instruction reads, thereby
  # requiring them to be initialised.
  prop register reg-read
  # Identifies fields which are registers that the instruction writes, thereby
  # initialising them.
  prop register reg-write
  # Identifies a label that the instruction may jump to. There is never more
  # than one jump-target.
  prop int jump-target
  # Identifies a label that the instruction uses as a landing pad (with
  # push-landing-pad).
  prop int landing-pad
  # Indicates whether the landing-pad pushed by a push-landing-pad instruction
  # is a cleanup or a catch.
  prop bool landing-pad-is-cleanup
  # Identifies a field which must refer to a variable-like global entity.
  prop int global-var-ref
  # Identifies a field which must refer to a function-like global entity.
  prop int global-fun-ref
  # Identifies a field which must refer to some global entity. This must be
  # given in addition to global-var-ref and global-fun-ref.
  prop int global-ref
  # Identifies the first register, inclusive, read by a special-reg-read-*
  # attributed instruction. There is never more than one of these properties on
  # an instruction.
  prop int reg-read-base
  # Identifies the number of registers read by a special-reg-read-* attributed
  # instruction. There is never more than one of these properties on an
  # instruction.
  prop int reg-read-count
  # Indicates the number of arguments statically passed to any functions
  # referenced via global-fun-ref. There is never more than one of these
  # properties on an instruction.
  prop int static-arg-count

  # Records the source position in effect until the next declaration indicating
  # otherwise.
  #
  # This is independent of the global src-pos.
  elt src-pos {
    str filename
    int line-offset
    int start-line
    int end-line
    int start-column
    int end-column

    constraint {
      @.line_offset >= 0 &&
      @.start_line >= 0 &&
      @.end_line >= @.start_line &&
      @.start_column >= 0 &&
      @.end_column >= @.start_column
    }
  }

  # Creates one or more registers of the chosen type at the top of the stack.
  #
  # Semantics: `count` registers of type `register-type` come into existence.
  # If `n` registers of that type existed before this instruction, the first is
  # allocated at index `n`, and the last at `n+count-1`. The new registers are
  # uninitialised, and must be initialised (provably by static analysis) before
  # they are read.
  #
  # The new registers continue to exist until a `pop` instruction destroys
  # them.
  #
  # There is a limit on the maximum number of registers of any given kind that
  # can be allocated in this manner.
  #
  # This instruction is not legal for V-pseudo-registers.
  elt push {
    register-type dilpf register-type
    int count
    constraint {@.count > 0}
    constraint {@.count < 32767}
  }

  # Pops the registers allocated by the corresponding `push`.
  #
  # Semantics: `count` registers of type `register-type` cease to exist. If
  # there were `n` registers before this instruction, the registers from
  # (n-count), inclusive, to (n), exclusive, are destroyed.
  #
  # It is an error if `count` is greater than the number of registers that
  # currently exist.
  #
  # This instruction is not legal for V-pseudo-registers.
  elt pop {
    register-type dilpf register-type
    int count
    constraint {@.count > 0}
    constraint {@.count < 32767}
  }

  # Loads an immediate string into a V- or D-register.
  #
  # Semantics: dst is set to the string src.
  elt ld-imm-vd {
    register vd dst {
      prop reg-write
    }
    str src
  }

  # Loads an immediate integer into an I-register.
  #
  # Semantics: dst is set to the value of integer src.
  elt ld-imm-i {
    register i dst {
      prop reg-write
    }
    int src
  }

  # Loads the value of a global into a V- or D-register.
  #
  # Semantics: The global indexed by src is read and its value stored in dst.
  #
  # The effect of this instruction is undefined if the global is located within
  # a module that is still initialising and which is not being initialised by
  # this thread of execution.
  elt ld-glob {
    register vd dst {
      prop reg-write
    }
    int src {
      prop global-var-ref
      prop global-ref
    }
  }

  # Copies the value of one register to another.
  #
  # Semantics: dst is set to the value stored in src. Both registers must be of
  # the same type, or must both be D- or V-registers.
  elt ld-reg-s {
    register dvifl dst {
      prop reg-write
    }
    register dvifl src {
      prop reg-read
    }

    constraint {
      @.dst.type == @.src.type ||
      ((@.dst.type == ava_prt_data || @.dst.type == ava_prt_var) &&
       (@.src.type == ava_prt_data || @.src.type == ava_prt_var))
    }
  }

  # Widens a typed register to an untyped register.
  #
  # Semantics: dst is set to the normal form of the value stored in src.
  elt ld-reg-u {
    register dv dst {
      prop reg-write
    }
    register ifl src {
      prop reg-read
    }
  }

  # Narrows an untyped register to a typed register.
  #
  # Semantics: dst is set to the typed value represented by the value stored in
  # src. If the value is not interpretable as dst's type, a format exception is
  # thrown.
  elt ld-reg-d {
    attr can-throw

    register ifl dst {
      prop reg-write
    }

    register dv src {
      prop reg-read
    }
  }

  # Loads a value into a P-register.
  #
  # Semantics: The given D- or V-register is read and its value stored into the
  # chosen P-register. If spread is true, the parameter is to be treated as a
  # spread parameter; otherwise, it is a singular dynamic parameter.
  elt ld-parm {
    register p dst {
      prop reg-write
    }
    register dv src {
      prop reg-read
    }
    bool spread
  }

  # Sets a global variable to the value of a local variable.
  #
  # Semantics: The global variable at the given index is set to the value stored
  # in src.
  #
  # The effect of this instruction is undefined if dst is located within a
  # module that has completed initialisation, or if it is executed on a thread
  # of execution other than the one initialising the module that contains it.
  elt set-glob {
    int dst {
      prop global-var-ref
      prop global-ref
    }
    register dv src {
      prop reg-read
    }
  }

  # Loads the empty list into an L-register.
  #
  # Semantics: The given L-register is set to the empty list.
  elt lempty {
    register l dst {
      prop reg-write
    }
  }

  # Appends an element to a list.
  #
  # Semantics: The L-register dst is set to the value of the L-register in lsrc
  # with the element read from esrc appended.
  elt lappend {
    attr can-throw ;# Eg, OOM, max list length exceeded
    register l dst {
      prop reg-write
    }
    register l lsrc {
      prop reg-read
    }
    register dv esrc {
      prop reg-read
    }
  }

  # Concatenates two lists.
  #
  # Semantics: The L-register dst is set to the concatenation of the lists in
  # the L-registers left and right, in that order.
  elt lcat {
    attr can-throw ;# Eg, OOM, max list length exceeded
    register l dst {
      prop reg-write
    }
    register l left {
      prop reg-read
    }
    register l right {
      prop reg-read
    }
  }

  # Extracts the first element of a list.
  #
  # Semantics: The D- or V-register dst is set to the first element in the
  # L-register src. If src is the empty list, an error_exception of type
  # empty-list is thrown.
  elt lhead {
    attr can-throw
    register dv dst {
      prop reg-write
    }
    register l src {
      prop reg-read
    }
  }

  # Deletes the first element of a list.
  #
  # Semantics: The L-register dst is set to the value of the L-register src,
  # with the first element deleted. If src is the empty list, an
  # error_exception of type empty-list is thrown.
  elt lbehead {
    attr can-throw
    register l dst {
      prop reg-write
    }
    register l src {
      prop reg-read
    }
  }

  # Flattens a list.
  #
  # Semantics: Every element in L-register src is interpreted as a list, and
  # all are concatenated into one list, which is stored in L-register dst.
  elt lflatten {
    attr can-throw
    register l dst {
      prop reg-write
    }
    register l src {
      prop reg-read
    }
  }

  # Accesses a single element in a list.
  #
  # Semantics: dst is set to the ixth element (from 0) in src. If ix is
  # negative or is greater than or equal to the length of src, an
  # error_exception whose user type and message is given by extype and
  # exmessage is thrown.
  elt lindex {
    attr can-throw
    register dv dst {
      prop reg-write
    }
    register l src {
      prop reg-read
    }
    register i ix {
      prop reg-read
    }
    str extype
    str exmessage
  }

  # Determines the length of a list.
  #
  # Semantics: dst is set to the number of elements contained by src.
  elt llength {
    register i dst {
      prop reg-write
    }
    register l src {
      prop reg-read
    }
  }

  # Adds a fixed value to an I-register.
  #
  # Semantics: dst is set to src+incr. The result of overflow is undefined.
  elt iadd-imm {
    register i dst {
      prop reg-write
    }
    register i src {
      prop reg-read
    }
    int incr
  }

  # Compares two I-registers.
  #
  # Semantics: If left==right, dst is set to 0. If left<right, dst is set to
  # -1. Otherwise, if left>right, dst is set to +1.
  elt icmp {
    register i dst {
      prop reg-write
    }
    register i left {
      prop reg-read
    }
    register i right {
      prop reg-write
    }
  }

  # Normalises a boolean integer to be 0 or 1.
  #
  # Semantics: The src I-register is read. If it is 0, 0 is written to the dst
  # I-register. Otherwise, 1 is written to the dst I-register.
  elt bool {
    register i dst {
      prop reg-write
    }
    register i src {
      prop reg-read
    }
  }

  # Asserts that a value (an argument, presumably) is the empty string.
  #
  # Semantics: The given V-register is read. If it is the empty string, this
  # instruction has no effect. If it is not the empty string, behaviour is
  # undefined.
  #
  # This is normally only used on arguments, and is used to assert that ()
  # arguments are indeed empty. Implementations which throw exceptions in
  # response to the assertion failure should indicate that.
  elt aaempty {
    attr can-throw
    register v src {
      prop reg-read
    }
  }

  # Invokes a statically-known function with statically-bound arguments.
  #
  # Semantics: The given function is invoked with an array of D-registers
  # beginning at d$base, inclusive, and ending at d($base+$nargs), exclusive.
  # If the function returns normally, its return value is stored in dst, which
  # is a D- or V-register.
  #
  # If marshalling the function's arguments or return value fails, the
  # exception propagates.
  #
  # This instruction uninitialises the D-registers passed as arguments.
  #
  # The number of arguments given must exactly match the number of arguments
  # taken by the function.
  elt invoke-ss {
    attr special-reg-read-d
    attr can-throw

    register dv dst {
      prop reg-write
    }
    int fun {
      prop global-fun-ref
      prop global-ref
    }
    int base {
      prop reg-read-base
    }
    int nargs {
      prop reg-read-count
      prop static-arg-count
    }

    constraint {
      @.base >= 0 && @.nargs > 0
    }
  }

  # Invokes a statically-known function with dynamically-bound arguments.
  #
  # Semantics: The given function is invoked, passing the parameters stored in
  # the P-registers starting at p$base, inclusive, and ending at
  # p($base+$nparms), exclusive. The parameters are bound to arguments at
  # runtime.
  #
  # If binding the parameters fails, an ava_error_exception with the type name
  # "bad-arguments" is thrown.
  #
  # Regardless of whether the statement succeeds, the input P-registers are
  # uninitialised by this instruction.
  #
  # Any exceptions resulting from marshalling the function call propagate.
  elt invoke-sd {
    attr special-reg-read-p
    attr can-throw

    register dv dst {
      prop reg-write
    }
    int fun {
      prop global-fun-ref
      prop global-ref
    }

    int base {
      prop reg-read-base
    }
    int nparms {
      prop reg-read-count
    }

    constraint {
      @.base >= 0 && @.nparms > 0
    }
  }

  # Invokes a dynamically-known function with dynamically-bound arguments.
  #
  # Semantics: The function in `function` is invoked, passing the parameters
  # stored in the P-registers starting at p$base, inclusive, and ending at
  # p($base+$nparms), exclusive. The parameters are bound to arguments at
  # runtime.
  #
  # If binding the parameters fails, an ava_error_exception with the type name
  # "bad-arguments" is thrown.
  #
  # Regardless of whether the statement succeeds, the input P-registers are
  # uninitialised by this instruction.
  #
  # Any exceptions resulting from marshalling the function call propagate.
  elt invoke-dd {
    attr special-reg-read-p
    attr can-throw

    register dv dst {
      prop reg-write
    }
    register f fun {
      prop reg-read
    }

    int base {
      prop reg-read-base
    }
    int nparms {
      prop reg-read-count
    }

    constraint {
      @.base >= 0 && @.nparms > 0
    }
  }

  # Performs partial application of a function.
  #
  # Semantics: Starting from the first non-implicit argument in the function in
  # src, each non-implicit argument in the function is changed to an implicit
  # argument whose value is read from successive D-registers starting at
  # d$base. $nargs arguments are rebound this way. The result is written into
  # dst.
  #
  # The behaviour of this function is undefined if the function in src does not
  # have at least $nargs non-implicit arguments.
  #
  # This instruction is intended for the construction of closures. It is
  # important to note that it ignores the actual binding type of arguments it
  # rebinds, and thus does not behave the way one might intend if it binds over
  # non-pos arguments.
  elt partial {
    attr special-reg-read-d
    attr can-throw

    register f dst {
      prop reg-write
    }
    register f src {
      prop reg-read
    }

    int base {
      prop reg-read-base
    }
    int nargs {
      prop reg-read-count
    }

    constraint {
      @.base >= 0 && @.nargs > 0
    }
  }

  # Returns from the containing function.
  #
  # Semantics: The current function immediately terminates execution, producing
  # the chosen register's value as its return value.
  elt ret {
    attr terminal
    attr terminal-no-fallthrough
    attr require-empty-exception
    register dv return-value {
      prop reg-read
    }
  }

  # Branches to another location within the function.
  #
  # Semantics: The key I-register is read. If it is equal to value and invert
  # is false, or is not equal to value and invert is true, control jumps to the
  # label whose name matches target. Otherwise, control continues to the next
  # instruction.
  #
  # The P-Code is considered invalid if target references a nonexistent label.
  elt branch {
    attr terminal
    register i key {
      prop reg-read
    }
    int value
    bool invert
    int target {
      prop jump-target
    }
  }

  # Unconditionally branches to another location within the function.
  #
  # Semantics: Control jumps to the label whes name matches the given target.
  #
  # The P-Code is considered invalid if target references a nonexistent label.
  elt goto {
    attr terminal
    attr terminal-no-fallthrough
    int target {
      prop jump-target
    }
  }

  # Labels a point in the instruction sequence.
  #
  # Semantics: This instruction has no effect, but serves as a jump target for
  # the branch instruction.
  elt label {
    int name
  }

  # The primary instruction for exception handling.
  #
  # Semantics: An exception handler is pushed onto the logical exception
  # handler stack. Control proceeds to the next instruction. If any exception
  # is thrown while this handler is at the top of the stack, the stack is
  # unwound to this function frame, the exception handler is popped from the
  # exception-handler stack, the caught exception pushed onto the
  # caught-exception stack, and control transfers to the label identified by
  # target (as per goto).
  #
  # Note that the exception-handler and caught-exception stacks are conceptual
  # only. All static paths through the function must have the same stack
  # configurations for every instruction. The identity of the try instruction
  # that pushed each element is considered part of the configuration for this
  # purpose.
  #
  # The exception-handler and caught-exception stacks share the concept of
  # ordering, in that it is meaningful to refer to the single top of the
  # collective exception stacks.
  #
  # The exception stacks must be empty at any point where the function might
  # return.
  #
  # If cleanup is true, exceptions of any kind (including foreign exceptions)
  # will be caught, and all exceptions will be treated as foreign (eg, ex-type
  # will return ava_pet_other_exception and ex-value will be empty). Otherwise
  # it is undefined whether foreign exceptions will be caught. Code generators
  # should ensure that foreign exceptions are expediently rethrown if
  # encountered.
  elt try {
    attr terminal
    attr push-landing-pad
    bool cleanup {
      prop landing-pad-is-cleanup
    }
    int target {
      prop landing-pad
    }
  }

  # Extracts the type of the current exception.
  #
  # Semantics: dst is set to the integer representing the type of the top-most
  # caught-exception.
  elt ex-type {
    attr require-caught-exception
    register i dst {
      prop reg-write
    }
  }

  # Extracts the value of the current exception.
  #
  # Semantics: dst is set to the value of the top-most caught-exception.
  elt ex-value {
    attr require-caught-exception
    register dv dst {
      prop reg-write
    }
  }

  # Ends an exception-handling context.
  #
  # Semantics: The top of the combined exception stacks, whichever type it is,
  # is dropped. This instruction otherwise does nothing.
  #
  # The effect of reaching an yrt instruction after catching a foreign
  # exception is undefined; the exception may be dropped or rethrown. Code
  # generators should ensure that foreign exceptions always get rethrown.
  elt yrt {
    # Terminal so that every basic block has at most one exception handling
    # context.
    attr terminal
    attr pop-exception
  }

  # Throws a new exception.
  #
  # Semantics: An exception whose type is identified by ex-type and whose value
  # is identified by ex-value is thrown from the current location, as per
  # ava_throw().
  elt throw {
    attr terminal
    attr terminal-no-fallthrough
    attr can-throw

    int ex-type
    register dv ex-value {
      prop reg-read
    }

    constraint {
      ava_pcode_is_valid_ex_type(@.ex_type)
    }
  }

  # Rethrows a caught exception.
  #
  # Semantics: The exception at the top of the caught-exception is thrown, as
  # per ava_rethrow().
  elt rethrow {
    attr terminal
    attr terminal-no-fallthrough
    attr can-throw
    attr require-caught-exception
  }
}

# User macros are implemented by a syntax-unit-manipulating virtual machine of
# sorts.
#
# Unlike executable P-Code, the macro VM is a simple stack machine, where every
# value is an ava_parse_unit or ava_parse_statement. The VM has no control flow
# instructions; it simply executes every instruction sequentally and
# unconditionally. When the VM terminates, the stack must contain a single
# ava_parse_statement.
#
# The VM starts with a stack containing an empty ava_parse_statement. Input is
# provided in the form of the `left` and `right` instructions.
struct macro m {
  parent global g

  # Sets the human-readable context, used in diagnostics
  elt context {
    str value
  }

  # Pushes the left input of the macro onto the stack.
  #
  #   ( ) -- ( s )
  #
  # The parse units to the left of the provoker are cloned into a statement
  # which is pushed onto the stack.
  elt left { }
  # Pushes the right input of the macro onto the stack.
  #
  #   ( ) -- ( s )
  #
  # The parse units to the right of the provoker are cloned into a statement
  # which is pushed onto the stack.
  elt right { }
  # Truncates a statement to a fixed number of elements.
  #
  #   ( s ) -- ( s )
  #
  # The statement at the top of the stack has all but the first count elements
  # deleted. An error occurs if the statement does not have that many elements.
  elt head {
    int count
    constraint { @.count >= 0 }
  }
  # Removes a fixed number of elements from a statement.
  #
  #   ( s ) -- ( s )
  #
  # The first count units from the statement at the top of the stack are
  # deleted. An error occurs if the statement does not have that many elements.
  elt behead {
    int count
    constraint { @.count >= 0 }
  }
  # Removes initial elements from a statement so that it has the given length.
  #
  #   ( s ) -- ( s )
  #
  # All but the last count units are removed from the statement at the top of
  # the stack. An error occurs if the statement does not contain at least count
  # units.
  elt tail {
    int count
    constraint { @.count >= 0 }
  }
  # Removes a fixed number of trailing elements from a statement.
  #
  #   ( s ) -- ( s )
  #
  # The last count units are removed from the statement at the top of the
  # stack. An error occurs if the statement does not contain at least count
  # units.
  elt curtail {
    int count
    constraint { @.count >= 0 }
  }
  # Ensures a statement is non-empty.
  #
  #   ( s ) -- ( s )
  #
  # An error occurs if the statement at the top of the stack has no units.
  # Otherwise, this instruction has no effect.
  elt nonempty { }
  # Extracts a singular unit from a statement.
  #
  #   ( s ) -- ( u )
  #
  # A statement is popped from the stack. If it does not contain exactly one
  # parse unit, an error occurs. Otherwise, the only parse unit it contains is
  # pushed onto the stack.
  elt singular { }
  # Adds an element to a container.
  #
  #   ( s s ) -- ( s )
  #   ( s u ) -- ( s )
  #   ( u u ) -- ( u )
  #   ( u s ) -- ( u )
  #
  # Two values are popped off the stack. The second must be a suitable
  # container for the former top-of-stack. The top-of-stack is added to the end
  # of the second, which is then pushed onto the stack.
  #
  # Appending a statement to another statement or semiliteral concatenates the
  # two. (Appending a semiliteral to a statement adds the semiliteral to the
  # statement.)
  elt append { }

  # Pushes a unique bareword onto the stack.
  #
  #   ( ) -- ( u )
  #
  # During any macro VM execution, equivalent gensym instructions produce
  # equivalent barewords which are guaranteed to be distinct from any other
  # gensym in the same invocation, and to never collide with any other gensym
  # or normal identifier globally.
  elt gensym {
    str value
  }

  # Pushes a bareword onto the stack.
  #
  #   ( ) -- ( u )
  elt bareword {
    str value
  }
  # Pushes an A-String onto the stack.
  #
  #   ( ) -- ( u )
  elt astring {
    str value
  }
  # Pushes an L-String onto the stack.
  #
  #   ( ) -- ( u )
  elt lstring {
    str value
  }
  # Pushes an R-String onto the stack.
  #
  #   ( ) -- ( u )
  elt rstring {
    str value
  }
  # Pushes an LR-String onto the stack.
  #
  #   ( ) -- ( u )
  elt lrstring {
    str value
  }
  # Pushes a Verbatim onto the stack.
  #
  #   ( ) -- ( u )
  elt verbatim {
    str value
  }
  # Pushes an empty substitution onto the stack.
  #
  #   ( ) -- ( u )
  elt subst { }
  # Pushes an empty semiliteral onto the stack.
  #
  #   ( ) -- ( u )
  elt semilit { }
  # Pushes an empty block onto the stack.
  #
  #   ( ) -- ( u )
  elt block { }
  # Pushes an empty statement onto the stack.
  #
  #   ( ) -- ( s )
  elt statement { }
  # Wraps the unit at the top of the stack in a Spread.
  #
  #   ( u ) -- ( u )
  elt spread { }

  # Causes the attempt at macro expansion to be abandoned and an error node to
  # be produced.
  #
  # This should not actually be found in P-Code files, but rather is used to
  # effect a placeholder while the file containing the bad macro is processed.
  elt die { }
}
