/*  Copyright (c) 2016 Darrenldl All rights reserved.
 *
 *  This file is part of ffprinter
 *
 *  ffprinter is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  ffprinter is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with ffprinter.  If not, see <http://www.gnu.org/licenses/>.
 */

#define _XOPEN_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>

#include <stdint.h>
#include <inttypes.h>
#include "../lib/uthash.h"

#include "ffprinter_structure_template.h"

#include <limits.h>

#include <openssl/sha.h>

#include "../lib/simple_bitmap.h"

#ifndef FFPRINTER_H
#define FFPRINTER_H

#ifdef __CYGWIN__
#undef __STRICT_ANSI__
#endif

// uncomment following line to enable debugging output
//#define FFP_DEBUG

// uncomment following line to use memset_s (C11) for memory wiping
// instead of using mem_wipe_sec (see below)
//#define FFP_USE_MEMSET_S_FOR_MEM_WIPE

#if CHAR_BIT != 8
#error "Unsupported size of char"
#endif

#define FFP_VERSION         "00.01"

#define DB_FILE_EXTENSION   ".ffprint"

#define ALLOW_ZERO_LEN_STR  0x00000001

#define FS_PATH_MAX         4096
#define FILE_NAME_MAX       260
#define TAG_MAX_NUM         40
#define TAG_LEN_MAX         40
// TAG_LEN_MAX * 2 is for when the entire tag consists of separator
//#define TAG_STR_MAX         (TAG_MAX_NUM * (TAG_LEN_MAX * 2) + (TAG_MAX_NUM + 1))
#define TAG_STR_MAX         ((1 + TAG_LEN_MAX) * (TAG_MAX_NUM) + 1)
#define USER_MSG_MAX        200
#define EXTRACT_MAX_NUM     5
#define EXTRACT_SIZE_MAX    4
#define SECT_MAX_NUM        1000
#define SECT_MIN_NUM        2
#define ID_GEN_MAX_TRIES    1000
#define SCANMEM_PATTERN_MAX 1000

#define EID_LEN             8   // 64 bit = 8 bytes
#define EID_STR_MAX         (EID_LEN * 2)        // 64 bit value uses 16 hex digits

#define CHECKSUM_MAX_LEN    64
#define CHECKSUM_STR_MAX    (CHECKSUM_MAX_LEN * 2)

#define FILE_SIZE_STR_MAX   20
#define FILE_SIZE_MAX       UINT64_C(109951162777600)     // 100 TB

#define FFP_MAX_ALLOC       10485760            // 10 MB

#define ALLOW_NULL_CHILD_PTR    0x00000001
#define GO_THROUGH_CHAIN        0x00000002

#define VERIFY_STR_NOT_TERMINATED   100
#define VERIFY_WRONG_STR_LEN        101
#define VERIFY_MISSING_HEAD         102
#define VERIFY_MISSING_TAIL         103
#define VERIFY_BROKEN_FORWARD_LINK  104
#define VERIFY_WRONG_FORWARD_STAT   105
#define VERIFY_BROKEN_BACKWARD_LINK 104
#define VERIFY_WRONG_BACKWARD_STAT  105

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
#define FILE_END_TOO_SOON   24      // too soon, too soon
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
#define MAP_TOO_SHORT       36      // special code for bitmap use only
#define GEN_ID_FAIL         37
#define BROKEN_QUOTES       38      // code for grab_until_sep only
#define INVALID_DATE        39
#define FILE_EMPTY          40      // code for fingerprint function only
#define UNHANDLED_EXCEPTION 41
#define FFP_GENERAL_FAIL    42
#define FILE_NAME_TOO_LONG  44
#define FILE_NAME_EMPTY     45
#define INDEX_OUT_OF_RANGE  46
#define INVALID_HEX_STR     47

#define L1_EID_TO_E_ARR_SIZE    100
#define L2_EIE_INIT_SIZE        2
#define L2_EIE_GROW_SIZE        1

#define L1_FN_TO_E_ARR_SIZE     100
#define L2_FNE_INIT_SIZE        2
#define L2_FNE_GROW_SIZE        1

#define L1_TAG_TO_E_ARR_SIZE    100
#define L2_TGE_INIT_SIZE        2
#define L2_TGE_GROW_SIZE        1

#define L1_CSUM_TO_FD_ARR_SIZE  100
#define L2_CSF_INIT_SIZE        1
#define L2_CSF_GROW_SIZE        1

#define L1_CSUM_TO_S_ARR_SIZE   100
#define L2_CSS_INIT_SIZE        1
#define L2_CSS_GROW_SIZE        1

#define L1_FSIZE_TO_FD_ARR_SIZE 100
#define L2_FZF_INIT_SIZE        1
#define L2_FZF_GROW_SIZE        1

#define L1_ENTRY_ARR_SIZE       100
#define L2_ENTRY_INIT_SIZE      1
#define L2_ENTRY_GROW_SIZE      1

#define L1_FILE_DATA_ARR_SIZE   100
#define L2_FILE_DATA_INIT_SIZE  1
#define L2_FILE_DATA_GROW_SIZE  1

#define L1_SECT_ARR_SIZE        100
#define L2_SECT_INIT_SIZE       1
#define L2_SECT_GROW_SIZE       1

#define L1_MISC_ALLOC_ARR_SIZE  100
#define L2_MISC_ALLOC_INIT_SIZE 1
#define L2_MISC_ALLOC_GROW_SIZE 1

#define L1_DTT_YEAR_ARR_SIZE    10
#define L2_DTT_YEAR_INIT_SIZE   1
#define L2_DTT_YEAR_GROW_SIZE   1

#define L1_SECT_FIELD_ARR_SIZE  1000
#define L2_SECT_FIELD_INIT_SIZE 1
#define L2_SECT_FIELD_GROW_SIZE 1

#define ENTRY_OTHER             0x09
#define ENTRY_FILE              0x11
#define ENTRY_GROUP             0x22

#define CHECKSUM_MAX_NUM        3

#define CHECKSUM_SHA1_INDEX     0
#define CHECKSUM_SHA256_INDEX   1
#define CHECKSUM_SHA512_INDEX   2

#define CHECKSUM_UNUSED         0x00
#define CHECKSUM_SHA1_ID        0x01
#define CHECKSUM_SHA256_ID      0x02
#define CHECKSUM_SHA512_ID      0x03

#define CREATED_BY_SYS          0x02
#define CREATED_BY_USR          0x04

