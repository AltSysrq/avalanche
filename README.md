Avalanche
=========

Introduction
------------

Avalanche is a minimalist, compiled language strongly inspired by Tcl. It is
not intended to be terribly fast, nor to be memory-safe. Its main strengths are
its compact and flexible syntax, ease of string handling, and trivial binding
to C functions.

Status: Basically abandoned. This was originally supposed to be a really simple
project with which to learn Rust, but ended up not involving it at all. The
bootstrap runtime implements a usable language, though it's not terribly
performant and doesn't have anything resembling good tooling. Over time, the
scope of the project expanded to be more performance focussed, peaking at the
new, exotic ABI, green threading, and garbage collector. While this made things
more interesting programming-wise, I realised I was now working on a
programming language I wouldn't use for anything (not usable for the core of a
game due to GC, not usable as an embedded language due to exotic ABI/GC) and
therefore stopped working on it.

See [the spec](SPEC.md) for a description of how the language works.

Examples
--------

### Hello World

```
  puts "hello world"
```
