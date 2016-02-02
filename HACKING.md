Project Layout
==============

## bootstrap-runtime

The `bootstrap-runtime` directory contains the original C-based runtime and
compiler, which is used to compile the newer Avalanche-based runtime and
compiler. Note that it only implements the language sufficiently well for this
purpose; generally, new language features are not backported to it.

It is worth noting that the bootstrap runtime was originally intended to be
*the* runtime. As such it contains a number of complexities introduced for the
sake of performance that are at this point unwarranted (eg, the ESBA data
structure).

The `org.ava-lang.avast` directory within the `bootstrap-runtime` directory is
a sufficient subset of the Avast library for the real compiler to work.

## contrib

The `contrib` directory contains libraries that for one reason or another are
vendored into the Avalanche build tree. Modifications to the original files are
kept to a minimum where possible.

Naturally, different contrib libraries have varying copyright and licensing
terms.

### contrib/libbacktrace

Provides backtrace support for exceptions. This was originally a GCC component.
The vendored version here is derived from the fork that lives within the Rust
repository.

## platform-native

The `platform-native` directory contains runtime support for native targets.
The code here is written in C since it would be difficult or impossible to
properly write it in Avalanche.

Authoratative documentation on the native platform is also found here.

Note that this does _not_ contain and part of the compiler or code generator
for native targets.

The `src/avalanche` subdirectory contains the headers which represent the
native runtime's public API. This is mostly limited to routines invoked by
generated code and accessors for various metrics. Headers elsewhere under `src`
define private APIs.
