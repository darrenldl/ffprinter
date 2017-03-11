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
#include "ffprinter_function_template.h"

const char space_pad[] = "                                                                                                                                                                                                                            ";

int interruptable = 0;
int interruptable_old = 0;

#ifndef FFP_USE_MEMSET_S_FOR_MEM_WIPE
int mem_wipe_sec(void* ptr, uint64_t size) {
    volatile uint64_t*  vol_int_ptr = ptr;
    volatile uint64_t*  end_int_ptr = vol_int_ptr + size / sizeof(uint64_t);

    volatile unsigned char* vol_uchar_ptr = (unsigned char*) end_int_ptr;
    volatile unsigned char* end_uchar_ptr = ptr + size;

    while (vol_int_ptr < end_int_ptr) {
        *vol_int_ptr++ = 0;
    }
    while (vol_uchar_ptr < end_uchar_ptr) {
        *vol_uchar_ptr++ = 0;
    }

    return 0;
}
#endif

int init_uniq_char_map (uniq_char_map* uniq_char) {
    if (!uniq_char) {
        printf("init_uniq_char_map : uniq_char is null\n");
        return WRONG_ARGS;
    }

    uniq_char->next = 0;
    uniq_char->character = 0;
    mem_wipe_sec(&uniq_char->l1_arr_map, sizeof(simple_bitmap));

    return 0;
}

int reverse_strcpy(char* dst, const char* src) {
    int i, len;

    i = 0;
    while (src[i] != 0) {
        i++;
    }
    len = i;

    for (i = 0; i < len; i++) {
        dst[i] = src[len-1 - i];
    }
    dst[len] = 0;

    return 0;
}

int verify_str_terminated (const char* str, str_len_int max_len, str_len_int* length, uint32_t flags) {
    str_len_int i;

    if (!str) {
        return WRONG_ARGS;
    }
    if (!(flags & ALLOW_ZERO_LEN_STR) && max_len == 0) {
        return WRONG_ARGS;
    }

    for (i = 0; i < max_len+1; i++) {
        if (str[i] == 0) {
            break;
        }
    }

    if (length) {
        *length = i;
    }

    if (i == max_len+1) {
        return VERIFY_FAIL;
    }

    return 0;
}

int tag_count(const char* tag_str, str_len_int* ret_tag_num, str_len_int* ret_tag_min_len, str_len_int* ret_tag_max_len) {
    char tag[TAG_STR_MAX+1];
    str_len_int max_len = 0;
    unsigned char min_len_recorded = 0;
    str_len_int min_len;
    str_len_int tag_num = 0;
    str_len_int tag_str_len;

    str_len_int i;
    str_len_int len_read = 0;
    str_len_int len_copied = 0;

    tag_str_len = strlen(tag_str);

    for (i = 0; i < tag_str_len; i += len_read + 1) {
        grab_until_sep
            (   tag,
                tag_str + i,
                tag_str_len - i,
                &len_read,
                &len_copied,
                NO_IGNORE_ESCAPE_CHAR,
                '|'
            );
        if (max_len < len_copied) {
            max_len = len_copied;
        }
        if (!min_len_recorded) {
            min_len = len_copied;
            min_len_recorded = 1;
        }
        else {
            if (min_len > len_copied) {
                min_len = len_copied;
            }
        }
        tag_num++;
    }

    if (ret_tag_num) {
        *ret_tag_num = tag_num;
    }
    if (ret_tag_max_len) {
        *ret_tag_max_len = max_len;
    }

    return 0;
}

int grab_until_sep(char* dst, const char* src, str_len_int max_len, str_len_int* len_read, str_len_int* len_copied, unsigned char ignore_escp_char, char sep) {
    str_len_int i, j;

    int ret;

    unsigned char in_single_quotes = 0;
    unsigned char in_double_quotes = 0;

    if ((ret = verify_str_terminated(src, max_len, &i, 0x0))) {
        return ret;
    }

    if (i < max_len) {
        max_len = i;
    }

    for (   i = 0, j = 0;
            i < max_len;
            i++
        )
    {
        if (        in_single_quotes    ) {     // within a pair of single quotes
            // escape characters are treated as literal character in single quotes
            if (src[i] == '\'') {
                in_single_quotes = 0;
            }
            else {  // normal characters
                dst[j++] = src[i];
            }
        }
        else if (   in_double_quotes    ) {     // within a pair of double quotes
            // escape characters are still respected within double quotes
            if (src[i] == '\\' && !ignore_escp_char) {   // if current character is escape character, read next character from src (which may be NULL) into dst instead
                i++;
                dst[j++] = src[i];
            }
            else {
                if (src[i] == '\"') {
                    in_double_quotes = 0;
                }
                else {  // normal characters
                    dst[j++] = src[i];
                }
            }
        }
        else {      // not within any quotes
            if (src[i] == '\\' && !ignore_escp_char) {   // if current character is escape character, read next character from src (which may be NULL) into dst instead
                i++;
                dst[j++] = src[i];
            }
            else {  // not escaping anything
                if (        src[i] == '\'') {   // detect a single quote character
                    in_single_quotes = 1;
                }
                else if (   src[i] == '\"') {   // detect a double quote character
                    in_double_quotes = 1;
                }
                else if (   src[i] == sep) {    // detect separator or end of string
                    break;
                }
                else {  // normal characters
                    dst[j++] = src[i];
                }
            }
        }
    }

    dst[j] = 0;

    if (len_read) {
        *len_read = i;
    }
    if (len_copied) {
        *len_copied = j;
    }

    return 0;
}

