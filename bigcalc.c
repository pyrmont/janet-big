#include <janet.h>
#include <tommath.h>

static int bigcalc_int_get(void *p, Janet key, Janet *out);

static int bigcalc_int_gc(void *p, size_t len) {
  (void)len;
  mp_int *b = (mp_int *)p;
  mp_clear(b);
  return 0;
}

static void bigcalc_int_to_string(void *p, JanetBuffer *buf) {
  mp_int *b = (mp_int *)p;
  int sz;
  if (mp_radix_size(b, 10, &sz) != MP_OKAY)
    janet_panic("unable to format string");

  janet_buffer_extra(buf, sz);

  if (mp_toradix_n(b, buf->data + buf->count, 10, sz) != MP_OKAY)
    janet_panic("unable to format string");

  buf->count += (sz - 1);
}

static int32_t bigcalc_int_hash(void *p, size_t size) {
  mp_int *b = (mp_int *)p;
  uint32_t hash = 5381;
  for (size_t i = 0; i < USED(b); i++) {
    hash = (hash << 5) + hash + ((char)DIGIT(b, i));
  }
  return (int32_t)hash;
}

static int bigcalc_int_compare(void *p1, void *p2) {
  mp_int *x = (mp_int *)p1;
  mp_int *y = (mp_int *)p2;
  return mp_cmp(x, y);
}

static const JanetAbstractType bigcalc_int_type = {
    "bigcalc/int",
    bigcalc_int_gc,
    NULL,
    bigcalc_int_get,
    NULL,
    NULL, // bigcalc_int_marshal,
    NULL, // bigcalc_int_unmarshal,
    bigcalc_int_to_string,
    bigcalc_int_compare,
    bigcalc_int_hash,
};

static mp_int *bigcalc_coerce_janet_to_int(Janet *argv, int i) {
  mp_err err;

  if (janet_checkabstract(argv[i], &bigcalc_int_type))
    return (mp_int *)janet_unwrap_abstract(argv[i]);

  mp_int *b = janet_abstract(&bigcalc_int_type, sizeof(mp_int));
  if (mp_init(b) != MP_OKAY)
    abort();

  switch (janet_type(argv[i])) {
  case JANET_NUMBER:
    if ((err = mp_set_double(b, janet_unwrap_number(argv[i]))) != MP_OKAY)
      janet_panicf("unable to coerce int: %s", mp_error_to_string(err));
    break;
  case JANET_STRING: {
    JanetString s = janet_unwrap_string(argv[i]);
    if ((err = mp_read_radix(b, (char *)s, 10)) != MP_OKAY)
      janet_panicf("unable to coerce int: %s", mp_error_to_string(err));
    break;
  }
  // TODO u64/s64 types.
  default:
    janet_panicf("unable to coerce slot #%d to big int", i);
    break;
  }

  janet_gcpressure(b->alloc);
  return b;
}

static Janet bigcalc_int(int32_t argc, Janet *argv) {
  mp_err err;
  janet_fixarity(argc, 1);

  mp_int *b = janet_abstract(&bigcalc_int_type, sizeof(mp_int));
  if (mp_init(b) != MP_OKAY)
    abort(); /* Simpler to ignore out of memory in this code path for now. */

  switch (janet_type(argv[0])) {
  case JANET_NUMBER:
    if ((err = mp_set_double(b, janet_unwrap_number(argv[0]))) != MP_OKAY)
      janet_panicf("%s", mp_error_to_string(err));
    break;
  case JANET_STRING: {
    JanetString s = janet_unwrap_string(argv[0]);
    if ((err = mp_read_radix(b, (char *)s, 10)) != MP_OKAY)
      janet_panicf("%s", mp_error_to_string(err));
    break;
  }
  // TODO u64/s64 types.
  default:
    // XXX print type properly.
    janet_panic("unable to initialize big int from provided type");
    break;
  }
  janet_gcpressure(b->alloc);
  return janet_wrap_abstract(b);
}

