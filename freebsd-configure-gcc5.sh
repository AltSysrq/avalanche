#! /bin/sh

CC=gcc5
# TODO: Figure out how to get this to work correctly with g++5
# For some reason it doesn't seem to link against the correct C++ stdlib.
CXX=clang++
COMMON_FLAGS='-Wall -Wextra -Wno-long-long -Wno-unused-parameter -g3'
CFLAGS="$COMMON_FLAGS -Wno-clobbered  -fdiagnostics-color=never -fno-diagnostics-show-caret"
CXXFLAGS="$COMMON_FLAGS"
CPPFLAGS='-I/usr/local/include'
LDFLAGS='-L/usr/local/lib -L/usr/local/lib/gcc5'
CXXLDFLAGS="$LDFLAGS"

export CC CXX CFLAGS CXXFLAGS CPPFLAGS LDFLAGS CXXLDFLAGS

exec ./configure