int discard_input_buffer(void) {
    int c;

    while ((c = getchar()) != '\n' && c != EOF) {
        ;;  // do nothing
    }

    return 0;
}

int escape_char(char* dst, const char* src, str_len_int max_len, char tar) {
    str_len_int src_len;
    str_len_int i, j;

    if (        !dst
            ||  !src
       )
    {
        return WRONG_ARGS;
    }

    src_len = strlen(src);

    if (max_len < src_len) {
        return BUFFER_NO_SPACE;
    }

    for (i = 0, j = 0; i < src_len; i++, j++) {
        if (src[i] == tar) {
            dst[j] = '\\';
            j++;
            dst[j] = src[i];
        }
        else {
            dst[j] = src[i];
        }
    }

    return 0;
}

int bytes_to_hex_str (char* dst, unsigned char* ptr, uint32_t size) {
    uint32_t i;

    for (i = 0; i < size; i++) {
        dst += sprintf(dst, "%02x", ptr[i]);
    }
    *dst = 0;

    return 0;
}

int hex_str_to_bytes (unsigned char* dst, char* src) {
    uint32_t i, dst_indx;

    str_len_int src_len;

    src_len = strlen(src);

    // check all digits are hex digits
    for (i = 0; i < src_len; i++) {
        if (!isxdigit(src[i])) {
            break;
        }
    }
    if (i < src_len) {
        return INVALID_HEX_STR;
    }

    // turn upper case to lower cases
    upper_to_lower_case(src);

    // write into byte array
    dst_indx = 0;
    i = 0;
    if (src_len % 2 == 1) {     // odd number of digits, prefix with 0 in byte string
        if ('a' <= src[i] && src[i] <= 'f') {
            dst[dst_indx] = (src[i] - 'a') + 0x0A;  // consume only one digit
        }
        else if ('0' <= src[i] && src[i] <= '9') {
            dst[dst_indx] =  src[i] - '0';          // consume only one digit
        }
        dst_indx++;
        i++;
    }
    for (   /*initialised above*/;
            i < src_len;
            i += 2, dst_indx++
        )
    {
        // handle first digit
        if ('a' <= src[i] && src[i] <= 'f') {
            dst[dst_indx]  = ((src[i] - 'a') + 0x0A) * 0x10;   // consume only one digit
        }
        else if ('0' <= src[i] && src[i] <= '9') {
            dst[dst_indx]  =  (src[i] - '0')         * 0x10;   // consume only one digit
        }
        // handle second digit
        // since remaining number of digits must be even at this point
        // i+1 must be a valid index
        if ('a' <= src[i] && src[i] <= 'f') {
            dst[dst_indx] += ((src[i+1] - 'a') + 0x0A);   // consume only one digit
        }
        else if ('0' <= src[i] && src[i] <= '9') {
            dst[dst_indx] +=  (src[i+1] - '0')        ;   // consume only one digit
        }
    }

    return 0;
}

int upper_to_lower_case (char* str) {
    while (*str) {
        if ('A' <= *str && *str <= 'F') {
            *str += 32;
        }

        str++;
    }

    return 0;
}

int match_sub (const char* haystack, const char* needle, int start_pos) {
    int i, j, haystack_len, needle_len;

    haystack_len = strlen(haystack);
    needle_len = strlen(needle);

    if (haystack_len - start_pos < needle_len) {
        return 0;
    }

    for (   i = start_pos, j = 0;
            i < haystack_len && j < needle_len;
            i++, j++
        )
    {
        if (haystack[i] != needle[j]) {
            return 0;
        }
    }

    return 1;
}

int match_sub_min_max (const char* haystack, const char* needle, int start_pos_min, int start_pos_max) {
    int i, j, k, haystack_len, needle_len;

    haystack_len = strlen(haystack);
    needle_len = strlen(needle);

    if (start_pos_min < 0) {
        start_pos_min = 0;
    }

    if (start_pos_max < 0) {
        start_pos_max = haystack_len-1;
    }

    if (haystack_len - start_pos_min < needle_len) {
        return 0;
    }

    if (start_pos_max > haystack_len) {
        start_pos_max = haystack_len-1;
    }

    if (start_pos_max < start_pos_min) {
        return 0;
    }

    for (i = start_pos_min; i <= start_pos_max; i++) {
        for (   j = i, k = 0;
                j < haystack_len && k < needle_len;
                j++, k++
            )
        {
            if (haystack[j] != needle[k]) {
                break;
            }
        }

        if (k == needle_len) {
            return 1;
        }
    }

    return 0;
}

