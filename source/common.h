#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#define CHECK_STRUCT_SIZE(X, Y) _Static_assert(sizeof(X) == Y, "sizeof(" #X ") is incorrect! (should be " #Y ")")

#define errorf(fmt, ...) { fprintf(stderr, "%s(): " fmt "\n", __FUNCTION__, ##__VA_ARGS__); }
#define print_error(func, ret, ...) errorf(func " failed (ret = %i)", ##__VA_ARGS__, ret)
#define memalign32(sz) aligned_alloc(0x20, __builtin_align_up(sz, 0x20));

