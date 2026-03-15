#include <stddef.h>
#include <stdio.h>

// Some toolchains used with Pico SDK for RP2350 miss retarget lock symbols
// required by newlib's setvbuf implementation. The BTstack demo only uses
// setvbuf to disable buffering, so a no-op shim is sufficient here.
int picokey_setvbuf(FILE *stream, char *buffer, int mode, size_t size)
{
    (void)stream;
    (void)buffer;
    (void)mode;
    (void)size;
    return 0;
}