/* Structure copy functions */
int copy_checksum_result(checksum_result* dst, checksum_result* src) {
    uint16_t u16i;
    dst->type = src->type;
    dst->len = src->len;
    for (u16i = 0; u16i < dst->len; u16i++) {
        dst->checksum[u16i] = src->checksum[u16i];
    }
    bytes_to_hex_str(dst->checksum_str, dst->checksum, dst->len);

    return 0;
}

int copy_extract_sample(extract_sample* dst, extract_sample* src) {
    uint16_t u16i;
    dst->position = src->position;
    dst->len = src->len;
    for (u16i = 0; u16i < dst->len; u16i++) {
        dst->extract[u16i] = src->extract[u16i];
    }

    return 0;
}

/* For entry */
init_generic_trans_struct_one_to_one(eid, e)
init_layer1_generic_arr(eid, e, L1_EID_TO_E_ARR_SIZE)
init_layer2_generic_arr(eid, e, L2_EIE_INIT_SIZE)
init_generic_exist_mat(eid, EID_STR_MAX)

add_generic_to_htab(eid, e)
del_generic_from_htab(eid, e);

add_generic_to_layer2_arr(eid, e, L2_EIE_GROW_SIZE, L1_EID_TO_E_ARR_SIZE)
del_generic_from_layer2_arr(eid, e, L1_EID_TO_E_ARR_SIZE)
get_generic_from_layer2_arr(eid, e, L1_EID_TO_E_ARR_SIZE)
get_l1_generic_from_layer2_arr(eid, e)

del_l2_generic_arr(eid, e, L2_EIE_INIT_SIZE, L2_EIE_GROW_SIZE)

add_generic_to_generic_exist_mat(eid, L2_EIE_GROW_SIZE, L1_EID_TO_E_ARR_SIZE, EID_STR_MAX)
del_generic_from_generic_exist_mat(eid, e, L1_EID_TO_E_ARR_SIZE)

/* For file name */
init_generic_trans_struct_one_to_many(fn, e)
init_layer1_generic_arr(fn, e, L1_FN_TO_E_ARR_SIZE)
init_layer2_generic_arr(fn, e, L2_FNE_INIT_SIZE)
init_generic_exist_mat(fn, FILE_NAME_MAX)

add_generic_to_htab(fn, e)
del_generic_from_htab(fn, e)

add_generic_to_generic_chain(fn, e, prev_same_fn, next_same_fn, file_name, linked_entry)
del_generic_from_generic_chain(fn, e, prev_same_fn, next_same_fn, linked_entry)

add_generic_to_layer2_arr(fn, e, L2_FNE_GROW_SIZE, L1_FN_TO_E_ARR_SIZE)
del_generic_from_layer2_arr(fn, e, L1_FN_TO_E_ARR_SIZE)
get_generic_from_layer2_arr(fn, e, L1_FN_TO_E_ARR_SIZE)
get_l1_generic_from_layer2_arr(fn, e)

del_l2_generic_arr(fn, e, L2_FNE_INIT_SIZE, L2_FNE_GROW_SIZE)

add_generic_to_generic_exist_mat(fn, L2_FNE_GROW_SIZE, L1_FN_TO_E_ARR_SIZE, FILE_NAME_MAX)
del_generic_from_generic_exist_mat(fn, e, L1_FN_TO_E_ARR_SIZE)

/* For tag */
init_generic_trans_struct_one_to_many(tag, e)
init_layer1_generic_arr(tag, e, L1_TAG_TO_E_ARR_SIZE)
init_layer2_generic_arr(tag, e, L2_TGE_INIT_SIZE)
init_generic_exist_mat(tag, TAG_STR_MAX)

add_generic_to_htab(tag, e)
del_generic_from_htab(tag, e)

add_generic_to_generic_chain(tag, e, prev_same_tag, next_same_tag, tag_str, linked_entry)
del_generic_from_generic_chain(tag, e, prev_same_tag, next_same_tag, linked_entry)

add_generic_to_layer2_arr(tag, e, L2_TGE_GROW_SIZE, L1_TAG_TO_E_ARR_SIZE)
del_generic_from_layer2_arr(tag, e, L1_TAG_TO_E_ARR_SIZE)
get_generic_from_layer2_arr(tag, e, L1_TAG_TO_E_ARR_SIZE)
get_l1_generic_from_layer2_arr(tag, e)

del_l2_generic_arr(tag, e, L2_TGE_INIT_SIZE, L2_TGE_GROW_SIZE)

add_generic_to_generic_exist_mat(tag, L2_TGE_GROW_SIZE, L1_TAG_TO_E_ARR_SIZE, TAG_STR_MAX)
del_generic_from_generic_exist_mat(tag, e, L1_TAG_TO_E_ARR_SIZE)

/* For file data checksum */
// sha1
init_generic_trans_struct_one_to_many(sha1f, fd)
init_layer1_generic_arr(sha1f, fd, L1_CSUM_TO_FD_ARR_SIZE)
init_layer2_generic_arr(sha1f, fd, L2_CSF_INIT_SIZE)
init_generic_exist_mat(sha1f, CHECKSUM_STR_MAX)