#define COMMAND_BUFFER_SIZE     400

#define DATE_TOD                1
#define DATE_TOM                2
#define DATE_TUSR               3

#define PDIR_MODE_FILENAME      1
#define PDIR_MODE_EID           2

#define NO_IGNORE_ESCAPE_CHAR   0
#define IGNORE_ESCAPE_CHAR      1

#define FIELD_REC_USE_E_NAME    UINT32_C(0x00000001)
#define FIELD_REC_USE_F_SIZE    UINT32_C(0x00000002)
#define FIELD_REC_USE_F_SHA1    UINT32_C(0x00000004)
#define FIELD_REC_USE_F_SHA256  UINT32_C(0x00000008)
#define FIELD_REC_USE_F_SHA512  UINT32_C(0x00000010)
#define FIELD_REC_USE_S_SHA1    UINT32_C(0x00000020)
#define FIELD_REC_USE_S_SHA256  UINT32_C(0x00000040)
#define FIELD_REC_USE_S_SHA512  UINT32_C(0x00000080)

#define sizeof_member(type, member) sizeof(((type*)0)->member)

#define ffp_min(a, b) ((a) < (b) ? (a) : (b))
#define ffp_max(a, b) ((a) > (b) ? (a) : (b))

// stringify sprintf
#define strfy_dummy(x) #x
#define strfy_sprintf(ptr, width, var_type, var) sprintf(ptr, "%0"strfy_dummy(width)var_type, var)

#define NUM_TO_INDEX(NUM)   (NUM-1)

#ifndef FFP_DEBUG
#define debug_printf(...)
#else
#define debug_printf printf
#endif

#define tf_print(str) printf("test flag - "#str"\n")

#define SET_NOT_INTERRUPTABLE() \
    __sync_synchronize();               \
    interruptable = 0;                  \
    __sync_synchronize()

#define SET_INTERRUPTABLE() \
    __sync_synchronize();               \
    interruptable = 1;                  \
    __sync_synchronize()

#define BACKUP_INTERRUPTABLE_FLAG() \
    __sync_synchronize();               \
    interruptable_old = interruptable;  \
    __sync_synchronize();

#define REVERT_INTERRUPTABLE_FLAG() \
    __sync_synchronize();               \
    interruptable = interruptable_old;  \
    __sync_synchronize()

#define INTERRUPTABLE       1
#define NOT_INTERRUPTABLE   0

#define MARK_DB_UNSAVED(dh) \
    __sync_synchronize();   \
    dh->unsaved = 1;        \
    __sync_synchronize();

#define MARK_DB_SAVED(dh) \
    __sync_synchronize();   \
    dh->unsaved = 0;        \
    __sync_synchronize();


extern int interruptable;
extern int interruptable_old;

#define record_misc_alloc(l2_arr, temp_record, ret, target) \
    ret = add_misc_alloc_record_to_layer2_arr(l2_arr, &temp_record, NULL);  \
    if (ret) {                                                              \
        return ret;                                                         \
    }                                                                       \
    temp_record->ptr = (unsigned char*) target

#define update_misc_alloc(l2_arr, temp_record, index, ret, target) \
    ret = get_misc_alloc_record_from_layer2_arr(l2_arr, &temp_record, index);   \
    if (ret) {                                                                  \
        return ret;                                                             \
    }                                                                           \
    temp_record->ptr = (unsigned char*) target

#define calc_pad(str, min_len)  ((int) ( (min_len > strlen(str)) ? ((min_len) - strlen(str)) : 0) )
extern const char space_pad[];

#ifdef FFP_USE_MEMSET_S_FOR_MEM_WIPE
#define mem_wipe_sec(ptr, size) memset_s(ptr, size, 0, size)
#else
int mem_wipe_sec(void* ptr, uint64_t size);
#endif

typedef uint64_t str_len_int;

typedef struct linked_entry linked_entry;

typedefs_trans(eid,         e   )
typedefs_trans(fn,          e   )
typedefs_trans(tag,         e   )
typedefs_trans(sha1f,       fd  )
typedefs_trans(sha256f,     fd  )
typedefs_trans(sha512f,     fd  )
typedefs_trans(sha1s,       s   )
typedefs_trans(sha256s,     s   )
typedefs_trans(sha512s,     s   )
typedefs_trans(f_size,      fd  )

typedef struct file_data        file_data;
typedef struct checksum_result  checksum_result;
typedef struct extract_sample   extract_sample;
typedef struct section          section;

typedef struct misc_alloc_record misc_alloc_record;

typedefs_obj_arr(entry)
typedefs_obj_arr(file_data)
typedefs_obj_arr(section)
typedefs_obj_arr(misc_alloc_record)
typedefs_obj_arr(dtt_year)

typedef struct dtt_year     dtt_year;
typedef struct dtt_month    dtt_month;
typedef struct dtt_day      dtt_day;
typedef struct dtt_hour     dtt_hour;

typedef uint64_t ffp_bid_int;
typedef uint64_t ffp_eid_int;

typedef struct uniq_char_map uniq_char_map;

typedef struct database_handle database_handle;

// below structures are for partial string matching
struct uniq_char_map {
    uniq_char_map* next;
    char character;
    simple_bitmap l1_arr_map; // layer 1 array containing this chararacter
};

int init_uniq_char_map (uniq_char_map* uniq_char);

struct misc_alloc_record {
    unsigned char* ptr;

/* Pool allocator structure */
    obj_meta_data_fields;
};

/* global variables */
// none

/* main structure for tree */
struct linked_entry {
    linked_entry* parent;
    linked_entry** child;
    bit_index child_arr_misc_record_number;

    unsigned char created_by;

    linked_entry* prev_same_fn;     // prev entry with same file name
    linked_entry* next_same_fn;     // next entry with same file name

    t_fn_to_e* fn_to_e;

    linked_entry* prev_same_tag;     // prev entry with same tag
    linked_entry* next_same_tag;     // next entry with same tag

    t_tag_to_e* tag_to_e;

    linked_entry* tod_prev_sort_min;    // prev entry in same hour, sorted by minute
    linked_entry* tod_next_sort_min;    // next entry in same hour, sorted by minute

    dtt_hour* tod_hour;

    linked_entry* tom_prev_sort_min;    // prev entry in same hour, sorted by minute
    linked_entry* tom_next_sort_min;    // next entry in same hour, sorted by minute

    dtt_hour* tom_hour;

