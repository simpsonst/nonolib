// -*- c-basic-offset: 2; indent-tabs-mode: nil -*-

#include <assert.h>
#include <stddef.h>

#include "nonogram.h"
#include "nonocache.h"

static const char safe_chars[] =
"0123456789"
"abcdefghijklmnopqrstuvwxyz"
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"._";

static inline int decode_char(char c)
{
  const char *p = strchr(safe_chars, c);
  if (!p) return -1;
  return p - safe_chars;
}

static int decode_len(const char **in, size_t *rem, unsigned *vp)
{
  if (*rem == 0)
    return -1;

  int skey = decode_char(0[*in]);
  if (skey < 0) return -1;
  unsigned key = skey;
  assert(key < sizeof safe_chars - 1);

  size_t req;
  unsigned r = 0;

  if (key >= 0x3eu) {
    return -1;
  } else if (key >= 0x3cu) {
    req = 4;
    r |= key & 0x1u;
  } else if (key >= 0x38u){
    req = 3;
    r |= key & 0x3u;
  } else if (key >= 0x30u){
    req = 2;
    r |= key & 0x7u;
  } else {
    req = 1;
    r |= key & 0x1fu;
  }

  if (req > *rem)
    return -1;

  for (unsigned i = 1; i < req; i++) {
    key = decode_char(i[*in]);
    if ((key & 0x30u) != 0x20u)
      return -1;
    r <<= 4;
    r |= key & 0xfu;
  }

  *in += req;
  *rem -= req;

  *vp = r;
  return 0;
}

static int encode_len(char **out, size_t *rem, unsigned v)
{
  size_t req;
  unsigned mask, mark;

  if (v > 1023) {
    req = 4;
    mask = 0x1u;
    mark = 0x3cu;
  } else if (v > 127) {
    req = 3;
    mask = 0x3u;
    mark = 0x38u;
  } else if (v > 31) {
    req = 2;
    mask = 0x7u;
    mark = 0x30u;
  } else {
    req = 1;
    mask = 0x1fu;
    mark = 0;
  }

  if (out) {
    if (*rem < req)
      return -1;
  } else {
    *rem += req;
    return 0;
  }

  while (req > 0) {
    // Which base-64 digit do we write out next?
    unsigned index = v >> (4 * (req - 1));
    index &= mask;
    index |= mark;
    assert(index < sizeof safe_chars - 1);

    // Write out the corresponding output character.
    *(*out)++ = safe_chars[index];
    --*rem;

    // Prepare for the next character.
    req--;
    mark = 0x20u;
    mask = 0xfu;
  }

  return 0;
}

int nonocache_encodepuzzle(char **out, size_t *rem, const nonogram_puzzle *puz)
{
  const size_t wid = nonogram_puzzlewidth(puz);
  const size_t hei = nonogram_puzzleheight(puz);

  for (size_t x = 0; x < wid; x++) {
    const nonogram_sizetype *const ptr = nonogram_getcoldata(puz, x);
    const size_t len = nonogram_getcollen(puz, x);
    for (size_t i = 0; i < len; i++) {
      assert(ptr[i]);
      if (encode_len(out, rem, ptr[i]) < 0)
        return -1;
    }
    if (encode_len(out, rem, 0) < 0)
      return -1;
  }

  for (size_t y = 0; y < hei; y++) {
    const nonogram_sizetype *const ptr = nonogram_getrowdata(puz, y);
    const size_t len = nonogram_getrowlen(puz, y);
    for (size_t i = 0; i < len; i++) {
      assert(ptr[i]);
      if (encode_len(out, rem, ptr[i]) < 0)
        return -1;
    }
    if (encode_len(out, rem, 0) < 0)
      return -1;
  }

  return 0;
}

int nonocache_encodecells(char **out, size_t *rem,
                          size_t wid, size_t hei, const nonogram_cell *grid)
{
  if (!out) {
    *rem += (wid * hei + 5) / 6;
    return 0;
  }

  unsigned v = 0;
  unsigned got = 0;

  for (size_t y = 0; y < hei; y++) {
    for (size_t x = 0; x < wid; x++) {
      v <<= 1;
      switch (grid[x + wid * y]) {
      case nonogram_BLANK:
      case nonogram_BOTH:
        return -1;

      case nonogram_SOLID:
        v |= 1u;
        break;
      }
      got++;

      assert(got < 7);
      if (got == 6) {
        assert(v < sizeof safe_chars - 1);
        if (*rem == 0)
          return -1;

        *(*out)++ = safe_chars[v];
        --*rem;
        v = 0;
        got = 0;
      }
    }
  }

  assert(got < 7);
  if (got > 0) {
    v <<= 6 - got;
    assert(v < sizeof safe_chars - 1);
    if (*rem == 0)
      return -1;

    *(*out)++ = safe_chars[v];
    --*rem;
  }

  return 0;
}

int nonocache_decodepuzzle(const char **in, size_t *rem, nonogram_puzzle *puz)
{
  const size_t wid = nonogram_puzzlewidth(puz);
  const size_t hei = nonogram_puzzleheight(puz);


  for (size_t x = 0; x < wid; x++) {
    unsigned len;

    do {
      if (decode_len(in, rem, &len) < 0)
        return -1;
      if (len) {
        int rc;
        if ((rc = nonogram_appendcolblock(puz, x, len)) < 0)
          return rc;
      }
    } while (len);
  }

  for (size_t y = 0; y < hei; y++) {
    unsigned len;

    do {
      if (decode_len(in, rem, &len) < 0)
        return -1;
      if (len) {
        int rc;
        if ((rc = nonogram_appendrowblock(puz, y, len)) < 0)
          return rc;
      }
    } while (len);
  }

  return 0;
}

int nonocache_decodecells(const char **in, size_t *rem,
                          size_t wid, size_t hei, nonogram_cell *grid)
{
  size_t req = (wid * hei + 5) / 6;
  if (*rem < req)
    return -1;

  size_t x = 0, y = 0;

  for (size_t i = 0; i < req; i++) {
    int c = decode_char(i[*in]);
    if (c < 0)
      return -1;
    unsigned v = c;

    for (int b = 5; b >= 0; b--) {
      grid[x + y * wid] = (v & (1u << b)) ? nonogram_SOLID : nonogram_DOT;
      x++;
      if (x == wid) {
        x = 0;
        y++;
        if (y == hei) {
          return 0;
        }
      }
    }
  }

  return 0;
}
