/* flowjs arena allocator -- shared across compiler modules */
#include "flowjs.h"
#include <stdlib.h>
#include <string.h>

#define FJS_ABLOCK_SZ 16384

typedef struct fjs_ablock {
  struct fjs_ablock *next;
  size_t used;
  unsigned char data[FJS_ABLOCK_SZ];
} fjs_ablock;

struct fjs_arena {
  fjs_ablock *head, *tail;
};

fjs_arena* fjs_arena_new(void) {
  fjs_arena *a = (fjs_arena *)malloc(sizeof(fjs_arena));
  if (!a) return NULL;
  a->head = a->tail = (fjs_ablock *)malloc(sizeof(fjs_ablock)); if (a->head) { a->head->next = NULL; a->head->used = 0; }
  return a;
}

void* fjs_arena_alloc(fjs_arena *a, size_t sz) {
  if (!a || !a->tail) return NULL;
  sz = (sz + 7) & ~(size_t)7;
  fjs_ablock *b = a->tail;
  if (b->used + sz > FJS_ABLOCK_SZ) {
    fjs_ablock *n = (fjs_ablock *)malloc(sizeof(fjs_ablock)); if (n) { n->next = NULL; n->used = 0; }
    if (!n) return NULL;
    b->next = n;
    a->tail = n;
    b = n;
  }
  void *p = b->data + b->used;
  b->used += sz;
  return p;
}

void fjs_arena_free(fjs_arena *a) {
  if (!a) return;
  fjs_ablock *b = a->head;
  while (b) { fjs_ablock *t = b; b = b->next; free(t); }
  free(a);
}

void fjs_ab_init(fjs_ab *b) {
  b->arena = fjs_arena_new();
  b->err = FJS_ERROR_OK;
}

fjs_ast* fjs_make_ast(fjs_ab *b, fjs_ast_kind kind) {
  fjs_ast *n = (fjs_ast *)fjs_arena_alloc(b->arena, sizeof(fjs_ast));
  if (!n) { b->err.kind = FJS_ERR_OOM; strcpy(b->err.message, "OOM"); return NULL; }
  *n = (fjs_ast){.kind = kind};
  return n;
}

static fjs_ast_list_node* alloc_node(fjs_ab *b) {
  return (fjs_ast_list_node *)fjs_arena_alloc(b->arena, sizeof(fjs_ast_list_node));
}

void fjs_list_push(fjs_ab *b, fjs_ast_list *list, fjs_ast *item) {
  if (!item) return;
  fjs_ast_list_node *n = alloc_node(b);
  if (!n) return;
  n->item = item;
  n->next = NULL;
  if (list->tail) list->tail->next = n;
  else list->head = n;
  list->tail = n;
  list->count++;
}

void fjs_list_push_front(fjs_ab *b, fjs_ast_list *list, fjs_ast *item) {
  if (!item) return;
  fjs_ast_list_node *n = alloc_node(b);
  if (!n) return;
  n->item = item;
  n->next = list->head;
  list->head = n;
  if (!list->tail) list->tail = n;
  list->count++;
}