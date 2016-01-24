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
that would result, as well as any that would result from immediately subsequent
lines containing only Whitespace and Comments. A Newline (and all preceding
occurrences in blank lines) is also suppressed if it is followed by any number
of Whitespace characters, a Backslash, and at least one Whitespace character. A
Backslash followed by one or more Whitespace characters itself results in a
Newline Token otherwise. Such "synthetic" Newline Tokens must occur
Independent.

```
  logical line 1
  logical line 2
  logical line 3 \ ; comment
  still logical line 3
  logical line 4 \ logical line 5
  logical lnie 6
  \ still logical line 6
  \ ; Doesn't escape the NL following it because it is only preceded by
    ; Whitespac
  logical line 7
  \*foo ; logical line 8, since the \ is not followed by Whitespace
  logical line 9
  ; Some long comment
  ; spanning multiple lines
  \ still logical line 9
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

A Backslash followed by an Asterisk `*` forms a Spread token. Any token
immediately following the Spread token is considered Independent. The Spread
token must always be Independent.

A Backslash followed by an ASCII letter then any number of bareword characters
is a Keysym token. The Keysym token must always be independent.

Modules, Packages, and Names
----------------------------

An Avalanche program is divided into two levels of sub-units: packages and
modules.

Packages are essentially shared libraries, though the executable program is
itself also considered a package. All global names in a package share a common
prefix, which consitsts of the package name followed by a colon. Packages may
depend on other packages non-cyclicly.

Each package can be decomposed into one or more modules. Each module
corresponds to one source file. Modules do not define separate namespaces, and
their boundaries are invisible outside of their own package. Modules are always
named after the path of the file which defines them, relative to the root
source directory; forward-slashes are used as a path separator, and the `.ava`
extension is omitted. Modules may depend on other modules in the same package
non-cyclicly.

Other than the forced `package:simple-name` structure, names are otherwise
mostly up to convention. Generally it is assumed that `.` and `:` are used as
namespace separators, but the language ascribes no special meaning to these
characters or any others.

Package names are conventionally all-lowercase reverse dotted domain names,
similar to Java convention. Multiple words are separated by hyphens. For
example, the Avalanche standard library is in the `org.ava-lang.avast` package.
Packages are conventionally expected to be complete units of functionality; for
example, one would not have have separate `com.example.httpclient.methods` and
`com.example.httpclient.client` packages. In the rare case where such division
is appropriate, the "parent" package should re-export all the names from the
"sub-packages" so that most clients need not worry about the division.

Local names (ie, the part following the package name and colon) are
conventionally composed of zero or more `.`-terminated namespaces, followed by
the simple name. Words are separated by hyphens. Namespace names are
all-lowercase; simple names are all-lowercase for most variables and functions,
all uppercase for constants, and mixed-case (ie, the first letter of each word
capitalised, the others lowercase) for non-local variables referencing mutable
storage and classes.

Conventionally, simple names containing only alphanumeric characters and
hyphens and not ending with a hyphen are "safe"; ie, they should not be used
for operator macro names. Other simple names may be used as operator macro
names.

Global names are given one of three visibility levels. Private names are
visible only to the module in which they are defined, and need not be globally
unique. Internal names are visible to all modles within the containing package.
Public names are visible to all packages.

Name resolution is performed via lexical scoping; each scope has its own
name-to-symbol table; when a name is to be resolved, the inner-most scope's
table with a matching name is used. New names are always defined in the
inner-most scope. Names in the symbol table may be either _strong_ or _weak_. A
strong entry always takes precedence over any other entry, and it is an error
to introduce more than one strong entry into the same table. It is legal to
introduce multiple weak entries into the same table, but any attempt to use a
name mapped to more than one weak entry results in an error, on grounds that
the name is ambiguous.

Namespace imports are achieved by finding all visible names with a given prefix
and adding them to the current scope as weak entries with a different prefix.
For example, one might import `com.example.httpclient:` to the prefix `http.`,
which would make `com.example.hnttpclient:foo` accessible as `http.foo` within
the scope of the import.

### Rationale

Modules are separate from packages and do not define namespaces so that the way
the author of a shared library organises their code is not reflected in the
resulting API; conversely, the API does not restrict the way one choses to
organise code. The conflation of packages and directory layout in Java is one
of the biggest mistakes in that language, which renders internal ("package
private") visibility nearly useless in larger projects.

Package names conventionally include a domain name to avoid conflicts between
packages which would otherwise have the same name. While in practise it is rare
for one piece of code to depend on two packages that have the same simple name,
different packages could do so, which would otherwise make it impossible to
link both into the same executable.

Domain names in package names are in reverse-dot order to permit certain types
of multi-package imports; eg, one could import `com.example.` to be able to
directly reference all packages under `com.example.`.

Importing works by copying symbols rather than building a search path because
it is easier to describe and reason about, especially given the ability to
transplant names onto a different prefix.

Arbitrary re-prefixing is permitted --- and strongly encouraged --- so that
programmers can use namespaces without needing to choose between stripping the
prefix entirely or using the whole name (a choice between ambiguity and extreme
verbosity).

Syntax II --- General Syntax
----------------------------

The output from lexical analysis is first subjected to grouping, which results
in a tree of syntax units. This phase essentially exists to balance
parentheses and group tokens according to those parentheses. The rules are as
follows:

- A Begin-Substitution or Begin-Name-Subscript token collects all following
  syntax units into a Substitution or Name-Subscript syntax unit, up to the
  next Close-Parenthesis token not consumed by one of those syntax units. The
  Close-Parenthesis is consumed but not considered part of the syntax units. An
  error results if no matching Close-Parenthesis is found.

- A Begin-Semiliteral or Begin-Numeric-Subscript act as the above, but produce
  a Semiliteral or Numeric-Subscript syntax unit, and close on a Close-Bracket.

- A Begin-Block or Begin-String-Subscript act as the above, but produce a Block
  or String-Subscript syntax unit, and close on a Close-Brace.

- A Close-Parenthesis, Close-Bracket, or Close-Brace token not consumed by one
  of the above rules results in an error.

- A Spread token forms a Spread unit wrapping whatever syntax unit follows it,
  which must not be a Newline token (even if Newline tokens are normally
  deleted in the context). It is an error if it is not followed by anything.

- Any other token is a syntax unit by itself.

All Newline tokens directly within a Substitution, Name-Subscript, Semiliteral,
Numeric-Subscript, or String-Subscript syntax are deleted.

The next step is subscript simplification. Recursively, from left to right,
evry Name-Subscript, Numeric-Subscript, and String-Subscript unit and the unit
immediately preceding it (which is guaranteed to exist by the lexical rules) is
transformed into a single Substitution containing the following elements:

- The Bareword `#name-subscript#`, `#numeric-subscript#`, or
  `#string-subscript#` based on whether the second unit is a Name-Subscript,
  Numeric-Subscript, or String-Subscript unit, respectively.

