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

#include "ffp_database.h"
#include "ffp_directory.h"
#include "ffp_database_function_template.h"
#include "ffprinter_function_template.h"
#include "ffp_term.h"
#include "ffp_scanmem.h"

#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

int verify_entry (database_handle* dh, linked_entry* entry, uint32_t flags) {     // should only be omitted for manual repair
    int i;
    uint64_t sect_indx;
    uint32_t extr_indx;
    uint16_t chks_indx;
    volatile extract_sample* temp_extract;
    volatile checksum_result* temp_checksum;
    str_len_int tag_num;
    str_len_int tag_min_len;
    str_len_int tag_max_len;
    volatile linked_entry*      temp_entry;
    volatile linked_entry       derefed_entry;
    volatile section*           temp_section;
    volatile file_data*         temp_file_data;
    volatile file_data          derefed_file_data;
    volatile section            derefed_section;
    volatile section*           derefed_section_p;
    volatile t_fn_to_e          derefed_fn_to_e;
    volatile t_tag_to_e         derefed_tag_to_e;
    volatile t_sha1f_to_fd      derefed_sha1f_to_fd;
    volatile t_sha256f_to_fd    derefed_sha256f_to_fd;
    volatile t_sha512f_to_fd    derefed_sha512f_to_fd;
    volatile t_sha1s_to_s       derefed_sha1s_to_s;
    volatile t_sha256s_to_s     derefed_sha256s_to_s;
    volatile t_sha512s_to_s     derefed_sha512s_to_s;
    volatile t_f_size_to_fd     derefed_f_size_to_fd;
    volatile dtt_hour           derefed_tod_hour;
    volatile dtt_hour           derefed_tom_hour;
    volatile dtt_hour           derefed_tusr_hour;

    char id_buf[EID_STR_MAX+1];

    debug_printf("verifying entry\n");

    if (!entry) {
        printf("verify_entry : entry is null\n");
        return WRONG_ARGS;
    }

    debug_printf("dereferencing entry pointer\n");
    derefed_entry = *entry;
    debug_printf("dereferencing was successful\n");

    debug_printf("checking of entry id string is terminated properly\n");
    if (verify_str_terminated(entry->entry_id_str, EID_STR_MAX, NULL, 0x0)) {
        printf("verify_entry : entry id string is not terminated properly\n");
        return VERIFY_FAIL;
    }

    if (entry == &dh->tree) {   // skip most tests if tree root
        debug_printf("entry is database tree root\n");

        debug_printf("checking if id byte array is all 0\n");
        for (i = 0; i < EID_LEN; i++) {
            if (entry->entry_id[i] != 0) {
                break;
            }
        }
        if (i < EID_LEN) {
            printf("verify_entry : entry is tree root, but id byte array not all 0\n");
            return VERIFY_FAIL;
        }
        debug_printf("id byte array passed check\n");

        debug_printf("checking if id string is all \'0\'\n");
        bytes_to_hex_str(id_buf, entry->entry_id, EID_LEN);

        if (strcmp(id_buf, entry->entry_id_str) != 0) {
            printf("verify_entry : entry is tree root, but id string not all \'0\'\n");
            return VERIFY_FAIL;
        }

        return 0;
    }

    debug_printf("checking parent\n");

    if (!entry->parent) {
        printf("verify_entry : parent is null\n");
        return VERIFY_FAIL;
    }

    debug_printf("dereferencing parent pointer\n");
    derefed_entry = *entry;
    debug_printf("dereferencing was successful\n");

    debug_printf("checking children\n");

    if (entry->child_num == 0 && entry->child_free_num == 0) {
        if (entry->child) {
            printf("verify_entry : children pointer array pointer is not null\n");
            return VERIFY_FAIL;
        }
    }
    else {
        if (!entry->child) {
            printf("verify_entry : children pointer array pointer is null\n");
            return VERIFY_FAIL;
        }
        debug_printf("dereferencing pointers to child pointer\n");
        for (i = 0; i < entry->child_num; i++) {
            debug_printf("dereferencing pointer to child pointer #%d\n", i);
            temp_entry = entry->child[i];
            debug_printf("dereferencing was successful\n");
        }
        debug_printf("dereferencing pointers to children entrie\n");
        for (i = 0; i < entry->child_num; i++) {
            temp_entry = entry->child[i];
            if (!temp_entry && flags & ALLOW_NULL_CHILD_PTR) {
                debug_printf("skipping dereferencing pointer to child entry #%d, value is NULL, flag ALLOW_NULL_CHILD_PTR enabled\n", i);
                continue;
            }
            debug_printf("dereferencing pointer to child entry #%d\n", i);
            derefed_entry = *temp_entry;
            debug_printf("dereferencing was successful\n");
        }
    }

    debug_printf("verifying prev_same_fn\n");
    if (entry->prev_same_fn) {
        debug_printf("dereferencing prev_same_fn\n");
        derefed_entry = *entry->prev_same_fn;
        debug_printf("dereferencing was successful\n");
    }
    debug_printf("verifying next_same_fn\n");
    if (entry->next_same_fn) {
        debug_printf("dereferencing next_same_fn\n");
        derefed_entry = *entry->next_same_fn;
        debug_printf("dereferencing was successful\n");
    }
    debug_printf("verifying parent fn_to_e\n");
    if (!entry->fn_to_e) {
        printf("verify_entry : parent fn_to_e missing\n");
        return VERIFY_FAIL;
    }
    debug_printf("dereferencing parent fn_to_e\n");
    derefed_fn_to_e = *entry->fn_to_e;
    debug_printf("dereferencing was successful\n");
    debug_printf("verifying prev_same_tag\n");
    if (entry->prev_same_tag) {
        debug_printf("dereferencing prev_same_tag\n");
        derefed_entry = *entry->prev_same_tag;
        debug_printf("dereferencing was successful\n");
    }
    debug_printf("verifying next_same_tag\n");
    if (entry->next_same_tag) {
        debug_printf("dereferencing next_same_tag\n");
        derefed_entry = *entry->next_same_tag;
        debug_printf("dereferencing was successful\n");
    }
    if (entry->tag_str_len == 0) {
        debug_printf("tag not used, tag related testing skipped\n");
    }
    else {
        debug_printf("verifying parent tag_to_e\n");
        if (!entry->tag_to_e) {
            printf("verify_entry : parent tag_to_e missing\n");
            return VERIFY_FAIL;
        }
        debug_printf("dereferencing parent tag_to_e\n");
        derefed_tag_to_e = *entry->tag_to_e;
        debug_printf("dereferencing was successful\n");
    }
    if (!entry->tod_utc_used) {
        debug_printf("time of addition fields not used, tod related testing skipped\n");
    }
    else {
        debug_printf("verifying tod_prev_sort_min\n");
        if (entry->tod_prev_sort_min) {
            debug_printf("dereferencing tod_prev_sort_min\n");
            derefed_entry = *entry->tod_prev_sort_min;
            debug_printf("dereferencing was successful\n");
        }
        debug_printf("verifying tod_next_sort_min\n");
        if (entry->tod_next_sort_min) {
            debug_printf("dereferencing tod_next_sort_min\n");
            derefed_entry = *entry->tod_next_sort_min;
            debug_printf("dereferencing was successful\n");
        }
        debug_printf("verifying tod_hour\n");
        if (!entry->tod_hour) {
            printf("verify_entry : tod_hour missing\n");
            return VERIFY_FAIL;
        }
        debug_printf("dereferencing tod_hour\n");
        derefed_tod_hour = *entry->tod_hour;
        debug_printf("dereferencing was successful\n");
    }
    if (!entry->tom_utc_used) {
        debug_printf("time of modification fields not used, tod related testing skipped\n");
    }
    else {
        debug_printf("verifying tom_prev_sort_min\n");
        if (entry->tom_prev_sort_min) {
            debug_printf("dereferencing tom_prev_sort_min\n");
            derefed_entry = *entry->tom_prev_sort_min;
            debug_printf("dereferencing was successful\n");
        }
        debug_printf("verifying tom_next_sort_min\n");
        if (entry->tom_next_sort_min) {
            debug_printf("dereferencing tom_next_sort_min\n");
            derefed_entry = *entry->tom_next_sort_min;
            debug_printf("dereferencing was successful\n");
        }
        debug_printf("verifying tom_hour\n");
        if (!entry->tom_hour) {
            printf("verify_entry : tom_hour missing\n");
            return VERIFY_FAIL;
        }
        debug_printf("dereferencing tom_hour\n");
        derefed_tom_hour = *entry->tom_hour;
        debug_printf("dereferencing was successful\n");
    }
    if (!entry->tusr_utc_used) {
        debug_printf("user specified time fields not used, tod related testing skipped\n");
    }
    else {
        debug_printf("verifying tusr_prev_sort_min\n");
        if (entry->tusr_prev_sort_min) {
            debug_printf("dereferencing tusr_prev_sort_min\n");
            derefed_entry = *entry->tusr_prev_sort_min;
            debug_printf("dereferencing was successful\n");
        }
        debug_printf("verifying tusr_next_sort_min\n");
        if (entry->tusr_next_sort_min) {
            debug_printf("dereferencing tusr_next_sort_min\n");
            derefed_entry = *entry->tusr_next_sort_min;
            debug_printf("dereferencing was successful\n");
        }
        debug_printf("verifying tusr_hour\n");
        if (!entry->tusr_hour) {
            printf("verify_entry : tusr_hour missing\n");
            return VERIFY_FAIL;
        }
        debug_printf("dereferencing tusr_hour\n");
        derefed_tusr_hour = *entry->tusr_hour;
        debug_printf("dereferencing was successful\n");
    }

    debug_printf("verifying branch id byte array\n");
    for (i = 0; i < EID_LEN; i++) {
        if (entry->branch_id[i] != 0) {
            break;
        }
    }
    if (i == EID_LEN) {
        printf("verify_entry : invalid branch id byte array\n");
        return VERIFY_FAIL;
    }
    debug_printf("verifying if branch id string is consistent with byte array\n");
    bytes_to_hex_str(id_buf, entry->branch_id, EID_LEN);
    if (strcmp(id_buf, entry->branch_id_str) != 0) {
        printf("verify_entry : branch id string does not match branch id byte array\n");
        return VERIFY_FAIL;
    }
    if (entry->has_parent) {
        debug_printf("verifying if branch id byte array matches with parent's branch id byte array\n");
        for (i = 0; i < EID_LEN; i++) {
            if (entry->branch_id[i] != entry->parent->branch_id[i]) {
                break;
            }
        }
        if (i < EID_LEN) {
            printf("verify_entry : branch id byte array does not match parent's branch id byte array\n");
            return VERIFY_FAIL;
        }
    }

    debug_printf("verifying depth\n");
    if (!entry->has_parent) {   // head of branch
        if (entry->depth != 1) {
            printf("verify_entry : depth is invalid\n");
            return VERIFY_FAIL;
        }
    }
    else {
        if (entry->depth != entry->parent->depth + 1) {
            printf("verify_entry : depth is invalid\n");
            return VERIFY_FAIL;
        }
    }

    debug_printf("verifying entry id byte array\n");
    for (i = 0; i < EID_LEN; i++) {
        if (entry->entry_id[i] != 0) {
            break;
        }
    }
    if (i == EID_LEN) {
        printf("verify_entry : invalid entry id byte array\n");
        return VERIFY_FAIL;
    }
    debug_printf("verifying if entry id string is consistent with byte array\n");
    bytes_to_hex_str(id_buf, entry->entry_id, EID_LEN);
    if (strcmp(id_buf, entry->entry_id_str) != 0) {
        printf("verify_entry : entry id string does not match entry id byte array\n");
        return VERIFY_FAIL;
    }

    debug_printf("verifying type\n");
    if (        entry->type != ENTRY_FILE
            &&  entry->type != ENTRY_GROUP
       ) 
    {
        printf("verify_entry : invalid type\n");
        return VERIFY_FAIL;
    }

    debug_printf("verifying created by flag\n");
    if (        entry->created_by != CREATED_BY_SYS
            &&  entry->created_by != CREATED_BY_USR
       )
    {
        printf("verify_entry : invalid created by flag\n");
        return VERIFY_FAIL;
    }

    debug_printf("verifying file name\n");
    if (entry->file_name_len == 0) {
        printf("verify_entry : entry name has zero length\n");
        return VERIFY_FAIL;
    }
    if (entry->file_name_len > FILE_NAME_MAX) {
        printf("verify_entry : entry name exceeds maximum\n");
        return VERIFY_FAIL;
    }
    if (entry->file_name[entry->file_name_len] != 0) {
        printf("verify_entry : file name is not null terminated\n");
        return VERIFY_FAIL;
    }

    if (entry->tag_str_len > 0) {
        debug_printf("verifying tag string\n");
        if (entry->tag_str[entry->tag_str_len] != 0) {
            printf("verify_entry : tag string is not null terminated\n");
            return VERIFY_FAIL;
        }

        if (entry->tag_str_len <= 2) {
            printf("verify_entry : tag string length too short\n");
            return VERIFY_FAIL;
        }
        if (entry->tag_str_len > TAG_STR_MAX) {
            printf("verify_entry : tag string length exceeds maximum\n");
            return VERIFY_FAIL;
        }

        if (    entry->tag_str[0] != '|'
                ||
                entry->tag_str[entry->tag_str_len-1] != '|'
                ||
                (       entry->tag_str[entry->tag_str_len-1] == '|'
                    &&  entry->tag_str[entry->tag_str_len-2] == '\\'
                )
           )
        {
            printf("verify_entry : tag string is not correctly formatted\n");
            return VERIFY_FAIL;
        }

        tag_count(entry->tag_str, &tag_num, &tag_min_len, &tag_max_len);
        if (tag_num > TAG_MAX_NUM) {
            printf("verify_entry : too many tags\n");
            return VERIFY_FAIL;
        }
        if (tag_max_len > TAG_LEN_MAX) {
            printf("verify_entry : one of the tags is too long\n");
            return WRONG_ARGS;
        }
        if (tag_min_len == 0) {
            printf("verify_entry : one of the tags is empty\n");
        }
    }

    if (entry->user_msg_len > 0) {
        debug_printf("verifying user message\n");
        if (entry->user_msg_len > USER_MSG_MAX) {
            printf("verify_entry : user message exceeds length\n");
            return VERIFY_FAIL;
        }
        if (entry->user_msg[entry->user_msg_len] != 0) {
            printf("verify_entry : user message is not null terminated\n");
            return VERIFY_FAIL;
        }
    }

    if (entry->tod_utc_used) {
        debug_printf("verifying time of addition\n");
        if (entry->tod_utc.tm_mon > 11) {
            printf("verify_entry : invalid month number\n");
            return VERIFY_FAIL;
        }
        if (entry->tod_utc.tm_mday < 1 || entry->tod_utc.tm_mday > 31) {
            printf("verify_entry : invalid day number\n");
            return VERIFY_FAIL;
        }
        if (entry->tod_utc.tm_hour > 23) {
            printf("verify_entry : invalid hour number\n");
            return VERIFY_FAIL;
        }
        if (entry->tod_utc.tm_min > 59) {
            printf("verify_entry : invalid minute number\n");
            return VERIFY_FAIL;
        }
        // other fields are ignored
    }

    if (entry->tusr_utc_used) {
        debug_printf("verifying user specified time\n");
        if (entry->tusr_utc.tm_mon > 11) {
            printf("verify_entry : invalid month number\n");
            return VERIFY_FAIL;
        }
        if (entry->tusr_utc.tm_mday < 1 || entry->tusr_utc.tm_mday > 31) {
            printf("verify_entry : invalid day number\n");
            return VERIFY_FAIL;
        }
        if (entry->tusr_utc.tm_hour > 23) {
            printf("verify_entry : invalid hour number\n");
            return VERIFY_FAIL;
        }
        if (entry->tusr_utc.tm_min > 59) {
            printf("verify_entry : invalid minute number\n");
            return VERIFY_FAIL;
        }
        // other fields are ignored
    }

    // not checking hash handle
    debug_printf("ignoring hash handle\n");

    debug_printf("verifying file data\n");
    if (!entry->data) {
        debug_printf("no file data is present\n");
        goto skip_file;
    }
    debug_printf("dereferencing file data pointer\n");
    derefed_file_data = *entry->data;
    debug_printf("dereferencing was successful\n");
    temp_file_data = entry->data;
    // sha1
    debug_printf("verifying prev_same_sha1 in file data\n");
    if (temp_file_data->prev_same_sha1) {
        debug_printf("dereferencing prev_same_sha1 in file data\n");
        derefed_file_data = *temp_file_data->prev_same_sha1;
        debug_printf("dereferencing was successful\n");
    }
    debug_printf("verifying next_same_sha1 in file data\n");
    if (temp_file_data->next_same_sha1) {
        debug_printf("dereferencing next_same_sha1 in file data\n");
        derefed_file_data = *temp_file_data->next_same_sha1;
        debug_printf("dereferencing was successful\n");
    }
    debug_printf("verifying parent sha1f_to_fd\n");
    if (temp_file_data->sha1f_to_fd) {
        debug_printf("dereferencing parent sha1f_to_fd\n");
        derefed_sha1f_to_fd = *temp_file_data->sha1f_to_fd;
        debug_printf("dereferencing was successful\n");
    }
    // sha256
    debug_printf("verifying prev_same_sha256 in file data\n");
    if (temp_file_data->prev_same_sha256) {
        debug_printf("dereferencing prev_same_sha256 in file data\n");
        derefed_file_data = *temp_file_data->prev_same_sha256;
        debug_printf("dereferencing was successful\n");
    }
    debug_printf("verifying next_same_sha256 in file data\n");
    if (temp_file_data->next_same_sha256) {
        debug_printf("dereferencing next_same_sha256 in file data\n");
        derefed_file_data = *temp_file_data->next_same_sha256;
        debug_printf("dereferencing was successful\n");
    }
    debug_printf("verifying parent sha256f_to_fd\n");
    if (temp_file_data->sha256f_to_fd) {
        debug_printf("dereferencing parent sha256f_to_fd\n");
        derefed_sha256f_to_fd = *temp_file_data->sha256f_to_fd;
        debug_printf("dereferencing was successful\n");
    }
    // sha512
    debug_printf("verifying prev_same_sha512 in file data\n");
    if (temp_file_data->prev_same_sha512) {
        debug_printf("dereferencing prev_same_sha512 in file data\n");
        derefed_file_data = *temp_file_data->prev_same_sha512;
        debug_printf("dereferencing was successful\n");
    }
    debug_printf("verifying next_same_sha512 in file data\n");
    if (temp_file_data->next_same_sha512) {
        debug_printf("dereferencing next_same_sha512 in file data\n");
        derefed_file_data = *temp_file_data->next_same_sha512;
        debug_printf("dereferencing was successful\n");
    }
    debug_printf("verifying parent sha512f_to_fd\n");
    if (temp_file_data->sha512f_to_fd) {
        debug_printf("dereferencing parent sha512f_to_fd\n");
        derefed_sha512f_to_fd = *temp_file_data->sha512f_to_fd;
        debug_printf("dereferencing was successful\n");
    }

    debug_printf("verifying checksum\n");
    for (chks_indx = 0; chks_indx < CHECKSUM_MAX_NUM; chks_indx++) {
        temp_checksum = temp_file_data->checksum + chks_indx;

        debug_printf("verifying checksum #%"PRIu16"\n", chks_indx);
        if (    temp_checksum->type != CHECKSUM_UNUSED
            &&  temp_checksum->type != CHECKSUM_SHA1_ID
            &&  temp_checksum->type != CHECKSUM_SHA256_ID
            &&  temp_checksum->type != CHECKSUM_SHA512_ID
           )
        {
            printf("verify_entry : checksum #%"PRIu16" invalid type\n", chks_indx);
            return VERIFY_FAIL;
        }

        if (temp_checksum->type == CHECKSUM_UNUSED) {
            continue;
        }

        if (temp_checksum->len > CHECKSUM_MAX_LEN) {
            printf("verify_entry : checksum #%"PRIu16" length exceeds maximum\n", chks_indx);
            return VERIFY_FAIL;
        }
        if (verify_str_terminated((char*) temp_checksum->checksum_str, CHECKSUM_STR_MAX, NULL, 0x0)) {
            printf("verify_entry : checksum #%"PRIu16" checksum_str not properly terminated\n", chks_indx);
            return VERIFY_FAIL;
        }
    }

    debug_printf("verifying extract\n");
    if (temp_file_data->extract_num > EXTRACT_MAX_NUM) {
        printf("verify_entry : number of extract exceeds maximum\n");
        return VERIFY_FAIL;
    }

    for (extr_indx = 0; extr_indx < temp_file_data->extract_num; extr_indx++) {
        temp_extract = temp_file_data->extract + extr_indx;

        debug_printf("verifying extract #%"PRIu32"\n", extr_indx);
        if (temp_extract->len > EXTRACT_SIZE_MAX) {
            printf("verify_entry : extract #%"PRIu32" length exceeds maximum\n", extr_indx);
            return VERIFY_FAIL;
        }
        if (temp_extract->position > temp_file_data->file_size - 1) {
            printf("verify_entry : extract #%"PRIu32" starting position exceeds or equal to file size\n", extr_indx);
            return VERIFY_FAIL;
        }
        if (temp_extract->position + temp_extract->len - 1 > temp_file_data->file_size - 1) {
            printf("verify_entry : extract #%"PRIu32" runs past end of file\n", extr_indx);
            return VERIFY_FAIL;
        }
    }
    debug_printf("verifying file size\n");
    debug_printf("verifying prev_same_f_size\n");
    if (temp_file_data->prev_same_f_size) {
        debug_printf("dereferencing prev_same_f_size\n");
        derefed_file_data = *temp_file_data->prev_same_f_size;
        debug_printf("dereferencing was successful\n");
    }
    debug_printf("verifying next_same_f_size\n");
    if (temp_file_data->next_same_f_size) {
        debug_printf("dereferencing next_same_f_size\n");
        derefed_file_data = *temp_file_data->next_same_f_size;
        debug_printf("dereferencing was successful\n");
    }
    debug_printf("verifying parent f_size_to_fd\n");
    if (temp_file_data->f_size_to_fd) {
        debug_printf("dereferencing parent f_size_to_fd\n");
        derefed_f_size_to_fd = *temp_file_data->f_size_to_fd;
        debug_printf("dereferencing was successful\n");
    }

    debug_printf("verifying sections\n");
    if (temp_file_data->section_num == 0 && temp_file_data->section_free_num == 0) {
        if (temp_file_data->section) {
            printf("verify_entry : section pointer not null but entry has 0 sections\n");
            return VERIFY_FAIL;
        }
        if (temp_file_data->norm_sect_size != 0 || temp_file_data->last_sect_size != 0) {
            printf("verify_entry : invalid section size, both norm and last should be zero\n");
            return VERIFY_FAIL;
        }
        goto skip_section;
    }
    else {
        if (!temp_file_data->section) {
            printf("verify_entry : null section pointer\n");
            return VERIFY_FAIL;
        }
        if (temp_file_data->norm_sect_size > temp_file_data->file_size) {
            printf("verify_entry : normal section size exceeds file size\n");
            return VERIFY_FAIL;
        }
        if (temp_file_data->last_sect_size > temp_file_data->norm_sect_size) {
            printf("verify_entry : last section size exceeds normal section size\n");
            return VERIFY_FAIL;
        }
        if (temp_file_data->section_num == 1 && temp_file_data->last_sect_size != temp_file_data->norm_sect_size) {
            printf("verify_entry : normal section size is not consistent with last section size\n");
            return VERIFY_FAIL;
        }
        if (temp_file_data->norm_sect_size == 0 && temp_file_data->last_sect_size != 0) {
            printf("verify_entry : last section size is not consistent with normal section size\n");
            return VERIFY_FAIL;
        }
    }

    debug_printf("dereferencing section pointers\n");
    for (sect_indx = 0; sect_indx < temp_file_data->section_num; sect_indx++) {
        debug_printf("dereferencing pointer to section pointer #%"PRIu64"\n", sect_indx);
        derefed_section_p = temp_file_data->section[sect_indx];
        debug_printf("dereferencing was successful\n");

        debug_printf("dereferencing pointer to section\n");
        derefed_section = *derefed_section_p;
        debug_printf("dereferencing was successful\n");
    }

    for (sect_indx = 0; sect_indx < temp_file_data->section_num; sect_indx++) {
        debug_printf("verifying section #%"PRIu64"\n", sect_indx);
        temp_section = temp_file_data->section[sect_indx];

        if (temp_section->start_pos >= temp_section->end_pos) {
            printf("verify_entry : start position crosses over or overlaps with end pos\n");
            return VERIFY_FAIL;
        }

        if (temp_section->start_pos >= temp_file_data->file_size) {
            printf("verify_entry : start position crosses over end of file\n");
            return VERIFY_FAIL;
        }
        if (temp_section->end_pos   >= temp_file_data->file_size) {
            printf("verify_entry : end position crosses over end of file\n");
            return VERIFY_FAIL;
        }

        if (temp_file_data->norm_sect_size > 0) {  // continuous sections
            if (sect_indx > 0) {
                if (temp_section->start_pos <= temp_file_data->section[sect_indx-1]->end_pos) {
                    printf("verify_entry : section overlaps with the previous one\n");
                    return VERIFY_FAIL;
                }
            }
        }

        // sha1
        debug_printf("verifying prev_same_sha1 in section\n");
        if (temp_section->prev_same_sha1) {
            debug_printf("dereferencing prev_same_sha1 in section\n");
            derefed_section = *temp_section->prev_same_sha1;
            debug_printf("dereferencing was successful\n");
        }
        debug_printf("verifying next_same_sha1 in section\n");
        if (temp_section->next_same_sha1) {
            debug_printf("dereferencing next_same_sha1 in section\n");
            derefed_section = *temp_section->next_same_sha1;
            debug_printf("dereferencing was successful\n");
        }
        debug_printf("verifying parent sha1_to_s\n");
        if (temp_section->sha1s_to_s) {
            debug_printf("dereferencing parent sha1s_to_s\n");
            derefed_sha1s_to_s = *temp_section->sha1s_to_s;
            debug_printf("dereferencing was successful\n");
        }
        // sha256
        debug_printf("verifying prev_same_sha256 in section\n");
        if (temp_section->prev_same_sha256) {
            debug_printf("dereferencing prev_same_sha256 in section\n");
            derefed_section = *temp_section->prev_same_sha256;
            debug_printf("dereferencing was successful\n");
        }
        debug_printf("verifying next_same_sha256 in section\n");
        if (temp_section->next_same_sha256) {
            debug_printf("dereferencing next_same_sha256 in section\n");
            derefed_section = *temp_section->next_same_sha256;
            debug_printf("dereferencing was successful\n");
        }
        debug_printf("verifying parent sha256_to_s\n");
        if (temp_section->sha256s_to_s) {
            debug_printf("dereferencing parent sha256s_to_s\n");
            derefed_sha256s_to_s = *temp_section->sha256s_to_s;
            debug_printf("dereferencing was successful\n");
        }
        // sha512
        debug_printf("verifying prev_same_sha512 in section\n");
        if (temp_section->prev_same_sha512) {
            debug_printf("dereferencing prev_same_sha512 in section\n");
            derefed_section = *temp_section->prev_same_sha512;
            debug_printf("dereferencing was successful\n");
        }
        debug_printf("verifying next_same_sha512 in section\n");
        if (temp_section->next_same_sha512) {
            debug_printf("dereferencing next_same_sha512 in section\n");
            derefed_section = *temp_section->next_same_sha512;
            debug_printf("dereferencing was successful\n");
        }
        debug_printf("verifying parent sha512_to_s\n");
        if (temp_section->sha512s_to_s) {
            debug_printf("dereferencing parent sha512s_to_s\n");
            derefed_sha512s_to_s = *temp_section->sha512s_to_s;
            debug_printf("dereferencing was successful\n");
        }

        debug_printf("verifying checksum\n");
        for (chks_indx = 0; chks_indx < CHECKSUM_MAX_NUM; chks_indx++) {
            temp_checksum = temp_section->checksum + chks_indx;

            debug_printf("verifying checksum #%"PRIu16"\n", chks_indx);
            if (    temp_checksum->type != CHECKSUM_UNUSED
                &&  temp_checksum->type != CHECKSUM_SHA1_ID
                &&  temp_checksum->type != CHECKSUM_SHA256_ID
                &&  temp_checksum->type != CHECKSUM_SHA512_ID
                )
            {
                printf("verify_entry : checksum #%"PRIu16" invalid type\n", chks_indx);
                return VERIFY_FAIL;
            }

            if (temp_checksum->type == CHECKSUM_UNUSED) {
                continue;
            }

            if (temp_checksum->len > CHECKSUM_MAX_LEN) {
                printf("verify_entry : checksum #%"PRIu16" length exceeds maximum\n", chks_indx);
                return VERIFY_FAIL;
            }
            if (verify_str_terminated((char*) temp_checksum->checksum_str, CHECKSUM_STR_MAX, 0, 0)) {
                printf("verify_entry : checksum #%"PRIu16" checksum_str not properly terminated\n", chks_indx);
                return VERIFY_FAIL;
            }
        }

        debug_printf("verifying extract\n");
        if (temp_section->extract_num > EXTRACT_MAX_NUM) {
            printf("verify_entry : number of extract exceeds maximum\n");
            return VERIFY_FAIL;
        }
        for (extr_indx = 0; extr_indx < temp_section->extract_num; extr_indx++) {
            temp_extract = temp_section->extract + extr_indx;

            debug_printf("verifying extract #%"PRIu32"\n", extr_indx);
            if (temp_extract->len > EXTRACT_SIZE_MAX) {
                printf("verify_entry : extract #%"PRIu32" length exceeds maximum\n", extr_indx);
                return VERIFY_FAIL;
            }
            else if (temp_extract->len == 0) {
                printf("verify_entry : extract #%"PRIu32" is empty\n", extr_indx);
                return VERIFY_FAIL;
            }

            if (temp_extract->position > entry->data->file_size - 1) {
                printf("verify_entry : extract #%"PRIu32" starting position exceeds or equal to file size\n", extr_indx);
                return VERIFY_FAIL;
            }
            if (temp_extract->position + temp_extract->len - 1 > entry->data->file_size - 1) {
                printf("verify_entry : extract #%"PRIu32" runs past end of file\n", extr_indx);
                return VERIFY_FAIL;
            }

            if (temp_extract->position < temp_section->start_pos) {
                printf("verify_entry : extract %"PRIu32" starts before the section starts\n", extr_indx);
                return VERIFY_FAIL;
            }
            if (temp_extract->position > temp_section->end_pos) {
                printf("verify_entry : extract %"PRIu32" starting position exceeds section ending position\n", extr_indx);
                return VERIFY_FAIL;
            }
            if (temp_extract->position + temp_extract->len - 1 > temp_section->end_pos) {
                printf("verify_entry : extract %"PRIu32" runs past end of section\n", extr_indx);
                return VERIFY_FAIL;
            }
        }
    }

skip_section:

skip_file:

    return 0;
}

