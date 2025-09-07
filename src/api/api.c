#include "globals.h"

void HTGetGameStatus(HTGameStatus *status) {
  if (status)
    *status = gGameStatus;
}
