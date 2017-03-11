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

/*
 *  Notes on addition and deletion in layer 2 array and existence matrix:
 *      The two structures are not tightly coupled together
 *
 *      That is, adding to layer 2 array does not automatically add to
 *      existence matrix, and vice versa
 *
 *      Similarly, deleting from layer 2 array does not automatically delete
 *      from existence matrix, and vice versa
 *
 *      However, in deletion from existence matrix, a lookup of the index
 *      is performed in the layer 2 array to obtain the entry str
 *
 *      To add an entry to both structures, call functions in the following order
 *          add to layer 2 array
 *          add to existence matrix
 *              (remember to pass index_in_arr returned via pointer
 *              by add to layer 2 array function)
 *
 *          You MUST have added the entry to layer 2 array before you can add to matrix
 *          Otherwise deletion from matrix later on will fail, as it does a lookup of the index
 *          in layer 2 array
 *
 *      To delete an entry completely, call functions in the following order
 *          delete from existence matrix
 *          delelete from layer2 array
 *          (both uses index in array to delete)
 *
 *          You CAN choose to just delete from existence matrix
 *          The entry will still exist in layer 2 array,
 *          But may create weird results when looking up
 *          See below for exact behaviour on different cases
 *          The following described behaviour is tested in unit test :
 *              test_possibly_wonky_deletion - unit_tests/test_template.c
 *
 *          If the entry being deleted is the only entry of (partial) matching string
 *          in the layer 1 array it is residing in,
 *          then deletion will cause the layer 1 array to be marked as containing
 *          NO matching entries in the bitmap in uniq_char structure
 *          And thus
 *          [Test area 1 in test_possibly_wonky_deletion]
 *          - For buffer version :
 *              Further lookups will fail, as the content of the layer 1 array
 *              is not inspected, due to being marked as non-matching in map
 *          - For map version :
 *              Further lookups will simply mark the layer 1 array as NOT containing
 *              matching entry
 *
 *          If the entry being deleted is NOT the only entry of (partial) matching string
 *          in the layer 1 array it is residing in,
 *          then deletion will cause the layer 1 array to REMAIN marked as
 *          containing matching entries(at least 1 entry)
 *          And thus
 *          [Test area 2 in test_possibly_wonky_deletion]
 *          - For buffer version :
 *              Further lookups will still inspect the content of the layer 1 array
 *              and revealing the entry that is deleted from existence matrix, but not from
 *              layer 2 array
 *          - For map version :
 *              Further lookups will still mark the layer 1 array as containing
 *              matching entry
 *
 *          Thus, depending on the way you look up things(i.e. will the partial string
 *          only return unique entry - well if it only returns unique entry you should use
 *          a hash table),
 *          the way you add entries(i.e. will there be multiple entries with common sub-string
 *          in any of the layer 1 array)
 *          and the nature of entry(i.e. string are unique or not),
 *          you may or may not always want to delete one entry from both structures
 */

 /* helper functions for use in templates */
#define fft_int_div_round_up(a, b) ((a) % (b) == 0 ? ((a) / (b) + 1) : (((a) + (b) - 1) / (b)))

#define link_to_tran(tar_p, tran_p) \
    strcpy(tran_p->str, (tar_p)->id);       \
    tran_p->head_tar = tar_p;               \
    tran_p->tail_tar = tar_p;               \
    tran_p->str_len = strlen((tar_p)->id);

#define init_obj_meta_data(obj_name, obj_type) \
    int init_##obj_name##_meta_data (obj_type * obj) { \
        if (!obj) {                 \
            return WRONG_ARGS;      \
        }                           \
                                    \
        obj->obj_arr_index = 0;     \
                                    \
        return 0;                   \
    }

#define init_generic_trans_struct_one_to_one(tag_attr, tag_target) \
    int init_##tag_attr##_to_##tag_target (t_##tag_attr##_to_##tag_target * tar) { \
        if (!tar) {                                                         \
            return WRONG_ARGS;                                              \
        }                                                                   \
                                                                            \
        tar->str[0] = 0;                                                    \
        tar->tar = 0;                                                       \
                                                                            \
        return 0;                                                           \
    }

#define verify_generic_trans_struct_one_to_one(tag_attr, tag_target, STR_MAX_LEN) \
    int verify_##tag_attr##_to_##tag_target (t_##tag_attr##_to_##tag_target * tar, int * error_code, uint32_t flags) { \
        str_len_int str_len;                                                \
                                                                            \
        if (!tar) {                                                         \
            return WRONG_ARGS;                                              \
        }                                                                   \
                                                                            \
        if (verify_str_terminated(tar->str, STR_MAX_LEN, &str_len, 0)) {    \
            if (error_code) {                                               \
                *error_code = VERIFY_STR_NOT_TERMINATED;                    \
            }                                                               \
            return VERIFY_FAIL;                                             \
        }                                                                   \
                                                                            \
        if (str_len != tar->str_len) {                                      \
            if (error_code) {                                               \
                *error_code = VERIFY_WRONG_STR_LEN;                         \
            }                                                               \
            return VERIFY_FAIL;                                             \
        }                                                                   \
                                                                            \
        return 0;                                                           \
    }