add_generic_to_htab(sha1f, fd)
del_generic_from_htab(sha1f, fd)

add_generic_to_generic_chain(sha1f, fd, prev_same_sha1, next_same_sha1, checksum[CHECKSUM_SHA1_INDEX].checksum_str, file_data)
del_generic_from_generic_chain(sha1f, fd, prev_same_sha1, next_same_sha1, file_data)

add_generic_to_layer2_arr(sha1f, fd, L2_CSF_GROW_SIZE, L1_CSUM_TO_FD_ARR_SIZE)
del_generic_from_layer2_arr(sha1f, fd, L1_CSUM_TO_FD_ARR_SIZE)
get_generic_from_layer2_arr(sha1f, fd, L1_CSUM_TO_FD_ARR_SIZE)
get_l1_generic_from_layer2_arr(sha1f, fd)

del_l2_generic_arr(sha1f, fd, L2_CSF_INIT_SIZE, L2_CSF_GROW_SIZE)

add_generic_to_generic_exist_mat(sha1f, L2_CSF_GROW_SIZE, L1_CSUM_TO_FD_ARR_SIZE, CHECKSUM_STR_MAX)
del_generic_from_generic_exist_mat(sha1f, fd, L1_CSUM_TO_FD_ARR_SIZE)

// sha256
init_generic_trans_struct_one_to_many(sha256f, fd)
init_layer1_generic_arr(sha256f, fd, L1_CSUM_TO_FD_ARR_SIZE)
init_layer2_generic_arr(sha256f, fd, L2_CSF_INIT_SIZE)
init_generic_exist_mat(sha256f, CHECKSUM_STR_MAX)

add_generic_to_htab(sha256f, fd)
del_generic_from_htab(sha256f, fd)

add_generic_to_generic_chain(sha256f, fd, prev_same_sha256, next_same_sha256, checksum[CHECKSUM_SHA256_INDEX].checksum_str, file_data)
del_generic_from_generic_chain(sha256f, fd, prev_same_sha256, next_same_sha256, file_data)

add_generic_to_layer2_arr(sha256f, fd, L2_CSF_GROW_SIZE, L1_CSUM_TO_FD_ARR_SIZE)
del_generic_from_layer2_arr(sha256f, fd, L1_CSUM_TO_FD_ARR_SIZE)
get_generic_from_layer2_arr(sha256f, fd, L1_CSUM_TO_FD_ARR_SIZE)
get_l1_generic_from_layer2_arr(sha256f, fd)

del_l2_generic_arr(sha256f, fd, L2_CSF_INIT_SIZE, L2_CSF_GROW_SIZE)

add_generic_to_generic_exist_mat(sha256f, L2_CSF_GROW_SIZE, L1_CSUM_TO_FD_ARR_SIZE, CHECKSUM_STR_MAX)
del_generic_from_generic_exist_mat(sha256f, fd, L1_CSUM_TO_FD_ARR_SIZE)

// sha512
init_generic_trans_struct_one_to_many(sha512f, fd)
init_layer1_generic_arr(sha512f, fd, L1_CSUM_TO_FD_ARR_SIZE)
init_layer2_generic_arr(sha512f, fd, L2_CSF_INIT_SIZE)
init_generic_exist_mat(sha512f, CHECKSUM_STR_MAX)

add_generic_to_htab(sha512f, fd)
del_generic_from_htab(sha512f, fd)

add_generic_to_generic_chain(sha512f, fd, prev_same_sha512, next_same_sha512, checksum[CHECKSUM_SHA512_INDEX].checksum_str, file_data)
del_generic_from_generic_chain(sha512f, fd, prev_same_sha512, next_same_sha512, file_data)

add_generic_to_layer2_arr(sha512f, fd, L2_CSF_GROW_SIZE, L1_CSUM_TO_FD_ARR_SIZE)
del_generic_from_layer2_arr(sha512f, fd, L1_CSUM_TO_FD_ARR_SIZE)
get_generic_from_layer2_arr(sha512f, fd, L1_CSUM_TO_FD_ARR_SIZE)
get_l1_generic_from_layer2_arr(sha512f, fd)

del_l2_generic_arr(sha512f, fd, L2_CSF_INIT_SIZE, L2_CSF_GROW_SIZE)

add_generic_to_generic_exist_mat(sha512f, L2_CSF_GROW_SIZE, L1_CSUM_TO_FD_ARR_SIZE, CHECKSUM_STR_MAX)
del_generic_from_generic_exist_mat(sha512f, fd, L1_CSUM_TO_FD_ARR_SIZE)

/* For section checksum */
// sha1
init_generic_trans_struct_one_to_many(sha1s, s)
init_generic_exist_mat(sha1s, CHECKSUM_STR_MAX)
init_layer1_generic_arr(sha1s, s, L1_CSUM_TO_S_ARR_SIZE)
init_layer2_generic_arr(sha1s, s, L2_CSS_INIT_SIZE)

