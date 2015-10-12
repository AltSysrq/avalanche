#! /bin/sh

CC=gcc5
CXX=clang++
COMMON_FLAGS='-Wall -Wextra -Wno-long-long -Wno-unused-parameter -g3'
CFLAGS="$COMMON_FLAGS -Wno-clobbered  -fdiagnostics-color=never -fno-diagnostics-show-caret"
CXXFLAGS="$COMMON_FLAGS -fno-caret-diagnostics -fno-diagnostics-fixit-info -fno-color-diagnostics"
CPPFLAGS='-I/usr/local/include'
LDFLAGS='-L/usr/local/lib -L/usr/local/lib/gcc5'
CXXLDFLAGS="$LDFLAGS"

export CC CXX CFLAGS CXXFLAGS CPPFLAGS LDFLAGS CXXLDFLAGS

exec ./configure
