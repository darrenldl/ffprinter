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

#include "ffp_directory.h"

/* internal macros */
#define ret_if_add_len_too_long(path_pos, add_len, er_h) \
    if ((path_pos) + (add_len) > PATH_LEN_MAX) {    \
        error_write(er_h, "path too long");         \
        return PATH_TOO_LONG;                       \
    }

#define notify_ambig_resolve(fuzzy_input, result) \
    printf("path_to_dir : %s resolved to %s\n", fuzzy_input, result)

static int point_to_root (dir_info* tar) {
    tar->db_name[0] = 0;
    mem_wipe_sec(tar->entry_id, EID_STR_MAX+1);

    tar->pointers_usable = 1;
    tar->dh = NULL;
    tar->entry = NULL;

    return 0;
}

static int point_to_dh (dir_info* tar, database_handle* dh) {
    int i;

    strcpy(tar->db_name, dh->name);

    for (i = 0; i < EID_STR_MAX; i++) {
        tar->entry_id[i] = '0';
    }
    tar->entry_id[i] = 0;

    tar->dh = dh;
    
    tar->entry = &dh->tree;

    tar->pointers_usable = 1;

    return 0;
}

static int point_to_entry (dir_info* tar, linked_entry* entry) {
    strcpy(tar->entry_id, entry->entry_id_str);

    tar->entry = entry;

    tar->pointers_usable = 1;

    return 0;
}

int is_pointing_to_db (const dir_info* tar) {
    int i;

    if (tar->db_name[0] == 0) {
        return 0;
    }

    for (i = 0; i < EID_STR_MAX; i++) {
        if (tar->entry_id[i] != '0') {
            return 0;
        }
    }

    return 1;
}

int copy_dir (dir_info* dst, const dir_info* src) {
    dst->root = src->root;

    strcpy(dst->db_name,    src->db_name);

    strcpy(dst->entry_id,   src->entry_id);

    dst->pointers_usable = src->pointers_usable;

    dst->dh = src->dh;

    dst->entry = src->entry;

    return 0;
}

int init_dir_info (dir_info* info, root_dir* root, char* db_name) {
    info->root = root;

    database_handle* temp_dh_find;

    if (db_name) {
        lookup_db_name(root->db, db_name, &temp_dh_find);
        if (!temp_dh_find) {
            printf("init_dir_info : no such database\n");
            return DUPLICATE_DB;
        }
        strcpy(info->db_name, db_name);
    }
    else {
        info->db_name[0] = 0;
    }

    mem_wipe_sec(info->entry_id, EID_STR_MAX);

    info->pointers_usable = 0;
    info->dh = NULL;
    info->entry = NULL;

    return 0;
}

int init_root_dir (root_dir* root) {
    root->db = 0;

    root->db_num = 0;

    return 0;
}

int update_dir_pointers(dir_info* dir, error_handle* er_h) {
    dir->pointers_usable = 0;

    if (is_pointing_to_root(dir)) {
        dir->dh = NULL;
        dir->entry = NULL;
        dir->pointers_usable = 1;
    }
    else {
        lookup_db_name(dir->root->db, dir->db_name, &dir->dh);
        if (!dir->dh) {
            error_mark_starter(er_h, "update_dir_pointers");
            error_write(er_h, "no such directory");
            return NO_SUCH_LOGIC_DIR;
        }

        if (is_pointing_to_db(dir)) {
            dir->entry = &dir->dh->tree;
        }
        else {
            lookup_entry_id_via_dh(dir->dh, dir->entry_id, &dir->entry);
            if (!dir->entry) {
                error_mark_starter(er_h, "update_dir_pointers");
                error_write(er_h, "no such directory");
                return NO_SUCH_LOGIC_DIR;
            }
        }
    }

    return 0;
}

int add_dh_to_root_dir (root_dir* root, char* name, database_handle** ret_dh, error_handle* er_h) {
    database_handle* temp_dh;
    database_handle* temp_dh_find;

    error_mark_starter(er_h, "add_dh_to_root_dir");

    if (ret_dh) {
        *ret_dh = 0;
    }

    lookup_db_name(root->db, name, &temp_dh_find);
    if (temp_dh_find) {
        error_write(er_h, "database with same name already exists");
        return DUPLICATE_DB;
    }

    temp_dh = malloc(sizeof(database_handle));
    if (!temp_dh) {
        error_write(er_h, "failed to allocate memory for database");
        return MALLOC_FAIL;
    }

    mem_wipe_sec(temp_dh, sizeof(database_handle));

    init_database_handle(temp_dh);

    // copy the name into the newly created database handle
    strcpy(temp_dh->name, name);
    strcpy(temp_dh->tree.file_name, name);

    add_db(&root->db, temp_dh);

    root->db_num++;

    if (ret_dh) {
        *ret_dh = temp_dh;
    }

    return 0;
}