    linked_entry* tusr_prev_sort_min;
    linked_entry* tusr_next_sort_min;

    dtt_hour* tusr_hour;

    ffp_eid_int child_num;                  // number of used children slot
    ffp_eid_int child_free_num;             // number of free children slot
    unsigned char branch_id[EID_LEN];       // entry id of head of branch
    char branch_id_str[EID_STR_MAX+1];
    ffp_eid_int depth;                  // not stored in file, depth of tree root is 0, 1 for head of branch, add 1 for deeper levels and so on
    unsigned char entry_id[EID_LEN];
    char entry_id_str[EID_STR_MAX+1];
    unsigned char has_parent;           // not stored in file, 0 if head of branch

    t_eid_to_e* eid_to_e;

    char file_name[FILE_NAME_MAX + 1];  // should always be terminated by null character
    str_len_int file_name_len;          // never counts null character

    uint8_t type;

    char tag_str[TAG_STR_MAX + 1];
    str_len_int tag_str_len;

    char user_msg[USER_MSG_MAX + 1];
    str_len_int user_msg_len;

    unsigned char tod_utc_used;     // time of addition usage indicator
    struct tm tod_utc;              // time of addition, UTC time zone
    unsigned char tom_utc_used;     // time of modification usage indicator
    struct tm tom_utc;              // time of modification, UTC time zone
    unsigned char tusr_utc_used;    // user specified time usage indicator
    struct tm tusr_utc;             // user specified time, defaults to nothing

    file_data* data; // may be null, if the entry is group, or is intended to be purely informative

/* Temporary linkage */
    linked_entry* link_prev;
    linked_entry* link_next;

/* Pool allocator structure */
    obj_meta_data_fields;
};

/* structure for translation */
generic_trans_struct_one_to_one(eid,        e,  linked_entry,   EID_STR_MAX)
generic_trans_struct_one_to_many(fn,        e,  linked_entry,   FILE_NAME_MAX)
generic_trans_struct_one_to_many(tag,       e,  linked_entry,   TAG_STR_MAX)
generic_trans_struct_one_to_many(sha1f,     fd, file_data,      CHECKSUM_STR_MAX)
generic_trans_struct_one_to_many(sha256f,   fd, file_data,      CHECKSUM_STR_MAX)
generic_trans_struct_one_to_many(sha512f,   fd, file_data,      CHECKSUM_STR_MAX)
generic_trans_struct_one_to_many(sha1s,     s,  section,        CHECKSUM_STR_MAX)
generic_trans_struct_one_to_many(sha256s,   s,  section,        CHECKSUM_STR_MAX)
generic_trans_struct_one_to_many(sha512s,   s,  section,        CHECKSUM_STR_MAX)
generic_trans_struct_one_to_many(f_size,    fd, file_data,      FILE_SIZE_STR_MAX)

/* structures for tree of date time */
// dtt = date time tree

struct dtt_hour {
    unsigned char in_use;
    linked_entry* head;
};

struct dtt_day {
    unsigned char in_use;
    dtt_hour hour[24];
};

struct dtt_month {
    unsigned char in_use;
    dtt_day day[31];
};

struct dtt_year {
    int year;       // always store actual year rather than offset from 1900
    dtt_year* next;
    dtt_month month[12];

/* Pool allocator structure */
    obj_meta_data_fields;
};

struct checksum_result {
    uint16_t type;
    uint16_t len;
    unsigned char checksum[CHECKSUM_MAX_LEN];
    char checksum_str[CHECKSUM_STR_MAX+1];
};

int copy_checksum_result(checksum_result* dst, checksum_result* src);

struct extract_sample {
    uint64_t position;  // position is always absolute(counting from start of file)
    uint16_t len;
    unsigned char extract[EXTRACT_SIZE_MAX];
};

int copy_extract_sample(extract_sample* dst, extract_sample* src);

struct section {
    uint64_t start_pos;     // start position in file
    uint64_t end_pos;       // end position in file

    checksum_result checksum[CHECKSUM_MAX_NUM];
    uint16_t checksum_num;
    extract_sample extract[EXTRACT_MAX_NUM];
    uint32_t extract_num;

/* Translation structures */
    section* prev_same_sha1;
    section* next_same_sha1;

    section* prev_same_sha256;
    section* next_same_sha256;

    section* prev_same_sha512;
    section* next_same_sha512;

    t_sha1s_to_s*      sha1s_to_s;
    t_sha256s_to_s*    sha256s_to_s;
    t_sha512s_to_s*    sha512s_to_s;

/* Parent linkage */
    file_data* parent_file_data;

/* Pool allocator structure */
    obj_meta_data_fields;

/* Hash table structure */
    //UT_hash_handle hh;
};

struct file_data {
    uint64_t file_size;

/* File wise fields */
    checksum_result checksum[CHECKSUM_MAX_NUM]; // unused one has type CHECKSUM_UNUSED
    //uint16_t checksum_num;                      // number filled
    extract_sample extract[EXTRACT_MAX_NUM];
    uint32_t extract_num;
    char file_size_str[FILE_SIZE_STR_MAX+1];

/* Section wise fields */
    section**   section;            // sections are normally continuous
    uint64_t    section_num;        // number of used section slot
    uint64_t    section_free_num;   // number of free section slot
    // if one sets them to be non-continuous
    // section_num should still reflect true number of sections
    // and both norm_sect_size and last_sect_size should be 0
    uint64_t    norm_sect_size;
    uint64_t    last_sect_size;

/* Translation structures */
    file_data* prev_same_sha1;
    file_data* next_same_sha1;

    file_data* prev_same_sha256;
    file_data* next_same_sha256;

    file_data* prev_same_sha512;
    file_data* next_same_sha512;

    t_sha1f_to_fd*     sha1f_to_fd;
    t_sha256f_to_fd*   sha256f_to_fd;
    t_sha512f_to_fd*   sha512f_to_fd;

    file_data* prev_same_f_size;
    file_data* next_same_f_size;

    t_f_size_to_fd* f_size_to_fd;

/* Parent linkage */
    linked_entry* parent_entry;

/* Pool allocator structure */
    obj_meta_data_fields;

/* Hash table structure */
    UT_hash_handle hh;
};                         // sections are to be stored sequentially regardless of continuity

/* linear representation of fields in an entry */
typedef struct field_record field_record;
typedef struct sect_field   sect_field;
typedef struct field_match_bitmap field_match_bitmap;
typedefs_obj_arr(sect_field)