- A Bareword consisting of the ending tag on the Subscript unit surrounded by
  hashes.

- The unit preceding the Subscript, unmodified.

- The Subscript unit itself, but changed into a Substitution with an empty
  string tag.

For example, the expression `foo[a + b]tag` is transformed into
```
  (#numeric-subscript# #tag# foo (a + b))
```

When a subscript is preceded by a Spread unit, the subscript acts as if it were
within the spread, such that `\*foo[42]` is equivalent to `\*(foo[42])` rather
than `(\*foo)[42]`.

After grouping and subscript simplification, the parser recursively walks the
structure to perform variable and expander symplification.

A Bareword of length at least 3 beginning with two dolar signs and no other
dollar signs has the dollar signs dropped and becomes an Expander syntax unit.

For each remaining Bareword syntax unit which contains at least one dollar
sign, the following transformation is applied:

- The Bareword is split into alternating string and variable parts, delimited
  by dollar signs, with any leading empty string part dropped. For example, the
  bareword `$foo` is just a variable part `foo`; `a$b$c` is a string `a`,
  variable `b`, and string `c`; `$a$$b` is variable `a`, empty string, variable
  `b`. An error occurs if any variable part is empty.

- Each string part is placed into an R-String, L-String, or LR-String
  token/syntax-unit, depending on whether it is at the beginning, end, or
  middle of the sequence of parts.

- Each variable part is placed into a Substitution syntax units, containing the
  Bareword `#var#` followed by an A-String containing the variable name.

- All syntax units produced are wrapped in a Substitution group.

