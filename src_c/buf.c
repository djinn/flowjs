#include "flowjs.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

void fjs_buf_init(fjs_buf *b) { *b = (fjs_buf){NULL, 0, 0}; }

void fjs_buf_grow(fjs_buf *b, size_t extra) {
  size_t need = b->len + extra;
  if (need > b->cap) {
    size_t newcap = b->cap ? b->cap : 64;
    while (newcap < need) newcap *= 2;
    char *nd = (char *)realloc(b->data, newcap);
    if (!nd) return; /* silent OOM */
    b->data = nd;
    b->cap = newcap;
  }
}

void fjs_buf_append(fjs_buf *b, const char *data, size_t len) {
  if (len == 0) return;
  fjs_buf_grow(b, len);
  memcpy(b->data + b->len, data, len);
  b->len += len;
}

void fjs_buf_putc(fjs_buf *b, char c) {
  fjs_buf_grow(b, 1);
  b->data[b->len++] = c;
}

void fjs_buf_puts(fjs_buf *b, const char *s) {
  if (s) fjs_buf_append(b, s, strlen(s));
}

void fjs_buf_printf(fjs_buf *b, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char tmp[256];
  int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  if (n > 0) fjs_buf_append(b, tmp, (size_t)(n < (int)sizeof(tmp) ? n : sizeof(tmp)));
}

void fjs_buf_free(fjs_buf *b) { free(b->data); *b = (fjs_buf){NULL, 0, 0}; }

/* ---- number formatting ------------------------------------------------- */

void fjs_double_to_js(fjs_buf *b, double x) {
  if (x != x) { fjs_buf_puts(b, "NaN"); return; }
  if (x == 0.0) { fjs_buf_puts(b, "0"); return; }
  if (x == (double)(int64_t)x) {
    /* Integer within safe range: use exact digits */
    int64_t i = (int64_t)x;
    if (i < 0) { fjs_buf_putc(b, '-'); i = -i; }
    uint64_t u = (uint64_t)i;
    char buf[24]; int pos = 0;
    if (u == 0) buf[pos++] = '0';
    else while (u) { buf[pos++] = '0' + (u % 10); u /= 10; }
    for (int j = pos-1; j >= 0; j--) fjs_buf_putc(b, buf[j]);
    return;
  }
  char buf[64];
  int n = snprintf(buf, sizeof(buf), "%.15g", x);
  /* ensure there's a '.' or 'e' */
  bool has_dot = false, has_e = false;
  for (int i = 0; i < n; i++) { if (buf[i]=='.') has_dot=true; if (buf[i]=='e'||buf[i]=='E') has_e=true; }
  if (!has_dot && !has_e) { buf[n++] = '.'; buf[n++] = '0'; }
  fjs_buf_append(b, buf, (size_t)n);
}
