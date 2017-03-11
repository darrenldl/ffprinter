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
 *
 *  Notes :
 *  This code file contains tests for templates in :
 *      ffprinter_function_template.h
 *      ffp_database_function_template.h
 *
 *  The tests contained here should highlight most, if not all, requirements
 *  expected from the function templates
 *
 *  Please ensure the templates are still fully functional
 *  should you wish to modify the templates
 *  unless you are modifying all uses of the templates entirely
 *
 *  Memory clean up is not required unless that is part of testing
 *
 *  Notes for using tests in case of modification of templates:
 *      Below tests assumes that addition of pointers/values are all sequential
 *      If your implementation is randomness based, please devise another set of tests
 *      as the tests below will not work properly in that case
 *
 *  Notes on different style of return code in functions which
 *  get structure from layer2 array vs functions which execute lookup
 *      get functions return FIND_FAIL when the slot corresponding to the index
 *      is not actually used, that is, it identififes this as an error
 *      This is because get functions are only meant to retrive things with valid index
 *      rather than being a search function
 *
 *      lookup functions return 0 when failed to find as it anticipates the possibility
 *      of failing to retrieve objects due to being search functions
 */

#include "unit_test.h"

#include "../src/ffprinter.h"
#include "../src/ffprinter_function_template.h"
#include "../src/ffp_database_function_template.h"

// use small sizes to force growth handling quicker
#define UTEST_ID_LENGTH         10
#define UTEST_L1_SIZE           4
#define UTEST_L2_INIT_SIZE      10
#define UTEST_L2_GROW_SIZE      2

#define LARGE_INT               100000

#define MAT_MAX_INDEX           100000   // max index for use in testing existence matrix

typedef struct utester utester;

struct utester {
    char id[UTEST_ID_LENGTH+1];

    utester* prev_same_id;
    utester* next_same_id;
};

typedefs_trans(id, ut)

generic_trans_struct_one_to_many(id, ut, utester, UTEST_ID_LENGTH)
layer1_generic_arr(id, ut, UTEST_L1_SIZE)
layer2_generic_arr(id, ut)
generic_exist_mat(id, UTEST_ID_LENGTH)

init_generic_trans_struct_one_to_many(id, ut)
init_layer1_generic_arr(id, ut, UTEST_L1_SIZE)
init_layer2_generic_arr(id, ut, UTEST_L2_INIT_SIZE)
init_generic_exist_mat(id, UTEST_ID_LENGTH)

add_generic_to_layer2_arr(id, ut, UTEST_L2_GROW_SIZE, UTEST_L1_SIZE)
del_generic_from_layer2_arr(id, ut, UTEST_L1_SIZE)
get_generic_from_layer2_arr(id, ut, UTEST_L1_SIZE)

get_l1_generic_from_layer2_arr(id, ut)

del_l2_generic_arr(id, ut, UTEST_L2_INIT_SIZE, UTEST_L2_GROW_SIZE)

add_generic_to_generic_exist_mat(id, UTEST_L2_GROW_SIZE, UTEST_L1_SIZE, UTEST_ID_LENGTH)
del_generic_from_generic_exist_mat(id, ut, UTEST_L1_SIZE)

del_generic_exist_mat(id, 10)

lookup_generic_one_to_many_tran(id, ut, id, utester)
lookup_generic_part_map(id, ut, id, UTEST_ID_LENGTH)
lookup_generic_part(id, ut, id, UTEST_ID_LENGTH)

