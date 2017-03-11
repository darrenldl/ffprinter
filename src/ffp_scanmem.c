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

#include "ffp_scanmem.h"

int mem_scan(const void* start, size_t haystack_size, unsigned char* pattern, size_t pattern_size, const unsigned char** ret_ptr_buf, uint32_t buf_size, uint32_t* num_used) {
    size_t i;

    const unsigned char* ptr;

    uint32_t index;

    if (pattern_size > haystack_size) {
        return 0;
    }

    if (buf_size == 0) {
        return WRONG_ARGS;
    }

    ptr = start;
    index = 0;
    for (i = 0; i <= haystack_size - pattern_size; i++) {
        if (memcmp(ptr, pattern, pattern_size) == 0) {
            ret_ptr_buf[index] = ptr;
            index++;

            if (index == buf_size) {
                if (num_used) {
                    *num_used = index;
                }
                return BUFFER_FULL;
            }
        }
    }

    if (num_used) {
        *num_used = index;
    }

    return 0;
}

int stack_scan(uint32_t rem_depth, unsigned char* pattern, uint32_t pattern_size, const unsigned char** ptr_buf, uint32_t buf_size) {
    unsigned char stack_space[1024];    // don't initialise
    uint32_t num_used;
    int i;

    if (rem_depth == 0) {   // not reachable if called by itself recursively
        return 0;
    }

    if (rem_depth > 1) {
        stack_scan(rem_depth - 1, pattern, pattern_size, ptr_buf, buf_size);
    }

    i = mem_scan(stack_space, sizeof(stack_space), pattern, pattern_size, ptr_buf, buf_size, &num_used);
    if (i == BUFFER_FULL) {
        printf("stack_scan : pointer buffer full\n");   // right this overwrites stack...hm...
    }
    for (i = 0; i < num_used; i++) {
        printf("#%d    address : %p    offset from scan starting point : %ld\n", i, ptr_buf[i], ptr_buf[i] - stack_space);
    }

    return 0;
}