int del_dh_from_root_dir (root_dir* root, char* name, error_handle* er_h) {
    database_handle* temp_dh_find;

    error_mark_starter(er_h, "del_dh_from_root_dir");

    lookup_db_name(root->db, name, &temp_dh_find);
    if (!temp_dh_find) {
        error_write(er_h, "no such database\n");
        return FIND_FAIL;
    }

    fres_database_handle(temp_dh_find);

    del_db(&root->db, temp_dh_find);

    mem_wipe_sec(temp_dh_find, sizeof(database_handle));

    free(temp_dh_find);

    root->db_num--;

    return 0;
}

int path_to_dir(const dir_info* cur_dir_p, const char* path_buf, str_len_int* rem_len, simple_bitmap* map_buf, simple_bitmap* map_buf2, dir_info* dir, int mode, error_handle* er_h) {
    str_len_int len_read;

    int i, ret;

    bit_index num_used;

    database_handle* temp_dh_find;

    database_handle* cur_dh;
    linked_entry* cur_entry;

    dir_info cur_dir;

    database_handle* iter_dh, * iter_dh_temp;

    char buf[PATH_LEN_MAX+1];

    str_len_int path_len;       // length of received path
    str_len_int rem_path_len;   // remaining path length

    const int entry_buf_size = 2;
    linked_entry* entry_buf[entry_buf_size];

    const char* path_cur;

    if (        mode != PDIR_MODE_FILENAME
            &&  mode != PDIR_MODE_EID
       )
    {
        return WRONG_ARGS;
    }

    error_mark_starter(er_h, "path_to_dir");

    if (rem_len) {
        if (path_buf[*rem_len] != 0) {
            // should not be reachable
            printf("path_to_dir : supposed unreachable section reached, section 1\n");
            return VERIFY_FAIL;
        }

        path_len = *rem_len;
    }
    else {
        if ((i = verify_str_terminated(path_buf, PATH_LEN_MAX, &path_len, ALLOW_ZERO_LEN_STR))) {
            error_write(er_h, "path too long or not properly terminated");
            return i;
        }
    }

    copy_dir(&cur_dir, cur_dir_p);

    if (path_len == 0) {
        return EMPTY_PATH;
    }

    path_cur = path_buf;
    rem_path_len = path_len;

    // default to cur_dir's position
    copy_dir(dir, &cur_dir);

    // parse path
    // check if root
    if (*path_cur == '/') {   // absolute path
        if (path_len == 1) {    // root itself
            point_to_root(dir);
            return 0;
        }
        path_cur        += 1;  // jump over leading slash
        rem_path_len    -= 1;

        // check if any database has been added, if not return no such directory
        if (cur_dir.root->db_num == 0) {
            error_write(er_h, "no such directory");
            return NO_SUCH_LOGIC_DIR;
        }

        // grab database name
        grab_until_sep(buf, path_cur, rem_path_len, &len_read, NULL, IGNORE_ESCAPE_CHAR, '/');
        if (len_read > FILE_NAME_MAX) {
            error_write(er_h, "no such directory");
            return NO_SUCH_LOGIC_DIR;
        }
        path_cur        += len_read;   // jump over database name
        rem_path_len    -= len_read;

        // find database
        lookup_db_name(cur_dir.root->db, buf, &temp_dh_find);
        if (temp_dh_find) {     // found exact match
            point_to_dh(dir, temp_dh_find);
        }
        else {  // no exact match
            // do partial lookup
            i = 0;
            HASH_ITER(hh, cur_dir.root->db, iter_dh, iter_dh_temp) {
                if (strstr(iter_dh->name, buf)) {
                    i++;
                    temp_dh_find = iter_dh;
                    if (i > 1) {
                        break;
                    }
                }
            }
            if (i == 0) {
                error_write(er_h, "no such directory");
                return NO_SUCH_LOGIC_DIR;
            }
            else if (i > 1) {
                error_write(er_h, "ambiguous path");
                return FOUND_DUPLICATE;
            }

            point_to_dh(dir, temp_dh_find);
            notify_ambig_resolve(buf, temp_dh_find->name);
        }

        // check if we're done
        // either this is the last chunk of characters already
        // or there's one more character, which must be slash(due to how grab_until_seperator works)
        if (is_end_of_path(rem_path_len)) {
            return 0;
        }

        // take away the slash
        path_cur        += 1;
        rem_path_len    -= 1;

        // traverse within database
        return path_to_dir(dir, path_cur, &rem_path_len, map_buf, map_buf2, dir, mode, er_h);
    }
    else {          // relative path
        // handle case when current directory no longer exists
        if (!is_pointing_to_root(&cur_dir)) {    // if not pointing to root dir
            // force update pointers
            ret = update_dir_pointers(&cur_dir, er_h);
            if (ret) {
                return ret;
            }

            cur_dh = cur_dir.dh;
            cur_entry = cur_dir.entry;
        }

        grab_until_sep(buf, path_buf, rem_path_len, &len_read, NULL, IGNORE_ESCAPE_CHAR, '/');

        if (mode == PDIR_MODE_FILENAME) {
            if (len_read > FILE_NAME_MAX) {
                error_write(er_h, "no such directory");
                return NO_SUCH_LOGIC_DIR;
            }
        }
        else if (mode == PDIR_MODE_EID) {
            if (len_read > EID_STR_MAX) {
                error_write(er_h, "no such directory");
                return NO_SUCH_LOGIC_DIR;
            }
        }
        path_cur        += len_read;
        rem_path_len    -= len_read;

        if (len_read == 1 && buf[0] == '.') {  // current dir
            copy_dir(dir, &cur_dir);
        }
        else if (len_read == 2 && (buf[0] == '.' && buf[1] == '.')) {   // previous dir
            if (is_pointing_to_root(&cur_dir)) {  // already in root, can't do anything
                ;; // do nothing
            }
            // in a database otherwise
            else if (is_pointing_to_db(&cur_dir)) {       // root of database itself
                point_to_root(dir);     // point to root dir
            }
            // in a branch otherwise
            else if (!cur_entry->has_parent) {      // head of a branch
                point_to_dh(dir, cur_dh);       // point to database
            }
            // a normal entry otherwise
            else {
                // point to parent
                point_to_entry(dir, cur_entry->parent);
            }
        }
        else {      // go look for a certain child
            if (is_pointing_to_root(&cur_dir)) { // in root
                // attempt exact match via hashtable
                lookup_db_name(cur_dir.root->db, buf, &temp_dh_find);
                if (temp_dh_find) {     // found exact match
                    point_to_dh(dir, temp_dh_find);
                }
                else {  // no exact match
                    // do partial lookup
                    i = 0;
                    HASH_ITER(hh, cur_dir.root->db, iter_dh, iter_dh_temp) {
                        if (strstr(iter_dh->name, buf)) {
                            i++;
                            temp_dh_find = iter_dh;
                            if (i > 1) {
                                break;
                            }
                        }
                    }
                    if (i == 0) {
                        error_write(er_h, "no such directory");
                        return NO_SUCH_LOGIC_DIR;
                    }
                    else if (i > 1) {
                        error_write(er_h, "ambiguous path");
                        return FOUND_DUPLICATE;
                    }

                    point_to_dh(dir, temp_dh_find);
                    notify_ambig_resolve(buf, temp_dh_find->name);
                }
            }
            else {  // root of database, or head of branch, or normal entry
                if (mode == PDIR_MODE_FILENAME) {
                    find_children_via_file_name(cur_dh, map_buf, map_buf2, buf, cur_entry, entry_buf, entry_buf_size, &num_used);
                }
                else if (mode == PDIR_MODE_EID) {
                    // overly lengthy
                    if (len_read > EID_STR_MAX) {
                        error_write(er_h, "no such directory");
                        return NO_SUCH_LOGIC_DIR;
                    }

                    upper_to_lower_case(buf);

                    find_children_via_entry_id_part(cur_dh, map_buf, map_buf2, buf, cur_entry, entry_buf, entry_buf_size, &num_used);
                }

                if (num_used == 0) {
                    error_write(er_h, "no such directory");
                    return NO_SUCH_LOGIC_DIR;
                }
                else if (num_used > 1) {
                    error_write(er_h, "ambiguous path");
                    return FOUND_DUPLICATE;
                }

                // point to entry
                point_to_entry(dir, entry_buf[0]);

                // report ambiguous resolve
                if (mode == PDIR_MODE_FILENAME) {
                    if (strcmp(entry_buf[0]->file_name, buf) != 0) {
                        notify_ambig_resolve(buf, entry_buf[0]->file_name);
                    }
                }
                else if (mode == PDIR_MODE_EID) {
                    if (strcmp(entry_buf[0]->entry_id_str, buf) != 0) {
                        notify_ambig_resolve(buf, entry_buf[0]->entry_id_str);
                    }
                }
            }
        }

        // check if we're done
        // either this is the last chunk of characters already
        // or there's one more character, which must be slash
        if (is_end_of_path(rem_path_len)) {
            return 0;
        }

        // take away the slash
        path_cur        += 1;
        rem_path_len    -= 1;

        // traverse within database
        return path_to_dir(dir, path_cur, &rem_path_len, map_buf, map_buf2, dir, mode, er_h);
    }

    return LOGIC_ERROR; // should not be reachable
}

