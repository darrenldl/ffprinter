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

#define lookup_generic_one_to_one_tran(tag_attr, tag_target, target_name, result_type) \
    int lookup_##target_name (t_##tag_attr##_to_##tag_target* htab, const char* str, result_type ** result) { \
        t_##tag_attr##_to_##tag_target * temp_tran_find;                    \
                                                                            \
        HASH_FIND_STR(htab, str, temp_tran_find);                           \
        if (!temp_tran_find) {                                              \
            *result = 0;                                                    \
            return FIND_FAIL;                                               \
        }                                                                   \
                                                                            \
        *result = temp_tran_find->tar;                                      \
        return 0;                                                           \
    }

#define lookup_generic_one_to_one_tran_via_dh(dh_type, tag_attr, tag_target, target_name, result_type) \
    lookup_generic_one_to_one_tran(tag_attr, tag_target, target_name, result_type) \
    int lookup_##target_name##_via_dh (dh_type* dh, const char* str, result_type ** result) { \
        return lookup_##target_name(dh->tag_attr##_to_##tag_target, str, result);   \
    }

#define lookup_generic_one_to_many_tran(tag_attr, tag_target, target_name, result_type) \
    int lookup_##target_name (t_##tag_attr##_to_##tag_target* htab, const char* str, result_type ** result) { \
        t_##tag_attr##_to_##tag_target * temp_tran_find;                    \
                                                                            \
        HASH_FIND_STR(htab, str, temp_tran_find);                           \
        if (!temp_tran_find) {                                              \
            *result = 0;                                                    \
            return FIND_FAIL;                                               \
        }                                                                   \
                                                                            \
        *result = temp_tran_find->head_tar;                                 \
        return 0;                                                           \
    }

#define lookup_generic_one_to_many_tran_via_dh(dh_type, tag_attr, tag_target, target_name, result_type) \
    lookup_generic_one_to_many_tran(tag_attr, tag_target, target_name, result_type) \
    int lookup_##target_name##_via_dh (dh_type* dh, const char* str, result_type ** result) { \
        return lookup_##target_name(dh->tag_attr##_to_##tag_target, str, result);   \
    }

