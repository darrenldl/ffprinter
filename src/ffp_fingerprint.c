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

#include "ffp_fingerprint.h"
#include "ffp_database.h"
#include "ffprinter_function_template.h"

#define record_dirp(l2_arr, temp_record, ret, target, max_index, er_h) \
    ret = add_dirp_record_to_layer2_arr(l2_arr, &temp_record, NULL);    \
    if (ret) {                                                          \
        error_write(er_h, "failed to record dirp");                     \
        return ret;                                                     \
    }                                                                   \
    temp_record->dirp = target;                                         \
    if (*max_index < temp_record->obj_arr_index) {           \
        *max_index = temp_record->obj_arr_index;             \
    }

#define del_dirp_record(l2_arr, ret, index, er_h) \
    ret = del_dirp_record_from_layer2_arr(l2_arr, index);   \
    if (ret) {                                              \
        error_write(er_h, "failed to delete dirp record");  \
    }                                                       \

const uint64_t file_size_class_arr[CLASS_NUM] =
{   1024LL,             //   1KiB
    10240LL,            //  10KiB
    102400LL,           // 100KiB

    1048576LL,          //   1MiB
    10485760LL,         //  10MiB
    104857600LL,        // 100MiB

    1073741824LL,       //   1GiB
    10737418240LL,      //  10GiB
    107374182400LL      // 100GiB
};

//const uint64_t sect_ideal_size_arr[CLASS_NUM] =
//  100B,       10KiB,       15MiB,           25MiB
//{   100LL,      10240LL,     15728640LL,      26214400LL      };

const uint64_t sect_ideal_num_arr[CLASS_NUM] = 
{   5,                  //   1KiB
    10,                 //  10KiB
    10,                 // 100KiB

    10,                 //   1MiB
    10,                 //  10MiB
    10,                 // 100MiB

    50,                 //   1GiB
    100,                //  10GiB
    100                 // 100GiB
};

init_layer1_obj_arr(dirp_record, L1_DIRP_RECORD_ARR_SIZE)
init_layer2_obj_arr(dirp_record, L2_DIRP_RECORD_INIT_SIZE)
add_obj_to_layer2_arr(dirp_record, dirp_record, L2_DIRP_RECORD_GROW_SIZE, L1_DIRP_RECORD_ARR_SIZE)
del_obj_from_layer2_arr(dirp_record, dirp_record, L1_DIRP_RECORD_ARR_SIZE)
get_obj_from_layer2_arr(dirp_record, dirp_record, L1_DIRP_RECORD_ARR_SIZE)
get_l1_obj_from_layer2_arr(dirp_record)
del_l2_obj_arr(dirp_record, L2_DIRP_RECORD_INIT_SIZE, L2_DIRP_RECORD_GROW_SIZE)

int fill_rand_name(linked_entry* entry) {
    int i;

    unsigned char name_bytes[FILE_NAME_MAX+1];

    int actual_file_name_len =
        ffp_min
        (
            FILE_NAME_MAX,
            RAND_FILE_NAME_LEN
        );

    for (i = 0; i < actual_file_name_len / 2; i++) {
        // below obviously skewed yadadadada
        name_bytes[i] = rand() % 256;
    }

    // convert to hex string
    bytes_to_hex_str(entry->file_name, name_bytes, actual_file_name_len / 2);

    entry->file_name_len = strlen(entry->file_name);

    return 0;
}

static int path_to_file_name (const char* path, char* file_name) {
    const char* file_name_p = NULL;

    int i;

    int end;

    int file_name_len = 0;

    if (path[strlen(path)-1] == '/') {
        // skip slash
        end = strlen(path) - 2;
    }
    else {
        end = strlen(path) - 1;
    }
    
    // read backwards till a slash is found
    for (i = end; i >= 0; i--) {
        if (path[i] == '/' && (i > 0 ? path[i-1] != '\\' : 1)) {
            file_name_p = path + i + 1;
            break;
        }

        file_name_len++;

        if (i == 0) {   // avoid underflow and wrap around in case i is unsigned
            break;
        }
    }

    if (file_name_p == NULL) {
        file_name_p = path;
    }

    if (file_name_len > FILE_NAME_MAX) {
        // truncate length
        file_name_len = FILE_NAME_MAX;
    }
    else if (file_name_len == 0) {
        return FILE_NAME_EMPTY;
    }

    // fill in file name
    for (i = 0; i < file_name_len; i++) {
        file_name[i] = file_name_p[i];
    }

    // terminate
    file_name[i] = 0;

    return 0;
}