int dir_to_path(const dir_info* dir_p, char* path_buf, int mode, error_handle* er_h) {
    linked_entry* temp_entry;

    linked_entry* cur_entry;
    database_handle* cur_dh;

    str_len_int path_pos = 0;
    char* path_cur = path_buf;
    str_len_int str_len;

    char path_buf2[PATH_LEN_MAX];
    char* path_cur2 = path_buf2;
    str_len_int path_pos2 = 0;

    dir_info dir;

    copy_dir(&dir, dir_p);

    error_mark_starter(er_h, "dir_to_path");

    int ret;

    if (    mode != PDIR_MODE_FILENAME
        &&  mode != PDIR_MODE_EID
       )
    {
        return WRONG_ARGS;
    }

    if (is_pointing_to_root(&dir)) {    // root of logic directory, not in any database
        ret_if_add_len_too_long(path_pos, 1, er_h);
        path_buf[path_pos] = '/';
        path_pos += 1;
        path_cur += 1;
    }
    else {
        // check if dir is still pointint to an active entry
        // force update dir pointers
        ret = update_dir_pointers(&dir, er_h);
        if (ret) {
            return ret;
        }

        cur_dh = dir.dh;
        cur_entry = dir.entry;

        if (is_pointing_to_db(&dir)) { // root of database
            ret_if_add_len_too_long(path_pos, 1, er_h);
            path_buf[path_pos] = '/';
            path_pos += 1;
            path_cur += 1;

            str_len = strlen(dir.db_name);
            ret_if_add_len_too_long(path_pos, str_len, er_h);
            strcpy(path_cur, dir.db_name);
            path_pos += str_len;
            path_cur += str_len;
        }
        else {
            if (!cur_entry->has_parent) { // head of branch
                // write root
                ret_if_add_len_too_long(path_pos, 1, er_h);
                path_buf[path_pos] = '/';
                path_pos += 1;
                path_cur += 1;

                // write database name
                str_len = strlen(dir.db_name);
                ret_if_add_len_too_long(path_pos, str_len, er_h);
                strcpy(path_cur, dir.db_name);
                path_pos += str_len;
                path_cur += str_len;

                // write slash
                ret_if_add_len_too_long(path_pos, 1, er_h);
                path_buf[path_pos] = '/';
                path_pos += 1;
                path_cur += 1;

                // write entry
                if (mode == PDIR_MODE_FILENAME) {
                    str_len = strlen(cur_entry->file_name);
                    ret_if_add_len_too_long(path_pos, str_len, er_h);
                    strcpy(path_cur, cur_entry->file_name);
                    path_pos += str_len;
                    path_cur += str_len;
                }
                else if (mode == PDIR_MODE_EID) {
                    ret_if_add_len_too_long(path_pos, EID_STR_MAX, er_h);
                    strcpy(path_cur, cur_entry->entry_id_str);
                    path_pos += EID_STR_MAX;
                    path_cur += EID_STR_MAX;
                }
            }
            else {      // normal entry
                // write root
                ret_if_add_len_too_long(path_pos, 1, er_h);
                path_buf[path_pos] = '/';
                path_pos += 1;
                path_cur += 1;

                // write database name
                str_len = strlen(dir.db_name);
                ret_if_add_len_too_long(path_pos, str_len, er_h);
                strcpy(path_cur, dir.db_name);
                path_pos += str_len;
                path_cur += str_len;

                if (mode == PDIR_MODE_FILENAME) {
                    // check if path will be possibly too long
                    // taking upper bound here obviously
                    // (the + 1 part is for the slash)
                    if (cur_entry->depth * (FILE_NAME_MAX + 1) > PATH_LEN_MAX - path_pos) { // too long
                        // fill in "/.../" instead and add the file name
                        ret_if_add_len_too_long(path_pos, 5, er_h);
                        strcpy(path_cur, "/.../");
                        path_pos += 5;
                        path_cur += 5;

                        // fill in file name
                        str_len = strlen(cur_entry->file_name);
                        ret_if_add_len_too_long(path_pos, str_len, er_h);
                        strcpy(path_cur, cur_entry->file_name);
                        path_pos += str_len;
                        path_cur += str_len;
                    }
                    else {      // enough space
                        temp_entry = cur_entry;
                        while (temp_entry != &cur_dh->tree) {    // while root of db not reached
                            str_len = strlen(temp_entry->file_name);
                            /* no need to check if path too long */
                            reverse_strcpy(path_cur2, temp_entry->file_name);
                            path_pos2 += str_len;
                            path_cur2 += str_len;
                            path_buf2[path_pos2] = '/';
                            path_pos2 += 1;
                            path_cur2 += 1;

                            temp_entry = temp_entry->parent;
                        }
                        path_buf2[path_pos2] = 0;
                        reverse_strcpy(path_cur, path_buf2);
                        path_pos += path_pos2;
                        path_cur += path_pos2;
                    }
                }
                else if (mode == PDIR_MODE_EID) {
                    // check if path will be too long
                    // taking upper bound here obviously
                    // (the + 1 part is for the slash)
                    if (cur_entry->depth * (EID_STR_MAX + 1) > PATH_LEN_MAX - path_pos) {   // too long
                        // fill in "/.../" instead and add the entry id
                        ret_if_add_len_too_long(path_pos, 5, er_h);
                        strcpy(path_cur, "/.../");
                        path_pos += 5;
                        path_cur += 5;

                        // fill in entry id
                        ret_if_add_len_too_long(path_pos, EID_STR_MAX, er_h);
                        strcpy(path_cur, cur_entry->entry_id_str);
                        path_pos += EID_STR_MAX;
                        path_cur += EID_STR_MAX;
                    }
                    else {      // enough space
                        temp_entry = cur_entry;
                        while (temp_entry != &cur_dh->tree) {    // while root of db not reached
                            /* no need to check if path too long */
                            reverse_strcpy(path_cur2, temp_entry->entry_id_str);
                            path_pos2 += EID_STR_MAX;
                            path_cur2 += EID_STR_MAX;
                            path_buf2[path_pos2] = '/';
                            path_pos2 += 1;
                            path_cur2 += 1;

                            temp_entry = temp_entry->parent;
                        }
                        path_buf2[path_pos2] = 0;
                        reverse_strcpy(path_cur, path_buf2);
                        path_pos += path_pos2;
                        path_cur += path_pos2;
                    }
                }
            }
        }
    }

    path_buf[path_pos] = 0;

    return 0;
}

int print_root_dir (root_dir* root) {
    database_handle* db, * tmp;
    int i;

    if (!root) {
        printf("print_root_dir : root pointer is null\n");
        return 0;
    }

    printf("db_num : %"PRIu32"\n", root->db_num);

    i = 0;
    HASH_ITER(hh, root->db, db, tmp) {
        printf("database #%d, name : %s\n", i, db->name);
        i++;
    }

    return 0;
}

int print_dir (const dir_info* dir) {
    printf("root : %p\n", dir->root);
    print_root_dir(dir->root);

    printf("db_name : %s\n", dir->db_name);

    printf("entry_id : %s\n", dir->entry_id);

    printf("pointers_usable : %d\n", dir->pointers_usable);

    printf("database handle pointer : %p\n", dir->dh);
    
    printf("entry pointer : %p\n", dir->entry);

    return 0;
}
