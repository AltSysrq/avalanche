#! /usr/bin/env python2.7

import sys

list_sz = int(sys.argv[1])
sum_list = int(sys.argv[2])

def build_list(n):
    l = []
    i = 0
    while i < n:
        l.append(i)
        i += 1
    return l

def do_sum_list(l):
    sum = 0
    for n in l:
        sum += n
    return sum

l = build_list(list_sz)
if 0 != sum_list:
    do_sum_list(l)