add_generic_to_htab(sha1s, s)
del_generic_from_htab(sha1s, s)

add_generic_to_generic_chain(sha1s, s, prev_same_sha1, next_same_sha1, checksum[CHECKSUM_SHA512_INDEX].checksum_str, section)
del_generic_from_generic_chain(sha1s, s, prev_same_sha1, next_same_sha1, section)

add_generic_to_layer2_arr(sha1s, s, L2_CSS_GROW_SIZE, L1_CSUM_TO_S_ARR_SIZE)
del_generic_from_layer2_arr(sha1s, s, L1_CSUM_TO_S_ARR_SIZE)
get_generic_from_layer2_arr(sha1s, s, L1_CSUM_TO_S_ARR_SIZE)
get_l1_generic_from_layer2_arr(sha1s, s)

del_l2_generic_arr(sha1s, s, L2_CSS_INIT_SIZE, L2_CSS_GROW_SIZE)

add_generic_to_generic_exist_mat(sha1s, L2_CSS_GROW_SIZE, L1_CSUM_TO_S_ARR_SIZE, CHECKSUM_STR_MAX)
del_generic_from_generic_exist_mat(sha1s, s, L1_CSUM_TO_S_ARR_SIZE)

// sha256
init_generic_trans_struct_one_to_many(sha256s, s)
init_generic_exist_mat(sha256s, CHECKSUM_STR_MAX)
init_layer1_generic_arr(sha256s, s, L1_CSUM_TO_S_ARR_SIZE)
init_layer2_generic_arr(sha256s, s, L2_CSS_INIT_SIZE)

add_generic_to_htab(sha256s, s)
del_generic_from_htab(sha256s, s)

add_generic_to_generic_chain(sha256s, s, prev_same_sha256, next_same_sha256, checksum[CHECKSUM_SHA512_INDEX].checksum_str, section)
del_generic_from_generic_chain(sha256s, s, prev_same_sha256, next_same_sha256, section)

add_generic_to_layer2_arr(sha256s, s, L2_CSS_GROW_SIZE, L1_CSUM_TO_S_ARR_SIZE)
del_generic_from_layer2_arr(sha256s, s, L1_CSUM_TO_S_ARR_SIZE)
get_generic_from_layer2_arr(sha256s, s, L1_CSUM_TO_S_ARR_SIZE)
get_l1_generic_from_layer2_arr(sha256s, s)

del_l2_generic_arr(sha256s, s, L2_CSS_INIT_SIZE, L2_CSS_GROW_SIZE)

add_generic_to_generic_exist_mat(sha256s, L2_CSS_GROW_SIZE, L1_CSUM_TO_S_ARR_SIZE, CHECKSUM_STR_MAX)
del_generic_from_generic_exist_mat(sha256s, s, L1_CSUM_TO_S_ARR_SIZE)

// sha512
init_generic_trans_struct_one_to_many(sha512s, s)
init_generic_exist_mat(sha512s, CHECKSUM_STR_MAX)
init_layer1_generic_arr(sha512s, s, L1_CSUM_TO_S_ARR_SIZE)
init_layer2_generic_arr(sha512s, s, L2_CSS_INIT_SIZE)

add_generic_to_htab(sha512s, s)
del_generic_from_htab(sha512s, s)

add_generic_to_generic_chain(sha512s, s, prev_same_sha512, next_same_sha512, checksum[CHECKSUM_SHA512_INDEX].checksum_str, section)
del_generic_from_generic_chain(sha512s, s, prev_same_sha512, next_same_sha512, section)

add_generic_to_layer2_arr(sha512s, s, L2_CSS_GROW_SIZE, L1_CSUM_TO_S_ARR_SIZE)
del_generic_from_layer2_arr(sha512s, s, L1_CSUM_TO_S_ARR_SIZE)
get_generic_from_layer2_arr(sha512s, s, L1_CSUM_TO_S_ARR_SIZE)
get_l1_generic_from_layer2_arr(sha512s, s)

del_l2_generic_arr(sha512s, s, L2_CSS_INIT_SIZE, L2_CSS_GROW_SIZE)

add_generic_to_generic_exist_mat(sha512s, L2_CSS_GROW_SIZE, L1_CSUM_TO_S_ARR_SIZE, CHECKSUM_STR_MAX)
del_generic_from_generic_exist_mat(sha512s, s, L1_CSUM_TO_S_ARR_SIZE)

/* For file size */
init_generic_trans_struct_one_to_many(f_size, fd)
init_generic_exist_mat(f_size, FILE_SIZE_STR_MAX)
init_layer1_generic_arr(f_size, fd, L1_FSIZE_TO_FD_ARR_SIZE)

add_generic_to_htab(f_size, fd)
del_generic_from_htab(f_size, fd)

init_layer2_generic_arr(f_size, fd, L2_FZF_INIT_SIZE)

add_generic_to_generic_chain(f_size, fd, prev_same_f_size, next_same_f_size, file_size_str, file_data)
del_generic_from_generic_chain(f_size, fd, prev_same_f_size, next_same_f_size, file_data)