#define init_generic_trans_struct_one_to_many(tag_attr, tag_target) \
    int init_##tag_attr##_to_##tag_target (t_##tag_attr##_to_##tag_target * tar) { \
        if (!tar) {                                                         \
            return WRONG_ARGS;                                              \
        }                                                                   \
                                                                            \
        tar->str[0] = 0;                                                    \
        tar->number = 0;                                                    \
        tar->head_tar = 0;                                                  \
        tar->tail_tar = 0;                                                  \
                                                                            \
        return 0;                                                           \
    }

#define verify_generic_trans_struct_one_to_many(tag_attr, tag_target, tag_prev, tag_next, target_type, STR_MAX_LEN) \
    int verify_##tag_attr##_to_##tag_target (t_##tag_attr##_to_##tag_target * tar, int * error_code, uint32_t flags) { \
        uint64_t u64i;                                                      \
        str_len_int str_len;                                                \
        target_type * temp_entry;                                           \
                                                                            \
        if (!tar) {                                                         \
            return WRONG_ARGS;                                              \
        }                                                                   \
                                                                            \
        if (verify_str_terminated(tar->str, STR_MAX_LEN, &str_len, 0)) {    \
            if (error_code) {                                               \
                *error_code = VERIFY_STR_NOT_TERMINATED;                    \
            }                                                               \
            return VERIFY_FAIL;                                             \
        }                                                                   \
                                                                            \
        if (str_len != tar->str_len) {                                      \
            if (error_code) {                                               \
                *error_code = VERIFY_WRONG_STR_LEN;                         \
            }                                                               \
            return VERIFY_FAIL;                                             \
        }                                                                   \
                                                                            \
        if (tar->number > 0) {                                              \
            if (!tar->head_tar) {                                           \
                if (error_code) {                                           \
                    *error_code = VERIFY_MISSING_HEAD;                      \
                }                                                           \
                return VERIFY_FAIL;                                         \
            }                                                               \
            if (!tar->tail_tar) {                                           \
                if (error_code) {                                           \
                    *error_code = VERIFY_MISSING_TAIL;                      \
                }                                                           \
                return VERIFY_FAIL;                                         \
            }                                                               \
        }                                                                   \
                                                                            \
        if (flags & GO_THROUGH_CHAIN && tar->number > 0) {                  \
            /* forward direction */                                         \
            temp_entry = tar->head_tar;                                     \
            for (u64i = 1; u64i < tar->number && temp_entry; u64i++) {      \
                temp_entry = temp_entry->tag_next;                          \
            }                                                               \
            if (temp_entry != tar->tail_tar) {                              \
                if (error_code) {                                           \
                    *error_code = VERIFY_BROKEN_FORWARD_LINK;               \
                }                                                           \
                return VERIFY_FAIL;                                         \
            }                                                               \
            if (u64i != tar->number) {                                      \
                if (error_code) {                                           \
                    *error_code = VERIFY_WRONG_FORWARD_STAT;                \
                }                                                           \
                return VERIFY_FAIL;                                         \
            }                                                               \
            /* backward direction */                                        \
            temp_entry = tar->tail_tar;                                     \
            for (u64i = 1; u64i < tar->number && temp_entry; u64i++) {      \
                temp_entry = temp_entry->tag_prev;                          \
            }                                                               \
            if (temp_entry != tar->head_tar) {                              \
                if (error_code) {                                           \
                    *error_code = VERIFY_BROKEN_BACKWARD_LINK;              \
                }                                                           \
                return VERIFY_FAIL;                                         \
            }                                                               \
            if (u64i != tar->number) {                                      \
                if (error_code) {                                           \
                    *error_code = VERIFY_WRONG_BACKWARD_STAT;               \
                }                                                           \
                return VERIFY_FAIL;                                         \
            }                                                               \
        }                                                                   \
                                                                            \
        return 0;                                                           \
    }

#define add_generic_to_htab(tag_attr, tag_target) \
    int add_##tag_attr##_to_##tag_target##_to_htab (t_##tag_attr##_to_##tag_target ** htab_p, t_##tag_attr##_to_##tag_target * tar) { \
        if (!htab_p) {                                                              \
            return WRONG_ARGS;                                                      \
        }                                                                           \
        if (!tar) {                                                                 \
            return WRONG_ARGS;                                                      \
        }                                                                           \
                                                                                    \
        HASH_ADD_STR(*htab_p, str, tar);                                            \
                                                                                    \
        return 0;                                                                   \
                                                                                    \
    }

#define del_generic_from_htab(tag_attr, tag_target) \
    int del_##tag_attr##_to_##tag_target##_from_htab (t_##tag_attr##_to_##tag_target ** htab_p, t_##tag_attr##_to_##tag_target * tar) { \
        if (!htab_p) {                                                              \
            return WRONG_ARGS;                                                      \
        }                                                                           \
        if (!tar) {                                                                 \
            return WRONG_ARGS;                                                      \
        }                                                                           \
                                                                                    \
        HASH_DEL(*htab_p, tar);                                                     \
                                                                                    \
        return 0;                                                                   \
    }

