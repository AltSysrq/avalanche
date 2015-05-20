#! /usr/bin/env tclsh8.6
#-
# Copyright (c) 2015 Jason Lingle
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

set ATTR 1
set ULONG 2

puts {
/* This file was generated by generate-esba-list-swizzle.inc.tcl.
 * Do not edit directly.
 */

typedef void (*ava_esba_list_swizzle_down_f)(
  void*restrict dst, const ava_value*restrict src);
typedef void (*ava_esba_list_swizzle_up_f)(
  ava_value*restrict dst, const ava_value*restrict template,
  const void*restrict src);
}

puts ""
puts "#define POLYMORPH_ATTR $ATTR"
puts "#define POLYMORPH_ULONG $ULONG"
puts "#define POLYMORPH_ALL ($ATTR|$ULONG)"
puts ""

for {set i 0} {$i < 4} {incr i} {
  set attr [expr {$i & $ATTR}]
  set ulong [expr {$i & $ULONG}]

  puts "typedef struct {"
  if {$attr} {
    puts "  const ava_attribute*restrict attr;"
  }
  if {$ulong} {
    puts "  ava_ulong ulong;"
  }
  puts "} ava_esba_list_swizzled_$i;"
  puts ""

  puts "static void ava_esba_list_swizzle_down_${i}("
  puts "  ava_esba_list_swizzled_$i*restrict dst,"
  puts "  const ava_value*restrict src"
  puts ") {"
  foreach field {attr ulong} {
    if {[set $field]} {
      puts "  dst->$field = ava_value_${field}(*src);"
    }
  }
  puts "}"
  puts ""

  puts "static void ava_esba_list_swizzle_up_${i}("
  puts "  ava_value*restrict dst,"
  puts "  const ava_value*restrict template,"
  puts "  const ava_esba_list_swizzled_$i*restrict src"
  puts ") {"
  puts "  const ava_attribute*restrict attr;"
  puts "  ava_ulong ulong;"
  foreach field {attr ulong} {
    if {[set $field]} {
      puts "  $field = src->$field;"
    } else {
      puts "  $field = ava_value_${field}(*template);"
    }
  }
  puts "  *dst = ava_value_with_ulong(attr, ulong);"
  puts "}"
  puts ""
}

puts "static const size_t ava_esba_list_element_size_pointers\[4\] = {"
for {set i 0} {$i < 4} {incr i} {
  puts "  sizeof(ava_esba_list_swizzled_$i) / sizeof(void*),"
}
puts "};"
puts ""

puts "static const ava_esba_list_swizzle_down_f"
puts "ava_esba_list_swizzle_down\[4\] = {"
for {set i 0} {$i < 4} {incr i} {
  puts "  (ava_esba_list_swizzle_down_f)ava_esba_list_swizzle_down_$i,"
}
puts "};"
puts ""

puts "static const ava_esba_list_swizzle_up_f"
puts "ava_esba_list_swizzle_up\[4\] = {"
for {set i 0} {$i < 4} {incr i} {
  puts "  (ava_esba_list_swizzle_up_f)ava_esba_list_swizzle_up_$i,"
}
puts "};"
puts ""

