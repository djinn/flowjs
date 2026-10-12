#ifndef FLOWJS_LEXER_H
#define FLOWJS_LEXER_H

#include "flowjs.h"

/* Public error function (used by parser) */
void fjs_lexer_error(fjs_lexer *lx, const char *msg);

/* Resume scanning the body of a template literal after `${ expr }`.
 * Called by the parser right after it consumed the closing '}' of a
 * substitution.  Returns the continuation token (FJS_TK_TEMPLATE or
 * FJS_TK_TEMPLATE_TAIL) and leaves it in lx->tok. */
fjs_tk_kind fjs_lexer_resume_template(fjs_lexer *lx);

/* True if the *previous* significant token allows a regex literal next. */
bool fjs_lexer_regex_allowed(fjs_lexer *lx);

#endif /* FLOWJS_LEXER_H */