#! /usr/bin/env tclsh8.6

proc build_map {n} {
  set m {}
  for {set i 0} {$i < $n} {incr i} {
    dict set m $i [expr {$i+1}]
  }
  return $m
}

proc sum_map {m n} {
  set sum 0
  for {set i 0} {$i < $n} {incr i} {
    incr sum [dict get $m $i]
  }
  return $sum
}

proc main {map_sz sum_map} {
  set m [build_map $map_sz]
  if {$sum_map} {
    sum_map $m $map_sz
  }
}

main {*}$argv