struct field_record {
    uint64_t    field_usage_map;
    char        name        [FILE_NAME_MAX+1];
    struct tm   tod_utc;
    struct tm   tom_utc;
    struct tm   tusr_utc;
    char        f_size      [FILE_SIZE_STR_MAX+1];
    char        f_sha1      [CHECKSUM_STR_MAX];
    char        f_sha256    [CHECKSUM_STR_MAX];
    char        f_sha512    [CHECKSUM_STR_MAX];

    sect_field**  sect;
    uint64_t sect_field_in_use_num;
    uint64_t sect_field_num;
    uint64_t sect_field_free_num;
};

struct sect_field {
    char        s_sha1      [CHECKSUM_STR_MAX];
    char        s_sha256    [CHECKSUM_STR_MAX];
    char        s_sha512    [CHECKSUM_STR_MAX];

    obj_meta_data_fields;
};

layer1_obj_arr(sect_field, sect_field, L1_SECT_FIELD_ARR_SIZE)
layer2_obj_arr(sect_field)

/* corresponding bitmaps */
struct field_match_bitmap {
    simple_bitmap map_buf1;
    simple_bitmap map_buf2;

    simple_bitmap map_name_match;
    simple_bitmap map_f_size_match;
    simple_bitmap map_f_sha1_match;
    simple_bitmap map_f_sha256_match;
    simple_bitmap map_f_sha512_match;
    simple_bitmap map_s_sha1_match;
    simple_bitmap map_s_sha256_match;
    simple_bitmap map_s_sha512_match;

    simple_bitmap map_result;
};

/* structures for pool allocator */
layer1_obj_arr(entry, linked_entry, L1_ENTRY_ARR_SIZE)
layer2_obj_arr(entry)

layer1_obj_arr(file_data, file_data, L1_FILE_DATA_ARR_SIZE)
layer2_obj_arr(file_data)

layer1_obj_arr(section, section, L1_SECT_ARR_SIZE)
layer2_obj_arr(section)

layer1_obj_arr(misc_alloc_record, misc_alloc_record, L1_MISC_ALLOC_ARR_SIZE)
layer2_obj_arr(misc_alloc_record)

layer1_obj_arr(dtt_year, dtt_year, L1_DTT_YEAR_ARR_SIZE)
layer2_obj_arr(dtt_year)

// max length excluding null character
int verify_str_terminated (const char* str, str_len_int max_len, str_len_int* length, uint32_t flags);

int tag_count(const char* tag_str, str_len_int* ret_tag_num, str_len_int* ret_tag_min_len, str_len_int* ret_tag_max_len);

int reverse_strcpy(char* dst, const char* src);

int upper_to_lower_case (char* str);

int match_sub (const char* haystack, const char* needle, int start_pos);

int match_sub_min_max (const char* haystack, const char* needle, int start_pos_min, int start_pos_max);

int grab_until_sep(char* dst, const char* src, str_len_int max_len, str_len_int* len_read, str_len_int* len_copied, unsigned char ignore_escp_char, char sep);

int discard_input_buffer(void);

int bytes_to_hex_str (char* dst, unsigned char* ptr, uint32_t size);

int hex_str_to_bytes (unsigned char* dst, char* src);

// entry id translation

generic_exist_mat(eid, EID_STR_MAX)
layer1_generic_arr(eid, e, L1_EID_TO_E_ARR_SIZE)
layer2_generic_arr(eid, e)

int init_eid_to_e (t_eid_to_e* eid_to_e);
int init_eid_exist_mat (eid_exist_mat* matrix);
int init_layer2_eid_to_e_arr (layer2_eid_to_e_arr* l2_arr);

int add_eid_to_e_to_htab (t_eid_to_e** htab, t_eid_to_e* eid_to_e);
int del_eid_to_e_from_htab (t_eid_to_e** htab, t_eid_to_e* eid_to_e);

int add_eid_to_e_to_layer2_arr (layer2_eid_to_e_arr* l2_arr, t_eid_to_e** eid_to_e, bit_index* index);
int del_eid_to_e_from_layer2_arr (layer2_eid_to_e_arr* l2_arr, bit_index index);
int get_eid_to_e_from_layer2_arr (layer2_eid_to_e_arr* l2_arr, t_eid_to_e** result, bit_index index);
int get_l1_eid_to_e_from_layer2_arr (layer2_eid_to_e_arr* l2_arr, layer1_eid_to_e_arr** result, bit_index index_of_l1_arr);

int del_l2_eid_to_e_arr (layer2_eid_to_e_arr* l2_arr);

int add_eid_to_eid_exist_mat (eid_exist_mat* matrix, char* eid_str, bit_index index_in_arr);
int del_eid_from_eid_exist_mat (eid_exist_mat* matrix, layer2_eid_to_e_arr* l2_arr, bit_index index_in_arr);

// for file name translation

generic_exist_mat(fn, FILE_NAME_MAX)
layer1_generic_arr(fn, e, L1_FN_TO_E_ARR_SIZE)
layer2_generic_arr(fn, e)

int init_fn_to_e (t_fn_to_e* fn_to_e);
int init_fn_exist_mat (fn_exist_mat* matrix);
int init_layer2_fn_to_e_arr (layer2_fn_to_e_arr* l2_arr);

int add_fn_to_e_to_htab (t_fn_to_e** htab, t_fn_to_e* fn_to_e);
int del_fn_to_e_from_htab (t_fn_to_e** htab, t_fn_to_e* fn_to_e);

int add_e_to_fn_to_e_chain (linked_entry* tar_entry, linked_entry* entry);
int del_e_from_fn_to_e_chain (t_fn_to_e** htab, fn_exist_mat* matrix, layer2_fn_to_e_arr* l2_arr, linked_entry* entry);

int add_fn_to_e_to_layer2_arr (layer2_fn_to_e_arr* l2_arr, t_fn_to_e** fn_to_e, bit_index* index);
int del_fn_to_e_from_layer2_arr (layer2_fn_to_e_arr* l2_arr, bit_index index);
int get_fn_to_e_from_layer2_arr (layer2_fn_to_e_arr* l2_arr, t_fn_to_e** result, bit_index index);
int get_l1_fn_to_e_from_layer2_arr (layer2_fn_to_e_arr* l2_arr, layer1_fn_to_e_arr** result, bit_index index_of_l1_arr);

int del_l2_fn_to_e_arr (layer2_fn_to_e_arr* l2_arr);

