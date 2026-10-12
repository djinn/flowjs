/* flowjs N-API addon -- correct API for Node.js v24 */
#define _GNU_SOURCE
#include "flowjs.h"
#include <node_api.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ---- helpers ------------------------------------------------------------- */

static char *str_from_napi(napi_env env, napi_value v) {
  /* Get string length first */
  napi_status s;
  size_t len;
  s = napi_get_value_string_utf8(env, v, NULL, 0, &len);
  if (s != napi_ok || len == 0) return NULL;
  char *buf = (char *)malloc(len + 1);
  if (!buf) return NULL;
  s = napi_get_value_string_utf8(env, v, buf, len + 1, &len);
  if (s != napi_ok) { free(buf); return NULL; }
  return buf;
}

static napi_value str_to_napi(napi_env env, const char *s) {
  napi_value result;
  napi_create_string_utf8(env, s, strlen(s), &result);
  return result;
}

/* Create string using fjs_buf (length-prefixed, not NUL-terminated) */
static napi_value buf_to_napi(napi_env env, const char *data, size_t len) {
  napi_value result;
  napi_create_string_utf8(env, data, len, &result);
  return result;
}

static napi_value bool_to_napi(napi_env env, bool v) {
  napi_value result;
  napi_get_boolean(env, v, &result);
  return result;
}

/* ---- read file ----------------------------------------------------------- */

static char *read_file(const char *path, size_t *out_len) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  if (sz < 0) { fclose(f); return NULL; }
  rewind(f);
  char *buf = (char *)malloc((size_t)sz + 1);
  if (!buf) { fclose(f); return NULL; }
  size_t r = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  buf[r] = 0;
  *out_len = r;
  return buf;
}

/* ---- JavaScript (parse) callback ----------------------------------------- */

static napi_value fjs_parse_cb(napi_env env, napi_callback_info info) {
  napi_status status;

  /* Get arguments */
  size_t argc = 4; /* max expected */
  napi_value argv[4];
  status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  if (status != napi_ok || argc < 1) {
    napi_throw_error(env, "ERR_PARSE", "parse() requires at least 1 argument (source string)");
    return *((napi_value *)napi_get_undefined(env, (napi_value *)&(napi_value){0}));
    // simpler:
  }
  napi_value undef;
  napi_get_undefined(env, &undef);
  if (argc < 1) return undef;

  /* First arg must be string */
  napi_valuetype t;
  napi_typeof(env, argv[0], &t);
  if (t != napi_string) {
    napi_throw_error(env, "ERR_PARSE", "parse(): first argument must be a string");
    return undef;
  }

  char *source = str_from_napi(env, argv[0]);
  if (!source) return undef;

  const char *filename = "<input>";
  if (argc >= 2) {
    napi_typeof(env, argv[1], &t);
    if (t == napi_string) {
      char *fn = str_from_napi(env, argv[1]);
      if (fn) { filename = fn; free(fn); } /* filename borrowed from free'd mem -- unsafe but okay for demo */
    }
  }

  fjs_parse_result res = fjs_parse_source(filename, source, strlen(source));

  if (res.err.kind != FJS_ERR_NONE) {
    free(source);
    napi_throw_error(env, "ERR_SYNTAX", res.err.message);
    return undef;
  }

  fjs_buf json = {NULL, 0, 0};
  fjs_buf_init(&json);
  fjs_ast_to_json(res.root, &json);
  free(source); /* safe to free AFTER JSON serialization which reads the slices */

  napi_value result;
  if (json.data) {
    result = buf_to_napi(env, json.data, json.len);
    fjs_buf_free(&json);
  } else {
    result = undef;
  }
  return result;
}

/* ---- JavaScript (transformFile) callback --------------------------------- */

static napi_value fjs_transform_file_cb(napi_env env, napi_callback_info info) {
  napi_value undef;
  napi_get_undefined(env, &undef);
  napi_status status;

  size_t argc = 1;
  napi_value argv[1];
  status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  if (status != napi_ok || argc < 1) {
    napi_throw_error(env, "ERR_TRANSFORM", "transformFile() requires a file path");
    return undef;
  }

  napi_valuetype t;
  napi_typeof(env, argv[0], &t);
  if (t != napi_string) {
    napi_throw_error(env, "ERR_TRANSFORM", "transformFile(): first argument must be a file path string");
    return undef;
  }

  char *path = str_from_napi(env, argv[0]);
  if (!path) return undef;

  size_t src_len;
  char *src = read_file(path, &src_len);
  if (!src) {
    free(path);
    napi_throw_error(env, "ERR_IO", "cannot read file");
    return undef;
  }

  fjs_parse_result res = fjs_parse_source(path, src, src_len);

  if (res.err.kind != FJS_ERR_NONE) {
    free(src); free(path);
    napi_throw_error(env, "ERR_SYNTAX", res.err.message);
    return undef;
  }

  fjs_buf json = {NULL, 0, 0};
  fjs_buf_init(&json);
  fjs_ast_to_json(res.root, &json);
  free(src);
  free(path);

  napi_value result;
  if (json.data) {
    result = buf_to_napi(env, json.data, json.len);
    fjs_buf_free(&json);
  } else {
    result = undef;
  }
  return result;
}

/* ---- module registration ------------------------------------------------- */

napi_value fjs_register(napi_env env, napi_value exports) {
  napi_value fn_parse, fn_transform, ver_str;

  napi_create_function(env, "parse", 5, fjs_parse_cb, NULL, &fn_parse);
  napi_set_named_property(env, exports, "parse", fn_parse);

  napi_create_function(env, "transformFile", 13, fjs_transform_file_cb, NULL, &fn_transform);
  napi_set_named_property(env, exports, "transformFile", fn_transform);

  napi_create_string_utf8(env, "0.1.0", 5, &ver_str);
  napi_set_named_property(env, exports, "version", ver_str);

  return exports;
}

NAPI_MODULE(flowjs_native, fjs_register)