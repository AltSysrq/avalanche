/*-
 * Copyright (c) 2015, Jason Lingle
 *
 * Permission to  use, copy,  modify, and/or distribute  this software  for any
 * purpose  with or  without fee  is hereby  granted, provided  that the  above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE  IS PROVIDED "AS  IS" AND  THE AUTHOR DISCLAIMS  ALL WARRANTIES
 * WITH  REGARD   TO  THIS  SOFTWARE   INCLUDING  ALL  IMPLIED   WARRANTIES  OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT  SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL,  DIRECT,   INDIRECT,  OR  CONSEQUENTIAL  DAMAGES   OR  ANY  DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF  CONTRACT, NEGLIGENCE  OR OTHER  TORTIOUS ACTION,  ARISING OUT  OF OR  IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "test.c"

#include <stdio.h>

#include "runtime/avalanche/defs.h"
#include "runtime/avalanche/alloc.h"
#include "runtime/avalanche/string.h"
#include "runtime/avalanche/symtab.h"

#define STR(str) ava_string_of_cstring(str)

defsuite(symtab);

static ava_symtab* root;

defsetup {
  root = ava_symtab_new(NULL);
}

defteardown { }

static ava_symbol* symbol(const char* name) {
  ava_symbol* sym = AVA_NEW(ava_symbol);
  sym->full_name = ava_string_of_cstring(name);
  return sym;
}

static const ava_symbol* lookup(const ava_symtab* symtab, const char* str) {
  const ava_symbol** results;
  size_t num_results;

  num_results = ava_symtab_get(&results, symtab, ava_string_of_cstring(str));
  ck_assert_int_lt(num_results, 2);

  return num_results? results[0] : NULL;
}

deftest(simple_lookup) {
  ava_symbol* a, * b;

  a = symbol("a");
  b = symbol("b");

  ck_assert_ptr_eq(NULL, ava_symtab_put(root, a));
  ck_assert_ptr_eq(NULL, ava_symtab_put(root, b));

  ck_assert_ptr_eq(a, lookup(root, "a"));
  ck_assert_ptr_eq(b, lookup(root, "b"));
  ck_assert_ptr_eq(NULL, lookup(root, "c"));
}

deftest(put_conflict) {
  ava_symbol* a, * b;

  a = symbol("a");
  b = symbol("a");

  ck_assert_ptr_eq(NULL, ava_symtab_put(root, a));
  ck_assert_ptr_eq(a, ava_symtab_put(root, b));
  ck_assert_ptr_eq(a, lookup(root, "a"));
}

deftest(nested_scopes) {
  ava_symtab* nested = ava_symtab_new(root);
  ava_symbol* a, * b, * c, * d;

  a = symbol("a");
  b = symbol("a");
  c = symbol("c");
  d = symbol("d");

  ck_assert_ptr_eq(NULL, ava_symtab_put(root, a));
  ck_assert_ptr_eq(NULL, ava_symtab_put(nested, b));
  ck_assert_ptr_eq(NULL, ava_symtab_put(root, c));
  ck_assert_ptr_eq(NULL, ava_symtab_put(nested, d));
  ck_assert_ptr_eq(a, lookup(root, "a"));
  ck_assert_ptr_eq(b, lookup(nested, "a"));
  ck_assert_ptr_eq(c, lookup(root, "c"));
  ck_assert_ptr_eq(c, lookup(nested, "c"));
  ck_assert_ptr_eq(NULL, lookup(root, "d"));
  ck_assert_ptr_eq(d, lookup(nested, "d"));
}

deftest(simple_import) {
  ava_string absolutised, ambiguous;
  ava_symtab* imported;
  ava_symbol* sym, * sym2;

  sym = symbol("org.ava-lang.avast:demo");
  sym2 = symbol("org.ava-lang.avast:plugh");
  ck_assert_ptr_eq(NULL, ava_symtab_put(root, sym));

  imported = ava_symtab_import(&absolutised, &ambiguous, root,
                               STR("org.ava-lang.avast:"),
                               STR("avast."),
                               ava_false, ava_true);
  ck_assert(ava_string_is_present(absolutised));
  ck_assert_int_eq(0, ava_strcmp(STR("org.ava-lang.avast:"), absolutised));
  ck_assert(!ava_string_is_present(ambiguous));

  ck_assert_ptr_eq(sym, lookup(root, "org.ava-lang.avast:demo"));
  ck_assert_ptr_eq(NULL, lookup(root, "avast.demo"));
  ck_assert_ptr_eq(sym, lookup(imported, "org.ava-lang.avast:demo"));
  ck_assert_ptr_eq(sym, lookup(imported, "avast.demo"));

  ck_assert_ptr_eq(NULL, ava_symtab_put(imported, sym2));
  ck_assert_ptr_eq(sym2, lookup(root, "org.ava-lang.avast:plugh"));
  ck_assert_ptr_eq(NULL, lookup(root, "avast.plugh"));
  ck_assert_ptr_eq(sym2, lookup(imported, "org.ava-lang.avast:plugh"));
  ck_assert_ptr_eq(sym2, lookup(imported, "avast.plugh"));
}

deftest(repeated_import_is_noop) {
  ava_symtab* imported, * again;
  ava_string abs, amb;

  imported = ava_symtab_import(&abs, &amb, root,
                               STR("foo."), AVA_EMPTY_STRING,
                               ava_false, ava_true);
  again = ava_symtab_import(&abs, &amb, imported,
                            STR("foo."), AVA_EMPTY_STRING,
                            ava_false, ava_true);
  ck_assert_ptr_eq(imported, again);
}

deftest(relative_import) {
  ava_symbol* sym;
  ava_symtab* imported, * sub;
  ava_string abs, amb;

  sym = symbol("foo.bar.baz");

  ava_symtab_put(root, sym);
  imported = ava_symtab_import(&abs, &amb, root,
                               STR("foo."), AVA_EMPTY_STRING,
                               ava_false, ava_true);
  ck_assert(ava_string_is_present(abs));
  ck_assert_int_eq(0, ava_strcmp(abs, STR("foo.")));
  ck_assert(!ava_string_is_present(amb));

  sub = ava_symtab_import(&abs, &amb, imported,
                          STR("bar."), AVA_EMPTY_STRING,
                          ava_false, ava_true);
  ck_assert(ava_string_is_present(abs));
  ck_assert_int_eq(0, ava_strcmp(abs, STR("foo.bar.")));
  ck_assert(!ava_string_is_present(amb));

  ck_assert_ptr_eq(sym, lookup(sub, "baz"));
}

deftest(ambiguous_relative_import) {
  ava_symtab* st = root;
  ava_string abs, amb;

  ava_symtab_put(st, symbol("foo.plugh.xyzzy"));
  ava_symtab_put(st, symbol("bar.plugh.xyzzy"));

  st = ava_symtab_import(&abs, &amb, st,
                         STR("foo."), AVA_EMPTY_STRING,
                         ava_true, ava_true);
  st = ava_symtab_import(&abs, &amb, st,
                         STR("bar."), AVA_EMPTY_STRING,
                         ava_true, ava_true);
  st = ava_symtab_import(&abs, &amb, st,
                         STR("plugh."), AVA_EMPTY_STRING,
                         ava_false, ava_true);

  ck_assert(ava_string_is_present(abs));
  ck_assert(ava_string_is_present(amb));
  ck_assert(!ava_strcmp(abs, STR("foo.plugh.")) ||
            !ava_strcmp(abs, STR("bar.plugh.")));
  ck_assert(!ava_strcmp(amb, STR("foo.plugh.")) ||
            !ava_strcmp(amb, STR("bar.plugh.")));
  ck_assert_int_ne(0, ava_strcmp(abs, amb));
}

deftest(absolute_import_not_absolutised) {
  ava_symtab* st = root;
  ava_string abs, amb;
  ava_symbol* s;

  ava_symtab_put(st, symbol("foo.plugh.xyzzy"));
  st = ava_symtab_import(&abs, &amb, st,
                         STR("foo."), AVA_EMPTY_STRING,
                         ava_false, ava_false);
  st = ava_symtab_import(&abs, &amb, st,
                         STR("plugh."), AVA_EMPTY_STRING,
                         ava_true, ava_false);
  s = symbol("plugh.bar");
  ck_assert_ptr_eq(NULL, ava_symtab_put(st, s));
  ck_assert_ptr_eq(s, lookup(st, "bar"));
  ck_assert_ptr_eq(NULL, lookup(st, "xyzzy"));
}

deftest(ambiguous_lookup) {
  const ava_symbol** results;
  size_t num_results;
  ava_symbol* a, * b;
  ava_string abs, amb;
  ava_symtab* st = root;

  st = ava_symtab_import(&abs, &amb, st,
                         STR("foo."), AVA_EMPTY_STRING,
                         ava_true, ava_true);
  st = ava_symtab_import(&abs, &amb, st,
                         STR("bar."), AVA_EMPTY_STRING,
                         ava_true, ava_true);
  a = symbol("foo.plugh");
  b = symbol("bar.plugh");
  ava_symtab_put(st, a);
  ava_symtab_put(st, b);

  num_results = ava_symtab_get(&results, st, STR("plugh"));
  ck_assert_int_eq(2, num_results);
  ck_assert(a == results[0] || a == results[1]);
  ck_assert(b == results[0] || b == results[1]);
}

deftest(ambiguous_lookup_static_overflow) {
  char buf[32];
  const ava_symbol** results;
  size_t num_results;
  ava_symbol* syms[32];
  ava_string abs, amb;
  ava_symtab* st = root;
  ava_uint i, j;
  ava_uint found[32];

  for (i = 0; i < 32; ++i) {
    snprintf(buf, sizeof(buf), "%d.foo", i);
    syms[i] = symbol(buf);
    ck_assert_ptr_eq(NULL, ava_symtab_put(st, syms[i]));
    snprintf(buf, sizeof(buf), "%d.", i);
    st = ava_symtab_import(&abs, &amb, st,
                           STR(buf), AVA_EMPTY_STRING,
                           ava_true, ava_false);
  }

  num_results = ava_symtab_get(&results, st, STR("foo"));
  ck_assert_int_eq(32, num_results);

  memset(found, 0, sizeof(found));
  for (i = 0; i < 32; ++i) {
    for (j = 0; j < 32; ++j) {
      if (results[j] == syms[i])
        ++found[i];
    }
  }

  for (i = 0; i < 32; ++i)
    ck_assert_int_eq(1, found[i]);
}

deftest(local_fqn_supercedes_imported) {
  ava_symbol* global, * local;
  ava_symtab* st = root;
  ava_string abs, amb;

  global = symbol("foo.bar");
  local = symbol("bar");

  ck_assert_ptr_eq(NULL, ava_symtab_put(st, global));
  ck_assert_ptr_eq(NULL, ava_symtab_put(st, local));
  st = ava_symtab_import(&abs, &amb, st,
                         STR("foo."), AVA_EMPTY_STRING,
                         ava_true, ava_true);

  ck_assert_ptr_eq(local, lookup(st, "bar"));
}

deftest(strong_import_supercedes_weak) {
  ava_symbol* strong, * weak;
  ava_symtab* st = root;
  ava_string abs, amb;

  strong = symbol("strong.bar");
  weak = symbol("weak.bar");

  ck_assert_ptr_eq(NULL, ava_symtab_put(st, strong));
  ck_assert_ptr_eq(NULL, ava_symtab_put(st, weak));
  st = ava_symtab_import(&abs, &amb, st,
                         STR("strong."), AVA_EMPTY_STRING,
                         ava_true, ava_true);
  st = ava_symtab_import(&abs, &amb, st,
                         STR("weak."), AVA_EMPTY_STRING,
                         ava_true, ava_false);

  ck_assert_ptr_eq(strong, lookup(st, "bar"));
}

deftest(inherited_absolute_supercedes_local_import) {
  ava_symbol* inherited, * local;
  ava_symtab* st = root;
  ava_string abs, amb;

  inherited = symbol("foo");
  local = symbol("local.foo");

  ck_assert_ptr_eq(NULL, ava_symtab_put(st, inherited));
  ck_assert_ptr_eq(NULL, ava_symtab_put(st, local));
  st = ava_symtab_new(st);
  st = ava_symtab_import(&abs, &amb, st,
                         STR("local."), AVA_EMPTY_STRING,
                         ava_true, ava_false);

  ck_assert_ptr_eq(inherited, lookup(st, "foo"));
}

deftest(local_import_supercedes_inherited_import) {
  ava_symbol* inherited, * local;
  ava_symtab* st = root;
  ava_string abs, amb;

  inherited = symbol("inherited.foo");
  local = symbol("local.foo");

  ck_assert_ptr_eq(NULL, ava_symtab_put(st, inherited));
  ck_assert_ptr_eq(NULL, ava_symtab_put(st, local));
  st = ava_symtab_import(&abs, &amb, st,
                         STR("inherited."), AVA_EMPTY_STRING,
                         ava_true, ava_false);
  st = ava_symtab_new(st);
  st = ava_symtab_import(&abs, &amb, st,
                         STR("local."), AVA_EMPTY_STRING,
                         ava_true, ava_false);

  ck_assert_ptr_eq(local, lookup(st, "foo"));
}