int add_fn_to_fn_exist_mat (fn_exist_mat* matrix, char* file_name, bit_index index_in_arr);
int del_fn_from_fn_exist_mat (fn_exist_mat* matrix, layer2_fn_to_e_arr* l2_arr, bit_index index_in_arr);

// for tag translation
generic_exist_mat(tag, TAG_STR_MAX)
layer1_generic_arr(tag, e, L1_TAG_TO_E_ARR_SIZE)
layer2_generic_arr(tag, e)

int init_tag_to_e (t_tag_to_e* tag_to_e);
int init_tag_exist_mat (tag_exist_mat* matrix);
int init_layer2_tag_to_e_arr (layer2_tag_to_e_arr* l2_arr);

int add_tag_to_e_to_htab (t_tag_to_e** htab, t_tag_to_e* tag_to_e);
int del_tag_to_e_from_htab (t_tag_to_e** htab, t_tag_to_e* tag_to_e);

int add_e_to_tag_to_e_chain (linked_entry* tar_entry, linked_entry* entry);
int del_e_from_tag_to_e_chain (t_tag_to_e** htab, tag_exist_mat* matrix, layer2_tag_to_e_arr* l2_arr, linked_entry* entry);

int add_tag_to_e_to_layer2_arr (layer2_tag_to_e_arr* l2_arr, t_tag_to_e** tag_to_e, bit_index* index);
int del_tag_to_e_from_layer2_arr (layer2_tag_to_e_arr* l2_arr, bit_index index);
int get_tag_to_e_from_layer2_arr (layer2_tag_to_e_arr* l2_arr, t_tag_to_e** result, bit_index index);
int get_l1_tag_to_e_from_layer2_arr (layer2_tag_to_e_arr* l2_arr, layer1_tag_to_e_arr** result, bit_index index_of_l1_arr);

int del_l2_tag_to_e_arr (layer2_tag_to_e_arr* l2_arr);

int add_tag_to_tag_exist_mat (tag_exist_mat* matrix, char* tag_str, bit_index index_in_arr);
int del_tag_from_tag_exist_mat (tag_exist_mat* matrix, layer2_tag_to_e_arr* l2_arr, bit_index index_in_arr);

/* For file data checksum */
// sha1
generic_exist_mat(sha1f, CHECKSUM_STR_MAX)
layer1_generic_arr(sha1f, fd, L1_CSUM_TO_FD_ARR_SIZE)
layer2_generic_arr(sha1f, fd)

int init_sha1f_to_fd (t_sha1f_to_fd* target);
int init_layer2_sha1f_to_fd_arr (layer2_sha1f_to_fd_arr* l2_arr_arr);

int add_sha1f_to_fd_to_htab (t_sha1f_to_fd** htab, t_sha1f_to_fd* target);
int del_sha1f_to_fd_from_htab (t_sha1f_to_fd** htab, t_sha1f_to_fd* target);

int add_sha1f_to_sha1f_exist_mat (sha1f_exist_mat* matrix, char* sha1_str, bit_index index_in_arr);
int del_sha1f_from_sha1f_exist_mat (sha1f_exist_mat* matrix, layer2_sha1f_to_fd_arr* l2_arr, bit_index index_in_arr);

int add_sha1f_to_fd_to_layer2_arr (layer2_sha1f_to_fd_arr* l2_arr, t_sha1f_to_fd** target, bit_index* index);
int del_sha1f_to_fd_from_layer2_arr (layer2_sha1f_to_fd_arr* l2_arr_arr, bit_index index);
int get_sha1f_to_fd_from_layer2_arr (layer2_sha1f_to_fd_arr* l2_arr_arr, t_sha1f_to_fd** result, bit_index index);
int get_l1_sha1f_to_fd_from_layer2_arr (layer2_sha1f_to_fd_arr* l2_arr_arr, layer1_sha1f_to_fd_arr** result, bit_index index_of_l1_arr);

int del_l2_sha1f_to_fd_arr (layer2_sha1f_to_fd_arr* l2_arr);

int add_fd_to_sha1f_to_fd_chain (file_data* tar_fd, file_data* fd);
int del_fd_from_sha1f_to_fd_chain (t_sha1f_to_fd** htab_p, sha1f_exist_mat* matrix_arr, layer2_sha1f_to_fd_arr* l2_arr_arr, file_data* fd);

// sha256
generic_exist_mat(sha256f, CHECKSUM_STR_MAX)
layer1_generic_arr(sha256f, fd, L1_CSUM_TO_FD_ARR_SIZE)
layer2_generic_arr(sha256f, fd)

int init_sha256f_to_fd (t_sha256f_to_fd* target);
int init_layer2_sha256f_to_fd_arr (layer2_sha256f_to_fd_arr* l2_arr_arr);

int add_sha256f_to_fd_to_htab (t_sha256f_to_fd** htab, t_sha256f_to_fd* target);
int del_sha256f_to_fd_from_htab (t_sha256f_to_fd** htab, t_sha256f_to_fd* target);

int add_sha256f_to_sha256f_exist_mat (sha256f_exist_mat* matrix, char* sha256_str, bit_index index_in_arr);
int del_sha256f_from_sha256f_exist_mat (sha256f_exist_mat* matrix, layer2_sha256f_to_fd_arr* l2_arr, bit_index index_in_arr);

int add_sha256f_to_fd_to_layer2_arr (layer2_sha256f_to_fd_arr* l2_arr, t_sha256f_to_fd** target, bit_index* index);
int del_sha256f_to_fd_from_layer2_arr (layer2_sha256f_to_fd_arr* l2_arr, bit_index index);
int get_sha256f_to_fd_from_layer2_arr (layer2_sha256f_to_fd_arr* l2_arr, t_sha256f_to_fd** result, bit_index index);
int get_l1_sha256f_to_fd_from_layer2_arr (layer2_sha256f_to_fd_arr* l2_arr, layer1_sha256f_to_fd_arr** result, bit_index index_of_l1_arr);

int del_l2_sha256f_to_fd_arr (layer2_sha256f_to_fd_arr* l2_arr);

int add_fd_to_sha256f_to_fd_chain (file_data* tar_fd, file_data* fd);
int del_fd_from_sha256f_to_fd_chain (t_sha256f_to_fd** htab_p, sha256f_exist_mat* matrix_arr, layer2_sha256f_to_fd_arr* l2_arr_arr, file_data* fd);

