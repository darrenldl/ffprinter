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

#include "ffprinter.h"
#include "ffp_error.h"
#include <openssl/sha.h>
#include <sys/stat.h>

#ifndef FFP_FINGERPRINT_H
#define FFP_FINGERPRINT_H

#define EXTRACT_LEAK_MAX_PERCENT        5   // maximum % of file info leak by extract

#define RAND_FILE_NAME_LEN              8

#define FS_FILE_ACCESS_FAIL             500
#define FS_UNRECOGNISED_FILE_TYPE       501
#define FS_FILE_TOO_LARGE               502

// fallback section number
#define FALLBACK_SECT_NUM       100

#define SECT_SIZE_SMALL         100LL

#define FPRINT_USE_F_NAME       UINT32_C(0x00000001)
#define FPRINT_USE_F_SIZE       UINT32_C(0x00000002)
#define FPRINT_USE_F_EXTR       UINT32_C(0x00000004)
#define FPRINT_USE_F_SHA1       UINT32_C(0x00000008)
#define FPRINT_USE_F_SHA256     UINT32_C(0x00000010)
#define FPRINT_USE_F_SHA512     UINT32_C(0x00000020)
#define FPRINT_USE_S_EXTR       UINT32_C(0x00000040)
#define FPRINT_USE_S_SHA1       UINT32_C(0x00000080)
#define FPRINT_USE_S_SHA256     UINT32_C(0x00000100)
#define FPRINT_USE_S_SHA512     UINT32_C(0x00000200)

#define FILE_BUFFER_SIZE            1024

#define L1_DIRP_RECORD_ARR_SIZE     1000
#define L2_DIRP_RECORD_INIT_SIZE    1
#define L2_DIRP_RECORD_GROW_SIZE    1

#define CLASS_NUM   9
extern const uint64_t file_size_class_arr[CLASS_NUM];
extern const uint64_t sect_ideal_size_arr[CLASS_NUM];

typedef struct dirp_record dirp_record;

struct dirp_record {
    DIR* dirp;

    /* Pool allocator structure */
    obj_meta_data_fields;
};

typedefs_obj_arr(dirp_record)

/* structures for pool allocator */
layer1_obj_arr(dirp_record, dirp_record, L1_DIRP_RECORD_ARR_SIZE)
layer2_obj_arr(dirp_record)

int init_layer2_dirp_record_arr(layer2_dirp_record_arr* l2_arr);
int add_dirp_record_to_layer2_arr(layer2_dirp_record_arr* l2_arr, dirp_record** record, bit_index* index);
int del_dirp_record_from_layer2_arr(layer2_dirp_record_arr* l2_arr, bit_index index);
int get_dirp_record_from_layer2_arr(layer2_dirp_record_arr* l2_arr, dirp_record** result, bit_index index);
int get_l1_dirp_record_from_layer2_arr(layer2_dirp_record_arr* l2_arr, layer1_dirp_record_arr** result, bit_index index_of_l1_arr);
int del_l2_dirp_record_arr(layer2_dirp_record_arr* l2_arr);

int fill_rand_name(linked_entry* entry);

int gen_tree (database_handle* dh, char* path, linked_entry* parent, uint32_t flags, unsigned char recursive, ffp_eid_int* rem_depth_p, error_handle* er_h, linked_entry** entry_being_used, FILE** file_being_used, layer2_dirp_record_arr* l2_dirp_record_arr, bit_index* max_dirp_record_index);

int fingerprint_file(database_handle* dh, char* file_name, linked_entry* entry, uint32_t flags, error_handle* er_h, FILE** file_being_used);

int compare_fingerprint(linked_entry* entry1, linked_entry* entry2, uint16_t result_flags);

#endif
