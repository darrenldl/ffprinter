#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

int main (void) {
    char path[] = "../src/";

    struct stat buf;

    if (stat(path, &buf)) {
        printf("failed to get stat\n");
    }
    else {
    }

    return 0;
}