int gen_tree (database_handle* dh, char* path, linked_entry* parent, uint32_t flags, unsigned char recursive, ffp_eid_int* rem_depth, error_handle* er_h, linked_entry** entry_being_used, FILE** file_being_used, layer2_dirp_record_arr* l2_dirp_record_arr, bit_index* max_dirp_record_index) {
    int ret;

    struct stat tar_stat;

    linked_entry* entry;

    DIR* dirp;

    struct dirent* dp;

    dirp_record* temp_dirp_record;

    error_mark_starter(er_h, "gen_tree");

    if (rem_depth && *rem_depth == 0) {
        return 0;
    }

    if (rem_depth) {
        *rem_depth = *rem_depth - 1;
    }

    if (stat(path, &tar_stat)) {
        error_write(er_h, "failed to get stats - file may not exist");
        return FS_FILE_ACCESS_FAIL;
    }

    if (S_ISDIR(tar_stat.st_mode)) {
        if (!recursive) {
            error_write(er_h, "specified a directory, but not in recursive mode");
            return GENERAL_FAIL;
        }

        if (chdir(path)) {
            error_write(er_h, "failed to change to directory");
            return FS_FILE_ACCESS_FAIL;
        }

        SET_NOT_INTERRUPTABLE();

        // open the directory to go through the files
        dirp = opendir(".");

        // set loose resource
        record_dirp(l2_dirp_record_arr, temp_dirp_record, ret, dirp, max_dirp_record_index, er_h);

        // grab space for entry
        ret = add_entry_to_layer2_arr(&dh->l2_entry_arr, &entry, NULL);
        if (ret) {
            error_write(er_h, "failed to get space for entry");
            return ret;
        }

        // generate entry id
        ret = set_new_entry_id(dh, entry);
        if (ret) {
            error_write(er_h, "failed to generate new entry id");
            return ret;
        }

        // link to entry id related structures
        link_entry_to_entry_id_structures(dh, entry);

        // put into parent
        put_into_children_array(parent, entry);

        // set as group type
        entry->type = ENTRY_GROUP;

        // fill in file name
        if (flags & FPRINT_USE_F_NAME) {
            ret = path_to_file_name(path, entry->file_name);
            switch (ret) {
                case 0 :
                    break;
                case FILE_NAME_EMPTY:
                    error_write(er_h, "target file name is empty\n");
                    return ret;
                default:
                    error_write(er_h, "unknown error");
                    return ret;
            }

            entry->file_name_len = strlen(entry->file_name);
        }
        else {
            fill_rand_name(entry);
        }

        // link to file name related structures
        link_entry_to_file_name_structures(dh, entry);

        set_entry_general_info(dh, entry);

        entry->created_by = CREATED_BY_SYS;

        MARK_DB_UNSAVED(dh);

        SET_INTERRUPTABLE();

        if (dirp == NULL) {
            error_write(er_h, "failed to open directory");
            return FS_FILE_ACCESS_FAIL;
        }

        while ((dp = readdir(dirp))) {
            if (        strcmp(dp->d_name, ".")     == 0
                    ||  strcmp(dp->d_name, "..")    == 0
               )
            {
                continue;
            }

            if (stat(dp->d_name, &tar_stat)) {
                error_write(er_h, "failed to get stats - file may not exist");
                return FS_FILE_ACCESS_FAIL;
            }

            if (        S_ISDIR(tar_stat.st_mode)
                    ||  S_ISREG(tar_stat.st_mode)
               )
            {
                gen_tree(dh, dp->d_name, entry, flags, recursive, rem_depth, er_h, entry_being_used, file_being_used, l2_dirp_record_arr, max_dirp_record_index);
            }
            else /* other file types */ {
                ;;  // ignore
            }
        }

        SET_NOT_INTERRUPTABLE();

        if (closedir(dirp)) {
            error_write(er_h, "failed to close directory");
            return FS_FILE_ACCESS_FAIL;
        }

        // delete record
        del_dirp_record(l2_dirp_record_arr, ret, temp_dirp_record->obj_arr_index, er_h);

        if (chdir("..")) {  // go back one level
            error_write(er_h, "failed to change to directory");
            return FS_FILE_ACCESS_FAIL;
        }

        SET_INTERRUPTABLE();
    }
    else if (S_ISREG(tar_stat.st_mode)) {
        SET_NOT_INTERRUPTABLE();

        // grab space for entry
        ret = add_entry_to_layer2_arr(&dh->l2_entry_arr, &entry, NULL);
        if (ret) {
            error_write(er_h, "failed to get space for entry");
            return ret;
        }

        // set loose resource
        *entry_being_used = entry;

        // generate entry id
        ret = set_new_entry_id(dh, entry);
        if (ret) {
            error_write(er_h, "failed to generate new entry id");
            return ret;
        }

        // link to entry id related structures
        link_entry_to_entry_id_structures(dh, entry);

        // put into parent
        put_into_children_array(parent, entry);

        // set as file type
        entry->type = ENTRY_FILE;

        // fill in file name
        if (flags & FPRINT_USE_F_NAME) {
            ret = path_to_file_name(path, entry->file_name);
            switch (ret) {
                case 0 :
                    break;
                case FILE_NAME_EMPTY:
                    error_write(er_h, "target file name is empty\n");
                    return ret;
                default:
                    error_write(er_h, "unknown error");
                    return ret;
            }

            entry->file_name_len = strlen(entry->file_name);
        }
        else {
            fill_rand_name(entry);
        }

        // link entry to file name related structures
        link_entry_to_file_name_structures(dh, entry);

        set_entry_general_info(dh, entry);

        entry->created_by = CREATED_BY_SYS;

        SET_INTERRUPTABLE();

        ret = fingerprint_file(dh, path, entry, flags, er_h, file_being_used);
        if (ret) {
            return ret;
        }

        SET_NOT_INTERRUPTABLE();

        // clean up pointers
        *entry_being_used = NULL;

        SET_INTERRUPTABLE();
    }
    else {
        error_write(er_h, "unsupported file type");
        return FS_UNRECOGNISED_FILE_TYPE;
    }

    if (rem_depth) {
        *rem_depth = *rem_depth + 1;
    }

    return 0;
}

