#include <stdio.h>
#include <stdlib.h>

int main (void) {
    uint32_t num = 0x00000004;
    if (*((char*) &num) == 0x04) {
        printf("This machine is Little-Endian\n");
    }
    else {
        printf("This machine is Big-Endian\n");
    }

    return 0;
}
