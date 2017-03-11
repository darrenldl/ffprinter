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

#ifndef FFP_SCANMEM_H
#define FFP_SCANMEM_H

int mem_scan(const void* start, size_t haystack_size, unsigned char* pattern, size_t pattern_size, const unsigned char** ret_ptr_buf, uint32_t buf_size, uint32_t* num_used);

int stack_scan(uint32_t rem_depth, unsigned char* pattern, uint32_t pattern_size, const unsigned char** ptr_buf, uint32_t buf_size);

#endif
