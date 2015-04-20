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
false. The empty string is false. Other strings are not interpretable as
booleans.

### Integers

Integers are numeric strings not containing a decimal point. By default, they
are in base-10; the prefices `0b`, `0o`, and `0x` can be used to select binary,
octal, and hexadecimal, respectively, case-insensitive. Non-numeric boolean
strings can also be considered integers, with values 1 for `true` and 0 for
`false`. In most contexts, the empty string is considered a zero integer.

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

Avalanche uses a rather simple tokenisation scheme. Like Tcl, it is
non-regular.

The following characters are considered whitespace: Space (0x20), horizontal
tab (0x09).

"Newline" refers to a Line-Feed character, a Carraige-Return immediately
followed by a Line-Feed, or a lone Carraige-Return. All "Newlines" are
implicitly normalised to a lone Line-Feed character.

The following characters are "special":
```
([{}])"`;\
```

The ASCII characters 0x00--0x08, 0x0B, 0x0C, 0x0E--0x1F, and 0x7F are illegal.

All other characters are non-special.

Backslashes `\` are illegal in the base state, except in some combinations
described below.

Whitespace is discarded by the lexer, but does separate tokens, and is relevant
immediately preceding certain token types. Tokens are considered "preceded by
whitespace" if they are proceded by one or more actual Whitespace characters,
or occur at the very beginning of a line.

Any contiguous sequence of non-special characters is a Bareword Token. Certain
contexts may place additional interpretation on the contents of Bareword
Tokens.

```
  Bareword ::= {nonspecial}+
```

A semicolon `;` begins a comment. All characters from the semicolon to the next
Newline (or end-of-file) are discarded, not including the Newline itself.

```
  Comment ::= ;[^\n]*
```

A Newline produces a Newline Token. A backslash followed by a Newline or any
whitespace character suppresses the next Newline Token that would occur, if no
other tokens occur on that line. Otherwise, the blackslash itself acts as a
Newline Token.

The characters between, but not including, a double-quote and a back-quote
(either on either side) form a String Literal Token. Within a String Literal,
C-style backslash escape sequences are supported.

- The following characters following a backslash simplify to themselves: \`"'
- The following characters are interpreted as in C (with e being ESC): `abefnrtv`
- An `x` is followed by two hexadecimal digits, not case-sensitive, and expands
  to the byte value represented thereby.
- Any other sequence following a backslash is illegal.

Note that strings _are_ permitted to span lines. A string which begins and ends
with a double-quote is an A-String-Literal; beginning with a back-quote is an
L-String-Literal; ending with a back-quote is an R-String-Literal; and
beginning and ending with a back-quote is an LR-String-Literal.

```
  string-escape ::= ([\\`"'abefnrtv]|x[0-9A-Fa-f]{2})
  string-body ::= ([^\\"`]|\\{string-escape})*
  A-String-Literal ::= "{string-body}"
  L-String-Literal ::= `{string-body}"
  R-String-Literal ::= "{string-body}`
  LR-String-Literal ::= `{string-body}`
```

A left-parenthesis `(` is interpreted as a Begin-Substitution Token if it is
preceded by whitespace, and as a Begin-Name-Subscript Token otherwise. A
close-parenthesis `)` is a Close-Paren Token.

A left-bracket `[` is interpreted as a Begin-Semiliteral Token if it is
preceded by whitespace, and as a Begin-Numeric-Subscript Token otherwise. A
close-bracket `]` is a Close-Bracket Token.

A left-brace `{` is interpreted as a Begin-Block Token if it is preceded by
whitespace, and is an error otherwise. A close-brace '}' is a Close-Brace
Token.

A backslash followed by a left brace `{` begins a Verbatim Token. The token
terminates upon encountering a _matching_ brace `}` also preceded by a
backslash. Only brace characters which are also preceded by a backslash are
counted for matching. Escape sequences work the same as in String Literals,
except that the leader is a backslash followed by a semicolon. All characters
between the outermost braces not part of a backslash-semicolon escape sequence
are preserved verbatim.