// sha512
generic_exist_mat(sha512f, CHECKSUM_STR_MAX)
layer1_generic_arr(sha512f, fd, L1_CSUM_TO_FD_ARR_SIZE)
layer2_generic_arr(sha512f, fd)

int init_sha512f_to_fd (t_sha512f_to_fd* target);
int init_layer2_sha512f_to_fd_arr (layer2_sha512f_to_fd_arr* l2_arr_arr);

int add_sha512f_to_fd_to_htab (t_sha512f_to_fd** htab, t_sha512f_to_fd* target);
int del_sha512f_to_fd_from_htab (t_sha512f_to_fd** htab, t_sha512f_to_fd* target);

int add_sha512f_to_sha512f_exist_mat (sha512f_exist_mat* matrix, char* sha512_str, bit_index index_in_arr);
int del_sha512f_from_sha512f_exist_mat (sha512f_exist_mat* matrix, layer2_sha512f_to_fd_arr* l2_arr, bit_index index_in_arr);

int add_sha512f_to_fd_to_layer2_arr (layer2_sha512f_to_fd_arr* l2_arr, t_sha512f_to_fd** target, bit_index* index);
int del_sha512f_to_fd_from_layer2_arr (layer2_sha512f_to_fd_arr* l2_arr, bit_index index);
int get_sha512f_to_fd_from_layer2_arr (layer2_sha512f_to_fd_arr* l2_arr, t_sha512f_to_fd** result, bit_index index);
int get_l1_sha512f_to_fd_from_layer2_arr (layer2_sha512f_to_fd_arr* l2_arr, layer1_sha512f_to_fd_arr** result, bit_index index_of_l1_arr);

int del_l2_sha512f_to_fd_arr (layer2_sha512f_to_fd_arr* l2_arr);

int add_fd_to_sha512f_to_fd_chain (file_data* tar_fd, file_data* fd);
int del_fd_from_sha512f_to_fd_chain (t_sha512f_to_fd** htab_p, sha512f_exist_mat* matrix_arr, layer2_sha512f_to_fd_arr* l2_arr_arr, file_data* fd);

/* For section checksum */
// sha1
generic_exist_mat(sha1s, CHECKSUM_STR_MAX)
layer1_generic_arr(sha1s, s, L1_CSUM_TO_S_ARR_SIZE)
layer2_generic_arr(sha1s, s)

int init_sha1s_to_s (t_sha1s_to_s* target);
int init_layer2_sha1s_to_s_arr (layer2_sha1s_to_s_arr* l2_arr_arr);

int add_sha1s_to_s_to_htab (t_sha1s_to_s** htab, t_sha1s_to_s* target);
int del_sha1s_to_s_from_htab (t_sha1s_to_s** htab, t_sha1s_to_s* target);

int add_sha1s_to_sha1s_exist_mat (sha1s_exist_mat* matrix, char* sha1_str, bit_index index_in_arr);
int del_sha1s_from_sha1s_exist_mat (sha1s_exist_mat* matrix, layer2_sha1s_to_s_arr* l2_arr, bit_index index_in_arr);

int add_sha1s_to_s_to_layer2_arr (layer2_sha1s_to_s_arr* l2_arr, t_sha1s_to_s** sha1s_to_s, bit_index* index);
int del_sha1s_to_s_from_layer2_arr (layer2_sha1s_to_s_arr* l2_arr, bit_index index);
int get_sha1s_to_s_from_layer2_arr (layer2_sha1s_to_s_arr* l2_arr, t_sha1s_to_s** result, bit_index index);
int get_l1_sha1s_to_s_from_layer2_arr (layer2_sha1s_to_s_arr* l2_arr, layer1_sha1s_to_s_arr** result, bit_index index_of_l1_arr);

int del_l2_sha1s_to_s_arr (layer2_sha1s_to_s_arr* l2_arr);

int add_s_to_sha1s_to_s_chain (section* tar_sect, section* sect);
int del_s_from_sha1s_to_s_chain (t_sha1s_to_s** htab_p, sha1s_exist_mat* matrix, layer2_sha1s_to_s_arr* l2_arr_arr, section* sect);

// sha256
generic_exist_mat(sha256s, CHECKSUM_STR_MAX)
layer1_generic_arr(sha256s, s, L1_CSUM_TO_S_ARR_SIZE)
layer2_generic_arr(sha256s, s)

int init_sha256s_to_s (t_sha256s_to_s* target);
int init_layer2_sha256s_to_s_arr (layer2_sha256s_to_s_arr* l2_arr_arr);

int add_sha256s_to_s_to_htab (t_sha256s_to_s** htab, t_sha256s_to_s* target);
int del_sha256s_to_s_from_htab (t_sha256s_to_s** htab, t_sha256s_to_s* target);

int add_sha256s_to_sha256s_exist_mat (sha256s_exist_mat* matrix, char* sha256_str, bit_index index_in_arr);
int del_sha256s_from_sha256s_exist_mat (sha256s_exist_mat* matrix, layer2_sha256s_to_s_arr* l2_arr, bit_index index_in_arr);

int add_sha256s_to_s_to_layer2_arr (layer2_sha256s_to_s_arr* l2_arr, t_sha256s_to_s** sha256s_to_s, bit_index* index);
int del_sha256s_to_s_from_layer2_arr (layer2_sha256s_to_s_arr* l2_arr_arr, bit_index index);
int get_sha256s_to_s_from_layer2_arr (layer2_sha256s_to_s_arr* l2_arr, t_sha256s_to_s** result, bit_index index);
int get_l1_sha256s_to_s_from_layer2_arr (layer2_sha256s_to_s_arr* l2_arr, layer1_sha256s_to_s_arr** result, bit_index index_of_l1_arr);

int del_l2_sha256s_to_s_arr (layer2_sha256s_to_s_arr* l2_arr);

int add_s_to_sha256s_to_s_chain (section* tar_sect, section* sect);
int del_s_from_sha256s_to_s_chain (t_sha256s_to_s** htab_p, sha256s_exist_mat* matrix, layer2_sha256s_to_s_arr* l2_arr_arr, section* sect);

// sha512
generic_exist_mat(sha512s, CHECKSUM_STR_MAX)
layer1_generic_arr(sha512s, s, L1_CSUM_TO_S_ARR_SIZE)
layer2_generic_arr(sha512s, s)

int init_sha512s_to_s (t_sha512s_to_s* target);
int init_layer2_sha512s_to_s_arr (layer2_sha512s_to_s_arr* l2_arr_arr);

