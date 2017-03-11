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

#ifndef FFP_DATABASE_H
#define FFP_DATABASE_H

/* Entry validation
 * 
 * This method ensures the specified entry conform to specification of the entry
 *
 * This method is whitelist based, thus by default(i.e. flags = 0),
 * all requirements are necessary
 *
 * This method will return whenever an error is discovered
 * call needs to handle the error afterwards
 *
 * This method will perform pointer check, and may induce segmentation
 * fault if pointers are not pointing to correct memory locations
 *
 * This method should NOT be the only code for checking in any sequence of interfacing
 * to outside world. E.g. reading file, adding new entry, taking user input
 *
 * This method will NOT go through the entire linked list, for any of the linked list within
 * an entry,
 * so it cannot detect looped linked list or broken linked list beyond the depth of one
 *
 * This method CANNOT detect whether entry is corrupted in its data fields
 *
 * This method will NOT ensure uniqueness of entry id, branch id,
 * such enforcement has to be done before actually adding to the hashtable
 */
int verify_entry (database_handle* dh, linked_entry* entry, uint32_t flags);

int verify_eid_to_e(t_eid_to_e* eid_to_e, int* error_code, uint32_t flags);

int verify_fn_to_e(t_fn_to_e* fn_to_e, int* error_code, uint32_t flags);

int verify_tag_to_e(t_tag_to_e* tag_to_e, int* error_code, uint32_t flags);

int verify_sha1f_to_fd(t_sha1f_to_fd* sha1f_to_fd, int* error_code, uint32_t flags);
int verify_sha256f_to_fd(t_sha256f_to_fd* sha256f_to_fd, int* error_code, uint32_t flags);
int verify_sha512f_to_fd(t_sha512f_to_fd* sha512f_to_fd, int* error_code, uint32_t flags);

int verify_sha1s_to_s(t_sha1s_to_s* sha1s_to_s, int* error_code, uint32_t flags);
int verify_sha256s_to_s(t_sha256s_to_s* sha256s_to_s, int* error_code, uint32_t flags);
int verify_sha512s_to_s(t_sha512s_to_s* sha512s_to_s, int* error_code, uint32_t flags);

int verify_f_size_to_fd(t_f_size_to_fd* f_size_to_fd, int* error_code, uint32_t flags);

int verify_dh (database_handle* dh, uint32_t flags);

/* Rule of the functions which access structures under database handle :
 * (lookup and add functions for database handle are similar,
 * but they take hash table to handles as first argument instead)
 *
 *      Overriding rule :
 *          Return WRONG_ARGS upon invalid arguments
 *
 *      For exact lookup functions :
 *          Take database handle as first argument
 *          Take target value/data as second argument
 *          Take pointer to result as last argument
 *
 *          If target is found :
 *              set *result = pointer to target(or address of target)
 *              return 0
 *          If target is not found :
 *              set *result = 0
 *              return FIND_FAIL
 *
 *      For partial/blurry lookup functions using buffers :
 *          Take database handle as first argument
 *          Take target value/data as second argument
 *          Take buffer size as third last argument
 *          Take pointer to buffer as second last argument
 *          Take pointer to number used integer as last argument
 *          (Buffer is an array of pointers to the target type)
 *
 *          If target(s) is/are found :
 *              Keep adding :
 *                  If buffer is not full :
 *                      fill the buffer sequentially
 *                  If buffer is full :
 *                      set number used integer accordingly
 *                      return BUFFER_FULL
 *                  If no more targets to add :
 *                      set number used integer accordingly
 *                      return 0
 *          If no target(s) is/are found :
 *              set first pointer in buffer to 0
 *              set *num_used = 0
 *              return FIND_FAIL
 *
 *      For entry adding functions :
 *          Take database handle as first argument
 *          Take pointer to target(object to be added) as second argument
 *          If already exists :
 *              return DUPLICATE_ERROR
 *          else :
 *              add
 *              return 0
 */
int lookup_db_name (database_handle* dh_table, const char* name, database_handle** result);

int add_db (database_handle** dh_table, database_handle* target);

int del_db (database_handle** dh_table, database_handle* dh);

/* Entry ID lookup */
int lookup_entry_id_via_dh (database_handle* dh, const char* eid_str, linked_entry** result);

int lookup_entry_id_part_via_dh (database_handle* dh, const char* eid_str_part, simple_bitmap* map_buf, simple_bitmap* map_result, t_eid_to_e** result_buf, bit_index buf_size, bit_index* num_used);

int lookup_entry_id_part_map_via_dh (database_handle* dh, const char* name_part, simple_bitmap* map_buf, simple_bitmap* map_result);

/* File name lookup */
int lookup_file_name_via_dh (database_handle* dh, const char* file_name, linked_entry** result);

int lookup_file_name_part_via_dh (database_handle* dh, const char* name_part, simple_bitmap* map_buf, simple_bitmap* map_result, t_fn_to_e** result_buf, bit_index buf_size, bit_index* num_used);

int lookup_file_name_part_map_via_dh (database_handle* dh, const char* name_part, simple_bitmap* map_buf, simple_bitmap* map_result);