#define init_layer1_obj_arr(obj_name, L1_ARR_SIZE)  \
    static int init_layer1_##obj_name##_arr (layer1_##obj_name##_arr* l1_arr) { \
        if (!l1_arr) {                                                                  \
            return WRONG_ARGS;                                                          \
        }                                                                               \
                                                                                        \
        mem_wipe_sec(l1_arr->arr, L1_ARR_SIZE * sizeof(l1_arr->arr[0]));                \
                                                                                        \
        bitmap_init(&l1_arr->usage_map, l1_arr->usage_map_raw, 0, L1_ARR_SIZE, 0);      \
                                                                                        \
        return 0;                                                                       \
    }

#define init_layer2_obj_arr(obj_name, L2_ARR_INIT_SIZE) \
    int init_layer2_##obj_name##_arr (layer2_##obj_name##_arr* l2_arr) { \
        int i;                                                                              \
        layer1_##obj_name##_arr* temp_l1_arr;                                               \
                                                                                            \
        map_block* temp_base;                                                               \
                                                                                            \
        if (!l2_arr) {                                                                      \
            return WRONG_ARGS;                                                              \
        }                                                                                   \
                                                                                            \
        /* grab space for pointers */                                                       \
        l2_arr->l1_arr =                                                                    \
        malloc(                                                                             \
                L2_ARR_INIT_SIZE                                                            \
                * sizeof(layer1_##obj_name##_arr*)                                          \
              );                                                                            \
        if (!l2_arr->l1_arr) {                                                              \
            return MALLOC_FAIL;                                                             \
        }                                                                                   \
                                                                                            \
        /* grab space and make pointers point to them */                                    \
        temp_l1_arr =                                                                       \
        malloc(                                                                             \
                L2_ARR_INIT_SIZE                                                            \
                * sizeof(layer1_##obj_name##_arr)                                           \
              );                                                                            \
        if (!temp_l1_arr) {                                                                 \
            return MALLOC_FAIL;                                                             \
        }                                                                                   \
                                                                                            \
        for (i = 0; i < L2_ARR_INIT_SIZE; i++) {                                            \
            init_layer1_##obj_name##_arr(temp_l1_arr + i);                                  \
            l2_arr->l1_arr[i] = temp_l1_arr + i;                                            \
        }                                                                                   \
                                                                                            \
        /* grab space for bitmap */                                                         \
        temp_base = malloc(get_bitmap_map_block_number(L2_ARR_INIT_SIZE));                  \
        if (!temp_base) {                                                                   \
            return MALLOC_FAIL;                                                             \
        }                                                                                   \
                                                                                            \
        bitmap_init(&l2_arr->l1_arr_map, temp_base, 0, L2_ARR_INIT_SIZE, 0);                \
                                                                                            \
        return 0;                                                                           \
    }

#define add_obj_to_layer2_arr(obj_name, obj_type, L2_ARR_GROW_SIZE, L1_ARR_SIZE) \
    int add_##obj_name##_to_layer2_arr (layer2_##obj_name##_arr * l2_arr, obj_type ** ret_obj, bit_index* index) { \
        layer1_##obj_name##_arr* temp_l1_arr;                                               \
        layer1_##obj_name##_arr** temp_l1_arr_p;                                            \
        obj_type * temp_obj_p;                                                              \
                                                                                            \
        int i;                                                                              \
                                                                                            \
        bit_index temp_index_result;                                                        \
        bit_index temp_index_result_backup;                                                 \
                                                                                            \
        uint64_t l2_num_total;                                                              \
                                                                                            \
        if (        !l2_arr                                                                 \
           /* must return either object or index, otherwise the object will be stranded */  \
                ||  (!ret_obj && !index)                                                    \
           )                                                                                \
        {                                                                                   \
            return WRONG_ARGS;                                                              \
        }                                                                                   \
                                                                                            \
        /* if no more usable l1_arr */                                                      \
        if (l2_arr->l1_arr_map.number_of_zeros == 0) {                                      \
            l2_num_total = l2_arr->l1_arr_map.length;                                       \
            /* realloc for more pointers */                                                 \
            temp_l1_arr_p =                                                                 \
            realloc(                                                                        \
                    l2_arr->l1_arr,                                                         \
                                                                                            \
                    (l2_num_total + L2_ARR_GROW_SIZE)                                       \
                    * sizeof(layer1_##obj_name##_arr*)                                      \
                   );                                                                       \
            if (!temp_l1_arr_p) {                                                           \
                return MALLOC_FAIL;                                                         \
            }                                                                               \
            /* grab space make pointers point to them */                                    \
            temp_l1_arr =                                                                   \
            malloc(                                                                         \
                    L2_ARR_GROW_SIZE                                                        \
                    * sizeof(layer1_##obj_name##_arr)                                       \
                  );                                                                        \
            if (!temp_l1_arr) {                                                             \
                return MALLOC_FAIL;                                                         \
            }                                                                               \
                                                                                            \
            for (i = 0; i < L2_ARR_GROW_SIZE; i++) {                                        \
                init_layer1_##obj_name##_arr(temp_l1_arr + i);                              \
                temp_l1_arr_p[l2_num_total + i] = temp_l1_arr + i;                          \
            }                                                                               \
                                                                                            \
            l2_arr->l1_arr = temp_l1_arr_p;                                                 \
                                                                                            \
            ffp_grow_bitmap(&l2_arr->l1_arr_map, l2_num_total + L2_ARR_GROW_SIZE);          \
        }                                                                                   \
                                                                                            \
        /* find first usable l1_arr */                                                      \
        bitmap_first_zero_bit_index(&l2_arr->l1_arr_map, &temp_index_result, 0);            \
        temp_l1_arr = *(l2_arr->l1_arr + temp_index_result);                                \
        temp_index_result_backup = temp_index_result;                                       \
                                                                                            \
        /* find first usable slot in the l1_arr */                                          \
        bitmap_first_zero_bit_index(&temp_l1_arr->usage_map, &temp_index_result, 0);        \
        temp_obj_p = temp_l1_arr->arr + temp_index_result;                                  \
                                                                                            \
        /* wipe object */                                                                   \
        mem_wipe_sec(temp_obj_p, sizeof(obj_type));                                         \
                                                                                            \
        /* grab object from slot and mark in bitmap */                                      \
        if (ret_obj) {                                                                      \
            *ret_obj = temp_obj_p;                                                          \
        }                                                                                   \
        bitmap_write(&temp_l1_arr->usage_map, temp_index_result, 1);                        \
                                                                                            \
        if (temp_l1_arr->usage_map.number_of_zeros == 0) {                                  \
            bitmap_write(&l2_arr->l1_arr_map, temp_index_result_backup, 1);                 \
        }                                                                                   \
                                                                                            \
        temp_obj_p->obj_arr_index = temp_index_result_backup * L1_ARR_SIZE + temp_index_result;\
                                                                                            \
        if (index) {                                                                        \
            *index = temp_obj_p->obj_arr_index;                                             \
        }                                                                                   \
                                                                                            \
        return 0;                                                                           \
    }

#define del_obj_from_layer2_arr(obj_name, obj_type, L1_ARR_SIZE) \
    int del_##obj_name##_from_layer2_arr(layer2_##obj_name##_arr * l2_arr, bit_index index) { \
        bit_index l2_index = index / L1_ARR_SIZE;                           \
        bit_index l1_index = index % L1_ARR_SIZE;                           \
                                                                            \
        layer1_##obj_name##_arr* temp_l1_arr;                               \
        obj_type* temp_obj_p;                                               \
                                                                            \
        map_block temp_block;                                               \
                                                                            \
        if (!l2_arr) {                                                      \
            return WRONG_ARGS;                                              \
        }                                                                   \
                                                                            \
        /* check if index is reachable in any L1 array at all */            \
        if (l2_index > l2_arr->l1_arr_map.length - 1) {                     \
            return INDEX_OUT_OF_RANGE;                                      \
        }                                                                   \
                                                                            \
        temp_l1_arr = l2_arr->l1_arr[l2_index];                             \
                                                                            \
        if (l1_index > temp_l1_arr->usage_map.length - 1) {                 \
            return INDEX_OUT_OF_RANGE;                                      \
        }                                                                   \
        /* check if the index is actually used in that L1 array */          \
        bitmap_read(&temp_l1_arr->usage_map, l1_index, &temp_block);        \
        if (!temp_block) {                                                  \
            return FIND_FAIL;                                               \
        }                                                                   \
                                                                            \
        temp_obj_p = temp_l1_arr->arr + l1_index;                           \
                                                                            \
        /* wipe object */                                                   \
        mem_wipe_sec(temp_obj_p, sizeof(obj_type));                         \
                                                                            \
        /* mark it in bitmap */                                             \
        bitmap_write(&temp_l1_arr->usage_map, l1_index, 0);                 \
                                                                            \
        /* if the l1_arr was previously marked full, now mark it usable */  \
        bitmap_read(&l2_arr->l1_arr_map, l2_index, &temp_block);            \
        if (temp_block == 1) {                                              \
            bitmap_write(&l2_arr->l1_arr_map, l2_index, 0);                 \
        }                                                                   \
                                                                            \
        return 0;                                                           \
    }

#define get_obj_from_layer2_arr(obj_name, obj_type, L1_ARR_SIZE) \
    int get_##obj_name##_from_layer2_arr (layer2_##obj_name##_arr * l2_arr, obj_type ** ret_obj, bit_index index) { \
        bit_index l2_index = index / L1_ARR_SIZE;                       \
        bit_index l1_index = index % L1_ARR_SIZE;                       \
                                                                        \
        layer1_##obj_name##_arr* temp_l1_arr;                           \
                                                                        \
        map_block temp_block;                                           \
                                                                        \
        if (        !l2_arr                                             \
                ||  !ret_obj                                            \
           )                                                            \
        {                                                               \
            return WRONG_ARGS;                                          \
        }                                                               \
                                                                        \
        if (l2_index > l2_arr->l1_arr_map.length - 1) {                 \
            *ret_obj = 0;                                               \
            return INDEX_OUT_OF_RANGE;                                  \
        }                                                               \
                                                                        \
        temp_l1_arr = l2_arr->l1_arr[l2_index];                         \
                                                                        \
        if (l1_index > temp_l1_arr->usage_map.length - 1) {             \
            *ret_obj = 0;                                               \
            return INDEX_OUT_OF_RANGE;                                  \
        }                                                               \
        /* check if the index is actually used in that L1 array */      \
        bitmap_read(&temp_l1_arr->usage_map, l1_index, &temp_block);    \
        if (!temp_block) {  /* slot is not used */                      \
            *ret_obj = 0;                                               \
            return FIND_FAIL;                                           \
        }                                                               \
                                                                        \
        *ret_obj = temp_l1_arr->arr + l1_index;                         \
                                                                        \
        return 0;                                                       \
    }