verify_generic_trans_struct_one_to_one(eid, e, EID_STR_MAX)

verify_generic_trans_struct_one_to_many(fn, e, prev_same_fn, next_same_fn, linked_entry, FILE_NAME_MAX)

verify_generic_trans_struct_one_to_many(tag, e, prev_same_tag, next_same_tag, linked_entry, TAG_STR_MAX)

verify_generic_trans_struct_one_to_many(sha1f, fd, prev_same_sha1, next_same_sha1, file_data, CHECKSUM_STR_MAX)
verify_generic_trans_struct_one_to_many(sha256f, fd, prev_same_sha256, next_same_sha256, file_data, CHECKSUM_STR_MAX)
verify_generic_trans_struct_one_to_many(sha512f, fd, prev_same_sha512, next_same_sha512, file_data, CHECKSUM_STR_MAX)

verify_generic_trans_struct_one_to_many(sha1s, s, prev_same_sha1, next_same_sha1, section, CHECKSUM_STR_MAX)
verify_generic_trans_struct_one_to_many(sha256s, s, prev_same_sha256, next_same_sha256, section, CHECKSUM_STR_MAX)
verify_generic_trans_struct_one_to_many(sha512s, s, prev_same_sha512, next_same_sha512, section, CHECKSUM_STR_MAX)

verify_generic_trans_struct_one_to_many(f_size, fd, prev_same_f_size, next_same_f_size, file_data, FILE_SIZE_STR_MAX)

int lookup_db_name (database_handle* dh_table, const char* name, database_handle** result) {
    database_handle* temp_dh_find;

    HASH_FIND_STR(dh_table, name, temp_dh_find);
    if (!temp_dh_find) {
        *result = 0;
        return FIND_FAIL;
    }

    *result = temp_dh_find;
    return 0;
}

int add_db (database_handle** dh_table, database_handle* target) {
    database_handle* temp_dh_find;

    HASH_FIND_STR(*dh_table, target->name, temp_dh_find);
    if (temp_dh_find) {
        return DUPLICATE_ERROR;
    }

    HASH_ADD_STR(*dh_table, name, target);

    return 0;
}

int del_db (database_handle** dh_table, database_handle* dh) {
    database_handle* temp_dh_find;

    HASH_FIND_STR(*dh_table, dh->name, temp_dh_find);
    if (!temp_dh_find) {
        return FIND_FAIL;
    }

    HASH_DEL(*dh_table, temp_dh_find);

    return 0;
}

/* Entry ID lookup */
lookup_generic_one_to_one_tran_via_dh(database_handle, eid, e, entry_id, linked_entry)

lookup_generic_part_map_via_dh(database_handle, eid, e, entry_id, EID_STR_MAX)

lookup_generic_part_via_dh(database_handle, eid, e, entry_id, EID_STR_MAX)

