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
#include "ffp_error.h"

#ifndef FFP_DIRECTORY_H
#define FFP_DIRECTORY_H

#define is_pointing_to_root(dir)        ((dir)->db_name[0] == 0)

#define is_end_of_path(rem_path_len)    ((rem_path_len) == 0 || (rem_path_len) == 1)

#define mark_dir_pointers_usable(dir)       ((dir)->pointers_usable = 1)
#define mark_dir_pointers_not_usable(dir)   ((dir)->pointers_usable = 0)

//#define is_pointing_to_db(dir)          ((dir)->db_name[0] != 0 && (dir)->entry_id[0] == 0)

#define NO_SUCH_LOGIC_DIR   120
#define PATH_TOO_LONG       121
#define EMPTY_PATH          122
#define BROKEN_PATH         123
#define FOUND_DUPLICATE     124
#define CUR_DIR_INVALID     125

#define PATH_LEN_MAX        1024

typedef struct dir_info dir_info;
typedef struct root_dir root_dir;

struct dir_info {
    root_dir* root;

    // IDs are used instead of pointers
    // to avoid dereferencing problem when entry is removed

    char db_name[FILE_NAME_MAX+1];     // root database name, starts with null character when not in any database
    char entry_id[EID_STR_MAX+1];   // the group type entry dir_info is referring to, starts with 0 if pointing to root of data base

    unsigned char pointers_usable;  // following pointers are properly assigned only if pointers_usable is non-zero
    // pointer values :
    //      both are NULL if dir is pointing to root
    database_handle* dh;    // optional, used only for returning
    linked_entry* entry;    // optional, used only for returning
};

struct root_dir {
    database_handle* db;    // hashtable for databases
    uint32_t db_num;    // number of databases opened
};

int is_pointing_to_db (const dir_info* tar);

int init_dir_info (dir_info* info, root_dir* root, char* db_name);

int init_root_dir (root_dir* root);

int update_dir_pointers(dir_info* info, error_handle* er_h);

int add_dh_to_root_dir (root_dir* root, char* name, database_handle** ret_dh, error_handle* er_h);

int del_dh_from_root_dir (root_dir* root, char* name, error_handle* er_h);

int path_to_dir(const dir_info* cur_dir, const char* path_buf, str_len_int* rem_len, simple_bitmap* map_buf, simple_bitmap* map_buf2, dir_info* dir, int mode, error_handle* er_h);

int dir_to_path(const dir_info* dir, char* path_buf, int mode, error_handle* er_h);   // path buf should have length PATH_LEN_MAX

int copy_dir (dir_info* dst, const dir_info* src);

/* Debug functions */
int print_root_dir (root_dir* root);

int print_dir (const dir_info* dir);

#endif