#define get_l1_obj_from_layer2_arr(obj_name) \
    int get_l1_##obj_name##_from_layer2_arr(layer2_##obj_name##_arr * l2_arr, layer1_##obj_name##_arr ** ret_obj, bit_index index_of_l1_arr) { \
        if (        !l2_arr                                     \
                ||  !ret_obj                                    \
           )                                                    \
        {                                                       \
            return WRONG_ARGS;                                  \
        }                                                       \
                                                                \
        if (index_of_l1_arr > l2_arr->l1_arr_map.length - 1) {  \
            *ret_obj = 0;                                       \
            return INDEX_OUT_OF_RANGE;                          \
        }                                                       \
                                                                \
        *ret_obj = l2_arr->l1_arr[index_of_l1_arr];             \
                                                                \
        return 0;                                               \
    }

#define del_l2_obj_arr(obj_name, L2_ARR_INIT_SIZE, L2_ARR_GROW_SIZE) \
    int del_l2_##obj_name##_arr(layer2_##obj_name##_arr * l2_arr) { \
        bit_index i;                                                                        \
        layer1_##obj_name##_arr* temp_l1_arr;                                               \
        simple_bitmap* temp_map;                                                            \
                                                                                            \
        if (!l2_arr) {                                                                      \
            return WRONG_ARGS;                                                              \
        }                                                                                   \
                                                                                            \
        temp_map = &l2_arr->l1_arr_map;                                                     \
                                                                                            \
        /* wipe and free layer 1 arrays */                                                  \
        /* wipe and free first chunk of layer 1 arrays */                                   \
        temp_l1_arr = l2_arr->l1_arr[0];                                                    \
        mem_wipe_sec(temp_l1_arr, L2_ARR_INIT_SIZE * sizeof(layer1_##obj_name##_arr));      \
        free(temp_l1_arr);                                                                  \
                                                                                            \
        /* wipe and free remaining chunks */                                                \
        for (i = L2_ARR_INIT_SIZE; i < temp_map->length; i += L2_ARR_GROW_SIZE) {           \
            temp_l1_arr = l2_arr->l1_arr[i];                                                \
            mem_wipe_sec(temp_l1_arr, L2_ARR_GROW_SIZE * sizeof(layer1_##obj_name##_arr));  \
            free(temp_l1_arr);                                                              \
        }                                                                                   \
                                                                                            \
        /* wipe layer 1 array pointer array */                                              \
        mem_wipe_sec(                                                                       \
                        l2_arr->l1_arr,                                                     \
                        temp_map->length                                                    \
                        * sizeof(layer1_##obj_name##_arr*)                                  \
                    );                                                                      \
                                                                                            \
        /* free layer 1 array pointer array */                                              \
        free(l2_arr->l1_arr);                                                               \
                                                                                            \
         /* wipe bitmap raw */                                                              \
        mem_wipe_sec(temp_map->base, get_bitmap_map_block_number(temp_map->length));        \
                                                                                            \
        /* free layer 1 array tracking bitmap raw */                                        \
        free(temp_map->base);                                                               \
                                                                                            \
        /* wipe bitmap meta data */                                                         \
        mem_wipe_sec(temp_map, sizeof(simple_bitmap));                                      \
                                                                                            \
        return 0;                                                                           \
    }

#define init_layer1_generic_arr(tag_attr, tag_target, L1_ARR_SIZE)  \
    init_layer1_obj_arr(tag_attr##_to_##tag_target, L1_ARR_SIZE)

#define init_layer2_generic_arr(tag_attr, tag_target, L2_ARR_INIT_SIZE) \
    init_layer2_obj_arr(tag_attr##_to_##tag_target, L2_ARR_INIT_SIZE)

#define add_generic_to_layer2_arr(tag_attr, tag_target, L2_ARR_GROW_SIZE, L1_ARR_SIZE) \
    add_obj_to_layer2_arr(tag_attr##_to_##tag_target, t_##tag_attr##_to_##tag_target, L2_ARR_GROW_SIZE, L1_ARR_SIZE)

#define del_generic_from_layer2_arr(tag_attr, tag_target, L1_ARR_SIZE) \
    del_obj_from_layer2_arr(tag_attr##_to_##tag_target, t_##tag_attr##_to_##tag_target, L1_ARR_SIZE)

#define get_generic_from_layer2_arr(tag_attr, tag_target, L1_ARR_SIZE) \
    get_obj_from_layer2_arr(tag_attr##_to_##tag_target, t_##tag_attr##_to_##tag_target, L1_ARR_SIZE)

#define get_l1_generic_from_layer2_arr(tag_attr, tag_target) \
    get_l1_obj_from_layer2_arr(tag_attr##_to_##tag_target)

#define del_l2_generic_arr(tag_attr, tag_target, L2_ARR_INIT_SIZE, L2_ARR_GROW_SIZE) \
    del_l2_obj_arr(tag_attr##_to_##tag_target, L2_ARR_INIT_SIZE, L2_ARR_GROW_SIZE)

#define init_generic_exist_mat(tag_attr, STR_MAX_LEN) \
    int init_##tag_attr##_exist_mat (tag_attr##_exist_mat* matrix) { \
        int i;                                  \
                                                \
        if (!matrix) {                          \
            return WRONG_ARGS;                  \
        }                                       \
                                                \
        for (i = 0; i < STR_MAX_LEN; i++) {     \
            matrix->uniq_char[i] = 0;           \
        }                                       \
                                                \
        matrix->max_length = 0;                 \
                                                \
        return 0;                               \
    }

#define add_generic_to_generic_exist_mat(tag_attr, L2_ARR_GROW_SIZE, L1_ARR_SIZE, STR_MAX_LEN) \
    int add_##tag_attr##_to_##tag_attr##_exist_mat (tag_attr##_exist_mat * matrix, char* str, bit_index index_in_arr) { \
        int i;                                                                              \
                                                                                            \
        uniq_char_map* temp_uniq_char;                                                      \
        uniq_char_map* temp_uniq_char_backup;                                               \
                                                                                            \
        map_block* temp_base;                                                               \
                                                                                            \
        uint32_t index_of_l1_arr = index_in_arr / L1_ARR_SIZE;                              \
                                                                                            \
        bit_index new_length;                                                               \
                                                                                            \
        str_len_int str_len;                                                                \
                                                                                            \
        if ((i = verify_str_terminated(str, STR_MAX_LEN, &str_len, 0))) {                   \
            return i;                                                                       \
        }                                                                                   \
                                                                                            \
        for (i = 0; i < str_len; i++) {                                                     \
            if (!matrix->uniq_char[i]) {    /* this position is empty */                    \
                /* make space for a new uniq char */                                        \
                temp_uniq_char = malloc(sizeof(uniq_char_map));                             \
                if (!temp_uniq_char) {                                                      \
                    return MALLOC_FAIL;                                                     \
                }                                                                           \
                init_uniq_char_map(temp_uniq_char);                                         \
                matrix->uniq_char[i] = temp_uniq_char;                                      \
                temp_uniq_char->character = str[i];                                         \
                                                                                            \
                /* make space for a new bitmap */                                           \
                new_length = index_of_l1_arr + 1;                                           \
                temp_base =                                                                 \
                malloc(                                                                     \
                        get_bitmap_map_block_number(new_length)                             \
                        * sizeof(map_block)                                                 \
                      );                                                                    \
                if (!temp_base) {                                                           \
                    return MALLOC_FAIL;                                                     \
                }                                                                           \
                                                                                            \
                /* initialise bitmap */                                                     \
                bitmap_init(&temp_uniq_char->l1_arr_map, temp_base, 0, new_length, 0);      \
                /* mark in bitmap */                                                        \
                bitmap_write(&temp_uniq_char->l1_arr_map, index_of_l1_arr, 1);              \
            }                                                                               \
            else {      /* not empty, check if char exist in this position */               \
                temp_uniq_char = matrix->uniq_char[i];                                      \
                temp_uniq_char_backup = temp_uniq_char;                                     \
                while (temp_uniq_char) {                                                    \
                    if (temp_uniq_char->character == str[i]) {                              \
                        break;                                                              \
                    }                                                                       \
                    temp_uniq_char_backup = temp_uniq_char;                                 \
                    temp_uniq_char = temp_uniq_char->next;                                  \
                }                                                                           \
                if (!temp_uniq_char) {  /* no match found */                                \
                    /* attach to tail */                                                    \
                    temp_uniq_char = malloc(sizeof(uniq_char_map));                         \
                    if (!temp_uniq_char) {                                                  \
                        return MALLOC_FAIL;                                                 \
                    }                                                                       \
                    init_uniq_char_map(temp_uniq_char);                                     \
                    temp_uniq_char->character = str[i];                                     \
                    temp_uniq_char_backup->next = temp_uniq_char;                           \
                                                                                            \
                    /* make space for a new bitmap */                                       \
                    new_length = index_of_l1_arr + 1;                                       \
                    temp_base =                                                             \
                    malloc(                                                                 \
                            get_bitmap_map_block_number(new_length)                         \
                            * sizeof(map_block)                                             \
                          );                                                                \
                    if (!temp_base) {                                                       \
                        return MALLOC_FAIL;                                                 \
                    }                                                                       \
                                                                                            \
                    /* initialise bitmap */                                                 \
                    bitmap_init(&temp_uniq_char->l1_arr_map, temp_base, 0, new_length, 0);  \
                    /* mark in bitmap */                                                    \
                    bitmap_write(&temp_uniq_char->l1_arr_map, index_of_l1_arr, 1);          \
                }                                                                           \
                else {  /* match found */                                                   \
                    /* grow if necessary */                                                 \
                    if (temp_uniq_char->l1_arr_map.length < index_of_l1_arr + 1) {          \
                        new_length = index_of_l1_arr + 1;                                   \
                        ffp_grow_bitmap(&temp_uniq_char->l1_arr_map, new_length);           \
                    }                                                                       \
                                                                                            \
                    /* mark on bitmap */                                                    \
                    bitmap_write(&temp_uniq_char->l1_arr_map, index_of_l1_arr, 1);          \
                }                                                                           \
            }                                                                               \
        }                                                                                   \
                                                                                            \
        if (str_len > matrix->max_length) {                                                 \
            matrix->max_length = str_len;                                                   \
        }                                                                                   \
                                                                                            \
        return 0;                                                                           \
    }

#define del_generic_from_generic_exist_mat(tag_attr, tag_target, L1_ARR_SIZE) \
    int del_##tag_attr##_from_##tag_attr##_exist_mat (tag_attr##_exist_mat * matrix, layer2_##tag_attr##_to_##tag_target##_arr * l2_arr, bit_index index_in_arr) { \
        int i, j;                                                                   \
                                                                                    \
        str_len_int str_len;                                                        \
                                                                                    \
        uniq_char_map* temp_uniq_char;                                              \
        uniq_char_map* temp_uniq_char_backup = NULL;                                \
                                                                                    \
        uint32_t index_of_l1_arr = index_in_arr / L1_ARR_SIZE;                      \
        uint32_t index_of_l1_arr_in_linear = index_of_l1_arr * L1_ARR_SIZE;         \
        uint32_t index_in_l1_arr = index_in_arr % L1_ARR_SIZE;                      \
                                                                                    \
        t_##tag_attr##_to_##tag_target * result;                                    \
        t_##tag_attr##_to_##tag_target * result2;                                   \
                                                                                    \
        layer1_##tag_attr##_to_##tag_target##_arr* temp_l1_arr;                     \
                                                                                    \
        int ret;                                                                    \
                                                                                    \
        bit_index index_result;                                                     \
        bit_index index_result_backup;                                              \
                                                                                    \
        if ((ret = get_##tag_attr##_to_##tag_target##_from_layer2_arr(l2_arr, &result, index_in_arr))) {\
            return ret;                                                             \
        }                                                                           \
                                                                                    \
        str_len = result->str_len;                                                  \
                                                                                    \
        for (i = 0; i < str_len; i++) {                                             \
            if (!matrix->uniq_char[i]) {    /* empty position */                    \
                return LOGIC_ERROR;                                                 \
            }                                                                       \
                                                                                    \
            temp_uniq_char = matrix->uniq_char[i];                                  \
                                                                                    \
            while (temp_uniq_char) {                                                \
                if (temp_uniq_char->character == result->str[i]) {                  \
                    break;                                                          \
                }                                                                   \
                temp_uniq_char_backup = temp_uniq_char;                             \
                temp_uniq_char = temp_uniq_char->next;                              \
            }                                                                       \
            if (!temp_uniq_char) {                                                  \
                return LOGIC_ERROR;                                                 \
            }                                                                       \
                                                                                    \
            /* check if layer 1 has other entries which share this character in this position*/\
            temp_l1_arr = l2_arr->l1_arr[index_of_l1_arr];                          \
            for (j = 0, index_result_backup = 0; j < temp_l1_arr->usage_map.number_of_ones; j++) {\
                bitmap_first_one_bit_index(&temp_l1_arr->usage_map, &index_result, index_result_backup);\
                index_result_backup = index_result + 1;                             \
                                                                                    \
                /* if this index corresponds to the target index, skip */           \
                if (index_result == index_in_l1_arr) {                              \
                    continue;                                                       \
                }                                                                   \
                                                                                    \
                get_##tag_attr##_to_##tag_target##_from_layer2_arr(l2_arr, &result2, index_of_l1_arr_in_linear + index_result);\
                                                                                    \
                if (result2->str_len - 1 >= i && result2->str[i] == result->str[i]) {\
                    break;                                                          \
                }                                                                   \
            }                                                                       \
            if (j == temp_l1_arr->usage_map.number_of_ones) { /* this entry is the only one */\
                /* mark in bitmap that the L1 arr contains no entry */              \
                bitmap_write(&temp_uniq_char->l1_arr_map, index_of_l1_arr, 0);      \
            }                                                                       \
                                                                                    \
            if (temp_uniq_char->l1_arr_map.number_of_ones == 0) {                   \
                /* stich and free uniq_char and L1 arr map */                       \
                if (matrix->uniq_char[i] == temp_uniq_char) {                       \
                    matrix->uniq_char[i] = temp_uniq_char->next;                    \
                }                                                                   \
                else {                                                              \
                    temp_uniq_char_backup->next = temp_uniq_char->next;             \
                }                                                                   \
                free(temp_uniq_char->l1_arr_map.base);                              \
                free(temp_uniq_char);                                               \
            }                                                                       \
        }                                                                           \
                                                                                    \
        /* check if need to update max length */                                    \
        if (str_len == matrix->max_length) {                                        \
            /* defaults to 0 incase nothing shows up */                             \
            matrix->max_length = 0;                                                 \
                                                                                    \
            /* find new max length */                                               \
            for (i = str_len - 1; i >= 0; i--) {                                    \
                /* break if the position is not empty */                            \
                /* which indicates this position is the new max length - 1 */       \
                if (matrix->uniq_char[i]) {                                         \
                    matrix->max_length = i + 1;                                     \
                    break;                                                          \
                }                                                                   \
                                                                                    \
                if (i == 0) {   /* avoid underflow and wrap around in case i is unsigned */\
                    break;                                                          \
                }                                                                   \
            }                                                                       \
        }                                                                           \
                                                                                    \
        return 0;                                                                   \
    }

#define del_generic_exist_mat(tag_attr, STR_MAX_LEN) \
    int del_##tag_attr##_exist_mat(tag_attr##_exist_mat * matrix) { \
        int i;                                                              \
                                                                            \
        uniq_char_map* temp_uniq_char;                                      \
        uniq_char_map* temp_uniq_char_backup;                               \
                                                                            \
        if (!matrix) {                                                      \
            return WRONG_ARGS;                                              \
        }                                                                   \
                                                                            \
        for (i = 0; i < STR_MAX_LEN; i++) {                                 \
            if (!matrix->uniq_char[i]) {    /* this position is empty */    \
                continue;                                                   \
            }                                                               \
            /* free entire chain of uniq char */                            \
            temp_uniq_char = matrix->uniq_char[i];                          \
            while (temp_uniq_char) {                                        \
                temp_uniq_char_backup = temp_uniq_char->next;               \
                /* free layer 1 array bitmap raw */                         \
                free(temp_uniq_char->l1_arr_map.base);                      \
                /* free uniq char */                                        \
                free(temp_uniq_char);                                       \
                temp_uniq_char = temp_uniq_char_backup;                     \
            }                                                               \
        }                                                                   \
                                                                            \
        return 0;                                                           \
    }

#define add_generic_to_generic_chain(tag_attr, tag_target, tag_prev, tag_next, str_name, target_type) \
    int add_##tag_target##_to_##tag_attr##_to_##tag_target##_chain (target_type * dst, target_type * tar) { \
        t_##tag_attr##_to_##tag_target * temp_tran;                             \
                                                                                \
        if (!dst) {                                                             \
            return WRONG_ARGS;                                                  \
        }                                                                       \
        if (!tar) {                                                             \
            return WRONG_ARGS;                                                  \
        }                                                                       \
        if (strcmp((const char*) dst->str_name, (const char*) tar->str_name) != 0) {\
            return WRONG_ARGS;                                                  \
        }                                                                       \
                                                                                \
        temp_tran = dst->tag_attr##_to_##tag_target;                            \
        tar->tag_attr##_to_##tag_target = dst->tag_attr##_to_##tag_target;      \
                                                                                \
        tar->tag_prev = temp_tran->tail_tar;                                    \
        temp_tran->tail_tar->tag_next = tar;                                    \
        tar->tag_next = 0;                                                      \
        temp_tran->tail_tar = tar;                                              \
                                                                                \
        temp_tran->number++;                                                    \
                                                                                \
        return 0;                                                               \
    }

#define del_generic_from_generic_chain(tag_attr, tag_target, tag_prev, tag_next, target_type) \
    int del_##tag_target##_from_##tag_attr##_to_##tag_target##_chain (t_##tag_attr##_to_##tag_target ** htab_p, tag_attr##_exist_mat * matrix, layer2_##tag_attr##_to_##tag_target##_arr * l2_arr, target_type * tar) { \
        t_##tag_attr##_to_##tag_target * temp_tran;                                         \
                                                                                            \
        if (!matrix) {                                                                      \
            return WRONG_ARGS;                                                              \
        }                                                                                   \
        if (!l2_arr) {                                                                      \
            return WRONG_ARGS;                                                              \
        }                                                                                   \
        if (!tar) {                                                                         \
            return WRONG_ARGS;                                                              \
        }                                                                                   \
                                                                                            \
        if (!tar->tag_attr##_to_##tag_target) { /* not linked */                            \
            return 0;                                                                       \
        }                                                                                   \
                                                                                            \
        temp_tran = tar->tag_attr##_to_##tag_target;                                        \
                                                                                            \
        if (tar->tag_prev == NULL && tar->tag_next == NULL) {  /* last entry in list */     \
            /* delete from hash table */                                                    \
            del_##tag_attr##_to_##tag_target##_from_htab(htab_p, temp_tran);                \
            /* delete from existence matrix */                                              \
            del_##tag_attr##_from_##tag_attr##_exist_mat(matrix, l2_arr, temp_tran->obj_arr_index);\
            /* delete from layer 2 array */                                                 \
            del_##tag_attr##_to_##tag_target##_from_layer2_arr(l2_arr, temp_tran->obj_arr_index);\
            /* nullify link */                                                              \
            tar->tag_attr##_to_##tag_target = NULL;                                         \
        }                                                                                   \
        else if (tar->tag_prev == NULL) {    /* head of list */                             \
            temp_tran->head_tar = tar->tag_next;                                            \
            tar->tag_next->tag_prev = NULL;                                                 \
        }                                                                                   \
        else if(tar->tag_next == NULL) {     /* tail of list */                             \
            temp_tran->tail_tar = tar->tag_prev;                                            \
            tar->tag_prev->tag_next = NULL;                                                 \
        }                                                                                   \
        else {      /* somewhere in list */                                                 \
            tar->tag_prev->tag_next = tar->tag_next;                                        \
            tar->tag_next->tag_prev = tar->tag_prev;                                        \
        }                                                                                   \
                                                                                            \
        return 0;                                                                           \
    }