#define lookup_generic_part_ranged(tag_attr, tag_target, target_name, STR_MAX_LEN) \
    int lookup_##target_name##_part_ranged (layer2_##tag_attr##_to_##tag_target##_arr* l2_arr, tag_attr##_exist_mat* matrix, const char* str_part, int16_t start_pos_min, int16_t start_pos_max, simple_bitmap* map_buf, simple_bitmap* map_result, t_##tag_attr##_to_##tag_target ** result_buf, bit_index buf_size, bit_index* num_used) { \
        int i, j;  /* used for counting */                                                      \
                                                                                                \
        bit_index temp_index_skip_to;                                                           \
        bit_index temp_index_result;                                                            \
                                                                                                \
        bit_index temp_index_skip_to2;                                                          \
        bit_index temp_index_result2;                                                           \
                                                                                                \
        bit_index temp_num_used = 0;                                                            \
                                                                                                \
        uint16_t start_pos_lower_bound; /* inclusive */                                         \
        uint16_t start_pos_upper_bound; /* inclusive */                                         \
                                                                                                \
        str_len_int str_part_len;                                                               \
                                                                                                \
        layer1_##tag_attr##_to_##tag_target##_arr* temp_l1_arr = NULL;                          \
                                                                                                \
        if (!num_used) {                                                                        \
            return WRONG_ARGS;                                                                  \
        }                                                                                       \
                                                                                                \
        /* num_used defaults to 0 */                                                            \
        *num_used = 0;                                                                          \
                                                                                                \
        if ((i = verify_str_terminated(str_part, STR_MAX_LEN, &str_part_len, 0))) {             \
            return i;                                                                           \
        }                                                                                       \
                                                                                                \
        if (buf_size == 0) {    /* should never get a zero size buffer */                       \
            return WRONG_ARGS;                                                                  \
        }                                                                                       \
                                                                                                \
        /* look up existence matrix */                                                          \
        /* check if passed maximum length recorded */                                           \
        if (str_part_len > matrix->max_length) {                                                \
            return 0;                                                                           \
        }                                                                                       \
                                                                                                \
        /* calculate actual lower and upper bound of start position */                          \
        if (start_pos_min < 0) {                                                                \
            start_pos_lower_bound = 0;                                                          \
        }                                                                                       \
        else {                                                                                  \
            start_pos_lower_bound = start_pos_min;                                              \
        }                                                                                       \
                                                                                                \
        if (start_pos_max < 0) {                                                                \
            start_pos_upper_bound = matrix->max_length - str_part_len;                          \
        }                                                                                       \
        else {                                                                                  \
            start_pos_upper_bound = start_pos_max;                                              \
        }                                                                                       \
                                                                                                \
        if (start_pos_lower_bound > start_pos_upper_bound) {                                    \
            return WRONG_ARGS;                                                                  \
        }                                                                                       \
                                                                                                \
        if (matrix->max_length - start_pos_lower_bound < str_part_len) {                        \
            return WRONG_ARGS;                                                                  \
        }                                                                                       \
                                                                                                \
        if (matrix->max_length - start_pos_upper_bound < str_part_len) {                        \
            return WRONG_ARGS;                                                                  \
        }                                                                                       \
                                                                                                \
        lookup_##target_name##_part_map_ranged(matrix, str_part, start_pos_min, start_pos_max, map_buf, map_result);\
                                                                                                \
        /* for the index that contains all the characters, add them to result buffer */         \
        for (i = 0, temp_index_skip_to = 0; i < map_result->number_of_ones; i++) {              \
            /* get index of L1 array that contains the entries with all matching characters */  \
            bitmap_first_one_bit_index(map_result, &temp_index_result, temp_index_skip_to);     \
            temp_index_skip_to = temp_index_result + 1;                                         \
                                                                                                \
            /* get the L1 array */                                                              \
            get_l1_##tag_attr##_to_##tag_target##_from_layer2_arr(l2_arr, &temp_l1_arr, temp_index_result);\
                                                                                                \
            /* go through the L1 array to find entries of partial matching string */            \
            for (j = 0, temp_index_skip_to2 = 0; j < temp_l1_arr->usage_map.number_of_ones; j++) {\
                bitmap_first_one_bit_index(&temp_l1_arr->usage_map, &temp_index_result2, temp_index_skip_to2);\
                temp_index_skip_to2 = temp_index_result2 + 1;                                   \
                                                                                                \
                if (match_sub_min_max(temp_l1_arr->arr[temp_index_result2].str, str_part, start_pos_min, start_pos_max)) { /* if entry str contains str_part as a substring, then add to buffer */\
                    result_buf[temp_num_used] = temp_l1_arr->arr + temp_index_result2;          \
                    temp_num_used++;                                                            \
                }                                                                               \
                \
                if (temp_num_used == buf_size) {  /* buf_size is guaranteed to be larger than zero at this point, due to input check */\
                    *num_used = temp_num_used;                                                  \
                    return BUFFER_FULL; /* not necessarily an error, but let the caller know anyway */\
                }                                                                               \
            }                                                                                   \
        }                                                                                       \
                                                                                                \
        *num_used = temp_num_used;                                                              \
        return 0;                                                                               \
    }

#define lookup_generic_part(tag_attr, tag_target, target_name, STR_MAX_LEN) \
    lookup_generic_part_ranged(tag_attr, tag_target, target_name, STR_MAX_LEN) \
    int lookup_##target_name##_part (layer2_##tag_attr##_to_##tag_target##_arr* l2_arr, tag_attr##_exist_mat* matrix, const char* str_part, simple_bitmap* map_buf, simple_bitmap* map_result, t_##tag_attr##_to_##tag_target ** result_buf, bit_index buf_size, bit_index* num_used) { \
        return lookup_##target_name##_part_ranged(l2_arr, matrix, str_part, -1, -1, map_buf, map_result, result_buf, buf_size, num_used); \
    }

#define lookup_generic_part_via_dh(dh_type, tag_attr, tag_target, target_name, STR_MAX_LEN) \
    lookup_generic_part(tag_attr, tag_target, target_name, STR_MAX_LEN) \
    int lookup_##target_name##_part_via_dh (dh_type* dh, const char* str_part, simple_bitmap* map_buf, simple_bitmap* map_result, t_##tag_attr##_to_##tag_target ** result_buf, bit_index buf_size, bit_index* num_used) { \
        return lookup_##target_name##_part(&dh->l2_##tag_attr##_to_##tag_target##_arr, &dh->tag_attr##_mat, str_part, map_buf, map_result, result_buf, buf_size, num_used);    \
    }

