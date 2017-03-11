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
#include "ffp_database.h"
#include <sys/types.h>
#include <sys/stat.h>

#include "../lib/order32.h"

#ifndef FFP_FILE_H
#define FFP_FILE_H

// field presence masks
#define HAS_TAG_STR     0x0000000000000001LL
#define HAS_USR_MSG     0x0000000000000002LL
#define HAS_ADD_TIME    0x0000000000000004LL
#define HAS_MOD_TIME    0x0000000000000008LL
#define HAS_USR_TIME    0x0000000000000010LL
#define HAS_FILE_DATA   0x0000000000000020LL

#define DFILE_BUFFER_SIZE   1024

#define WARN_VERIFY_TIMES   1

#define IS_STR      1
#define NOT_STR     0

int load_file (database_handle* dh, char* file_name);

int save_file (database_handle* dh, char* file_name);

#endif
