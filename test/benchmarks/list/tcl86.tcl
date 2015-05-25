#! /usr/bin/env tclsh8.6

proc build_list {n} {
  set l {}
  for {set i 0} {$i < $n} {incr i} {
    lappend l $i
  }
  return $l
}

proc sum_list {l} {
  set sum 0
  foreach n $l {
    incr sum $n
  }
  return $sum
}

proc main {list_sz sum_list} {
  set l [build_list $list_sz]
  if {$sum_list} {
    sum_list $l
  }
}

main {*}$argv
