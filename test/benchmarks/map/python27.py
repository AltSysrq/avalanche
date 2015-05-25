#! /usr/bin/env python2.7

import sys

map_sz = int(sys.argv[1])
sum_map = int(sys.argv[2])

def build_map(n):
    m = {}
    i = 0
    while i < n:
        m[i] = i+1
        i += 1
    return m

def do_sum_map(m,n):
    sum = 0
    i = 0
    while i < n:
        sum += m[i]
        i += 1
    return sum

l = build_map(map_sz)
if 0 != sum_map:
    do_sum_map(l, map_sz)