For example, the bareword `pre$a$mid$x$post` is equivalent to the input string
```
  ("pre` (#var# a) `mid` (#var# x) `post")
```

As a special exception to the above, the bareword `$` is expanded to the
expression `((#var# $))`, called the "context variable".

Keysym tokens then undergo keysym simplification. Any Keysym token is repaced
with a Substitution containing the bareword `#keysym#` and a Bareword whose
string value is the same as the Keysym (which does not include the leading
backslash). It is an error if the Keysym contains a dollar sign.

Next, the parser performs string regrouping on the direct contents of each
Semiliteral. Units are first grouped into sequences of contiguous units where
each unit meets at least one of the following:

- It immediately precedes an L-Sring or LR-String.
- It immediately follows an R-String or LR-String.
- It is an L-String, LR-String, or R-String.

Such sequences are wrapped in a single Substitution unit, which replaces that
sequence as a whole within the Semiliteral. Any Bareword units are replaced
with Verbatim units with the same content (to prevent macro processing to occur
on them).

An error occurs if the first unit of a Semiliteral is an L-String or an
LR-String, or if the final unit of a Semiliteral is an R-String or an
LR-String.

Finally, group-tag simplification is performed. Any Substitution, Semiliteral,
or Block with a non-empty closing tag is replaced by a single Substitution
containing the following elements:

- A bareword composed of the string `#substitution#`, `#semiliteral#`, or
  `#block#` (according to the input unit type) followed by the tag on the input
  unit itself.

- The input unit, except with no tag.

For example, the expression `(foo bar)baz` is transformed into
`(#substitution#baz (foo bar))`.

Every syntax unit is broken into statements separated by Newline units.
(Recall that Newlines were stripped from most unit types previously; they only
remain at top-level and Blocks.)

Macro expansion as described in Syntax III is performed on every statement,
starting from the top-level and recursing into the sub-units of each syntax
unit in a statement only after macro expansion for the statement's top-level
has completed.

Once all this is completed, the program has been reduced to a form to which the
described runtime semantics can be applied.

Syntax III --- Macro Processing
-------------------------------

Macros in avalanche come in four flavours:

- Expander macros occur anywhere but must be explicitly referenced using an
  Expander syntax unit. They are substituted before any implcit macro
  processing, including within semiliterals.

- Control macros occur at the beginning of a statement, and are substituted
  before any other implicit macro processing.

- Operator macros occur anywehere, and are substituted in an order based on
  their precedence and associativity.

- Function macros occur where a function name would normally occur, and are
  expanded after all other macro processing completes.

Macro processing is performed within Blocks (including the top-level) and
Substitutions. (Subscript simplification has already eliminated all Subscript
syntax units.) Semiliterals are not subject to any kind of macro processing,
but any Blocks or Substitutions they contain are, and Expanders are still
expanded.

A syntax unit is said to invoke a macro if it is a Bareword whose symbol in the
current scope references a macro. L-Strings, R-Strings, and LR-Strings are
treated as operator macros with precedence 20, using the following
pseudo-macro-templates (where `@` indicates conversion of the string to an
A-String):
```
  L-String =>   (#string-concat# (<) @) >
  R-String =>   < (#string-concat# @ (>))
  LR-String =>  (#string-concat# (#string-concat# (<) @) (>))
```

The macro processing algorithm for a single statement is as follows:

- Attempt to locate an Expander. If one is found, it must resolve to a unique
  symbol of type expander-macro. Substute it with no arguments. The
  substitution replaces the Expander syntax unit alone.

- If the first syntax unit invokes a control macro and the enclosing unit is a
  Block, substitute that macro with an empty left argument and a right argument
  consisting of all other syntax units in the statement. Restart macro
  processing on the result.

- Attempt to locate an operator macro. If one is found, substitute it, using
  all syntax units preceding the invoking unit as the left argument, and all
  syntax units following it as the right argument. Restart macro processing on
  the result. The algorithm for locating operator macros is described later.

- If the first syntax unit invokes a function macro and is followed by at least
  one syntax unit, substitute that macro with an empty left argument and a
  right argument consisting of all other syntax units in the statement. Restart
  macro processing on the result.

- Recurse into evert syntax unit subordinate to the statement and run macro
  processing upon it.

- Macro processing for the statement is complete.

Macro processing generally terminates if the statement is reduced to 0 or 1
syntax unit, except that control macros may be substituted in a statement
containing one syntax unit in a location where the result of evaluation would
cause the result to be discarded.

Every operator macro is associated with a precedence, which is an integer
between 1 and 40, inclusive. Precedence also dictates associativity; precedence
levels with an even index have left-to-right associativity, while those with an
odd index have right-to-left associativity.

The algorithm for locating an operator macro is as follows:

- For each precedence level starting from 1:

  - Examine each syntax unit in the statement. If the precedence level is even,
    start with the last one and move left (yielding left-to-right
    associativity); otherwise, start with the first one and move right
    (yielding right-to-left associativity).

  - If the search encounters a syntax unit which invokes an operator macro of
    the current precedence, that syntax unit is the next operator macro to
    invoke.

(Note the perhaps counter-intuitive property that higher-precedence operators
are processed _after_ lower-precedence operators.)

Barewords which result in ambiguous name lookups only result in errors at
macro-substitution time if at least one of the symbols would actually cause the
bareword to be used as a macro.

Aside from certain built-in macros that produce non-denotable results (eg,
`fun` or `ret`) or perform more complex transformations (eg, `=`), macro
substitution is performed by substituting the surrounding tokens into a syntax
template. A syntax template is essentially a sequence of normal syntax units,
except that Barewords are restricted/transformed as described below. Note that
all of this occurs after things like variable simplification, so rules for
barewords apply equally to variable names.

- A Bareword whose text begins with `!` is substituted into the result
  verbatim, except with the leading `!` removed.

- A Bareword whose text begins with `#` is substituted into the result
  verbatim, including the leading `#`. It is an error if the Bareword is only
  one-character long or does not also end in a `#`.

- The Bareword "$" is substituted into the result verbatim.

- A Bareword whose text begins with `%` is replaced at macro-definition time
  with the fully-qualified name of the symbol the remainder of the name
  references; at substitution time, a Bareword containing solely that
  fully-qualified name is substituted.

- A Bareword whose text begins with `?` is replaced at macro-substitution time
  with a Bareword containing a name guaranteed be unique throughout the entire
  compilation unit. All occurrences of the same `?`-prefixed Bareword within
  the same substitution of the macro result in the same substitution.

- A Bareword beginning with `>` refers to the syntax units following the macro
  invoker. It must match the pattern `>[0-9]*(-[0-9]+)?[*+]?`. The first of the
  optional integers indicates the index of the first syntax unit to be
  substituted, 0-indexed from the left, and the second indicates the index of
  the last syntax unit to be substituted, 0-indexed from the _right_. Both are
  inclusive, but result in an empty list if the beginning is after the end. A
  trailing `*` indicates that zero or more values are to be substituted; `+`
  indicates that one or more values are to be substituted.

  - A lone `>` expands to all syntax units following the macro invoker, but
    results in an error if there are none.

  - If a trailing `+` or `*` is given, both integers default to 0 if omitted.
    An error occurs if the token expands to zero elements in the case of `+`;
    `*` prevents any error from occurring if a range expands to zero elements.

  - If only one of the integers is given and neither `+` nor `*` is given, only
    the single syntax unit at that index is used to replace the unit. An error
    occurs if there is no unit at that index.

  Argument substitutions do not place their expansions into any kind of
  explicit grouping, so they must be explicitly wrapped if this is desired.

- A Bareword beginning with `<` acts the same as `>`, except that it acts on
  the syntax units preceding the invoker, and the roles of the indices are
  reversed, ie, the first indexes from the right, the second from the left.

- Any Bareword not described above is considered an error.

The above rules make macros mostly "hygenic" by default; the macro writer must
choose between generating a unique symbol apon expansion, resolving a symbol at
definition time, or explicitly using something from the expansion context.
(Technically, a macro is not perfectly hygenic since the expansion context
could redefine a fully-qualified name to have a different meaning.)

Basic Execution Semantics
-------------------------

The below paragraphs do not account for built-in macros that, eg, define
functions or effect control structures.

Execution of a Block or Substitution is performed by evaluating each statement
in sequence. The execution result of executing a Block is the empty string; the
execution result of executing a Substitution is the result of evaluating its
final statement, or the empty string if it contains no statements. (Certain
built-in macros may create Substitutions containing more than one statement,
though it is not syntactically denotable.)

A statement is evaluated by first evaluating each of its syntax units in
sequence from left to right. If the statement contains only one syntax unit, it
is evaluated to produce the result of the statement. Otherwise, the statement
is treated as a function call. If the first syntax unit was a Bareword, it must
name a function visible in the current scope; otherwise, the first syntax unit
is assumed to be an expression expanding to a [function
reference](src/runtime/avalanche/function.h). The rest of the syntax units'
evaluation results are each passed to the function as a single parameter, or
possibly spread if this state is set by a builtin macro.

Barewords, A-Strings, and Verbatims evaluate to their string value.

Substitutions evaluate to the result of executing their contents.

Semiliterals evaluate to a [list](src/runtime/avalanche/list.h) which contains
the same number of elements as the Semiliteral contains syntax units. Each
element at index N equals the result of evaluating the Nth syntax unit of the
Semiliteral. All syntax units are evaluated left-to-right when the Semiliteral
is evaluated.

Blocks evaluate to a [function reference](src/runtime/avalanche/function.h) of
a function which executes that block. The function implicitly takes N
positional arguments, given base-10 names from 1 to N, where N is the name of
the variable reference with the greatest numeric name. (Only names matching
`[1-9][0-9]*` are considered "numeric" for this purpose.)