/* Tag lookup */
int lookup_tag_str_via_dh (database_handle* dh, const char* tag_str, linked_entry** result);

int lookup_tag_via_dh_with_preproc (database_handle* dh, const char* in_tag, simple_bitmap* map_buf, simple_bitmap* map_result, t_tag_to_e** result_buf, bit_index buf_size, bit_index* num_used);

int lookup_tag_map_via_dh_with_preproc (database_handle* dh, const char* in_tag, simple_bitmap* map_buf, simple_bitmap* map_result);

int lookup_tag_part_via_dh_with_preproc (database_handle* dh, const char* in_tag, simple_bitmap* map_buf, simple_bitmap* map_result, t_tag_to_e** result_buf, bit_index buf_size, bit_index* num_used);

int lookup_tag_part_map_via_dh_preproc (database_handle* dh, const char* in_tag, simple_bitmap* map_buf, simple_bitmap* map_result);

/* File data checksum lookup */
int lookup_file_sha1_via_dh (database_handle* dh, const char* checksum, file_data** result);
int lookup_file_sha256_via_dh (database_handle* dh, const char* checksum, file_data** result);
int lookup_file_sha512_via_dh (database_handle* dh, const char* checksum, file_data** result);

int lookup_file_sha1_part_via_dh (database_handle* dh, const char* checksum_part, simple_bitmap* map_buf, simple_bitmap* map_result, t_sha1f_to_fd** result_buf, bit_index buf_size, bit_index* num_used);
int lookup_file_sha256_part_via_dh (database_handle* dh, const char* checksum_part, simple_bitmap* map_buf, simple_bitmap* map_result, t_sha256f_to_fd** result_buf, bit_index buf_size, bit_index* num_used);
int lookup_file_sha512_part_via_dh (database_handle* dh, const char* checksum_part, simple_bitmap* map_buf, simple_bitmap* map_result, t_sha512f_to_fd** result_buf, bit_index buf_size, bit_index* num_used);

int lookup_file_sha1_part_map_via_dh (database_handle* dh, const char* checksum_part, simple_bitmap* map_buf, simple_bitmap* map_result);
int lookup_file_sha256_part_map_via_dh (database_handle* dh, const char* checksum_part, simple_bitmap* map_buf, simple_bitmap* map_result);
int lookup_file_sha512_part_map_via_dh (database_handle* dh, const char* checksum_part, simple_bitmap* map_buf, simple_bitmap* map_result);

/* Section checksum lookup */
int lookup_sect_sha1_via_dh (database_handle* dh, const char* checksum, section** result);
int lookup_sect_sha256_via_dh (database_handle* dh, const char* checksum, section** result);
int lookup_sect_sha512_via_dh (database_handle* dh, const char* checksum, section** result);

int lookup_sect_sha1_part_via_dh (database_handle* dh, const char* checksum_part, simple_bitmap* map_buf, simple_bitmap* map_result, t_sha1s_to_s** result_buf, bit_index buf_size, bit_index* num_used);
int lookup_sect_sha256_part_via_dh (database_handle* dh, const char* checksum_part, simple_bitmap* map_buf, simple_bitmap* map_result, t_sha256s_to_s** result_buf, bit_index buf_size, bit_index* num_used);
int lookup_sect_sha512_part_via_dh (database_handle* dh, const char* checksum_part, simple_bitmap* map_buf, simple_bitmap* map_result, t_sha512s_to_s** result_buf, bit_index buf_size, bit_index* num_used);

int lookup_sect_sha1_part_map_via_dh (database_handle* dh, const char* checksum_part, simple_bitmap* map_buf, simple_bitmap* map_result);
int lookup_sect_sha256_part_map_via_dh (database_handle* dh, const char* checksum_part, simple_bitmap* map_buf, simple_bitmap* map_result);
int lookup_sect_sha512_part_map_via_dh (database_handle* dh, const char* checksum_part, simple_bitmap* map_buf, simple_bitmap* map_result);

/* File size lookup */
int lookup_file_size_via_dh (database_handle* dh, const char* file_size, file_data** result);

int lookup_file_size_part_via_dh (database_handle* dh, const char* file_size_part, simple_bitmap* map_buf, simple_bitmap* map_result, t_f_size_to_fd** result_buf, bit_index buf_size, bit_index* num_used);

int lookup_file_size_part_map_via_dh (database_handle* dh, const char* file_size_part, simple_bitmap* map_buf, simple_bitmap* map_result);

/* Date time lookup */
int lookup_date_time (database_handle* dh, unsigned char date_type, int16_t year, uint8_t month, uint8_t day, uint8_t hour, linked_entry** result);

int find_entry_in_sub_branch(database_handle* dh, linked_entry* sub_branch_head, field_record* rec, unsigned char only_examine_latest, field_match_bitmap* match_map, unsigned int score);

int find_children_via_file_name(database_handle* dh, simple_bitmap* map_buf, simple_bitmap* map_buf2, char* file_name, linked_entry* parent, linked_entry** result_buf, bit_index buf_size, bit_index* num_used);