add_generic_to_layer2_arr(f_size, fd, L2_FZF_GROW_SIZE, L1_FSIZE_TO_FD_ARR_SIZE)
del_generic_from_layer2_arr(f_size, fd, L1_FSIZE_TO_FD_ARR_SIZE)
get_generic_from_layer2_arr(f_size, fd, L1_FSIZE_TO_FD_ARR_SIZE)
get_l1_generic_from_layer2_arr(f_size, fd)

del_l2_generic_arr(f_size, fd, L2_FZF_INIT_SIZE, L2_FZF_GROW_SIZE)

add_generic_to_generic_exist_mat(f_size, L2_FZF_GROW_SIZE, L1_FSIZE_TO_FD_ARR_SIZE, FILE_SIZE_STR_MAX)
del_generic_from_generic_exist_mat(f_size, fd, L1_FSIZE_TO_FD_ARR_SIZE)

/* other pool allocated objects */
init_layer1_obj_arr(entry, L1_ENTRY_ARR_SIZE)
init_layer2_obj_arr(entry, L2_ENTRY_INIT_SIZE)
add_obj_to_layer2_arr(entry, linked_entry, L2_ENTRY_GROW_SIZE, L1_ENTRY_ARR_SIZE)
del_obj_from_layer2_arr(entry, linked_entry, L1_ENTRY_ARR_SIZE)
get_obj_from_layer2_arr(entry, linked_entry, L1_ENTRY_ARR_SIZE)
get_l1_obj_from_layer2_arr(entry)
del_l2_obj_arr(entry, L2_ENTRY_INIT_SIZE, L2_ENTRY_GROW_SIZE)

init_layer1_obj_arr(file_data, L1_FILE_DATA_ARR_SIZE)
init_layer2_obj_arr(file_data, L2_FILE_DATA_INIT_SIZE)
add_obj_to_layer2_arr(file_data, file_data, L2_FILE_DATA_GROW_SIZE, L1_FILE_DATA_ARR_SIZE)
del_obj_from_layer2_arr(file_data, file_data, L1_FILE_DATA_ARR_SIZE)
get_obj_from_layer2_arr(file_data, file_data, L1_FILE_DATA_ARR_SIZE)
get_l1_obj_from_layer2_arr(file_data)
del_l2_obj_arr(file_data, L2_FILE_DATA_INIT_SIZE, L2_FILE_DATA_GROW_SIZE)

init_layer1_obj_arr(section, L1_SECT_ARR_SIZE)
init_layer2_obj_arr(section, L2_SECT_INIT_SIZE)
add_obj_to_layer2_arr(section, section, L2_SECT_GROW_SIZE, L1_SECT_ARR_SIZE)
del_obj_from_layer2_arr(section, section, L1_SECT_ARR_SIZE)
get_obj_from_layer2_arr(section, section, L1_SECT_ARR_SIZE)
get_l1_obj_from_layer2_arr(section)
del_l2_obj_arr(section, L2_SECT_INIT_SIZE, L2_SECT_GROW_SIZE)

init_layer1_obj_arr(misc_alloc_record, L1_MISC_ALLOC_ARR_SIZE)
init_layer2_obj_arr(misc_alloc_record, L2_MISC_ALLOC_INIT_SIZE)
add_obj_to_layer2_arr(misc_alloc_record, misc_alloc_record, L2_MISC_ALLOC_GROW_SIZE, L1_MISC_ALLOC_ARR_SIZE)
del_obj_from_layer2_arr(misc_alloc_record, misc_alloc_record, L1_MISC_ALLOC_ARR_SIZE)
get_obj_from_layer2_arr(misc_alloc_record, misc_alloc_record, L1_MISC_ALLOC_ARR_SIZE)
get_l1_obj_from_layer2_arr(misc_alloc_record)
del_l2_obj_arr(misc_alloc_record, L2_MISC_ALLOC_INIT_SIZE, L2_MISC_ALLOC_GROW_SIZE)

init_layer1_obj_arr(dtt_year, L1_DTT_YEAR_ARR_SIZE)
init_layer2_obj_arr(dtt_year, L2_DTT_YEAR_INIT_SIZE)
add_obj_to_layer2_arr(dtt_year, dtt_year, L2_DTT_YEAR_GROW_SIZE, L1_DTT_YEAR_ARR_SIZE)
del_obj_from_layer2_arr(dtt_year, dtt_year, L1_DTT_YEAR_ARR_SIZE)
get_obj_from_layer2_arr(dtt_year, dtt_year, L1_DTT_YEAR_ARR_SIZE)
get_l1_obj_from_layer2_arr(dtt_year)
del_l2_obj_arr(dtt_year, L2_DTT_YEAR_INIT_SIZE, L2_DTT_YEAR_GROW_SIZE)

