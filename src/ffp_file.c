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

#include "ffp_file.h"

#pragma GCC diagnostic ignored "-Wunused-function"

#define IS_STR      1       // sequential read without taking endianness into account
#define NOT_STR     0       // do endian conversion

#define BUF_READ        1       // buffer for reading
#define BUF_WRITE       2       // buffer for writing

#define ret_close_file(ret_val, fp) \
    fclose(fp); return ret_val;

struct buffer_info {
    FILE* fp;
    long int file_size;
    long int file_pos;
    unsigned char* buf;
    unsigned char* buf_backup;
    uint16_t buffer_pos;
    uint16_t buffer_size;
    unsigned char buffer_eol;   // end of life
    uint16_t buffer_alt_size;
};

static int init_buffer_info(struct buffer_info* info, unsigned char* buffer, unsigned char* buffer_backup, uint16_t size, unsigned char mode) {
    if (        mode != BUF_READ
            &&  mode != BUF_WRITE
       )
    {
        return WRONG_ARGS;
    }

    info->fp = 0;
    info->file_size = 0;
    info->file_pos = 0;
    info->buf = buffer;
    info->buf_backup = buffer_backup;
    info->buffer_size = size;

    if (mode == BUF_READ) {
        info->buffer_pos = size;
    }
    else if (mode == BUF_WRITE) {
        info->buffer_pos = 0;
    }

    info->buffer_eol = 0;
    info->buffer_alt_size = 0;

    return 0;
}

static int flush_buf (struct buffer_info* info);

static void memcpy_forward(void* in_dst, void* in_src, uint32_t size) {
    unsigned char* dst = (unsigned char*) in_dst;
    unsigned char* src = (unsigned char*) in_src;
    unsigned char* dst_end = dst + size;

    debug_printf("memcpy_forward : ");

    while (dst < dst_end) {
        *dst = *src;
        debug_printf("%02X ", *dst);
        dst++;
        src++;
    }

    debug_printf("\n");
}

static void memcpy_reverse(void* in_dst, void* in_src, uint32_t size) {
    unsigned char* dst = (unsigned char*) in_dst;
    unsigned char* src = (unsigned char*) in_src;
    unsigned char* dst_end = dst;

    dst += size-1;

    debug_printf("memcpy_reverse : ");

    while (dst >= dst_end) {
        *dst = *src;
        debug_printf("%02X ", *dst);
        dst--;
        src++;
    }

    debug_printf("\n");
}

/* Database file manipulation */

// endianess is handled in copy_buf_to_ptr internally
// the database file is always stored in big endian
static int copy_buf_to_ptr (struct buffer_info* info, void* in_ptr, uint32_t size, unsigned char is_string) {
    unsigned char* ptr = (unsigned char*) in_ptr;
    unsigned int space;         // space filled
    size_t bytes_read;
    //unsigned int temp = 0;

    if (size > info->buffer_size) {
        printf("copy_buf_to_ptr : buffer too small\n");
        return BUFFER_NO_SPACE;
    }

    if (!info->buffer_eol) {
        space = info->buffer_size - info->buffer_pos;
    }
    else {
        space = info->buffer_alt_size - info->buffer_pos;
    }

    if (space >= size) {
        if (is_string) {
            memcpy_forward(ptr, (info->buf + info->buffer_pos), size);
        }
        else {
            if (O32_HOST_ORDER == O32_LITTLE_ENDIAN) {
                memcpy_reverse(ptr, (info->buf + info->buffer_pos), size);
            }
            else if (O32_HOST_ORDER == O32_BIG_ENDIAN) {
                memcpy_forward(ptr, (info->buf + info->buffer_pos), size);
            }
            else {
                printf("copy_buf_to_ptr : endianness not supported\n");
                return ENDIAN_NO_SUPPORT;
            }
        }
        info->buffer_pos += size;
        space -= size;
        return 0;
    }
    else if (space < size) {
        if (info->buffer_eol) {
            printf("copy_buf_to_ptr : file ended before finishing read\n");
            return FILE_END;
        }

        if (is_string) {
            memcpy_forward(ptr, (info->buf + info->buffer_pos), space);
        }
        else {
            if (O32_HOST_ORDER == O32_LITTLE_ENDIAN) {
                memcpy_forward(info->buf_backup, (info->buf + info->buffer_pos), space);
            }
            else if (O32_HOST_ORDER == O32_BIG_ENDIAN) {
                memcpy_forward(ptr, (info->buf + info->buffer_pos), space);
            }
            else {
                printf("copy_buf_to_ptr : endianness not supported\n");
                return ENDIAN_NO_SUPPORT;
            }
        }
        info->buffer_pos += space;

        // load more things into buffer
        bytes_read = fread(info->buf, 1, info->buffer_size, info->fp);
        info->file_pos += bytes_read;
        info->buffer_pos = 0;
        if (bytes_read < info->buffer_size) {
            if (info->file_pos != info->file_size) {
                // error encountered
                printf("copy_buf_to_ptr : fread error\n");
                return FREAD_ERROR;
            }
            info->buffer_eol = 1;
            info->buffer_alt_size = bytes_read;
            if (size-space > info->buffer_alt_size) {
                printf("copy_buf_to_ptr : file ended before finishing read\n");
                return FILE_END_TOO_SOON;
            }
        }

        // now finish copying the remaining bit
        if (is_string) {
            memcpy_forward((ptr+space), (info->buf + info->buffer_pos), size-space);
        }
        else {
            if (O32_HOST_ORDER == O32_LITTLE_ENDIAN) {
                memcpy_forward((info->buf_backup + space), (info->buf + info->buffer_pos), size-space);
                memcpy_reverse(ptr, info->buf_backup, size);
            }
            else if (O32_HOST_ORDER == O32_BIG_ENDIAN) {
                memcpy_forward((ptr+space), (info->buf + info->buffer_pos), size-space);
            }
            else {
                printf("copy_buf_to_ptr : endianness not supported\n");
                return ENDIAN_NO_SUPPORT;
            }
        }
        info->buffer_pos += size-space;
    }

    return 0;
}

static int copy_ptr_to_buf (struct buffer_info* info, void* in_ptr, uint32_t size, unsigned char is_string) {
    unsigned char* ptr = (unsigned char*) in_ptr;
    unsigned int space = info->buffer_size - info->buffer_pos;  // empty space

    if (size > info->buffer_size) {
        printf("copy_ptr_to_buf : buffer too small\n");
        return BUFFER_NO_SPACE;
    }

    if (space < size) {
        flush_buf(info);
    }

    if (is_string) {
        memcpy_forward((info->buf + info->buffer_pos), ptr, size);
    }
    else {
        if (O32_HOST_ORDER == O32_LITTLE_ENDIAN) {
            memcpy_reverse((info->buf + info->buffer_pos), ptr, size);
        }
        else if (O32_HOST_ORDER == O32_BIG_ENDIAN) {
            memcpy_forward((info->buf + info->buffer_pos), ptr, size);
        }
        else {
            printf("copy_buf_to_ptr : endianness not supported\n");
            return ENDIAN_NO_SUPPORT;
        }
    }
    info->buffer_pos += size;

    return 0;
}

static int flush_buf (struct buffer_info* info) {
    size_t bytes_written;

    if (info->buffer_pos > 0) {
        bytes_written = fwrite(info->buf, 1, info->buffer_pos, info->fp);
        if (bytes_written < info->buffer_pos) {
            printf("flush_buf : fwrite error\n");
            return FWRITE_ERROR;
        }

        // file_pos should not run past file_size in ffprinter code
        if (info->file_pos == info->file_size) {
            info->file_size += info->buffer_pos;
        }

        info->file_pos += info->buffer_pos;
        info->buffer_pos = 0;
    }

    return 0;
}

static int buf_seek(struct buffer_info* info, long int offset, int whence) {
    flush_buf(info);

    switch (whence) {
        case SEEK_SET :
            fseek(info->fp, offset, SEEK_SET);
            info->file_pos = offset;
            break;
        case SEEK_CUR :
            fseek(info->fp, offset, SEEK_CUR);
            info->file_pos += offset;
            break;
        case SEEK_END :
            fseek(info->fp, offset, SEEK_END);
            info->file_pos = info->file_size-1 - offset;
            break;
        default :
            return WRONG_ARGS;
    }

    return 0;
}