int add_sha512s_to_s_to_htab (t_sha512s_to_s** htab, t_sha512s_to_s* target);
int del_sha512s_to_s_from_htab (t_sha512s_to_s** htab, t_sha512s_to_s* target);

int add_sha512s_to_sha512s_exist_mat (sha512s_exist_mat* matrix, char* sha512_str, bit_index index_in_arr);
int del_sha512s_from_sha512s_exist_mat (sha512s_exist_mat* matrix, layer2_sha512s_to_s_arr* l2_arr, bit_index index_in_arr);

int add_sha512s_to_s_to_layer2_arr (layer2_sha512s_to_s_arr* l2_arr, t_sha512s_to_s** sha512s_to_s, bit_index* index);
int del_sha512s_to_s_from_layer2_arr (layer2_sha512s_to_s_arr* l2_arr_arr, bit_index index);
int get_sha512s_to_s_from_layer2_arr (layer2_sha512s_to_s_arr* l2_arr, t_sha512s_to_s** result, bit_index index);
int get_l1_sha512s_to_s_from_layer2_arr (layer2_sha512s_to_s_arr* l2_arr, layer1_sha512s_to_s_arr** result, bit_index index_of_l1_arr);

int del_l2_sha512s_to_s_arr (layer2_sha512s_to_s_arr* l2_arr);

int add_s_to_sha512s_to_s_chain (section* tar_sect, section* sect);
int del_s_from_sha512s_to_s_chain (t_sha512s_to_s** htab_p, sha512s_exist_mat* matrix, layer2_sha512s_to_s_arr* l2_arr_arr, section* sect);

/* For file size */
generic_exist_mat(f_size, FILE_SIZE_STR_MAX)
layer1_generic_arr(f_size, fd, L1_FSIZE_TO_FD_ARR_SIZE)
layer2_generic_arr(f_size, fd)

int init_f_size_to_fd (t_f_size_to_fd* f_size_to_fd);
int init_f_size_exist_mat (f_size_exist_mat* matrix);
int init_layer2_f_size_to_fd_arr (layer2_f_size_to_fd_arr* l2_arr);

int add_f_size_to_fd_to_htab (t_f_size_to_fd** htab, t_f_size_to_fd* f_size_to_fd);
int del_f_size_to_fd_from_htab (t_f_size_to_fd** htab, t_f_size_to_fd* f_size_to_fd);

int add_f_size_to_f_size_exist_mat (f_size_exist_mat* matrix, char* f_size_str, bit_index index_in_arr);
int del_f_size_from_f_size_exist_mat (f_size_exist_mat* matrix, layer2_f_size_to_fd_arr* l2_arr, bit_index index_in_arr);

int add_f_size_to_fd_to_layer2_arr (layer2_f_size_to_fd_arr* l2_arr, t_f_size_to_fd** f_size_to_fd, bit_index* index);
int del_f_size_to_fd_from_layer2_arr (layer2_f_size_to_fd_arr* l2_arr, bit_index index);
int get_f_size_to_fd_from_layer2_arr (layer2_f_size_to_fd_arr* l2_arr, t_f_size_to_fd** result, bit_index index);
int get_l1_f_size_to_fd_from_layer2_arr (layer2_f_size_to_fd_arr* l2_arr, layer1_f_size_to_fd_arr** result, bit_index index_of_l1_arr);

int del_l2_f_size_to_fd_arr (layer2_f_size_to_fd_arr* l2_arr);

int add_fd_to_f_size_to_fd_chain (file_data* tar_fd, file_data* fd);
int del_fd_from_f_size_to_fd_chain (t_f_size_to_fd** htab_p, f_size_exist_mat* matrix, layer2_f_size_to_fd_arr* l2_arr, file_data* fd);

// pool allocator functions
// entry
int init_layer2_entry_arr(layer2_entry_arr* l2_arr);
int add_entry_to_layer2_arr(layer2_entry_arr* l2_arr, linked_entry** ret_entry, bit_index* index);
int del_entry_from_layer2_arr(layer2_entry_arr* l2_arr, bit_index index);
int get_entry_from_layer2_arr(layer2_entry_arr* l2_arr, linked_entry** ret_entry, bit_index index);
int get_l1_entry_from_layer2_arr(layer2_entry_arr* l2_arr, layer1_entry_arr** ret_entry, bit_index index_of_l1_arr);
int del_l2_entry_arr(layer2_entry_arr* l2_arr);

// file data
int init_layer2_file_data_arr(layer2_file_data_arr* l2_arr);
int add_file_data_to_layer2_arr(layer2_file_data_arr* l2_arr, file_data** ret_file_data, bit_index* index);
int del_file_data_from_layer2_arr(layer2_file_data_arr* l2_arr, bit_index index);
int get_file_data_from_layer2_arr(layer2_file_data_arr* l2_arr, file_data** ret_file_data, bit_index index);
int get_l1_file_data_from_layer2_arr(layer2_file_data_arr* l2_arr, layer1_file_data_arr** ret_file_data, bit_index index_of_l1_arr);
int del_l2_file_data_arr(layer2_file_data_arr* l2_arr);

// section
int init_layer2_section_arr(layer2_section_arr* l2_arr);
int add_section_to_layer2_arr(layer2_section_arr* l2_arr, section** ret_section, bit_index* index);
int del_section_from_layer2_arr(layer2_section_arr* l2_arr, bit_index index);
int get_section_from_layer2_arr(layer2_section_arr* l2_arr, section** ret_section, bit_index index);
int get_l1_section_from_layer2_arr(layer2_section_arr* l2_arr, layer1_section_arr** ret_section, bit_index index_of_l1_arr);
int del_l2_section_arr(layer2_section_arr* l2_arr);

// misc alloc record
int init_layer2_misc_alloc_record_arr(layer2_misc_alloc_record_arr* l2_arr);
int add_misc_alloc_record_to_layer2_arr(layer2_misc_alloc_record_arr* l2_arr, misc_alloc_record** ret_misc_alloc_record, bit_index* index);
int del_misc_alloc_record_from_layer2_arr(layer2_misc_alloc_record_arr* l2_arr, bit_index index);
int get_misc_alloc_record_from_layer2_arr(layer2_misc_alloc_record_arr* l2_arr, misc_alloc_record** ret_misc_alloc_record, bit_index index);
int get_l1_misc_alloc_record_from_layer2_arr(layer2_misc_alloc_record_arr* l2_arr, layer1_misc_alloc_record_arr** ret_misc_alloc_record, bit_index index_of_l1_arr);
int del_l2_misc_alloc_record_arr(layer2_misc_alloc_record_arr* l2_arr);