int fingerprint_file (database_handle* dh, char* path, linked_entry* entry, uint32_t flags, error_handle* er_h, FILE** file_being_used) {
    FILE* file;
    long int file_size;
    struct stat file_stat;
    file_data* temp_file_data;
    section* temp_section;
    checksum_result* temp_checksum;
    extract_sample* temp_extract;
    uint64_t sect_num;
    uint64_t norm_sect_size;
    uint64_t last_sect_size;
    uint64_t extract_pos;
    uint16_t extract_num;
    uint8_t extract_len;
    int bytes, bytes_left;
    uint64_t i, j;
    int ret2;
    int ret;

    SHA_CTX wf_sha1;
    SHA_CTX s_sha1;
    unsigned char sha1_digest   [   SHA_DIGEST_LENGTH       ];

    SHA256_CTX wf_sha256;
    SHA256_CTX s_sha256;
    unsigned char sha256_digest [   SHA256_DIGEST_LENGTH    ];

    SHA512_CTX wf_sha512;
    SHA512_CTX s_sha512;
    unsigned char sha512_digest [   SHA512_DIGEST_LENGTH    ];

    unsigned char data_buf[FILE_BUFFER_SIZE];   // 1KiB

    unsigned char fread_failed = 0;

    uint32_t sections_needed;

    error_mark_starter(er_h, "fingerprint_file");

    *file_being_used = NULL;

    if ((ret = verify_str_terminated(path, FS_PATH_MAX, NULL, 0x0))) {
        return ret;
    }

    if (stat(path, &file_stat)) {
        error_write(er_h, "failed to get stats - file may not exist");
        return FFP_GENERAL_FAIL;
    }

    if (        !S_ISREG(file_stat.st_mode)
            &&  !S_ISDIR(file_stat.st_mode)
       )
    {
        return FS_UNRECOGNISED_FILE_TYPE;
    }

    SET_NOT_INTERRUPTABLE();

    file = fopen(path, "rb");
    if (!file) {
        error_write(er_h, "unable to open file");
        return FOPEN_FAIL;
    }

    *file_being_used = file;

    SET_INTERRUPTABLE();

    file_size = file_stat.st_size;

    if (file_size == 0) {
        return 0;
    }

    if (file_size > FILE_SIZE_MAX) {
        printf("fingerprint_file : %s : file too large\n", entry->file_name);
        return FS_FILE_TOO_LARGE;
    }

    // initialise file wise hash structures
    if (flags & FPRINT_USE_F_SHA1) {
        SHA1_Init(&wf_sha1);
    }
    if (flags & FPRINT_USE_F_SHA256) {
        SHA256_Init(&wf_sha256);
    }
    if (flags & FPRINT_USE_F_SHA512) {
        SHA512_Init(&wf_sha512);
    }

    sections_needed
        =   (flags & FPRINT_USE_S_EXTR)
        |   (flags & FPRINT_USE_S_SHA1)
        |   (flags & FPRINT_USE_S_SHA256)
        |   (flags & FPRINT_USE_S_SHA512);

    // handle sections
    if (file_size < SECT_SIZE_SMALL) {   // overly small
        sect_num = 1;
        norm_sect_size = file_size;
        last_sect_size = file_size;
    }
    else {
        if (file_size > file_size_class_arr[CLASS_NUM - 1]) {  // overly large
            sect_num = FALLBACK_SECT_NUM;
        }
        else {      // categorised size
            sect_num = 0;
            for (i = CLASS_NUM - 1; i >= 0; i--) {   // search backwards
                if (file_size < file_size_class_arr[i]) {   // lies within class
                    sect_num = sect_ideal_num_arr[i];
                }

                if (i == 0) {   // avoid underflow and wrap around in case i is unsigned
                    break;
                }
            }
            if (sect_num == 0) {
                printf("fingerprint_file : logic error\n");
                return LOGIC_ERROR;
            }
        }

        norm_sect_size = (file_size + sect_num - 1) / sect_num;     // round up
        last_sect_size = file_size % norm_sect_size;
        if (last_sect_size == 0) {  // if no left over
            last_sect_size = norm_sect_size;
        }
    }

    SET_NOT_INTERRUPTABLE();

    // make space for file data
    ret = add_file_data_to_layer2_arr(&dh->l2_file_data_arr, &temp_file_data, NULL);
    if (ret) {
        error_write(er_h, "failed to get space for file data");
        return ret;
    }
    entry->data = temp_file_data;

    if (sections_needed) {
        // make space for sections
        grow_section_array(temp_file_data, sect_num);
        for (i = 0; i < sect_num; i++) {
            ret = add_section_to_layer2_arr(&dh->l2_section_arr, &temp_section, NULL);
            if (ret) {
                error_write(er_h, "failed to get space for section");
                goto fingerprint_done;
            }

            put_into_section_array(temp_file_data, temp_section);
        }

        temp_file_data->norm_sect_size = norm_sect_size;
        temp_file_data->last_sect_size = last_sect_size;
    }

    SET_INTERRUPTABLE();

    // start reading content
    for (i = 0; i < sect_num; i++) {
        if (sections_needed) {
            temp_section = entry->data->section[i];
            if (i < sect_num - 1) {
                temp_section->start_pos = i * norm_sect_size;
                temp_section->end_pos   = temp_section->start_pos + norm_sect_size - 1;
            }
            else {
                temp_section->start_pos = i * norm_sect_size;
                temp_section->end_pos   = temp_section->start_pos + last_sect_size - 1;
            }

            if (i < sect_num - 1) {
                bytes_left = norm_sect_size;
            }
            else {
                bytes_left = last_sect_size;
            }

            if (flags & FPRINT_USE_S_SHA1) {
                SHA1_Init(&s_sha1);
            }
            if (flags & FPRINT_USE_S_SHA256) {
                SHA256_Init(&s_sha256);
            }
            if (flags & FPRINT_USE_S_SHA512) {
                SHA512_Init(&s_sha512);
            }
        }

        while (bytes_left > 0) {
            bytes = fread(data_buf, 1, ffp_min(FILE_BUFFER_SIZE, bytes_left), file);
            if (bytes == 0) {
                break;
            }
            bytes_left -= bytes;

            if (flags & FPRINT_USE_F_SHA1) {
                SHA1_Update(&wf_sha1, data_buf, bytes);
            }
            if (flags & FPRINT_USE_F_SHA256) {
                SHA256_Update(&wf_sha256, data_buf, bytes);
            }
            if (flags & FPRINT_USE_F_SHA512) {
                SHA512_Update(&wf_sha512, data_buf, bytes);
            }

            if (sections_needed) {
                if (flags & FPRINT_USE_S_SHA1) {
                    SHA1_Update(&s_sha1, data_buf, bytes);
                }
                if (flags & FPRINT_USE_S_SHA256) {
                    SHA256_Update(&s_sha256, data_buf, bytes);
                }
                if (flags & FPRINT_USE_S_SHA512) {
                    SHA512_Update(&s_sha512, data_buf, bytes);
                }
            }
        }

        if (sections_needed) {
            // initialise section wise checksums to unused
            for (j = 0; j < CHECKSUM_MAX_NUM; j++) {
                temp_checksum = temp_section->checksum + j;
                temp_checksum->type = CHECKSUM_UNUSED;
            }

            // section wise checksums
            if (flags & FPRINT_USE_S_SHA1) {
                SHA1_Final(sha1_digest, &s_sha1);
                temp_checksum = temp_section->checksum + CHECKSUM_SHA1_INDEX;
                memcpy(temp_checksum->checksum, sha1_digest, SHA_DIGEST_LENGTH);
                temp_checksum->type = CHECKSUM_SHA1_ID;
                temp_checksum->len = SHA_DIGEST_LENGTH;
                bytes_to_hex_str(temp_checksum->checksum_str, temp_checksum->checksum, temp_checksum->len);
            }
            if (flags & FPRINT_USE_S_SHA256) {
                SHA256_Final(sha256_digest, &s_sha256);
                temp_checksum = temp_section->checksum + CHECKSUM_SHA256_INDEX;
                memcpy(temp_checksum->checksum, sha256_digest, SHA256_DIGEST_LENGTH);
                temp_checksum->type = CHECKSUM_SHA256_ID;
                temp_checksum->len = SHA256_DIGEST_LENGTH;
                bytes_to_hex_str(temp_checksum->checksum_str, temp_checksum->checksum, temp_checksum->len);
            }
            if (flags & FPRINT_USE_S_SHA512) {
                SHA512_Final(sha512_digest, &s_sha512);
                temp_checksum = temp_section->checksum + CHECKSUM_SHA512_INDEX;
                memcpy(temp_checksum->checksum, sha512_digest, SHA512_DIGEST_LENGTH);
                temp_checksum->type = CHECKSUM_SHA512_ID;
                temp_checksum->len = SHA512_DIGEST_LENGTH;
                bytes_to_hex_str(temp_checksum->checksum_str, temp_checksum->checksum, temp_checksum->len);
            }

            link_sect_to_checksum_structures(dh, temp_section);
        }

        if (bytes_left != 0 && bytes == 0) {
            fread_failed = 1;

            printf("fingerprint_file : warning, file ended before fingerprinting is finished\n");

            // edit current section as last section
            if (i < sect_num - 1) { // if not already last section
                last_sect_size = norm_sect_size - bytes_left;
            }
            else {  // if already last section
                last_sect_size = last_sect_size - bytes_left;
                if (sect_num == 1) {
                    norm_sect_size = last_sect_size;
                }
            }

            if (sect_num > 1) {     // re-adjust section number if needed
                temp_file_data->section_num = sect_num - (i + 1);
                temp_file_data->section_free_num = sect_num - temp_file_data->section_num;
            }

            // reduce section number
            sect_num = i + 1;

            // re-adjust file size
            file_size = (sect_num - 1) * norm_sect_size + last_sect_size;

            break;
        }
    }

    // initialise whole file checksums to unused
    for (i = 0; i < CHECKSUM_MAX_NUM; i++) {
        temp_checksum = temp_file_data->checksum + i;
        temp_checksum->type = CHECKSUM_UNUSED;
    }


    // whole file checksums
    if (flags & FPRINT_USE_F_SHA1) {
        SHA1_Final(sha1_digest, &wf_sha1);
        temp_checksum = temp_file_data->checksum + CHECKSUM_SHA1_INDEX;
        memcpy(temp_checksum->checksum, sha1_digest, SHA_DIGEST_LENGTH);
        temp_checksum->type = CHECKSUM_SHA1_ID;
        temp_checksum->len = SHA_DIGEST_LENGTH;
        bytes_to_hex_str(temp_checksum->checksum_str, temp_checksum->checksum, temp_checksum->len);
    }
    if (flags & FPRINT_USE_F_SHA256) {
        SHA256_Final(sha256_digest, &wf_sha256);
        temp_checksum = temp_file_data->checksum + CHECKSUM_SHA256_INDEX;
        memcpy(temp_checksum->checksum, sha256_digest, SHA256_DIGEST_LENGTH);
        temp_checksum->type = CHECKSUM_SHA256_ID;
        temp_checksum->len = SHA256_DIGEST_LENGTH;
        bytes_to_hex_str(temp_checksum->checksum_str, temp_checksum->checksum, temp_checksum->len);
    }
    if (flags & FPRINT_USE_F_SHA512) {
        SHA512_Final(sha512_digest, &wf_sha512);
        temp_checksum = temp_file_data->checksum + CHECKSUM_SHA512_INDEX;
        memcpy(temp_checksum->checksum, sha512_digest, SHA512_DIGEST_LENGTH);
        temp_checksum->type = CHECKSUM_SHA512_ID;
        temp_checksum->len = SHA512_DIGEST_LENGTH;
        bytes_to_hex_str(temp_checksum->checksum_str, temp_checksum->checksum, temp_checksum->len);
    }

    SET_NOT_INTERRUPTABLE();

    // fill in file size
    if (flags & FPRINT_USE_F_SIZE) {
        temp_file_data->file_size = file_size;
        sprintf(temp_file_data->file_size_str, "%"PRIu64"", file_size);

        // link to file size related structures
        link_file_data_to_file_size_structures(dh, temp_file_data);
    }

    link_file_data_to_checksum_structures(dh, temp_file_data);

    SET_INTERRUPTABLE();

    // rewind file
    fseek(file, 0, SEEK_SET);

    if (fread_failed) {     // fread failed previously
        // forget about extracts
        temp_file_data->extract_num = 0;

        for (i = 0; i < sect_num; i++) {
            temp_section = temp_file_data->section[i];

            temp_section->extract_num = 0;
        }
    }
    else {
        if (flags & FPRINT_USE_F_EXTR) {
            // whole file extract
            // calculation for extract sampling
            if (file_size < EXTRACT_SIZE_MAX) {   // too small of a file
                extract_num = 1;
                extract_len = file_size;
            }
            else if (file_size < EXTRACT_MAX_NUM * EXTRACT_SIZE_MAX) {
                extract_num = file_size / EXTRACT_SIZE_MAX;     // deliberately truncating
                extract_len = EXTRACT_SIZE_MAX;
            }
            else {      // expected normal length
                extract_num = EXTRACT_MAX_NUM;
                extract_len = EXTRACT_SIZE_MAX;
            }
            if (100 * extract_num * extract_len / file_size > EXTRACT_LEAK_MAX_PERCENT) {
                extract_len = 1;
                extract_num =
                    ffp_min(
                            file_size * EXTRACT_LEAK_MAX_PERCENT / 100,
                            EXTRACT_MAX_NUM
                           );
            }

            temp_file_data->extract_num = extract_num;

            for (i = 0; i < extract_num; i++) {
                extract_pos = i * (file_size / extract_num);

                fseek(file, extract_pos, SEEK_SET);

                bytes = fread(data_buf, 1, extract_len, file);
                if (bytes != extract_len) {
                    fread_failed = 1;

                    printf("fingerprint_fail : warning, file ended before extract sampling is finished\n");
                }

                if (fread_failed) {
                    // discard current extract
                    temp_file_data->extract_num = i;

                    break;
                }

                temp_extract = temp_file_data->extract + i;
                memcpy(temp_extract->extract, data_buf, extract_len);
                temp_extract->len = extract_len;
                temp_extract->position = extract_pos; //i * (file_size / extract_num);
            }

        }
        else {
            temp_file_data->extract_num = 0;
        }

        if (flags & FPRINT_USE_S_EXTR) {
            // handle section wise extracts
            for (i = 0; i < sect_num; i++) {
                temp_section = temp_file_data->section[i];

                if (i < sect_num - 1) {
                    bytes_left = norm_sect_size;
                }
                else {
                    bytes_left = last_sect_size;
                }
                if (bytes_left < EXTRACT_SIZE_MAX) {   // too small of a section
                    extract_num = 1;
                    extract_len = bytes_left;
                }
                else if (bytes_left < EXTRACT_MAX_NUM * EXTRACT_SIZE_MAX) {
                    extract_num = bytes_left / EXTRACT_SIZE_MAX;
                    extract_len = EXTRACT_SIZE_MAX;
                }
                else {      // expected normal length
                    extract_num = EXTRACT_MAX_NUM;
                    extract_len = EXTRACT_SIZE_MAX;
                }
                if (100 * extract_num * extract_len / (temp_section->end_pos - temp_section->start_pos + 1) > EXTRACT_LEAK_MAX_PERCENT) {
                    extract_len = 1;
                    extract_num =
                        ffp_min(
                                (temp_section->end_pos - temp_section->start_pos + 1) * EXTRACT_LEAK_MAX_PERCENT / 100,
                                EXTRACT_MAX_NUM
                               );
                }

                temp_section->extract_num = extract_num;

                for (j = 0; j < extract_num; j++) {
                    extract_pos = i * norm_sect_size + j * (bytes_left / extract_num);

                    fseek(file, extract_pos, SEEK_SET);

                    bytes = fread(data_buf, 1, extract_len, file);
                    if (bytes != extract_len) {
                        fread_failed = 1;

                        printf("fingerprint_file : warning, file ended before extract sampling is finished\n");
                    }

                    if (fread_failed) {
                        // discard current extract
                        temp_section->extract_num = j;

                        break;
                    }

                    temp_extract = temp_section->extract + j;
                    memcpy(temp_extract->extract, data_buf, extract_len);
                    temp_extract->len = extract_len;
                    temp_extract->position = extract_pos;
                }

                if (fread_failed) {
                    // set remaining section's extract num to 0
                    for (j = i + 1; j < sect_num; j++) {
                        temp_section = temp_file_data->section[j];

                        temp_section->extract_num = 0;
                    }

                    break;
                }
            }
        }
    }

    SET_NOT_INTERRUPTABLE();

fingerprint_done:

    if (file) {
        fclose(file);
    }

    ret2 = verify_entry(dh, entry, 0x0);
    if (ret2) {
        printf("fingerprint_file : verify entry detected errors\n");
        printf("file name : %s\n", path);
        printf("Please report to developer as this should not happen\n");
        printf("It is recommended that you revert to previous version of database as this may indicate your database is now corrupted\n");
        printf("Sorry for the inconvenience\n");
        ret = ret2;
    }

    // clear pointer so cleanup function will not clean it again
    *file_being_used = NULL;

    MARK_DB_UNSAVED(dh);

    SET_INTERRUPTABLE();

    return ret;
}
