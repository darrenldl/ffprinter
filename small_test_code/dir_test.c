#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>

int main (void) {
    char buf[1024];
    DIR* dirp = opendir(".");
    struct dirent* dp;

    while ((dp = readdir(dirp))) {
        printf("found : %s\n", dp->d_name);
    }

    closedir(dirp);

    if (getcwd(buf, sizeof(buf)) == NULL) {
        printf("failed to get current directory\n");
    }
    else {
        printf("current directory : %s\n", buf);
    }

    return 0;
}