init_layer1_obj_arr(sect_field, L1_SECT_FIELD_ARR_SIZE)
init_layer2_obj_arr(sect_field, L2_SECT_FIELD_INIT_SIZE)
add_obj_to_layer2_arr(sect_field, sect_field, L2_SECT_FIELD_GROW_SIZE, L1_SECT_FIELD_ARR_SIZE)
del_obj_from_layer2_arr(sect_field, sect_field, L1_SECT_FIELD_ARR_SIZE)
get_obj_from_layer2_arr(sect_field, sect_field, L1_SECT_FIELD_ARR_SIZE)
get_l1_obj_from_layer2_arr(sect_field)
del_l2_obj_arr(sect_field, L2_SECT_FIELD_INIT_SIZE, L2_SECT_FIELD_GROW_SIZE)

// clear link
int link_entry_clear(linked_entry* tar) {
    if (!tar) {
        return WRONG_ARGS;
    }

    tar->link_prev = 0;
    tar->link_next = 0;

    return 0;
}

// add to tail
int link_entry_tail(linked_entry* dst, linked_entry* tar) {
    linked_entry* tmp;

    if (!dst || !tar) {
        return WRONG_ARGS;
    }

    if (dst == tar) {
        return 0;
    }

    tmp = dst;
    while (tmp->link_next) {
        tmp = tmp->link_next;
    }

    tmp->link_next = tar;

    tar->link_prev = tmp;
    tar->link_next = 0;

    return 0;
}

// add to head
int link_entry_head(linked_entry* dst, linked_entry* tar) {
    linked_entry* tmp;

    if (!dst || !tar) {
        return WRONG_ARGS;
    }

    if (dst == tar) {
        return 0;
    }

    tmp = dst;
    while (tmp->link_prev) {
        tmp = tmp->link_prev;
    }

    tmp->link_prev = tar;

    tar->link_prev = 0;
    tar->link_next = tmp;

    return 0;
}

// add after
int link_entry_after(linked_entry* dst, linked_entry* tar) {
    linked_entry* tmp;

    if (!dst || !tar) {
        return WRONG_ARGS;
    }

    if (dst == tar) {
        return 0;
    }

    tmp = dst->link_next;

    dst->link_next = tar;

    if (tmp) {
        tmp->link_prev = tar;
    }

    tar->link_prev = dst;
    tar->link_next = tmp;

    return 0;
}

// add before
int link_entry_before(linked_entry* dst, linked_entry* tar) {
    linked_entry* tmp;

    if (!dst || !tar) {
        return WRONG_ARGS;
    }

    if (dst == tar) {
        return 0;
    }

    tmp = dst->link_prev;

    if (!tmp) {
        tmp->link_next = tar;
    }
    dst->link_prev = tar;

    tar->link_prev = tmp;
    tar->link_next = dst;

    return 0;
}

// get head
int link_entry_get_head(linked_entry* tar, linked_entry** ret) {
    linked_entry* tmp;

    if (!tar || !ret) {
        return WRONG_ARGS;
    }

    tmp = tar;
    while (tmp->link_prev) {
        tmp = tmp->link_prev;
    }

    *ret = tmp;

    return 0;
}

// get tail
int link_entry_get_tail(linked_entry* tar, linked_entry** ret) {
    linked_entry* tmp;

    if (!tar || !ret) {
        return WRONG_ARGS;
    }

    tmp = tar;
    while (tmp->link_next) {
        tmp = tmp->link_next;
    }

    *ret = tmp;

    return 0;
}