/* File name lookup */
lookup_generic_one_to_many_tran_via_dh(database_handle, fn, e, file_name, linked_entry)

lookup_generic_part_map_via_dh(database_handle, fn, e, file_name, FILE_NAME_MAX)

lookup_generic_part_via_dh(database_handle, fn, e, file_name, FILE_NAME_MAX)

/* Tag lookup */
lookup_generic_one_to_many_tran_via_dh(database_handle, tag, e, tag_str, linked_entry)

lookup_generic_part_map_via_dh(database_handle, tag, e, tag, TAG_STR_MAX)

lookup_generic_part_via_dh(database_handle, tag, e, tag, TAG_STR_MAX)

int lookup_tag_via_dh_with_preproc (database_handle* dh, const char* in_tag, simple_bitmap* map_buf, simple_bitmap* map_result, t_tag_to_e** result_buf, bit_index buf_size, bit_index* num_used) {
    char tag[TAG_STR_MAX+1];

    str_len_int in_tag_len;

    int i, j;

    if ((i = verify_str_terminated(in_tag, TAG_STR_MAX, &in_tag_len, 0))) {
        return i;
    }

    // process the tag
    tag[0] = '|';
    for (i = 0, j = 1; i < in_tag_len; i++, j++) {
        if (in_tag[i] == '|') {
            tag[j] = '\\';
            j++;
            tag[j] = '|';
        }
        else {
            tag[j] = in_tag[i];
        }
    }
    tag[j] = '|';
    tag[j+1] = 0;

    return lookup_tag_part_via_dh(dh, tag, map_buf, map_result, result_buf, buf_size, num_used);
}

int lookup_tag_map_via_dh_with_preproc (database_handle* dh, const char* in_tag, simple_bitmap* map_buf, simple_bitmap* map_result) {
    char tag[TAG_STR_MAX+1];

    str_len_int in_tag_len;

    int i, j;

    if ((i = verify_str_terminated(in_tag, TAG_STR_MAX, &in_tag_len, 0))) {
        return i;
    }

    // process the tag
    tag[0] = '|';
    for (i = 0, j = 1; i < in_tag_len; i++, j++) {
        if (in_tag[i] == '|') {
            tag[j] = '\\';
            j++;
            tag[j] = '|';
        }
        else {
            tag[j] = in_tag[i];
        }
    }
    tag[j] = '|';
    tag[j+1] = 0;

    return lookup_tag_part_map_via_dh(dh, tag, map_buf, map_result);
}

int lookup_tag_part_via_dh_with_preproc(database_handle* dh, const char* in_tag, simple_bitmap* map_buf, simple_bitmap* map_result, t_tag_to_e** result_buf, bit_index buf_size, bit_index* num_used) {
    char tag[TAG_STR_MAX];

    str_len_int in_tag_len;

    int i, j;

    if ((i = verify_str_terminated(in_tag, TAG_STR_MAX, &in_tag_len, 0))) {
        return i;
    }

    // process the tag
    for (i = 0, j = 0; i < in_tag_len; i++, j++) {
        if (in_tag[i] == '|') {
            tag[j] = '\\';
            j++;
            tag[j] = '|';
        }
        else {
            tag[j] = in_tag[i];
        }
    }
    tag[j] = 0;

    return lookup_tag_part_via_dh(dh, tag, map_buf, map_result, result_buf, buf_size, num_used);
}

int lookup_tag_part_map_via_dh_with_preproc (database_handle* dh, const char* in_tag, simple_bitmap* map_buf, simple_bitmap* map_result) {
    char tag[TAG_STR_MAX+1];

    str_len_int in_tag_len;

    int i, j;

    if ((i = verify_str_terminated(in_tag, TAG_STR_MAX, &in_tag_len, 0))) {
        return i;
    }

    // process the tag
    for (i = 0, j = 0; i < in_tag_len; i++, j++) {
        if (in_tag[i] == '|') {
            tag[j] = '\\';
            j++;
            tag[j] = '|';
        }
        else {
            tag[j] = in_tag[i];
        }
    }
    tag[j] = 0;

    return lookup_tag_part_map_via_dh(dh, tag, map_buf, map_result);
}

/* File data checksum lookup */
lookup_generic_one_to_many_tran_via_dh(database_handle, sha1f,      fd, file_sha1,      file_data)
lookup_generic_one_to_many_tran_via_dh(database_handle, sha256f,    fd, file_sha256,    file_data)
lookup_generic_one_to_many_tran_via_dh(database_handle, sha512f,    fd, file_sha512,    file_data)

lookup_generic_part_map_via_dh(database_handle, sha1f,      fd, file_sha1,      CHECKSUM_STR_MAX)
lookup_generic_part_map_via_dh(database_handle, sha256f,    fd, file_sha256,    CHECKSUM_STR_MAX)
lookup_generic_part_map_via_dh(database_handle, sha512f,    fd, file_sha512,    CHECKSUM_STR_MAX)

lookup_generic_part_via_dh(database_handle, sha1f,      fd, file_sha1,      CHECKSUM_STR_MAX)
lookup_generic_part_via_dh(database_handle, sha256f,    fd, file_sha256,    CHECKSUM_STR_MAX)
lookup_generic_part_via_dh(database_handle, sha512f,    fd, file_sha512,    CHECKSUM_STR_MAX)

/* Section checksum lookup */
lookup_generic_one_to_many_tran_via_dh(database_handle, sha1s,      s, sect_sha1,   section)
lookup_generic_one_to_many_tran_via_dh(database_handle, sha256s,    s, sect_sha256, section)
lookup_generic_one_to_many_tran_via_dh(database_handle, sha512s,    s, sect_sha512, section)

lookup_generic_part_map_via_dh(database_handle, sha1s,      s, sect_sha1,   CHECKSUM_STR_MAX)
lookup_generic_part_map_via_dh(database_handle, sha256s,    s, sect_sha256, CHECKSUM_STR_MAX)
lookup_generic_part_map_via_dh(database_handle, sha512s,    s, sect_sha512, CHECKSUM_STR_MAX)

lookup_generic_part_via_dh(database_handle, sha1s,      s, sect_sha1,   CHECKSUM_STR_MAX)
lookup_generic_part_via_dh(database_handle, sha256s,    s, sect_sha256, CHECKSUM_STR_MAX)
lookup_generic_part_via_dh(database_handle, sha512s,    s, sect_sha512, CHECKSUM_STR_MAX)

/* File size lookup */
lookup_generic_one_to_many_tran_via_dh(database_handle, f_size, fd, file_size, file_data)

lookup_generic_part_map_via_dh(database_handle, f_size, fd, f_size, FILE_SIZE_STR_MAX)

lookup_generic_part_via_dh(database_handle, f_size, fd, f_size, FILE_SIZE_STR_MAX)

/* Date time look up */
int lookup_date_time (database_handle* dh, unsigned char date_type, int16_t year, uint8_t month, uint8_t day, uint8_t hour, linked_entry** result) {
    dtt_year*   temp_year;
    dtt_month*  temp_month;
    dtt_day*    temp_day;
    dtt_hour*   temp_hour;

    if (date_type == DATE_TOD) {
        temp_year = dh->tod_dt_tree;
    }
    else if (date_type == DATE_TUSR) {
        temp_year = dh->tusr_dt_tree;
    }
    else {
        printf("lookup_date_time : invalid date type\n");
        return WRONG_ARGS;
    }

    while (temp_year) {
        if (temp_year->year == year) {
            break;
        }
        temp_year = temp_year->next;
    }
    if (!temp_year) {
        *result = 0;
        return FIND_FAIL;
    }

    temp_month = temp_year->month + month;

    temp_day = temp_month->day + day;

    temp_hour = temp_day->hour + hour;

    *result = temp_hour->head;
    return 0;
}

/* Sub-branch search */
int find_entry_in_sub_branch(database_handle* dh, linked_entry* sub_branch_head, field_record* rec, unsigned char only_examine_latest, field_match_bitmap* match_map, unsigned int score) {
    sect_field* temp_sect;

    uint64_t i;

    bit_index bit_indx;
    bit_index max_entry_map_length = 0;

    unsigned int total_metric_num = 0;
    unsigned int min_match_num;
    unsigned int match_count;

    map_block temp_result;

    if (score > 100) {
        return WRONG_ARGS;
    }

    // match name
    if (rec->field_usage_map & FIELD_REC_USE_E_NAME) {
        lookup_file_name_part_map_via_dh(dh, rec->name, &match_map->map_buf1, &match_map->map_buf2);

        part_name_trans_map_to_entry_map(&dh->l2_fn_to_e_arr, &match_map->map_buf2, &match_map->map_name_match, rec->name);

        if (match_map->map_name_match.length > max_entry_map_length) {
            max_entry_map_length = match_map->map_name_match.length;
        }
        total_metric_num++;
    }

    // match time of addition
    if (rec->field_usage_map & FIELD_REC_USE_E_TADD) {
    }

    // match time of modification
    if (rec->field_usage_map & FIELD_REC_USE_E_TMOD) {
    }

    // match user specified time
    if (rec->field_usage_map & FIELD_REC_USE_E_TMOD) {
    }

    // match tags

    // match file size
    if (rec->field_usage_map & FIELD_REC_USE_F_SIZE) {
        lookup_file_name_part_map_via_dh(dh, rec->f_size, &match_map->map_buf1, &match_map->map_buf2);

        part_f_size_trans_map_to_entry_map(&dh->l2_f_size_to_fd_arr, &match_map->map_buf2, &match_map->map_f_size_match, rec->f_size);

        if (match_map->map_f_size_match.length > max_entry_map_length) {
            max_entry_map_length = match_map->map_f_size_match.length;
        }
        total_metric_num++;
    }

    // match file sha1
    if (rec->field_usage_map & FIELD_REC_USE_F_SHA1) {
        lookup_file_sha1_part_map_via_dh(dh, rec->f_sha1, &match_map->map_buf1, &match_map->map_buf2);

        part_f_sha1_trans_map_to_entry_map(&dh->l2_sha1f_to_fd_arr, &match_map->map_buf2, &match_map->map_f_sha1_match, rec->f_sha1);

        if (match_map->map_f_sha1_match.length > max_entry_map_length) {
            max_entry_map_length = match_map->map_f_sha1_match.length;
        }
        total_metric_num++;
    }

    // match file sha256
    if (rec->field_usage_map & FIELD_REC_USE_F_SHA256) {
        lookup_file_sha256_part_map_via_dh(dh, rec->f_sha256, &match_map->map_buf1, &match_map->map_buf2);

        part_f_sha256_trans_map_to_entry_map(&dh->l2_sha256f_to_fd_arr, &match_map->map_buf2, &match_map->map_f_sha256_match, rec->f_sha256);

        if (match_map->map_f_sha256_match.length > max_entry_map_length) {
            max_entry_map_length = match_map->map_f_sha256_match.length;
        }
        total_metric_num++;
    }

    // match file sha512
    if (rec->field_usage_map & FIELD_REC_USE_F_SHA512) {
        lookup_file_sha512_part_map_via_dh(dh, rec->f_sha512, &match_map->map_buf1, &match_map->map_buf2);

        part_f_sha512_trans_map_to_entry_map(&dh->l2_sha512f_to_fd_arr, &match_map->map_buf2, &match_map->map_f_sha512_match, rec->f_sha512);

        if (match_map->map_f_sha512_match.length > max_entry_map_length) {
            max_entry_map_length = match_map->map_f_sha512_match.length;
        }
        total_metric_num++;
    }

    for (i = 0; i < rec->sect_field_in_use_num; i++) {
        temp_sect = rec->sect[i];

        // match section sha1
        if (rec->field_usage_map & FIELD_REC_USE_S_SHA1) {
            lookup_sect_sha1_part_map_via_dh(dh, temp_sect->s_sha1, &match_map->map_buf1, &match_map->map_buf2);

            part_s_sha1_trans_map_to_entry_map(&dh->l2_sha1s_to_s_arr, &match_map->map_buf2, &match_map->map_s_sha1_match, rec->f_sha1);

            if (match_map->map_s_sha1_match.length > max_entry_map_length) {
                max_entry_map_length = match_map->map_s_sha1_match.length;
            }
            total_metric_num++;
        }

        // match section sha256
        if (rec->field_usage_map & FIELD_REC_USE_S_SHA256) {
            lookup_sect_sha256_part_map_via_dh(dh, temp_sect->s_sha256, &match_map->map_buf1, &match_map->map_buf2);

            part_s_sha256_trans_map_to_entry_map(&dh->l2_sha256s_to_s_arr, &match_map->map_buf2, &match_map->map_s_sha256_match, rec->f_sha256);

            if (match_map->map_s_sha256_match.length > max_entry_map_length) {
                max_entry_map_length = match_map->map_s_sha256_match.length;
            }
            total_metric_num++;
        }

        // match section sha512
        if (rec->field_usage_map & FIELD_REC_USE_S_SHA512) {
            lookup_sect_sha512_part_map_via_dh(dh, temp_sect->s_sha512, &match_map->map_buf1, &match_map->map_buf2);

            part_s_sha512_trans_map_to_entry_map(&dh->l2_sha512s_to_s_arr, &match_map->map_buf2, &match_map->map_s_sha512_match, rec->f_sha512);

            if (match_map->map_s_sha512_match.length > max_entry_map_length) {
                max_entry_map_length = match_map->map_s_sha512_match.length;
            }
            total_metric_num++;
        }
    }

    // calculate minimum number of metrics matched required from score
    min_match_num = (score * total_metric_num  + (100 / 2)) / 100;  // rounding to nearest integer

    // set all to 0
    bitmap_zero(&match_map->map_result);

    // filter out entry with high enough matching metrics
    for (bit_indx = 0; bit_indx < max_entry_map_length; bit_indx++) {
        match_count = 0;

        // check name bitmap
        if (rec->field_usage_map & FIELD_REC_USE_E_NAME) {
            if (bit_indx <= match_map->map_name_match.length - 1) {
                bitmap_read(&match_map->map_name_match, bit_indx, &temp_result);
                if (temp_result) {
                    match_count++;
                }
            }
        }

        // check file size bitmap
        if (rec->field_usage_map & FIELD_REC_USE_F_SIZE) {
            if (bit_indx <= match_map->map_f_size_match.length - 1) {
                bitmap_read(&match_map->map_f_size_match, bit_indx, &temp_result);
                if (temp_result) {
                    match_count++;
                }
            }
        }

        // check file sha1 bitmap
        if (rec->field_usage_map & FIELD_REC_USE_F_SHA1) {
            if (bit_indx <= match_map->map_f_sha1_match.length - 1) {
                bitmap_read(&match_map->map_f_sha1_match, bit_indx, &temp_result);
                if (temp_result) {
                    match_count++;
                }
            }
        }

        // check file sha256 bitmap
        if (rec->field_usage_map & FIELD_REC_USE_F_SHA256) {
            if (bit_indx <= match_map->map_f_sha256_match.length - 1) {
                bitmap_read(&match_map->map_f_sha256_match, bit_indx, &temp_result);
                if (temp_result) {
                    match_count++;
                }
            }
        }

        // check file sha512 bitmap
        if (rec->field_usage_map & FIELD_REC_USE_F_SHA512) {
            if (bit_indx <= match_map->map_f_sha512_match.length - 1) {
                bitmap_read(&match_map->map_f_sha512_match, bit_indx, &temp_result);
                if (temp_result) {
                    match_count++;
                }
            }
        }

        // check section sha1 bitmap
        if (rec->field_usage_map & FIELD_REC_USE_S_SHA1) {
            if (bit_indx <= match_map->map_s_sha1_match.length - 1) {
                bitmap_read(&match_map->map_s_sha1_match, bit_indx, &temp_result);
                if (temp_result) {
                    match_count++;
                }
            }
        }

        // check section sha256 bitmap
        if (rec->field_usage_map & FIELD_REC_USE_S_SHA256) {
            if (bit_indx <= match_map->map_s_sha256_match.length - 1) {
                bitmap_read(&match_map->map_s_sha256_match, bit_indx, &temp_result);
                if (temp_result) {
                    match_count++;
                }
            }
        }

        // check section sha512 bitmap
        if (rec->field_usage_map & FIELD_REC_USE_S_SHA512) {
            if (bit_indx <= match_map->map_s_sha512_match.length - 1) {
                bitmap_read(&match_map->map_s_sha512_match, bit_indx, &temp_result);
                if (temp_result) {
                    match_count++;
                }
            }
        }

        if (match_count >= min_match_num) {
            // mark in result bitmap
            bitmap_write(&match_map->map_result, bit_indx, 1);
        }
    }

    return 0;
}

