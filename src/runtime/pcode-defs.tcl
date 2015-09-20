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
  attr var
  attr fun
  prop int global-ref
  prop int global-fun-ref

  # Records the source filename in effect until the next declaration indicating
  # otherwise.
  elt src-file {
    str filename
  }

  # Records the source line number in effect until the next declaration
  # indicating otherwise.
  elt src-line {
    int line
  }

  # Raw C code block.
  #
  # Defines C code to be inserted when transpiling to C. If visibility is
  # public, the code is inserted into all modules that directly load this
  # P-Code; if private, the code is only inserted into the transpilation of the
  # containing P-Code itself. There is no internal visiblity for this type. If
  # mandatory is true, execution of the P-Code is considered impossible if the
  # code cannot be meaningfully inserted, making interpretation impossible.
  #
  # Naturally, use of this declaration type is inherently unsafe, since it is
  # impossible to reason about what it may do.
  elt c {
    # Whether this C block is mandatory for the P-Code to be valid.
    bool mandatory
    # Whether all client code of this P-Code has this block inserted.
    bool is-public
    # The raw C code to insert.
    #
    # No checks are performed on the contents of this code.
    str text
  }

  # Declares a global variable defined by a different P-Code unit.
  #
  # Semantics: At any point before this P-Code unit's initialisation
  # function(s) are run, the name of this variable will be resolved to the
  # referenced global symbol. If resolution fails, this unit fails to load.
  # This resolution may occur before the program is executed.
  elt ext-var {
    attr var
    demangled-name name
  }

  # Declares a global function defined by a different P-Code unit.
  #
  # Semantics: Resolution of the name occurs as with ext-var. The resolved
  # symbol is assumed to be a function with the given prototype.
  elt ext-fun {
    attr var
    attr fun
    demangled-name name
    function prototype
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
    bool publish
    demangled-name name
  }

  # Declares a global function defined by this P-Code unit.
  #
  # Semantics: All notes from `var` apply, except that it is initialised to
  # a function object which can be executed to execute body.
  elt fun {
    attr var
    attr fun
    bool publish
    demangled-name name
    function prototype
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
  }

  # Exports a symbol visible to Avalanche code referencing a global in this
  # P-Code unit.
  elt export {
    # The global which is being exported.
    int global {
      prop global-ref
    }
    # If true, any P-Code unit created as a composite including this one
    # (ie, linkage of modules into a package) includes this export statement. A
    # value of true corresponds to public visibility; false to internal
    # visibility.
    bool reexport
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
    bool reexport
    # The name by which this macro is exposed to Avalanche code.
    str name
    # The body of this macro.
    struct macro m body
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
    int fun {
      prop global-fun-ref
    }
  }
}

# A single executable instruction.
struct exe x {
  parent global g

  prop register reg-read
  prop register reg-write
  prop str jump-target
  prop int global-ref
  prop int global-fun-ref

  # Records the source filename in effect until the next declaration indicating
  # otherwise.
  #
  # This is independent of the global src-file.
  elt src-file {
    str filename
  }

  # Records the source line number in effect until the next declaration
  # indicating otherwise.
  #
  # This is independent of the global src-line.
  elt src-line {
    int line
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
      prop global-ref
    }
  }

  # Copies the value of one register to another.
  #
  # Semantics: dst is set to the value stored in src, possibly after
  # conversion. The following table lists legal dst/src type pairs where dst
  # and src are different. D- and V-registers are considered equivalent here
  # and are not noted separately.
  #
  #   dv->i      Parsed as integer, default 0
  #   i->dv      Integer stringified
  #   dv->f      Parsed as ava_function
  #   f->dv      Function stringified
  #   dv->l      Parsed as list
  #   l->dv      List stringified
  #
  # Exceptions may be thrown by parsing conversions.
  #
  # Note that conversions between types means that sequences like
  #   ld-reg i0 d0
  #   ld-reg d0 i0
  # result in changing *both* registers.
  #
  # This instruction cannot operate on P-registers at all, see ld-parm for
  # that.
  elt ld-reg {
    register dvifl dst {
      prop reg-write
    }
    register dvifl src {
      prop reg-read
    }

    constraint {
      @.dst.type == @.src.type ||
      @.dst.type == ava_prt_var ||
      @.dst.type == ava_prt_data ||
      @.src.type == ava_prt_var ||
      @.src.type == ava_prt_data
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
    register l dst {
      prop reg-write
    }
    register l src {
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
    register dv dst {
      prop reg-write
    }
    int fun {
      prop global-ref
      prop global-fun-ref
    }
    int base
    int nargs

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
    register dv dst {
      prop reg-write
    }
    int fun {
      prop global-ref
      prop global-fun-ref
    }

    int base
    int nparms

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
    register dv dst {
      prop reg-write
    }
    register f fun {
      prop reg-read
    }

    int base
    int nparms

    constraint {
      @.base >= 0 && @.nparms > 0
    }
  }

  # Performs partial application of a function.
  #
  # Semantics: Starting from the first non-implicit argument in the function in
  # src, each argument in the function is changed to an implicit argument whose
  # value is read from successive D-registers starting at d$base. $nargs
  # arguments are rebound this way. The result is written into dst.
  #
  # The behaviour of this function is undefined if the function in src does not
  # have at least $nargs arguments following the first non-implicit argument,
  # or if it has no non-implicit arguments at all.
  #
  # This instruction is intended for the construction of closures. It is
  # important to note that it ignores the actual binding type of arguments it
  # rebinds, and thus does not behave the way one might intend if it binds over
  # non-pos arguments.
  elt partial {
    register f dst {
      prop reg-write
    }
    register f src {
      prop reg-write
    }

    int base
    int nargs

    constraint {
      @.base >= 0 && @.nargs > 0
    }
  }

  # Returns from the containing function.
  #
  # Semantics: The current function immediately terminates execution, producing
  # the chosen register's value as its return value.
  elt ret {
    register dv return-value {
      prop reg-read
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

  # Branches to another location within the function.
  #
  # Semantics: The key I-register is read. If it is equal to value, control
  # jumps to the label whose name matches target. Otherwise, control continues
  # to the next instruction.
  #
  # The P-Code is considered invalid if target references a nonexistent label.
  elt branch {
    register i key {
      prop reg-read
    }
    int value
    int target
  }

  # Labels a point in the instruction sequence.
  #
  # Semantics: This instruction has no effect, but serves as a jump target for
  # the branch instruction.
  elt label {
    int name
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
