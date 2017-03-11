#include "../src/ffprinter.h"

int main (void) {
    int size;

    const int sect_num = 50;

    printf("size of entry :     %10d  %8f   %6f\n", sizeof(linked_entry), sizeof(linked_entry) / 1024.0, sizeof(linked_entry) / 1024.0 / 1024.0);
    printf("size of file data : %10d  %8f   %6f\n", sizeof(file_data), sizeof(file_data) / 1024.0, sizeof(file_data) / 1024.0 / 1024.0);
    printf("size of section :   %10d  %8f   %6f\n", sizeof(section), sizeof(section) / 1024.0, sizeof(section) / 1024.0 / 1024.0);

    printf("size of checksum result : %10d\n", sizeof(checksum_result));
    printf("size of extract sample :  %10d\n", sizeof(extract_sample));

    size = sizeof(linked_entry) + sizeof(file_data) + sizeof(section) * sect_num;

    printf("e + fd + 50 * s = %10d  %8f %6f\n", size, size / 1024.0, size / 1024.0 / 1024.0);

    printf("1 GiB can store %d of such entry\n", 1024 * 1024 * 1024 / size);

    return 0;
}
