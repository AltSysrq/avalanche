Avalanche Language Specification
================================

Type System
-----------

Like Tcl, Avalanche recognises one and only one type --- the string. The
standard library provides functions to manipulate strings with particular
formats to achieve, eg, lists, and there are internal optimisations to avoid
constant stringify/parse cycles.

Unlike Tcl, code is not normally itself represented as a string. Rather, code
blocks are compiled, and string references thereto are passed around.

Also unlike Tcl, Avalance strings are byte strings rather than character
strings.

The compiler does need to about some of the composite pseudo-types, which are
documented below. Pseudo-types commonly present within source code are also
described.

### Booleans

The canonical boolean values are `true` and `false`. `yes` and `on` are also
understood to mean `true`; `no` and `off` are also understood to mean `false`.
None of these are case-sensitive. All strings interpretable as non-zero,
non-NaN numbers are considered true; strings interpretable as zero or NaN are
false. Other strings are not interpretable as booleans.

### Integers

Integers are numeric strings not containing a decimal point. By default, they
are in base-10; the prefices `0b`, `0o`, and `0x` can be used to select binary,
octal, and hexadecimal, respectively, case-insensitive. Non-numeric boolean
strings can also be considered integers, with values 1 for `true` and 0 for
`false`.

### Floats

Floats are numeric strings containing a decimal point. They use the same syntax
as accepted by `strtod(3)`.

### Lists

A list is any string which can be parsed with Avalanche's Command tokenisation
rules without invoking any command, variable substitution, etc. (Ie, only
literal tokens are permitted.) The list is said to contain one element for each
token parsed. Canonical lists have each item bare if possible, and otherwise
nested within verbatim quote blocks.

### Dicts

A dict(ionary) is any list with an even number of elements. Each even element
(counting from zero) is a key, matched to the value following it. If the same
key occurs more than once, only the last occurrance is meaningful.

Syntax I --- Tokenisation / Lexing
----------------------------------

Avalanche uses a rather simple tokenisation scheme. Like Tcl, it requires a PDA
(rather than an NFA) to parse, though it is a bit more complex than that of
Tcl.

"Scopes" parsing described later are defined entirely by recursion levels in
the lexer.

"Closing characters" are any of `)`, `]`, or `}`.

### Ground State

The ground state is the initial state of the lexer.

All whitespace, including newlines, is discarded.

If a semicolon `;` is encountered, it and all characters up to the next newline
or end of file are discarded.

If an ampersand `&` is encountered, the state switches to the Special Form
state after consuming it.

If a closing character is encoundered, the state is popped.

Any other character switches to the Command state in Free mode.

### Special Form State

The Special Form state consumes all characters up to but not including the next
semicolon `;` or newline, whichever comes first. The result must be a valid
List of at least one element. The first element defines the type of special
form; the rest are its arguments. The meaning of special forms is discussed
later.

### Command State

Command form exists in two forms: Parenthetical and Free. The only difference
is in how newlines are handled.

All whitespace, except for newlines, is discarded.

If a newline is encountered, it is discarded if in Parenthetical mode;
otherwise, it is consumed and the state switches to the Ground state.

If a semicolon `;` is encountered, it and all characters up to but not
including the next newline are discarded.

If a left parenthesis `(` is encountered, the state is pushed to Command state,
Parenthetical mode. It must be popped by a matching `)`. The resulting token
sequence is considered one token at this level; this is a Command Substitution.

If a left bracket `[` is encountered, the state is pushed to Command state,
Parenthetical mode. It must be popped by a matching `]`.

If a left brace `{` is encountered, the state is pushed to the Ground state. It
must be popped by a matching `}`. The resulting token sequence is considered
one token at this level, and is considered a Code Block.

If a closing character is encountered, the state is popped.

If a double-quote `"` or back-quote `\`` is encountered, the state pushes to
the String state.

For any other character, the state pushes to Token state.

### Token State

Token State consumes all characters up to the next whitespace, semicolon `;`,
back-quote `\``, or paren-like character `(`, `[`, `{`, `}`, `]`, `)`.

If the consumed characters begin with a dollar sign `$`, the token is
considered a variable substitution; if they begin with an ampersand `&`, it is
considered a variable reference. Otherwise, it is a bareword. In the former
two cases, the "value" of the token is all characters after the `$` or `&`; in
the latter, it is the whole token.

(Notice that, unlike Tcl, *no* escaping is permitted outside of quotes.)

Token State switches to Post-Token State after consuming its characters.

### String State

String State consumes characters until it encounteres a `"` or `\`` not escaped.

Backslash escaping works mostly as in C, except that octal escapes are not
supported, but `\e` for ASCII ESC is s upported. `\xXX` escapes always have two
hexits.

The "value" of a string token is its contents with backslash escape sequences
substituted.

Strings come in four flavours, which provide string interpolation in the higher
level of the language:
```
  "This is a V-String"
  "This is an L-String`
  `This is an R-String"
  `This is an LR-String`
```

### Post-Token State

Any whitespace character, closing character, or semicolon `;` is not consumed
and the state is popped.

A left parenthesis `(` is consumed, and the state is pushed to the Command
state in Parenthetical mode, which must be terminated by a matching `)`. The
resulting token sequence is considered one token, a Name Subscript.

A left bracket `[` is consumed, and the state is pushed to the Command state in
Parenthetical mode, which must be terminated by a matching `]`. The resulting
token sequence is considered one token, an Index Subscript.

A back-quote `\`` is not consumed, and the state is switched to String state.

Any other character is an error.

Syntax II --- Parsing
---------------------

"Parsing" as described here only applies to executable code; constructs lexed
for other purpose are not subject to it.

Parsing is _strongly_ tied to the set of defined identifiers, and is also
strongly connected to the recursive state of the lexer.

Each logical token (which might be more than one physical token, eg, in the
case of nested parentheses) is associated with a valence (one of V, L, R, or
LR) and, if non-V valence, an integer precedence.

Valence determines whether the token takes positional arguments; tokens with no
arguments have valence V; postfix tokens are L; prefix are R; and interfix are
LR. Precedence effectively controls order of operations: Given two ordered
tokens A and B, B activates-before A iff
`ceil(B.precedence/2)<floor(A.precedence/2)`; otherwise, A activates-before B.
This means that odd precedence produces right associativity of LR tokens, and
even precedence produces left associativity.

Parsing operates by locating the token which activates-before all other tokens.
All tokens in the list on either side (corresponding to its valence) are passed
to whatever handles that token; it is an error if there are no tokens on a
valent side. Typically, the handler will substitute itself with some new token
sequence containing its arguments, or will cause its arguments to be evaluated
separately.

A token sequence which exclusively contains V tokens is considered a standard
command. A standard command of zero commands is a syntax error. If there is
exactly one token, the sequence describes a commend evaluating to the result of
evaluating that token alone. Otherwise, the the sequence describes a command
invocation, where the first token evaluates to the specifier of the command,
and all other tokens evaluate to its arguments.

