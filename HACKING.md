Project Layout
--------------

### bootstrap-runtime

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
