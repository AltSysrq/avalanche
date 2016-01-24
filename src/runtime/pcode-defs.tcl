#-
# Copyright (c) 2015, 2016 Jason Lingle
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
  # Whether this element is a valid target for global variable references which
  # mutate the variable directly.
  attr var-mutable
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
  # Property which is a reference, by absolute index, to an element with the
  # struct-def property in the same P-Code object.
  prop int global-sxt-ref
  # Property used in conjunction with global-sxt-ref to indicate that the
  # referenced struct must have a tail field.
  prop int global-sxt-with-tail-ref
  # Property indicating that the element describes a function with the given
  # prototype.
  #
  # Always singular.
  prop function prototype
  # Property indicating that the element declares or defines a struct with the
  # given struct definition.
  #
  # Always singular.
  prop sxt struct-def
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
  # Property on elements which may participate in linking, indicating the name
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
    attr var-mutable
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
  }

  # Declares the existence of a struct which can be used with native-interop
  # instructions.
  #
  # Struct declarations do not participate in linkage at the P-Code level.
  # Whether any such linkage occurs at platform level is entirely
  # platform-dependent.
  #
  # In a fully-assembled program, there should be exactly one decl-sxt which
  # has define set to true (including any parent or composed structures
  # implicitly declared by the definition in this instruction). The effect of
  # failing to meet this requirement is undefined. Similarly, the effect of
  # having multiple decl-sxts defining structures with the same name (including
  # implicitly via extension or composition) but different contents is
  # undefined.
  #
  # Despite the "undefined" statemements above, a program does not become
  # unsafe simply by virtue of having a decl-sxt instruction; the worst that
  # may happen is that the final result fail to compile or link. However,
  # should linking succeed, the behaviour of the unsafe exe instructions
  # becomes far less well-defined.
  elt decl-sxt {
    attr entity

    # Whether the back-end should emit any code that may be necessary to
    # concretely "define" the struct on the platform. Note that this is not
    # recursive; structs implicitly declared by extension or composition are
    # not affected by this flag, only the top-level struct in def itself.
    bool define
    # The ava_struct describing this structure.
    sxt def {
      prop struct-def
    }
  }

  # Defines a static strangelet.
  #
  # This is considered unsafe because it causes any ld-glob instructions to
  # produce strangelets, which may themselves result in unsafe operations.
  #
  # Semantics: This element behaves like a read-only variable which contains a
  # strangelet referencing an instance of sxt. The reference is set, and the
  # memory zero-initialised, at some time before the first read of the variable
  # occurs.
  #
  # If sxt has a tail field, its size is zero.
  #
  # If thr-local is true, every *platform* thread sees a different strangelet.
  #
  # On read, the strangelet type is SXT.
  elt S-bss {
    attr entity
    attr var
    attr linkage-definition

    int sxt {
      prop global-entity-ref
      prop global-sxt-ref
    }
    bool publish {
      prop publish
    }
    demangled-name name {
      prop linkage-name
    }
    bool thr-local
  }

  # Like S-bss, but the strangelet references an array of length length
  # instances of sxt.
  #
  # On read, the strangelet type is SXT[].
  elt S-bss-a {
    attr entity
    attr var
    attr linkage-definition

    int sxt {
      prop global-entity-ref
      prop global-sxt-ref
    }
    bool publish {
      prop publish
    }
    demangled-name name {
      prop linkage-name
    }
    bool thr-local
    int length

    constraint {
      @.length >= 0
    }
  }

  # Like S-bss, but the length of the tail field on sxt is given by length.
  elt S-bss-t {
    attr entity
    attr var
    attr linkage-definition

    int sxt {
      prop global-entity-ref
      prop global-sxt-ref
      prop global-sxt-with-tail-ref
    }
    bool publish {
      prop publish
    }
    demangled-name name {
      prop linkage-name
    }
    bool thr-local
    int length

    constraint {
      @.length >= 0
    }
  }

  # Declares a static strangelet defined by a different P-Code unit.
  #
  # Semantics: At any point before this P-Code unit's initialisation
  # function(s) are run, the name of this variable will be resolved to a
  # published S-bss, S-bss-a, or S-bss-t symbol. If resolvution fails, this
  # unit fails to load. This resolution may occur before the program is loaded.
  #
  # As a variable, this behaves the same as S-bss. Note that the S-ext-bss
  # itself is typeless.
  #
  # Behaviour is undefined if thr-local on the S-ext-bss is different from the
  # thr-local on the S-bss it references.
  elt S-ext-bss {
    attr var
    attr entity
    attr effectively-published
    demangled-name name {
      prop linkage-name
    }
    bool thr-local
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
      ava_st_function_macro == @.type ||
      ava_st_expander_macro == @.type
    }
    constraint {
      (ava_st_operator_macro == @.type &&
       (@.precedence >= 1 &&
        @.precedence <= AVA_MAX_OPERATOR_MACRO_PRECEDENCE)) ||
      (ava_st_operator_macro != type &&
       @.precedence == 0)
    }
  }

  # Defines a keysym symbol exported by this P-Code unit.
  elt keysym {
    # The name of the symbol as exposed to Avalanche code.
    str name
    # The actual identity of this keysym.
    str id
    # Whether this keysym should be reexported to composite P-Code units.
    bool reexport {
      prop reexport
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
  # Identifies a field which must refer to a global entity with the var-mutable
  # attribute.
  prop int global-var-mutable-ref
  # Identifies a field which must refer to a function-like global entity.
  prop int global-fun-ref
  # Identifies a field which must refer to a struct-like global entity.
  prop int global-sxt-ref
  # Identifies a field which must refer to a struct-like global entity which
  # has a tail field.
  prop int global-sxt-with-tail-ref
  # Identifies a field which must refer to some global entity. This must be
  # given in addition to global-var-ref, global-fun-ref, etc.
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
  # Indicates a field which refers by index to a field within the struct
  # referenced by the instruction's global-sxt-ref. The field must be an int.
  prop int sxt-field-ref-int
  # Indicates a field which refers by index to a field within the struct
  # referenced by the instruction's global-sxt-ref. The field must be a real.
  prop int sxt-field-ref-real
  # Indicates a field which refers by index to a field within the struct
  # referenced by the instruction's global-sxt-ref. The field must be a value.
  prop int sxt-field-ref-value
  # Indicates a field which refers by index to a field within the struct
  # referenced by the instruction's global-sxt-ref. The field must be a ptr or
  # hybrid.
  prop int sxt-field-ref-ptr-hybrid
  # Indicates a field which refers by index to a field within the struct
  # referenced by the instruction's global-sxt-ref. The field must be a hybrid.
  prop int sxt-field-ref-hybrid
  # Indicates a field which refers by index to a field within the struct
  # referenced by the instruction's global-sxt-ref. The field must be a
  # composition (array, compose, or tail).
  prop int sxt-field-ref-composite
  # Indicates a field which refers by index to a field within the struct
  # referenced by the instruction's global-sxt-ref. The field must be an atomic
  # integer.
  prop int sxt-field-ref-atomic-int
  # Indicates a field which refers by index to a field within the struct
  # referenced by the instruction's global-sxt-ref. The field must be an atomic
  # ptr.
  prop int sxt-field-ref-atomic-ptr

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
      prop global-var-mutable-ref
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

  # Hints to the architecture that the code is busy-waiting on a condition.
  #
  # This instruction has no observable effect.
  #
  # Its semantics are those of the `pause` instruction on AMD64. It should be
  # used in spinlock-like constructs to prevent the CPU from wasting time and
  # energy speculating on a lock failure.
  #
  # An example of cpu-pause to implement a primitive spinlock follows. Assume
  # struct 42 has a single atomic integer field and that a strangelet
  # referencing it is in v0.
  #
  #   [push i 4]
  #   [ld-imm-i i2 0]
  #   [ld-imm-i i3 1]
  #
  #   ; Try to take lock
  #   [label 1]
  #   [S-ia-cas i0 i1 v0 42 0 i2 i3 true false seqcst monotonic]
  #   [branch i0 1 false 3]
  #
  #   ; Failed, spin for it to become free
  #   [label 2]
  #   [cpu-pause]
  #   [S-ia-ld i1 v0 42 0 true monotonic]
  #   [branch i1 0 true 2]
  #
  #   ; Free, try to take again
  #   [goto 1]
  #
  #   ; Success
  #   [label 2]
  #   [pop i 4]
  elt cpu-pause {
  }

  # Instructions beginning with "S" operate on strangelets and are inherently
  # wildly unsafe.
  #
  # Since the backend platform may enforce strong typing, a simple notation is
  # used to describe the behaviour of types on such systems. Note that this
  # type system does not actually exist at the P-Code level or on backends that
  # do not target a strongly-typed system.
  #
  # The notation `A <- B` means "A assignable from B". A and B may name types,
  # struct references, or registers referencing strangelets.
  #
  # The notation `T[]` means an array containing elements of type T.

  proc S-sxt {{extra {}}} {
    int sxt "
      prop global-ref
      prop global-sxt-ref
      $extra
    "
  }

  # Allocates a structure scoped to the current function call.
  #
  # Semantics: dst is set to a strangelet pointing to a single unique instance
  # of the structure referenced by sxt. If sxt has a tail field, its effective
  # length is zero. If zero-init is true, the data is zero-initialised,
  # otherwise its initial contents are undefined.
  #
  # Pointers within the structure are visible to the garbage collector. The
  # strangelet ceases to be valid when the containing function returns, or if
  # an S-set-sp is used to restore the stack to a position before this
  # instruction executes.
  #
  # Behaviour is undefined if there is insufficient space for the new
  # structure.
  #
  # Types:
  #   dst <- SXT
  elt S-new-s {
    register dv dst {
      prop reg-write
    }
    S-sxt
    bool zero-init
  }

  # Like S-new-s, but an array of $length instances is allocated instead.
  #
  # Behaviour is undefined if $length is negative.
  #
  # Types:
  #   dst <- SXT[]
  elt S-new-sa {
    register dv dst {
      prop reg-write
    }
    S-sxt
    register i length {
      prop reg-read
    }
    bool zero-init
  }

  # Like S-new-s, but the length of the tail field is specified in
  # $tail-length.
  #
  # Behaviour is undefined if $tail-length is negative.
  #
  # Types:
  #   dst <- SXT
  elt S-new-st {
    register dv dst {
      prop reg-write
    }
    S-sxt {
      prop global-sxt-with-tail-ref
    }
    register i tail-length {
      prop reg-read
    }
    bool zero-init
  }

  # Allocates a structure with dynamic lifetime.
  #
  # Semantics: dst is set to a strangelet pointing to a single unique instance
  # of the structure referenced by sxt. If sxt has a tail field, its effective
  # length is zero. If zero-init is true, the data is zero-initialised,
  # otherwise its initial contents are undefined.
  #
  # Pointers within the structure are visible to the garbage collector if the
  # atomic field is not true. Whether atomic allocations retain pointers is
  # undefined.
  #
  # If precise is true, pointers to composed members in sxt or to any other
  # location that is not the exact pointer produced by this allocation are not
  # guaranteed to retain the allocation. Otherwise, any pointer into the
  # allocated memory will retain the memory. A pointer to one byte past the end
  # is *not* guaranteed to retain the memory.
  #
  # Types:
  #   dst <- SXT
  elt S-new-h {
    attr can-throw

    register dv dst {
      prop reg-write
    }
    S-sxt
    bool zero-init
    bool atomic
    bool precise
  }

  # Like S-new-h, but an array of $length instances is allocated instead.
  #
  # Behaviour is undefined if $length is negative.
  #
  # Types:
  #   dst <- SXT[]
  elt S-new-ha {
    attr can-throw

    register dv dst {
      prop reg-write
    }
    S-sxt
    register i length {
      prop reg-read
    }
    bool zero-init
    bool atomic
    bool precise
  }

  # Like S-new-h, but the length of the tail field (if any) is specified in
  # $tail-length.
  #
  # Behaviour is undefined if $tail-length is negative.
  #
  # Types:
  #   dst <- SXT
  elt S-new-ht {
    attr can-throw

    register dv dst {
      prop reg-write
    }
    S-sxt {
      prop global-sxt-with-tail-ref
    }
    register i tail-length {
      prop reg-read
    }
    bool zero-init
    bool atomic
    bool precise
  }

  # Determines the size of a structure.
  #
  # Semantics: dst is set to the allocation size of the given structure. If sxt
  # has a tail field, the size reflects the size of the structure with a tail
  # length of zero.
  #
  # This instruction is intended for interoperation with C APIs that require
  # passing the size of user structs in and other low-level operations on
  # memory-oriented systems. The instruction is not defined for backends
  # which cannot ascribe a byte size to a structure.
  elt S-sizeof {
    register i dst {
      prop reg-write
    }
    S-sxt {
      prop global-sxt-ref
    }
  }

  # Determines the ABI alignment of a structure.
  #
  # Semantics: dst is set to the ABI alignment of the given structure.
  #
  # This instruction is intended for low-level operations on memory-oriented
  # systems. It is not defined for backends which do not have a concept of
  # memory alignment.
  elt S-alignof {
    register i dst {
      prop reg-write
    }
    S-sxt {
      prop global-sxt-ref
    }
  }

  # Gets the current stack-pointer.
  #
  # Semantics: dst is set to a strangelet that in some unspecified manner
  # represents the current stack pointer, such that S-set-sp can restore the
  # stack to that location.
  #
  # Platforms with no notion of a stack pointer need not provide any meaningful
  # value in dst.
  #
  # Types:
  #   dst <- platform-dependentent-stack-pointer-type
  elt S-get-sp {
    register dv dst {
      prop reg-write
    }
  }

  # Resets the current stack-pointer.
  #
  # Semantics: src is assumed to be a strangelet containing a valid stack
  # pointer produced by a call to S-get-sp in the current function. The stack
  # pointer is reset to that value, invalidating any function-scoped
  # allocations and any stack-pointer references that occurred between the
  # S-get-sp that produced the value in src and this instruction.
  #
  # Behaviour is undefined if src was not produced by an S-get-sp instruction,
  # was not produced in the current function invocation, or if another S-set-sp
  # instruction has since invalidated the reference.
  #
  # On platforms with no notion of a stack pointer, this may simply be a no-op.
  # Such platforms must ensure that function-scoped memory can be automatically
  # reclaimed before the function returns, however.
  #
  # Types:
  #   platform-dependentent-stack-pointer-type <- src
  elt S-set-sp {
    register dv src {
      prop reg-read
    }
  }

  # Copies one instance of a structure to another.
  #
  # Semantics: dst and src are both assumed to be strangelets referencing an
  # instance of sxt. The immediate values in src are copied atop those in dst,
  # such that the two structures are fully independent copies at the end.
  #
  # If sxt has a tail field, its contents in dst are not affected.
  #
  # If preserve-src is false, the value referenced by src is destroyed by the
  # operation, and any further attempt to use that memory has undefined
  # behaviour.
  #
  # Behaviour is undefined if dst and src refer to the same memory.
  #
  # Types:
  #   SXT <- dst
  #   SXT <- src
  elt S-cpy {
    register dv dst {
      prop reg-read ;# mutable destination
    }
    register dv src {
      prop reg-read
    }
    S-sxt
    bool preserve-src
  }

  # Copies an array slice of structure instances to another.
  #
  # Semantics: Each element in dst, from dst-off, inclusive, to
  # (dst-off+count), exclusive, is copied from corresponding elements in src
  # starting at src-off as with S-cpy.
  #
  # Behaviour is undefined if either array is not sufficiently sized for the
  # operation, or if count, dst-off, or src-off is negative.
  #
  # Behaviour is undefined if the memory regions in the destination and source
  # overlap.
  #
  # Types:
  #   SXT[] <- dst
  #   SXT[] <- src
  elt S-cpy-a {
    register dv dst {
      prop reg-read ;# mutable destination
    }
    register i dst-off {
      prop reg-read
    }
    register dv src {
      prop reg-read
    }
    register i src-off {
      prop reg-read
    }
    register i count {
      prop reg-read
    }
    S-sxt
    bool preserve-src
  }

  # Like S-cpy, but the first $tail-length elements from src's tail are also
  # copied into dst's tail.
  #
  # Behaviour is undefined if tail-length is negative or if it is greater than
  # the tail length used to allocate either src or dst.
  #
  # Types:
  #   SXT <- dst
  #   SXT <- src
  elt S-cpy-t {
    register dv dst {
      prop reg-read ;# mutable destination
    }
    register dv src {
      prop reg-read
    }
    register i tail-length {
      prop reg-read
    }
    S-sxt {
      prop global-sxt-with-tail-ref
    }
    bool preserve-src
  }

  # Reads an integer field from a struct.
  #
  # Semantics: src is assumed to be a strangelet referencing an instance of the
  # given struct. The integer field identified by field is read non-atomically
  # and the value stored in dst.
  #
  # Types:
  #   SXT <- src
  elt S-i-ld {
    register i dst {
      prop reg-write
    }
    register dv src {
      prop reg-read
    }
    S-sxt
    int field {
      prop sxt-field-ref-int
    }
    bool volatil
  }

  # Writes an integer field in a struct.
  #
  # Semantics: dst is assumed to be a strangelet referencing an instance of
  # sxt. src is truncated to the size of the integer field identified by $field
  # and then written into it non-atomically.
  #
  # Types:
  #   SXT <- dst
  elt S-i-st {
    register dv dst {
      prop reg-read ;# mutable destination
    }
    S-sxt
    int field {
      prop sxt-field-ref-int
    }
    register i src {
      prop reg-read
    }
    bool volatil
  }

  # Atomically reads an integer field in a struct.
  #
  # Semantics: dst is set to the possibly-sign-extended value of the select
  # integer field in src (assumed to be a strangelet referencing an instance of
  # sxt), which is read atomically with the given memory order.
  #
  # Types:
  #   SXT <- src
  elt S-ia-ld {
    register i dst {
      prop reg-write
    }
    register dv src {
      prop reg-read
    }
    S-sxt
    int field {
      prop sxt-field-ref-atomic-int
    }
    bool volatil
    memory-order order
  }

  # Atomically stores an integer into a field in a struct.
  #
  # Semantics: dst is assumed to be a strangelet referencing an instance of
  # sxt. The chosen atomic integer field is atomically set to the value of src
  # with the given memory order.
  #
  # Types:
  #   SXT <- dst
  elt S-ia-st {
    register dv dst {
      prop reg-read ;# mutable destination
    }
    S-sxt
    int field {
      prop sxt-field-ref-atomic-int
    }
    register i src {
      prop reg-read
    }
    bool volatil
    memory-order order
  }

  # Performs an atomic compare-and-swap operation on an atomic integer in a
  # struct.
  #
  # Semantics: dst is assumed to be a strangelet referencing an instance of
  # sxt. from and to are both truncated as necessary to the size of that field.
  # The operation will not succeed if the current value of the field is not
  # equal to $from, and may succeed if it is equal to $from. If $weak is false,
  # the operation always succeds if the initial comparison passes.
  #
  # On success, the truncated value from $to is written into the field and
  # $success is set to 1. The overall memory ordering of the operation is
  # dictated by at least success-order.
  #
  # On failure, $success is set to 0, and the overall memory ordering of the
  # operation is dictated by at least failure-order.
  #
  # In either case, $actual is set to the possibly-sign-extended value of the
  # field prior to the operation.
  #
  # Types:
  #   SXT <- dst
  elt S-ia-cas {
    register i success {
      prop reg-write
    }
    register i actual {
      prop reg-write
    }
    register dv dst {
      prop reg-read ;# mutable destination
    }
    S-sxt
    int field {
      prop sxt-field-ref-atomic-int
    }
    register i from {
      prop reg-read
    }
    register i to {
      prop reg-read
    }
    bool volatil
    bool weak
    memory-order success-order
    memory-order failure-order
  }

  # Performs an atomic read-modify-write operation on an atomic integer in a
  # struct.
  #
  # Semantics: dst is assumed to be a strangelet referencing an instance of
  # sxt. The indicated atomic integer field is read (and possibly
  # sign-extended) and the old value written to $old. $src is truncated to the
  # size of the field, and then combined with the old value as dictated by $op,
  # and the result written back into the field.
  #
  # Types:
  #   SXT <- dst
  elt S-ia-rmw {
    register i old {
      prop reg-write
    }
    register dv dst {
      prop reg-read ;# mutable destination
    }
    S-sxt
    int field {
      prop sxt-field-ref-atomic-int
    }
    register i src {
      prop reg-read
    }
    rmw-op op
    bool volatil
    memory-order order
  }

  # Loads a value from a real field of a struct.
  #
  # Semantics: src is assumed to be a strangelet referencing an instance of
  # sxt. The chosen field is read, expanded or truncated to an ava_real, then
  # stored in dst.
  #
  # Types:
  #   SXT <- src
  elt S-r-ld {
    register dv dst {
      prop reg-write
    }
    register dv src {
      prop reg-read
    }
    S-sxt
    int field {
      prop sxt-field-ref-real
    }
    bool volatil
  }

  # Writes a value into a real field of a struct.
  #
  # Semantics: dst is assumed to be a strangelet referencing an instance of
  # sxt. src is converted to an ava_real, then expanded or truncated to the
  # appropriate size, then stored in that field of dst.
  #
  # Types:
  #   SXT <- dst
  elt S-r-st {
    attr can-throw

    register dv dst {
      prop reg-read ;# mutable destination
    }
    S-sxt
    int field {
      prop sxt-field-ref-real
    }
    register dv src  {
      prop reg-read
    }
    bool volatil
  }

  # Reads a pointer field in a struct into a strangelet.
  #
  # Semantics: src is assumed to be a strangelet referencing an instance of
  # sxt. The pointer in the selected field is read non-atomically, wrapped in a
  # strangelet, and stored in dst.
  #
  # Behaviour is undefined if executed on an uninitialised hybrid field or a
  # hybrid field that holds an integer.
  #
  # Types:
  #   SXT <- src
  #   dst <- SXT.field
  elt S-p-ld {
    register dv dst {
      prop reg-write
    }
    register dv src {
      prop reg-read
    }
    S-sxt
    int field {
      prop sxt-field-ref-ptr-hybrid
    }
    bool volatil
  }

  # Writes a pointer field in a struct.
  #
  # Semantics: src is assumed to be a strangelet, and its pointer is extracted.
  # dst is assumed to be a strangelet referencing an instance of sxt. The
  # pointer from src is written non-atomically into the selected pointer field.
  #
  # If executed on a hybrid field, the field will then be considered to hold a
  # pointer. If field names a hybrid field, behaviour is undefined if the
  # pointer in src does not have at least 2-byte alignment.
  #
  # Types:
  #   SXT <- dst
  #   SXT.field <- src
  elt S-p-st {
    register dv dst {
      prop reg-read ;# mutable destination
    }
    S-sxt
    int field {
      prop sxt-field-ref-ptr-hybrid
    }
    register dv src {
      prop reg-read
    }
    bool volatil
  }

  # Atomically reads a pointer field in a struct.
  #
  # Semantics: dst is set to a strangelet containing the value in the pointer
  # field in src (assumed to be a strangelet referencing an instance of sxt),
  # which is read atomically with the given memory order.
  #
  # Types:
  #   SXT <- src
  #   dst <- SXT.field
  elt S-pa-ld {
    register dv dst {
      prop reg-write
    }
    register dv src {
      prop reg-read
    }
    S-sxt
    int field {
      prop sxt-field-ref-atomic-ptr
    }
    bool volatil
    memory-order order
  }

  # Atomically stores a pointer into a field in a struct.
  #
  # Semantics: dst is assumed to be a strangelet referencing an instance of
  # sxt. The chosen atomic pointer field is atomically set to the pointer in
  # strangelet src with the given memory order.
  #
  # Types:
  #   SXT <- dst
  #   SXT.field <- src
  elt S-pa-st {
    register dv dst {
      prop reg-read ;# mutable destination
    }
    S-sxt
    int field {
      prop sxt-field-ref-atomic-ptr
    }
    register dv src {
      prop reg-read
    }
    bool volatil
    memory-order order
  }

  # Performs an atomic compare-and-swap operation on an atomic pointer in a
  # struct.
  #
  # Semantics: dst is assumed to be a strangelet referencing an instance of
  # sxt. from and to are both assumed to be strangelets; operations involve the
  # pointers stored therein. The operation will not succeed if the current
  # value of the field is not equal to $from, and may succeed if it is equal to
  # $from. If $weak is false, the operation always succeds if the initial
  # comparison passes.
  #
  # On success, the pointer in $to is written into the field and $success is
  # set to 1. The overall memory ordering of the operation is dictated by at
  # least success-order.
  #
  # On failure, $success is set to 0, and the overall memory ordering of the
  # operation is dictated by at least failure-order.
  #
  # In either case, $actual is set to a strangelet holding the pointer that was
  # originally in that location.
  #
  # Types:
  #   SXT <- dst
  #   actual <- SXT.field
  #   SXT.field <- from
  #   SXT.field <- to
  elt S-pa-cas {
    register i success {
      prop reg-write
    }
    register dv actual {
      prop reg-write
    }
    register dv dst {
      prop reg-read ;# mutable destination
    }
    S-sxt
    int field {
      prop sxt-field-ref-atomic-ptr
    }
    register dv from {
      prop reg-read
    }
    register dv to {
      prop reg-read
    }
    bool volatil
    bool weak
    memory-order success-order
    memory-order failure-order
  }

  # Atomically sets a pointer field, providing the old value.
  #
  # Semantics: dst is assumed to be a strangelet referencing an instance of
  # sxt. The pointer in the chosen field is read and stored in a strangelet in
  # old. The chosen field is then set to the pointer stored in src, which is
  # assumed to be a strangelet.
  #
  # Types:
  #   SXT <- dst
  #   old <- SXT.field
  #   SXT.field <- src
  elt S-pa-xch {
    register dv old {
      prop reg-write
    }
    register dv dst {
      prop reg-read ;# mutable destination
    }
    S-sxt
    int field {
      prop sxt-field-ref-atomic-ptr
    }
    register dv src {
      prop reg-read
    }
    bool volatil
    memory-order order
  }

  # Loads the integer value stored in a hybrid field of a struct.
  #
  # Semantics: src is assumed to be a strangelet referencing an instance of
  # sxt. The integer in the selected hybrid field is written into dst.
  #
  # Behaviour is undefined if the field is uninitialised or currently holds a
  # pointer.
  #
  # Types:
  #   SXT <- src
  elt S-hi-ld {
    register i dst {
      prop reg-write
    }
    register dv src {
      prop reg-read
    }
    S-sxt
    int field {
      prop sxt-field-ref-hybrid
    }
    bool volatil
  }

  # Writes an integer into a hybrid field of a struct.
  #
  # Semantics: dst is assumed to be a strangelet referencing an instance of
  # sxt. The selected hybrid field is set to the integer in src.
  #
  # Behaviour is undefined if the value in src is even.
  #
  # Types:
  #   SXT <- dst
  elt S-hi-st {
    register dv dst {
      prop reg-read ;# mutable destination
    }
    S-sxt
    int field {
      prop sxt-field-ref-hybrid
    }
    register i src {
      prop reg-read
    }
    bool volatil
  }

  # Determines whether a hybrid field holds an integer.
  #
  # Semantics: src is assumed to be a strangelet referencing an instance of
  # sxt. The selected hybrid field is inspected. If it contains an integer, dst
  # is set to 1. Otherwise, dst is set to 0.
  #
  # Behaviour is undefined if executed on an uninitialised field.
  #
  # Types:
  #   SXT <- src
  elt S-hy-intp {
    register i dst {
      prop reg-write
    }
    register dv src {
      prop reg-read
    }
    S-sxt
    int field {
      prop sxt-field-ref-hybrid
    }
    bool volatil
  }

  # Loads a value field from a struct.
  #
  # Semantics: src is assumed to be a strangelet referencing an instance of
  # sxt. The given value field is read non-atomically and stored in dst.
  #
  # Types:
  #   SXT <- src
  elt S-v-ld {
    register dv dst {
      prop reg-write
    }
    register dv src {
      prop reg-read
    }
    S-sxt
    int field {
      prop sxt-field-ref-value
    }
    bool volatil
  }

  # Stores a value into a field in a struct.
  #
  # Semantics: dst is assumed to be a strangelet referencing an instance of
  # sxt. The value in src is written into the selected value field of dst.
  #
  # Types:
  #   SXT <- dst
  elt S-v-st {
    register dv dst {
      prop reg-read ;# mutable destination
    }
    S-sxt
    int field {
      prop sxt-field-ref-value
    }
    register dv src {
      prop reg-read
    }
    bool volatil
  }

  # Obtains a pointer to a composed field.
  #
  # Semantics: dst is set to a strangelet containing a pointer to the
  # identified field in src, which is assumed to be a strangelet referencing an
  # instance of sxt.
  #
  # On strongly-typed platforms, dst will point to a raw instance of sxt for
  # compose fields, and to an array/list/etc for array and tail fields.
  #
  # Types:
  #   SXT <- src
  #   dst <- SXT.field
  elt S-gfp {
    register dv dst {
      prop reg-write
    }
    register dv src {
      prop reg-read
    }
    S-sxt
    int field {
      prop sxt-field-ref-composite
    }
  }

  # Selects an element from an array.
  #
  # Semantics: dst is set to a strangelet containing a pointer to the indexth
  # element in an array of sxt instances referenced by assumed strangelet src.
  #
  # Behaviour is undefined if index is negative or greater than or equal to the
  # actual length of the array.
  #
  # On strongly-typed platforms, this reduces the dimensionality of the array
  # types by 1.
  #
  # Types:
  #   SXT[] <- src
  #   dst <- SXT
  elt S-gap {
    register dv dst {
      prop reg-write
    }
    register dv src {
      prop reg-read
    }
    S-sxt
    register i index {
      prop reg-read
    }
  }

  # Inserts a memory barrier (or "fence") at the given location.
  #
  # Semantics: This instruction does nothing by itself, but introduces a
  # barrier restricting the ordering of memory operations relative to the
  # membar itself.
  #
  # More information can be found in the LLVM documentation for the fence
  # instruction. http://llvm.org/docs/LangRef.html#fence-instruction
  #
  # This instruction is not actually unsafe, but is not useful except when used
  # with the unsafe strangelet operations, and so is similarly prefixed.
  elt S-membar {
    memory-order order
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
  # Pushes an expander onto the stack.
  #
  #   ( ) -- ( u )
  elt expander {
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