/* Children lookup */
int find_children_via_file_name(database_handle* dh, simple_bitmap* map_buf, simple_bitmap* map_buf2, char* file_name, linked_entry* parent, linked_entry** result_buf, bit_index buf_size, bit_index* num_used) {
    bit_index temp_index_result;
    bit_index temp_index_skip_to;
    bit_index temp_index_result2;
    bit_index temp_index_skip_to2;
    bit_index i, j, ret;

    bit_index temp_num_used = 0;

    layer1_fn_to_e_arr* temp_l1_arr;

    linked_entry* temp_entry;
    linked_entry* temp_entry_find;

    t_fn_to_e* fn_to_e;

    if (buf_size == 0) {
        return WRONG_ARGS;
    }

    if (!num_used) {
        return WRONG_ARGS;
    }

    *num_used = temp_num_used;

    if (parent->child_num == 0) {   // nothing to go through
        return 0;
    }

    // try to use hash table first
    lookup_file_name_via_dh(dh, file_name, &temp_entry_find);
    if (temp_entry_find) {  // found exact match
        // pick option with lower number to search linearly
        if (temp_entry_find->fn_to_e->number < parent->child_num) {
            // do a sequential search on chain of entries with same name
            temp_entry = temp_entry_find->fn_to_e->head_tar;
            while (temp_entry) {
                if (temp_entry->parent == parent) {
                    result_buf[temp_num_used] = temp_entry;
                    temp_num_used++;

                    if (temp_num_used == buf_size) {
                        *num_used = temp_num_used;
                        return BUFFER_FULL;
                    }
                }
                temp_entry = temp_entry->next_same_fn;
            }
        }
        else {
            // do a sequential search on children
            for (i = 0; i < parent->child_num; i++) {
                temp_entry = parent->child[i];
                if (strcmp(temp_entry->file_name, file_name) == 0) {
                    result_buf[temp_num_used] = temp_entry;
                    temp_num_used++;

                    if (temp_num_used == buf_size) {
                        *num_used = temp_num_used;
                        return BUFFER_FULL;
                    }
                }
            }
        }
    }
    else {  // no exact match
        ret = lookup_file_name_part_map_via_dh(dh, file_name, map_buf, map_buf2);
        if (ret) {
            return ret;
        }

        /* choose option with lower cost
         * 
         * Note :
         *      Below assumes the cost of traversing the entire linked list is negligible
         *      which is obviously not true for when the list is very long
         *
         *      But the cost of finding out the exact cost of bitmap based searching
         *      is very expensive itself, so the following ignores the cost of
         *      linked list traversal
         */
        // go with bitmap based searching
        if (map_buf2->number_of_ones * L1_FN_TO_E_ARR_SIZE <= parent->child_num) {
            for (i = 0, temp_index_skip_to = 0; i < map_buf2->number_of_ones; i++) {
                // get index of L1 array that contains matching entries
                bitmap_first_one_bit_index(map_buf2, &temp_index_result, temp_index_skip_to);
                temp_index_skip_to = temp_index_result + 1;

                // get the L1 array
                get_l1_fn_to_e_from_layer2_arr(&dh->l2_fn_to_e_arr, &temp_l1_arr, temp_index_result);

                // go through the L1 array to find entries with exact matching string
                for (j = 0, temp_index_skip_to2 = 0; j < temp_l1_arr->usage_map.number_of_ones; j++) {
                    // get index of entry to get
                    bitmap_first_one_bit_index(&temp_l1_arr->usage_map, &temp_index_result2, temp_index_skip_to2);
                    temp_index_skip_to2 = temp_index_result2 + 1;

                    fn_to_e = temp_l1_arr->arr + temp_index_result2;
                    if (strstr(fn_to_e->str, file_name)) {
                        temp_entry = fn_to_e->head_tar;
                        while (temp_entry) {
                            if (temp_entry->parent == parent) {
                                result_buf[temp_num_used] = temp_entry;
                                temp_num_used++;

                                if (temp_num_used == buf_size) { // buf_size guaranteed to be larger than zero at this point, due to input check
                                    *num_used = temp_num_used;
                                    return BUFFER_FULL;
                                }
                            }

                            temp_entry = temp_entry->next_same_fn;
                        }
                    }
                }
            }
        }
        // go with linear searching within parent
        else {
            for (i = 0; i < parent->child_num; i++) {
                temp_entry = parent->child[i];
                if (strstr(temp_entry->file_name, file_name)) {
                    result_buf[temp_num_used] = temp_entry;
                    temp_num_used++;

                    if (temp_num_used == buf_size) {
                        *num_used = temp_num_used;
                        return BUFFER_FULL;
                    }
                }
            }
        }

    }

    *num_used = temp_num_used;

    return 0;
}

int find_children_via_entry_id_part(database_handle* dh, simple_bitmap* map_buf, simple_bitmap* map_buf2, char* entry_id, linked_entry* parent, linked_entry** result_buf, bit_index buf_size, bit_index* num_used) {
    bit_index temp_map_length;
    map_block* temp_map_raw;
    bit_index temp_index_result;
    bit_index temp_index_skip_to;
    bit_index temp_index_result2;
    bit_index temp_index_skip_to2;
    bit_index i, j, ret;

    bit_index temp_num_used = 0;

    layer1_eid_to_e_arr* temp_l1_arr;

    linked_entry* temp_entry;
    linked_entry* temp_entry_find;

    t_eid_to_e* eid_to_e;

    if (buf_size == 0) {
        return WRONG_ARGS;
    }

    if (!num_used) {
        return WRONG_ARGS;
    }

    *num_used = temp_num_used;

    if (parent->child_num == 0) {   // nothing to go through
        return 0;
    }

    // try to use hash table first
    lookup_entry_id_via_dh(dh, entry_id, &temp_entry_find);
    if (temp_entry_find) {  // found exact match
        if (temp_entry_find->parent == parent) {
            result_buf[temp_num_used] = temp_entry_find;
            temp_num_used++;

            if (temp_num_used == buf_size) { // buf_size guaranteed to be larger than zero at this point, due to input check
                *num_used = temp_num_used;
                return BUFFER_FULL;
            }
        }
        else {
            ;;  // do nothing
        }
    }
    else {  // no exact match
        // grow map_buf if necessary
        temp_map_length = dh->l2_eid_to_e_arr.l1_arr_map.length;
        if (temp_map_length > map_buf->length) {
            if (        get_bitmap_map_block_number(temp_map_length)
                    !=  get_bitmap_map_block_number(map_buf->length)
               )
            {
                temp_map_raw =
                    realloc(
                            map_buf->base,
                            get_bitmap_map_block_number(temp_map_length)
                            * sizeof(map_block)
                           );
                if (!temp_map_raw) {
                    return MALLOC_FAIL;
                }
            }
            bitmap_grow(map_buf, temp_map_raw, 0, temp_map_length, 0);
        }

        // grow map_buf2 if necessary
        if (temp_map_length > map_buf2->length) {
            if (        get_bitmap_map_block_number(temp_map_length)
                    !=  get_bitmap_map_block_number(map_buf2->length)
               )
            {
                temp_map_raw =
                    realloc(
                            map_buf2->base,
                            get_bitmap_map_block_number(temp_map_length)
                            * sizeof(map_block)
                           );
                if (!temp_map_raw) {
                    return MALLOC_FAIL;
                }
            }
            bitmap_grow(map_buf2, temp_map_raw, 0, temp_map_length, 0);
        }

        ret = lookup_entry_id_part_map_via_dh(dh, entry_id, map_buf, map_buf2);
        if (ret) {
            return ret;
        }

        /* choose option with lower cost */
        // go with bitmap based searching
        if (map_buf2->number_of_ones * L1_EID_TO_E_ARR_SIZE <= parent->child_num) {
            temp_index_skip_to = 0;
            for (i = 0, temp_index_skip_to = 0; i < map_buf2->number_of_ones; i++) {
                // get index of L1 array that contains matching entries
                bitmap_first_one_bit_index(map_buf2, &temp_index_result, temp_index_skip_to);
                temp_index_skip_to = temp_index_result + 1;

                // get the L1 array
                get_l1_eid_to_e_from_layer2_arr(&dh->l2_eid_to_e_arr, &temp_l1_arr, temp_index_result);

                // go through the L1 array to find entries with exact matching string
                for (j = 0, temp_index_skip_to2 = 0; j < temp_l1_arr->usage_map.number_of_ones; j++) {
                    // get index of entry to get
                    bitmap_first_one_bit_index(&temp_l1_arr->usage_map, &temp_index_result2, temp_index_skip_to2);
                    temp_index_skip_to2 = temp_index_result2 + 1;

                    eid_to_e = temp_l1_arr->arr + temp_index_result2;
                    if (strstr(eid_to_e->str, entry_id)) {
                        temp_entry = eid_to_e->tar;
                        if (temp_entry->parent == parent) {
                            result_buf[temp_num_used] = temp_entry;
                            temp_num_used++;

                            if (temp_num_used == buf_size) { // buf_size guaranteed to be larger than zero at this point, due to input check
                                *num_used = temp_num_used;
                                return BUFFER_FULL;
                            }
                        }
                    }
                }
            }
        }
        // go with linear searching within parent
        else {
            for (i = 0; i < parent->child_num; i++) {
                temp_entry = parent->child[i];
                if (strstr(temp_entry->entry_id_str, entry_id)) {
                    result_buf[temp_num_used] = temp_entry;
                    temp_num_used++;

                    if (temp_num_used == buf_size) {
                        *num_used = temp_num_used;
                        return BUFFER_FULL;
                    }
                }
            }
        }

    }

    *num_used = temp_num_used;

    return 0;
}

int gen_entry_id(unsigned char* entry_id, char* entry_id_str) {
    unsigned char eid[EID_LEN];
    char eid_str[EID_STR_MAX+1];

    int i;

    if (!entry_id && !entry_id_str) {
        return WRONG_ARGS;
    }

    for (i = 0; i < EID_LEN; i++) {
        // below obviously makes it not uniformly random
        eid[i] = rand() % 256;
    }

    // convert to hex string
    bytes_to_hex_str(eid_str, eid, EID_LEN);

    if (entry_id) {
        for (i = 0; i < EID_LEN; i++) {
            entry_id[i] = eid[i];
        }
    }

    if (entry_id_str) {
        strcpy(entry_id_str, eid_str);
    }

    return 0;
}

int set_new_entry_id(database_handle* dh, linked_entry* entry) {
    int count = 0;

    int i;

    linked_entry* temp_entry_find;

    unsigned char eid[EID_LEN];
    char eid_str[EID_STR_MAX+1];

    do {
        gen_entry_id(eid, eid_str);

        // check for collision
        lookup_entry_id_via_dh(dh, eid_str, &temp_entry_find);

        count++;
    } while (temp_entry_find && count < ID_GEN_MAX_TRIES);

    if (count == ID_GEN_MAX_TRIES) {
        return GEN_ID_FAIL;
    }

    for (i = 0; i < EID_LEN; i++) {
        entry->entry_id[i] = eid[i];
    }
    strcpy(entry->entry_id_str, eid_str);

    return 0;
}

int set_entry_general_info(database_handle* dh, linked_entry* entry) {
    int i;

    time_t raw_time;
    struct tm* time_info;

    // set has parent flag
    if (entry->parent == &dh->tree) {
        entry->has_parent = 0;
    }
    else {
        entry->has_parent = 1;
    }

    // set branch id
    if (entry->has_parent) {
        // copy branch id from parent
        for (i = 0; i < EID_LEN; i++) {
            entry->branch_id[i] = entry->parent->branch_id[i];
        }
        strcpy(entry->branch_id_str, entry->parent->branch_id_str);
    }
    else {
        // fill in branch id using entry id
        for (i = 0; i < EID_LEN; i++) {
            entry->branch_id[i] = entry->entry_id[i];
        }
        strcpy(entry->branch_id_str, entry->entry_id_str);
    }

    // set depth
    entry->depth = entry->parent->depth + 1;

    // get current time
    time(&raw_time);
    time_info = gmtime(&raw_time);

    // set time of addition
    memcpy(
            &entry->tod_utc,
            time_info,

            sizeof(struct tm)
          );

    link_entry_to_date_time_tree(dh, entry, DATE_TOD);

    // set time of modification
    memcpy(
            &entry->tom_utc,
            time_info,

            sizeof(struct tm)
          );

    link_entry_to_date_time_tree(dh, entry, DATE_TOM);

    return 0;
}

/*
int link_entry (database_handle* dh, linked_entry* new_parent, linked_entry* child) {
    linked_entry** temp_entry_p;
    linked_entry* old_parent;
    int i, j;

    if (new_parent == child->parent) {  // no need for linking
        return 0;
    }

    if (!new_parent) {
        new_parent = &dh->tree;
    }

    old_parent = child->parent;

    if (child->parent) {
      // shrink old parent's children array
        for (i = 0; i < old_parent->child_num; i++) {
            if (old_parent->child[i] == child) {
                break;
            }
        }
        if (i == old_parent->child_num) {   // no match found
            return LOGIC_ERROR;
        }

        // shift the array
        for (j = i; j < old_parent->child_num; j++) {
            old_parent->child[j] = old_parent->child[j+1];
        }

        // nullify the extra spot
        old_parent->child[old_parent->child_num] = 0;

        // realloc for parent's children array
        temp_entry_p = realloc(old_parent->child, sizeof(linked_entry*) * (old_parent->child_num - 1));
        if (!temp_entry_p) {
            printf("link_entry : shrink failed\n");
            return MALLOC_FAIL;
        }
        old_parent->child = temp_entry_p;

        old_parent->child_num--;
    }

  // grow new parent's children array
    temp_entry_p = realloc(new_parent->child, sizeof(linked_entry*) * (new_parent->child_num + 1));
    if (!temp_entry_p) {
        printf("link_entry : grow failed\n");
        return MALLOC_FAIL;
    }
    new_parent->child = temp_entry_p;

    new_parent->child[new_parent->child_num] = child;

    new_parent->child_num++;

  // check if child still has a proper parent
    if (new_parent == &dh->tree) {
        child->has_parent = 0;
    }

    return 0;
}
*/

int link_branch (database_handle* dh, linked_entry* entry, linked_entry* tail, linked_entry** ret_tail) {
    ffp_eid_int i;

    linked_entry* temp;
    linked_entry* temp_backup;

    if (!tail) {
        link_entry_clear(entry);
    }
    else {
        link_entry_after(tail, entry);
    }

    temp = entry;
    temp_backup = temp;
    while (temp->child_num == 1) {
        link_entry_after(temp, temp->child[0]);

        temp_backup = temp;
        temp = temp->child[0];
    }

    tail = temp;

    if (temp->child_num > 1) {
        for (i = 0; i < temp->child_num; i++) {
            link_branch(dh, temp->child[i], tail, &tail);
        }
    }

    if (ret_tail) {
        *ret_tail = tail;
    }

    return 0;
}

int link_branch_ends (database_handle* dh, linked_entry* entry, linked_entry* tail, linked_entry** ret_tail) {
    ffp_eid_int i;

    linked_entry* temp;

    if (!tail) {
        link_entry_clear(entry);
    }
    else {
        if (entry->child_num == 0) {
            link_entry_after(tail, entry);
        }
    }

    temp = entry;
    while (temp->child_num == 1) {
        temp = temp->child[0];
    }

    if (temp->child_num == 0) {
        tail = temp;
    }
    else {
        tail = entry;
    }

    if (temp->child_num > 1) {
        for (i = 0; i < temp->child_num; i++) {
            link_branch_ends(dh, temp->child[i], tail, &tail);
        }
    }

    if (ret_tail) {
        *ret_tail = tail;
    }

    return 0;
}

int link_entry_to_entry_id_structures(database_handle* dh, linked_entry* target) {
    bit_index temp_index;
    t_eid_to_e* temp_eid_to_e;
    int verify_error_code;

    // add entry to eid_to_entry hashtable
    add_eid_to_e_to_layer2_arr(&dh->l2_eid_to_e_arr, &temp_eid_to_e, &temp_index);
    init_eid_to_e(temp_eid_to_e);
    strcpy(temp_eid_to_e->str, target->entry_id_str);
    temp_eid_to_e->tar = target;
    verify_eid_to_e(temp_eid_to_e, &verify_error_code, 0);
    add_eid_to_e_to_htab(&dh->eid_to_e, temp_eid_to_e);
    // add entry to existence matrix
    add_eid_to_eid_exist_mat(&dh->eid_mat, temp_eid_to_e->str, temp_index);
    // link translation structure to entry
    target->eid_to_e = temp_eid_to_e;

    return 0;
}

int unlink_entry_from_entry_id_structures(database_handle* dh, linked_entry* target) {
    t_eid_to_e* temp_eid_to_e;

    if (        !dh
            ||  !target
       )
    {
        return WRONG_ARGS;
    }

    if (!target->eid_to_e) {    // not previously linked
        return 0;
    }

    temp_eid_to_e = target->eid_to_e;

    // delete from hashtable
    del_eid_to_e_from_htab(&dh->eid_to_e, temp_eid_to_e);
    // delete from layer 2 array
    del_eid_to_e_from_layer2_arr(&dh->l2_eid_to_e_arr, temp_eid_to_e->obj_arr_index);

    // nullify link
    target->eid_to_e = NULL;

    return 0;
}

