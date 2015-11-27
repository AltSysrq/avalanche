#! /bin/sh

CC=gcc5
CXX=clang++37
CLANG=clang37
CLANGXX=clang++37
COMMON_FLAGS='-Wall -Wextra -Wno-long-long -Wno-unused-parameter -g3'
CFLAGS="$COMMON_FLAGS -Wno-clobbered  -fdiagnostics-color=never -fno-diagnostics-show-caret"
CXXFLAGS="$COMMON_FLAGS -fno-caret-diagnostics -fno-diagnostics-fixit-info -fno-color-diagnostics"
CPPFLAGS='-I/usr/local/include'
LDFLAGS='-L/usr/local/lib -L/usr/local/lib/gcc5'
CXXLDFLAGS="$LDFLAGS"
# Need to disable debug info from gas or it conflicts with what LLVM outputs
CCASFLAGS="-g0"

export CC CXX CFLAGS CXXFLAGS CPPFLAGS LDFLAGS CXXLDFLAGS CCASFLAGS CLANG CLANGXX

exec ./configure