// dtt year
int init_layer2_dtt_year_arr(layer2_dtt_year_arr* l2_arr);
int add_dtt_year_to_layer2_arr(layer2_dtt_year_arr* l2_arr, dtt_year** ret_dtt_year, bit_index* index);
int del_dtt_year_from_layer2_arr(layer2_dtt_year_arr* l2_arr, bit_index index);
int get_dtt_year_from_layer2_arr(layer2_dtt_year_arr* l2_arr, dtt_year** ret_dtt_year, bit_index index);
int get_l1_dtt_year_from_layer2_arr(layer2_dtt_year_arr* l2_arr, layer1_dtt_year_arr** ret_dtt_year, bit_index index_of_l1_arr);
int del_l2_dtt_year_arr(layer2_dtt_year_arr* l2_arr);

// sect field
int init_layer2_sect_field_arr(layer2_sect_field_arr* l2_arr);
int add_sect_field_to_layer2_arr(layer2_sect_field_arr* l2_arr, sect_field** ret_sect_field, bit_index* index);
int del_sect_field_from_layer2_arr(layer2_sect_field_arr* l2_arr, bit_index index);
int get_sect_field_from_layer2_arr(layer2_sect_field_arr* l2_arr, sect_field** ret_sect_field, bit_index index);
int get_l1_sect_field_from_layer2_arr(layer2_sect_field_arr* l2_arr, layer1_sect_field_arr** ret_sect_field, bit_index index_of_l1_arr);
int del_l2_sect_field_arr(layer2_sect_field_arr* l2_arr);

struct database_handle {
    unsigned char           unsaved;

    //ffp_eid_int total_entry_num;

    char                    name[FILE_NAME_MAX+1];

    /* main tree */
    linked_entry            tree;

    /* ID translation */
    t_eid_to_e*             eid_to_e;           // hash table, used for exact matching
    eid_exist_mat           eid_mat;            // existence matrix, used for partial matching
    layer2_eid_to_e_arr     l2_eid_to_e_arr;    // layer 2 array, used for partial matching

    /* for file name translation */
    t_fn_to_e*              fn_to_e;            // hash table, used for exact matching
    fn_exist_mat            fn_mat;             // existence matrix, used for partial matching
    layer2_fn_to_e_arr      l2_fn_to_e_arr;     // layer 2 array, used for partial matching

    /* for tag translation */
    t_tag_to_e*             tag_to_e;           // hash table, used for exact matching
    tag_exist_mat           tag_mat;            // existence matrix, used for partial matching
    layer2_tag_to_e_arr     l2_tag_to_e_arr;    // layer 2 array, used for partial matching

    /* for file data checksum translation */
    // sha1
    t_sha1f_to_fd*              sha1f_to_fd;    // hash table, used for exact matching
    sha1f_exist_mat             sha1f_mat;      // existence matrix, used for partial matching
    layer2_sha1f_to_fd_arr      l2_sha1f_to_fd_arr; // layer 2 array, used for partial matching
    // sha256
    t_sha256f_to_fd*            sha256f_to_fd;      // hash table, used for exact matching
    sha256f_exist_mat           sha256f_mat;        // existence matrix, used for partial matching
    layer2_sha256f_to_fd_arr    l2_sha256f_to_fd_arr;   // layer 2 array, used for partial matching
    // sha512
    t_sha512f_to_fd*            sha512f_to_fd;      // hash table, used for exact matching
    sha512f_exist_mat           sha512f_mat;        // existence matrix, used for partial matching
    layer2_sha512f_to_fd_arr    l2_sha512f_to_fd_arr;   // layer 2 array, used for partial matching

    /* for section checksum translation */
    // sha1
    t_sha1s_to_s*           sha1s_to_s;         // hash table, used for exact matching
    sha1s_exist_mat         sha1s_mat;          // existence matrix, used for partial matching
    layer2_sha1s_to_s_arr   l2_sha1s_to_s_arr;  // layer 2 array, used for partial matching
    // sha256
    t_sha256s_to_s*             sha256s_to_s;           // hash table, used for exact matching
    sha256s_exist_mat           sha256s_mat;            // existence matrix, used for partial matching
    layer2_sha256s_to_s_arr     l2_sha256s_to_s_arr;    // layer 2 array, used for partial matching
    // sha512
    t_sha512s_to_s*             sha512s_to_s;           // hash table, used for exact matching
    sha512s_exist_mat           sha512s_mat;            // existence matrix, used for partial matching
    layer2_sha512s_to_s_arr     l2_sha512s_to_s_arr;    // layer 2 array, used for partial matching

    /* for file size translation */
    t_f_size_to_fd*             f_size_to_fd;    // hash table, used for exact matching
    f_size_exist_mat            f_size_mat;     // existence matrix, used for partial matching
    layer2_f_size_to_fd_arr     l2_f_size_to_fd_arr;  // layer 2 array, used for partial matching

    /* for date time tree */
    dtt_year*                   tod_dt_tree;
    dtt_year*                   tom_dt_tree;
    dtt_year*                   tusr_dt_tree;

    /* pool allocator structure */
    layer2_entry_arr                l2_entry_arr;
    layer2_file_data_arr            l2_file_data_arr;
    layer2_section_arr              l2_section_arr;
    layer2_misc_alloc_record_arr    l2_misc_alloc_record_arr;
    layer2_dtt_year_arr             l2_dtt_year_arr;

    bit_index max_misc_alloc_record_index;

    /* Hash table structure */
    UT_hash_handle hh;
}; 

int link_entry_clear(linked_entry* tar);

int link_entry_tail(linked_entry* dst, linked_entry* tar);

int link_entry_head(linked_entry* dst, linked_entry* tar);

int link_entry_after(linked_entry* dst, linked_entry* tar);

int link_entry_before(linked_entry* dst, linked_entry* tar);

int link_entry_get_head(linked_entry* tar, linked_entry** ret);

int link_entry_get_tail(linked_entry* tar, linked_entry** ret);

int init_database_handle(database_handle* dh);

int fres_database_handle(database_handle* dh);

int ffp_grow_bitmap (simple_bitmap* map, bit_index length);

#endif
