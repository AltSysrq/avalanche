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

static void make_strings(ava_string*, unsigned, unsigned);
static void write_graph(ava_string*, unsigned, unsigned);
static void write_tree(FILE*, ava_rope*, int is_live);
static void unmark_tree(ava_rope*);
static int is_marked(ava_rope*);
static void set_mark(ava_rope*, int);

int main(int argc, char** argv) {
  if (3 != argc)
    errx(EX_USAGE, "Usage: %s <num-iterations> <stride>", argv[0]);

  srand(0);

  unsigned count = atoi(argv[1]);
  unsigned stride = atoi(argv[2]);
  ava_string strings[count*count];
  memset(strings, 0, sizeof(strings));
  make_strings(strings, count, stride);

  unsigned i;
  for (i = 0; i < count; ++i)
    write_graph(strings, count, i);
}

static void make_strings(ava_string* dst, unsigned count, unsigned stride) {
  char buf[128];
  unsigned i, t, merge;

  memset(buf, 'a', sizeof(buf));

  for (i = 0; i < count; ++i)
    dst[i] = ava_string_of_bytes(buf, sizeof(buf));

  for (t = 1; t < count; ++t) {
    merge = rand() % (stride < count - t? stride : count - t);
    for (i = 0; i < merge; ++i)
      dst[t*count + i] = dst[(t-1)*count + i];

    dst[t*count + merge] = ava_string_concat(dst[(t-1)*count + merge],
                                             dst[(t-1)*count + merge+1]);

    for (i = merge+1; i < count - t; ++i)
      dst[t*count + i] = dst[(t-1)*count + i+1];
  }
}

static void write_graph(ava_string* strings, unsigned count, unsigned tmax) {
  char filename[16];
  FILE* out;
  unsigned i, t;

  snprintf(filename, sizeof(filename), "%03d.dot", tmax);
  out = fopen(filename, "w");
  fprintf(out, "digraph {\n");

  for (t = tmax; t <= tmax; --t) {
    for (i = 0; i < count; ++i) {
      if (strings[t*count + i].rope)
        write_tree(out, (ava_rope*)strings[t*count + i].rope, t == tmax);
    }
  }

  for (t = 0; t <= tmax; ++t)
    for (i = 0; i < count; ++i)
      if (strings[t*count + i].rope)
        unmark_tree((ava_rope*)strings[t*count + i].rope);

  fprintf(out, "}\n");
  fclose(out);
}

static void write_tree(FILE* out, ava_rope* rope, int is_live) {
  if (is_marked(rope)) return;

  fprintf(out, "  \"%p\" [style=filled,fillcolor=%s,label=\"%d\"];\n",
          rope, 0 == rope->depth? "azure" : is_live? "aquamarine" : "gold",
          rope->depth);

  if (rope->depth) {
    fprintf(out, "  \"%p\" -> \"%p\";\n", rope, (void*)rope->v.concat_left);
    fprintf(out, "  \"%p\" -> \"%p\";\n", rope, (void*)rope->concat_right);
  }

  set_mark(rope, 1);

  if (rope->depth) {
    write_tree(out, (ava_rope*)rope->v.concat_left, is_live);
    write_tree(out, (ava_rope*)rope->concat_right, is_live);
  }
}

static void unmark_tree(ava_rope* rope) {
  if (!is_marked(rope)) return;

  set_mark(rope, 0);
  if (rope->depth) {
    unmark_tree((ava_rope*)rope->v.concat_left);
    unmark_tree((ava_rope*)rope->concat_right);
  }
}

static void set_mark(ava_rope* rope, int mark) {
  rope->length &= 0xFFFFFF;
  rope->length |= mark << 24;
}

static int is_marked(ava_rope* rope) {
  return rope->length >> 24;
}