// link entry to file name related structures
int link_entry_to_file_name_structures(database_handle* dh, linked_entry* target) {
    linked_entry* temp_entry_find;
    bit_index temp_index;
    t_fn_to_e* temp_fn_to_e;
    int verify_error_code;

    lookup_file_name_via_dh(dh, target->file_name, &temp_entry_find);
    if (!temp_entry_find) {   // no entry with same file name found
        // add file name to layer 2 array
        add_fn_to_e_to_layer2_arr(&dh->l2_fn_to_e_arr, &temp_fn_to_e, &temp_index);
        init_fn_to_e(temp_fn_to_e);
        strcpy(temp_fn_to_e->str, target->file_name);
        temp_fn_to_e->head_tar = target;
        temp_fn_to_e->tail_tar = target;
        temp_fn_to_e->number = 1;
        verify_fn_to_e(temp_fn_to_e, &verify_error_code, GO_THROUGH_CHAIN);
        add_fn_to_e_to_htab(&dh->fn_to_e, temp_fn_to_e);
        // add file name to existence matrix
        add_fn_to_fn_exist_mat(&dh->fn_mat, temp_fn_to_e->str, temp_index);
        // link translation structure to entry
        target->fn_to_e = temp_fn_to_e;
    }
    else {      // found entry with same file name, add to chain
        add_e_to_fn_to_e_chain(temp_entry_find, target);
    }

    return 0;
}

int link_entry_to_tag_str_structures(database_handle* dh, linked_entry* target) {
    linked_entry* temp_entry_find;
    bit_index temp_index;
    t_tag_to_e* temp_tag_to_e;
    int verify_error_code;

    lookup_tag_str_via_dh(dh, target->tag_str, &temp_entry_find);
    if (!temp_entry_find) {     // no entry with same tag string found
        // add tag string to layer 2 array
        add_tag_to_e_to_layer2_arr(&dh->l2_tag_to_e_arr, &temp_tag_to_e, &temp_index);
        init_tag_to_e(temp_tag_to_e);
        strcpy(temp_tag_to_e->str, target->tag_str);
        temp_tag_to_e->head_tar = target;
        temp_tag_to_e->tail_tar = target;
        temp_tag_to_e->number = 1;
        verify_tag_to_e(temp_tag_to_e, &verify_error_code, GO_THROUGH_CHAIN);
        add_tag_to_e_to_htab(&dh->tag_to_e, temp_tag_to_e);
        // add tag string to existence matrix
        add_tag_to_tag_exist_mat(&dh->tag_mat, temp_tag_to_e->str, temp_index);
        // link translation structure to entry
        target->tag_to_e = temp_tag_to_e;
    }
    else {      // found entry with same tag string, add to tail
        add_e_to_tag_to_e_chain(temp_entry_find, target);
    }

    return 0;
}

int link_entry_to_date_time_tree(database_handle* dh, linked_entry* target, unsigned char mode) {
    dtt_year* temp_year;
    dtt_year* temp_year_backup;
    dtt_month* temp_month;
    dtt_day* temp_day;
    dtt_hour* temp_hour;

    dtt_year* dtt;
    struct tm* tar_time;

    time_t raw_time;
    struct tm* time_info;

    linked_entry* temp_entry_find;
    linked_entry* temp_entry_find_backup;

    int i;

    if (        mode != DATE_TOD
            &&  mode != DATE_TOM
            &&  mode != DATE_TUSR
       )
    {
        return WRONG_ARGS;
    }

    switch (mode) {
        case DATE_TOD :
            tar_time = &target->tod_utc;
            break;
        case DATE_TOM :
            tar_time = &target->tom_utc;
            break;
        case DATE_TUSR :
            tar_time = &target->tusr_utc;
            break;
        default :
            return LOGIC_ERROR;
    }

    if ((raw_time = timegm(tar_time)) == -1) {
        return INVALID_DATE;
    }
    time_info = gmtime(&raw_time);
    memcpy(
            tar_time,
            time_info,

            sizeof(struct tm)
          );

    // deal with year
    if (        mode == DATE_TOD    &&  !dh->tod_dt_tree    ) {
        add_dtt_year_to_layer2_arr(&dh->l2_dtt_year_arr, &dtt, NULL);
        dh->tod_dt_tree = dtt;
        dtt->year = tar_time->tm_year + 1900;
        temp_year = dtt;
    }
    else if (   mode == DATE_TOM    &&  !dh->tom_dt_tree   ) {
        add_dtt_year_to_layer2_arr(&dh->l2_dtt_year_arr, &dtt, NULL);
        dh->tom_dt_tree = dtt;
        dtt->year = tar_time->tm_year + 1900;
        temp_year = dtt;
    }
    else if (   mode == DATE_TUSR   &&  !dh->tusr_dt_tree   ) {
        add_dtt_year_to_layer2_arr(&dh->l2_dtt_year_arr, &dtt, NULL);
        dh->tusr_dt_tree = dtt;
        dtt->year = tar_time->tm_year + 1900;
        temp_year = dtt;
    }
    else {
        switch (mode) {
            case DATE_TOD :
                dtt = dh->tod_dt_tree;
                break;
            case DATE_TOM :
                dtt = dh->tom_dt_tree;
                break;
            case DATE_TUSR :
                dtt = dh->tusr_dt_tree;
                break;
            default :
                return LOGIC_ERROR;
        }

        temp_year = dtt;
        while (temp_year) {
            if (temp_year->year == tar_time->tm_year + 1900) {
                break;
            }

            temp_year_backup = temp_year;
            temp_year = temp_year->next;
        }
        if (!temp_year) {
            temp_year = temp_year_backup;
        }
        if (temp_year->year != tar_time->tm_year + 1900) {
            add_dtt_year_to_layer2_arr(&dh->l2_dtt_year_arr, &temp_year->next, NULL);
            temp_year = temp_year->next;

            temp_year->year = tar_time->tm_year + 1900;
            temp_year->next = NULL;
            for (i = 0; i < 12; i++) {
                temp_year->month[i].in_use = 0;
            }
        }
    }

    // deal with month
    temp_month = temp_year->month + tar_time->tm_mon;
    temp_month->in_use = 1;

    // deal with day
    temp_day = temp_month->day + tar_time->tm_mday;
    temp_day->in_use = 1;

    // deal with hour
    temp_hour = temp_day->hour + tar_time->tm_hour;
    if (!temp_hour->in_use) {
        temp_hour->head = target;

        switch (mode) {
            case DATE_TOD :
                target->tod_prev_sort_min = NULL;
                target->tod_next_sort_min = NULL;
                break;
            case DATE_TOM :
                target->tom_prev_sort_min = NULL;
                target->tom_next_sort_min = NULL;
                break;
            case DATE_TUSR :
                target->tusr_prev_sort_min = NULL;
                target->tusr_next_sort_min = NULL;
                break;
            default :
                return LOGIC_ERROR;
        }

        temp_hour->in_use = 1;
    }
    else {
        temp_entry_find = temp_hour->head;
        temp_entry_find_backup = temp_entry_find;

        switch (mode) {
            case DATE_TOD :
                while (temp_entry_find) {
                    if (temp_entry_find->tod_utc.tm_min > target->tod_utc.tm_min) {
                        temp_entry_find_backup->tod_next_sort_min = target;
                        target->tod_prev_sort_min = temp_entry_find_backup;
                        target->tod_next_sort_min = temp_entry_find;
                        temp_entry_find->tod_prev_sort_min = target;
                        break;
                    }
                    temp_entry_find_backup = temp_entry_find;
                    temp_entry_find = temp_entry_find->tod_next_sort_min;
                }
                if (!temp_entry_find) {
                    temp_entry_find_backup->tod_next_sort_min = target;
                    target->tod_prev_sort_min = temp_entry_find_backup;
                    target->tod_next_sort_min = NULL;
                }
                break;
            case DATE_TOM :
                while (temp_entry_find) {
                    if (temp_entry_find->tom_utc.tm_min > target->tom_utc.tm_min) {
                        temp_entry_find_backup->tom_next_sort_min = target;
                        target->tom_prev_sort_min = temp_entry_find_backup;
                        target->tom_next_sort_min = temp_entry_find;
                        temp_entry_find->tom_prev_sort_min = target;
                        break;
                    }
                    temp_entry_find_backup = temp_entry_find;
                    temp_entry_find = temp_entry_find->tom_next_sort_min;
                }
                if (!temp_entry_find) {
                    temp_entry_find_backup->tom_next_sort_min = target;
                    target->tom_prev_sort_min = temp_entry_find_backup;
                    target->tom_next_sort_min = NULL;
                }
                break;
            case DATE_TUSR :
                while (temp_entry_find) {
                    if (temp_entry_find->tusr_utc.tm_min > target->tusr_utc.tm_min) {
                        temp_entry_find_backup->tusr_next_sort_min = target;
                        target->tusr_prev_sort_min = temp_entry_find_backup;
                        target->tusr_next_sort_min = temp_entry_find;
                        temp_entry_find->tusr_prev_sort_min = target;
                        break;
                    }
                    temp_entry_find_backup = temp_entry_find;
                    temp_entry_find = temp_entry_find->tusr_next_sort_min;
                }
                if (!temp_entry_find) {
                    temp_entry_find_backup->tusr_next_sort_min = target;
                    target->tusr_prev_sort_min = temp_entry_find_backup;
                    target->tusr_next_sort_min = NULL;
                }
                break;
            default :
                return LOGIC_ERROR;
        }
    }

    switch (mode) {
        case DATE_TOD :
            target->tod_hour = temp_hour;
            target->tod_utc_used = 1;
            break;
        case DATE_TOM :
            target->tom_hour = temp_hour;
            target->tom_utc_used = 1;
            break;
        case DATE_TUSR :
            target->tusr_hour = temp_hour;
            target->tusr_utc_used = 1;
            break;
        default :
            return LOGIC_ERROR;
    }

    return 0;
}

int unlink_entry_from_date_time_tree(database_handle* dh, linked_entry* target, unsigned char mode) {
    dtt_year* temp_year;
    dtt_year* temp_year_backup;
    dtt_month* temp_month;
    dtt_day* temp_day;
    dtt_hour* temp_hour;

    dtt_year* dtt;
    struct tm* tar_time;

    linked_entry* temp_entry_find;
    linked_entry* temp_entry_find_backup;

    int i;

    if (        mode != DATE_TOD
            &&  mode != DATE_TUSR
       )
    {
        return WRONG_ARGS;
    }

    if (mode == DATE_TOD) {
        if (!target->tod_hour) {        // not previously linked
            return 0;
        }

        dtt = dh->tod_dt_tree;
        tar_time = &target->tod_utc;
    }
    else if (mode == DATE_TUSR) {
        if (!target->tusr_hour) {       // not previously linked
            return 0;
        }

        dtt = dh->tusr_dt_tree;
        tar_time = &target->tusr_utc;
    }

    // deal with year
    temp_year = dtt;
    while (temp_year) {
        if (temp_year->year == tar_time->tm_year + 1900) {
            break;
        }

        temp_year_backup = temp_year;
        temp_year = temp_year->next;
    }
    if (!temp_year) {
        return LOGIC_ERROR;
    }

    // deal with month
    temp_month = temp_year->month + tar_time->tm_mon;

    // deal with day
    temp_day = temp_month->day + tar_time->tm_mday;

    // deal with hour
    temp_hour = temp_day->hour + tar_time->tm_hour;

    if (!temp_hour->in_use) {
        return LOGIC_ERROR;
    }
    temp_entry_find = temp_hour->head;
    temp_entry_find_backup = temp_entry_find;
    if (mode == DATE_TOD) {
        if (        target->tod_prev_sort_min == NULL
                &&  target->tod_next_sort_min == NULL
           )
        {
            temp_hour->head = NULL;
            temp_hour->in_use = 0;
        }
        else if (target->tod_prev_sort_min == NULL) {
            temp_hour->head = target->tod_next_sort_min;
            target->tod_next_sort_min->tod_prev_sort_min = NULL;

            target->tod_next_sort_min = NULL;
        }
        else if (target->tod_next_sort_min == NULL) {
            target->tod_prev_sort_min->tod_next_sort_min = NULL;

            target->tod_prev_sort_min = NULL;
        }
    }
    else if (mode == DATE_TUSR) {
        if (        target->tusr_prev_sort_min == NULL
                &&  target->tusr_next_sort_min == NULL
           )
        {
            temp_hour->head = NULL;
            temp_hour->in_use = 0;
        }
        else if (target->tusr_prev_sort_min == NULL) {
            temp_hour->head = target->tusr_next_sort_min;
            target->tusr_next_sort_min->tusr_prev_sort_min = NULL;

            target->tusr_next_sort_min = NULL;
        }
        else if (target->tusr_next_sort_min == NULL) {
            target->tusr_prev_sort_min->tusr_next_sort_min = NULL;

            target->tusr_prev_sort_min = NULL;
        }
    }

    if (!temp_hour->in_use) {
        for (i = 0; i < 24; i++) {
            if (temp_day->hour[i].in_use) {
                break;
            }
        }
        if (i == 24) {
            temp_day->in_use = 0;
        }
    }

    if (!temp_day->in_use) {
        for (i = 0; i < 32; i++) {
            if (temp_month->day[i].in_use) {
                break;
            }
        }
        if (i == 32) {
            temp_month->in_use = 0;
        }
    }

    if (!temp_month->in_use) {
        for (i = 0; i < 12; i++) {
            if (temp_year->month[i].in_use) {
                break;
            }
        }
        if (i == 12) {
            if (temp_year == dh->tod_dt_tree) {
                dh->tod_dt_tree = temp_year->next;
            }
            else if (temp_year == dh->tusr_dt_tree) {
                dh->tusr_dt_tree = temp_year->next;
            }
            else {
                temp_year_backup->next = temp_year->next;
            }

            del_dtt_year_from_layer2_arr(&dh->l2_dtt_year_arr, temp_year->obj_arr_index);
        }
    }

    return 0;
}

int link_file_data_to_file_size_structures(database_handle* dh, file_data* target) {
    file_data* temp_file_data_find;
    bit_index temp_index;
    t_f_size_to_fd* temp_f_size_to_fd;
    int verify_error_code;
    char file_size_str_buf[FILE_SIZE_STR_MAX+1];

    // convert file size into string
    strfy_sprintf(file_size_str_buf, FILE_SIZE_STR_MAX, PRIu64, target->file_size);

    lookup_file_size_via_dh(dh, file_size_str_buf, &temp_file_data_find);
    if (!temp_file_data_find) {   // no file data with same file size found
        // add file size to layer 2 array
        add_f_size_to_fd_to_layer2_arr(&dh->l2_f_size_to_fd_arr, &temp_f_size_to_fd, &temp_index);
        init_f_size_to_fd(temp_f_size_to_fd);
        strcpy(temp_f_size_to_fd->str, file_size_str_buf);
        temp_f_size_to_fd->head_tar = target;
        temp_f_size_to_fd->tail_tar = target;
        temp_f_size_to_fd->number = 1;
        verify_f_size_to_fd(temp_f_size_to_fd, &verify_error_code, GO_THROUGH_CHAIN);
        add_f_size_to_fd_to_htab(&dh->f_size_to_fd, temp_f_size_to_fd);
        // add file size to existence matrix
        add_f_size_to_f_size_exist_mat(&dh->f_size_mat, temp_f_size_to_fd->str, temp_index);
        // link translation structure to file data
        target->f_size_to_fd = temp_f_size_to_fd;
    }
    else {      // found file_data with same file size, add to tail
        add_fd_to_f_size_to_fd_chain(temp_file_data_find, target);
    }

    return 0;
}

int link_file_data_to_sha1_structures(database_handle* dh, file_data* target, checksum_result* target_checksum_result) {
    file_data* temp_file_data_find;
    bit_index temp_index;
    t_sha1f_to_fd* temp_sha1f_to_fd;
    int verify_error_code;

    lookup_file_sha1_via_dh(dh, target_checksum_result->checksum_str, &temp_file_data_find);
    if (!temp_file_data_find) {     // no file data with same checksum
        // add whole file checksum to layer 2 array
        add_sha1f_to_fd_to_layer2_arr(&dh->l2_sha1f_to_fd_arr, &temp_sha1f_to_fd, &temp_index);
        init_sha1f_to_fd(temp_sha1f_to_fd);
        strcpy(temp_sha1f_to_fd->str, target_checksum_result->checksum_str);
        temp_sha1f_to_fd->head_tar = target;
        temp_sha1f_to_fd->tail_tar = target;
        temp_sha1f_to_fd->number = 1;
        verify_sha1f_to_fd(temp_sha1f_to_fd, &verify_error_code, GO_THROUGH_CHAIN);
        add_sha1f_to_fd_to_htab(&dh->sha1f_to_fd, temp_sha1f_to_fd);
        // add whole file checksum to existence matrix
        add_sha1f_to_sha1f_exist_mat(&dh->sha1f_mat, temp_sha1f_to_fd->str, temp_index);
        // link translation structure to file data
        target->sha1f_to_fd = temp_sha1f_to_fd;
    }
    else {  // found file_data with same checksum, add to tail
        add_fd_to_sha1f_to_fd_chain(temp_file_data_find, target);
    }

    return 0;
}