// no dependencies on correctness of other structures
int test_add_get_del_with_layer2_arr() {
    uint64_t i;

    utester tdum;
    utester tdum2;
    t_id_to_ut* id_to_ut;
    t_id_to_ut* id_to_ut_found;
    layer2_id_to_ut_arr l2_arr;
    uint64_t index_in_arr;

    add_trackers();

    int ret;

    announce_test(test_add_get_del_with_layer2_arr);
    announce_test_begin();

    // setup data object
    strcpy(tdum.id, "id_abc");
    strcpy(tdum2.id, "id_def");

    // setup layer 2 array
    init_layer2_id_to_ut_arr(&l2_arr);

    // test invalid get using unused, but allocated index
    printf("test area 1 : invalid get, unused but allocated index\n");
    incre_check();
    ret = get_id_to_ut_from_layer2_arr(&l2_arr, &id_to_ut_found, 0);
    if (id_to_ut_found != NULL) {
        printf("got non-null result from layer 2 array using invalid index\n");
        printf("expected behaviour : get null result which indicates invalid read\n");
        incre_error();
    }
    if (ret != FIND_FAIL) {
        printf("got error code other than FIND_FAIL\n");
        printf("expected behaviour : error code to be FIND_FAIL\n");
        printf("returned error code : %d\n", ret);
        incre_error();
    }

    // test invalid get using unused, and unallocated index
    printf("test area 2 : invalid get, unused and unallocated index\n");
    incre_check();
    ret = get_id_to_ut_from_layer2_arr(&l2_arr, &id_to_ut_found, 10000);
    if (id_to_ut_found != NULL) {
        printf("got non-null result from layer 2 array using invalid index\n");
        printf("expected behaviour : get null result which indicates invalid read\n");
        incre_error();
    }
    if (ret != INDEX_OUT_OF_RANGE) {
        printf("got error code other than INDEX_OUT_OF_RANGE\n");
        printf("expected behaviour : error code to be INDEX_OUT_OF_RANGE\n");
        printf("returned error code : %d\n", ret);
        incre_error();
    }

    // add to layer2 array
    add_id_to_ut_to_layer2_arr(&l2_arr, &id_to_ut, &index_in_arr);
    // setup translation structure
    init_id_to_ut(id_to_ut);
    // link id_to_ut to tdum
    link_to_tran(&tdum, id_to_ut);

    // test valid get using used index
    printf("test area 3 : valid get using used index\n");
    incre_check();
    ret = get_id_to_ut_from_layer2_arr(&l2_arr, &id_to_ut_found, index_in_arr);
    if (id_to_ut_found == NULL) {
        printf("got null result from layer 2 array using valid index\n");
        printf("expected behaviour : get null result which indicates invalid read\n");
        incre_error();
    }
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
    }

    // invalid delete from layer2 array, allocated index(reachable in some L1 array)
    printf("test area 4 : invalid delete of unused index but reachable\n");
    incre_check();
    ret = del_id_to_ut_from_layer2_arr(&l2_arr, 10);
    if (ret != FIND_FAIL) {
        printf("got error code other than FIND_FAIL\n");
        printf("expected behaviour : error code to be FIND_FAIL\n");
        printf("returned error code : %d\n", ret);
        incre_error();
    }

    // invalid delete from layer2 array, unallocated index(unreachable in any L1 array)
    printf("test area 5 : invalid delete of unused index and unreachable index\n");
    incre_check();
    ret = del_id_to_ut_from_layer2_arr(&l2_arr, 100000);
    if (ret != INDEX_OUT_OF_RANGE) {
        printf("got error code other than INDEX_OUT_OF_RANGE\n");
        printf("expected behaviour : error code to be INDEX_OUT_OF_RANGE\n");
        printf("returned error code : %d\n", ret);
        incre_error();
    }

    // valid delete from layer2 array
    printf("test area 6 : valid delete of used index\n");
    incre_check();
    ret = del_id_to_ut_from_layer2_arr(&l2_arr, index_in_arr);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
    }

    // invalid delete from layer2 array
    printf("test area 7 : invalid delete of already deleted and used index\n");
    incre_check();
    ret = del_id_to_ut_from_layer2_arr(&l2_arr, index_in_arr);
    if (ret != FIND_FAIL) {
        printf("got error code other than FIND_FAIL\n");
        printf("expected behaviour : error code to be FIND_FAIL\n");
        printf("returned error code : %d\n", ret);
        incre_error();
    }

    // large quantity test for testing growth, assuming sequential allocation
    // use alternate pointer pattern
    // i.e. if index is even, use id_to_ut, otherwise use id_to_ut2
    printf("test area 8 : large quantity test for test growing\n");
    incre_check();
    for (i = 0; i < LARGE_INT; i++) {
        add_id_to_ut_to_layer2_arr(&l2_arr, &id_to_ut, &index_in_arr);
        if (i % 2 == 0) {
            link_to_tran(&tdum, id_to_ut);
        }
        else {
            link_to_tran(&tdum2, id_to_ut);
        }
    }
    // check for allocations using get
    for (i = 0; i < LARGE_INT; i++) {
        ret = get_id_to_ut_from_layer2_arr(&l2_arr, &id_to_ut_found, i);
        if (ret != 0) {
            printf("got error code other than 0\n");
            printf("expected behaviour error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            break;
        }
        if (i % 2 == 0) {
            if (        strcmp(id_to_ut_found->str, tdum.id) != 0
                    ||  id_to_ut_found->head_tar    != &tdum
                    ||  id_to_ut_found->tail_tar    != &tdum
                    ||  id_to_ut_found->str_len     != strlen(tdum.id)
               )
            {
                printf("linkage mismatch in layer 2 array\n");
                printf("expected behaviour : pointer to be same as &id_to_ut\n");
                incre_error();
                break;
            }
        }
        else {
            if (        strcmp(id_to_ut_found->str, tdum2.id) != 0
                    ||  id_to_ut_found->head_tar    != &tdum2
                    ||  id_to_ut_found->tail_tar    != &tdum2
                    ||  id_to_ut_found->str_len     != strlen(tdum2.id)
               )
            {
                printf("linkage mismatch in layer 2 array\n");
                printf("expected behaviour : pointer to be same as &id_to_ut\n");
                incre_error();
                break;
            }
        }
    }

    // delete all stored pointers
    printf("test area 9 : continuing from test area 8, sequentially delete everything and verify\n");
    incre_check();
    for (i = 0; i < LARGE_INT; i++) {
        ret = del_id_to_ut_from_layer2_arr(&l2_arr, i);
        if (ret != 0) {
            printf("got error code other than 0\n");
            printf("expected behaviour error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            break;
        }
        ret = get_id_to_ut_from_layer2_arr(&l2_arr, &id_to_ut_found, i);
        if (ret != FIND_FAIL) {
            printf("got error code other than FIND_FAIL\n");
            printf("expected behaviour : error code to be FIND_FAIL\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            break;
        }
    }

    // allocate and deletions alternately for 2 sets i.e. : alloc, del, alloc, del
    // with different offsets
    printf("test area 10 : large quantity allocation to test index reuse\n");
    incre_check();
    for (i = 0; i < LARGE_INT; i++) {
        add_id_to_ut_to_layer2_arr(&l2_arr, &id_to_ut, NULL);
        if (i % 2 == 0) {
            link_to_tran(&tdum, id_to_ut);
        }
        else {
            link_to_tran(&tdum2, id_to_ut);
        }
    }
    // confirm allocations
    for (i = 0; i < LARGE_INT; i++) {
        ret = get_id_to_ut_from_layer2_arr(&l2_arr, &id_to_ut_found, i);
        if (ret != 0) {
            printf("got error code other than 0\n");
            printf("expected behaviour error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            break;
        }
        if (i % 2 == 0) {
            if (        strcmp(id_to_ut_found->str, tdum.id) != 0
                    ||  id_to_ut_found->head_tar    != &tdum
                    ||  id_to_ut_found->tail_tar    != &tdum
                    ||  id_to_ut_found->str_len     != strlen(tdum.id)
               )
            {
                printf("linkage mismatch in layer 2 array\n");
                printf("expected behaviour : pointer to be same as &id_to_ut\n");
                incre_error();
                break;
            }
        }
        else {
            if (        strcmp(id_to_ut_found->str, tdum2.id) != 0
                    ||  id_to_ut_found->head_tar    != &tdum2
                    ||  id_to_ut_found->tail_tar    != &tdum2
                    ||  id_to_ut_found->str_len     != strlen(tdum2.id)
               )
            {
                printf("linkage mismatch in layer 2 array\n");
                printf("expected behaviour : pointer to be same as &id_to_ut\n");
                incre_error();
                break;
            }
        }
    }

    printf("test area 11 : continuing from test area 10 - deletion with gaps\n");
    incre_check();
    // 500(five hundred) to 1000(one thousand) INCLUSIVE
    for (i = 500; i <= 1000; i++) {
        ret = del_id_to_ut_from_layer2_arr(&l2_arr, i);
        if (ret != 0) {
            printf("got error code other than 0\n");
            printf("expected behaviour error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test11;
        }
        ret = get_id_to_ut_from_layer2_arr(&l2_arr, &id_to_ut_found, i);
        if (ret != FIND_FAIL) {
            printf("got error code other than FIND_FAIL\n");
            printf("expected behaviour : error code to be FIND_FAIL\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test11;
        }
    }
    // 2000(two thousand) to 3000(three thousand) INCLUSIVE
    for (i = 2000; i <= 3000; i++) {
        ret = del_id_to_ut_from_layer2_arr(&l2_arr, i);
        if (ret != 0) {
            printf("got error code other than 0\n");
            printf("expected behaviour error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test11;
        }
        ret = get_id_to_ut_from_layer2_arr(&l2_arr, &id_to_ut_found, i);
        if (ret != FIND_FAIL) {
            printf("got error code other than FIND_FAIL\n");
            printf("expected behaviour : error code to be FIND_FAIL\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test11;
        }
    }
    // 200(two hundred) to 400(four hundred) INCLUSIVE
    for (i = 200; i <= 400; i++) {
        ret = del_id_to_ut_from_layer2_arr(&l2_arr, i);
        if (ret != 0) {
            printf("got error code other than 0\n");
            printf("expected behaviour error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test11;
        }
        ret = get_id_to_ut_from_layer2_arr(&l2_arr, &id_to_ut_found, i);
        if (ret != FIND_FAIL) {
            printf("got error code other than FIND_FAIL\n");
            printf("expected behaviour : error code to be FIND_FAIL\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test11;
        }
    }
    // allocate 201 units, which should use index 200 to 400
    for (i = 0; i <= 200; i++) {
        add_id_to_ut_to_layer2_arr(&l2_arr, &id_to_ut, NULL);
    }
    // confirm prediction
    for (i = 200; i <= 400; i++) {
        ret = get_id_to_ut_from_layer2_arr(&l2_arr, &id_to_ut_found, i);
        if (ret != 0) {
            printf("got error code other than 0\n");
            printf("expected behaviour error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test11;
        }
    }
    // allocate 501 units, which should use index 500 to 1000
    for (i = 0; i <= 500; i++) {
        add_id_to_ut_to_layer2_arr(&l2_arr, &id_to_ut, NULL);
    }
    // confirm prediction
    for (i = 500; i <= 1000; i++) {
        ret = get_id_to_ut_from_layer2_arr(&l2_arr, &id_to_ut_found, i);
        if (ret != 0) {
            printf("got error code other than 0\n");
            printf("expected behaviour error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test11;
        }
    }
    // allocate 1001 units, which should use index 2000 to 3000
    for (i = 0; i <= 1000; i++) {
        add_id_to_ut_to_layer2_arr(&l2_arr, &id_to_ut, NULL);
    }
    // confirm prediction
    for (i = 2000; i <= 3000; i++) {
        ret = get_id_to_ut_from_layer2_arr(&l2_arr, &id_to_ut_found, i);
        if (ret != 0) {
            printf("got error code other than 0\n");
            printf("expected behaviour error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test11;
        }
    }

end_test11:

    printf("test area 12 : cleanup\n");
    incre_check();
    // clear everything
    ret = del_l2_id_to_ut_arr(&l2_arr);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test12;
    }

end_test12:

    announce_test_end();
    report_stat();
    print_test_tag_for_report_collector(test_add_get_del_with_layer2_arr);
    
    return error_num;
}

// depends on layer2 array to be correct, for the buffer filling function
// well supposedly correct, cause tests cannot prove correctness yadadadada
int test_add_lookup_buffer_del_with_exist_mat(int ret_test_l2_arr) {
    int i;

    utester tdum;
    utester tdum2;
    t_id_to_ut* id_to_ut;
    layer2_id_to_ut_arr l2_arr;
    uint64_t index_in_arr;

    id_exist_mat matrix;
    simple_bitmap map_buffer;
    map_block raw_map_buffer[get_bitmap_map_block_number(MAT_MAX_INDEX)];
    simple_bitmap map_buffer2;
    map_block raw_map_buffer2[get_bitmap_map_block_number(MAT_MAX_INDEX)];

    t_id_to_ut* tran_buffer[100];
    const uint64_t buffer_size = 100;
    uint64_t num_used;

    int ret;

    add_trackers();

    announce_test(test_add_lookup_buffer_del_with_exist_mat);
    announce_test_begin();

    skip_if_prereq_failed(ret_test_l2_arr);

    // setup data object
    strcpy(tdum.id, "id_abc");
    strcpy(tdum2.id, "id_def");

    // setup translation structures
    init_id_exist_mat(&matrix);
    init_layer2_id_to_ut_arr(&l2_arr);
    bitmap_init(&map_buffer, raw_map_buffer, NULL, MAT_MAX_INDEX, 0);
    bitmap_init(&map_buffer2, raw_map_buffer2, NULL, MAT_MAX_INDEX, 0);

    // empty lookup via existence matrix
    printf("test area 1 : lookup when existence matrix is empty\n");
    incre_check();
    ret = lookup_id_part(&l2_arr, &matrix, "foobar", &map_buffer, &map_buffer2, tran_buffer, buffer_size, &num_used);
    if (num_used != 0) {
        printf("number of slots in buffer filled was reported to be non-zero\n");
        printf("expected behaviour : report 0 slots filled\n");
        printf("reported value : %"PRIu64"\n", num_used);
        incre_error();
        goto end_test1;
    }
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test1;
    }
    for (i = 0; i < buffer_size; i++) {
        if (tran_buffer[i] != NULL) {
            printf("slot %d in buffer was not 0/null\n", i);
            printf("expected behaviour : all slots to be 0/null\n");
            printf("filled slot : %p\n", tran_buffer[i]);
            incre_error();
            goto end_test1;
        }
    }

end_test1:

    // invalid delete when empty
    printf("test area 2 : invalid delete of entry when matrix is empty\n");
    incre_check();
    ret = del_id_from_id_exist_mat(&matrix, &l2_arr, 0);
    // should return FIND_FAIL
    // as it uses get_id_to_ut_from_layer2_arr to retrieve the translation structure first
    if (ret != FIND_FAIL) {
        printf("got error code other than FIND_FAIL\n");
        printf("expected behaviour : error code to be FIND_FAIL\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test2;
    }

end_test2:

    // add test data to layer 2 array, layer 2 array related functions assumed to be correct
    add_id_to_ut_to_layer2_arr(&l2_arr, &id_to_ut, &index_in_arr);
    // init
    init_id_to_ut(id_to_ut);
    // linkage creation
    link_to_tran(&tdum, id_to_ut)
    // add to existence matrix
    add_id_to_id_exist_mat(&matrix, tdum.id, index_in_arr);
    // full text lookup on non-empty structure
    printf("test area 3 : full text lookup when one entry exists in matrix\n");
    incre_check();
    ret = lookup_id_part(&l2_arr, &matrix, tdum.id, &map_buffer, &map_buffer2, tran_buffer, buffer_size, &num_used);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test3;
    }
    if (num_used != 1) {
        printf("number of slots used in buffer is not one\n");
        printf("expected behaviour : number used to be exactly one\n");
        printf("number used reported : %"PRIu64"\n", num_used);
        incre_error();
        goto end_test3;
    }
    if (strcmp(tran_buffer[0]->str, tdum.id) != 0) {
        printf("retrieved id_to_ut does not contain the correct id\n");
        printf("expected behaviour : retrieved translation structure to contain the id of tdum\n");
        printf("id of retrieved translation structure : %s\n", tran_buffer[0]->str);
        printf("id of tdum : %s\n", tdum.id);
        incre_error();
        goto end_test3;
    }
    if (tran_buffer[0]->head_tar != &tdum) {
        printf("retrieved id_to_ut head pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut head_tar pointer : %p\n", tran_buffer[0]->head_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test3;
    }
    if (tran_buffer[0]->tail_tar != &tdum) {
        printf("retrieved id_to_ut tail pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut tail_tar pointer : %p\n", tran_buffer[0]->tail_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test3;
    }

end_test3:

    // invalid delete when not empty
    printf("test area 4 : invalid delete of entry when matrix is not empty\n");
    incre_check();
    ret = del_id_from_id_exist_mat(&matrix, &l2_arr, 2);
    // should return FIND_FAIL
    // as it uses get_id_to_ut_from_layer2_arr to retrieve the translation structure first
    if (ret != FIND_FAIL) {
        printf("got error code other than FIND_FAIL\n");
        printf("expected behaviour : error code to be FIND_FAIL\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test4;
    }

end_test4:

    // valid delete of added entry from test area 2
    // technically should also delete from layer 2 array
    // but since this is the only entry, and the only entry in its layer 1 array
    // so existence matrix will register the entire layer 1 array as having no
    // matching entries
    // thus lookup will fail regardless
    // regardles of whether the uniq_char structure is deleted or not
    printf("test area 5 : continuing from test area 2, deleting valid entry\n");
    incre_check();
    ret = del_id_from_id_exist_mat(&matrix, &l2_arr, 0);
    if (ret != 0) {
        printf("function del_id_from_id_exist_mat\n");
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test5;
    }
    // confirm via lookup
    ret = lookup_id_part(&l2_arr, &matrix, tdum.id, &map_buffer, &map_buffer2, tran_buffer, buffer_size, &num_used);
    if (ret != 0) {
        printf("function lookup_id_part\n");
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test5;
    }
    if (num_used != 0) {
        printf("number of slots used in buffer is not zero\n");
        printf("expected behaviour : number used to be exactly one\n");
        printf("number used reported : %"PRIu64"\n", num_used);
        incre_error();
        goto end_test5;
    }

end_test5:

    // add same entry many times
    printf("test area 6 : adding same entry multiple times\n");
    incre_check();
    for (i = 0; i < LARGE_INT; i++) {
        ret = add_id_to_id_exist_mat(&matrix, tdum.id, index_in_arr);
        if (ret != 0) {
            printf("got error code other than 0\n");
            printf("expected behaviour : error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test6;
        }
    }
    // confirm via lookup
    ret = lookup_id_part(&l2_arr, &matrix, tdum.id, &map_buffer, &map_buffer2, tran_buffer, buffer_size, &num_used);
    if (ret != 0) {
        printf("function lookup_id_part\n");
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test6;
    }
    if (num_used != 1) {
        printf("number of slots used in buffer is not one\n");
        printf("expected behaviour : number used to be exactly one\n");
        printf("number used reported : %"PRIu64"\n", num_used);
        incre_error();
        goto end_test6;
    }

end_test6:

    // lookup via partial match, continuing from test area 6, with only one entry in matrix
    printf("test area 7 : continuing from test area 6, valid lookups using partial text with one entry in matrix\n");
    incre_check();
    // search with text "id_"
    ret = lookup_id_part(&l2_arr, &matrix, "id_", &map_buffer, &map_buffer2, tran_buffer, buffer_size, &num_used);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test7;
    }
    if (num_used != 1) {
        printf("number of slots used in buffer is not one\n");
        printf("expected behaviour : number used to be exactly one\n");
        printf("number used reported : %"PRIu64"\n", num_used);
        incre_error();
        goto end_test7;
    }
    if (strcmp(tran_buffer[0]->str, tdum.id) != 0) {
        printf("retrieved id_to_ut does not contain the correct id\n");
        printf("expected behaviour : retrieved translation structure to contain the id of tdum\n");
        printf("id of retrieved translation structure : %s\n", tran_buffer[0]->str);
        printf("id of tdum : %s\n", tdum.id);
        incre_error();
        goto end_test7;
    }
    if (tran_buffer[0]->head_tar != &tdum) {
        printf("retrieved id_to_ut head pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut head_tar pointer : %p\n", tran_buffer[0]->head_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test7;
    }
    if (tran_buffer[0]->tail_tar != &tdum) {
        printf("retrieved id_to_ut tail pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut tail_tar pointer : %p\n", tran_buffer[0]->tail_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test7;
    }
    // search with text "_ab"
    ret = lookup_id_part(&l2_arr, &matrix, "_ab", &map_buffer, &map_buffer2, tran_buffer, buffer_size, &num_used);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test7;
    }
    if (num_used != 1) {
        printf("number of slots used in buffer is not one\n");
        printf("expected behaviour : number used to be exactly one\n");
        printf("number used reported : %"PRIu64"\n", num_used);
        incre_error();
        goto end_test7;
    }
    if (strcmp(tran_buffer[0]->str, tdum.id) != 0) {
        printf("retrieved id_to_ut does not contain the correct id\n");
        printf("expected behaviour : retrieved translation structure to contain the id of tdum\n");
        printf("id of retrieved translation structure : %s\n", tran_buffer[0]->str);
        printf("id of tdum : %s\n", tdum.id);
        incre_error();
        goto end_test7;
    }
    if (tran_buffer[0]->head_tar != &tdum) {
        printf("retrieved id_to_ut head pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut head_tar pointer : %p\n", tran_buffer[0]->head_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test7;
    }
    if (tran_buffer[0]->tail_tar != &tdum) {
        printf("retrieved id_to_ut tail pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut tail_tar pointer : %p\n", tran_buffer[0]->tail_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test7;
    }
    // search with text "abc"
    ret = lookup_id_part(&l2_arr, &matrix, "abc", &map_buffer, &map_buffer2, tran_buffer, buffer_size, &num_used);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test7;
    }
    if (num_used != 1) {
        printf("number of slots used in buffer is not one\n");
        printf("expected behaviour : number used to be exactly one\n");
        printf("number used reported : %"PRIu64"\n", num_used);
        incre_error();
        goto end_test7;
    }
    if (strcmp(tran_buffer[0]->str, tdum.id) != 0) {
        printf("retrieved id_to_ut does not contain the correct id\n");
        printf("expected behaviour : retrieved translation structure to contain the id of tdum\n");
        printf("id of retrieved translation structure : %s\n", tran_buffer[0]->str);
        printf("id of tdum : %s\n", tdum.id);
        incre_error();
        goto end_test7;
    }
    if (tran_buffer[0]->head_tar != &tdum) {
        printf("retrieved id_to_ut head pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut head_tar pointer : %p\n", tran_buffer[0]->head_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test7;
    }
    if (tran_buffer[0]->tail_tar != &tdum) {
        printf("retrieved id_to_ut tail pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut tail_tar pointer : %p\n", tran_buffer[0]->tail_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test7;
    }

end_test7:

    // invalid partial lookups
    printf("test area 8 : invalid lookups using partial text\n");
    incre_check();
    // search with text "abcd"
    ret = lookup_id_part(&l2_arr, &matrix, "abcd", &map_buffer, &map_buffer2, tran_buffer, buffer_size, &num_used);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test8;
    }
    if (num_used != 0) {
        printf("number of slots used in buffer is not zero\n");
        printf("expected behaviour : number used to be exactly zero\n");
        printf("number used reported : %"PRIu64"\n", num_used);
        incre_error();
        goto end_test8;
    }
    if (strcmp(tran_buffer[0]->str, tdum.id) != 0) {
        printf("retrieved id_to_ut does not contain the correct id\n");
        printf("expected behaviour : retrieved translation structure to contain the id of tdum\n");
        printf("id of retrieved translation structure : %s\n", tran_buffer[0]->str);
        printf("id of tdum : %s\n", tdum.id);
        incre_error();
        goto end_test8;
    }
    if (tran_buffer[0]->head_tar != &tdum) {
        printf("retrieved id_to_ut head pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut head_tar pointer : %p\n", tran_buffer[0]->head_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test8;
    }
    if (tran_buffer[0]->tail_tar != &tdum) {
        printf("retrieved id_to_ut tail pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut tail_tar pointer : %p\n", tran_buffer[0]->tail_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test8;
    }
    // search with text "id_def"
    ret = lookup_id_part(&l2_arr, &matrix, "id_def", &map_buffer, &map_buffer2, tran_buffer, buffer_size, &num_used);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test8;
    }
    if (num_used != 0) {
        printf("number of slots used in buffer is not zero\n");
        printf("expected behaviour : number used to be exactly zero\n");
        printf("number used reported : %"PRIu64"\n", num_used);
        incre_error();
        goto end_test8;
    }
    if (strcmp(tran_buffer[0]->str, tdum.id) != 0) {
        printf("retrieved id_to_ut does not contain the correct id\n");
        printf("expected behaviour : retrieved translation structure to contain the id of tdum\n");
        printf("id of retrieved translation structure : %s\n", tran_buffer[0]->str);
        printf("id of tdum : %s\n", tdum.id);
        incre_error();
        goto end_test8;
    }
    if (tran_buffer[0]->head_tar != &tdum) {
        printf("retrieved id_to_ut head pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut head_tar pointer : %p\n", tran_buffer[0]->head_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test8;
    }
    if (tran_buffer[0]->tail_tar != &tdum) {
        printf("retrieved id_to_ut tail pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut tail_tar pointer : %p\n", tran_buffer[0]->tail_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test8;
    }
    // search with text "id__"
    ret = lookup_id_part(&l2_arr, &matrix, "id__", &map_buffer, &map_buffer2, tran_buffer, buffer_size, &num_used);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test8;
    }
    if (num_used != 0) {
        printf("number of slots used in buffer is not zero\n");
        printf("expected behaviour : number used to be exactly zero\n");
        printf("number used reported : %"PRIu64"\n", num_used);
        incre_error();
        goto end_test8;
    }
    if (strcmp(tran_buffer[0]->str, tdum.id) != 0) {
        printf("retrieved id_to_ut does not contain the correct id\n");
        printf("expected behaviour : retrieved translation structure to contain the id of tdum\n");
        printf("id of retrieved translation structure : %s\n", tran_buffer[0]->str);
        printf("id of tdum : %s\n", tdum.id);
        incre_error();
        goto end_test8;
    }
    if (tran_buffer[0]->head_tar != &tdum) {
        printf("retrieved id_to_ut head pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut head_tar pointer : %p\n", tran_buffer[0]->head_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test8;
    }
    if (tran_buffer[0]->tail_tar != &tdum) {
        printf("retrieved id_to_ut tail pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut tail_tar pointer : %p\n", tran_buffer[0]->tail_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test8;
    }
    // search with text "id_z"
    ret = lookup_id_part(&l2_arr, &matrix, "id_z", &map_buffer, &map_buffer2, tran_buffer, buffer_size, &num_used);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test8;
    }
    if (num_used != 0) {
        printf("number of slots used in buffer is not zero\n");
        printf("expected behaviour : number used to be exactly zero\n");
        printf("number used reported : %"PRIu64"\n", num_used);
        incre_error();
        goto end_test8;
    }
    if (strcmp(tran_buffer[0]->str, tdum.id) != 0) {
        printf("retrieved id_to_ut does not contain the correct id\n");
        printf("expected behaviour : retrieved translation structure to contain the id of tdum\n");
        printf("id of retrieved translation structure : %s\n", tran_buffer[0]->str);
        printf("id of tdum : %s\n", tdum.id);
        incre_error();
        goto end_test8;
    }
    if (tran_buffer[0]->head_tar != &tdum) {
        printf("retrieved id_to_ut head pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut head_tar pointer : %p\n", tran_buffer[0]->head_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test8;
    }
    if (tran_buffer[0]->tail_tar != &tdum) {
        printf("retrieved id_to_ut tail pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut tail_tar pointer : %p\n", tran_buffer[0]->tail_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test8;
    }
    // search with text "id_abcdefg"
    ret = lookup_id_part(&l2_arr, &matrix, "id_abcdefg", &map_buffer, &map_buffer2, tran_buffer, buffer_size, &num_used);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test8;
    }
    if (num_used != 0) {
        printf("number of slots used in buffer is not zero\n");
        printf("expected behaviour : number used to be exactly zero\n");
        printf("number used reported : %"PRIu64"\n", num_used);
        incre_error();
        goto end_test8;
    }
    if (strcmp(tran_buffer[0]->str, tdum.id) != 0) {
        printf("retrieved id_to_ut does not contain the correct id\n");
        printf("expected behaviour : retrieved translation structure to contain the id of tdum\n");
        printf("id of retrieved translation structure : %s\n", tran_buffer[0]->str);
        printf("id of tdum : %s\n", tdum.id);
        incre_error();
        goto end_test8;
    }
    if (tran_buffer[0]->head_tar != &tdum) {
        printf("retrieved id_to_ut head pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut head_tar pointer : %p\n", tran_buffer[0]->head_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test8;
    }
    if (tran_buffer[0]->tail_tar != &tdum) {
        printf("retrieved id_to_ut tail pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut tail_tar pointer : %p\n", tran_buffer[0]->tail_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test8;
    }
    // search with text "id_000000000000000000000000000000000000000000000000"
    // the string is longer than UTEST_ID_LENGTH, thus should fail in verification of string
    ret = lookup_id_part(&l2_arr, &matrix, "id_000000000000000000000000000000000000000000000000", &map_buffer, &map_buffer2, tran_buffer, buffer_size, &num_used);
    if (ret != VERIFY_FAIL) {
        printf("got error code other than VERIFY_FAIL\n");
        printf("expected behaviour : error code to be VERIFY_FAIL\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test8;
    }
    if (num_used != 0) {
        printf("number of slots used in buffer is not zero\n");
        printf("expected behaviour : number used to be exactly zero\n");
        printf("number used reported : %"PRIu64"\n", num_used);
        incre_error();
        goto end_test8;
    }
    if (strcmp(tran_buffer[0]->str, tdum.id) != 0) {
        printf("retrieved id_to_ut does not contain the correct id\n");
        printf("expected behaviour : retrieved translation structure to contain the id of tdum\n");
        printf("id of retrieved translation structure : %s\n", tran_buffer[0]->str);
        printf("id of tdum : %s\n", tdum.id);
        incre_error();
        goto end_test8;
    }
    if (tran_buffer[0]->head_tar != &tdum) {
        printf("retrieved id_to_ut head pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut head_tar pointer : %p\n", tran_buffer[0]->head_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test8;
    }
    if (tran_buffer[0]->tail_tar != &tdum) {
        printf("retrieved id_to_ut tail pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut tail_tar pointer : %p\n", tran_buffer[0]->tail_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test8;
    }
    // search with text "0123456789"
    // the text is exactly length of UTEST_ID_LENGTH(UTEST_ID_LENGTH = 10 assuming)
    // thus should pass in verification of string
    ret = lookup_id_part(&l2_arr, &matrix, "0123456789", &map_buffer, &map_buffer2, tran_buffer, buffer_size, &num_used);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test8;
    }
    if (num_used != 0) {
        printf("number of slots used in buffer is not zero\n");
        printf("expected behaviour : number used to be exactly zero\n");
        printf("number used reported : %"PRIu64"\n", num_used);
        incre_error();
        goto end_test8;
    }
    if (strcmp(tran_buffer[0]->str, tdum.id) != 0) {
        printf("retrieved id_to_ut does not contain the correct id\n");
        printf("expected behaviour : retrieved translation structure to contain the id of tdum\n");
        printf("id of retrieved translation structure : %s\n", tran_buffer[0]->str);
        printf("id of tdum : %s\n", tdum.id);
        incre_error();
        goto end_test8;
    }
    if (tran_buffer[0]->head_tar != &tdum) {
        printf("retrieved id_to_ut head pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut head_tar pointer : %p\n", tran_buffer[0]->head_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test8;
    }
    if (tran_buffer[0]->tail_tar != &tdum) {
        printf("retrieved id_to_ut tail pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut tail_tar pointer : %p\n", tran_buffer[0]->tail_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test8;
    }

end_test8:

    // add second test data to layer 2 array, layer 2 array functions assumed to be correct
    add_id_to_ut_to_layer2_arr(&l2_arr, &id_to_ut, &index_in_arr);
    // create linkage
    link_to_tran(&tdum2, id_to_ut)
    // add to existence matrix
    add_id_to_id_exist_mat(&matrix, tdum2.id, index_in_arr);
    // valid partial lookups with two entries in matrix
    printf("test area 9 : continuing from test area 8, valid partial text lookups with two entires in matrix\n");
    incre_check();
    // search with text "id_"
    ret = lookup_id_part(&l2_arr, &matrix, "id_", &map_buffer, &map_buffer2, tran_buffer, buffer_size, &num_used);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test9;
    }
    if (num_used != 2) {
        printf("number of slots used in buffer is not two\n");
        printf("expected behaviour : number used to be exactly two\n");
        printf("number used reported : %"PRIu64"\n", num_used);
        incre_error();
        goto end_test9;
    }
    // technically ordering does not matter and is not within any requirement
    // but here assumes sequential order(which holds for current implementation)
    // first entry, with str : "id_abc"
    if (strcmp(tran_buffer[0]->str, tdum.id) != 0) {
        printf("retrieved id_to_ut does not contain the correct id\n");
        printf("expected behaviour : retrieved translation structure to contain the id of tdum\n");
        printf("id of retrieved translation structure : %s\n", tran_buffer[0]->str);
        printf("id of tdum : %s\n", tdum.id);
        incre_error();
        goto end_test9;
    }
    if (tran_buffer[0]->head_tar != &tdum) {
        printf("retrieved id_to_ut head pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut head_tar pointer : %p\n", tran_buffer[0]->head_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test9;
    }
    if (tran_buffer[0]->tail_tar != &tdum) {
        printf("retrieved id_to_ut tail pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut tail_tar pointer : %p\n", tran_buffer[0]->tail_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test9;
    }
    // second entry, with str : "id_def"
    if (strcmp(tran_buffer[1]->str, tdum2.id) != 0) {
        printf("retrieved id_to_ut does not contain the correct id\n");
        printf("expected behaviour : retrieved translation structure to contain the id of tdum2\n");
        printf("id of retrieved translation structure : %s\n", tran_buffer[1]->str);
        printf("id of tdum : %s\n", tdum2.id);
        incre_error();
        goto end_test9;
    }
    if (tran_buffer[1]->head_tar != &tdum2) {
        printf("retrieved id_to_ut head pointer is not pointing to tdum2\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut head_tar pointer : %p\n", tran_buffer[1]->head_tar);
        printf("address of tdum : %p\n", &tdum2);
        incre_error();
        goto end_test9;
    }
    if (tran_buffer[1]->tail_tar != &tdum2) {
        printf("retrieved id_to_ut tail pointer is not pointing to tdum2\n");
        printf("expected behaviour : to point to tdum2\n");
        printf("address stored in id_to_ut tail_tar pointer : %p\n", tran_buffer[1]->tail_tar);
        printf("address of tdum : %p\n", &tdum2);
        incre_error();
        goto end_test9;
    }

end_test9:

    // delete one of the entry and confirm the other entry remains intact
    printf("test area 10 : delete entries and confirm other entries remain intact\n");
    incre_check();
    ret = del_id_from_id_exist_mat(&matrix, &l2_arr, 0);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test10;
    }
    ret = del_id_to_ut_from_layer2_arr(&l2_arr, 0);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test10;
    }
    // confirm deletion of first entry via lookup
    ret = lookup_id_part(&l2_arr, &matrix, tdum.id, &map_buffer, &map_buffer2, tran_buffer, buffer_size, &num_used);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test10;
    }
    if (num_used != 0) {
        printf("number of slots used in buffer is not zero\n");
        printf("expected behaviour : number used to be exactly zero\n");
        printf("number used reported : %"PRIu64"\n", num_used);
        incre_error();
        goto end_test10;
    }
    // confirm second entry is still intact
    ret = lookup_id_part(&l2_arr, &matrix, tdum2.id, &map_buffer, &map_buffer2, tran_buffer, buffer_size, &num_used);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test10;
    }
    if (num_used != 1) {
        printf("number of slots used in buffer is not one\n");
        printf("expected behaviour : number used to be exactly one\n");
        printf("number used reported : %"PRIu64"\n", num_used);
        incre_error();
        goto end_test10;
    }
    if (strcmp(tran_buffer[0]->str, tdum2.id) != 0) {
        printf("retrieved id_to_ut does not contain the correct id\n");
        printf("expected behaviour : retrieved translation structure to contain the id of tdum2\n");
        printf("id of retrieved translation structure : %s\n", tran_buffer[0]->str);
        printf("id of tdum : %s\n", tdum2.id);
        incre_error();
        goto end_test10;
    }
    if (tran_buffer[0]->head_tar != &tdum2) {
        printf("retrieved id_to_ut head pointer is not pointing to tdum2\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut head_tar pointer : %p\n", tran_buffer[0]->head_tar);
        printf("address of tdum : %p\n", &tdum2);
        incre_error();
        goto end_test10;
    }
    if (tran_buffer[0]->tail_tar != &tdum2) {
        printf("retrieved id_to_ut tail pointer is not pointing to tdum2\n");
        printf("expected behaviour : to point to tdum2\n");
        printf("address stored in id_to_ut tail_tar pointer : %p\n", tran_buffer[0]->tail_tar);
        printf("address of tdum : %p\n", &tdum2);
        incre_error();
        goto end_test10;
    }
    // add another entry and delete second entry
    add_id_to_ut_to_layer2_arr(&l2_arr, &id_to_ut, &index_in_arr);
    // init
    init_id_to_ut(id_to_ut);
    // linkage creation
    link_to_tran(&tdum, id_to_ut);
    add_id_to_id_exist_mat(&matrix, tdum.id, index_in_arr);
    // delete the second entry
    ret = del_id_from_id_exist_mat(&matrix, &l2_arr, 1);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test10;
    }
    ret = del_id_to_ut_from_layer2_arr(&l2_arr, 1);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test10;
    }
    // confirm second entry is gone
    ret = lookup_id_part(&l2_arr, &matrix, tdum2.id, &map_buffer, &map_buffer2, tran_buffer, buffer_size, &num_used);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test10;
    }
    if (num_used != 0) {
        printf("number of slots used in buffer is not zero\n");
        printf("expected behaviour : number used to be exactly zero\n");
        printf("number used reported : %"PRIu64"\n", num_used);
        incre_error();
        goto end_test10;
    }
    // confirm third entry is still intact
    ret = lookup_id_part(&l2_arr, &matrix, tdum.id, &map_buffer, &map_buffer2, tran_buffer, buffer_size, &num_used);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test10;
    }
    if (num_used != 1) {
        printf("number of slots used in buffer is not one\n");
        printf("expected behaviour : number used to be exactly one\n");
        printf("number used reported : %"PRIu64"\n", num_used);
        incre_error();
        goto end_test10;
    }
    if (strcmp(tran_buffer[0]->str, tdum.id) != 0) {
        printf("retrieved id_to_ut does not contain the correct id\n");
        printf("expected behaviour : retrieved translation structure to contain the id of tdum\n");
        printf("id of retrieved translation structure : %s\n", tran_buffer[0]->str);
        printf("id of tdum : %s\n", tdum.id);
        incre_error();
        goto end_test10;
    }
    if (tran_buffer[0]->head_tar != &tdum) {
        printf("retrieved id_to_ut head pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut head_tar pointer : %p\n", tran_buffer[0]->head_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test10;
    }
    if (tran_buffer[0]->tail_tar != &tdum) {
        printf("retrieved id_to_ut tail pointer is not pointing to tdum\n");
        printf("expected behaviour : to point to tdum\n");
        printf("address stored in id_to_ut tail_tar pointer : %p\n", tran_buffer[0]->tail_tar);
        printf("address of tdum : %p\n", &tdum);
        incre_error();
        goto end_test10;
    }

end_test10:

    printf("test area 11 : cleanup\n");
    incre_check();
    // clear everything
    ret = del_l2_id_to_ut_arr(&l2_arr);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test11;
    }
    ret = del_id_exist_mat(&matrix);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test11;
    }

end_test11:

    announce_test_end();
    report_stat();
    print_test_tag_for_report_collector(test_add_lookup_buffer_del_with_exist_mat);

    return error_num;
}

// no dependencies on correctness of other structures
// map generation does not require access to layer 2 array
int test_add_lookup_map_del_with_exist_mat(int ret_test_l2_arr, int ret_test_exist_mat) {
    int i;

    utester tdum;
    utester tdum2;
    t_id_to_ut* id_to_ut;
    layer2_id_to_ut_arr l2_arr;
    uint64_t index_in_arr;

    id_exist_mat matrix;
    simple_bitmap map_buffer;
    map_block raw_map_buffer[get_bitmap_map_block_number(MAT_MAX_INDEX)];
    simple_bitmap map_result;
    map_block raw_map_result[get_bitmap_map_block_number(MAT_MAX_INDEX)];

    int ret;

    add_trackers();

    announce_test(test_add_lookup_map_del_with_exist_mat);
    announce_test_begin();

    skip_if_prereq_failed(ret_test_l2_arr);
    skip_if_prereq_failed(ret_test_exist_mat);

    // setup data object
    strcpy(tdum.id, "id_abc");
    strcpy(tdum2.id, "id_def");

    // setup translation structures
    init_id_exist_mat(&matrix);
    init_layer2_id_to_ut_arr(&l2_arr);
    bitmap_init(&map_buffer, raw_map_buffer, NULL, MAT_MAX_INDEX, 0);
    bitmap_init(&map_result, raw_map_result, NULL, MAT_MAX_INDEX, 0);

    // empty lookup via existence matrix
    printf("test area 1 : lookup when existence matrix is empty\n");
    incre_check();
    ret = lookup_id_part_map(&matrix, "foobar", &map_buffer, &map_result);
    if (map_result.number_of_ones != 0) {
        printf("number of layer 1 array containing the entry reported to be non-zero\n");
        printf("expected behaviour : report zero layer 1 array matching\n");
        printf("reported number : %"PRIu64"\n", map_result.number_of_ones);
        incre_error();
        goto end_test1;
    }
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test1;
    }

end_test1:

    // invalid delete when empty
    printf("test area 2 : invalid delete of entry when matrix is empty\n");
    incre_check();
    ret = del_id_from_id_exist_mat(&matrix, &l2_arr, 0);
    // should return FIND_FAIL
    // as it uses get_id_to_ut_from_layer2_arr to retrieve the translation structure first
    if (ret != FIND_FAIL) {
        printf("got error code other than FIND_FAIL\n");
        printf("expected behaviour : error code to be FIND_FAIL\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test2;
    }

end_test2:

    // add test data to layer 2 array, layer 2 array related functions assumed to be correct
    add_id_to_ut_to_layer2_arr(&l2_arr, &id_to_ut, &index_in_arr);
    // init
    init_id_to_ut(id_to_ut);
    // linkage creation
    link_to_tran(&tdum, id_to_ut);
    // add to existence matrix
    add_id_to_id_exist_mat(&matrix, tdum.id, index_in_arr);
    // full text lookup on non-empty structure
    printf("test area 3 : full text lookup when one entry exists in matrix\n");
    incre_check();
    ret = lookup_id_part_map(&matrix, tdum.id, &map_buffer, &map_result);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test3;
    }
    if (map_result.number_of_ones != 1) {
        printf("number of layer 1 array containing the entry reported to be not one\n");
        printf("expected behaviour : report one layer 1 array matching\n");
        printf("reported number : %"PRIu64"\n", map_result.number_of_ones);
        incre_error();
        goto end_test3;
    }

end_test3:

    // invalid delete when not empty
    printf("test area 4 : invalid delete of entry when matrix is not empty\n");
    incre_check();
    ret = del_id_from_id_exist_mat(&matrix, &l2_arr, 2);
    // should return FIND_FAIL
    // as it uses get_id_to_ut_from_layer2_arr to retrieve the translation structure first
    if (ret != FIND_FAIL) {
        printf("got error code other than FIND_FAIL\n");
        printf("expected behaviour : error code to be FIND_FAIL\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test4;
    }

end_test4:

    // valid delete of added entry from test area 2
    // technically should also delete from layer 2 array
    // but since this is the only entry, and the only entry in its layer 1 array
    // so existence matrix will register the entire layer 1 array as having no
    // matching entries
    // thus lookup will fail regardless
    // regardles of whether the uniq_char structure is deleted or not
    printf("test area 5 : continuing from test area 2, deleting valid entry\n");
    incre_check();
    ret = del_id_from_id_exist_mat(&matrix, &l2_arr, 0);
    if (ret != 0) {
        printf("function del_id_from_id_exist_mat\n");
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test5;
    }
    // confirm via lookup
    ret = lookup_id_part_map(&matrix, tdum.id, &map_buffer, &map_result);
    if (ret != 0) {
        printf("function lookup_id_part\n");
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test5;
    }
    if (map_result.number_of_ones != 0) {
        printf("number of layer 1 array containing the entry reported to be not zero\n");
        printf("expected behaviour : report zero layer 1 array matching\n");
        printf("reported number : %"PRIu64"\n", map_result.number_of_ones);
        incre_error();
        goto end_test5;
    }

end_test5:

    // add same entry many times
    printf("test area 6 : adding same entry multiple times\n");
    incre_check();
    for (i = 0; i < LARGE_INT; i++) {
        ret = add_id_to_id_exist_mat(&matrix, tdum.id, index_in_arr);
        if (ret != 0) {
            printf("got error code other than 0\n");
            printf("expected behaviour : error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test6;
        }
    }
    // confirm via lookup
    ret = lookup_id_part_map(&matrix, tdum.id, &map_buffer, &map_result);
    if (ret != 0) {
        printf("function lookup_id_part_map\n");
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test6;
    }
    if (map_result.number_of_ones != 1) {
        printf("number of layer 1 array containing the entry reported to be not one\n");
        printf("expected behaviour : report one layer 1 array matching\n");
        printf("number used reported : %"PRIu64"\n", map_result.number_of_ones);
        incre_error();
        goto end_test6;
    }

end_test6:

    // lookup via partial match, continuing from test area 6, with only one entry in matrix
    printf("test area 7 : continuing from test area 6, valid lookups using partial text with one entry in matrix\n");
    incre_check();
    // search with text "id_"
    ret = lookup_id_part_map(&matrix, "id_", &map_buffer, &map_result);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test7;
    }
    if (map_result.number_of_ones != 1) {
        printf("number of layer 1 array containing the entry reported to be not one\n");
        printf("expected behaviour : report one layer 1 array matching\n");
        printf("number used reported : %"PRIu64"\n", map_result.number_of_ones);
        incre_error();
        goto end_test7;
    }
    // search with text "_ab"
    ret = lookup_id_part_map(&matrix, "_ab", &map_buffer, &map_result);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test7;
    }
    if (map_result.number_of_ones != 1) {
        printf("number of layer 1 array containing the entry reported to be not one\n");
        printf("expected behaviour : report one layer 1 array matching\n");
        printf("number used reported : %"PRIu64"\n", map_result.number_of_ones);
        incre_error();
        goto end_test7;
    }
    // search with text "abc"
    ret = lookup_id_part_map(&matrix, "abc", &map_buffer, &map_result);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test7;
    }
    if (map_result.number_of_ones != 1) {
        printf("number of layer 1 array containing the entry reported to be not one\n");
        printf("expected behaviour : report one layer 1 array matching\n");
        printf("number used reported : %"PRIu64"\n", map_result.number_of_ones);
        incre_error();
        goto end_test7;
    }

end_test7:

    // invalid partial lookups
    printf("test area 8 : invalid lookups using partial text\n");
    incre_check();
    // search with text "abcd"
    ret = lookup_id_part_map(&matrix, "abcd", &map_buffer, &map_result);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test8;
    }
    if (map_result.number_of_ones != 0) {
        printf("number of layer 1 array containing the entry reported to be not zero\n");
        printf("expected behaviour : report zero layer 1 array matching\n");
        printf("number used reported : %"PRIu64"\n", map_result.number_of_ones);
        incre_error();
        goto end_test8;
    }
    // search with text "id_def"
    ret = lookup_id_part_map(&matrix, "id_def", &map_buffer, &map_result);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test8;
    }
    if (map_result.number_of_ones != 0) {
        printf("number of layer 1 array containing the entry reported to be not zero\n");
        printf("expected behaviour : report zero layer 1 array matching\n");
        printf("number used reported : %"PRIu64"\n", map_result.number_of_ones);
        incre_error();
        goto end_test8;
    }
    // search with text "id__"
    ret = lookup_id_part_map(&matrix, "id__", &map_buffer, &map_result);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test8;
    }
    if (map_result.number_of_ones != 0) {
        printf("number of layer 1 array containing the entry reported to be not zero\n");
        printf("expected behaviour : report zero layer 1 array matching\n");
        printf("number used reported : %"PRIu64"\n", map_result.number_of_ones);
        incre_error();
        goto end_test8;
    }
    // search with text "id_z"
    ret = lookup_id_part_map(&matrix, "id_z", &map_buffer, &map_result);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test8;
    }
    if (map_result.number_of_ones != 0) {
        printf("number of layer 1 array containing the entry reported to be not zero\n");
        printf("expected behaviour : report zero layer 1 array matching\n");
        printf("number used reported : %"PRIu64"\n", map_result.number_of_ones);
        incre_error();
        goto end_test8;
    }
    // search with text "id_abcdefg"
    ret = lookup_id_part_map(&matrix, "id_abcdefg", &map_buffer, &map_result);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test8;
    }
    if (map_result.number_of_ones != 0) {
        printf("number of layer 1 array containing the entry reported to be not zero\n");
        printf("expected behaviour : report zero layer 1 array matching\n");
        printf("number used reported : %"PRIu64"\n", map_result.number_of_ones);
        incre_error();
        goto end_test8;
    }
    // search with text "id_000000000000000000000000000000000000000000000000"
    // the string is longer than UTEST_ID_LENGTH, thus should fail in verification of string
    ret = lookup_id_part_map(&matrix, "id_000000000000000000000000000000000000000000000000", &map_buffer, &map_result);
    if (ret != VERIFY_FAIL) {
        printf("got error code other than VERIFY_FAIL\n");
        printf("expected behaviour : error code to be VERIFY_FAIL\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test8;
    }
    if (map_result.number_of_ones != 0) {
        printf("number of layer 1 array containing the entry reported to be not zero\n");
        printf("expected behaviour : report zero layer 1 array matching\n");
        printf("number used reported : %"PRIu64"\n", map_result.number_of_ones);
        incre_error();
        goto end_test8;
    }
    // search with text "0123456789"
    // the text is exactly length of UTEST_ID_LENGTH(UTEST_ID_LENGTH = 10 assuming)
    // thus should pass in verification of string
    ret = lookup_id_part_map(&matrix, "0123456789", &map_buffer, &map_result);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test8;
    }
    if (map_result.number_of_ones != 0) {
        printf("number of layer 1 array containing the entry reported to be not zero\n");
        printf("expected behaviour : report zero layer 1 array matching\n");
        printf("number used reported : %"PRIu64"\n", map_result.number_of_ones);
        incre_error();
        goto end_test8;
    }

end_test8:

    // add second test data to layer 2 array, layer 2 array functions assumed to be correct
    add_id_to_ut_to_layer2_arr(&l2_arr, &id_to_ut, &index_in_arr);
    // init
    init_id_to_ut(id_to_ut);
    // linkage creation
    link_to_tran(&tdum2, id_to_ut);
    // add to existence matrix
    add_id_to_id_exist_mat(&matrix, tdum2.id, index_in_arr);
    // valid partial lookups with two entries in matrix
    printf("test area 9 : continuing from test area 8, valid partial text lookups with two entires in matrix\n");
    incre_check();
    // search with text "id_"
    ret = lookup_id_part_map(&matrix, "id_", &map_buffer, &map_result);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test9;
    }
    if (map_result.number_of_ones != 1) {
        printf("number of layer 1 array containing the entry reported to be not one\n");
        printf("expected behaviour : report one layer 1 array matching\n");
        printf("number used reported : %"PRIu64"\n", map_result.number_of_ones);
        incre_error();
        goto end_test9;
    }

end_test9:

    // test spreading over to another layer 1 array
    printf("test area 10 : test spreading over to another layer 1 array\n");
    incre_check();
    for (i = 0; i < UTEST_L1_SIZE; i++) {
        add_id_to_ut_to_layer2_arr(&l2_arr, &id_to_ut, &index_in_arr);
        // init
        init_id_to_ut(id_to_ut);
        // linkage creation
        link_to_tran(&tdum2, id_to_ut);
        add_id_to_id_exist_mat(&matrix, tdum2.id, index_in_arr);
    }
    // lookup id_def
    // should have two layer 1 array marked as containing matching entries
    ret = lookup_id_part_map(&matrix, "id_", &map_buffer, &map_result);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test10;
    }
    if (map_result.number_of_ones != 2) {
        printf("number of layer 1 array containing the entry reported to be not two\n");
        printf("expected behaviour : report two layer 1 array matching\n");
        printf("number used reported : %"PRIu64"\n", map_result.number_of_ones);
        incre_error();
        goto end_test10;
    }

end_test10:

    printf("test area 11 : cleanup\n");
    incre_check();
    // clear everything
    ret = del_l2_id_to_ut_arr(&l2_arr);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test11;
    }
    ret = del_id_exist_mat(&matrix);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test11;
    }

end_test11:

    announce_test_end();
    report_stat();
    print_test_tag_for_report_collector(test_add_lookup_map_del_with_exist_mat);

    return error_num;
}

int test_stress_add_del(int ret_test_l2_arr, int ret_test_exist_mat) {
    int i, j;

    utester* tdum;
    t_id_to_ut* id_to_ut;
    layer2_id_to_ut_arr l2_arr;
    uint64_t index_in_arr;

    id_exist_mat matrix;
    simple_bitmap map_buffer;
    map_block raw_map_buffer[get_bitmap_map_block_number(MAT_MAX_INDEX)];

    int ret = 0;

    uint64_t t1_index_arr[LARGE_INT];
    uint64_t t2_index_arr[LARGE_INT];

    time_t t;

    add_trackers();

    announce_test(test_stress_add);
    announce_test_begin();

    skip_if_prereq_failed(ret_test_l2_arr);
    skip_if_prereq_failed(ret_test_exist_mat);

    // does not need high quality randomness
    srand(0);

    init_id_exist_mat(&matrix);
    init_layer2_id_to_ut_arr(&l2_arr);
    bitmap_init(&map_buffer, raw_map_buffer, NULL, MAT_MAX_INDEX, 0);

    // stress test on both layer 2 array addition and existence matrix addition, using 0 as seed
    printf("test area 1 : stress test on adding to layer 2 array and existence matrix, fixed random seed for id generation\n");
    printf("        seed : 0\n");
    incre_check();
    for (i = 0; i < LARGE_INT; i++) {
        // create data object with mildly random string
        tdum = malloc(sizeof(utester));
        for (j = 0; j < UTEST_ID_LENGTH; j++) {
            tdum->id[j] = rand() % 128;  // should be good enough to avoid integer overflow
        }
        tdum->id[UTEST_ID_LENGTH] = 0;

        ret = add_id_to_ut_to_layer2_arr(&l2_arr, &id_to_ut, &index_in_arr);
        if (ret != 0) {
            printf("iteration : %d\n", i);
            printf("got error code other than 0\n");
            printf("expected behaviour : error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test1;
        }
        // init
        init_id_to_ut(id_to_ut);
        // linkage creation
        link_to_tran(tdum, id_to_ut);
        t1_index_arr[i] = index_in_arr;
        ret = add_id_to_id_exist_mat(&matrix, tdum->id, index_in_arr);
        if (ret != 0) {
            printf("iteration : %d\n", i);
            printf("got error code other than 0\n");
            printf("expected behaviour : error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test1;
        }
    }

end_test1:

    // stress test again, but using time as seed
    srand((unsigned) time(&t));
    printf("test area 2 : stress test on adding to layer 2 array and existence matrix, using time as random seed for id generation\n");
    printf("        seed : %ld\n", t);
    incre_check();
    for (i = 0; i < LARGE_INT; i++) {
        // create data object with mildly random string
        tdum = malloc(sizeof(utester));
        for (j = 0; j < UTEST_ID_LENGTH; j++) {
            tdum->id[j] = rand() % 128;  // should be good enough to avoid integer overflow
        }
        tdum->id[UTEST_ID_LENGTH] = 0;

        ret = add_id_to_ut_to_layer2_arr(&l2_arr, &id_to_ut, &index_in_arr);
        if (ret != 0) {
            printf("iteration : %d\n", i);
            printf("got error code other than 0\n");
            printf("expected behaviour : error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test2;
        }
        // init
        init_id_to_ut(id_to_ut);
        // linkage creation
        link_to_tran(tdum, id_to_ut);
        t2_index_arr[i] = index_in_arr;
        ret = add_id_to_id_exist_mat(&matrix, tdum->id, index_in_arr);
        if (ret != 0) {
            printf("iteration : %d\n", i);
            printf("got error code other than 0\n");
            printf("expected behaviour : error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test2;
        }
    }

end_test2:

    printf("test area 3 : continuing from test area 1, stress test on releasing all the entries\n");
    incre_check();
    for (i = 0; i < LARGE_INT; i++) {
        ret = del_id_from_id_exist_mat(&matrix, &l2_arr, t1_index_arr[i]);
        if (ret != 0) {
            printf("iteration : %d\n", i);
            printf("function : del_id_from_id_exist_mat\n");
            printf("got error code other than 0\n");
            printf("expected behaviour : error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test3;
        }
        ret = del_id_to_ut_from_layer2_arr(&l2_arr, t1_index_arr[i]);
        if (ret != 0) {
            printf("iteration : %d\n", i);
            printf("function : del_id_to_ut_from_layer2_arr\n");
            printf("got error code other than 0\n");
            printf("expected behaviour : error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test3;
        }
    }

end_test3:

    printf("test area 4 : continuing from test area 2, stress test on releasing all the entries\n");
    incre_check();
    for (i = 0; i < LARGE_INT; i++) {
        ret = del_id_from_id_exist_mat(&matrix, &l2_arr, t2_index_arr[i]);
        if (ret != 0) {
            printf("iteration : %d\n", i);
            printf("function : del_id_from_id_exist_mat\n");
            printf("got error code other than 0\n");
            printf("expected behaviour : error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test4;
        }
        ret = del_id_to_ut_from_layer2_arr(&l2_arr, t2_index_arr[i]);
        if (ret != 0) {
            printf("iteration : %d\n", i);
            printf("function : del_id_to_ut_from_layer2_arr\n");
            printf("got error code other than 0\n");
            printf("expected behaviour : error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test4;
        }
    }

end_test4:

    printf("test area 5 : test releasing layer 2 array\n");
    incre_check();
    ret = del_l2_id_to_ut_arr(&l2_arr);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test5;
    }

end_test5:

    printf("test area 6 : test releasing existence matrix\n");
    incre_check();
    ret = del_id_exist_mat(&matrix);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test6;
    }

end_test6:

    announce_test_end();
    report_stat();
    print_test_tag_for_report_collector(test_stress_add);

    return error_num;
}

int test_possibly_wonky_deletion(int ret_test_l2_arr, int ret_test_exist_mat) {
    int i;

    utester tdum;
    utester tdum2;
    t_id_to_ut* id_to_ut;
    layer2_id_to_ut_arr l2_arr;
    uint64_t index_in_arr;

    id_exist_mat matrix;
    simple_bitmap map_buffer;
    map_block raw_map_buffer[get_bitmap_map_block_number(MAT_MAX_INDEX)];
    simple_bitmap map_buffer2;
    map_block raw_map_buffer2[get_bitmap_map_block_number(MAT_MAX_INDEX)];
    simple_bitmap map_result;
    map_block raw_map_result[get_bitmap_map_block_number(MAT_MAX_INDEX)];

    t_id_to_ut* tran_buffer[100];
    const uint64_t buffer_size = 100;
    uint64_t num_used;

    int ret;

    add_trackers();

    announce_test(test_add_get_del_with_layer2_arr);
    announce_test_begin();

    skip_if_prereq_failed(ret_test_l2_arr);
    skip_if_prereq_failed(ret_test_exist_mat);

    // setup data object
    strcpy(tdum.id, "id_abc");
    strcpy(tdum2.id, "id_def");

    // setup translation structures
    init_id_exist_mat(&matrix);
    init_layer2_id_to_ut_arr(&l2_arr);
    bitmap_init(&map_buffer, raw_map_buffer, NULL, MAT_MAX_INDEX, 0);
    bitmap_init(&map_buffer2, raw_map_buffer2, NULL, MAT_MAX_INDEX, 0);
    bitmap_init(&map_result, raw_map_result, NULL, MAT_MAX_INDEX, 0);

    // test for when the target of deletion from matrix is the only matching entry in layer 1 array it's residing in, buffer version
    printf("test area 1 : target of deletion is the only matching entry in layer 1 array, lookup using both buffer version and map version\n");
    incre_check();
    // fill one entry
    ret = add_id_to_ut_to_layer2_arr(&l2_arr, &id_to_ut, &index_in_arr);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test1;
    }
    // init
    init_id_to_ut(id_to_ut);
    // linkage creation
    link_to_tran(&tdum, id_to_ut);
    ret = add_id_to_id_exist_mat(&matrix, tdum.id, index_in_arr);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test1;
    }
    // fill the remaining space in layer 1 array(spilling out to next layer 1 array doesn't hurt)
    for (i = 0; i < UTEST_L1_SIZE; i++) {
        ret = add_id_to_ut_to_layer2_arr(&l2_arr, &id_to_ut, &index_in_arr);
        if (ret != 0) {
            printf("got error code other than 0\n");
            printf("expected behaviour : error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test1;
        }
        // init
        init_id_to_ut(id_to_ut);
        // linkage creation
        link_to_tran(&tdum2, id_to_ut);
        ret = add_id_to_id_exist_mat(&matrix, tdum2.id, index_in_arr);
        if (ret != 0) {
            printf("got error code other than 0\n");
            printf("expected behaviour : error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test1;
        }
    }
    // now delete the first entry from existence matrix
    ret = del_id_from_id_exist_mat(&matrix, &l2_arr, 0);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test1;
    }
    // lookup should fail for both buffer and map version of lookup function
    // lookup using buffer version
    ret = lookup_id_part(&l2_arr, &matrix, tdum.id, &map_buffer, &map_buffer2, tran_buffer, buffer_size, &num_used);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test1;
    }
    if (num_used != 0) {
        printf("number of slots in buffer filled was reported to be non-zero\n");
        printf("expected behaviour : report 0 slots filled\n");
        printf("reported value : %"PRIu64"\n", num_used);
        incre_error();
        goto end_test1;
    }
    // lookup using map version
    ret = lookup_id_part_map(&matrix, tdum.id, &map_buffer, &map_result);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test1;
    }
    if (map_result.number_of_ones != 0) {
        printf("number of layer 1 array containing the entry reported to be non-zero\n");
        printf("expected behaviour : report zero layer 1 array matching\n");
        printf("reported number : %"PRIu64"\n", map_result.number_of_ones);
        incre_error();
        goto end_test1;
    }

end_test1:

    // clear everything
    ret = del_l2_id_to_ut_arr(&l2_arr);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test2;
    }
    ret = del_id_exist_mat(&matrix);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test2;
    }
    // init again
    init_id_exist_mat(&matrix);
    init_layer2_id_to_ut_arr(&l2_arr);
    bitmap_zero(&map_buffer);

    // test for when the target of deletion from matrix is NOT the only matching entry in layer 1 array it's residing in
    printf("test area 2 : target of deletion is not the only matching entry in layer 1 array, lookup using both buffer version and map version\n");
    incre_check();
    // fill one entry
    ret = add_id_to_ut_to_layer2_arr(&l2_arr, &id_to_ut, &index_in_arr);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test2;
    }
    // init
    init_id_to_ut(id_to_ut);
    // linkage creation
    link_to_tran(&tdum, id_to_ut);
    ret = add_id_to_id_exist_mat(&matrix, tdum.id, index_in_arr);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test2;
    }
    // fill up a bit (around half of layer 1 array)
    for (i = 0; i < UTEST_L1_SIZE / 2; i++) {
        ret = add_id_to_ut_to_layer2_arr(&l2_arr, &id_to_ut, &index_in_arr);
        if (ret != 0) {
            printf("got error code other than 0\n");
            printf("expected behaviour : error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test2;
        }
        // init
        init_id_to_ut(id_to_ut);
        // linkage creation
        link_to_tran(&tdum2, id_to_ut);
        ret = add_id_to_id_exist_mat(&matrix, tdum2.id, index_in_arr);
        if (ret != 0) {
            printf("got error code other than 0\n");
            printf("expected behaviour : error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test2;
        }
    }
    // fill another entry
    ret = add_id_to_ut_to_layer2_arr(&l2_arr, &id_to_ut, &index_in_arr);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test2;
    }
    // init
    init_id_to_ut(id_to_ut);
    // linkage creation
    link_to_tran(&tdum, id_to_ut);
    ret = add_id_to_id_exist_mat(&matrix, tdum.id, index_in_arr);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test2;
    }
    // fill up remaining space of layer 1 array, again spilling out doesn't hurt
    for (i = 0; i < UTEST_L1_SIZE / 2; i++) {
        ret = add_id_to_ut_to_layer2_arr(&l2_arr, &id_to_ut, &index_in_arr);
        if (ret != 0) {
            printf("got error code other than 0\n");
            printf("expected behaviour : error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test2;
        }
        // init
        init_id_to_ut(id_to_ut);
        // linkage creation
        link_to_tran(&tdum2, id_to_ut);
        ret = add_id_to_id_exist_mat(&matrix, tdum2.id, index_in_arr);
        if (ret != 0) {
            printf("got error code other than 0\n");
            printf("expected behaviour : error code to be 0\n");
            printf("returned error code : %d\n", ret);
            incre_error();
            goto end_test2;
        }
    }
    // lookup should succeed for both buffer and map version of lookup function
    // lookup using buffer version
    // as inspection of entire layer 1 array takes place, it will reveal that there
    // are two matching entries within
    ret = lookup_id_part(&l2_arr, &matrix, tdum.id, &map_buffer, &map_buffer2, tran_buffer, buffer_size, &num_used);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test2;
    }
    if (num_used != 2) {
        printf("number of slots in buffer filled was reported to be not two\n");
        printf("expected behaviour : report 2 slots filled\n");
        printf("reported value : %"PRIu64"\n", num_used);
        incre_error();
        goto end_test2;
    }
    // lookup using map version
    // should report that one layer 1 array still contains matching entries
    ret = lookup_id_part_map(&matrix, tdum.id, &map_buffer, &map_result);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test2;
    }
    if (map_result.number_of_ones != 1) {
        printf("number of layer 1 array containing the entry reported to be not one\n");
        printf("expected behaviour : report one layer 1 array matching\n");
        printf("reported number : %"PRIu64"\n", map_result.number_of_ones);
        incre_error();
        goto end_test2;
    }

end_test2:

    printf("test area 3 : cleanup\n");
    incre_check();
    // clear everything
    ret = del_l2_id_to_ut_arr(&l2_arr);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test3;
    }
    ret = del_id_exist_mat(&matrix);
    if (ret != 0) {
        printf("got error code other than 0\n");
        printf("expected behaviour : error code to be 0\n");
        printf("returned error code : %d\n", ret);
        incre_error();
        goto end_test3;
    }

end_test3:

    announce_test_end();
    report_stat();
    print_test_tag_for_report_collector(test_add_get_del_with_layer2_arr);

    return error_num;
}

int main (void) {
    int ret_test_l2_arr = 0;
    int ret_test_exist_mat = 0;

    int ret_total = 0;

    announce_test_set(test_ffprinter_template);
    print_set_tag_for_report_collector(test_ffprinter_template);

    ret_total += ret_test_l2_arr = test_add_get_del_with_layer2_arr();

    ret_total += ret_test_exist_mat = test_add_lookup_buffer_del_with_exist_mat(ret_test_l2_arr);

    ret_total += ret_test_exist_mat = test_add_lookup_map_del_with_exist_mat(ret_test_l2_arr, ret_test_exist_mat);

    ret_total += test_stress_add_del(ret_test_l2_arr, ret_test_exist_mat);

    ret_total += test_possibly_wonky_deletion(ret_test_l2_arr, ret_test_exist_mat);

    report_total(ret_total);

    return ret_total;
}
