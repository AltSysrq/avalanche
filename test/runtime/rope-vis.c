/*-
 * Copyright (c) 2015 Jason Lingle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "runtime/avalanche.h"
#include "bsd.h"

static void make_strings(ava_string*, unsigned);
static void write_graph(ava_string*, unsigned);
static void write_tree(FILE*, ava_rope*, int is_live);
static void unmark_tree(ava_rope*);
static int is_marked(ava_rope*);
static void set_mark(ava_rope*, int);

int main(int argc, char** argv) {
  if (2 != argc)
    errx(EX_USAGE, "Usage: %s <num-iterations>", argv[0]);

  unsigned count = atoi(argv[1]);
  ava_string strings[count];
  make_strings(strings, count);

  unsigned i;
  for (i = 2; i < count; ++i)
    write_graph(strings, i);
}

static void make_strings(ava_string* dst, unsigned count) {
  char buf[128];
  ava_string base;
  unsigned i;

  memset(buf, 'a', sizeof(buf));
  base = ava_string_of_bytes(buf, sizeof(buf));

  dst[0] = AVA_EMPTY_STRING;
  for (i = 1; i < count; ++i)
    dst[i] = ava_string_concat(dst[i-1], base);
}

static void write_graph(ava_string* strings, unsigned n) {
  char filename[16];
  FILE* out;
  unsigned i;

  snprintf(filename, sizeof(filename), "%03d.dot", n);
  out = fopen(filename, "w");
  fprintf(out, "digraph {\n");

  for (i = n - 1; i > 0; --i)
    write_tree(out, (ava_rope*)strings[i].rope, i == n - 1);

  for (i = n - 1; i > 0; --i)
    unmark_tree((ava_rope*)strings[i].rope);

  fprintf(out, "}\n");
  fclose(out);
}

static void write_tree(FILE* out, ava_rope* rope, int is_live) {
  if (!rope->depth || is_marked(rope)) return;

  fprintf(out, "  \"%p\" [style=filled,fillcolor=%s,label=\"%d\"];\n",
          rope, is_live? "aquamarine" : "gold", rope->depth);

  if (rope->v.concat_left->depth) {
    fprintf(out, "  \"%p\" -> \"%p\";\n", rope, (void*)rope->v.concat_left);
  } else {
    fprintf(out, "  \"%p-lleaf\" [style=filled,fillcolor=azure,label=\"0\"];\n",
            rope);
    fprintf(out, "  \"%p\" -> \"%p-lleaf\";\n", rope, rope);
  }

  if (rope->concat_right->depth) {
    fprintf(out, "  \"%p\" -> \"%p\";\n", rope, (void*)rope->concat_right);
  } else {
    fprintf(out, "  \"%p-rleaf\" [style=filled,fillcolor=azure,label=\"0\"];\n",
            rope);
    fprintf(out, "  \"%p\" -> \"%p-rleaf\";\n", rope, rope);
  }

  set_mark(rope, 1);

  write_tree(out, (ava_rope*)rope->v.concat_left, is_live);
  write_tree(out, (ava_rope*)rope->concat_right, is_live);
}

static void unmark_tree(ava_rope* rope) {
  if (!is_marked(rope)) return;

  set_mark(rope, 0);
  unmark_tree((ava_rope*)rope->v.concat_left);
  unmark_tree((ava_rope*)rope->concat_right);
}

static void set_mark(ava_rope* rope, int mark) {
  rope->length &= 0xFFFFFF;
  rope->length |= mark << 24;
}

static int is_marked(ava_rope* rope) {
  return rope->length >> 24;
}