int link_file_data_to_sha256_structures(database_handle* dh, file_data* target, checksum_result* target_checksum_result) {
    file_data* temp_file_data_find;
    bit_index temp_index;
    t_sha256f_to_fd* temp_sha256f_to_fd;
    int verify_error_code;

    lookup_file_sha256_via_dh(dh, target_checksum_result->checksum_str, &temp_file_data_find);
    if (!temp_file_data_find) {     // no file data with same checksum
        // add whole file checksum to layer 2 array
        add_sha256f_to_fd_to_layer2_arr(&dh->l2_sha256f_to_fd_arr, &temp_sha256f_to_fd, &temp_index);
        init_sha256f_to_fd(temp_sha256f_to_fd);
        strcpy(temp_sha256f_to_fd->str, target_checksum_result->checksum_str);
        temp_sha256f_to_fd->head_tar = target;
        temp_sha256f_to_fd->tail_tar = target;
        temp_sha256f_to_fd->number = 1;
        verify_sha256f_to_fd(temp_sha256f_to_fd, &verify_error_code, GO_THROUGH_CHAIN);
        add_sha256f_to_fd_to_htab(&dh->sha256f_to_fd, temp_sha256f_to_fd);
        // add whole file checksum to existence matrix
        add_sha256f_to_sha256f_exist_mat(&dh->sha256f_mat, temp_sha256f_to_fd->str, temp_index);
        // link translation structure to file data
        target->sha256f_to_fd = temp_sha256f_to_fd;
    }
    else {  // found file_data with same checksum, add to tail
        add_fd_to_sha256f_to_fd_chain(temp_file_data_find, target);
    }

    return 0;
}

int link_file_data_to_sha512_structures(database_handle* dh, file_data* target, checksum_result* target_checksum_result) {
    file_data* temp_file_data_find;
    bit_index temp_index;
    t_sha512f_to_fd* temp_sha512f_to_fd;
    int verify_error_code;

    lookup_file_sha512_via_dh(dh, target_checksum_result->checksum_str, &temp_file_data_find);
    if (!temp_file_data_find) {     // no file data with same checksum
        // add whole file checksum to layer 2 array
        add_sha512f_to_fd_to_layer2_arr(&dh->l2_sha512f_to_fd_arr, &temp_sha512f_to_fd, &temp_index);
        init_sha512f_to_fd(temp_sha512f_to_fd);
        strcpy(temp_sha512f_to_fd->str, target_checksum_result->checksum_str);
        temp_sha512f_to_fd->head_tar = target;
        temp_sha512f_to_fd->tail_tar = target;
        temp_sha512f_to_fd->number = 1;
        verify_sha512f_to_fd(temp_sha512f_to_fd, &verify_error_code, GO_THROUGH_CHAIN);
        add_sha512f_to_fd_to_htab(&dh->sha512f_to_fd, temp_sha512f_to_fd);
        // add whole file checksum to existence matrix
        add_sha512f_to_sha512f_exist_mat(&dh->sha512f_mat, temp_sha512f_to_fd->str, temp_index);
        // link translation structure to file data
        target->sha512f_to_fd = temp_sha512f_to_fd;
    }
    else {  // found file_data with same checksum, add to tail
        add_fd_to_sha512f_to_fd_chain(temp_file_data_find, target);
    }

    return 0;
}

int link_file_data_to_checksum_structures(database_handle* dh, file_data* target) {
    checksum_result* target_checksum_result;
    int i;

    for (i = 0; i < CHECKSUM_MAX_NUM; i++) {
        target_checksum_result = target->checksum + i;

        switch (target_checksum_result->type) {
            case CHECKSUM_UNUSED :
                // do nothing
                break;
            case CHECKSUM_SHA1_ID:
                link_file_data_to_sha1_structures(dh, target, target_checksum_result);
                break;
            case CHECKSUM_SHA256_ID :
                link_file_data_to_sha256_structures(dh, target, target_checksum_result);
                break;
            case CHECKSUM_SHA512_ID :
                link_file_data_to_sha512_structures(dh, target, target_checksum_result);
                break;
            default:
                return LOGIC_ERROR;
        }
    }

    return 0;
}

int link_sect_to_sha1_structures(database_handle* dh, section* target, checksum_result* target_checksum_result) {
    section* temp_section_find;
    bit_index temp_index;
    t_sha1s_to_s* temp_sha1s_to_s;
    int verify_error_code;

    lookup_sect_sha1_via_dh(dh, target_checksum_result->checksum_str, &temp_section_find);
    if (!temp_section_find) {     // no section with same checksum
        // add whole file checksum to layer 2 array
        add_sha1s_to_s_to_layer2_arr(&dh->l2_sha1s_to_s_arr, &temp_sha1s_to_s, &temp_index);
        init_sha1s_to_s(temp_sha1s_to_s);
        strcpy(temp_sha1s_to_s->str, target_checksum_result->checksum_str);
        temp_sha1s_to_s->head_tar = target;
        temp_sha1s_to_s->tail_tar = target;
        temp_sha1s_to_s->number = 1;
        verify_sha1s_to_s(temp_sha1s_to_s, &verify_error_code, GO_THROUGH_CHAIN);
        add_sha1s_to_s_to_htab(&dh->sha1s_to_s, temp_sha1s_to_s);
        // add whole file checksum to existence matrix
        add_sha1s_to_sha1s_exist_mat(&dh->sha1s_mat, temp_sha1s_to_s->str, temp_index);
        // link translation structure to section
        target->sha1s_to_s = temp_sha1s_to_s;
    }
    else {  // found file_data with same checksum, add to tail
        add_s_to_sha1s_to_s_chain(temp_section_find, target);
    }

    return 0;
}

int link_sect_to_sha256_structures(database_handle* dh, section* target, checksum_result* target_checksum_result) {
    section* temp_section_find;
    bit_index temp_index;
    t_sha256s_to_s* temp_sha256s_to_s;
    int verify_error_code;
 
    lookup_sect_sha256_via_dh(dh, target_checksum_result->checksum_str, &temp_section_find);
    if (!temp_section_find) {     // no section with same checksum
        // add whole file checksum to layer 2 array
        add_sha256s_to_s_to_layer2_arr(&dh->l2_sha256s_to_s_arr, &temp_sha256s_to_s, &temp_index);
        init_sha256s_to_s(temp_sha256s_to_s);
        strcpy(temp_sha256s_to_s->str, target_checksum_result->checksum_str);
        temp_sha256s_to_s->head_tar = target;
        temp_sha256s_to_s->tail_tar = target;
        temp_sha256s_to_s->number = 1;
        verify_sha256s_to_s(temp_sha256s_to_s, &verify_error_code, GO_THROUGH_CHAIN);
        add_sha256s_to_s_to_htab(&dh->sha256s_to_s, temp_sha256s_to_s);
        // add whole file checksum to existence matrix
        add_sha256s_to_sha256s_exist_mat(&dh->sha256s_mat, temp_sha256s_to_s->str, temp_index);
        // link translation structure to section
        target->sha256s_to_s = temp_sha256s_to_s;
    }
    else {  // found file_data with same checksum, add to tail
        add_s_to_sha256s_to_s_chain(temp_section_find, target);
    }

    return 0;
}

int link_sect_to_sha512_structures(database_handle* dh, section* target, checksum_result* target_checksum_result) {
    section* temp_section_find;
    bit_index temp_index;
    t_sha512s_to_s* temp_sha512s_to_s;
    int verify_error_code;
     
    lookup_sect_sha512_via_dh(dh, target_checksum_result->checksum_str, &temp_section_find);
    if (!temp_section_find) {     // no file data with same checksum
        // add whole file checksum to layer 2 array
        add_sha512s_to_s_to_layer2_arr(&dh->l2_sha512s_to_s_arr, &temp_sha512s_to_s, &temp_index);
        init_sha512s_to_s(temp_sha512s_to_s);
        strcpy(temp_sha512s_to_s->str, target_checksum_result->checksum_str);
        temp_sha512s_to_s->head_tar = target;
        temp_sha512s_to_s->tail_tar = target;
        temp_sha512s_to_s->number = 1;
        verify_sha512s_to_s(temp_sha512s_to_s, &verify_error_code, GO_THROUGH_CHAIN);
        add_sha512s_to_s_to_htab(&dh->sha512s_to_s, temp_sha512s_to_s);
        // add whole file checksum to existence matrix
        add_sha512f_to_sha512f_exist_mat(&dh->sha512f_mat, temp_sha512s_to_s->str, temp_index);
        // link translation structure to section
        target->sha512s_to_s = temp_sha512s_to_s;
    }
    else {  // found file_data with same checksum, add to tail
        add_s_to_sha512s_to_s_chain(temp_section_find, target);
    }

    return 0;
}

int link_sect_to_checksum_structures(database_handle* dh, section* target) {
    checksum_result* target_checksum_result;
    int i;

    for (i = 0; i < CHECKSUM_MAX_NUM; i++) {
        target_checksum_result = target->checksum + i;

        switch (target_checksum_result->type) {
            case CHECKSUM_UNUSED :
                // do nothing
                break;
            case CHECKSUM_SHA1_ID :
                link_sect_to_sha1_structures(dh,    target, target_checksum_result);
                break;
            case CHECKSUM_SHA256_ID :
                link_sect_to_sha256_structures(dh,  target, target_checksum_result);
                break;
            case CHECKSUM_SHA512_ID :
                link_sect_to_sha512_structures(dh,  target, target_checksum_result);
                break;
            default :
                return LOGIC_ERROR;
        }
    }

    return 0;
}

int grow_children_array(linked_entry* parent, ffp_eid_int increment) {
    linked_entry** temp_entry_p;
    linked_entry** temp_entry_p2;

    // grow parent's children array
    if (parent->child_num == 0 && parent->child_free_num == 0) {
        temp_entry_p =
            malloc(
                    sizeof(linked_entry*)
                    * increment
                  );

        temp_entry_p2 = temp_entry_p;
    }
    else {
        temp_entry_p =
            realloc(
                    parent->child,

                    sizeof(linked_entry*)
                    * (parent->child_num + parent->child_free_num + increment)
                   );

        if (temp_entry_p) {
            temp_entry_p2 = temp_entry_p + parent->child_num;
        }
    }

    if (!temp_entry_p) {
        return MALLOC_FAIL;
    }

    // wipe array
    mem_wipe_sec(temp_entry_p2, increment * sizeof(linked_entry*));

    // update pointer
    parent->child = temp_entry_p;

    // update stats
    parent->child_free_num += increment;

    return 0;
}

int put_into_children_array(linked_entry* parent, linked_entry* entry) {
    int ret;

    linked_entry** slot;

    if (!parent) {
        return WRONG_ARGS;
    }

    if (parent->child_free_num == 0) {
        ret = grow_children_array(parent, 1);
        if (ret) {
            return ret;
        }
    }

    slot = parent->child + parent->child_num;

    *slot = entry;

    // add remaining linkage
    entry->parent = parent;

    // set depth
    entry->depth = parent->depth + 1;

    // update stats
    parent->child_num++;
    parent->child_free_num--;

    return 0;
}

int grow_section_array(file_data* target, uint64_t increment) {
    section** temp_sect_p;
    section** temp_sect_p2;

    // grow target's section array
    if (target->section_num == 0 && target->section_free_num == 0) {
        temp_sect_p =
            malloc(
                    sizeof(section*)
                    * increment
                  );
        temp_sect_p2 = temp_sect_p;
    }
    else {
        temp_sect_p =
            realloc(
                    target->section,

                    sizeof(section*)
                    * (target->section_num + target->section_free_num + increment)
                   );

        if (temp_sect_p) {
            temp_sect_p2 = temp_sect_p + target->section_num;
        }
    }

    if (!temp_sect_p) {
        return MALLOC_FAIL;
    }

    // wipe array
    mem_wipe_sec(temp_sect_p2, increment * sizeof(section*));

    // update pointer
    target->section = temp_sect_p;

    // update stats
    target->section_free_num += increment;

    return 0;
}

int put_into_section_array(file_data* target, section* sect) {
    int ret;

    section** slot;

    if (!target) {
        return WRONG_ARGS;
    }

    if (target->section_free_num == 0) {
        ret = grow_section_array(target, 1);
        if (ret) {
            return ret;
        }
    }

    slot = target->section + target->section_num;

    *slot = sect;

    // add remaining linkage
    sect->parent_file_data = target;

    // update stats
    target->section_num++;
    target->section_free_num--;

    return 0;
}

int grow_sect_field_array(field_record* rec, uint64_t increment) {
    sect_field** temp_sect_field_p;
    sect_field** temp_sect_field_p2;

    // grow target's section array
    if (rec->sect_field_num == 0 && rec->sect_field_free_num == 0) {
        temp_sect_field_p =
            malloc(
                    sizeof(sect_field*)
                    * increment
                  );
        temp_sect_field_p2 = temp_sect_field_p;
    }
    else {
        temp_sect_field_p =
            realloc(
                    rec->sect,

                    sizeof(sect_field*)
                    * (rec->sect_field_num + rec->sect_field_free_num + increment)
                   );

        if (temp_sect_field_p) {
            temp_sect_field_p2 = temp_sect_field_p + rec->sect_field_num;
        }
    }

    if (!temp_sect_field_p) {
        return MALLOC_FAIL;
    }

    // wipe array
    mem_wipe_sec(temp_sect_field_p2, increment * sizeof(sect_field*));

    // update pointer
    rec->sect= temp_sect_field_p;

    // update stats
    rec->sect_field_free_num += increment;

    return 0;
}

int put_into_sect_field_array(field_record* rec, sect_field* sect) {
    int ret;

    sect_field** slot;

    if (!rec) {
        return WRONG_ARGS;
    }

    if (rec->sect_field_free_num == 0) {
        ret = grow_sect_field_array(rec, 1);
        if (ret) {
            return ret;
        }
    }

    slot = rec->sect + rec->sect_field_num;

    *slot = sect;

    // update stats
    rec->sect_field_num++;
    rec->sect_field_free_num--;

    return 0;
}

int init_field_record(layer2_sect_field_arr* l2_arr, field_record* rec) {
    const int init_sect_field_num = 100;

    int i, ret;

    sect_field* temp_sect_field;

    if (!rec) {
        return WRONG_ARGS;
    }

    mem_wipe_sec(rec, sizeof(field_record));

    grow_sect_field_array(rec, init_sect_field_num);

    for (i = 0; i < init_sect_field_num; i++) {
        ret = add_sect_field_to_layer2_arr(l2_arr, &temp_sect_field, NULL);
        if (ret) {
            return ret;
        }

        put_into_sect_field_array(rec, temp_sect_field);
    }

    return 0;
}

int init_field_match_bitmap(field_match_bitmap* match_map) {
    const size_t map_size = 10000;
    const size_t map_alloc_size
        = sizeof(map_block) * get_bitmap_map_block_number(map_size);

    map_block* map_temp_raw;

    map_temp_raw = malloc(map_alloc_size);
    if (!map_temp_raw) {
        return MALLOC_FAIL;
    }
    bitmap_init(&match_map->map_buf1, map_temp_raw, 0, map_size, 0);

    map_temp_raw = malloc(map_alloc_size);
    if (!map_temp_raw) {
        return MALLOC_FAIL;
    }
    bitmap_init(&match_map->map_buf2, map_temp_raw, 0, map_size, 0);

    map_temp_raw = malloc(map_alloc_size);
    if (!map_temp_raw) {
        return MALLOC_FAIL;
    }
    bitmap_init(&match_map->map_result, map_temp_raw, 0, map_size, 0);

    return 0;
}

