/* simple bitmap library
 * Author : darrenldl <dldldev@yahoo.com>
 * 
 * Version : 0.11
 * 
 * Note:
 *    simple bitmap is NOT thread safe
 * 
 * License:
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 * 
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 * 
 * For more information, please refer to <http://unlicense.org/>
 */

#include "simple_bitmap.h"

#ifdef SIMPLE_BITMAP_SILENT
#define printf(...)
#endif

#define s_b_min(a, b) ((a) < (b) ? (a) : (b))
#define s_b_max(a, b) ((a) > (b) ? (a) : (b))

int bitmap_init (simple_bitmap* map, map_block* base, map_block* end, bit_index size_in_bits, map_block default_value) {
    map->base = base;

    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map == NULL) {
        printf("bitmap_init : map is NULL\n");
        return WRONG_INPUT;
    }
#endif
    if (end == NULL) {
#ifndef SIMPLE_BITMAP_SKIP_CHECK
        if (size_in_bits == 0) {
            printf("bitmap_init : end is NULL but size is 0 as well\n");
            return WRONG_INPUT;
        }
#endif
        map->end = base + get_bitmap_map_block_index(size_in_bits-1);
        map->length = size_in_bits;
    }
    else {
        map->end = end;
        map->length = (end - base + 1) * MAP_BLOCK_BIT;
    }

    if (default_value > 1) {
        ;     // do nothing
    }
    else if (default_value & (map_block) 0x1) {
        return bitmap_one(map);
    }
    else {
        return bitmap_zero(map);
    }

    return 0;
}

