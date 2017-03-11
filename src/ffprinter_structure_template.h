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

#define typedefs_trans(tag_attr, tag_target) \
    typedef struct t_##tag_attr##_to_##tag_target t_##tag_attr##_to_##tag_target; \
    typedef struct tag_attr##_exist_mat tag_attr##_exist_mat; \
    typedef struct layer1_##tag_attr##_to_##tag_target##_arr layer1_##tag_attr##_to_##tag_target##_arr; \
    typedef struct layer2_##tag_attr##_to_##tag_target##_arr layer2_##tag_attr##_to_##tag_target##_arr;

#define generic_trans_struct_one_to_one(tag_attr, tag_target, target_type, STR_LEN) \
    struct t_##tag_attr##_to_##tag_target { \
        char str[(STR_LEN) + 1];            \
        str_len_int str_len;                \
                                            \
        target_type * tar;                  \
                                            \
        obj_meta_data_fields;               \
                                            \
        UT_hash_handle hh;                  \
    };

#define generic_trans_struct_one_to_many(tag_attr, tag_target, target_type, STR_LEN) \
    struct t_##tag_attr##_to_##tag_target { \
        char str[(STR_LEN) + 1];            \
        str_len_int str_len;                \
                                            \
        uint32_t number;                    \
                                            \
        target_type * head_tar;             \
        target_type * tail_tar;             \
                                            \
        obj_meta_data_fields;               \
                                            \
        UT_hash_handle hh;                  \
    };

#define generic_exist_mat(tag_attr, LEN) \
    struct tag_attr##_exist_mat {           \
        uniq_char_map* uniq_char[(LEN)];    \
        uint16_t max_length;                \
    };

#define obj_meta_data_fields \
    bit_index obj_arr_index;

#define typedefs_obj_arr(obj_name) \
    typedef struct layer1_##obj_name##_arr layer1_##obj_name##_arr; \
    typedef struct layer2_##obj_name##_arr layer2_##obj_name##_arr;

#define layer1_obj_arr(obj_name, obj_type, L1_ARR_SIZE) \
    struct layer1_##obj_name##_arr {                                            \
        obj_type arr[(L1_ARR_SIZE)];                                            \
        simple_bitmap usage_map;                                                \
        map_block usage_map_raw[get_bitmap_map_block_number((L1_ARR_SIZE))];    \
    };

#define layer2_obj_arr(obj_name) \
    struct layer2_##obj_name##_arr {          \
        layer1_##obj_name##_arr ** l1_arr;    \
        simple_bitmap l1_arr_map;             \
    };

#define layer1_generic_arr(tag_attr, tag_target, L1_ARR_SIZE) \
    layer1_obj_arr(tag_attr##_to_##tag_target, t_##tag_attr##_to_##tag_target, L1_ARR_SIZE)

#define layer2_generic_arr(tag_attr, tag_target) \
    layer2_obj_arr(tag_attr##_to_##tag_target)