int load_file (database_handle* dh, char* file_name) {
    FILE* data_file;

    unsigned char buffer[DFILE_BUFFER_SIZE];
    unsigned char buffer_secondary[DFILE_BUFFER_SIZE];
    unsigned char head_tail[15];

    char eid_str_buf[EID_STR_MAX + 1];

    char version_buf[6];

    struct buffer_info info;

    struct stat file_stat;

    int i, j;  // used for counting only

#ifdef FFP_DEBUG
    int k;
#endif

    ffp_eid_int a;

    uint16_t temp_checksum_num;

    uint16_t temp_checksum_type;

    unsigned char* tmp;     // used for pointing to location to fill data in from file

    unsigned char temp_parent_entry_id[EID_LEN];

    ffp_eid_int temp_entry_num;

    checksum_result* temp_checksum_result;

    int ret = 0;

    linked_entry* temp_entry;
    linked_entry* temp_parent_entry;

    int64_t temp_time_int64;
    int8_t temp_time_int8;
    uint8_t temp_time_uint8;
    uint16_t temp_time_uint16;

    uint64_t field_presence_bitmap;

    uint64_t temp_sect_num;
    uint64_t temp_child_num;

    linked_entry* temp_entry_find;

    file_data* temp_file_data;

    section* temp_section;

    misc_alloc_record*  temp_misc_alloc_record;

    str_len_int dbase_file_name_len;

    // parameters check

    init_buffer_info(&info, buffer, buffer_secondary, DFILE_BUFFER_SIZE, BUF_READ);

    // do file existance, name checking etc

    if ((i = verify_str_terminated(file_name, FILE_NAME_MAX, &dbase_file_name_len, ALLOW_ZERO_LEN_STR))) {
        printf("load_file : database file name not null terminated\n");
        return i;
    }
    if (dbase_file_name_len == 0) {
        printf("load_file : database file name is empty\n");
        return WRONG_ARGS;
    }

    data_file = fopen(file_name, "rb");

    if (!data_file) {
        printf("load_file : unable to open file\n");
        perror("load_file : error");
        return FOPEN_FAIL;
    }

    info.fp = data_file;
    stat(file_name, &file_stat);
    info.file_size = file_stat.st_size;
    info.file_pos = 0; 

    printf("loading database general info\n");

    debug_printf("checking preamble\n");

    // check preamble
    tmp = head_tail;
    ret = copy_buf_to_ptr(&info, tmp, 15, IS_STR);
    if (ret) {
        ret_close_file(ret, data_file);
    }
    for (i = 1; i <= 15; i++) {
        if (head_tail[i-1] != i) {
            printf("load_file : preamble is corrupted\n");
            ret_close_file(FILE_BROKEN, data_file);
        }
    }

    //** deal with database info and stats **//

    debug_printf("grabbing database version\n");

    // grab database version
    tmp = (unsigned char*) version_buf;
    ret = copy_buf_to_ptr(&info, tmp, 5, IS_STR);
    if (ret) {
        ret_close_file(ret, data_file);
    }
    version_buf[5] = 0;

    debug_printf("database version : <%s>\n", version_buf);

    // check if database version is comptatible with software version
    // only one version of database right now
    if (strcmp(version_buf, "00.01") != 0) {
        printf("load_file : unsupported databse version\n");
        ret_close_file(FILE_NOSUPPORT, data_file);
    }

    debug_printf("grabbing number of branches\n");

    // get number of branches
    tmp = (unsigned char*) &temp_child_num;
    ret = copy_buf_to_ptr(&info, tmp, sizeof_member(linked_entry, child_num), NOT_STR);
    if (ret) {
        ret_close_file(ret, data_file);
    }

    debug_printf("number of branches : %"PRIu64"\n", temp_child_num);

    // grow dh->tree children array
    ret = grow_children_array(&dh->tree, temp_child_num);
    if (ret) {
        ret_close_file(ret, data_file);
    }

    record_misc_alloc(&dh->l2_misc_alloc_record_arr, temp_misc_alloc_record, ret, dh->tree.child);

    debug_printf("grabbing number of entries\n");

    // grab total number of entries
    tmp = (unsigned char*) &temp_entry_num;
    ret = copy_buf_to_ptr(&info, tmp, sizeof(ffp_eid_int), 0);
    if (ret) {
        ret_close_file(ret, data_file);
    }

    debug_printf("number of entries : %"PRIu64"\n", temp_entry_num);

    //dh->total_entry_num = temp_entry_num;

    /*if (temp_entry_num == 0) {
        printf("load_file : database is empty");
        ret_close_file(0, data_file);
    }*/

    //** start to deal with entries **//
    if (temp_entry_num == 0) {
        printf("database is empty");
    }

    for (a = 0; a < temp_entry_num; a++) {
        // the percentage calculation obviously blows up if a is too close
        // to the upper limit of ffp_eid_int(uint64_t as of time of writing)
        // but not dangerous as it wraps around safely(assuming ffp_eid_int is using unsigned integer)
        // and only make the output look wonky
        printf("\rloading entries : %"PRIu64" / %"PRIu64" %"PRIu64"%%", a + 1, temp_entry_num, (100 * (a + 1) + temp_entry_num / 2) / temp_entry_num);
        fflush(stdout);

        debug_printf("dealing with entry #%"PRIu64"\n", a);

        // make space for a new entry
        ret = add_entry_to_layer2_arr(&dh->l2_entry_arr, &temp_entry, NULL);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        debug_printf("grabbing branch id\n");

        // fill in the branch_id
        tmp = (unsigned char*) &temp_entry->branch_id;
        ret = copy_buf_to_ptr(&info, tmp, EID_LEN, IS_STR);
        if (ret) {  // error occured
            ret_close_file(ret, data_file);
        }

        bytes_to_hex_str(temp_entry->branch_id_str, temp_entry->branch_id, EID_LEN);
        debug_printf("branch id : %s\n", temp_entry->branch_id_str);

        debug_printf("grabbing entry id\n");

        // fill in the entry_id
        tmp = (unsigned char*) &temp_entry->entry_id;
        ret = copy_buf_to_ptr(&info, tmp, EID_LEN, IS_STR);
        if (ret) {  // error occured
            ret_close_file(ret, data_file);
        }

        bytes_to_hex_str(temp_entry->entry_id_str, temp_entry->entry_id, EID_LEN);
        debug_printf("entry id : %s\n", temp_entry->entry_id_str);

        for (i = 0; i < EID_LEN; i++) {     // check if all 0
            if (temp_entry->entry_id[i] != 0) {
                break;
            }
        }
        if (i == EID_LEN) {
            printf("load_file : invalid entry id\n");
            ret_close_file(FILE_BROKEN, data_file);
        }

        debug_printf("grabbing parent entry id\n");

        // fill in the entry_id of parent
        tmp = (unsigned char*) &temp_parent_entry_id;
        ret = copy_buf_to_ptr(&info, tmp, EID_LEN, IS_STR);
        if (ret) {  // error occured
            ret_close_file(ret, data_file);
        }

        bytes_to_hex_str(eid_str_buf, temp_parent_entry_id, EID_LEN);
        debug_printf("parent entry id : %s\n", eid_str_buf);

        // find parent or give up
        for (i = 0; i < EID_LEN; i++) {     // check if all 0
            if (temp_parent_entry_id[i] != 0) {
                break;
            }
        }
        if (i == EID_LEN) {   // all zero, no parent, add to head of dh->tree
            put_into_children_array(&dh->tree, temp_entry);
            temp_entry->has_parent = 0;
        }
        else { // has parent apparently, attempt to find and link to it
            lookup_entry_id_via_dh(dh, eid_str_buf, &temp_parent_entry);
            if (!temp_parent_entry) {
                printf("load_file : cannot find parent of entry\n");
                ret_close_file(FIND_FAIL, data_file);
            }
            put_into_children_array(temp_parent_entry, temp_entry);
            temp_entry->has_parent = 1;
        }

        debug_printf("grabbing entry type\n");

        // fill in type of entry
        tmp = (unsigned char*) &temp_entry->type;
        ret = copy_buf_to_ptr(&info, tmp, sizeof_member(linked_entry, type), NOT_STR);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        debug_printf("entry type : %u\n", temp_entry->type);

        if (    temp_entry->type != ENTRY_OTHER
            &&  temp_entry->type != ENTRY_FILE
            &&  temp_entry->type != ENTRY_GROUP
           )
        {
            printf("load_file : invalid entry type\n");
            ret_close_file(FILE_BROKEN, data_file);
        }

        debug_printf("grabbing children number\n");

        // fill in the child_num
        tmp = (unsigned char*) &temp_child_num;
        ret = copy_buf_to_ptr(&info, tmp, sizeof_member(linked_entry, child_num), NOT_STR);
        if (ret) {  // error occured
            ret_close_file(ret, data_file);
        }

        debug_printf("children number : %"PRIu64"\n", temp_child_num);

        if (temp_child_num > 0) {
            ret = grow_children_array(temp_entry, temp_child_num);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            record_misc_alloc(&dh->l2_misc_alloc_record_arr, temp_misc_alloc_record, ret, temp_entry->child);
        }

        debug_printf("grabbing created by flag\n");

        tmp = (unsigned char*) &temp_entry->created_by;
        ret = copy_buf_to_ptr(&info, tmp, sizeof_member(linked_entry, created_by), NOT_STR);
        if (ret) {  // error occured
            ret_close_file(ret, data_file);
        }

        debug_printf("created by flag : %x\n", (unsigned int) temp_entry->created_by);

        if (        temp_entry->created_by != CREATED_BY_SYS
                &&  temp_entry->created_by != CREATED_BY_USR
           )
        {
            printf("load_file : invalid created by flag\n");
            ret_close_file(FILE_BROKEN, data_file);
        }

        debug_printf("grabbing file name length\n");

        // get file name length
        tmp = (unsigned char*) &temp_entry->file_name_len;
        ret = copy_buf_to_ptr(&info, tmp, sizeof_member(linked_entry, file_name_len), NOT_STR);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        debug_printf("file name length : %u\n", temp_entry->file_name_len);

        if (temp_entry->file_name_len > FILE_NAME_MAX) {
            printf("load_file : file name length exceeds maximum length\n");
            ret_close_file(FILE_BROKEN, data_file);
        }

        debug_printf("grabbing file name\n");

        // fill in file name
        tmp = (unsigned char*) temp_entry->file_name;
        ret = copy_buf_to_ptr(&info, tmp, temp_entry->file_name_len, IS_STR);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        // add null character
        temp_entry->file_name[temp_entry->file_name_len] = 0;

        debug_printf("file name : %s\n", temp_entry->file_name);

        debug_printf("adding to file name translation structures\n");

        // add entry to the filename translation hashtable
        // or chain entry to other entries under same filename
        link_entry_to_file_name_structures(dh, temp_entry);

        debug_printf("grabbing field presence indicator bitmap\n");

        // read field presence indicator bitmap
        tmp = (unsigned char*) &field_presence_bitmap;
        ret = copy_buf_to_ptr(&info, tmp, sizeof(uint64_t), NOT_STR);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        if (field_presence_bitmap & HAS_TAG_STR) {
            debug_printf("grabbing tag string length\n");

            // fill in the tag_str_len
            tmp = (unsigned char*) &temp_entry->tag_str_len;
            ret = copy_buf_to_ptr(&info, tmp, sizeof_member(linked_entry, tag_str_len), NOT_STR);
            if (ret) {  // error occured
                ret_close_file(ret, data_file);
            }

            debug_printf("tag string length : %u\n", temp_entry->tag_str_len);

            if (temp_entry->tag_str_len > TAG_STR_MAX) {
                printf("load_file : tag string is too long\n");
                ret_close_file(FILE_BROKEN, data_file);
            }

            debug_printf("grabbing tag string\n");

            // fill in tag string
            tmp = (unsigned char*) temp_entry->tag_str;
            ret = copy_buf_to_ptr(&info, tmp, temp_entry->tag_str_len, 1);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // add null character
            temp_entry->tag_str[temp_entry->tag_str_len] = 0;

            debug_printf("tag string : %s\n", temp_entry->tag_str);

            if (        temp_entry->tag_str_len > 0
                    && 
                    (           temp_entry->tag_str[0]                          !=      '|'
                            ||  temp_entry->tag_str[temp_entry->tag_str_len-1]  !=      '|'
                    )
               )
            {
                printf("load_file : tag string does not have proper format\n");
                ret_close_file(FILE_BROKEN, data_file);
            }

            debug_printf("adding to tag string translation structures\n");

            // add entry to the tag string translation hashtable
            // or chain entry to other entries under same tag string
            link_entry_to_tag_str_structures(dh, temp_entry);
        }

        if (field_presence_bitmap & HAS_USR_MSG) {
            debug_printf("grabbing user message length\n");

            // get user message length
            tmp = (unsigned char*) &temp_entry->user_msg_len;
            ret = copy_buf_to_ptr(&info, tmp, sizeof_member(linked_entry, user_msg_len), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            debug_printf("user message length : %u\n", temp_entry->user_msg_len);

            if (temp_entry->user_msg_len > USER_MSG_MAX) {
                printf("load_file : user message exceeds maximum length\n");
                ret_close_file(FILE_BROKEN, data_file);
            }

            debug_printf("grabbing user message\n");

            // fill in user message
            tmp = (unsigned char*) temp_entry->user_msg;
            ret = copy_buf_to_ptr(&info, tmp, temp_entry->user_msg_len, IS_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            temp_entry->user_msg[temp_entry->user_msg_len] = 0;

            debug_printf("user message : %s\n", temp_entry->user_msg);
        }

        if (field_presence_bitmap & HAS_ADD_TIME) {
            debug_printf("dealing with time of addition\n");

            debug_printf("grabbing time of addition, seconds\n");

            // fill in time of addition of entry, using int64_t for all ints in file
            // seconds
            tmp = (unsigned char*) &temp_time_uint8;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tod_utc.tm_sec = temp_time_uint8;

            debug_printf("time of addition, seconds : %d\n", temp_entry->tod_utc.tm_sec);

            debug_printf("grabbing time of addition, minutes\n");

            // minutes
            tmp = (unsigned char*) &temp_time_uint8;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tod_utc.tm_min = temp_time_uint8;

            debug_printf("time of addition, minutes : %d\n", temp_entry->tod_utc.tm_min);

            debug_printf("grabbing time of addition, hours\n");

            // hours
            tmp = (unsigned char*) &temp_time_uint8;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tod_utc.tm_hour = temp_time_uint8;

            debug_printf("time of addition, hours : %d\n", temp_entry->tod_utc.tm_hour);

            debug_printf("grabbing time of addition, day of month\n");

            // day of month
            tmp = (unsigned char*) &temp_time_uint8;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tod_utc.tm_mday = temp_time_uint8;

            debug_printf("time of addition, day of month : %d\n", temp_entry->tod_utc.tm_mday);

            debug_printf("grabbing time of addition, month\n");

            // month
            tmp = (unsigned char*) &temp_time_uint8;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tod_utc.tm_mon = temp_time_uint8;

            debug_printf("time of addition, month : %d\n", temp_entry->tod_utc.tm_mon);

            debug_printf("grabbing time of addition, year\n");

            // year
            tmp = (unsigned char*) &temp_time_int64;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(int64_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            if (temp_time_int64 < INT_MIN || temp_time_int64 > INT_MAX) {
                printf("load_file : invalid year in time of addition\n");
                ret_close_file(FILE_BROKEN, data_file);
            }
            temp_entry->tod_utc.tm_year = temp_time_int64;

            debug_printf("time of addition, year : %d\n", temp_entry->tod_utc.tm_year);

            debug_printf("grabbing time of addition, week day\n");

            // week day
            tmp = (unsigned char*) &temp_time_uint8;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tod_utc.tm_wday = temp_time_uint8;

            debug_printf("time of addition, week day : %d\n", temp_entry->tod_utc.tm_wday);

            debug_printf("grabbing time of addition, year day\n");

            // year day
            tmp = (unsigned char*) &temp_time_uint16;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(uint16_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tod_utc.tm_yday = temp_time_uint16;

            debug_printf("time of addition, year day : %d\n", temp_entry->tod_utc.tm_yday);

            debug_printf("grabbing daylight saving flag\n");

            // daylight saving flag
            tmp = (unsigned char*) &temp_time_int8;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(int8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tod_utc.tm_isdst = temp_time_int8;

            debug_printf("time of addition, daylight saving flag : %d\n", temp_entry->tod_utc.tm_isdst);

            debug_printf("add entry to date time dh->tree\n");

            link_entry_to_date_time_tree(dh, temp_entry, DATE_TOD);
        }

        if (field_presence_bitmap & HAS_MOD_TIME) {
            debug_printf("dealing with time of modification\n");

            debug_printf("grabbing time of modification, seconds\n");

            // fill in time of modification of entry, using int64_t for all ints in file
            // seconds
            tmp = (unsigned char*) &temp_time_uint8;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tom_utc.tm_sec = temp_time_uint8;

            debug_printf("time of modification, seconds : %d\n", temp_entry->tom_utc.tm_sec);

            debug_printf("grabbing time of modification, minutes\n");

            // minutes
            tmp = (unsigned char*) &temp_time_uint8;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tom_utc.tm_min = temp_time_uint8;

            debug_printf("time of modification, minutes : %d\n", temp_entry->tom_utc.tm_min);

            debug_printf("grabbing time of modification, hours\n");

            // hours
            tmp = (unsigned char*) &temp_time_uint8;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tom_utc.tm_hour = temp_time_uint8;

            debug_printf("time of modification, hours : %d\n", temp_entry->tom_utc.tm_hour);

            debug_printf("grabbing time of modification, day of month\n");

            // day of month
            tmp = (unsigned char*) &temp_time_uint8;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tom_utc.tm_mday = temp_time_uint8;

            debug_printf("time of modification, day of month : %d\n", temp_entry->tom_utc.tm_mday);

            debug_printf("grabbing time of modification, month\n");

            // month
            tmp = (unsigned char*) &temp_time_uint8;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tom_utc.tm_mon = temp_time_uint8;

            debug_printf("time of modification, month : %d\n", temp_entry->tom_utc.tm_mon);

            debug_printf("grabbing time of modification, year\n");

            // year
            tmp = (unsigned char*) &temp_time_int64;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(int64_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            if (temp_time_int64 < INT_MIN || temp_time_int64 > INT_MAX) {
                printf("load_file : invalid year in time of modification\n");
                ret_close_file(FILE_BROKEN, data_file);
            }
            temp_entry->tom_utc.tm_year = temp_time_int64;

            debug_printf("time of modification, year : %d\n", temp_entry->tom_utc.tm_year);

            debug_printf("grabbing time of modification, week day\n");

            // week day
            tmp = (unsigned char*) &temp_time_uint8;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tom_utc.tm_wday = temp_time_uint8;

            debug_printf("time of modification, week day : %d\n", temp_entry->tom_utc.tm_wday);

            debug_printf("grabbing time of modification, year day\n");

            // year day
            tmp = (unsigned char*) &temp_time_uint16;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(uint16_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tom_utc.tm_yday = temp_time_uint16;

            debug_printf("time of modification, year day : %d\n", temp_entry->tom_utc.tm_yday);

            debug_printf("grabbing daylight saving flag\n");

            // daylight saving flag
            tmp = (unsigned char*) &temp_time_int8;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(int8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tom_utc.tm_isdst = temp_time_int8;

            debug_printf("time of modification, daylight saving flag : %d\n", temp_entry->tom_utc.tm_isdst);

            debug_printf("add entry to date time dh->tree\n");

            link_entry_to_date_time_tree(dh, temp_entry, DATE_TOM);
        }

        if (field_presence_bitmap & HAS_USR_TIME) {
            debug_printf("dealing with user specified time\n");

            debug_printf("grabbing user specified time, seconds\n");

            // fill in user specified time of entry, using int64_t for all ints in file
            // seconds
            tmp = (unsigned char*) &temp_time_uint8;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tusr_utc.tm_sec = temp_time_uint8;

            debug_printf("user specified time, seconds : %d\n", temp_entry->tusr_utc.tm_sec);

            debug_printf("grabbing user specified time, minutes\n");

            // minutes
            tmp = (unsigned char*) &temp_time_uint8;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tusr_utc.tm_min = temp_time_uint8;

            debug_printf("user specified time, minutes : %d\n", temp_entry->tusr_utc.tm_min);

            debug_printf("grabbing user specified time, hours\n");

            // hours
            tmp = (unsigned char*) &temp_time_uint8;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tusr_utc.tm_hour = temp_time_uint8;

            debug_printf("user specified time, hours : %d\n", temp_entry->tusr_utc.tm_hour);

            debug_printf("grabbing user specified time, day of month\n");

            // day of month
            tmp = (unsigned char*) &temp_time_uint8;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tusr_utc.tm_mday = temp_time_uint8;

            debug_printf("user specified time, day of month : %d\n", temp_entry->tusr_utc.tm_mday);

            debug_printf("grabbing user specified time, month\n");

            // month
            tmp = (unsigned char*) &temp_time_uint8;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tusr_utc.tm_mon = temp_time_uint8;

            debug_printf("user specified time, month : %d\n", temp_entry->tusr_utc.tm_mon);

            debug_printf("grabbing user specified time, year\n");

            // year
            tmp = (unsigned char*) &temp_time_int64;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(int64_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tusr_utc.tm_year = temp_time_int64;

            debug_printf("user specified time, year : %d\n", temp_entry->tusr_utc.tm_year);

            debug_printf("grabbing user specified time, week day\n");

            // week day
            tmp = (unsigned char*) &temp_time_uint8;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tusr_utc.tm_wday = temp_time_uint8;

            debug_printf("user specified time, week day : %d\n", temp_entry->tusr_utc.tm_wday);

            debug_printf("grabbing user specified time, year day\n");

            // year day
            tmp = (unsigned char*) &temp_time_uint16;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(uint16_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tusr_utc.tm_yday = temp_time_uint16;

            debug_printf("user specified time, year day : %d\n", temp_entry->tusr_utc.tm_yday);

            debug_printf("grabbing daylight saving flag\n");

            // daylight saving flag
            tmp = (unsigned char*) &temp_time_int8;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(int8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
            temp_entry->tusr_utc.tm_isdst = temp_time_int8;

            debug_printf("user specified time, daylight saving flag : %d\n", temp_entry->tusr_utc.tm_isdst);

            debug_printf("add entry to date time dh->tree\n");

            debug_printf("add entry to date time dh->tree\n");

            link_entry_to_date_time_tree(dh, temp_entry, DATE_TUSR);
        }

        if (field_presence_bitmap & HAS_FILE_DATA) {
            debug_printf("file data is present\n");
        }
        else {
            debug_printf("no file data is present\n");
            goto skip_file;
        }

        // allocate space for file data
        add_file_data_to_layer2_arr(&dh->l2_file_data_arr, &temp_file_data, NULL);
        temp_entry->data = temp_file_data;

        debug_printf("dealing with file data\n");

        //** start handling file data **//

        debug_printf("grabbing file size\n");

        // fill in file size
        tmp = (unsigned char*) &temp_file_data->file_size;
        ret = copy_buf_to_ptr(&info, tmp, sizeof_member(file_data, file_size), 0);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        debug_printf("file size : %"PRIu64"\n", temp_file_data->file_size);

        if (temp_file_data->file_size > FILE_SIZE_MAX) {
            printf("file size exceeds maximum length\n");
            ret_close_file(FILE_BROKEN, data_file);
        }

        debug_printf("adding to file size translation structures\n");

        // add entry to the file size translation hashtable
        // or chain entry to other entries under same filename
        link_file_data_to_file_size_structures(dh, temp_file_data);

        debug_printf("grabbing number of checksum\n");

        // fill in number of checksums
        tmp = (unsigned char*) &temp_checksum_num;
        ret = copy_buf_to_ptr(&info, tmp, sizeof(temp_checksum_num), 0);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        debug_printf("number of checksums : %u\n", temp_checksum_num);

        if (temp_checksum_num > CHECKSUM_MAX_NUM) {
            printf("load_file : number of checksums exceeds maximum\n");
            ret_close_file(FILE_BROKEN, data_file);
        }

        debug_printf("dealing with checksums\n");

        // fill in checksum results
        for (i = 0; i < temp_checksum_num; i++) {
            debug_printf("checksum #%d\n", i);

            debug_printf("grabbing type of checksum\n");

            // fill in type of checksum
            tmp = (unsigned char*) &temp_checksum_type;
            ret = copy_buf_to_ptr(&info, tmp, sizeof_member(checksum_result, type), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            switch (temp_checksum_type) {
                case CHECKSUM_SHA1_ID :
                    temp_checksum_result = temp_file_data->checksum + CHECKSUM_SHA1_INDEX;
                    break;
                case CHECKSUM_SHA256_ID :
                    temp_checksum_result = temp_file_data->checksum + CHECKSUM_SHA256_INDEX;
                    break;
                case CHECKSUM_SHA512_ID :
                    temp_checksum_result = temp_file_data->checksum + CHECKSUM_SHA512_INDEX;
                    break;
                default :
                    printf("load_file : invalid checksum type\n");
                    ret_close_file(FILE_BROKEN, data_file);
            }

            temp_checksum_result->type = temp_checksum_type;

            debug_printf("type of checksum : %u\n", temp_checksum_result->type);

            debug_printf("grabbing length of checksum\n");

            // fill in length of checksum
            tmp = (unsigned char*) &temp_checksum_result->len;
            ret = copy_buf_to_ptr(&info, tmp, sizeof_member(checksum_result, len), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            debug_printf("length of checksum : %u\n", temp_checksum_result->len);

            if (temp_checksum_result->len > CHECKSUM_MAX_LEN) {
                printf("load_file : length of checksum exceeds maximum length\n");
                ret_close_file(FILE_BROKEN, data_file);
            }

            debug_printf("grabbing checksum result\n");

            // fill in checksum result
            tmp = (unsigned char*) temp_checksum_result->checksum;
            ret = copy_buf_to_ptr(&info, tmp, temp_checksum_result->len, IS_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            debug_printf("checksum result : ");
#ifdef FFP_DEBUG
            for (j = 0; j < temp_checksum_result->len; j++) {
                printf("%02X ", temp_checksum_result->checksum[j]);
            }
            debug_printf("\n");
#endif

            debug_printf("converting checksum to string\n");

            // add file checksum to hashtable
            // convert checksum to string
            bytes_to_hex_str(temp_checksum_result->checksum_str, temp_checksum_result->checksum, temp_checksum_result->len);

            debug_printf("checksum string : %s\n", temp_checksum_result->checksum_str);

            debug_printf("adding to checksum translation structures\n");
        }

        ret = link_file_data_to_checksum_structures(dh, temp_file_data);
        if (ret) {
            ret_close_file(FILE_BROKEN, data_file);
        }

        debug_printf("grabbing number of extracts\n");

        // fill in number of extracts
        tmp = (unsigned char*) &temp_file_data->extract_num;
        ret = copy_buf_to_ptr(&info, tmp, sizeof_member(file_data, extract_num), NOT_STR);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        debug_printf("number of extracts : %u\n", temp_file_data->extract_num);

        if (temp_file_data->extract_num > EXTRACT_MAX_NUM) {
            printf("load_file : number of extracts exceeds maximum\n");
            ret_close_file(FILE_BROKEN, data_file);
        }

        debug_printf("dealing with extracts\n");

        // fill in extract samples
        for (i = 0; i < temp_file_data->extract_num; i++) {
            debug_printf("extract #%d\n", i);

            debug_printf("grabbing position of extract\n");

            // fill in position of extract
            tmp = (unsigned char*) &temp_file_data->extract[i].position;
            ret = copy_buf_to_ptr(&info, tmp, sizeof_member(extract_sample, position), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            debug_printf("position of extract : %"PRIu64"\n", temp_file_data->extract[i].position);

            if (temp_file_data->extract[i].position >= temp_file_data->file_size) {
                printf("load_file : extract position exceeds or equal to file size\n");
                ret_close_file(FILE_BROKEN, data_file);
            }

            debug_printf("grabbing length of extract\n");

            // fill in length of extract
            tmp = (unsigned char*) &temp_file_data->extract[i].len;
            ret = copy_buf_to_ptr(&info, tmp, sizeof_member(extract_sample, len), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            debug_printf("length of extract : %u\n", temp_file_data->extract[i].len);

            if (temp_file_data->extract[i].len > EXTRACT_SIZE_MAX) {
                printf("load_file : extract length exceeds maximum\n");
                ret_close_file(FILE_BROKEN, data_file);
            }
            if (temp_file_data->extract[i].position + temp_file_data->extract[i].len >= temp_file_data->file_size) {
                printf("load_file : extract cross the end of file\n");
                ret_close_file(FILE_BROKEN, data_file);
            }

            debug_printf("grabbing result of extraction\n");

            // fill in the result of extraction
            tmp = (unsigned char*) temp_file_data->extract[i].extract;
            ret = copy_buf_to_ptr(&info, tmp, temp_file_data->extract[i].len, IS_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            debug_printf("result of extraction : ");
#ifdef FFP_DEBUG
            for (j = 0; j < temp_file_data->extract[i].len; j++) {
                printf("%02X ", temp_file_data->extract[i].extract[j]);
            }
#endif
            debug_printf("\n");
        }


        //** start to deal with sections **//

        debug_printf("grabbing number of sections\n");

        // fill in number of sections
        tmp = (unsigned char*) &temp_sect_num;
        ret = copy_buf_to_ptr(&info, tmp, sizeof_member(file_data, section_num), NOT_STR);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        debug_printf("number of sections : %"PRIu64"\n", temp_sect_num);

        // make space for the sections
        if (temp_sect_num > 0) {
            grow_section_array(temp_file_data, temp_sect_num);

            record_misc_alloc(&dh->l2_misc_alloc_record_arr, temp_misc_alloc_record, ret, temp_file_data->section);
        }

        debug_printf("grabbing normal section size\n");

        tmp = (unsigned char*) &temp_file_data->norm_sect_size;
        ret = copy_buf_to_ptr(&info, tmp, sizeof_member(file_data, norm_sect_size), 0);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        debug_printf("normal section size : %"PRIu64"\n", temp_file_data->norm_sect_size);

        if (temp_sect_num == 0 && temp_file_data->norm_sect_size != 0) {
            printf("load_file : normal section size should be zero\n");
            ret_close_file(FILE_BROKEN, data_file);
        }

        if (temp_file_data->norm_sect_size > temp_file_data->file_size) {
            printf("load_file : normal section size exceeds file size\n");
            ret_close_file(FILE_BROKEN, data_file);
        }

        debug_printf("grabbing last section size\n");

        tmp = (unsigned char*) &temp_file_data->last_sect_size;
        ret = copy_buf_to_ptr(&info, tmp, sizeof_member(file_data, last_sect_size), 0);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        debug_printf("last section size : %"PRIu64"\n", temp_file_data->last_sect_size);

        if (temp_sect_num == 0 && temp_file_data->last_sect_size != 0) {
            printf("load_file : last section size should be zero\n");
            ret_close_file(FILE_BROKEN, data_file);
        }

        if (temp_sect_num == 1 && temp_file_data->last_sect_size != temp_file_data->norm_sect_size) {
            printf("load_file : normal section size is not consistent with last section size\n");
            ret_close_file(FILE_BROKEN, data_file);
        }

        if (temp_file_data->last_sect_size > temp_file_data->norm_sect_size) {
            printf("load_file : last section size exceeds normal section size\n");
            ret_close_file(FILE_BROKEN, data_file);
        }

        debug_printf("dealing with sections\n");

        // go through sections
        for (i = 0; i < temp_sect_num; i++) {
            add_section_to_layer2_arr(&dh->l2_section_arr, &temp_section, NULL);

            put_into_section_array(temp_file_data, temp_section);

            debug_printf("section #%d\n", i);

            debug_printf("grabbing section start position\n");

            // grab starting position
            tmp = (unsigned char*) &temp_section->start_pos;
            ret = copy_buf_to_ptr(&info, tmp, sizeof_member(section, start_pos), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            debug_printf("start position : %"PRIu64"\n", temp_section->start_pos);

            debug_printf("grabbing section end position\n");

            // grab ending position
            tmp = (unsigned char*) &temp_section->end_pos;
            ret = copy_buf_to_ptr(&info, tmp, sizeof_member(section, end_pos), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            debug_printf("end position : %"PRIu64"\n", temp_section->end_pos);

            debug_printf("grabbing number of checksum\n");

            // fill in number of checksum
            tmp = (unsigned char*) &temp_checksum_num;
            ret = copy_buf_to_ptr(&info, tmp, sizeof(temp_checksum_num), 0);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            debug_printf("number of checksums : %u\n", temp_checksum_num);

            if (temp_checksum_num > CHECKSUM_MAX_NUM) {
                printf("load_file : number of checksums exceeds maximum\n");
                ret_close_file(FILE_BROKEN, data_file);
            }

            debug_printf("dealing with checksums\n");

            // fill in checksum results
            for (j = 0; j < temp_checksum_num; j++) {
                debug_printf("checksum #%d\n", j);

                debug_printf("grabbing type of checksum\n");

                // fill in type of checksum
                tmp = (unsigned char*) &temp_checksum_type;
                ret = copy_buf_to_ptr(&info, tmp, sizeof_member(checksum_result, type), 0);
                if (ret) {
                    ret_close_file(ret, data_file);
                }

                switch (temp_checksum_type) {
                    case CHECKSUM_SHA1_ID :
                        temp_checksum_result = temp_section->checksum + CHECKSUM_SHA1_INDEX;
                        break;
                    case CHECKSUM_SHA256_ID :
                        temp_checksum_result = temp_section->checksum + CHECKSUM_SHA256_INDEX;
                        break;
                    case CHECKSUM_SHA512_ID :
                        temp_checksum_result = temp_section->checksum + CHECKSUM_SHA512_INDEX;
                        break;
                    default :
                        printf("load_file : invalid checksum type\n");
                        ret_close_file(FILE_BROKEN, data_file);
                }

                temp_checksum_result->type = temp_checksum_type;

                debug_printf("type of checksum : %u\n", temp_checksum_result->type);

                debug_printf("grabbing length of checksum\n");

                // fill in length of checksum
                tmp = (unsigned char*) &temp_checksum_result->len;
                ret = copy_buf_to_ptr(&info, tmp, sizeof_member(checksum_result, len), 0);
                if (ret) {
                    ret_close_file(ret, data_file);
                }

                debug_printf("length of checksum : %u\n", temp_checksum_result->len);

                if (temp_checksum_result->len > CHECKSUM_MAX_LEN) {
                    printf("load_file : length of checksum exceeds maximum length\n");
                    ret_close_file(FILE_BROKEN, data_file);
                }

                debug_printf("grabbing checksum result\n");

                // fill in checksum result
                tmp = (unsigned char*) temp_section->checksum[j].checksum;
                ret = copy_buf_to_ptr(&info, tmp, temp_checksum_result->len, IS_STR);
                if (ret) {
                    ret_close_file(ret, data_file);
                }

                debug_printf("checksum result : ");
#ifdef FFP_DEBUG
                for (k = 0; k < temp_section->checksum[j].len; k++) {
                    printf("%02X ", temp_checksum_result->checksum[k]);
                }
                debug_printf("\n");
#endif

                debug_printf("converting checksum to string\n");

                // add file checksums to hashtable
                // convert checksum to string
                bytes_to_hex_str(temp_checksum_result->checksum_str, temp_checksum_result->checksum, temp_checksum_result->len);

                debug_printf("checksum string : %s\n", temp_checksum_result->checksum_str);

                debug_printf("adding to checksum translation structures\n");
            }

            ret = link_sect_to_checksum_structures(dh, temp_section);
            if (ret) {
                ret_close_file(FILE_BROKEN, data_file);
            }

            debug_printf("grabbing number of extracts\n");

            // fill in number of extracts
            tmp = (unsigned char*) &temp_section->extract_num;
            ret = copy_buf_to_ptr(&info, tmp, sizeof_member(file_data, extract_num), 0);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            debug_printf("number of extracts : %u\n", temp_section->extract_num);

            if (temp_section->extract_num > EXTRACT_MAX_NUM) {
                printf("load_file : number of extracts exceeds maximum\n");
                ret_close_file(FILE_BROKEN, data_file);
            }

            debug_printf("dealing with extracts\n");

            // fill in extract samples
            for (j = 0; j < temp_section->extract_num; j++) {
                debug_printf("extract #%d\n", j);

                debug_printf("grabbing position of extract\n");

                // fill in position of extract
                tmp = (unsigned char*) &temp_section->extract[j].position;
                ret = copy_buf_to_ptr(&info, tmp, sizeof_member(extract_sample, position), 0);
                if (ret) {
                    ret_close_file(ret, data_file);
                }

                debug_printf("position of extract : %"PRIu64"\n", temp_section->extract[j].position);

                if (temp_section->extract[j].position >= temp_entry->data->file_size) {
                    printf("load_file : extract position exceeds or equal to file size\n");
                    ret_close_file(FILE_BROKEN, data_file);
                }

                debug_printf("grabbing length of extract\n");

                // fill in length of extract
                tmp = (unsigned char*) &temp_section->extract[j].len;
                ret = copy_buf_to_ptr(&info, tmp, sizeof_member(extract_sample, len), 0);
                if (ret) {
                    ret_close_file(ret, data_file);
                }

                debug_printf("length of extract : %u\n", temp_section->extract[j].len);

                if (temp_section->extract[j].len > EXTRACT_SIZE_MAX) {
                    printf("load_file : extract length exceeds maximum\n");
                    ret_close_file(FILE_BROKEN, data_file);
                }
                if (temp_section->extract[j].position + temp_section->extract[j].len > temp_entry->data->file_size) {
                    printf("load_file : extract cross the end of file\n");
                    ret_close_file(FILE_BROKEN, data_file);
                }

                debug_printf("grabbing result of extraction\n");

                // fill in the result of extraction
                tmp = (unsigned char*) temp_section->extract[j].extract;
                ret = copy_buf_to_ptr(&info, tmp, temp_section->extract[j].len, 1);
                if (ret) {
                    ret_close_file(ret, data_file);
                }

                debug_printf("result of extraction : ");
#ifdef FFP_DEBUG
                for (k = 0; k < temp_section->extract[j].len; k++) {
                    printf("%02X ", temp_section->extract[j].extract[k]);
                }
#endif
                debug_printf("\n");
            }
        }

skip_file:

        // extra line of defense
        ret = verify_entry(dh, temp_entry, ALLOW_NULL_CHILD_PTR);  // allow pointer to child entry to be null
        if (ret) {
            printf("load_file : verify_entry detected errors\n");
            printf("Please report to developer as this should not happen\n");
            printf("It is recommended that you revert to previous version of database as this may indicate your database is now corrupted\n");
            printf("Sorry for the inconvenience\n");
            ret_close_file(FILE_BROKEN, data_file);
        }
        /*else {
            printf("load_file : no error was detected by verify_entry\n");
        }*/

        debug_printf("adding to other translation structures\n");

        //** add entry to other dh->tree/list/hashtable **//
        // add entry to eid_to_entry hashtable
        lookup_entry_id_via_dh(dh, temp_entry->entry_id_str, &temp_entry_find);
        if (temp_entry_find) {  // duplicates
            printf("load_file : duplicated entry id\n");
            ret_close_file(DUPLICATE_ERROR, data_file);
        }
        link_entry_to_entry_id_structures(dh, temp_entry);
    }

    printf("\n");

    debug_printf("grabbing tail\n");

    //** check tail **//
    tmp = head_tail;
    ret = copy_buf_to_ptr(&info, tmp, 15, 1);
    if (ret) {
        ret_close_file(ret, data_file);
    }

    debug_printf("checking tail\n");

    for (i = 1; i <= 15; i++) {
        if (head_tail[i-1] != i) {
            printf("load_file : tail is corrupted\n");
            ret_close_file(FILE_BROKEN, data_file);
        }
    }

    if (info.file_pos != info.file_size || (info.buffer_eol && info.buffer_pos != info.buffer_alt_size)) {
        printf("load_file : file is longer than expected\n");
        ret_close_file(FILE_BROKEN, data_file);
    }

    debug_printf("database file loaded\n");

    printf("loading completed\n");

    ret_close_file(0, data_file);
}

static int link_entry_for_save_file(database_handle* dh, linked_entry* entry, linked_entry* tail, linked_entry** ret_tail) {
    ffp_eid_int i;

    linked_entry* temp;
    linked_entry* temp_backup;

    int ret = 0;

    if (!entry) {
        return WRONG_ARGS;
    }

    if (verify_entry(dh, entry, 0x0)) {
        return VERIFY_FAIL;
    }

    if (!tail) {
        link_entry_clear(entry);
    }
    else {
        link_entry_after(tail, entry);
    }

    temp = entry;
    temp_backup = temp;
    while (temp->child_num == 1) {
        if (verify_entry(dh, temp->child[0], 0x0)) {  // verification failed, don't link the entry and its children
            // go back one layer
            temp = temp_backup;
            ret = VERIFY_FAIL;
            break;
        }
        link_entry_after(temp, temp->child[0]);

        temp_backup = temp;
        temp = temp->child[0];
    }

    tail = temp;

    if (temp->child_num > 1) {
        for (i = 0; i < temp->child_num; i++) {
            if (verify_entry(dh, temp->child[i], 0x0)) {
                ret = VERIFY_FAIL;
                continue;
            }
            ret = link_entry_for_save_file(dh, temp->child[i], tail, &tail);
        }
    }

    if (ret_tail) {
        *ret_tail = tail;
    }

    return ret;
}

int save_file (database_handle* dh, char* file_name) {
    FILE* data_file;

    struct stat file_stat;

    int i, j, ret;
    unsigned char head_tail[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

    unsigned char buffer[DFILE_BUFFER_SIZE];
    unsigned char buffer_secondary[DFILE_BUFFER_SIZE];

    char version_buf[] = "00.01";

    struct buffer_info info;

    ffp_eid_int total_entry_num = 0;

    str_len_int dbase_file_name_len;

    long int file_pos;

    int64_t temp_time_int64;
    uint8_t temp_time_uint8;
    int8_t temp_time_int8;
    uint16_t temp_time_uint16;

    uint64_t field_presence_bitmap;

    uint16_t temp_checksum_num;

    checksum_result* temp_checksum_result;
    //extract_sample* temp_extract_sample;

    //unsigned char* tmp;

    linked_entry* temp_entry;

    file_data* temp_file_data;

    section* temp_section;

    init_buffer_info(&info, buffer, buffer_secondary, DFILE_BUFFER_SIZE, BUF_WRITE);

    if ((i = verify_str_terminated(file_name, FILE_NAME_MAX, &dbase_file_name_len, ALLOW_ZERO_LEN_STR))) {
        printf("save_file : database file name not null terminated\n");
        return i;
    }
    if (dbase_file_name_len == 0) {
        printf("save_file : database file name is empty\n");
        return WRONG_ARGS;
    }

    data_file = fopen(file_name, "wb");

    if (!data_file) {
        printf("save_file : unable to open file\n");
        perror("save_file : error");
        return FOPEN_FAIL;
    }

    info.fp = data_file;
    stat(file_name, &file_stat);
    info.file_size = file_stat.st_size;
    info.file_pos = 0; 

    printf("writing database general info\n");

    // write preamble
    ret = copy_ptr_to_buf(&info, head_tail, 15, IS_STR);
    if (ret) {
        ret_close_file(ret, data_file);
    }

    // write version
    ret = copy_ptr_to_buf(&info, version_buf, 5, IS_STR);
    if (ret) {
        ret_close_file(ret, data_file);
    }

    // write number of branches
    ret = copy_ptr_to_buf(&info, &dh->tree.child_num, sizeof_member(linked_entry, child_num), NOT_STR);
    if (ret) {
        ret_close_file(ret, data_file);
    }

    flush_buf(&info);

    // write a dummy value(default to 0) to total number of entries, update later
    file_pos = info.file_pos;
    ret = copy_ptr_to_buf(&info, &total_entry_num, sizeof(ffp_eid_int), NOT_STR);
    if (ret) {
        ret_close_file(ret, data_file);
    }

    // link up all the entries
    ret = link_entry_for_save_file(dh, &dh->tree, NULL, NULL);
    if (ret == VERIFY_FAIL) {
        printf("WARNING : some entries failed verification\n");
        printf("          the entries and the entries under them are not saved in file\n");
        printf("          see help verify for finding the entries\n");
    }

    // traverse through the linked list and write each entry into file
    temp_entry = dh->tree.link_next;

    if (!temp_entry) {
        printf("database is empty");
    }

    while (temp_entry) {
        printf("\rwriting entry no. %"PRIu64"", total_entry_num + 1);
        fflush(stdout);

        // write branch id
        ret = copy_ptr_to_buf(&info, &temp_entry->branch_id, EID_LEN, IS_STR);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        // write entry id
        ret = copy_ptr_to_buf(&info, &temp_entry->entry_id, EID_LEN, IS_STR);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        // write parent entry id
        ret = copy_ptr_to_buf(&info, &temp_entry->parent->entry_id, EID_LEN, IS_STR);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        // write entry type
        ret = copy_ptr_to_buf(&info, &temp_entry->type, sizeof_member(linked_entry, type), NOT_STR);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        // write number of children
        ret = copy_ptr_to_buf(&info, &temp_entry->child_num, sizeof_member(linked_entry, child_num), NOT_STR);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        // write created by marker
        ret = copy_ptr_to_buf(&info, &temp_entry->created_by, sizeof_member(linked_entry, created_by), NOT_STR);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        // write file name length
        ret = copy_ptr_to_buf(&info, &temp_entry->file_name_len, sizeof_member(linked_entry, file_name_len), NOT_STR);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        // write file name
        ret = copy_ptr_to_buf(&info, &temp_entry->file_name, temp_entry->file_name_len, IS_STR);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        // write field presence bitmap
        field_presence_bitmap = 0x0;
        if (temp_entry->tag_str_len > 0) {
            field_presence_bitmap |= HAS_TAG_STR;
        }
        if (temp_entry->user_msg_len > 0) {
            field_presence_bitmap |= HAS_USR_MSG;
        }
        if (temp_entry->tod_utc_used) {
            field_presence_bitmap |= HAS_ADD_TIME;
        }
        if (temp_entry->tom_utc_used) {
            field_presence_bitmap |= HAS_MOD_TIME;
        }
        if (temp_entry->tusr_utc_used) {
            field_presence_bitmap |= HAS_USR_TIME;
        }
        if (temp_entry->data) {
            field_presence_bitmap |= HAS_FILE_DATA;
        }
        ret = copy_ptr_to_buf(&info, &field_presence_bitmap, sizeof(uint64_t), NOT_STR);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        if (temp_entry->tag_str_len > 0) {
            // write tag string length
            ret = copy_ptr_to_buf(&info, &temp_entry->tag_str_len, sizeof_member(linked_entry, tag_str_len), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // write tag string
            ret = copy_ptr_to_buf(&info, &temp_entry->tag_str, temp_entry->tag_str_len, IS_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
        }

        if (temp_entry->user_msg_len > 0) {
            // write user message length
            ret = copy_ptr_to_buf(&info, &temp_entry->user_msg_len, sizeof_member(linked_entry, user_msg_len), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // write user message
            ret = copy_ptr_to_buf(&info, &temp_entry->user_msg, temp_entry->user_msg_len, IS_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
        }

        if (temp_entry->tod_utc_used) {
            // write time of addition of entry
            // seconds
            temp_time_uint8 = temp_entry->tod_utc.tm_sec;
            ret = copy_ptr_to_buf(&info, &temp_time_uint8, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // minutes
            temp_time_uint8 = temp_entry->tod_utc.tm_min;
            ret = copy_ptr_to_buf(&info, &temp_time_uint8, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // hours
            temp_time_uint8 = temp_entry->tod_utc.tm_hour;
            ret = copy_ptr_to_buf(&info, &temp_time_uint8, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // day of month
            temp_time_uint8 = temp_entry->tod_utc.tm_mday;
            ret = copy_ptr_to_buf(&info, &temp_time_uint8, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // month
            temp_time_uint8 = temp_entry->tod_utc.tm_mon;
            ret = copy_ptr_to_buf(&info, &temp_time_uint8, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // year
            temp_time_int64 = temp_entry->tod_utc.tm_year;
            ret = copy_ptr_to_buf(&info, &temp_time_int64, sizeof(int64_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // week day
            temp_time_uint8 = temp_entry->tod_utc.tm_wday;
            ret = copy_ptr_to_buf(&info, &temp_time_uint8, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // year day
            temp_time_uint16 = temp_entry->tod_utc.tm_yday;
            ret = copy_ptr_to_buf(&info, &temp_time_uint16, sizeof(uint16_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // day light saving flag
            if (temp_entry->tod_utc.tm_isdst < 0) {
                temp_time_int8 = -1;
            }
            else if (temp_entry->tod_utc.tm_isdst == 0) {
                temp_time_int8 = 0;
            }
            else if (temp_entry->tod_utc.tm_isdst > 0) {
                temp_time_int8 = 1;
            }
            ret = copy_ptr_to_buf(&info, &temp_time_int8, sizeof(int8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
        }

        if (temp_entry->tom_utc_used) {
            // write time of modification of entry
            // seconds
            temp_time_uint8 = temp_entry->tom_utc.tm_sec;
            ret = copy_ptr_to_buf(&info, &temp_time_uint8, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // minutes
            temp_time_uint8 = temp_entry->tom_utc.tm_min;
            ret = copy_ptr_to_buf(&info, &temp_time_uint8, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // hours
            temp_time_uint8 = temp_entry->tom_utc.tm_hour;
            ret = copy_ptr_to_buf(&info, &temp_time_uint8, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // day of month
            temp_time_uint8 = temp_entry->tom_utc.tm_mday;
            ret = copy_ptr_to_buf(&info, &temp_time_uint8, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // month
            temp_time_uint8 = temp_entry->tom_utc.tm_mon;
            ret = copy_ptr_to_buf(&info, &temp_time_uint8, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // year
            temp_time_int64 = temp_entry->tom_utc.tm_year;
            ret = copy_ptr_to_buf(&info, &temp_time_int64, sizeof(int64_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // week day
            temp_time_uint8 = temp_entry->tom_utc.tm_wday;
            ret = copy_ptr_to_buf(&info, &temp_time_uint8, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // year day
            temp_time_uint16 = temp_entry->tom_utc.tm_yday;
            ret = copy_ptr_to_buf(&info, &temp_time_uint16, sizeof(uint16_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // day light saving flag
            if (temp_entry->tom_utc.tm_isdst < 0) {
                temp_time_int8 = -1;
            }
            else if (temp_entry->tom_utc.tm_isdst == 0) {
                temp_time_int8 = 0;
            }
            else if (temp_entry->tom_utc.tm_isdst > 0) {
                temp_time_int8 = 1;
            }
            ret = copy_ptr_to_buf(&info, &temp_time_int8, sizeof(int8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
        }

        if (temp_entry->tusr_utc_used) {
            // write user specified time of entry
            // seconds
            temp_time_uint8 = temp_entry->tusr_utc.tm_sec;
            ret = copy_ptr_to_buf(&info, &temp_time_uint8, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // minutes
            temp_time_uint8 = temp_entry->tusr_utc.tm_min;
            ret = copy_ptr_to_buf(&info, &temp_time_uint8, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // hours
            temp_time_uint8 = temp_entry->tusr_utc.tm_hour;
            ret = copy_ptr_to_buf(&info, &temp_time_uint8, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // day of month
            temp_time_uint8 = temp_entry->tusr_utc.tm_mday;
            ret = copy_ptr_to_buf(&info, &temp_time_uint8, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // month
            temp_time_uint8 = temp_entry->tusr_utc.tm_mon;
            ret = copy_ptr_to_buf(&info, &temp_time_uint8, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // year
            temp_time_int64 = temp_entry->tusr_utc.tm_year;
            ret = copy_ptr_to_buf(&info, &temp_time_int64, sizeof(int64_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // week day
            temp_time_uint8 = temp_entry->tusr_utc.tm_wday;
            ret = copy_ptr_to_buf(&info, &temp_time_uint8, sizeof(uint8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // year day
            temp_time_uint16 = temp_entry->tusr_utc.tm_yday;
            ret = copy_ptr_to_buf(&info, &temp_time_uint16, sizeof(uint16_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // day light saving flag
            if (temp_entry->tusr_utc.tm_isdst < 0) {
                temp_time_int8 = -1;
            }
            else if (temp_entry->tusr_utc.tm_isdst == 0) {
                temp_time_int8 = 0;
            }
            else if (temp_entry->tusr_utc.tm_isdst > 0) {
                temp_time_int8 = 1;
            }
            ret = copy_ptr_to_buf(&info, &temp_time_int8, sizeof(int8_t), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
        }

        if (!temp_entry->data) {
            goto skip_file;
        }

        temp_file_data = temp_entry->data;

        // write file size
        ret = copy_ptr_to_buf(&info, &temp_file_data->file_size, sizeof_member(file_data, file_size), NOT_STR);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        // calculate number of checksum used
        temp_checksum_num = 0;
        for (i = 0; i < CHECKSUM_MAX_NUM; i++) {
            temp_checksum_result = temp_file_data->checksum + i;

            if (temp_checksum_result->type != CHECKSUM_UNUSED) {
                temp_checksum_num++;
            }
        }

        // write number of checksums
        ret = copy_ptr_to_buf(&info, &temp_checksum_num, sizeof(temp_checksum_num), NOT_STR);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        // write checksum results
        for (i = 0; i < CHECKSUM_MAX_NUM; i++) {
            temp_checksum_result = temp_file_data->checksum + i;

            if (temp_checksum_result->type == CHECKSUM_UNUSED) {
                continue;
            }

            // write type of checksum
            ret = copy_ptr_to_buf(&info, &temp_checksum_result->type, sizeof_member(checksum_result, type), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // write length of checksum
            ret = copy_ptr_to_buf(&info, &temp_checksum_result->len, sizeof_member(checksum_result, len), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // write checksum result
            ret = copy_ptr_to_buf(&info, &temp_checksum_result->checksum, temp_checksum_result->len, IS_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
        }

        // write number of extracts
        ret = copy_ptr_to_buf(&info, &temp_file_data->extract_num, sizeof_member(file_data, extract_num), NOT_STR);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        // write extract samples
        for (i = 0; i < temp_file_data->extract_num; i++) {
            // write position of extract
            ret = copy_ptr_to_buf(&info, &temp_file_data->extract[i].position, sizeof_member(extract_sample, position), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // write length of extract
            ret = copy_ptr_to_buf(&info, &temp_file_data->extract[i].len, sizeof_member(extract_sample, len), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // write result of extraction
            ret = copy_ptr_to_buf(&info, &temp_file_data->extract[i].extract, temp_file_data->extract[i].len, IS_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }
        }

        //** start to deal with sections **//

        // write number of sections
        ret = copy_ptr_to_buf(&info, &temp_file_data->section_num, sizeof_member(file_data, section_num), NOT_STR);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        // write normal section size
        ret = copy_ptr_to_buf(&info, &temp_file_data->norm_sect_size, sizeof_member(file_data, norm_sect_size), NOT_STR);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        // write last section size
        ret = copy_ptr_to_buf(&info, &temp_file_data->last_sect_size, sizeof_member(file_data, last_sect_size), NOT_STR);
        if (ret) {
            ret_close_file(ret, data_file);
        }

        // write sections
        for (i = 0; i < temp_file_data->section_num; i++) {
            temp_section = temp_file_data->section[i];

            // write starting position
            ret = copy_ptr_to_buf(&info, &temp_section->start_pos, sizeof_member(section, start_pos), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // write ending position
            ret = copy_ptr_to_buf(&info, &temp_section->end_pos, sizeof_member(section, end_pos), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // calculate number of checksums
            temp_checksum_num = 0;
            for (j = 0; j < CHECKSUM_MAX_NUM; j++) {
                temp_checksum_result = temp_section->checksum + j;

                if (temp_checksum_result->type != CHECKSUM_UNUSED) {
                    temp_checksum_num++;
                }
            }

            // write number of checksums
            ret = copy_ptr_to_buf(&info, &temp_checksum_num, sizeof(temp_checksum_num), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // write checksum results
            for (j = 0; j < CHECKSUM_MAX_NUM; j++) {
                temp_checksum_result = temp_section->checksum + j;

                if (temp_checksum_result->type == CHECKSUM_UNUSED) {
                    continue;
                }

                // write type of checksum
                ret = copy_ptr_to_buf(&info, &temp_checksum_result->type, sizeof_member(checksum_result, type), NOT_STR);
                if (ret) {
                    ret_close_file(ret, data_file);
                }

                // write length of checksum
                ret = copy_ptr_to_buf(&info, &temp_checksum_result->len, sizeof_member(checksum_result, len), NOT_STR);
                if (ret) {
                    ret_close_file(ret, data_file);
                }

                // write checksum result
                ret = copy_ptr_to_buf(&info, &temp_checksum_result->checksum, temp_checksum_result->len, IS_STR);
                if (ret) {
                    ret_close_file(ret, data_file);
                }
            }

            // write number of extracts
            ret = copy_ptr_to_buf(&info, &temp_section->extract_num, sizeof_member(section, extract_num), NOT_STR);
            if (ret) {
                ret_close_file(ret, data_file);
            }

            // write extract samples
            for (j = 0; j < temp_section->extract_num; j++) {
                // write position of extract
                ret = copy_ptr_to_buf(&info, &temp_section->extract[j].position, sizeof_member(extract_sample, position), NOT_STR);
                if (ret) {
                    ret_close_file(ret, data_file);
                }

                // write length of extract
                ret = copy_ptr_to_buf(&info, &temp_section->extract[j].len, sizeof_member(extract_sample, len), NOT_STR);
                if (ret) {
                    ret_close_file(ret, data_file);
                }

                // write result of extraction
                ret = copy_ptr_to_buf(&info, &temp_section->extract[j].extract, temp_section->extract[j].len, IS_STR);
                if (ret) {
                    ret_close_file(ret, data_file);
                }
            }
        }

skip_file:

        temp_entry = temp_entry->link_next;
        total_entry_num++;
    }

    printf("\n");

    printf("total no. of entries written : %"PRIu64"\n", total_entry_num);

    // write tail
    ret = copy_ptr_to_buf(&info, head_tail, 15, IS_STR);
    if (ret) {
        ret_close_file(ret, data_file);
    }

    // update the total entry number
    buf_seek(&info, file_pos, SEEK_SET);
    ret = copy_ptr_to_buf(&info, &total_entry_num, sizeof(ffp_eid_int), NOT_STR);
    if (ret) {
        ret_close_file(ret, data_file);
    }

    flush_buf(&info);

    printf("writing completed\n");

    ret_close_file(0, data_file);
}