int find_children_via_entry_id_part(database_handle* dh, simple_bitmap* map_buf, simple_bitmap* map_buf2, char* entry_id, linked_entry* parent, linked_entry** result_buf, bit_index buf_size, bit_index* num_used);

int gen_entry_id(unsigned char* entry_id, char* entry_id_str);

int set_new_entry_id(database_handle* dh, linked_entry* entry);

/* Note:
 *      the following fields are required to be setup before calling:
 *          entry id
 *          parent pointer(added to a parent entry)
 *      set_entry_general_info sets the following fields :
 *          has parent flag
 *          branch id
 *          depth
 *          time of addition
 *
 *      set_entry_general_info links entry to date time tree as well
 */
int set_entry_general_info(database_handle* dh, linked_entry* entry);

int link_branch (database_handle* dh, linked_entry* entry, linked_entry* tail, linked_entry** ret_tail);

int link_branch_ends (database_handle* dh, linked_entry* entry, linked_entry* tail, linked_entry** ret_tail);

//int add_children(database_handle* dh, linked_entry* parent, linked_entry* entry);

int link_entry_to_entry_id_structures(database_handle* dh, linked_entry* target);

int unlink_entry_from_entry_id_structures(database_handle* dh, linked_entry* target);

int link_entry_to_file_name_structures(database_handle* dh, linked_entry* target);

int link_entry_to_tag_str_structures(database_handle* dh, linked_entry* target);

int link_entry_to_date_time_tree(database_handle* dh, linked_entry* target, unsigned char MODE);

int unlink_entry_from_date_time_tree(database_handle* dh, linked_entry* target, unsigned char MODE);

int link_file_data_to_file_size_structures(database_handle* dh, file_data* target);

int link_file_data_to_sha1_structures(database_handle* dh, file_data* target, checksum_result* target_checksum_result);

int link_file_data_to_sha512_structures(database_handle* dh, file_data* target, checksum_result* target_checksum_result);

int link_file_data_to_sha256_structures(database_handle* dh, file_data* target, checksum_result* target_checksum_result);

int link_file_data_to_checksum_structures(database_handle* dh, file_data* target);

int grow_children_array(linked_entry* parent, ffp_eid_int increment);

int put_into_children_array(linked_entry* parent, linked_entry* entry);

int grow_section_array(file_data* target, uint64_t increment);

int put_into_section_array(file_data* target, section* sect);

int grow_sect_field_array(field_record* rec, uint64_t increment);

int put_into_sect_field_array(field_record* rec, sect_field* sect);

int init_field_record(layer2_sect_field_arr* l2_arr, field_record* rec);

int init_field_match_bitmap(field_match_bitmap* match_map);

int link_sect_to_sha1_structures(database_handle* dh, section* target, checksum_result* target_checksum_result);

int link_sect_to_sha256_structures(database_handle* dh, section* target, checksum_result* target_checksum_result);

int link_sect_to_sha512_structures(database_handle* dh, section* target, checksum_result* target_checksum_result);

int link_sect_to_checksum_structures(database_handle* dh, section* target);

int link_entry (database_handle* dh, linked_entry* new_parent, linked_entry* child);

int copy_entry(database_handle* dh, linked_entry* dst_parent, linked_entry* src_entry, unsigned char recursive);

int del_section (database_handle* dh, section* sect);

int del_file_data (database_handle* dh, file_data* data);

int del_entry (database_handle* dh, linked_entry* entry);

/* map conversion */
int part_name_trans_map_to_entry_map(layer2_fn_to_e_arr* l2_arr, simple_bitmap* name_trans_map, simple_bitmap* entry_map, char* name_part);

int part_f_size_trans_map_to_entry_map(layer2_f_size_to_fd_arr* l2_arr, simple_bitmap* f_size_trans_map, simple_bitmap* entry_map, char* f_size_part);

int part_f_sha1_trans_map_to_entry_map(layer2_sha1f_to_fd_arr* l2_arr, simple_bitmap* f_sha1_trans_map, simple_bitmap* entry_map, char* f_sha1_part);

int part_f_sha256_trans_map_to_entry_map(layer2_sha256f_to_fd_arr* l2_arr, simple_bitmap* f_sha256_trans_map, simple_bitmap* entry_map, char* f_sha256_part);

int part_f_sha512_trans_map_to_entry_map(layer2_sha512f_to_fd_arr* l2_arr, simple_bitmap* f_sha512_trans_map, simple_bitmap* entry_map, char* f_sha512_part);

int part_s_sha1_trans_map_to_entry_map(layer2_sha1s_to_s_arr* l2_arr, simple_bitmap* s_sha1_trans_map, simple_bitmap* entry_map, char* s_sha1_part);

int part_s_sha256_trans_map_to_entry_map(layer2_sha256s_to_s_arr* l2_arr, simple_bitmap* s_sha256_trans_map, simple_bitmap* entry_map, char* s_sha256_part);

int part_s_sha512_trans_map_to_entry_map(layer2_sha512s_to_s_arr* l2_arr, simple_bitmap* s_sha512_trans_map, simple_bitmap* entry_map, char* s_sha512_part);

#endif