int bitmap_zero (simple_bitmap* map) {
    volatile map_block* cur;

    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map == NULL) {
        printf("bitmap_zero : map is NULL\n");
        return WRONG_INPUT;
    }
    if (map->base == NULL) {
        printf("bitmap_zero : base is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map->end == NULL) {
        printf("bitmap_zero : end is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map->length == 0) {
        printf("bitmap_zero : map has no length\n");
        return CORRUPTED_DATA;
    }
#endif

    // write 0s
    for (cur = map->base; cur <= map->end; cur++) {
        *cur = 0;
    }

    map->number_of_zeros = map->length;
    map->number_of_ones = 0;

    return 0;
}

int bitmap_one (simple_bitmap* map) {
    map_block mask;

    volatile map_block* cur;

    unsigned char count;

    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map == NULL) {
        printf("bitmap_one : map is NULL\n");
        return WRONG_INPUT;
    }
    if (map->base == NULL) {
        printf("bitmap_one : base is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map->end == NULL) {
        printf("bitmap_one : end is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map->length == 0) {
        printf("bitmap_one : map has no length\n");
        return CORRUPTED_DATA;
    }
#endif

    // write 1s
    for (cur = map->base; cur <= map->end; cur++) {
        *cur = (map_block) -1;
    }

    cur = map->end;

    // set the extra bits back to 0
    mask = 0;
    for (count = 0; count <= get_bitmap_map_block_bit_index(map->length-1); count++) {
        mask |= (map_block) 0x1 << (MAP_BLOCK_BIT - count - 1);
    }
    *cur &= mask;

    map->number_of_zeros = 0;
    map->number_of_ones = map->length;

    return 0;
}

static int map_blocks_ferris_wheel_flip (map_block* start, map_block* end, bit_index count) {
    map_block* start1 = start;
    map_block* start2 = end - count + 1;
    map_block temp;

    for (; start2 <= end; start1++, start2++) {
        temp = *start2;
        *start2 = *start1;
        *start1 = temp;
    }

    return 0;
}

int bitmap_shift (simple_bitmap* map, bit_index offset, char direction, map_block default_val, unsigned char wrap_around) {
    map_block* start;
    map_block* end;

    map_block temp;

    bit_index blocks_to_shift;

    unsigned char bits_to_shift;

    char shrink_direction;

    bit_index move_count;

    map_block* start1;

    map_block* cur;

    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map == NULL) {
        printf("bitmap_shift : map is NULL\n");
        return WRONG_INPUT;
    }
    if (map->base == NULL) {
        printf("bitmap_shift : base is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map->end == NULL) {
        printf("bitmap_shift : end is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map->length == 0) {
        printf("bitmap_shift : map has no length\n");
        return CORRUPTED_DATA;
    }
    if (map->base + get_bitmap_map_block_index(map->length-1) != map->end) {
        printf("bitmap_shift : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (map->number_of_zeros + map->number_of_ones != map->length) {
        printf("bitmap_shift : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
#endif

    blocks_to_shift = offset / MAP_BLOCK_BIT;

    bits_to_shift = offset % MAP_BLOCK_BIT;

    if (blocks_to_shift == 0) {
        goto bitshift;
    }

    if (!wrap_around) {  // no need to rotate
        if (blocks_to_shift >= get_bitmap_map_block_number(map->length)) {
            if (default_val > 1) {
                ; // do nothing
            }
            else if (default_val) {
                bitmap_one(map);
            }
            else {
                bitmap_zero(map);
            }
            return 0;
        }

        if (direction >= 0) {
            start = map->end - blocks_to_shift;
            start1 = map->end;

            for (; start1 >= map->base; start--, start1--) {
                *start1 = *start;
            }

            start = map->base + blocks_to_shift - 1;

            // overwrite remaining blocks with default value
            if (default_val > 1) {
                ; // do nothing
            }
            else {
                if (default_val == 1) {
                    temp = (map_block) -1;
                }
                else {
                    temp = 0;
                }
                for (; start >= map->base; start--) {
                    *start = temp;
                }
            }
        }
        else {
            start = map->base;
            start1 = map->base + blocks_to_shift;

            for (; start1 <= map->end; start++, start1++) {
                *start = *start1;
            }

            start = map->end - blocks_to_shift + 1;

            // overwrite remaining blocks with default value
            if (default_val > 1) {
                ; // do nothing
            }
            else {
                if (default_val == 1) {
                    temp = (map_block) -1;
                }
                else {
                    temp = 0;
                }
                for (; start <= map->end; start++) {
                    *start = temp;
                }
            }
        }
    }
    else {      // need to rotate
        if (blocks_to_shift >= map->length) {
            blocks_to_shift = blocks_to_shift % get_bitmap_map_block_number(map->length);
        }

        start = map->base;
        end = map->end;

        move_count = s_b_min(blocks_to_shift, get_bitmap_map_block_number(map->length) - blocks_to_shift);

        // main rotation algorithm begins
        if (direction >= 0) {
            if (move_count == blocks_to_shift) {
                shrink_direction = 1;
            }
            else {
                shrink_direction = -1;
            }
        }
        else {
            if (move_count == blocks_to_shift) {
                shrink_direction = -1;
            }
            else {
                shrink_direction = 1;
            }
        }

        while (start < end) {
            map_blocks_ferris_wheel_flip(start, end, move_count);

            if (shrink_direction == 1) {
                start += move_count;
            }
            else {
                end -= move_count;
            }

            if (start - end < 2 * move_count) { // need to flip direction and reduce unit
                shrink_direction *= -1;
                // move_count = min(move_count, start - end + 1 - move_count);
                move_count = start - end + 1 - move_count;
            }
        }
        // main rotation algorithm ends

        // handle the bitwise mismatching
        if (get_bitmap_excess_bits(map->length)) {
            if (direction >= 0) {   // shift to right
                // handle the remaining unshifted blocks
                for (   cur = map->base+blocks_to_shift-1;
                        cur > map->base;
                        cur--
                    ) 
                {
                    temp = *(cur-1) << get_bitmap_excess_bits(map->length);

                    *cur >>= MAP_BLOCK_BIT - get_bitmap_excess_bits(map->length);
                    *cur |= temp;
                }

                // handle the last block
                temp = (*(map->end) << get_bitmap_excess_bits(map->length));

                *(map->base) >>= MAP_BLOCK_BIT - get_bitmap_excess_bits(map->length);
                *(map->base) |= temp;
            }
            else {      // shift to left
                // handle the previously last block
                temp = *(map->end-blocks_to_shift+1) >> get_bitmap_excess_bits(map->length);

                *(map->end-blocks_to_shift) |= temp;

                // shift remaining blocks to left including the currently last block
                for (   cur = map->end-blocks_to_shift+1;
                        cur < map->end;
                        cur++
                    )
                {
                    temp = *(cur+1) >> get_bitmap_excess_bits(map->length);

                    *cur <<= MAP_BLOCK_BIT - get_bitmap_excess_bits(map->length);
                    *cur |= temp;
                }

                // handle the last block
                *(map->end) <<= MAP_BLOCK_BIT - get_bitmap_excess_bits(map->length);
            }
        }
    }

    bitmap_count_zeros_and_ones(map);

bitshift:
    if (bits_to_shift == 0) {
        return 0;
    }

    if (!wrap_around) {  // no need to recycle
        if (direction >= 0) {   // shift to right
            for (cur = map->end; cur > map->base; cur--) {
                *cur >>= bits_to_shift;

                *cur |= (*(cur-1) << (MAP_BLOCK_BIT - bits_to_shift));
            }

            *(map->base) >>= bits_to_shift;
        }
        else {      // shift to left
            for (cur = map->base; cur < map->end; cur++) {
                *cur <<= bits_to_shift;

                *cur |= (*(cur+1) >> (MAP_BLOCK_BIT - bits_to_shift));
            }

            *(map->end) <<= bits_to_shift;
        }
    }
    else {   // need to rotate
        if (direction >= 0) {   // shift to right
            temp = *(map->end);

            for (cur = map->end; cur > map->base; cur--) {
                *cur >>= bits_to_shift;

                *cur |= (*(cur-1) << (MAP_BLOCK_BIT - bits_to_shift));
            }

            *(map->base) >>= bits_to_shift;

            // piece the previous last block into the start first
            if (bits_to_shift < get_bitmap_map_block_bit_index(map->length-1)+1) {
                temp <<= get_bitmap_excess_bits(map->length) - bits_to_shift;
            }
            else {
                temp >>= bits_to_shift - get_bitmap_excess_bits(map->length);
                temp |= *(map->end) << get_bitmap_excess_bits(map->length);
            }

            *(map->base) |= temp;
        }
        else {      // shift to left
            temp = *(map->base);

            for (cur = map->base; cur < map->end; cur++) {
                *cur <<= bits_to_shift;

                *cur |= (*(cur+1) >> (MAP_BLOCK_BIT - bits_to_shift));
            }

            *(map->end) <<= bits_to_shift;

            // piece the previous first block into the end
            if (bits_to_shift < get_bitmap_excess_bits(map->length)) {
                temp >>= get_bitmap_excess_bits(map->length) - bits_to_shift;

                *(map->end) |= temp;
            }
            else {
                *(map->end-1) |= temp >> (MAP_BLOCK_BIT - (bits_to_shift - get_bitmap_excess_bits(map->length)));
                *(map->end) |= temp << (bits_to_shift - get_bitmap_excess_bits(map->length));
            }
        }
    }

    bitmap_count_zeros_and_ones(map);

    return 0;
}

int bitmap_not (simple_bitmap* map) {
    map_block* cur;

    unsigned char count;

    map_block mask;

    bit_index temp;

    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map == NULL) {
        printf("bitmap_not : map is NULL\n");
        return WRONG_INPUT;
    }
    if (map->base == NULL) {
        printf("bitmap_not : base is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map->end == NULL) {
        printf("bitmap_not : end is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map->length == 0) {
        printf("bitmap_not : map has no length\n");
        return CORRUPTED_DATA;
    }
    if (map->base + get_bitmap_map_block_index(map->length-1) != map->end) {
        printf("bitmap_not : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (map->number_of_zeros + map->number_of_ones != map->length) {
        printf("bitmap_not : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
#endif

    // flip bits
    for (cur = map->base; cur <= map->end; cur++) {
        *cur = ~(*cur);
    }

    // clean up the edge
    mask = 0;
    for (count = 0; count <= get_bitmap_map_block_bit_index(map->length-1); count++) {
        mask |= (map_block) 0x1 << (MAP_BLOCK_BIT - count - 1);
    }
    *(map->end) &= mask;

    // switch numbers
    temp = map->number_of_ones;
    map->number_of_ones = map->number_of_zeros;
    map->number_of_zeros = temp;

    return 0;
}

int bitmap_and (simple_bitmap* map1, simple_bitmap* map2, simple_bitmap* ret_map, unsigned char enforce_same_size) {
    map_block* cur1;
    map_block* cur2;
    map_block* cur_ret;

    bit_index i, min_len;

    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map1 == NULL) {
        printf("bitmap_and : map1 is NULL\n");
        return WRONG_INPUT;
    }
    if (map1->base == NULL) {
        printf("bitmap_and : map1->base is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map1->end == NULL) {
        printf("bitmap_and : map1->end is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map1->length == 0) {
        printf("bitmap_and : map1 has no length\n");
        return CORRUPTED_DATA;
    }
    if (map1->base + get_bitmap_map_block_index(map1->length-1) != map1->end) {
        printf("bitmap_and : map1 : is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (map1->number_of_zeros + map1->number_of_ones != map1->length) {
        printf("bitmap_and : map1 : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
    if (map2 == NULL) {
        printf("bitmap_and : map2 is NULL\n");
        return WRONG_INPUT;
    }
    if (map2->base == NULL) {
        printf("bitmap_and : map2->base is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map2->end == NULL) {
        printf("bitmap_and : map2->end is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map2->length == 0) {
        printf("bitmap_and : map2 has no length\n");
        return CORRUPTED_DATA;
    }
    if (map2->base + get_bitmap_map_block_index(map2->length-1) != map2->end) {
        printf("bitmap_and : map2 : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (map2->number_of_zeros + map2->number_of_ones != map2->length) {
        printf("bitmap_and : map2 : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
    if (ret_map == NULL) {
        printf("bitmap_and : ret_map is NULL\n");
        return WRONG_INPUT;
    }
    if (ret_map->base == NULL) {
        printf("bitmap_and : ret_map->base is NULL\n");
        return CORRUPTED_DATA;
    }
    if (ret_map->end == NULL) {
        printf("bitmap_and : ret_map->end is NULL\n");
        return CORRUPTED_DATA;
    }
    if (ret_map->length == 0) {
        printf("bitmap_and : ret_map has no length\n");
        return CORRUPTED_DATA;
    }
    if (ret_map->base + get_bitmap_map_block_index(ret_map->length-1) != ret_map->end) {
        printf("bitmap_and : ret_map : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (ret_map->number_of_zeros + ret_map->number_of_ones != ret_map->length) {
        printf("bitmap_and : ret_map : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }

    if (enforce_same_size) {
        if (!(map1->length == map2->length && map1->length == ret_map->length)) {
            printf("bitmap_and : map1, map2, ret_map have different sizes\n");
            return WRONG_INPUT;
        }
    }
#endif

    // get mininum of ends of all three maps
    min_len = s_b_min(map1->end - map1->base + 1, map2->end - map2->base + 1);
    min_len = s_b_min(min_len, ret_map->end - ret_map->base + 1);

    // do AND bitwise operation
    for (   i = 0, cur1 = map1->base, cur2 = map2->base, cur_ret = ret_map->base;
            i < min_len;
            i++, cur1++, cur2++, cur_ret++
        )
    {
        *cur_ret = *cur1 & *cur2;
    }

    bitmap_count_zeros_and_ones(ret_map);

    return 0;
}

int bitmap_or (simple_bitmap* map1, simple_bitmap* map2, simple_bitmap* ret_map, unsigned char enforce_same_size) {
    map_block* cur1;
    map_block* cur2;
    map_block* cur_ret;

    bit_index i, min_len;

    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map1 == NULL) {
        printf("bitmap_or : map1 is NULL\n");
        return WRONG_INPUT;
    }
    if (map1->base == NULL) {
        printf("bitmap_or : map1->base is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map1->end == NULL) {
        printf("bitmap_or : map1->end is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map1->length == 0) {
        printf("bitmap_or : map1 has no length\n");
        return CORRUPTED_DATA;
    }
    if (map1->base + get_bitmap_map_block_index(map1->length-1) != map1->end) {
        printf("bitmap_or : map1 : is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (map1->number_of_zeros + map1->number_of_ones != map1->length) {
        printf("bitmap_or : map1 : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
    if (map2 == NULL) {
        printf("bitmap_or : map2 is NULL\n");
        return WRONG_INPUT;
    }
    if (map2->base == NULL) {
        printf("bitmap_or : map2->base is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map2->end == NULL) {
        printf("bitmap_or : map2->end is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map2->length == 0) {
        printf("bitmap_or : map2 has no length\n");
        return CORRUPTED_DATA;
    }
    if (map2->base + get_bitmap_map_block_index(map2->length-1) != map2->end) {
        printf("bitmap_or : map2 : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (map2->number_of_zeros + map2->number_of_ones != map2->length) {
        printf("bitmap_or : map2 : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
    if (ret_map == NULL) {
        printf("bitmap_or : ret_map is NULL\n");
        return WRONG_INPUT;
    }
    if (ret_map->base == NULL) {
        printf("bitmap_or : ret_map->base is NULL\n");
        return CORRUPTED_DATA;
    }
    if (ret_map->end == NULL) {
        printf("bitmap_or : ret_map->end is NULL\n");
        return CORRUPTED_DATA;
    }
    if (ret_map->length == 0) {
        printf("bitmap_or : ret_map has no length\n");
        return CORRUPTED_DATA;
    }
    if (ret_map->base + get_bitmap_map_block_index(ret_map->length-1) != ret_map->end) {
        printf("bitmap_or : ret_map : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (ret_map->number_of_zeros + ret_map->number_of_ones != ret_map->length) {
        printf("bitmap_or : ret_map : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }

    if (enforce_same_size) {
        if (!(map1->length == map2->length && map1->length == ret_map->length)) {
            printf("bitmap_or : map1, map2, ret_map have different sizes\n");
            return WRONG_INPUT;
        }
    }
#endif

    // get mininum of ends of all three maps
    min_len = s_b_min(map1->end - map1->base + 1, map2->end - map2->base + 1);
    min_len = s_b_min(min_len, ret_map->end - ret_map->base + 1);

    // do OR bitwise operation
    for (   i = 0, cur1 = map1->base, cur2 = map2->base, cur_ret = ret_map->base;
            i < min_len;
            i++, cur1++, cur2++, cur_ret++
        )
    {
        *cur_ret = *cur1 | *cur2;
    }

    bitmap_count_zeros_and_ones(ret_map);

    return 0;
}

int bitmap_xor (simple_bitmap* map1, simple_bitmap* map2, simple_bitmap* ret_map, unsigned char enforce_same_size) {
    map_block* cur1;
    map_block* cur2;
    map_block* cur_ret;

    bit_index i, min_len;

    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map1 == NULL) {
        printf("bitmap_xor : map1 is NULL\n");
        return WRONG_INPUT;
    }
    if (map1->base == NULL) {
        printf("bitmap_xor : map1->base is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map1->end == NULL) {
        printf("bitmap_xor : map1->end is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map1->length == 0) {
        printf("bitmap_xor : map1 has no length\n");
        return CORRUPTED_DATA;
    }
    if (map1->base + get_bitmap_map_block_index(map1->length-1) != map1->end) {
        printf("bitmap_xor : map1 : is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (map1->number_of_zeros + map1->number_of_ones != map1->length) {
        printf("bitmap_xor : map1 : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
    if (map2 == NULL) {
        printf("bitmap_xor : map2 is NULL\n");
        return WRONG_INPUT;
    }
    if (map2->base == NULL) {
        printf("bitmap_xor : map2->base is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map2->end == NULL) {
        printf("bitmap_xor : map2->end is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map2->length == 0) {
        printf("bitmap_xor : map2 has no length\n");
        return CORRUPTED_DATA;
    }
    if (map2->base + get_bitmap_map_block_index(map2->length-1) != map2->end) {
        printf("bitmap_xor : map2 : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (map2->number_of_zeros + map2->number_of_ones != map2->length) {
        printf("bitmap_xor : map2 : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
    if (ret_map == NULL) {
        printf("bitmap_xor : ret_map is NULL\n");
        return WRONG_INPUT;
    }
    if (ret_map->base == NULL) {
        printf("bitmap_xor : ret_map->base is NULL\n");
        return CORRUPTED_DATA;
    }
    if (ret_map->end == NULL) {
        printf("bitmap_xor : ret_map->end is NULL\n");
        return CORRUPTED_DATA;
    }
    if (ret_map->length == 0) {
        printf("bitmap_xor : ret_map has no length\n");
        return CORRUPTED_DATA;
    }
    if (ret_map->base + get_bitmap_map_block_index(ret_map->length-1) != ret_map->end) {
        printf("bitmap_xor : ret_map : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (ret_map->number_of_zeros + ret_map->number_of_ones != ret_map->length) {
        printf("bitmap_xor : ret_map : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }

    if (enforce_same_size) {
        if (!(map1->length == map2->length && map1->length == ret_map->length)) {
            printf("bitmap_xor : map1, map2, ret_map have different sizes\n");
            return WRONG_INPUT;
        }
    }
#endif

    // get mininum of ends of all three maps
    min_len = s_b_min(map1->end - map1->base + 1, map2->end - map2->base + 1);
    min_len = s_b_min(min_len, ret_map->end - ret_map->base + 1);

    // do XOR bitwise operation
    for (   i = 0, cur1 = map1->base, cur2 = map2->base, cur_ret = ret_map->base;
            i < min_len;
            i++, cur1++, cur2++, cur_ret++
        )
    {
        *cur_ret = *cur1 ^ *cur2;
    }

    bitmap_count_zeros_and_ones(ret_map);

    return 0;
}

int bitmap_read (simple_bitmap* map, bit_index index, map_block* result) {
    uint_fast32_t block_index;
    unsigned char bit_indx;

    map_block mask;

    map_block buf;

    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map == NULL) {
        printf("bitmap_read : map is NULL\n");
        return WRONG_INPUT;
    }
    if (map->base == NULL) {
        printf("bitmap_read : base is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map->end == NULL) {
        printf("bitmap_read : end is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map->length == 0) {
        printf("bitmap_read : map has no length\n");
        return CORRUPTED_DATA;
    }
    if (map->base + get_bitmap_map_block_index(map->length-1) != map->end) {
        printf("bitmap_read : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (map->number_of_zeros + map->number_of_ones != map->length) {
        printf("bitmap_read : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
#endif
    block_index = get_bitmap_map_block_index(index);
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map->base + block_index > map->end || index >= map->length) {
        printf("bitmap_read : index exceeds range\n");
        return WRONG_INPUT;
    }
#endif
    bit_indx = get_bitmap_map_block_bit_index(index);

    mask = (map_block) 0x1 << ((MAP_BLOCK_BIT - 1) - bit_indx);

    buf = *(map->base + block_index) & mask;

    buf = buf >> ((MAP_BLOCK_BIT - 1) - bit_indx);

    *result = buf;

    return 0;
}

int bitmap_write (simple_bitmap* map, bit_index index, map_block input_value) {
    uint_fast32_t block_index;
    unsigned char bit_indx;

    map_block mask;

    map_block buf;

    map_block original;

    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map == NULL) {
        printf("bitmap_write : map is NULL\n");
        return WRONG_INPUT;
    }
    if (map->base == NULL) {
        printf("bitmap_write : base is NULL\n");
        return WRONG_INPUT;
    }
    if (map->end == NULL) {
        printf("bitmap_write : end is NULL\n");
        return WRONG_INPUT;
    }
    if (map->length == 0) {
        printf("bitmap_write : map has no length\n");
        return CORRUPTED_DATA;
    }
    if (map->base + get_bitmap_map_block_index(map->length-1) != map->end) {
        printf("bitmap_write : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (map->number_of_zeros + map->number_of_ones != map->length) {
        printf("bitmap_write : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
#endif
    block_index = get_bitmap_map_block_index(index);
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map->base + block_index > map->end || index >= map->length) {
        printf("bitmap_write : index exceeds range\n");
        return WRONG_INPUT;
    }
#endif
    bit_indx = get_bitmap_map_block_bit_index(index);

    mask = (map_block) 0x1 << ((MAP_BLOCK_BIT - 1) - bit_indx);

    buf = (input_value & (map_block) 0x1) << ((MAP_BLOCK_BIT - 1) - bit_indx);
    buf &= mask;

    original = *(map->base + block_index) & mask;

    *(map->base + block_index) &= ~mask;
    *(map->base + block_index) |= buf;

    if (buf == 0 && original != 0) {
        map->number_of_zeros    ++;
        map->number_of_ones     --;
    }
    else if (buf != 0 && original == 0) {
        map->number_of_zeros    --;
        map->number_of_ones     ++;
    }

    return 0;
}

int bitmap_count_zeros_and_ones (simple_bitmap* map) {
    map_block mask;

    map_block buf;

    map_block* cur;

    unsigned char count;

    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map == NULL) {
        printf("bitmap_count_zeros_and_ones : map is NULL\n");
        return WRONG_INPUT;
    }
    if (map->base == NULL) {
        printf("bitmap_count_zeros_and_ones : base is NULL\n");
        return WRONG_INPUT;
    }
    if (map->end == NULL) {
        printf("bitmap_count_zeros_and_ones : end is NULL\n");
        return WRONG_INPUT;
    }
    if (map->length == 0) {
        printf("bitmap_count_zeros_and_ones : map has no length\n");
        return CORRUPTED_DATA;
    }
    if (map->base + get_bitmap_map_block_index(map->length-1) != map->end) {
        printf("bitmap_count_zeros_and_ones : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
#endif

    // setup mask so left most bit is 1
    mask = (map_block) 0x1 << (MAP_BLOCK_BIT - 1);

    // reset
    map->number_of_zeros = 0;
    map->number_of_ones = 0;

    // count all bits before last map_block
    for (cur = map->base; cur < map->end; cur++) {
        buf = *cur;
        for (count = 0; count < MAP_BLOCK_BIT; count++) {
            if ((buf & mask) == 0) {
                map->number_of_zeros++;
            }
            else {
                map->number_of_ones++;
            }
            buf = buf << 1;
        }
    }

    // setting once more to be sure
    cur = map->end;
    buf = *cur;

    // setup mask so left most bit is 1
    mask = (map_block) 0x1 << (MAP_BLOCK_BIT - 1);

    // count bits in last map_block
    for (count = 0; count <= get_bitmap_map_block_bit_index(map->length-1); count++) {
        if ((buf & mask) == 0) {
            map->number_of_zeros++;
        }
        else {
            map->number_of_ones++;
        }
        buf = buf << 1;
    }

    // clean up the edge
    mask = 0;
    for (count = 0; count <= get_bitmap_map_block_bit_index(map->length-1); count++) {
        mask |= (map_block) 0x1 << (MAP_BLOCK_BIT - count - 1);
    }
    *cur &= mask;

    return 0;
}

int bitmap_first_one_bit_index (simple_bitmap* map, bit_index* result, bit_index skip_to_bit) {
    map_block buf;

    map_block mask;

    map_block* cur;

    uint_least16_t count = 0;

    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map == NULL) {
        printf("bitmap_first_one_bit_index : map is NULL\n");
        return WRONG_INPUT;
    }
    if (map->base == NULL) {
        printf("bitmap_first_one_bit_index : base is NULL\n");
        return WRONG_INPUT;
    }
    if (map->end == NULL) {
        printf("bitmap_first_one_bit_index : end is NULL\n");
        return WRONG_INPUT;
    }
    if (map->length == 0) {
        printf("bitmap_first_one_bit_index : map has no length\n");
        return CORRUPTED_DATA;
    }
    if (map->base + get_bitmap_map_block_index(map->length-1) != map->end) {
        printf("bitmap_first_one_bit_index : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (map->number_of_zeros + map->number_of_ones != map->length) {
        printf("bitmap_first_one_bit_index : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
    if (result == NULL) {
        printf("bitmap_first_one_bit_index : result is null\n");
        return WRONG_INPUT;
    }
    if (skip_to_bit >= map->length) {
        printf("bitmap_first_one_bit_index : skip_to_bit is out of range\n");
        return WRONG_INPUT;
    }
#endif

    // skip
    cur = map->base + get_bitmap_map_block_index(skip_to_bit);

    buf = *cur;

    // mask skipped bits
    mask = 0;
    for (count = 0; count < MAP_BLOCK_BIT - get_bitmap_map_block_bit_index(skip_to_bit); count++) {
        mask |= (map_block) 0x1 << count;
    }
    buf = buf & mask;

    // setup mask so left most bit is 1
    mask = (map_block) 0x1 << (MAP_BLOCK_BIT - 1);

    // find first non zero map_block
    if (buf == 0) {
        cur++;
        for (; cur <= map->end; cur++) {
            buf = *cur;
            if (buf != 0) {
                break;
            }
        }
    }

    // find first one bit
    for (count = 0; count < MAP_BLOCK_BIT; count++) {
        if (buf & mask) {
            break;
        }
        buf = buf << 1;
    }

    *result = (cur - map->base) * MAP_BLOCK_BIT + count;

    if (*result >= map->length) {
        return SEARCH_FAIL;
    }

    return 0;
}

int bitmap_first_zero_bit_index (simple_bitmap* map, bit_index* result, bit_index skip_to_bit) {
    map_block buf;

    map_block mask;

    map_block* cur;

    uint_least16_t count = 0;

    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map == NULL) {
        printf("bitmap_first_zero_bit_index : map is NULL\n");
        return WRONG_INPUT;
    }
    if (map->base == NULL) {
        printf("bitmap_first_zero_bit_index : base is NULL\n");
        return WRONG_INPUT;
    }
    if (map->end == NULL) {
        printf("bitmap_first_zero_bit_index : end is NULL\n");
        return WRONG_INPUT;
    }
    if (map->length == 0) {
        printf("bitmap_first_zero_bit_index : map has no length\n");
        return CORRUPTED_DATA;
    }
    if (map->base + get_bitmap_map_block_index(map->length-1) != map->end) {
        printf("bitmap_first_zero_bit_index : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (map->number_of_zeros + map->number_of_ones != map->length) {
        printf("bitmap_first_zero_bit_index : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
    if (skip_to_bit >= map->length) {
        printf("bitmap_first_zero_bit_index : skip_to_bit is out of range\n");
        return WRONG_INPUT;
    }
#endif

    // skip
    cur = map->base + get_bitmap_map_block_index(skip_to_bit);

    buf = *cur;

    // mask skipped bits
    mask = 0;
    for (count = 0; count < get_bitmap_map_block_bit_index(skip_to_bit); count++) {
        mask |= (map_block) 0x1 << (MAP_BLOCK_BIT - count - 1);
    }
    buf = buf | mask;

    // setup mask so left most bit is 1
    mask = (map_block) 0x1 << (MAP_BLOCK_BIT - 1);

    // find first non all one map_block
    if (buf == (map_block) -1) {
        cur++;
        for (; cur <= map->end; cur++) {
            buf = *cur;
            if (buf != (map_block) -1) {
                break;
            }
        }
    }

    // find first zero bit
    for (count = 0; count < MAP_BLOCK_BIT; count++) {
        if ((~buf) & mask) {
            break;
        }
        buf = buf << 1;
    }

    *result = (cur - map->base) * MAP_BLOCK_BIT + count;

    if (*result >= map->length) {
        return SEARCH_FAIL;
    }

    return 0;
}

int bitmap_first_one_cont_group (simple_bitmap* map, bitmap_cont_group* ret_grp, bit_index skip_to_bit) {
    map_block buf;

    map_block mask;

    map_block* cur;

    uint_least16_t count = 0;

    bit_index zero_count;
    bit_index one_count;

    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map == NULL) {
        printf("bitmap_first_one_cont_group : map is NULL\n");
        return WRONG_INPUT;
    }
    if (map->base == NULL) {
        printf("bitmap_first_one_cont_group : base is NULL\n");
        return WRONG_INPUT;
    }
    if (map->end == NULL) {
        printf("bitmap_first_one_cont_group : end is NULL\n");
        return WRONG_INPUT;
    }
    if (map->length == 0) {
        printf("bitmap_first_one_cont_group : map has no length\n");
        return CORRUPTED_DATA;
    }
    if (map->base + get_bitmap_map_block_index(map->length-1) != map->end) {
        printf("bitmap_first_one_cont_group : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (map->number_of_zeros + map->number_of_ones != map->length) {
        printf("bitmap_first_one_cont_group : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
    if (ret_grp == NULL) {
        printf("bitmap_first_one_cont_group : ret_grp is NULL\n");
        return WRONG_INPUT;
    }
    if (skip_to_bit >= map->length) {
        printf("bitmap_first_one_cont_group : skip_to_bit is out of range\n");
        return WRONG_INPUT;
    }
#endif

    // setup return group
    ret_grp->bit_type = 0x1;
    ret_grp->start = 0;
    ret_grp->length = 0;

    // skip
    cur = map->base + get_bitmap_map_block_index(skip_to_bit);

    buf = *cur;

    // mask skipped bits
    mask = 0;
    for (count = 0; count < MAP_BLOCK_BIT - get_bitmap_map_block_bit_index(skip_to_bit); count++) {
        mask |= (map_block) 0x1 << count;
    }
    buf = buf & mask;

    // setup mask so left most bit is 1
    mask = (map_block) 0x1 << (MAP_BLOCK_BIT - 1);

    // find first non zero map block
    if (buf == 0) {
        cur++;
        for (; cur <= map->end; cur++) {
            buf = *cur;
            if (buf != 0) {
                break;
            }
        }
    }

    // find first one bit
    for (zero_count = 0; zero_count < MAP_BLOCK_BIT; zero_count++) {
        if (buf & mask) {
            break;
        }
        buf = buf << 1;
    }

    // set start
    ret_grp->start = (cur - map->base) * MAP_BLOCK_BIT + zero_count;

    if (ret_grp->start >= map->length) {
        return SEARCH_FAIL;
    }

    // cur initialised already
    for (; cur <= map->end; cur++) {
        if (zero_count == 0) {
            buf = *cur;
        }

        // count ones
        for (one_count = 0; one_count < MAP_BLOCK_BIT - zero_count; one_count++) {
            if ((~buf) & mask) {
                break;
            }
            buf = buf << 1;
        }

        // update count
        ret_grp->length += one_count;

        if ((one_count + zero_count) != MAP_BLOCK_BIT) {
            // the one bits end in this map_block, no need to continue
            break;
        }

        zero_count = 0;
    }

    return 0;
}

int bitmap_first_zero_cont_group (simple_bitmap* map, bitmap_cont_group* ret_grp, bit_index skip_to_bit) {
    map_block buf;

    map_block mask;

    map_block* cur;

    uint_least16_t count = 0;

    bit_index zero_count;
    bit_index one_count;

    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map == NULL) {
        printf("bitmap_first_zero_cont_group : map is NULL\n");
        return WRONG_INPUT;
    }
    if (map->base == NULL) {
        printf("bitmap_first_zero_cont_group : base is NULL\n");
        return WRONG_INPUT;
    }
    if (map->end == NULL) {
        printf("bitmap_first_zero_cont_group : end is NULL\n");
        return WRONG_INPUT;
    }
    if (map->length == 0) {
        printf("bitmap_first_zero_cont_group : map has no length\n");
        return CORRUPTED_DATA;
    }
    if (map->base + get_bitmap_map_block_index(map->length-1) != map->end) {
        printf("bitmap_first_zero_cont_group : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (map->number_of_zeros + map->number_of_ones != map->length) {
        printf("bitmap_first_zero_cont_group : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
    if (ret_grp == NULL) {
        printf("bitmap_first_zero_cont_group : ret_grp is NULL\n");
        return WRONG_INPUT;
    }
    if (skip_to_bit >= map->length) {
        printf("bitmap_first_zero_cont_group : skip_to_bit is out of range\n");
        return WRONG_INPUT;
    }
#endif

    // setup return group
    ret_grp->bit_type = 0x0;
    ret_grp->start = 0;
    ret_grp->length = 0;

    // skip
    cur = map->base + get_bitmap_map_block_index(skip_to_bit);

    buf = *cur;

    // mask skipped bits
    mask = 0;
    for (count = 0; count < get_bitmap_map_block_bit_index(skip_to_bit); count++) {
        mask |= (map_block) 0x1 << (MAP_BLOCK_BIT - count - 1);
    }
    buf = buf | mask;

    // setup mask so left most bit is 1
    mask = (map_block) 0x1 << (MAP_BLOCK_BIT - 1);

    // find first non all one map block
    if (buf == (map_block) -1) {
        cur++;
        for (; cur <= map->end; cur++) {
            buf = *cur;
            if (buf != (map_block) -1) {
                break;
            }
        }
    }

    // find first zero bit
    for (one_count = 0; one_count < MAP_BLOCK_BIT; one_count++) {
        if ((~buf) & mask) {
            break;
        }
        buf = buf << 1;
    }

    // set start
    ret_grp->start = (cur - map->base) * MAP_BLOCK_BIT + one_count;

    if (ret_grp->start >= map->length) {
        return SEARCH_FAIL;
    }

    // cur initialised already
    for (; cur <= map->end; cur++) {
        if (one_count == 0) {
            buf = *cur;
        }

        // count zeros
        for (zero_count = 0; zero_count < MAP_BLOCK_BIT - one_count; zero_count++) {
            if (buf & mask) {
                break;
            }
            buf = buf << 1;
        }

        // update count
        ret_grp->length += zero_count;

        if ((zero_count + one_count) != MAP_BLOCK_BIT) {
            // the zero bits end in this map_block, no need to continue
            break;
        }

        one_count = 0;
    }

    return 0;
}

int bitmap_first_one_bit_index_back (simple_bitmap* map, bit_index* result, bit_index skip_to_bit) {
    map_block buf;

    map_block mask;

    map_block* cur;

    uint_least16_t count = 0;

    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map == NULL) {
        printf("bitmap_first_one_bit_index_back : map is NULL\n");
        return WRONG_INPUT;
    }
    if (map->base == NULL) {
        printf("bitmap_first_one_bit_index_back : base is NULL\n");
        return WRONG_INPUT;
    }
    if (map->end == NULL) {
        printf("bitmap_first_one_bit_index_back : end is NULL\n");
        return WRONG_INPUT;
    }
    if (map->length == 0) {
        printf("bitmap_first_one_bit_index_back : map has no length\n");
        return CORRUPTED_DATA;
    }
    if (map->base + get_bitmap_map_block_index(map->length-1) != map->end) {
        printf("bitmap_first_one_bit_index_back : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (map->number_of_zeros + map->number_of_ones != map->length) {
        printf("bitmap_first_one_bit_index_back : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
    if (result == NULL) {
        printf("bitmap_first_one_bit_index_back : result is null\n");
        return WRONG_INPUT;
    }
    if (skip_to_bit >= map->length) {
        printf("bitmap_first_one_bit_index_back : skip_to_bit is out of range\n");
        return WRONG_INPUT;
    }
#endif

    // skip
    cur = map->base + get_bitmap_map_block_index(skip_to_bit);

    buf = *cur;

    // mask skipped bits
    mask = 0;
    for (count = 0; count <= get_bitmap_map_block_bit_index(skip_to_bit); count++) {
        mask |= ((map_block) 0x1 << (MAP_BLOCK_BIT - 1)) >> count;
    }
    buf = buf & mask;

    // setup mask so right most bit is 1
    mask = (map_block) 0x1;

    // find first non zero map_block
    if (buf == 0) {
        cur--;
        for (; cur >= map->base; cur--) {
            buf = *cur;
            if (buf != 0) {
                break;
            }
        }
    }

    // find first one bit
    for (count = 0; count < MAP_BLOCK_BIT; count++) {
        if (buf & mask) {
            break;
        }
        buf = buf >> 1;
    }

    if (count == MAP_BLOCK_BIT) { // failed to find the one bit
        *result = map->length;
        return SEARCH_FAIL;
    }
    else {
        *result = (cur - map->base) * MAP_BLOCK_BIT + (MAP_BLOCK_BIT-1 - count);
        return 0;
    }

    return 0;
}

int bitmap_first_zero_bit_index_back (simple_bitmap* map, bit_index* result, bit_index skip_to_bit) {
    map_block buf;

    map_block mask;

    map_block* cur;

    uint_least16_t count = 0;

    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map == NULL) {
        printf("bitmap_first_zero_bit_index_back : map is NULL\n");
        return WRONG_INPUT;
    }
    if (map->base == NULL) {
        printf("bitmap_first_zero_bit_index_back : base is NULL\n");
        return WRONG_INPUT;
    }
    if (map->end == NULL) {
        printf("bitmap_first_zero_bit_index_back : end is NULL\n");
        return WRONG_INPUT;
    }
    if (map->length == 0) {
        printf("bitmap_first_zero_bit_index_back : map has no length\n");
        return CORRUPTED_DATA;
    }
    if (map->base + get_bitmap_map_block_index(map->length-1) != map->end) {
        printf("bitmap_first_zero_bit_index_back : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (map->number_of_zeros + map->number_of_ones != map->length) {
        printf("bitmap_first_zero_bit_index_back : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
    if (skip_to_bit >= map->length) {
        printf("bitmap_first_zero_bit_index_back : skip_to_bit is out of range\n");
        return WRONG_INPUT;
    }
#endif

    // skip
    cur = map->base + get_bitmap_map_block_index(skip_to_bit);

    buf = *cur;

    // mask skipped bits
    mask = 0;
    for (count = 0; count < MAP_BLOCK_BIT-1 - get_bitmap_map_block_bit_index(skip_to_bit); count++) {
        mask |= (map_block) 0x1 << count;
    }
    buf = buf | mask;

    // setup mask so right most bit is 1
    mask = (map_block) 0x1;

    // find first non all one map_block
    if (buf == (map_block) -1) {
        cur--;
        for (; cur >= map->base; cur--) {
            buf = *cur;
            if (buf != (map_block) -1) {
                break;
            }
        }
    }

    // find first zero bit
    for (count = 0; count < MAP_BLOCK_BIT; count++) {
        if ((~buf) & mask) {
            break;
        }
        buf = buf >> 1;
    }

    if (count == MAP_BLOCK_BIT) {
        *result = map->length;
        return SEARCH_FAIL;
    }
    else {
        *result = (cur - map->base) * MAP_BLOCK_BIT + (MAP_BLOCK_BIT-1 - count);
        return 0;
    }

    return 0;
}

int bitmap_first_one_cont_group_back (simple_bitmap* map, bitmap_cont_group* ret_grp, bit_index skip_to_bit) {
    map_block buf;

    map_block mask;

    map_block* cur;

    uint_least16_t count = 0;

    bit_index zero_count;
    bit_index one_count;

    bit_index end;

    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map == NULL) {
        printf("bitmap_first_one_cont_group_back : map is NULL\n");
        return WRONG_INPUT;
    }
    if (map->base == NULL) {
        printf("bitmap_first_one_cont_group_back : base is NULL\n");
        return WRONG_INPUT;
    }
    if (map->end == NULL) {
        printf("bitmap_first_one_cont_group_back : end is NULL\n");
        return WRONG_INPUT;
    }
    if (map->length == 0) {
        printf("bitmap_first_one_cont_group_back : map has no length\n");
        return CORRUPTED_DATA;
    }
    if (map->base + get_bitmap_map_block_index(map->length-1) != map->end) {
        printf("bitmap_first_one_cont_group_back : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (map->number_of_zeros + map->number_of_ones != map->length) {
        printf("bitmap_first_one_cont_group_back : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
    if (ret_grp == NULL) {
        printf("bitmap_first_one_cont_group_back : ret_grp is NULL\n");
        return WRONG_INPUT;
    }
    if (skip_to_bit >= map->length) {
        printf("bitmap_first_one_cont_group_back : skip_to_bit is out of range\n");
        return WRONG_INPUT;
    }
#endif

    // setup return group
    ret_grp->bit_type = 0x1;
    ret_grp->start = 0;
    ret_grp->length = 0;

    // skip
    cur = map->base + get_bitmap_map_block_index(skip_to_bit);

    buf = *cur;

    // mask skipped bits
    mask = 0;
    for (count = 0; count <= get_bitmap_map_block_bit_index(skip_to_bit); count++) {
        mask |= ((map_block) 0x1 << (MAP_BLOCK_BIT - 1)) >> count;
    }
    buf = buf & mask;

    // setup mask so right most bit is 1
    mask = (map_block) 0x1;

    // find first non zero map block
    if (buf == 0) {
        cur--;
        for (; cur >= map->base; cur--) {
            buf = *cur;
            if (buf != 0) {
                break;
            }
        }
    }

    // find first one bit
    for (zero_count = 0; zero_count < MAP_BLOCK_BIT; zero_count++) {
        if (buf & mask) {
            break;
        }
        buf = buf >> 1;
    }

    if (zero_count == MAP_BLOCK_BIT) {
        return SEARCH_FAIL;
    }
    else {
        end = (cur - map->base) * MAP_BLOCK_BIT + (MAP_BLOCK_BIT-1 - zero_count);
    }

    // cur initialised already
    for (; cur >= map->base; cur--) {
        if (zero_count == 0) {
            buf = *cur;
        }

        // count ones
        for (one_count = 0; one_count < MAP_BLOCK_BIT - zero_count; one_count++) {
            if ((~buf) & mask) {
                break;
            }
            buf = buf >> 1;
        }

        // update count
        ret_grp->length += one_count;

        if ((one_count + zero_count) != MAP_BLOCK_BIT) {
            // the one bits end in this map_block, no need to continue
            break;
        }

        zero_count = 0;
    }

    // set start
    ret_grp->start = end - ret_grp->length + 1;

    return 0;
}

int bitmap_first_zero_cont_group_back (simple_bitmap* map, bitmap_cont_group* ret_grp, bit_index skip_to_bit) {
    map_block buf;

    map_block mask;

    map_block* cur;

    uint_least16_t count = 0;

    bit_index zero_count;
    bit_index one_count;

    bit_index end;

    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map == NULL) {
        printf("bitmap_first_zero_cont_group_back : map is NULL\n");
        return WRONG_INPUT;
    }
    if (map->base == NULL) {
        printf("bitmap_first_zero_cont_group_back : base is NULL\n");
        return WRONG_INPUT;
    }
    if (map->end == NULL) {
        printf("bitmap_first_zero_cont_group_back : end is NULL\n");
        return WRONG_INPUT;
    }
    if (map->length == 0) {
        printf("bitmap_first_zero_cont_group_back : map has no length\n");
        return CORRUPTED_DATA;
    }
    if (map->base + get_bitmap_map_block_index(map->length-1) != map->end) {
        printf("bitmap_first_zero_cont_group_back : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (map->number_of_zeros + map->number_of_ones != map->length) {
        printf("bitmap_first_zero_cont_group_back : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
    if (ret_grp == NULL) {
        printf("bitmap_first_zero_cont_group_back : ret_grp is NULL\n");
        return WRONG_INPUT;
    }
    if (skip_to_bit >= map->length) {
        printf("bitmap_first_zero_cont_group_back : skip_to_bit is out of range\n");
        return WRONG_INPUT;
    }
#endif

    // setup return group
    ret_grp->bit_type = 0x0;
    ret_grp->start = 0;
    ret_grp->length = 0;

    // skip
    cur = map->base + get_bitmap_map_block_index(skip_to_bit);

    buf = *cur;

    // mask skipped bits
    mask = 0;
    for (count = 0; count < MAP_BLOCK_BIT-1 - get_bitmap_map_block_bit_index(skip_to_bit); count++) {
        mask |= (map_block) 0x1 << count;
    }
    buf = buf | mask;

    // setup mask so right most bit is 1
    mask = (map_block) 0x1;

    // find first non all one map block
    if (buf == (map_block) -1) {
        cur--;
        for (; cur >= map->base; cur--) {
            buf = *cur;
            if (buf != (map_block) -1) {
                break;
            }
        }
    }

    // find first zero bit
    for (one_count = 0; one_count < MAP_BLOCK_BIT; one_count++) {
        if ((~buf) & mask) {
            break;
        }
        buf = buf >> 1;
    }

    if (one_count == MAP_BLOCK_BIT) {
        return SEARCH_FAIL;
    }
    else {
        end = (cur - map->base) * MAP_BLOCK_BIT + (MAP_BLOCK_BIT-1 - one_count);
    }

    // cur initialised already
    for (; cur >= map->base; cur--) {
        if (one_count == 0) {
            buf = *cur;
        }

        // count zeros
        for (zero_count = 0; zero_count < MAP_BLOCK_BIT - one_count; zero_count++) {
            if (buf & mask) {
                break;
            }
            buf = buf >> 1;
        }

        // update count
        ret_grp->length += zero_count;

        if ((zero_count + one_count) != MAP_BLOCK_BIT) {
            // the zero bits end in this map_block, no need to continue
            break;
        }

        one_count = 0;
    }

    // set start
    ret_grp->start = end - ret_grp->length + 1;

    return 0;
}

// both maps must be initialised
int bitmap_copy (simple_bitmap* src_map, simple_bitmap* dst_map, unsigned char allow_truncate, map_block default_value) {
    map_block* src_cur;
    map_block* dst_cur;

    map_block mask;

    unsigned char count;

    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (src_map == NULL) {
        printf("bitmap_copy : src_map is NULL\n");
        return WRONG_INPUT;
    }
    if (src_map->base == NULL) {
        printf("bitmap_copy : src_map->base is NULL\n");
        return CORRUPTED_DATA;
    }
    if (src_map->end == NULL) {
        printf("bitmap_copy : src_map->end is NULL\n");
        return CORRUPTED_DATA;
    }
    if (src_map->length == 0) {
        printf("bitmap_copy : src_map has no length\n");
        return CORRUPTED_DATA;
    }
    if (src_map->base + get_bitmap_map_block_index(src_map->length-1) != src_map->end) {
        printf("bitmap_copy : src_map : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (src_map->number_of_zeros + src_map->number_of_ones != src_map->length) {
        printf("bitmap_copy : src_map : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
    if (dst_map == NULL) {
        printf("bitmap_copy : dst_map is NULL\n");
        return WRONG_INPUT;
    }
    if (dst_map->base == NULL) {
        printf("bitmap_copy : dst_map->base is NULL\n");
        return CORRUPTED_DATA;
    }
    if (dst_map->end == NULL) {
        printf("bitmap_copy : dst_map->end is NULL\n");
        return CORRUPTED_DATA;
    }
    if (dst_map->length == 0) {
        printf("bitmap_copy : dst_map has no length\n");
        return CORRUPTED_DATA;
    }
    if (dst_map->base + get_bitmap_map_block_index(dst_map->length-1) != dst_map->end) {
        printf("bitmap_copy : dst_map : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (dst_map->number_of_zeros + dst_map->number_of_ones != dst_map->length) {
        printf("bitmap_copy : dst_map : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
#endif

    if (src_map->length <= dst_map->length) { // no need for truncation at all
        // clean up a bit
        if (default_value & (map_block) 0x1) {
            bitmap_one(dst_map);
        }
        else {
            bitmap_zero(dst_map);
        }

        for (   src_cur = src_map->base, dst_cur = dst_map->base;
                src_cur <= src_map->end;
                src_cur ++, dst_cur++
            )
        {
            *dst_cur = *src_cur;
        }

        // clean off the edge
        dst_cur = dst_map->base + (src_map->end - src_map->base);
        mask = 0;
        for (count = 0; count <= get_bitmap_map_block_bit_index(src_map->length-1); count++) {
            mask |= (map_block) 0x1 << (MAP_BLOCK_BIT - count - 1);
        }
        if (default_value & (map_block) 0x1) {
            *dst_cur |= ~mask;
        }
        else {
            *dst_cur &= mask;
        }
    }
    else {   // need to truncate
        if (allow_truncate == 0) {
            printf("bitmap_copy : truncation needed but not allowed, both maps are untouched\n");
            return GENERAL_FAIL;
        }

        // clean up a bit
        if (default_value & (map_block) 0x1) {
            bitmap_one(dst_map);
        }
        else {
            bitmap_zero(dst_map);
        }

        for (   src_cur = src_map->base, dst_cur = dst_map->base;
                dst_cur <= dst_map->end;
                src_cur ++, dst_cur++
            )
        {
            *dst_cur = *src_cur;
        }

        // clean off the edge
        dst_cur = dst_map->end;
        mask = 0;
        for (count = 0; count <= get_bitmap_map_block_bit_index(dst_map->length-1); count++) {
            mask |= (map_block) 0x1 << (MAP_BLOCK_BIT - count - 1);
        }
        if (default_value & (map_block) 0x1) {
            *dst_cur |= ~mask;
        }
        else {
            *dst_cur &= mask;
        }
    }

    bitmap_count_zeros_and_ones(dst_map);

    return 0;
}

int bitmap_meta_copy (simple_bitmap* src_map, simple_bitmap* dst_map) {
    dst_map->base     =  src_map->base;
    dst_map->end      =  src_map->end;
    dst_map->length   =  src_map->length;

    dst_map->number_of_zeros   =  src_map->number_of_zeros;
    dst_map->number_of_ones    =  src_map->number_of_ones;
    return 0;
}

// memory management is not handled
// this function only handles meta data and initialise uninitialised map blocks
int bitmap_grow (simple_bitmap* map, map_block* base, map_block* end, bit_index size_in_bits, map_block default_value) {
    volatile map_block* cur;

    map_block mask;

    map_block* old_end;

    bit_index old_length;

    unsigned char count;

    //input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map == NULL) {
        printf("bitmap_grow : map is NULL\n");
        return WRONG_INPUT;
    }
    if (map->base == NULL) {
        printf("bitmap_grow : base is NULL\n");
        return WRONG_INPUT;
    }
    if (map->end == NULL) {
        printf("bitmap_grow : end is NULL\n");
        return WRONG_INPUT;
    }
    if (map->length == 0) {
        printf("bitmap_grow : map has no length\n");
        return CORRUPTED_DATA;
    }
    if (map->base + get_bitmap_map_block_index(map->length-1) != map->end) {
        printf("bitmap_grow : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (map->number_of_zeros + map->number_of_ones != map->length) {
        printf("bitmap_grow : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
#endif

    old_length = map->length;

    if (base) {     // changed memory location
        map->base = base;
        // the position of old end in memory may be moved by memory operations
        // but position wise in bitmap, it is still the "old" end
        old_end = base + get_bitmap_map_block_index(old_length-1);
    }
    else {          // remain in same spot
        old_end = map->end;
    }

    if (end == NULL) {
        if (size_in_bits == 0) {
            printf("bitmap_grow : end is NULL but size is 0 as well\n");
            return WRONG_INPUT;
        }
        if (size_in_bits < old_length) {
            printf("bitmap_grow : request length is smaller than old length\n");
            return WRONG_INPUT;
        }
        if (size_in_bits == old_length) {
            printf("bitmap_grow : request length is same as old length\n");
            return WRONG_INPUT;
        }
        map->end = map->base + get_bitmap_map_block_index(size_in_bits-1);
        map->length = size_in_bits;
    }
    else {
        if (end < old_end) {
            printf("bitmap_grow : request end is lower than old end\n");
            return WRONG_INPUT;
        }
        if (end == old_end) {
            printf("bitmap_grow : request end is same as old end\n");
            return WRONG_INPUT;
        }
        map->end = end;
        map->length = (map->end - map->base + 1) * MAP_BLOCK_BIT;
    }

    // clean off the edge and remaining map blocks
    mask = 0;
    for (count = 0; count <= get_bitmap_map_block_bit_index(old_length-1); count++) {
        mask |= (map_block) 0x1 << (MAP_BLOCK_BIT - count - 1);
    }
    if (default_value > 1) {
        ;     // do nothing
    }
    else if (default_value & (map_block) 0x1) {
        *old_end |= ~mask;
        for (cur = old_end + 1; cur <= map->end; cur++) {
            *cur = (map_block) -1;
        }
    }
    else {
        *old_end &= mask;
        for (cur = old_end + 1; cur <= map->end; cur++) {
            *cur = 0;
        }
    }

    bitmap_count_zeros_and_ones(map);

    return 0;
}

// cuts the bitmap and modifies the meta data
int bitmap_shrink (simple_bitmap* map, map_block* end, bit_index size_in_bits) {
    volatile map_block* cur;

    map_block mask;

    map_block* old_end;

    unsigned char count;

    //input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map == NULL) {
        printf("bitmap_shrink : map is NULL\n");
        return WRONG_INPUT;
    }
    if (map->base == NULL) {
        printf("bitmap_shrink : base is NULL\n");
        return WRONG_INPUT;
    }
    if (map->end == NULL) {
        printf("bitmap_shrink : end is NULL\n");
        return WRONG_INPUT;
    }
    if (map->length == 0) {
        printf("bitmap_shrink : map has no length\n");
        return CORRUPTED_DATA;
    }
    if (map->base + get_bitmap_map_block_index(map->length-1) != map->end) {
        printf("bitmap_shrink : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (map->number_of_zeros + map->number_of_ones != map->length) {
        printf("bitmap_shrink : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
#endif

    if (end == NULL) {
#ifndef SIMPLE_BITMAP_SKIP_CHECK
        if (size_in_bits == 0) {
            printf("bitmap_shrink : end is NULL but size is 0 as well\n");
            return WRONG_INPUT;
        }
        if (size_in_bits > map->length) {
            printf("bitmap_shrink : request length is larger than old length\n");
            return WRONG_INPUT;
        }
        if (size_in_bits == map->length) {
            printf("bitmap_shrink : request length is same as old length\n");
            return WRONG_INPUT;
        }
#endif
        old_end = map->end;
        map->end = map->base + get_bitmap_map_block_index(size_in_bits-1);
        map->length = size_in_bits;
    }
    else {
#ifndef SIMPLE_BITMAP_SKIP_CHECK
        if (end > map->end) {
            printf("bitmap_shrink : request end is higher than old end\n");
            return WRONG_INPUT;
        }
        if (end == map->end) {
            printf("bitmap_shrink : request end is same as old end\n");
            return WRONG_INPUT;
        }
#endif
        old_end = map->end;
        map->end = end;
        map->length = (end - map->base + 1) * MAP_BLOCK_BIT;
    }

    // wipe off the edge and old map blocks
    mask = 0;
    for (count = 0; count <= get_bitmap_map_block_bit_index(map->length-1); count++) {
        mask |= (map_block) 0x1 << (MAP_BLOCK_BIT - count - 1);
    }
    *(map->end) &= mask;
    for (cur = map->end + 1; cur <= old_end; cur++) {
        *cur = 0;
    }

    bitmap_count_zeros_and_ones(map);

    return 0;
}

int bitmap_show (simple_bitmap* map) {
    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map == NULL) {
        printf("bitmap_show : map is NULL\n");
        return WRONG_INPUT;
    }
#endif
    printf("####################\n");

    printf("map->base : %p\n", map->base);
    printf("map->end  : %p\n", map->end);
    printf("map->length : %"PRIu64"\n", map->length);
    printf("map->number_of_zeros : %"PRIu64"\n", map->number_of_zeros);
    printf("map->number_of_ones  : %"PRIu64"\n", map->number_of_ones);

    printf("####################\n");

    return 0;
}

int bitmap_cont_group_show (bitmap_cont_group* grp) {
    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (grp == NULL) {
        printf("bitmap_cont_group_show : grp is NULL\n");
        return WRONG_INPUT;
    }
#endif
    printf("####################\n");

    printf("grp->bit_type : %x\n", (grp->bit_type ? 0x1 : 0x0));
    printf("grp->start  : %"PRIu64"\n", grp->start);
    printf("grp->length : %"PRIu64"\n", grp->length);

    printf("####################\n");

    return 0;
}

int bitmap_raw_show (simple_bitmap* map) {
    uint_fast16_t count;

    map_block* cur;

    // input check
#ifndef SIMPLE_BITMAP_SKIP_CHECK
    if (map == NULL) {
        printf("bitmap_raw_show : map is NULL\n");
        return WRONG_INPUT;
    }
    if (map->base == NULL) {
        printf("bitmap_raw_show : base is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map->end == NULL) {
        printf("bitmap_raw_show : end is NULL\n");
        return CORRUPTED_DATA;
    }
    if (map->length == 0) {
        printf("bitmap_raw_show : map has no length\n");
        return CORRUPTED_DATA;
    }
    if (map->base + get_bitmap_map_block_index(map->length-1) != map->end) {
        printf("bitmap_raw_show : length is inconsistent with base and end\n");
        return CORRUPTED_DATA;
    }
    if (map->number_of_zeros + map->number_of_ones != map->length) {
        printf("bitmap_raw_show : inconsistent statistics of number of ones and zeros\n");
        return CORRUPTED_DATA;
    }
#endif
    printf("####################\n");

    printf("bitmap_raw :\n");

    count = 0;
    for (cur = map->base; cur <= map->end; cur++) {
        printf(MAP_BLOCK_FORMAT_STR" ", *cur);
        if (count == 15) {
            count = 0;
            printf("\n");
        }
        else {
            count++;
        }
    }
    printf("\n");

    printf("####################\n");

    return 0;
}
