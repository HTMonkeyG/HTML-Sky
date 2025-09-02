#ifndef __SETUP_H__
#define __SETUP_H__

#include "aliases.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
  SKYRE_SETUP_DIRECT = 0,
  SKYRE_SETUP_E8 = 1,
  SKYRE_SETUP_FF15 = 2
};

typedef struct {
  const char *sig;
  const char *name;
  i08 indirect;
  i32 offset;
} Signature_t;

typedef struct {
  Signature_t sig;
  void *fn;
} FuncSig_t;

i08 skyre_setupFuncWithSig(const Signature_t **sigcodes, void **functions, i32 num);
i08 skyre_createHookWithSig(FuncSig_t *func, void *detour);

#ifdef __cplusplus
}
#endif

#endif
