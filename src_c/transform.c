/* flowjs transform -- ES6+ to ES5 AST transformations */
#include "flowjs.h"
#include <string.h>

const fjs_transform_opts FJS_TRANSFORM_DEFAULTS = {
  .modules = true,
  .arrows = true,
  .let_const = true,
  .templates = true,
  .optional_chaining = true,
  .esm_to_cjs = true,
  .emit_helpers = true,
};

fjs_error fjs_transform(fjs_ast *root, fjs_transform_opts opts) {
  (void)root; (void)opts;
  return FJS_ERROR_OK;
}