int copy_entry(database_handle* dst_dh, linked_entry* dst_parent, linked_entry* src_entry, unsigned char recursive) {
    int i, j;

    int ret;

    int number_of_tries = 0;

    unsigned char eid[EID_LEN];
    char eid_str[EID_STR_MAX+1];

    linked_entry* temp_entry_find;
    linked_entry* copied_entry;
    file_data* temp_file_data;
    file_data* temp_file_data_src;
    section* temp_section;
    section* temp_section_src;

    // generate id
    do {
        gen_entry_id(eid, eid_str);
        lookup_entry_id_via_dh(dst_dh, eid_str, &temp_entry_find);
        number_of_tries++;
    } while (temp_entry_find && number_of_tries < 1000);

    if (number_of_tries == 1000) {
        return GEN_ID_FAIL;
    }

    // allocate space for new copied entry
    ret = add_entry_to_layer2_arr(&dst_dh->l2_entry_arr, &copied_entry, NULL);
    if (ret) {
        printf("copy_entry : failed to get space for new entry\n");
        return ret;
    }

    // fill in entry_id
    for (i = 0; i < EID_LEN; i++) {
        copied_entry->entry_id[i] = eid[i];
    }
    strcpy(copied_entry->entry_id_str, eid_str);

    // link to database via entry id
    link_entry_to_entry_id_structures(dst_dh, copied_entry);

    // copy file name and length
    strcpy(copied_entry->file_name, src_entry->file_name);
    copied_entry->file_name_len = src_entry->file_name_len;
    // copy type
    copied_entry->type = src_entry->type;
    // copy tag string and length
    strcpy(copied_entry->tag_str, src_entry->tag_str);
    copied_entry->tag_str_len = src_entry->tag_str_len;
    // copy user message and length
    strcpy(copied_entry->user_msg, src_entry->tag_str);
    copied_entry->user_msg_len = src_entry->user_msg_len;

    // copy time structures
    if (src_entry->tod_utc_used) {
        copied_entry->tod_utc_used = 1;
        memcpy( &copied_entry->tod_utc,
                &src_entry->tod_utc,

                sizeof(struct tm)
              );

        link_entry_to_date_time_tree(dst_dh, copied_entry, DATE_TOD);
    }
    if (src_entry->tusr_utc_used) {
        copied_entry->tusr_utc_used = 1;
        memcpy( &copied_entry->tusr_utc,
                &src_entry->tusr_utc,

                sizeof(struct tm)
              );

        link_entry_to_date_time_tree(dst_dh, copied_entry, DATE_TUSR);
    }

    // link to new parent
    copied_entry->parent = dst_parent;

    // insert itself to parent
    put_into_children_array(dst_parent, copied_entry);

    if (recursive) {
        if (src_entry->child_num > 0) {
            grow_children_array(copied_entry, src_entry->child_num);
            for (i = 0; i < src_entry->child_num; i++) {
                copy_entry(dst_dh, copied_entry, src_entry->child[i], recursive);
            }
        }
    }

    // link to database via file name
    link_entry_to_file_name_structures(dst_dh, copied_entry);

    if (src_entry->tag_str_len > 0) {  // if tags are used
        // link to database via tag string
        link_entry_to_entry_id_structures(dst_dh, copied_entry);
    }
    
    if (src_entry->data) { // if the src entry carries file data
        // allocate space for new copied file data
        ret = add_file_data_to_layer2_arr(&dst_dh->l2_file_data_arr, &temp_file_data, NULL);
        if (ret) {
            printf("copy_entry : failed to get space for new copied file data\n");
            return ret;
        }
        copied_entry->data = temp_file_data;

        // copy file data
        temp_file_data_src = src_entry->data;

        // copy file checksums
        for (i = 0; i < CHECKSUM_MAX_NUM; i++) {
            copy_checksum_result(temp_file_data->checksum + i, temp_file_data_src->checksum + i);
        }

        // link to database via file checksums
        link_file_data_to_checksum_structures(dst_dh, temp_file_data);

        // copy extracts
        for (i = 0; i < temp_file_data_src->extract_num; i++) {
            copy_extract_sample(temp_file_data->extract + i, temp_file_data_src->extract + i);
        }
        temp_file_data->extract_num = temp_file_data_src->extract_num;

        // copy file size
        temp_file_data->file_size = temp_file_data_src->file_size;
        strcpy(temp_file_data->file_size_str, temp_file_data_src->file_size_str);

        // link to database via file size
        link_file_data_to_file_size_structures(dst_dh, temp_file_data);

        // copy section sizes
        temp_file_data->norm_sect_size = temp_file_data_src->norm_sect_size;
        temp_file_data->last_sect_size = temp_file_data_src->last_sect_size;

        // link file data to parent entry
        temp_file_data->parent_entry = copied_entry;

        if (temp_file_data_src->section_num > 0) {  // if source file data contains sections
            // grow section pointer array
            grow_section_array(temp_file_data, temp_file_data_src->section_num);

            // fill in section pointer array
            for (i = 0; i < temp_file_data_src->section_num; i++) {
                // grab section object from l2 array
                ret = add_section_to_layer2_arr(&dst_dh->l2_section_arr, &temp_section, NULL);
                if (ret) {
                    printf("copy_entry : failed to get space for new section, section #%d\n", i);
                    return ret;
                }
                printf("temp_section : %p\n", temp_section);

                put_into_section_array(temp_file_data, temp_section);
            }

            // copy sections over
            for (i = 0; i < temp_file_data_src->section_num; i++) {
                temp_section = temp_file_data->section[i];
                temp_section_src = temp_file_data_src->section[i];

                temp_section->start_pos = temp_section_src->start_pos;
                temp_section->end_pos = temp_section_src->end_pos;

                // copy section checksums
                for (j = 0; j < CHECKSUM_MAX_NUM; j++) {
                    copy_checksum_result(temp_section->checksum + j, temp_section_src->checksum + j);
                }

                // link to database via section checksums
                link_sect_to_checksum_structures(dst_dh, temp_section);

                // copy extract
                for (j = 0; j < temp_section_src->extract_num; j++) {
                    copy_extract_sample(temp_section->extract + j, temp_section_src->extract + j);
                }
                temp_section->extract_num = temp_section_src->extract_num;

                // link section to parent file data
                temp_section->parent_file_data = temp_file_data;
            }
        }
    }

    // set has_parent flag
    if (copied_entry->parent == &dst_dh->tree) {   // head of branch
        copied_entry->has_parent = 0;
    }
    else {
        copied_entry->has_parent = 1;
    }

    // copy created by flag
    copied_entry->created_by = src_entry->created_by;

    // set depth
    copied_entry->depth = copied_entry->parent->depth + 1;

    // fill in new branch id
    if (copied_entry->has_parent) {
        // copy branch id from parent
        for (i = 0; i < EID_LEN; i++) {
            copied_entry->branch_id[i] = copied_entry->parent->branch_id[i];
        }
        strcpy(copied_entry->branch_id_str, copied_entry->parent->branch_id_str);
    }
    else {
        // fill in branch id using entry id
        for (i = 0; i < EID_LEN; i++) {
            copied_entry->branch_id[i] = copied_entry->entry_id[i];
        }
        strcpy(copied_entry->branch_id_str, copied_entry->entry_id_str);
    }

    ret = verify_entry(dst_dh, copied_entry, 0x0);
    if (ret) {
        printf("copy_entry : verify entry detected errors\n");
        printf("Please report to developer as this should not happen\n");
        printf("It is recommended that you revert to previous version of database as this may indicate your database is now corrupted\n");
        printf("Sorry for the inconvenience\n");
    }

    return 0;
}

int del_section (database_handle* dh, section* sect) {
    int i;

    checksum_result* temp_checksum;

    // unlink from section checksum structures
    for (i = 0; i < CHECKSUM_MAX_NUM; i++) {
        temp_checksum = sect->checksum + i;

        switch (temp_checksum->type) {
            case CHECKSUM_UNUSED :
                // do nothing
                break;
            case CHECKSUM_SHA1_ID :
                del_s_from_sha1s_to_s_chain(&dh->sha1s_to_s, &dh->sha1s_mat, &dh->l2_sha1s_to_s_arr, sect);
                break;
            case CHECKSUM_SHA256_ID :
                del_s_from_sha256s_to_s_chain(&dh->sha256s_to_s, &dh->sha256s_mat, &dh->l2_sha256s_to_s_arr, sect);
                break;
            case CHECKSUM_SHA512_ID :
                del_s_from_sha512s_to_s_chain(&dh->sha512s_to_s, &dh->sha512s_mat, &dh->l2_sha512s_to_s_arr, sect);
                break;
            default :
                return LOGIC_ERROR;
        }
    }

    // delete section from layer 2 array
    del_section_from_layer2_arr(&dh->l2_section_arr, sect->obj_arr_index);

    return 0;
}

int del_file_data (database_handle* dh, file_data* data) {
    uint64_t i;

    checksum_result* temp_checksum;

    // unlink from file data checksum structures
    for (i = 0; i < CHECKSUM_MAX_NUM; i++) {
        temp_checksum = data->checksum + i;

        switch (temp_checksum->type) {
            case CHECKSUM_UNUSED :
                // do nothing
                break;
            case CHECKSUM_SHA1_ID :
                del_fd_from_sha1f_to_fd_chain(&dh->sha1f_to_fd, &dh->sha1f_mat, &dh->l2_sha1f_to_fd_arr, data);
                break;
            case CHECKSUM_SHA256_ID :
                del_fd_from_sha256f_to_fd_chain(&dh->sha256f_to_fd, &dh->sha256f_mat, &dh->l2_sha256f_to_fd_arr, data);
                break;
            case CHECKSUM_SHA512_ID :
                del_fd_from_sha512f_to_fd_chain(&dh->sha512f_to_fd, &dh->sha512f_mat, &dh->l2_sha512f_to_fd_arr, data);
                break;
            default :
                return LOGIC_ERROR;
        }
    }

    // delete sections
    for (i = 0; i < data->section_num; i++) {
        del_section(dh, data->section[i]);
    }

    // same file size double linked list
    if (data->f_size_to_fd) {     // if previously linked to file size structures
        del_fd_from_f_size_to_fd_chain(&dh->f_size_to_fd, &dh->f_size_mat, &dh->l2_f_size_to_fd_arr, data);
    }

    // delete file data from layer 2 array
    del_file_data_from_layer2_arr(&dh->l2_file_data_arr, data->obj_arr_index);

    return 0;
}

int del_entry (database_handle* dh, linked_entry* entry) {
    int i, j;
    linked_entry* parent;

    parent = entry->parent;

    if (entry->child_num > 0) {
        // recursive delete
        for (i = 0; i < entry->child_num; i++) {
            del_entry(dh, entry->child[i]);
        }

        free(entry->child);
    }

    // find entry in parent's child array
    for (i = 0; i < parent->child_num; i++) {
        if (parent->child[i] == entry) {
            break;
        }
    }
    if (i == parent->child_num) {   // no match found
        return LOGIC_ERROR;
    }

    // shift the array
    for (j = i; j < parent->child_num; j++) {
        parent->child[j] = parent->child[j+1];
    }

    // nullify the extra spot
    parent->child[parent->child_num-1] = 0;

    // update array stats
    parent->child_num--;
    parent->child_free_num++;

    /* Remove from other structures */ 

    // remove from entry id structures
    // entries are never chained based on entry id
    unlink_entry_from_entry_id_structures(dh, entry);

    // same file name double linked list
    del_e_from_fn_to_e_chain(&dh->fn_to_e, &dh->fn_mat, &dh->l2_fn_to_e_arr, entry);

    // same tag double linked list
    if (entry->tag_to_e) {  // if previously linked to tag structures
        del_e_from_tag_to_e_chain(&dh->tag_to_e, &dh->tag_mat, &dh->l2_tag_to_e_arr, entry);
    }

    if (entry->data) {  // if entry contains file data
        del_file_data(dh, entry->data);
    }

    // remove from tod date time tree
    if (entry->tod_utc_used) {
        unlink_entry_from_date_time_tree(dh, entry, DATE_TOD);
    }

    // remove from tom date time tree
    if (entry->tom_utc_used) {
        unlink_entry_from_date_time_tree(dh, entry, DATE_TOD);
    }

    // remove from tusr date time tree
    if (entry->tusr_utc_used) {
        unlink_entry_from_date_time_tree(dh, entry, DATE_TUSR);
    }

    // delete from entry layer 2 array
    del_entry_from_layer2_arr(&dh->l2_entry_arr, entry->obj_arr_index);

    return 0;
}

/* map conversion */
// name translation structure map to entry map
int part_name_trans_map_to_entry_map(layer2_fn_to_e_arr* l2_arr, simple_bitmap* name_trans_map, simple_bitmap* entry_map, char* name_part) {
    bit_index i, j;
    bit_index temp_index_skip_to, temp_index_result;
    bit_index temp_index_skip_to2, temp_index_result2;

    linked_entry* temp_entry;

    layer1_fn_to_e_arr* temp_l1_arr;

    for (i = 0, temp_index_skip_to = 0; i < name_trans_map->number_of_ones; i++) {
        // get index of L1 array that contains the translation structure
        bitmap_first_one_bit_index(name_trans_map, &temp_index_result, temp_index_skip_to);
        temp_index_skip_to = temp_index_result + 1;

        // get the L1 array
        get_l1_fn_to_e_from_layer2_arr(l2_arr, &temp_l1_arr, temp_index_result);

        // go through the L1 array to find entries of partial matching string
        for (j = 0, temp_index_skip_to2 = 0; j < temp_l1_arr->usage_map.number_of_ones; j++) {
            bitmap_first_one_bit_index(&temp_l1_arr->usage_map, &temp_index_result2, temp_index_skip_to2);
            temp_index_skip_to2 = temp_index_result2 + 1;
            if (strstr(temp_l1_arr->arr[temp_index_result2].str, name_part)) {  // if entry contains target string as a substring
                // mark all possible entries in bitmap
                temp_entry = temp_l1_arr->arr[temp_index_result2].head_tar;

                while (temp_entry) {
                    ffp_grow_bitmap(entry_map, temp_entry->obj_arr_index + 1);

                    bitmap_write(entry_map, temp_entry->obj_arr_index, 1);

                    temp_entry = temp_entry->next_same_fn;
                }
            }
        }
    }

    return 0;
}

int part_f_size_trans_map_to_entry_map(layer2_f_size_to_fd_arr* l2_arr, simple_bitmap* f_size_trans_map, simple_bitmap* entry_map, char* f_size_part) {
    bit_index i, j;
    bit_index temp_index_skip_to, temp_index_result;
    bit_index temp_index_skip_to2, temp_index_result2;

    linked_entry* temp_entry;
    file_data* temp_file_data;

    layer1_f_size_to_fd_arr* temp_l1_arr;

    bitmap_zero(entry_map);

    for (i = 0, temp_index_skip_to = 0; i < f_size_trans_map->number_of_ones; i++) {
        // get index of L1 array that contains the translation structure
        bitmap_first_one_bit_index(f_size_trans_map, &temp_index_result, temp_index_skip_to);
        temp_index_skip_to = temp_index_result + 1;

        // get the L1 array
        get_l1_f_size_to_fd_from_layer2_arr(l2_arr, &temp_l1_arr, temp_index_result);

        // go through the L1 array to find entries of partial matching string
        for (j = 0, temp_index_skip_to2 = 0; j < temp_l1_arr->usage_map.number_of_ones; j++) {
            bitmap_first_one_bit_index(&temp_l1_arr->usage_map, &temp_index_result2, temp_index_skip_to2);
            temp_index_skip_to2 = temp_index_result2 + 1;
            if (strstr(temp_l1_arr->arr[temp_index_result2].str, f_size_part)) {  // if entry contains target string as a substring
                // mark all possible entries in bitmap
                temp_file_data = temp_l1_arr->arr[temp_index_result2].head_tar;

                while (temp_file_data) {
                    temp_entry = temp_file_data->parent_entry;

                    ffp_grow_bitmap(entry_map, temp_entry->obj_arr_index + 1);

                    bitmap_write(entry_map, temp_entry->obj_arr_index, 1);

                    temp_file_data = temp_file_data->next_same_f_size;
                }
            }
        }
    }

    return 0;
}

int part_f_sha1_trans_map_to_entry_map(layer2_sha1f_to_fd_arr* l2_arr, simple_bitmap* f_sha1_trans_map, simple_bitmap* entry_map, char* f_sha1_part) {
    bit_index i, j;
    bit_index temp_index_skip_to, temp_index_result;
    bit_index temp_index_skip_to2, temp_index_result2;

    linked_entry* temp_entry;
    file_data* temp_file_data;

    layer1_sha1f_to_fd_arr* temp_l1_arr;

    bitmap_zero(entry_map);

    for (i = 0, temp_index_skip_to = 0; i < f_sha1_trans_map->number_of_ones; i++) {
        // get index of L1 array that contains the translation structure
        bitmap_first_one_bit_index(f_sha1_trans_map, &temp_index_result, temp_index_skip_to);
        temp_index_skip_to = temp_index_result + 1;

        // get the L1 array
        get_l1_sha1f_to_fd_from_layer2_arr(l2_arr, &temp_l1_arr, temp_index_result);

        // go through the L1 array to find entries of partial matching string
        for (j = 0, temp_index_skip_to2 = 0; j < temp_l1_arr->usage_map.number_of_ones; j++) {
            bitmap_first_one_bit_index(&temp_l1_arr->usage_map, &temp_index_result2, temp_index_skip_to2);
            temp_index_skip_to2 = temp_index_result2 + 1;
            if (strstr(temp_l1_arr->arr[temp_index_result2].str, f_sha1_part)) {  // if entry contains target string as a substring
                // mark all possible entries in bitmap
                temp_file_data = temp_l1_arr->arr[temp_index_result2].head_tar;

                while (temp_file_data) {
                    temp_entry = temp_file_data->parent_entry;

                    ffp_grow_bitmap(entry_map, temp_entry->obj_arr_index + 1);

                    bitmap_write(entry_map, temp_entry->obj_arr_index, 1);

                    temp_file_data = temp_file_data->next_same_sha1;
                }
            }
        }
    }

    return 0;
}

