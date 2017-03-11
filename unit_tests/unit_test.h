#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <stdint.h>
#include <inttypes.h>
#include "../lib/uthash.h"

#include "../src/ffprinter_structure_template.h"

#include <limits.h>

#include <openssl/sha.h>

#include "../lib/simple_bitmap.h"

#define FREAD_ERROR         10
#define FILE_NOT_EXIST      11
#define WRONG_STAT          12
#define FIND_FAIL           13
#define ENDIAN_NO_SUPPORT   14
#define DUPLICATE_ERROR     15
#define FILE_BROKEN         16
#define BUFFER_NO_SPACE     17
#define FWRITE_ERROR        18
#define MALLOC_FAIL         19
#define LOGIC_ERROR         20
#define BUFFER_FULL         21
#define FILE_END            22
#define FOPEN_FAIL          23
#define FILE_END_TOO_SOON   24
#define WRONG_ARGS          25
#define VERIFY_FAIL         26
#define FINGERPRINT_FAIL    27
#define NO_SUCH_FUNC        28
#define NO_SPACE_FUNC       29
#define NO_SUCH_OPT         30
#define UNKNOWN_ERROR       31
#define NO_SUCH_ALIAS       32
#define DUPLICATE_DB        33
#define FILE_NOSUPPORT      34
#define QUIT_REQUESTED      35


#define announce_test_set(set_name) printf("Test set : "#set_name"\n")
#define announce_test(test_name)    printf("Test in progress : "#test_name"\n")
#define announce_test_begin()       printf("=====TEST BEGIN=====\n")
#define announce_test_end()         printf("===== TEST END =====\n")

#define add_trackers() \
    int error_num = 0;      \
    int total_checks = 0;

#define incre_error() \
    error_num++

#define incre_check() \
    total_checks++

#define skip_if_prereq_failed(ret_prereq) \
    if (ret_prereq != 0) {                                      \
        printf("Test skipped due to failing prerequisite(s)\n");  \
        announce_test_end();                                    \
        return 1;                                               \
    }

#define report_stat() \
    printf("Checks passed : %d / %d\n", total_checks - error_num, total_checks)

#define report_total(ret_total) \
    if (ret_total == 0) { \
        printf("No errors were found\n"); \
    } \
    else { \
        printf("%d errors were found in total\n", ret_total); \
    }

#define print_set_tag_for_report_collector(set_name) \
    printf("[FOR REPORT COLLECTOR] SET -"#set_name"-\n")

#define print_test_tag_for_report_collector(test_name) do { \
        if (error_num > 0) { \
            printf("[FOR REPORT COLLECTOR] TEST -"#test_name"- : FAILED - < %d / %d > \n", total_checks - error_num, total_checks); \
        } \
        else { \
            printf("[FOR REPORT COLLECTOR] TEST -"#test_name"- : SUCCESS - < %d / %d > \n", total_checks - error_num, total_checks); \
        } \
    } while (0)
