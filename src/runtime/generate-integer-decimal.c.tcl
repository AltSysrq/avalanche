#! /usr/bin/env tclsh8.6
#-
# Copyright (c) 2015, Jason Lingle
#
# Permission to  use, copy,  modify, and/or distribute  this software  for any
# purpose  with or  without fee  is hereby  granted, provided  that the  above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE  IS PROVIDED "AS  IS" AND  THE AUTHOR DISCLAIMS  ALL WARRANTIES
# WITH  REGARD   TO  THIS  SOFTWARE   INCLUDING  ALL  IMPLIED   WARRANTIES  OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT  SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL,  DIRECT,   INDIRECT,  OR  CONSEQUENTIAL  DAMAGES   OR  ANY  DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
# OF  CONTRACT, NEGLIGENCE  OR OTHER  TORTIOUS ACTION,  ARISING OUT  OF OR  IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

puts "/* This file was auto-generated from generate-integer-decimal.c.tcl;"
puts " * do not edit directly."
puts " */"
puts "#define AVA__INTERNAL_INCLUDE 1"
puts "#include \"avalanche/defs.h\""
puts "#include \"-integer-decimal.h\""
puts ""

puts "const ava_integer_decimal_entry ava_integer_decimal_table\[10000\] = {"
for {set i 0} {$i < 10000} {incr i} {
  scan [format %04d $i] %c%c%c%c a b c d
  if {$i >= 1000} {
    set digits 4
  } elseif {$i >= 100} {
    set digits 3
  } elseif {$i >= 10} {
    set digits 2
  } else {
    set digits 1
  }
  puts [format "  { .digits = %d, .value = { .c = { %d, %d, %d, %d } } }," \
            $digits $a $b $c $d]
}
puts "};"
