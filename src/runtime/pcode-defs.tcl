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
  attr conditional-jump
  attr unconditional-jump

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
  # Semantics: `count` registers of type `type` come into existence. If `n`
  # registers of that type existed before this instruction, the first is
  # allocated at index `n`, and the last at `n+count-1`. The new registers are
  # uninitialised, and must be initialised (provably by static analysis) before
  # they are read.
  #
  # The new registers continue to exist until the balancing `pop` instruction,
  # which is lexical rather than execution-driven. It is an error for there to
  # be no matching `pop` instruction.
  #
  # There is a limit on the maximum number of registers of any given kind that
  # can be allocated in this manner.
  #
  # This instruction is not legal for V-pseudo-registers.
  #
  # It is an error if a `push` has not been balanced by a `pop` by the end of
  # the containing function.
  elt push {
    register-type dilpf register-type
    int count
    constraint {@.count > 0}
    constraint {@.count < 32767}
  }

  # Pops the registers allocated by the corresponding `push`.
  #
  # This instruction is simply a marker of where a `push` ends. It is an error
  # if there is no matching `push`.
  elt pop {
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
  # This instruction cannot operate on P-registers at all.
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
  # the given P-register. The parameters are bound to arguments at runtime.
  #
  # If binding the parameters fails, an ava_error_exception with the type name
  # "bad-arguments" is thrown.
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
    register p parms {
      prop reg-read
    }
  }

  # Invokes a dynamically-known function with dynamically-bound arguments.
  #
  # Semantics: The function in `function` is invoked, passing the parameters
  # stored in the given P-register.
  #
  # If binding the parameters fails, an ava_error_exception with the type name
  # "bad-arguments" is thrown.
  #
  # Any exceptions resulting from marshalling the function call propagate.
  elt invoke-dd {
    register dv dst {
      prop reg-write
    }
    register f fun {
      prop reg-read
    }
    register p parms {
      prop reg-read
    }
  }

  # Returns from the containing function.
  #
  # Semantics: The current function immediately terminates execution, producing
  # the chosen register's value as its return value.
  elt ret {
    attr unconditional-jump
    register dv return-value {
      prop reg-read
    }
  }

  # Jumps to a label within the current function.
  #
  # Semantics: Control transfers to the statement following the label whose
  # name is specified by target. The label must be found within the function
  # containing this statement.
  elt goto {
    attr unconditional-jump
    str target {
      prop jump-target
    }
  }

  # Conditionally jumps to a label within the current function.
  #
  # Semantics: If the value in `condition` is non-zero, this statement behaves
  # like `goto`. Otherwise, it does nothing.
  elt goto-c {
    attr conditional-jump
    register i condition
    str target {
      prop jump-target
    }
  }

  # Labels a location within a function.
  #
  # Semantics: This statement has no effect, but may be used as a jump target
  # by other statements within the same subroutine.
  elt label {
    str name
  }
}