int part_f_sha256_trans_map_to_entry_map(layer2_sha256f_to_fd_arr* l2_arr, simple_bitmap* f_sha256_trans_map, simple_bitmap* entry_map, char* f_sha256_part) {
    bit_index i, j;
    bit_index temp_index_skip_to, temp_index_result;
    bit_index temp_index_skip_to2, temp_index_result2;

    linked_entry* temp_entry;
    file_data* temp_file_data;

    layer1_sha256f_to_fd_arr* temp_l1_arr;

    bitmap_zero(entry_map);

    for (i = 0, temp_index_skip_to = 0; i < f_sha256_trans_map->number_of_ones; i++) {
        // get index of L1 array that contains the translation structure
        bitmap_first_one_bit_index(f_sha256_trans_map, &temp_index_result, temp_index_skip_to);
        temp_index_skip_to = temp_index_result + 1;

        // get the L1 array
        get_l1_sha256f_to_fd_from_layer2_arr(l2_arr, &temp_l1_arr, temp_index_result);

        // go through the L1 array to find entries of partial matching string
        for (j = 0, temp_index_skip_to2 = 0; j < temp_l1_arr->usage_map.number_of_ones; j++) {
            bitmap_first_one_bit_index(&temp_l1_arr->usage_map, &temp_index_result2, temp_index_skip_to2);
            temp_index_skip_to2 = temp_index_result2 + 1;
            if (strstr(temp_l1_arr->arr[temp_index_result2].str, f_sha256_part)) {  // if entry contains target string as a substring
                // mark all possible entries in bitmap
                temp_file_data = temp_l1_arr->arr[temp_index_result2].head_tar;

                while (temp_file_data) {
                    temp_entry = temp_file_data->parent_entry;

                    ffp_grow_bitmap(entry_map, temp_entry->obj_arr_index + 1);

                    bitmap_write(entry_map, temp_entry->obj_arr_index, 1);

                    temp_file_data = temp_file_data->next_same_sha256;
                }
            }
        }
    }

    return 0;
}

int part_f_sha512_trans_map_to_entry_map(layer2_sha512f_to_fd_arr* l2_arr, simple_bitmap* f_sha512_trans_map, simple_bitmap* entry_map, char* f_sha512_part) {
    bit_index i, j;
    bit_index temp_index_skip_to, temp_index_result;
    bit_index temp_index_skip_to2, temp_index_result2;

    linked_entry* temp_entry;
    file_data* temp_file_data;

    layer1_sha512f_to_fd_arr* temp_l1_arr;

    bitmap_zero(entry_map);

    for (i = 0, temp_index_skip_to = 0; i < f_sha512_trans_map->number_of_ones; i++) {
        // get index of L1 array that contains the translation structure
        bitmap_first_one_bit_index(f_sha512_trans_map, &temp_index_result, temp_index_skip_to);
        temp_index_skip_to = temp_index_result + 1;

        // get the L1 array
        get_l1_sha512f_to_fd_from_layer2_arr(l2_arr, &temp_l1_arr, temp_index_result);

        // go through the L1 array to find entries of partial matching string
        for (j = 0, temp_index_skip_to2 = 0; j < temp_l1_arr->usage_map.number_of_ones; j++) {
            bitmap_first_one_bit_index(&temp_l1_arr->usage_map, &temp_index_result2, temp_index_skip_to2);
            temp_index_skip_to2 = temp_index_result2 + 1;
            if (strstr(temp_l1_arr->arr[temp_index_result2].str, f_sha512_part)) {  // if entry contains target string as a substring
                // mark all possible entries in bitmap
                temp_file_data = temp_l1_arr->arr[temp_index_result2].head_tar;

                while (temp_file_data) {
                    temp_entry = temp_file_data->parent_entry;

                    ffp_grow_bitmap(entry_map, temp_entry->obj_arr_index + 1);

                    bitmap_write(entry_map, temp_entry->obj_arr_index, 1);

                    temp_file_data = temp_file_data->next_same_sha512;
                }
            }
        }
    }

    return 0;
}

int part_s_sha1_trans_map_to_entry_map(layer2_sha1s_to_s_arr* l2_arr, simple_bitmap* s_sha1_trans_map, simple_bitmap* entry_map, char* s_sha1_part) {
    bit_index i, j;
    bit_index temp_index_skip_to, temp_index_result;
    bit_index temp_index_skip_to2, temp_index_result2;

    linked_entry* temp_entry;
    section* temp_section;

    layer1_sha1s_to_s_arr* temp_l1_arr;

    bitmap_zero(entry_map);

    for (i = 0, temp_index_skip_to = 0; i < s_sha1_trans_map->number_of_ones; i++) {
        // get index of L1 array that contains the translation structure
        bitmap_first_one_bit_index(s_sha1_trans_map, &temp_index_result, temp_index_skip_to);
        temp_index_skip_to = temp_index_result + 1;

        // get the L1 array
        get_l1_sha1s_to_s_from_layer2_arr(l2_arr, &temp_l1_arr, temp_index_result);

        // go through the L1 array to find entries of partial matching string
        for (j = 0, temp_index_skip_to2 = 0; j < temp_l1_arr->usage_map.number_of_ones; j++) {
            bitmap_first_one_bit_index(&temp_l1_arr->usage_map, &temp_index_result2, temp_index_skip_to2);
            temp_index_skip_to2 = temp_index_result2 + 1;
            if (strstr(temp_l1_arr->arr[temp_index_result2].str, s_sha1_part)) {  // if entry contains target string as a substring
                // mark all possible entries in bitmap
                temp_section = temp_l1_arr->arr[temp_index_result2].head_tar;

                while (temp_section) {
                    temp_entry = temp_section->parent_file_data->parent_entry;

                    ffp_grow_bitmap(entry_map, temp_entry->obj_arr_index + 1);

                    bitmap_write(entry_map, temp_entry->obj_arr_index, 1);

                    temp_section = temp_section->next_same_sha1;
                }
            }
        }
    }

    return 0;
}

int part_s_sha256_trans_map_to_entry_map(layer2_sha256s_to_s_arr* l2_arr, simple_bitmap* s_sha256_trans_map, simple_bitmap* entry_map, char* s_sha256_part) {
    bit_index i, j;
    bit_index temp_index_skip_to, temp_index_result;
    bit_index temp_index_skip_to2, temp_index_result2;

    linked_entry* temp_entry;
    section* temp_section;

    layer1_sha256s_to_s_arr* temp_l1_arr;

    bitmap_zero(entry_map);

    for (i = 0, temp_index_skip_to = 0; i < s_sha256_trans_map->number_of_ones; i++) {
        // get index of L1 array that contains the translation structure
        bitmap_first_one_bit_index(s_sha256_trans_map, &temp_index_result, temp_index_skip_to);
        temp_index_skip_to = temp_index_result + 1;

        // get the L1 array
        get_l1_sha256s_to_s_from_layer2_arr(l2_arr, &temp_l1_arr, temp_index_result);

        // go through the L1 array to find entries of partial matching string
        for (j = 0, temp_index_skip_to2 = 0; j < temp_l1_arr->usage_map.number_of_ones; j++) {
            bitmap_first_one_bit_index(&temp_l1_arr->usage_map, &temp_index_result2, temp_index_skip_to2);
            temp_index_skip_to2 = temp_index_result2 + 1;
            if (strstr(temp_l1_arr->arr[temp_index_result2].str, s_sha256_part)) {  // if entry contains target string as a substring
                // mark all possible entries in bitmap
                temp_section = temp_l1_arr->arr[temp_index_result2].head_tar;

                while (temp_section) {
                    temp_entry = temp_section->parent_file_data->parent_entry;

                    ffp_grow_bitmap(entry_map, temp_entry->obj_arr_index + 1);

                    bitmap_write(entry_map, temp_entry->obj_arr_index, 1);

                    temp_section = temp_section->next_same_sha256;
                }
            }
        }
    }

    return 0;
}

int part_s_sha512_trans_map_to_entry_map(layer2_sha512s_to_s_arr* l2_arr, simple_bitmap* s_sha512_trans_map, simple_bitmap* entry_map, char* s_sha512_part) {
    bit_index i, j;
    bit_index temp_index_skip_to, temp_index_result;
    bit_index temp_index_skip_to2, temp_index_result2;

    linked_entry* temp_entry;
    section* temp_section;

    layer1_sha512s_to_s_arr* temp_l1_arr;

    bitmap_zero(entry_map);

    for (i = 0, temp_index_skip_to = 0; i < s_sha512_trans_map->number_of_ones; i++) {
        // get index of L1 array that contains the translation structure
        bitmap_first_one_bit_index(s_sha512_trans_map, &temp_index_result, temp_index_skip_to);
        temp_index_skip_to = temp_index_result + 1;

        // get the L1 array
        get_l1_sha512s_to_s_from_layer2_arr(l2_arr, &temp_l1_arr, temp_index_result);

        // go through the L1 array to find entries of partial matching string
        for (j = 0, temp_index_skip_to2 = 0; j < temp_l1_arr->usage_map.number_of_ones; j++) {
            bitmap_first_one_bit_index(&temp_l1_arr->usage_map, &temp_index_result2, temp_index_skip_to2);
            temp_index_skip_to2 = temp_index_result2 + 1;
            if (strstr(temp_l1_arr->arr[temp_index_result2].str, s_sha512_part)) {  // if entry contains target string as a substring
                // mark all possible entries in bitmap
                temp_section = temp_l1_arr->arr[temp_index_result2].head_tar;

                while (temp_section) {
                    temp_entry = temp_section->parent_file_data->parent_entry;

                    ffp_grow_bitmap(entry_map, temp_entry->obj_arr_index + 1);

                    bitmap_write(entry_map, temp_entry->obj_arr_index, 1);

                    temp_section = temp_section->next_same_sha512;
                }
            }
        }
    }

    return 0;
}

// local macro
#define scan_l2_arr(tag_attr, tag_tar, ret, ret2, indx) \
    ret = 0;                                                    \
    indx = 0;                                                   \
    while (ret != INDEX_OUT_OF_RANGE) {                         \
        ret = get_l1_##tag_attr##_to_##tag_tar##_from_layer2_arr(&dh->l2_##tag_attr##_to_##tag_tar##_arr, &l1_##tag_attr##_to_##tag_tar##_arr, indx); \
        if (l1_eid_to_e_arr) {                                  \
            ret2 = mem_scan(l1_##tag_attr##_to_##tag_tar##_arr, sizeof(layer1_##tag_attr##_to_##tag_tar##_arr), pattern, pattern_size, ptr_buf, buf_size, &num_used); \
            switch (ret2) {                                     \
                case 0:                                         \
                    break;                                      \
                case BUFFER_FULL:                               \
                    printf("scanner pointer buffer full\n");    \
                    break;                                      \
                default:                                        \
                    continue;                                   \
            }                                                   \
                                                                \
            if (num_used > 0) {                                 \
                sprintf(msg, "matches found within layer 1 array #%"PRIu64"\n", indx); \
                printf("%s%.*s : %"PRIu32"\n", msg, calc_pad(msg,PRINT_PAD_SIZE), space_pad, num_used); \
                                                                \
                for (i = 0; i < num_used; i++) {                \
                    printf("#%d    address : %p    offset from layer 1 array ptr : %ld\n", i, ptr_buf[i], (void*) ptr_buf[i] - (void*) l1_##tag_attr##_to_##tag_tar##_arr); \
                }                                               \
            }                                                   \
        }                                                       \
                                                                \
        indx++;                                                 \
    }

#define scan_pool(tag_tar, ret, ret2, indx) \
    ret = 0;                                                    \
    indx = 0;                                                   \
    while (ret != INDEX_OUT_OF_RANGE) {                         \
        ret = get_l1_##tag_tar##_from_layer2_arr(&dh->l2_##tag_tar##_arr, &l1_##tag_tar##_arr, indx); \
        if (l1_eid_to_e_arr) {                                  \
            ret2 = mem_scan(l1_##tag_tar##_arr, sizeof(layer1_##tag_tar##_arr), pattern, pattern_size, ptr_buf, buf_size, &num_used); \
            switch (ret2) {                                     \
                case 0:                                         \
                    break;                                      \
                case BUFFER_FULL:                               \
                    printf("scanner pointer buffer full\n");    \
                    break;                                      \
                default:                                        \
                    continue;                                   \
            }                                                   \
                                                                \
            if (num_used > 0) {                                 \
                sprintf(msg, "matches found within layer 1 array #%"PRIu64"\n", indx); \
                printf("%s%.*s : %"PRIu32"\n", msg, calc_pad(msg,PRINT_PAD_SIZE), space_pad, num_used); \
                                                                \
                for (i = 0; i < num_used; i++) {                \
                    printf("#%d    address : %p    offset from layer 1 array ptr : %ld\n", i, ptr_buf[i], (void*) ptr_buf[i] - (void*)l1_##tag_tar##_arr); \
                }                                               \
            }                                                   \
        }                                                       \
                                                                \
        indx++;                                                 \
    }

int scan_db(database_handle* dh, unsigned char* pattern, size_t pattern_size) {
    const uint32_t buf_size = 1000;
    const unsigned char* ptr_buf[buf_size];
    uint32_t num_used;
    int ret, ret2;

    char msg[200];

    int i;

    layer1_entry_arr*                   l1_entry_arr;
    layer1_file_data_arr*               l1_file_data_arr;
    layer1_section_arr*                 l1_section_arr;
    layer1_misc_alloc_record_arr*       l1_misc_alloc_record_arr;
    layer1_dtt_year_arr*                l1_dtt_year_arr;

    layer1_eid_to_e_arr*    l1_eid_to_e_arr;
    layer1_fn_to_e_arr*     l1_fn_to_e_arr;
    layer1_tag_to_e_arr*    l1_tag_to_e_arr;

    layer1_f_size_to_fd_arr*    l1_f_size_to_fd_arr;

    layer1_sha1f_to_fd_arr*     l1_sha1f_to_fd_arr;
    layer1_sha256f_to_fd_arr*   l1_sha256f_to_fd_arr;
    layer1_sha512f_to_fd_arr*   l1_sha512f_to_fd_arr;

    layer1_sha1s_to_s_arr*     l1_sha1s_to_s_arr;
    layer1_sha256s_to_s_arr*   l1_sha256s_to_s_arr;
    layer1_sha512s_to_s_arr*   l1_sha512s_to_s_arr;

    bit_index k;

    printf("scanning database : %s\n", dh->name);

    // scan db itself
    ret = mem_scan(dh, sizeof(database_handle), pattern, pattern_size, ptr_buf, buf_size, &num_used);
    switch (ret) {
        case 0:
            break;
        case BUFFER_FULL:
            printf("scanner pointer buffer full\n");
            break;
        default:
            return ret;
    }

    if (num_used > 0) {
        strcpy(msg, "matches found within database object");
        printf("%s%.*s : %"PRIu32"\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad, num_used);

        for (i = 0; i < num_used; i++) {
            printf("#%d    address : %p    offset from dh ptr : %ld\n", i, ptr_buf[i], (void*) ptr_buf[i] - (void*) dh);
        }
    }

    /* scan layer 2 arrays/pools */
    // scan id translation structures
    printf("scanning entry id translation structures\n");
    scan_l2_arr(eid, e, ret, ret2, k)

    // scan file name translation structures
    printf("scanning file name translation structures\n");
    scan_l2_arr(fn, e, ret, ret2, k);

    // scan tag translation structures
    printf("scanning tag translation structures\n");
    scan_l2_arr(tag, e, ret, ret2, k);

    // scan file data checksum translation structures
    //      sha1
    printf("scanning file data sha1 translation structures\n");
    scan_l2_arr(sha1f, fd, ret, ret2, k);
    //      sha256
    printf("scanning file data sha256 translation structures\n");
    scan_l2_arr(sha256f, fd, ret, ret2, k);
    //      sha512
    printf("scanning file data sha512 translation structures\n");
    scan_l2_arr(sha512f, fd, ret, ret2, k);

    // scan section checksum translation structures
    //      sha1
    printf("scanning section sha1 translation structures\n");
    scan_l2_arr(sha1s, s, ret, ret2, k);
    //      sha256
    printf("scanning section sha256 translation structures\n");
    scan_l2_arr(sha256s, s, ret, ret2, k);
    //      sha512
    printf("scanning section sha512 translation structures\n");
    scan_l2_arr(sha512s, s, ret, ret2, k);

    // scan file size translation structures
    printf("scanning file size translation structures\n");
    scan_l2_arr(f_size, fd, ret, ret2, k);

    // scan pool allocator structures
    //      entry
    printf("scanning entry pool\n");
    scan_pool(entry, ret, ret2, k);
    //      file data
    printf("scanning file data pool\n");
    scan_pool(file_data, ret, ret2, k);
    //      section
    printf("scanning section pool\n");
    scan_pool(section, ret, ret2, k);
    //      misc alloc record
    printf("scanning misc alloc record pool\n");
    scan_pool(misc_alloc_record, ret, ret2, k);
    //      date time tree year
    printf("scanning date time tree year pool\n");
    scan_pool(dtt_year, ret, ret2, k);

    return 0;
}