int init_database_handle(database_handle* dh) {
    bit_index temp_index;

    int ret;

    t_eid_to_e* tree_eid_to_e;

    dh->unsaved = 0;

    mem_wipe_sec(&dh->name, FILE_NAME_MAX);

    dh->eid_to_e = 0;
    init_layer2_eid_to_e_arr(&dh->l2_eid_to_e_arr);
    init_eid_exist_mat(&dh->eid_mat);

    mem_wipe_sec(&dh->tree, sizeof(linked_entry));
    dh->tree.type = ENTRY_GROUP;
    add_eid_to_e_to_layer2_arr(&dh->l2_eid_to_e_arr, &tree_eid_to_e, &temp_index);
    init_eid_to_e(tree_eid_to_e);
    bytes_to_hex_str(dh->tree.entry_id_str, dh->tree.entry_id, EID_LEN);
    bytes_to_hex_str(tree_eid_to_e->str, dh->tree.entry_id, EID_LEN);
    tree_eid_to_e->tar = &dh->tree;
    add_eid_to_e_to_htab(&dh->eid_to_e, tree_eid_to_e);
    dh->tree.eid_to_e = tree_eid_to_e;
    add_eid_to_eid_exist_mat(&dh->eid_mat, tree_eid_to_e->str, temp_index);
    dh->tree.depth = 0;

    dh->fn_to_e = 0;
    init_layer2_fn_to_e_arr(&dh->l2_fn_to_e_arr);
    init_fn_exist_mat(&dh->fn_mat);

    dh->tag_to_e = 0;
    init_layer2_tag_to_e_arr(&dh->l2_tag_to_e_arr);
    init_tag_exist_mat(&dh->tag_mat);

    /* file data */
    // sha1
    dh->sha1f_to_fd = 0;
    init_layer2_sha1f_to_fd_arr(&dh->l2_sha1f_to_fd_arr);
    init_sha1f_exist_mat(&dh->sha1f_mat);
    // sha256
    dh->sha256f_to_fd = 0;
    init_layer2_sha256f_to_fd_arr(&dh->l2_sha256f_to_fd_arr);
    init_sha256f_exist_mat(&dh->sha256f_mat);
    // sha512
    dh->sha512f_to_fd = 0;
    init_layer2_sha512f_to_fd_arr(&dh->l2_sha512f_to_fd_arr);
    init_sha512f_exist_mat(&dh->sha512f_mat);

    /* section */
    // sha1
    dh->sha1s_to_s = 0;
    init_layer2_sha1s_to_s_arr(&dh->l2_sha1s_to_s_arr);
    init_sha1s_exist_mat(&dh->sha1s_mat);
    // sha256
    dh->sha256s_to_s = 0;
    init_layer2_sha256s_to_s_arr(&dh->l2_sha256s_to_s_arr);
    init_sha256s_exist_mat(&dh->sha256s_mat);
    // sha512
    dh->sha512s_to_s = 0;
    init_layer2_sha512s_to_s_arr(&dh->l2_sha512s_to_s_arr);
    init_sha512s_exist_mat(&dh->sha512s_mat);

    dh->f_size_to_fd = 0;
    init_layer2_f_size_to_fd_arr(&dh->l2_f_size_to_fd_arr);
    init_f_size_exist_mat(&dh->f_size_mat);

    mem_wipe_sec(&dh->tod_dt_tree,  sizeof(dtt_year*));
    mem_wipe_sec(&dh->tusr_dt_tree, sizeof(dtt_year*));

    // pool allocatr structure
    // entry
    ret = init_layer2_entry_arr(&dh->l2_entry_arr);
    if (ret) {
        return ret;
    }
    ret = init_layer2_file_data_arr(&dh->l2_file_data_arr);
    if (ret) {
        return ret;
    }
    ret = init_layer2_section_arr(&dh->l2_section_arr);
    if (ret) {
        return ret;
    }
    ret = init_layer2_misc_alloc_record_arr(&dh->l2_misc_alloc_record_arr);
    if (ret) {
        return ret;
    }
    ret = init_layer2_dtt_year_arr(&dh->l2_dtt_year_arr);
    if (ret) {
        return ret;
    }

    return 0;
}

int fres_database_handle(database_handle* dh) {
    int i, ret;

    misc_alloc_record* temp_alloc_record;

    // free allocations recorded in misc alloc record
    i = 0;
    ret = 0;
    while (1) {
        // assumes sequential allocation in layer 2 array
        ret = get_misc_alloc_record_from_layer2_arr(&dh->l2_misc_alloc_record_arr, &temp_alloc_record, i);

        if (ret != 0) {
            break;
        }

        free(temp_alloc_record->ptr);

        i++;
    }

    // delete(free) all database layer 2 arrays
    // translation objects
    del_l2_eid_to_e_arr             (   &dh->l2_eid_to_e_arr            );
    del_l2_fn_to_e_arr              (   &dh->l2_fn_to_e_arr             );
    del_l2_tag_to_e_arr             (   &dh->l2_tag_to_e_arr            );
    del_l2_sha1f_to_fd_arr          (   &dh->l2_sha1f_to_fd_arr         );
    del_l2_sha256f_to_fd_arr        (   &dh->l2_sha256f_to_fd_arr       );
    del_l2_sha512f_to_fd_arr        (   &dh->l2_sha512f_to_fd_arr       );
    del_l2_sha1s_to_s_arr           (   &dh->l2_sha1s_to_s_arr          );
    del_l2_sha256s_to_s_arr         (   &dh->l2_sha256s_to_s_arr        );
    del_l2_sha512s_to_s_arr         (   &dh->l2_sha512s_to_s_arr        );
    del_l2_f_size_to_fd_arr         (   &dh->l2_f_size_to_fd_arr        );

    // general objects
    del_l2_entry_arr                (   &dh->l2_entry_arr               );
    del_l2_file_data_arr            (   &dh->l2_file_data_arr           );
    del_l2_misc_alloc_record_arr    (   &dh->l2_misc_alloc_record_arr   );
    del_l2_section_arr              (   &dh->l2_section_arr             );
    del_l2_dtt_year_arr             (   &dh->l2_dtt_year_arr            );

    return 0;
}

int ffp_grow_bitmap (simple_bitmap* map, bit_index length) {
    map_block* temp_map_raw;

    if (length > map->length) {
        if (        get_bitmap_map_block_number(length)
                !=  get_bitmap_map_block_number(map->length)
           )
        {
            temp_map_raw =
                realloc(
                        map->base,
                        get_bitmap_map_block_number(length)
                        * sizeof(map_block)
                       );
            if (!temp_map_raw) {
                return MALLOC_FAIL;
            }

            bitmap_grow(map, temp_map_raw, 0, length, 0);
        }
        else {
            bitmap_grow(map, NULL, 0, length, 0);
        }
    }

    return 0;
}
