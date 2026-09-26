#pragma once
#include "mylib.h"

/* Names a record owned by mylib.h, so a split module has to import it. */
int mylib_extra_scaled_sum(struct mylib_point p, int scale);