#define lookup_generic_part_map_ranged(tag_attr, tag_target, target_name, STR_MAX_LEN) \
    int lookup_##target_name##_part_map_ranged (tag_attr##_exist_mat* matrix, const char* str_part, int16_t start_pos_min, int16_t start_pos_max, simple_bitmap* map_buf, simple_bitmap* map_result) { \
        int i, j;  /* used for counting */                                                      \
                                                                                                \
        uniq_char_map* temp_uniq_char;                                                          \
                                                                                                \
        simple_bitmap start_pos_map;                                                            \
        map_block raw_start_pos_map[get_bitmap_map_block_number(STR_MAX_LEN)];                  \
                                                                                                \
        bit_index temp_index_skip_to;                                                           \
        bit_index temp_index_result;                                                            \
                                                                                                \
        bit_index bitmap_old_len;                                                               \
        bit_index temp_map_length;                                                              \
                                                                                                \
        uint16_t start_pos_lower_bound; /* inclusive */                                         \
        uint16_t start_pos_upper_bound; /* inclusive */                                         \
                                                                                                \
        str_len_int str_part_len;                                                               \
                                                                                                \
        if ((i = verify_str_terminated(str_part, STR_MAX_LEN, &str_part_len, 0))) {             \
            return i;                                                                           \
        }                                                                                       \
                                                                                                \
        /* default result map to all 0 */                                                       \
        bitmap_zero(map_result);                                                                \
                                                                                                \
        /* look up existence matrix */                                                          \
        /* check if passed maximum length recorded */                                           \
        if (str_part_len > matrix->max_length) {                                                \
            return 0;                                                                           \
        }                                                                                       \
                                                                                                \
        /* calculate actual lower and upper bound of start position */                          \
        if (start_pos_min < 0) {                                                                \
            start_pos_lower_bound = 0;                                                          \
        }                                                                                       \
        else {                                                                                  \
            start_pos_lower_bound = start_pos_min;                                              \
        }                                                                                       \
                                                                                                \
        if (start_pos_max < 0) {                                                                \
            start_pos_upper_bound = matrix->max_length - str_part_len;                          \
        }                                                                                       \
        else {                                                                                  \
            start_pos_upper_bound = start_pos_max;                                              \
        }                                                                                       \
                                                                                                \
        if (start_pos_lower_bound > start_pos_upper_bound) {                                    \
            return WRONG_ARGS;                                                                  \
        }                                                                                       \
                                                                                                \
        if (matrix->max_length - start_pos_lower_bound < str_part_len) {                        \
            return WRONG_ARGS;                                                                  \
        }                                                                                       \
                                                                                                \
        if (matrix->max_length - start_pos_upper_bound < str_part_len) {                        \
            return WRONG_ARGS;                                                                  \
        }                                                                                       \
                                                                                                \
        /* mark down starting positions of all existence */                                     \
        bitmap_init(&start_pos_map, raw_start_pos_map, 0, STR_MAX_LEN, 0);                      \
        for (i = start_pos_lower_bound; i <= start_pos_upper_bound; i++) {                      \
            if (!matrix->uniq_char[i]) {    /* empty position */                                \
                continue;                                                                       \
            }                                                                                   \
            else {                                                                              \
                /* try to match first character first */                                        \
                temp_uniq_char = matrix->uniq_char[i];                                          \
                while (temp_uniq_char) {                                                        \
                    if (temp_uniq_char->character == str_part[0]) {                             \
                        break;                                                                  \
                    }                                                                           \
                    temp_uniq_char = temp_uniq_char->next;                                      \
                }                                                                               \
                if (!temp_uniq_char) {  /* no match found */                                    \
                    continue;                                                                   \
                }                                                                               \
                else {  /* match found, try to match remaining characters */                    \
                    for (j = 1; j < str_part_len; j++) {                                        \
                        temp_uniq_char = matrix->uniq_char[i + j];                              \
                        while (temp_uniq_char) {                                                \
                            if (temp_uniq_char->character == str_part[j]) {                     \
                                break;                                                          \
                            }                                                                   \
                            temp_uniq_char = temp_uniq_char->next;                              \
                        }                                                                       \
                        if (!temp_uniq_char) {  /* no match */                                  \
                            break;                                                              \
                        }                                                                       \
                    }                                                                           \
                    if (j == str_part_len) {    /* found match for all remaining characters */  \
                        /* mark that this is a valid starting position */                       \
                        bitmap_write(&start_pos_map, i, 1);                                     \
                    }                                                                           \
                }                                                                               \
            }                                                                                   \
        }                                                                                       \
        if (start_pos_map.number_of_ones == 0) {     /* found nothing */                        \
            return 0;                                                                           \
        }                                                                                       \
                                                                                                \
        /* for each starting position found, go through the matrix */                           \
        for (i = 0, temp_index_skip_to = 0; i < start_pos_map.number_of_ones; i++) {            \
            bitmap_first_one_bit_index(&start_pos_map, &temp_index_result, temp_index_skip_to); \
            temp_index_skip_to = temp_index_result + 1;                                         \
                                                                                                \
            temp_uniq_char = matrix->uniq_char[temp_index_result];                              \
            /* look for the uniq_char of the starting position again */                         \
            while (temp_uniq_char) {                                                            \
                if (temp_uniq_char->character == str_part[0]) {                                 \
                    break;                                                                      \
                }                                                                               \
                temp_uniq_char = temp_uniq_char->next;                                          \
            }                                                                                   \
            if (!temp_uniq_char) {  /* should never occur, as it should exist */                \
                return LOGIC_ERROR;                                                             \
            }                                                                                   \
                                                                                                \
            BACKUP_INTERRUPTABLE_FLAG();                                                        \
                                                                                                \
            SET_NOT_INTERRUPTABLE();                                                            \
                                                                                                \
            temp_map_length = temp_uniq_char->l1_arr_map.length;                                \
            ffp_grow_bitmap(map_buf, temp_map_length);                                          \
                                                                                                \
            REVERT_INTERRUPTABLE_FLAG();                                                        \
                                                                                                \
            /* copy bitmap of first character to map buffer */                                  \
            bitmap_copy(&temp_uniq_char->l1_arr_map, map_buf, 0, 0);                            \
                                                                                                \
            for (j = 1; j < str_part_len; j++) {                                                \
                temp_uniq_char = matrix->uniq_char[temp_index_result + j];                      \
                while (temp_uniq_char) {                                                        \
                    if (temp_uniq_char->character == str_part[j]) {                             \
                        break;                                                                  \
                    }                                                                           \
                    temp_uniq_char = temp_uniq_char->next;                                      \
                }                                                                               \
                if (!temp_uniq_char) { /* should exist */                                       \
                    return LOGIC_ERROR;                                                         \
                }                                                                               \
                                                                                                \
                BACKUP_INTERRUPTABLE_FLAG();                                                    \
                                                                                                \
                SET_NOT_INTERRUPTABLE();                                                        \
                                                                                                \
                temp_map_length = temp_uniq_char->l1_arr_map.length;                            \
                bitmap_old_len = map_buf->length;                                               \
                                                                                                \
                if (temp_map_length > map_buf->length) {                                        \
                    ffp_grow_bitmap(map_buf, temp_map_length);                                  \
                }                                                                               \
                else if (temp_map_length == map_buf->length) {                                  \
                    ;;  /* do nothing */                                                        \
                }                                                                               \
                else /* temp_map_length < map_buf->length */ {                                  \
                    bitmap_shrink(map_buf, NULL, temp_map_length);                              \
                }                                                                               \
                                                                                                \
                bitmap_and(&temp_uniq_char->l1_arr_map, map_buf, map_buf, 0);                   \
                                                                                                \
                if (bitmap_old_len > map_buf->length) {                                         \
                    bitmap_grow(map_buf, NULL, NULL, bitmap_old_len, 0);                        \
                }                                                                               \
                                                                                                \
                REVERT_INTERRUPTABLE_FLAG();                                                    \
            }                                                                                   \
                                                                                                \
            /* lay the map buffer over the result buffer */                                     \
            BACKUP_INTERRUPTABLE_FLAG();                                                        \
                                                                                                \
            SET_NOT_INTERRUPTABLE();                                                            \
                                                                                                \
            ffp_grow_bitmap(map_result, map_buf->length);                                       \
                                                                                                \
            REVERT_INTERRUPTABLE_FLAG();                                                        \
                                                                                                \
            bitmap_or(map_buf, map_result, map_result, 0);                                      \
        }                                                                                       \
                                                                                                \
        return 0;                                                                               \
    }

#define lookup_generic_part_map(tag_attr, tag_target, target_name, STR_MAX_LEN) \
    lookup_generic_part_map_ranged(tag_attr, tag_target, target_name, STR_MAX_LEN) \
    int lookup_##target_name##_part_map (tag_attr##_exist_mat* matrix, const char* str_part, simple_bitmap* map_buf, simple_bitmap* map_result) { \
        return lookup_##target_name##_part_map_ranged(matrix, str_part, -1, -1, map_buf, map_result); \
    }

#define lookup_generic_part_map_via_dh(dh_type, tag_attr, tag_target, target_name, STR_MAX_LEN) \
    lookup_generic_part_map(tag_attr, tag_target, target_name, STR_MAX_LEN) \
    int lookup_##target_name##_part_map_via_dh (dh_type* dh, const char* str_part, simple_bitmap* map_buf, simple_bitmap* map_result) { \
        return lookup_##target_name##_part_map(&dh->tag_attr##_mat, str_part, map_buf, map_result);    \
    }