#define BIGINT_OPMETHOD(NAME, OP, L, R)                                        \
  static Janet bigcalc_int_##NAME(int32_t argc, Janet *argv) {                 \
    mp_err err;                                                                \
    janet_fixarity(argc, 2);                                                   \
    mp_int *c = janet_abstract(&bigcalc_int_type, sizeof(mp_int));             \
    if (mp_init(c) != MP_OKAY)                                                 \
      abort();                                                                 \
    mp_int *L = (mp_int *)janet_getabstract(argv, 0, &bigcalc_int_type);       \
    mp_int *R = bigcalc_coerce_janet_to_int(argv, 1);                          \
    if ((err = mp_##OP(a, b, c)) != MP_OKAY)                                   \
      janet_panicf("%s", mp_error_to_string(err));                             \
    janet_gcpressure(c->alloc);                                                \
    return janet_wrap_abstract(c);                                             \
  }

#define BIGINT_DIVMODMETHOD(NAME, L, R, RET)                                   \
  static Janet bigcalc_int_##NAME(int32_t argc, Janet *argv) {                 \
    mp_err err;                                                                \
    janet_fixarity(argc, 2);                                                   \
    mp_int *r = janet_abstract(&bigcalc_int_type, sizeof(mp_int));             \
    if (mp_init(r) != MP_OKAY)                                                 \
      abort();                                                                 \
    mp_int *d = janet_abstract(&bigcalc_int_type, sizeof(mp_int));             \
    if (mp_init(d) != MP_OKAY)                                                 \
      abort();                                                                 \
    mp_int *L = janet_getabstract(argv, 0, &bigcalc_int_type);                 \
    mp_int *R = bigcalc_coerce_janet_to_int(argv, 1);                          \
    if ((err = mp_div(a, b, d, r)) != MP_OKAY)                                 \
      janet_panicf("%s", mp_error_to_string(err));                             \
    janet_gcpressure(d->alloc);                                                \
    janet_gcpressure(r->alloc);                                                \
    return janet_wrap_abstract(RET);                                           \
  }

BIGINT_DIVMODMETHOD(div, a, b, d)
BIGINT_DIVMODMETHOD(mod, a, b, r)
BIGINT_DIVMODMETHOD(rdiv, b, a, d)
BIGINT_DIVMODMETHOD(rmod, b, a, r)

BIGINT_OPMETHOD(add, add, a, b)
BIGINT_OPMETHOD(sub, sub, a, b)
BIGINT_OPMETHOD(mul, mul, a, b)
BIGINT_OPMETHOD(and, and, a, b)
BIGINT_OPMETHOD(or, or, a, b)
BIGINT_OPMETHOD(xor, xor, a, b)
BIGINT_OPMETHOD(radd, add, b, a)
BIGINT_OPMETHOD(rsub, sub, b, a)
BIGINT_OPMETHOD(rmul, mul, b, a)
BIGINT_OPMETHOD(rand, and, b, a)
BIGINT_OPMETHOD(ror, or, b, a)
BIGINT_OPMETHOD(rxor, xor, b, a)

static JanetMethod bigcalc_int_methods[] = {{"+", bigcalc_int_add},
                                            {"-", bigcalc_int_sub},
                                            {"*", bigcalc_int_mul},
                                            {"/", bigcalc_int_div},
                                            {"%", bigcalc_int_mod},
                                            {"&", bigcalc_int_and},
                                            {"|", bigcalc_int_or},
                                            {"^", bigcalc_int_xor},
                                            {"r+", bigcalc_int_radd},
                                            {"r-", bigcalc_int_rsub},
                                            {"r*", bigcalc_int_rmul},
                                            {"r/", bigcalc_int_rdiv},
                                            {"r%", bigcalc_int_rmod},
                                            {"r&", bigcalc_int_rand},
                                            {"r|", bigcalc_int_ror},
                                            {"r^", bigcalc_int_rxor},
                                            //{"<<", bigcalc_int_lshift},
                                            //{">>", bigcalc_int_rshift},
                                            {NULL, NULL}};

static int bigcalc_int_get(void *p, Janet key, Janet *out) {
  (void)p;
  if (!janet_checktype(key, JANET_KEYWORD))
    return 0;
  return janet_getmethod(janet_unwrap_keyword(key), bigcalc_int_methods, out);
}

static const JanetReg cfuns[] = {{"int", bigcalc_int,
                                  "(bigcalc/int &opt v)\n\n"
                                  "Create a new integer."},
                                 {NULL, NULL, NULL}};

JANET_MODULE_ENTRY(JanetTable *env) { janet_cfuns(env, "bigcalc", cfuns); }
