#! /bin/sh

set -e

find ava-tests -name \*.avam -delete
find ava-tests -name \*.ava -not -name \*_\* | \
     xargs -I% ../src/bootstrap/bin/compile-module-std %
../src/bootstrap/bin/link-package `find ava-tests -name \*.avam`
../src/bootstrap/bin/to-asm \
    ../src/runtime/-llvm-support/drivers/isa-unchecked.bc \
    ../src/runtime/-llvm-support/drivers/avast-unchecked.bc \
    ava-tests.avap >.ava-tests.S
mv .ava-tests.S ava-tests.S
