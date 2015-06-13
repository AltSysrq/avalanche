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

Syntax I --- Tokenisation / Lexing
----------------------------------

Avalanche uses a rather simple tokenisation scheme. Like Tcl, it is
non-regular, though to a lesser extent.

In all contexts, the ASCII characters 0x00--0x08, 0x0B, 0x0C, 0x0E--0x1F, and
0x7F are illegal; it is an error if they occur at any point in a string to be
tokenised. All other 8-bit encoding units are legal.

The following characters are considered whitespace: Space (0x20), horizontal
tab (0x09).

"Newline" refers to a Line-Feed character, a Carraige-Return immediately
followed by a Line-Feed, or a lone Carraige-Return. All "Newlines" are
implicitly normalised to a lone Line-Feed character before any other kind of
processing, as if the input originally used Line-Feed-only line endings
exclusively.

Tokens are classified as "attached" or "independent" based on the preceding
character. The first token in the input is always independent. Otherwise, the
token is attached unless the immediately preceding character is one of:

- Whitpespace (0x20, 0x09)
- Line-Feed or Carraige-Return (0x0A, 0x0D)
- One of the following special characters: Left-Parenthesis; Left-Bracket;
  Left-Brace; Back-Quote.

The following characters are "special":

- Left-Parenthesis: `(`
- Right-Parenthesis: `)`
- Left-Bracket: `[`
- Right-Bracket: `]`
- Left-Brace: `{`
- Right-Brace: `}`
- Semicolon: `;`
- Double-Quote: `"`
- Back-Quote: \`
- Semicolon: `;`
- Backslash: `\`

All legal non-Special, non-Whitespace, non-Newline characters are Word
characters.

A semicolon `;` begins a Comment. All characters from the Semicolon to the next
Newline (or end-of-file) are discarded, not including the Newline itself.

```
  ; this is a comment
  not-a-comment
```

Backslashes `\` are illegal in the base state, except in some combinations
described below.

A Newline produces a Newline Token. A Backslash followed by any amount of
Whitespace, a possible Comment, then a Newline suppresses the Newline Token
that would result. A Backslash followed by one or more Whitespace characters
itself results in a Newline Token otherwise. Such "synthetic" Newline Tokens
must occur Independent.

```
  logical line 1
  logical line 2
  logical line 3 \ ; comment
  still logical line 3
  logical line 4 \ logical line 5
```

Any contiguous sequence of Word characters is a Bareword Token. Certain
contexts may place additional interpretation on the contents of Bareword
Tokens. Barewords may only occur in Independent context.

```
  word 01234 +/=? <<- every token on this line is a Bareword
```

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

A-String-Literals and R-String-Literals must occur Independent.

```
  "simple string"
  "strings can
span lines
like this"
  `lr string`

  "backquotes emulate`$string`interpolation"

  "\e[7minverted text\e[0m"
  "explicit\nnewline"
  "contains quotes and backslash: \"\`\\"
  "hex escape \x5A\x5b\x00"
```

A Left-Parenthesis `(` is interpreted as a Begin-Substitution Token if it is
Independent, and as a Begin-Name-Subscript Token otherwise. A Right-Parenthesis
`)` is a Close-Paren Token. The Right-Paren may be immediately followed by any
number of Word characters, which are considered to be part of the same token.

```
  $dict($key)           ; Name-Subscript
  $dict($key)i          ; Custom Name-Subscript
  foo (bar baz)         ; Substitution
```

A Left-Bracket `[` is interpreted as a Begin-Semiliteral Token if it is
Independent, and as a Begin-Numeric-Subscript Token otherwise. A Right-Bracket
`]` is a Close-Bracket Token. The Right-Bracket may be immediateyl followed by
any number of Word characters, which are considered to be part of the same
token.

```
  $list[$offset]        ; Numeric-Subscript
  $list[$offset]i       ; Custom Name-Subscript
  [foo bar baz]         ; Semiliteral
```

A Left-Brace `{` is interpreted as a Begin-Block Token if it is Independent,
and a Begin-String-Subscript Token otherwise. A Right-Brace '}' is a
Close-Brace Token. The Right-Brace may be immediately followed by any number of
Word characters, which are considered to be part of the same token.

```
  $str{$index}          ; String-Subscript
  $str{$index}j         ; Custom String-Subscript
  map { foo bar }       ; Block
```

A Backslash followed by a Left-Brace `{` begins a Verbatim Token. The token
terminates upon encountering a _matching_ Right-Brace `}` also preceded by a
Backslash. Only brace characters which are also preceded by a Backslash are
counted for matching. Escape sequences work the same as in String Literals,
except that the leader is a Backslash followed by a Semicolon. All characters
between the outermost braces not part of a Backslash-Semicolon escape sequence
are preserved verbatim. Verbatim tokens must occur Independent.

```
  \{verbatim\}
  \{nested \{ver\{at\}im\}\}
  \{normal {braces are not counted\}
  \{not actual \e\s\c\a\p\e\ sequence}
  \{newline: \;n\}
  \{may
  span lines\}
```
