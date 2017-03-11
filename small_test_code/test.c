#include <stdio.h>
#include <openssl/sha.h>
#include <sys/stat.h>

int main () {
    unsigned char arr[10485760];
    int i, j, bytes;
    unsigned char c[SHA256_DIGEST_LENGTH];
    SHA256_CTX wf_sha256;
    SHA256_CTX s_sha256[100];
    SHA256_Init(&wf_sha256);
    for (i = 0; i < 100; i++) {
        SHA256_Init(s_sha256+i);
    }

    unsigned char data[1024];
    
    FILE* file = fopen("ffp_database.c", "rb");

    struct stat file_stat;
    stat("ffp_database.c", &file_stat);

    i = 0;
    while ((bytes = fread(data, 1, 1024, file)) != 0) {
        SHA256_Update(&wf_sha256, data, bytes);
        SHA256_Update(s_sha256 + i, data, bytes);
        SHA256_Final(c, s_sha256 + i);
        printf("section : %d\n", i);
        for (j = 0; j < SHA256_DIGEST_LENGTH; j++) {
            printf("%02X", c[j]);
        }
        printf("\n");
        i++;
    }
    SHA256_Final(c, &wf_sha256);

    printf("whole file :\n");
    for (j = 0; j < SHA256_DIGEST_LENGTH; j++) {
        printf("%02X", c[j]);
    }
    printf("\n");
}
