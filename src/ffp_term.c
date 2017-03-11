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

#include "ffp_term.h"

#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

#define SET_IN_FUNC() \
    __sync_synchronize();   \
    in_function = 1;        \
    __sync_synchronize();

#define SET_NOT_IN_FUNC() \
    __sync_synchronize();   \
    in_function = 0;        \
    __sync_synchronize();

#define notify_ambig_resolve(fuzzy_input, result) \
    printf("locator_to_dir : %s resolved to %s\n", fuzzy_input, result)

#define MARK_NEED_CLEANUP() \
    __sync_synchronize();   \
    need_cleanup = 1;       \
    __sync_synchronize();

#define MARK_NO_NEED_CLEANUP() \
    __sync_synchronize();   \
    need_cleanup = 0;       \
    __sync_synchronize();

static int prompt_func_jmp_set = 0;
static jmp_buf prompt_func;           // for jumping back to prompt function
static int inthandler_set = 0;
static int in_function = 0;
static int cancel_line = 0;

// buffer pointer for readline function call
static char* cmd_buf = NULL;
// flag to let process_cmd know if any cleaning up is needed
static int need_cleanup = 0;
/* loose resources which process_cmd may need to invoke cleanup functions for */
// for fls
static DIR* fls_dirp = NULL;
// for fp
static char cwd_backup[1024];
static FILE* file_being_used = NULL;
static database_handle* dh_being_used = NULL;
static linked_entry* entry_being_used = NULL;
static int l2_dirp_record_arr_set = 0;
static layer2_dirp_record_arr l2_dirp_record_arr;
static bit_index max_dirp_record_index = 0;

/*  Note on locator_to_dir :
 *      locator_to_dir stays silent and leave error message reporting
 *      to upper level of caller who has better knowledge in context
 *
 *      locator_to_dir stores error in a error handle when the error is very specific
 *      (e.g. part of locator is invalid) and the overhead of leaving the
 *      work to upper level of caller can be high - need to create a lot more
 *      error code
 *
 *      locator_to_dir returns one of the following error code :
 *          CUR_DIR_INVALID     - non-existent current logical directory
 *          NO_SUCH_ALIAS       - failed alias matching
 *          NO_SUCH_LOGIC_DIR   - failed locator resolution
 *          FOUND_DUPLICATE     - resolved to more than one target - ambiguous locator
 *          WRONG_ARGS          - invalid argument passed to function call(should not happen)
 *                                or invalid locator(most likely)
 *          UNKNOWN_ERROR       - unknown error from sub function calls
 *                                likely from path_to_dir, if any error at all
 *
 *      returns 0 if no errors
 */

/*      general template using switch

        switch (ret) {
            case 0:
                // do nothing
                break;
            case CUR_DIR_INVALID:
                printf("OP_NAME : current directory does not exist\n");
                return ret;
            case NO_SUCH_ALIAS:
                printf("OP_NAME : no such alias\n");
                return ret;
            case NO_SUCH_LOGIC_DIR:
                printf("OP_NAME : no such entry\n");
                return ret;
            case FOUND_DUPLICATE:
                printf("OP_NAME : ambiguous locator\n");
                return ret;
            case WRONG_ARGS:
                printf("OP_NAME : invalid locator\n");
                return ret;
            case UNKNOWN_ERROR:
                printf("OP_NAME : unknown error\n");
                return ret;
            default:
                printf("OP_NAME : unknown error\n");
                printf("error code : %d\n", ret);
                return ret;
        }
 */

int locator_to_dir(term_info* info, dir_info* dir, const char* locator_str, dir_info* ret_dir, error_handle* er_h) {
    int i, j, ret, len;

    linked_entry* temp_entry_find;
    const bit_index buf_size = 2;
    bit_index num_used;
    t_eid_to_e* eid_to_e_buf[buf_size];
    database_handle* temp_dh_find;
    entry_alias* temp_alias_find;

    dir_info result_dir;

    char str_buf[LOCATOR_LEN_MAX+1];

    const char* str;

    database_handle* iter_dh, * iter_dh_temp;

    if ((ret = verify_str_terminated(locator_str, LOCATOR_LEN_MAX, NULL, 0x0))) {
        return ret;
    }

    if (        !ret_dir
            ||  !er_h
       )
    {
        return WRONG_ARGS;
    }

    copy_dir(&result_dir, dir);

    result_dir.pointers_usable = 0;

    error_mark_starter(er_h, "locator_to_dir");

    if (IS_ALIAS(locator_str)) {                // alias
        str = SKIP_PREFIX(locator_str);

        lookup_alias(info, str, &temp_alias_find);
        if (!temp_alias_find) {
            error_write(er_h, "no such alias");
            return NO_SUCH_ALIAS;
        }

        lookup_db_name(dir->root->db, temp_alias_find->dir.db_name, &temp_dh_find);
        if (!temp_dh_find) {
            error_write(er_h, "no such directory");
            return NO_SUCH_LOGIC_DIR;
        }

        lookup_entry_id_via_dh(temp_dh_find, temp_alias_find->dir.entry_id, &temp_entry_find);
        if (!temp_entry_find) {
            error_write(er_h, "no such directory");
            return NO_SUCH_LOGIC_DIR;
        }

        strcpy(result_dir.db_name, temp_alias_find->dir.db_name);
        strcpy(result_dir.entry_id, temp_alias_find->dir.entry_id);
        result_dir.dh = temp_dh_find;
        result_dir.entry = temp_entry_find;
        result_dir.pointers_usable = 1;
    }
    else if (IS_DB_ID_PAIR(locator_str)) {      // db id pair
        str = SKIP_PREFIX(locator_str);
        strcpy(str_buf, str);

        len = strlen(str_buf);
        for (i = 0; i < len; i++) {
            if (IS_ARROW(str_buf + i)) {
                break;
            }
        }
        if (i == len) {
            error_write(er_h, "\"->\" not found in locator");
            return WRONG_ARGS;
        }
        if (i + 1 == len - 1) {   // no more characters after "->"
            error_write(er_h, "entry id missing after \"->\"");
            return WRONG_ARGS;
        }

        j = i + 2;    // position at which entry id starts

        str_buf[i] = 0;     // null terminate db name which is before the "->"

        if (i == 0) {   // no database name specified, default to current one
            if (is_pointing_to_root(dir)) {
                error_write(er_h, "currently not in any database, no database to default to");
                return WRONG_ARGS;
            }
            lookup_db_name(dir->root->db, dir->db_name, &temp_dh_find);
            if (!temp_dh_find) {
                error_write(er_h, "no such directory");
                return NO_SUCH_LOGIC_DIR;
            }
        }
        else {
            lookup_db_name(dir->root->db, str_buf, &temp_dh_find);
            if (!temp_dh_find) {    // no exact match
                // do partial lookup
                i = 0;
                HASH_ITER(hh, dir->root->db, iter_dh, iter_dh_temp) {
                    if (strstr(iter_dh->name, str_buf)) {
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

                notify_ambig_resolve(str_buf, temp_dh_find->name);
            }
        }

        for (i = j; i < len; i++) {     // check to make sure id is valid hex string
            if (!isxdigit(str_buf[i])) {
                break;
            }
        }
        if (i < len) {
            error_write(er_h, "invalid entry ID after \"->\"");
            return WRONG_ARGS;
        }

        upper_to_lower_case(str_buf + j);

        lookup_entry_id_via_dh(temp_dh_find, str_buf + j, &temp_entry_find);
        if (temp_entry_find) {
            strcpy(result_dir.db_name, temp_dh_find->name);
            strcpy(result_dir.entry_id, str_buf + j);
            result_dir.dh = temp_dh_find;
            result_dir.entry = temp_entry_find;
            result_dir.pointers_usable = 1;
        }
        else {
            // do a partial search
            lookup_entry_id_part_via_dh(temp_dh_find, str_buf + j, &info->map_buf, &info->map_buf2, eid_to_e_buf, buf_size, &num_used);
            if      (   num_used == 0   ) {
                error_write(er_h, "no such directory");
                return NO_SUCH_LOGIC_DIR;
            }
            else if (   num_used == 1   ) {
                strcpy(result_dir.db_name, temp_dh_find->name);
                strcpy(result_dir.entry_id, eid_to_e_buf[0]->str);
                result_dir.dh = temp_dh_find;
                result_dir.entry = eid_to_e_buf[0]->tar;
                result_dir.pointers_usable = 1;
            }
            else /*     num_used > 1   */ {
                error_write(er_h, "ambiguous entry id");
                return FOUND_DUPLICATE;
            }

            notify_ambig_resolve(str_buf + j, eid_to_e_buf[0]->str);
        }
    }
    else if (IS_ENTRY_ID(locator_str)) {        // entry id path
        str = SKIP_PREFIX(locator_str);

        len = strlen(str);

        ret = path_to_dir(dir, str, 0, &info->map_buf, &info->map_buf2, &result_dir, PDIR_MODE_EID, er_h);
        if (ret) {
            return ret;
        }
    }
    else {                                      // file name path
        if (IS_FORCE_FILE(locator_str)) {
            str = SKIP_PREFIX(locator_str);
        }
        else {
            str = locator_str;
        }

        ret = path_to_dir(dir, str, 0, &info->map_buf, &info->map_buf2, &result_dir, PDIR_MODE_FILENAME, er_h);
        if (ret) {
            return ret;
        }
    }

    copy_dir(ret_dir, &result_dir);

    return 0;
}

/*  Note:
 *      str is modified in place to insert a null character, if successful that is
 *
 *      str should have format "key->val"
 *
 *      if key does not exist, *key will be set to NULL
 *      if val does not exist, *val will be set to NULL
 *      if -> does not exist, has_arrow will be set to 0, 1 otherwise
 *
 *      if any of key, ->, val is missing
 *      parse_keyval_pair will return FFP_GENERAL_ERROR
 *
 *      if more than one -> is present
 *      only the first one will be used
 */
static int parse_keyval_pair(char* str, unsigned char* has_arrow, char** key_p, char** val_p) {
    str_len_int str_len;

    str_len = strlen(str);

    str_len_int i;

    char* val = NULL, * arrow = NULL;

    int ret = 0;

    // default to returning null
    *val_p = NULL;
    *key_p = NULL;

    for (i = 0; i < str_len; i++) {
        if (str[i] == '-' && (i + 1 >= str_len ? 0 : str[i+1] == '>')) {
            arrow = str + i;
            val = str + i + 2;
            break;
        }
    }

    if (arrow == NULL) {
        *has_arrow = 0;
    }
    else {
        *has_arrow = 1;
    }

    if (*val == '\0') {         // if val points to null character
        ret = FFP_GENERAL_FAIL;
    }
    else {
        *val_p = val;
    }

    if (i == 0) {               // -> is at the start of str
        ret = FFP_GENERAL_FAIL;
    }
    else {
        *arrow = '\0';      // terminate the key value substring
        *key_p = str;
    }

    return ret;
}

static int repair_keyval_pair(unsigned char has_arrow, char* key, char* val) {
    // if keyval pair was not valid
    if (        !has_arrow
            ||  !key
            ||  !val
       )
    {
        return WRONG_ARGS;
    }

    // revert the null terminator after key back to '-'
    // which is 2 chars before val
    *(val - 2) = '-';

    return 0;
}

static int parse_range(char* str, str_len_int* minus_count_p, char** val_left_p, char** val_right_p) {
    int ret = 0;

    str_len_int str_len;

    str_len = strlen(str);

    str_len_int i;

    char* minus;

    char* val_right = NULL;

    str_len_int minus_count = 0;

    // default to returning null
    *val_left_p = NULL;
    *val_right_p = NULL;

    for (i = 0; i < str_len; i++) {
        if (str[i] == '-') {
            minus_count++;
        }
    }

    if (minus_count_p) {
        *minus_count_p = minus_count;
    }

    if (minus_count == 0) {
        return FFP_GENERAL_FAIL;
    }

    for (i = 0; i < str_len; i++) {
        if (str[i] == '-') {
            minus = str + i;
            val_right = str + i + 1;
            break;
        }
    }

    if (*val_right == '\0') {   // if val_right points to null character
        ret = FFP_GENERAL_FAIL;
    }
    else {
        *val_right_p = val_right;
    }

    if (i == 0) {               // - is at the start of str
        ret = FFP_GENERAL_FAIL;
    }
    else {
        *minus = '\0';      // terminate the left value substring
        *val_left_p = str;
    }

    return ret;
}

int add_alias(term_info* info, entry_alias* alias) {
    entry_alias* temp_alias_find;

    HASH_FIND_STR(info->alias, alias->name, temp_alias_find);
    if (temp_alias_find) {
        printf("add_alias : alias already exists\n");
        return WRONG_ARGS;
    }

    HASH_ADD_STR(info->alias, name, alias);

    return 0;
}

int lookup_alias(term_info* info, const char* name, entry_alias** result_alias) {
    int i;

    entry_alias* temp_alias_find;

    for (i = 0; i < ALIAS_NAME_MAX; i++) {
        if (name[i] == 0) {
            break;
        }
    }
    if (i == ALIAS_NAME_MAX) {
        printf("lookup_alias : name not null terminated\n");
        return WRONG_ARGS;
    }

    HASH_FIND_STR(info->alias, name, temp_alias_find);
    if (!temp_alias_find) {
        *result_alias = 0;
        return NO_SUCH_ALIAS;
    }

    *result_alias = temp_alias_find;

    return 0;
}

int func_match(term_info* info, char* str, int (**handler) (term_info* info, dir_info* dir, int argc, char* argv[]), unsigned char* interruptable, int (**cleaner) ()) {
    int i;

    for (i = 0; i < FUNC_NUM_MAX; i++) {
        if (info->func_arr[i].name[0] == 0) {
            break;
        }

        if (strcmp(info->func_arr[i].name, str) == 0) {
            *handler = info->func_arr[i].handler;
            if (interruptable) {
                *interruptable = info->func_arr[i].interruptable;
            }
            if (cleaner) {
                *cleaner = info->func_arr[i].cleaner;
            }
            return 0;
        }
    }

    *handler = 0;
    if (interruptable) {
        *interruptable = 0;
    }
    if (cleaner) {
        *cleaner = 0;
    }
    return NO_SUCH_FUNC;
}

int process_cmd (term_info* info, char* cmd, dir_info* dir) {
    int ret, ret_cleaner;
    str_len_int len_read;
    int argc = 0;
    uint16_t cmd_pos = 0;
    char func_name[FUNC_NAME_MAX+1];
    char arg_str[COMMAND_BUFFER_SIZE+1];
    char* arg_str_ptr = arg_str;
    char* argv[10];
    unsigned char func_interruptable;

    str_len_int len;
    //str_to_func* temp_func_find;
    int (*func) (term_info* info, dir_info* dir, int argc, char* argv[]);
    int (*cleaner) ();

    if (verify_str_terminated(cmd, COMMAND_BUFFER_SIZE, NULL, 0x0)) {
        printf("process_cmd : command string is not null terminated\n");
        return WRONG_ARGS;
    }

    grab_until_sep(arg_str, cmd, COMMAND_BUFFER_SIZE, &len_read, NULL, NO_IGNORE_ESCAPE_CHAR, ' ');
    cmd_pos += len_read;
    if (len_read == 0) {
        return 0;
    }

    func_match(info, arg_str, &func, &func_interruptable, &cleaner);
    if (!func) {
        printf("process_cmd : no such function - \"%s\"\n", arg_str);
        return NO_SUCH_FUNC;
    }

    strcpy(func_name, arg_str);

    // the arguments are copied over to anticipate for functions modifying the argument directly
    len = strlen(cmd);
    while (cmd_pos < len) {
        grab_until_sep(arg_str_ptr, cmd + cmd_pos, COMMAND_BUFFER_SIZE - cmd_pos, &len_read, NULL, NO_IGNORE_ESCAPE_CHAR, ' ');
        cmd_pos += len_read;

        if (len_read == 0) {
            cmd_pos += 1;
            continue;
        }
        argv[argc] = arg_str_ptr;
        argc++;
        arg_str_ptr[len_read] = 0;
        arg_str_ptr += len_read + 1;
    }

    need_cleanup = 0;

    if (func_interruptable) {
        SET_INTERRUPTABLE();
    }
    else {
        SET_NOT_INTERRUPTABLE();
    }

    SET_IN_FUNC();

    ret = func(info, dir, argc, argv);

    mark_dir_pointers_not_usable(dir);

    SET_NOT_IN_FUNC();

    // using SET_IN_FUNC() already implies not interruptable
    // in current version of interrupt handler
    // but mark it explicitly anyway after we are done with the function call
    SET_NOT_INTERRUPTABLE();

    // clean up loose resources if requested
    if (need_cleanup) {
        if (!cleaner) {
            printf("process_cmd : warning, cleanup requested but no cleaner function provided\n");
        }
        else {
            if ((ret_cleaner = cleaner())) {
                printf("process_cmd : warning, clean up for \"%s\" failed\n", func_name);
                printf("process_cmd : cleaner return code : %d\n", ret_cleaner);
            }
        }
    }

    return ret;
}

void inthandler(int sig) {
    if (!prompt_func_jmp_set) {
        return;
    }

    if (in_function) {
        if (!interruptable) {
            printf("Current operation cannot be interrupted\n");
        }
        else {
            longjmp(prompt_func, 4);
        }
    }
    else {
        printf("\nKeyboard interrupt received\n");
        printf("Current line cancelled, press enter to continue.");
        fflush(stdout);
        cancel_line = 1;
    }
}

int prompt (term_info* info, dir_info* dir) {    // should have size COMMAND_BUFFER_SIZE
    int i, ret = 0;

    //char buf[COMMAND_BUFFER_SIZE+1];

    if (!inthandler_set) {
        signal(SIGINT, inthandler);
        __sync_synchronize();
        inthandler_set = 1;
    }

    if (!l2_dirp_record_arr_set) {
        init_layer2_dirp_record_arr(&l2_dirp_record_arr);
        l2_dirp_record_arr_set = 1;
    }

    i = setjmp(prompt_func);
    if (i == 0) {
        prompt_func_jmp_set = 1;
    }
    else {
        return 0;
    }

    //printf(PROMPT_STRING);

    //i = 0;
    //while ((i < COMMAND_BUFFER_SIZE) && (c = getchar()) != EOF && c != '\n') {
    //    buf[i++] = c;
    //}
    //buf[i] = 0;

    if (cmd_buf) {      // if failed to free last time due to interrupt
        free(cmd_buf);
    }

    cmd_buf = readline(PROMPT_STRING);

    if (cancel_line) {
        cancel_line = 0;
        if (cmd_buf) {
            free(cmd_buf);
            cmd_buf = NULL;
        }
    }
    else {
        if (cmd_buf && cmd_buf[0] != 0) {   // line not empty
            add_history(cmd_buf);
        }

        if (cmd_buf) {
            ret = process_cmd(info, cmd_buf, dir);

            free(cmd_buf);
            cmd_buf = NULL;
        }
    }

    return ret;
}

int add_func (term_info* info, char* name, int (*handler) (term_info* info, dir_info* dir, int argc, char* argv[]), unsigned char func_interruptable, int (*cleaner) ()) {
    int i;
    int (*func) (term_info* info, dir_info* dir, int argc, char* argv[]);

    for (i = 0; i < FUNC_NAME_MAX+1; i++) {
        if (name[i] == 0) {
            break;
        }
    }
    if (i == FUNC_NAME_MAX) {
        printf("add_func : function name not null terminated");
        return WRONG_ARGS;
    }

    func_match(info, name, &func, NULL, NULL);
    if (func) {
        printf("add_func : function %s already added\n", name);
        return WRONG_ARGS;
    }

    for (i = 0; i < FUNC_NUM_MAX; i++) {
        if (info->func_arr[i].handler == 0) {
            break;
        }
    }

    if (i == FUNC_NUM_MAX) {
        printf("no space in func arr\n");
        return NO_SPACE_FUNC;
    }

    strcpy(info->func_arr[i].name, name);

    info->func_arr[i].handler = handler;

    info->func_arr[i].interruptable = func_interruptable;

    info->func_arr[i].cleaner = cleaner;

    return 0;
}

int init_func_arr (term_info* info) {
    int i;

    for (i = 0; i < FUNC_NUM_MAX; i++) {
        info->func_arr[i].handler = NULL;
    }

    add_func(info, "newdb",     &newdb,     NOT_INTERRUPTABLE,  NULL);
    add_func(info, "savedb",    &savedb,    NOT_INTERRUPTABLE,  NULL);
    add_func(info, "loaddb",    &loaddb,    NOT_INTERRUPTABLE,  NULL);
    add_func(info, "unloaddb",  &unloaddb,  NOT_INTERRUPTABLE,  NULL);

    add_func(info, "pwd",       &pwd,           INTERRUPTABLE,  NULL);
    add_func(info, "ls",        &ls,            INTERRUPTABLE,  NULL);
    add_func(info, "cd",        &cd,        NOT_INTERRUPTABLE,  NULL);

    add_func(info, "cp",        &cp,        NOT_INTERRUPTABLE,  NULL);
    add_func(info, "rm",        &rm,        NOT_INTERRUPTABLE,  NULL);
    add_func(info, "mv",        &mv,        NOT_INTERRUPTABLE,  NULL);

    add_func(info, "touch",     &touch,     NOT_INTERRUPTABLE,  NULL);
    add_func(info, "mkdir",     &ffp_mkdir, NOT_INTERRUPTABLE,  NULL);

    add_func(info, "show",      &show,          INTERRUPTABLE,  NULL);

    add_func(info, "edit",      &edit,      NOT_INTERRUPTABLE,  NULL);
    add_func(info, "verify",    &verify,        INTERRUPTABLE,  NULL);
    add_func(info, "attach",    &attach,    NOT_INTERRUPTABLE,  NULL);
    add_func(info, "detach",    &detach,    NOT_INTERRUPTABLE,  NULL);

    add_func(info, "exit",      &ffp_exit,  NOT_INTERRUPTABLE,  NULL);

    add_func(info, "fp",        &fp,            INTERRUPTABLE,  &fp_cleanup);

    add_func(info, "fpwd",      &fpwd,          INTERRUPTABLE,  NULL);
    add_func(info, "fls",       &fls,           INTERRUPTABLE,  &fls_cleanup);
    add_func(info, "fcd",       &fcd,       NOT_INTERRUPTABLE,  NULL);

    add_func(info, "help",      &help,          INTERRUPTABLE,  NULL);

    return 0;
}

int init_term_info (term_info* info) {
    int i, ret;

    map_block* map_temp_raw;

    const size_t map_size = 10000;
    const size_t map_alloc_size
        = sizeof(map_block) * get_bitmap_map_block_number(map_size);

    init_func_arr(info);
    info->alias = 0;

    map_temp_raw = malloc(map_alloc_size);
    if (!map_temp_raw) {
        return MALLOC_FAIL;
    }
    bitmap_init(&info->map_buf, map_temp_raw, 0, map_size, 0);

    map_temp_raw = malloc(map_alloc_size);
    if (!map_temp_raw) {
        return MALLOC_FAIL;
    }
    bitmap_init(&info->map_buf2, map_temp_raw, 0, map_size, 0);

    if ((ret = init_field_match_bitmap(&info->match_map))) {
        return ret;
    }

    if ((ret = init_layer2_sect_field_arr(&info->l2_sect_field_arr))) {
        return ret;
    }

    for (i = 0; i < SETTING_NUM; i++) {
        info->setting[i] = 0;
    }

    return 0;
}

int pwd(term_info* info, dir_info* dir, int argc, char* argv[]) {
    int i, ret;

    char path[PATH_LEN_MAX];

    database_handle* temp_dh_find;
    linked_entry* temp_entry_find;

    error_handle er_h;

    unsigned char mode = PDIR_MODE_FILENAME;    // default to using file name mode

    error_mark_owner(&er_h, "pwd");

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "eid") == 0) {
            mode = PDIR_MODE_EID;
        }
    }

    ret = dir_to_path(dir, path, mode, &er_h);
    switch (ret) {
        case 0:
            // no error
            break;
        default:
            error_print_owner_msg(&er_h);
            error_mark_inactive(&er_h);
            return ret;
    }

    if (is_pointing_to_root(dir)) { // no database
        printf("%s\n", path);
    }
    else {
        ret = update_dir_pointers(dir, &er_h);
        if (ret) {
            error_print_owner_msg(&er_h);
            error_mark_inactive(&er_h);
            return ret;
        }

        temp_dh_find = dir->dh;
        temp_entry_find = dir->entry;

        if (temp_entry_find->type == ENTRY_FILE) {
            printf("%s%.*s    id : %s    type : file\n", path, calc_pad(path, PRINT_PAD_SIZE), space_pad, dir->entry_id);
        }
        else if (temp_entry_find->type == ENTRY_GROUP) {
            printf("%s%.*s    id : %s    type : group\n", path, calc_pad(path, PRINT_PAD_SIZE), space_pad, dir->entry_id);
        }
    }

    return 0;
}

int ls(term_info* info, dir_info* dir, int argc, char* argv[]) {
    linked_entry* temp_entry_find;
    linked_entry* temp_entry;
    database_handle* temp_dh_find;

    unsigned char opt_flag[LS_OPT_NUM];

    int i, j, ret;

    int arg_count = 0;

    char* str;

    error_handle er_h;

    dir_info result_dir;

    database_handle* iter_dh, * iter_dh_temp;

    error_mark_inactive(&er_h);
    error_mark_owner(&er_h, "ls");

    for (i = 0; i < LS_OPT_NUM; i++) {
        opt_flag[i] = 0;
    }

    for (i = 0; i < argc; i++) {
        if (IS_CHAR_OPTION(argv[i])) {    // option
            for (j = 1; j < strlen(argv[i]); j++) {
                switch (argv[i][j]) {
                    case 'l' :
                        opt_flag[LS_OPT_l] = 1;
                        break;
                    case 'd' :
                        opt_flag[LS_OPT_d] = 1;
                        break;
                    default :
                        printf("ls : unknown option\n");
                        return NO_SUCH_OPT;
                }
            }
        }
        else if (IS_WORD_OPTION(argv[i])) {
            printf("ls : unknown option\n");
            return NO_SUCH_OPT;
        }
        else {
            arg_count++;
        }
    }

    if (arg_count == 0) {
        copy_dir(&result_dir, dir);

        ret = update_dir_pointers(&result_dir, &er_h);
        if (ret) {
            error_print_owner_msg(&er_h);
            error_mark_inactive(&er_h);
            return ret;
        }

        i = 0;
        argc = 0;
        goto print_result_dir;
    }

    for (i = 0; i < argc; i++) {
        if (!IS_OPTION(argv[i])) {  // locator
            ret = locator_to_dir(info, dir, argv[i], &result_dir, &er_h);
            switch (ret) {
                case 0:
                    // no error
                    break;
                default:
                    error_print_owner_msg(&er_h);
                    error_mark_inactive(&er_h);
                    return ret;
            }

print_result_dir:

            if (is_pointing_to_root(&result_dir)) {
                if (dir->root->db_num > 0) {
                    // list all databases
                    HASH_ITER(hh, dir->root->db, iter_dh, iter_dh_temp) {
                        printf("%s%.*s    type : database\n", iter_dh->name, calc_pad(iter_dh->name, PRINT_PAD_SIZE), space_pad);
                    }
                }
            }
            else {
                temp_dh_find = result_dir.dh;
                temp_entry_find = result_dir.entry;

                if (temp_entry_find->type == ENTRY_FILE) {
                    if (opt_flag[LS_OPT_d]) {
                        if (opt_flag[LS_OPT_l]) {
                            str = temp_entry_find->file_name;
                            if (        temp_entry_find->has_parent
                                    // make sure it's not tree root
                                    &&  temp_entry_find->parent->has_parent
                                    &&  temp_entry_find->parent->type == ENTRY_FILE
                               )
                            {
                                printf(
                                        "%s%.*s    id : %s    type : file (a version of parent file)\n",
                                        str,
                                        calc_pad(str, PRINT_PAD_SIZE),
                                        space_pad,
                                        temp_entry_find->entry_id_str
                                      );
                            }
                            else {
                                printf(
                                        "%s%.*s    id : %s    type : file\n",
                                        str,
                                        calc_pad(str, PRINT_PAD_SIZE),
                                        space_pad,
                                        temp_entry_find->entry_id_str
                                      );
                            }
                        }
                        else {
                            printf("%s\n", temp_entry_find->file_name);
                        }
                    }
                    else {
                        for (j = 0; j < temp_entry_find->child_num; j++) {
                            temp_entry = temp_entry_find->child[j];
                            if (opt_flag[LS_OPT_l]) {
                                str = temp_entry->file_name;
                                printf(
                                        "%s%.*s    id : %s    type : file (a version of parent file)\n",
                                        str,
                                        calc_pad(str, PRINT_PAD_SIZE),
                                        space_pad,
                                        temp_entry->entry_id_str
                                      );
                            }
                            else {
                                printf("%s\n", temp_entry->file_name);
                            }
                        }
                    }
                }
                else if (temp_entry_find->type == ENTRY_GROUP) {
                    if (opt_flag[LS_OPT_d]) {
                        if (opt_flag[LS_OPT_l]) {
                            str = temp_entry_find->file_name;
                            printf(
                                    "%s%.*s    id : %s    type : group\n",
                                    str,
                                    calc_pad(str, PRINT_PAD_SIZE),
                                    space_pad,
                                    temp_entry_find->entry_id_str
                                  );
                        }
                        else {
                            printf("%s\n", temp_entry_find->file_name);
                        }
                    }
                    else {
                        for (j = 0; j < temp_entry_find->child_num; j++) {
                            temp_entry = temp_entry_find->child[j];
                            if (opt_flag[LS_OPT_l]) {
                                str = temp_entry->file_name;
                                if (temp_entry->type == ENTRY_FILE) {
                                    printf(
                                            "%s%.*s    id : %s    type : file\n",
                                            str,
                                            calc_pad(str, PRINT_PAD_SIZE),
                                            space_pad,
                                            temp_entry->entry_id_str
                                          );
                                }
                                else if (temp_entry->type == ENTRY_GROUP) {
                                    printf(
                                            "%s%.*s    id : %s    type : group\n",
                                            str,
                                            calc_pad(str, PRINT_PAD_SIZE),
                                            space_pad,
                                            temp_entry->entry_id_str
                                          );
                                }
                            }
                            else {
                                printf("%s\n", temp_entry->file_name);
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}

int cd(term_info* info, dir_info* dir, int argc, char* argv[]) {
    #if CD_OPT_NUM > 0
    unsigned char opt_flag[CD_OPT_NUM];
    #endif

    int i, target_count;
    int j;

    int ret;

    dir_info result_dir;

    error_handle er_h;

    error_mark_inactive(&er_h);
    error_mark_owner(&er_h, "cd");

    #if CD_OPT_NUM > 0
    for (i = 0; i < CD_OPT_NUM; i++) {
      opt_flag[i] = 0;
    }
    #endif

    target_count = 0;
    for (i = 0; i < argc; i++) {
        if (IS_OPTION(argv[i])) {
            printf("cd : unknown option\n");
            return NO_SUCH_OPT;
        }
        else {
            j = i;
            target_count++;
            if (target_count > 1) {
                printf("cd : too many targets\n");
                return WRONG_ARGS;
            }
        }
    }

    if (target_count < 1) {
        printf("cd : too few targets\n");
        return WRONG_ARGS;
    }
    else if (target_count > 1) {
        printf("cd : too many targets\n");
        return WRONG_ARGS;
    }

    ret = locator_to_dir(info, dir, argv[j], &result_dir, &er_h);
    switch (ret) {
        case 0:
            copy_dir(dir, &result_dir);
            break;
        default:
            error_print_owner_msg(&er_h);
            error_mark_inactive(&er_h);
            return ret;
    }

    return 0;
}

int cp(term_info* info, dir_info* dir, int argc, char* argv[]) {
    database_handle* src_dh;
    database_handle* dst_dh;
    linked_entry* dst_entry;
    linked_entry* src_entry;

    unsigned char opt_flag[CP_OPT_NUM];

    int i, j;
    int dst_index;
    int target_count = 0;
    int source_count;
    int ret;

    dir_info result_dir;

    str_len_int str_len;

    error_handle er_h;

    error_mark_inactive(&er_h);
    error_mark_owner(&er_h, "cp");

    for (i = 0; i < CP_OPT_NUM; i++) {
        opt_flag[i] = 0;
    }

    for (i = 0; i < argc; i++) {
        if (IS_CHAR_OPTION(argv[i])) { // option
            str_len = strlen(argv[i]);
            for (j = 1; j < str_len; j++) {
                switch (argv[i][j]) {
                    case 'r' :
                        opt_flag[CP_OPT_r] = 1;
                        break;
                    default :
                        printf("cp : unknown option\n");
                        return NO_SUCH_OPT;
                }
            }
        }
        else if (IS_WORD_OPTION(argv[i])) {
            printf("cp : unknown option\n");
            return NO_SUCH_OPT;
        }
        else {
            // dst_index holds the index of last entry
            dst_index = i;
            target_count++;
        }
    }

    if (target_count < 2) {
        printf("cp : too few targets\n");
        return WRONG_ARGS;
    }

    source_count = target_count-1;

    // resolve destionation first
    ret = locator_to_dir(info, dir, argv[dst_index], &result_dir, &er_h);
    switch (ret) {
        case 0 :
            // do nothing
            break;
        case NO_SUCH_LOGIC_DIR:
            printf("cp : destination does not exist\n");
            return ret;
        case FOUND_DUPLICATE:
            printf("cp : ambiguous destination\n");
            return ret;
        default:
            error_print_owner_msg(&er_h);
            error_mark_inactive(&er_h);
            return ret;
    }

    if (is_pointing_to_root(&result_dir)) {
        printf("cp : cannot copy into root directory (reserved for database objects)\n");
        return WRONG_ARGS;
    }
    else {
        dst_dh = result_dir.dh;
        dst_entry = result_dir.entry;

        grow_children_array(dst_entry, source_count);
    }

    // go through source targets
    for (i = 0; i < argc; i++) {
        if (!IS_OPTION(argv[i])) {
            if (i == dst_index) {
                break;
            }

            ret = locator_to_dir(info, dir, argv[i], &result_dir, &er_h);
            switch (ret) {
                case 0 :
                    // do nothing
                    break;
                case NO_SUCH_LOGIC_DIR:
                    printf("cp : source \"%s\" does not exist\n", argv[i]);
                    continue;
                case FOUND_DUPLICATE:
                    printf("cp : ambiguous source \"%s\"\n", argv[i]);
                    continue;
                default:
                    error_print_owner_msg(&er_h);
                    error_mark_inactive(&er_h);
                    continue;
            }

            if (is_pointing_to_root(&result_dir)) {
                printf("cp : cannot copy root directory");
                continue;
            }

            src_dh = result_dir.dh;
            src_entry = result_dir.entry;

            if (        src_entry->type == ENTRY_GROUP
                    &&  dst_entry->type == ENTRY_FILE
               )
            {
                printf("cp : file type source \"%s\" cannot be added under group type destionation\n", argv[i]);
                continue;
            }

            copy_entry(dst_dh, dst_entry, src_entry, opt_flag[CP_OPT_r]);

            MARK_DB_UNSAVED(dst_dh);
        }
    }

    return 0;
}

int rm(term_info* info, dir_info* dir, int argc, char* argv[]) {
    database_handle* tar_dh;
    linked_entry* tar_entry;

    unsigned char opt_flag[RM_OPT_NUM];

    int i, j;
    int ret;
    int target_count = 0;

    str_len_int str_len;

    dir_info result_dir;

    error_handle er_h;

    error_mark_inactive(&er_h);
    error_mark_owner(&er_h, "rm");

    for (i = 0; i < RM_OPT_NUM; i++) {
        opt_flag[i] = 0;
    }

    for (i = 0; i < argc; i++) {
        if (IS_CHAR_OPTION(argv[i])) { // option
            str_len = strlen(argv[i]);
            for (j = 1; j < str_len; j++) {
                switch (argv[i][j]) {
                    case 'r' :
                        opt_flag[RM_OPT_r] = 1;
                        break;
                    default :
                        printf("rm : unknown option\n");
                        return NO_SUCH_OPT;
                }
            }
        }
        else if (IS_WORD_OPTION(argv[i])) {
            printf("rm : unknown option\n");
            return NO_SUCH_OPT;
        }
        else {
            target_count++;
        }
    }

    if (target_count < 1) {
        printf("rm : too few targets\n");
        return WRONG_ARGS;
    }

    // go through targets
    for (i = 0; i < argc; i++) {
        if (!IS_OPTION(argv[i])) {
            ret = locator_to_dir(info, dir, argv[i], &result_dir, &er_h);
            switch (ret) {
                case 0 :
                    // do nothing
                    break;
                case NO_SUCH_LOGIC_DIR:
                    printf("rm : target \"%s\" does not exist\n", argv[i]);
                    continue;
                case FOUND_DUPLICATE:
                    printf("rm : ambiguous target \"%s\"\n", argv[i]);
                    continue;
                default:
                    error_print_owner_msg(&er_h);
                    error_mark_inactive(&er_h);
                    continue;
            }

            if (is_pointing_to_root(&result_dir)) {
                printf("rm : cannot remove root directory\n");
                return WRONG_ARGS;
            }
            else if (is_pointing_to_db(&result_dir)) {
                printf("rm : cannot remove database : use unloaddb instead\n");
                return WRONG_ARGS;
            }
            else {
                tar_dh = result_dir.dh;
                tar_entry = result_dir.entry;

                if (opt_flag[RM_OPT_r]) {
                    del_entry(tar_dh, tar_entry);
                }
                else {
                    if (tar_entry->child_num > 0) {
                        printf("rm : cannot remove target : \"%s\" : target contains entries\n", argv[i]);
                        return WRONG_ARGS;
                    }
                    else {
                        del_entry(tar_dh, tar_entry);

                        MARK_DB_UNSAVED(tar_dh);
                    }
                }
            }
        }
    }

    return 0;
}

int mv(term_info* info, dir_info* dir, int argc, char* argv[]) {
    database_handle* src_dh;
    database_handle* dst_dh;
    linked_entry* dst_entry;
    linked_entry* src_entry;

    unsigned char opt_flag[MV_OPT_NUM];

    int i, j;
    int dst_index;
    int target_count = 0;
    int source_count;
    int ret;

    dir_info result_dir;

    str_len_int str_len;

    error_handle er_h;

    error_mark_inactive(&er_h);
    error_mark_owner(&er_h, "mv");

    for (i = 0; i < MV_OPT_NUM; i++) {
        opt_flag[i] = 0;
    }

    for (i = 0; i < argc; i++) {
        if (IS_CHAR_OPTION(argv[i])) { // option
            str_len = strlen(argv[i]);
            for (j = 1; j < str_len; j++) {
                switch (argv[i][j]) {
                    case 'r' :
                        opt_flag[MV_OPT_r] = 1;
                        break;
                    default :
                        printf("mv : unknown option\n");
                        return NO_SUCH_OPT;
                }
            }
        }
        else if (IS_WORD_OPTION(argv[i])) {
            printf("mv : unknown option\n");
            return NO_SUCH_OPT;
        }
        else {
            // dst_index holds the index of last entry
            dst_index = i;
            target_count++;
        }
    }

    if (target_count < 2) {
        printf("mv : too few targets\n");
        return WRONG_ARGS;
    }

    source_count = target_count-1;

    // resolve destionation first
    ret = locator_to_dir(info, dir, argv[dst_index], &result_dir, &er_h);
    switch (ret) {
        case 0 :
            // do nothing
            break;
        case NO_SUCH_LOGIC_DIR:
            printf("mv : destination does not exist\n");
            return ret;
        case FOUND_DUPLICATE:
            printf("mv : ambiguous destination path\n");
            return ret;
        default:
            error_print_owner_msg(&er_h);
            error_mark_inactive(&er_h);
            return ret;
    }

    if (is_pointing_to_root(&result_dir)) {
        printf("mv : cannot move into root directory (reserved for database objects)\n");
        return WRONG_ARGS;
    }
    else {
        dst_dh = result_dir.dh;
        dst_entry = result_dir.entry;

        grow_children_array(dst_entry, source_count);
    }

    // go through source targets
    for (i = 0; i < argc; i++) {
        if (!IS_OPTION(argv[i])) {
            if (i == dst_index) {
                break;
            }

            ret = locator_to_dir(info, dir, argv[i], &result_dir, &er_h);
            switch (ret) {
                case 0 :
                    // do nothing
                    break;
                case NO_SUCH_LOGIC_DIR:
                    printf("mv : source \"%s\" does not exist\n", argv[i]);
                    continue;
                case FOUND_DUPLICATE:
                    printf("mv : ambiguous source \"%s\"\n", argv[i]);
                    continue;
                default:
                    error_print_owner_msg(&er_h);
                    error_mark_inactive(&er_h);
                    continue;
            }

            if (is_pointing_to_root(&result_dir)) {
                printf("mv : cannot move root directory");
                continue;
            }

            src_dh = result_dir.dh;
            src_entry = result_dir.entry;

            if (        src_entry->type == ENTRY_GROUP
                    &&  dst_entry->type == ENTRY_FILE
               )
            {
                printf("mv : file type source \"%s\" cannot be added under group type destionation\n", argv[i]);
                continue;
            }

            if (opt_flag[MV_OPT_r]) {
                copy_entry(dst_dh, dst_entry, src_entry, opt_flag[MV_OPT_r]);

                MARK_DB_UNSAVED(dst_dh);

                del_entry(src_dh, src_entry);

                MARK_DB_UNSAVED(src_dh);
            }
            else {
                if (src_entry->child_num > 0) {
                    printf("mv : cannot move target : \"%s\" : target contains entries\n", argv[i]);
                    return WRONG_ARGS;
                }
                else {
                    copy_entry(dst_dh, dst_entry, src_entry, opt_flag[MV_OPT_r]);

                    MARK_DB_UNSAVED(dst_dh);

                    del_entry(src_dh, src_entry);

                    MARK_DB_UNSAVED(src_dh);
                }
            }
        }
    }

    return 0;
}

int newdb(term_info* info, dir_info* dir, int argc, char* argv[]) {
    int arg_count = 0;

    char* name;

    str_len_int name_len;
    str_len_int extension_len;

    int i;

    int ret;

    error_handle er_h;

    root_dir* root;

    root = dir->root;

    error_mark_owner(&er_h, "newdb");
    error_mark_inactive(&er_h);

    for (i = 0; i < argc; i++) {
        if (IS_CHAR_OPTION(argv[i])) {
            ;;  // do nothing
        }
        else if (IS_WORD_OPTION(argv[i])) {
            ;;  // do nothing
        }
        else {
            if (arg_count == 0) {
                name = argv[i];
            }
            else {
                printf("newdb : too many arguments\n");
                return WRONG_ARGS;
            }
            arg_count++;
        }
    }

    if (arg_count == 0) {
        printf("newdb : too few arguments\n");
        return WRONG_ARGS;
    }

    name_len = strlen(name);
    extension_len = strlen(DB_FILE_EXTENSION);

    if (name_len > FILE_NAME_MAX - extension_len) {
        printf("newdb : name too long\n");
        return WRONG_ARGS;
    }

    // if name contains .ffp suffix, remove it
    if (        name_len >= extension_len
            &&  strcmp(name + name_len - extension_len, DB_FILE_EXTENSION) == 0
       )
    {
        printf("newdb : warning, name contains %s suffix, suffix will be removed\n", DB_FILE_EXTENSION);
        name[name_len - extension_len] = 0;
    }

    if (strlen(name) == 0) {    // becomes empty after removing suffix
        printf("newdb : name is empty\n");
        return WRONG_ARGS;
    }

    ret = add_dh_to_root_dir(root, name, NULL, &er_h);
    if (ret) {
        error_print_owner_msg(&er_h);
        error_mark_inactive(&er_h);
        return ret;
    }

    return 0;
}

int savedb(term_info* info, dir_info* dir, int argc, char* argv[]) {
    database_handle* temp_dh_find;

    char out_file_name[FILE_NAME_MAX+1];

    str_len_int name_len;
    str_len_int extension_len;

    if (argc < 1) {
        printf("savedb : too few arguments\n");
        return WRONG_ARGS;
    }
    else if (argc > 2) {
        printf("savedb : too many arguments\n");
        return WRONG_ARGS;
    }

    lookup_db_name(dir->root->db, argv[0], &temp_dh_find);
    if (!temp_dh_find) {
        printf("savedb : no such database\n");
        return WRONG_ARGS;
    }

    if (argc == 1) {
        strcpy(out_file_name, argv[0]);
    }
    else if (argc == 2) {
        strcpy(out_file_name, argv[1]);
    }

    // add suffix .ffp to file name if it doesn't have one
    name_len = strlen(out_file_name);
    extension_len = strlen(DB_FILE_EXTENSION);

    if (        name_len < extension_len
            // below addition and subtraction is safe due to short circuiting
            // i.e. if below line is reached, then name_len >= extension_len
            ||  strcmp(out_file_name + name_len - extension_len, DB_FILE_EXTENSION) != 0
       )
    {
        if (name_len + extension_len > FILE_NAME_MAX) {
            printf("savedb : file name too long\n");
            return WRONG_ARGS;
        }

        strcpy(out_file_name + name_len, DB_FILE_EXTENSION);
    }

    save_file(temp_dh_find, out_file_name);

    MARK_DB_SAVED(temp_dh_find);

    return 0;
}

int savealldb(term_info* info, dir_info* dir, int argc, char* argv[]) {

    return 0;
}

int loaddb(term_info* info, dir_info* dir, int argc, char* argv[]) {
    int ret;

    str_len_int file_name_len;

    str_len_int extension_len;

    char file_name[FILE_NAME_MAX+1];

    char db_name[FILE_NAME_MAX+1];

    char db_name_len;

    database_handle* dh;

    error_handle er_h;

    error_mark_owner(&er_h, "loaddb");
    error_mark_inactive(&er_h);

    if (argc < 1) {
        printf("loaddb : too few arguments\n");
        return WRONG_ARGS;
    }
    else if (argc > 1) {
        printf("loaddb : too many arguments\n");
        return WRONG_ARGS;
    }

    // make sure name is not too long
    if ((ret = verify_str_terminated(argv[0], FILE_NAME_MAX, &file_name_len, 0x0))) {
        printf("loaddb : file name too long\n");
        return WRONG_ARGS;
    }

    db_name_len = file_name_len;

    strcpy(db_name, argv[0]);
    strcpy(file_name, argv[0]);

    extension_len = strlen(DB_FILE_EXTENSION);

    // for file name, add suffix if there isn't one yet
    if (        file_name_len < extension_len
            // below addition and subtraction is safe due to short circuiting
            // i.e. if below line is reached, then name_len >= extension_len
            ||  strcmp(file_name + file_name_len - extension_len, DB_FILE_EXTENSION) != 0
       )
    {
        if (file_name_len + extension_len > FILE_NAME_MAX) {
            printf("loaddb : file name too long\n");
            return WRONG_ARGS;
        }

        strcpy(file_name + file_name_len, DB_FILE_EXTENSION);
    }

    // for database name, delete suffix if there is one
    if (        db_name_len >= extension_len
            &&  strcmp(db_name + db_name_len - extension_len, DB_FILE_EXTENSION) == 0
       )
    {
        db_name[db_name_len - extension_len] = 0;
    }

    db_name_len = strlen(db_name);
    if (db_name_len == 0) {
        printf("loaddb : database name is empty\n");
        return WRONG_ARGS;
    }

    ret = add_dh_to_root_dir(dir->root, db_name, &dh, &er_h);
    if (ret) {
        error_print_owner_msg(&er_h);
        error_mark_inactive(&er_h);
        return ret;
    }

    ret = load_file(dh, file_name);
    if (ret) {
        del_dh_from_root_dir(dir->root, argv[0], &er_h);
        return ret;
    }

    return 0;
}

int unloaddb(term_info* info, dir_info* dir, int argc, char* argv[]) {
    char name[FILE_NAME_MAX+1];

    error_handle er_h;

    dir_info result_dir;

    const int buf_size = 10;
    char buf[buf_size+1];

    int i, c;
    int ret;

    error_mark_owner(&er_h, "unloaddb");
    error_mark_inactive(&er_h);

    if (argc < 1) {
        printf("unloaddb : too few arguments\n");
        return WRONG_ARGS;
    }
    else if (argc > 1) {
        printf("unloaddb : too many arguments\n");
        return WRONG_ARGS;
    }

    // parse locator
    ret = locator_to_dir(info, dir, argv[0], &result_dir, &er_h);
    switch (ret) {
        case 0:
            break;
        default:
            error_print_owner_msg(&er_h);
            error_mark_inactive(&er_h);
            return ret;
    }

    if (!is_pointing_to_db(&result_dir)) {
        printf("unloaddb : locator does not point to a database\n");
        return WRONG_ARGS;
    }

    if (result_dir.dh->unsaved) {
        printf("The database %s has unsaved changes\n", result_dir.dh->name);
        printf("Type unload again to confirm and force unload : ");

        i = 0;
        while ((i < buf_size) && (c = getchar()) != EOF && c != '\n') {
            buf[i++] = c;
        }
        buf[i] = 0;

        if (c != '\n') {
            discard_input_buffer();
        }

        if (strcmp(buf, "unload") != 0) {
            printf("unloading cancelled\n");
            return 0;
        }
    }

    strcpy(name, result_dir.dh->name);

    ret = del_dh_from_root_dir(dir->root, result_dir.dh->name, &er_h);
    if (ret) {
        error_print_owner_msg(&er_h);
        error_mark_inactive(&er_h);
        return ret;
    }

    printf("database clean up finished : %s\n", name);

    return 0;
}

// core function for touch and ffp_mkdir
static int make_entry(term_info* info, dir_info* dir, int argc, char* argv[], unsigned char mode) {
    database_handle* tar_dh;
    linked_entry* tar_entry;

    linked_entry* temp_entry;
    linked_entry* temp_entry_find;

    dir_info tar_dir;

    int ret;

    int i;

    str_len_int str_len;

    char* str, * file_name;

    int number_of_tries = 0;

    time_t raw_time;
    struct tm* time_info;

    unsigned char eid[EID_LEN];
    char eid_str[EID_STR_MAX+1];

    char name[20];

    error_handle er_h;

    error_mark_inactive(&er_h);

    if (        mode != MAKE_MODE_TOUCH
            &&  mode != MAKE_MODE_MKDIR
       )
    {
        printf("make_entry : invalid mode\n");
        return WRONG_ARGS;
    }

    if (mode == MAKE_MODE_TOUCH) {
        error_mark_owner(&er_h, "touch");
    }
    else if (mode == MAKE_MODE_MKDIR) {
        error_mark_owner(&er_h, "mkdir");
    }

    if (mode == MAKE_MODE_TOUCH) {
        strcpy(name, "touch");
    }
    else if (mode == MAKE_MODE_MKDIR) {
        strcpy(name, "mkdir");
    }

    if (argc < 1) {
        printf("%s : too few arguments\n", name);
        return WRONG_ARGS;
    }
    else if (argc > 1) {
        printf("%s : too many arguments\n", name);
        return WRONG_ARGS;
    }

    // determine target directory
    str_len = strlen(argv[0]);
    str = argv[0];
    for (i = 0; i < str_len; i++) {
        if (str[i] == '/' && (i > 0 ? str[i-1] != '\\' : 1)) {
            break;
        }
    }
    if (i == str_len) {     // no slashes found, default to current directory
        copy_dir(&tar_dir, dir);
        file_name = argv[0];
    }
    else {  // found slashes, resolve path up till the last possible level
        for (i = str_len - 1; i >= 0; i--) {
            if (str[i] == '/' && (i > 0 ? str[i-1] != '\\' : 1)) {
                str[i] = 0;
                file_name = str + i + 1;
                break;
            }

            if (i == 0) {   // avoid underflow and wrap around in case i is unsigned
                break;
            }
        }
        if (strlen(file_name) == 0) {
            printf("%s : target name is empty\n", name);
            return WRONG_ARGS;
        }

        ret = locator_to_dir(info, dir, str, &tar_dir, &er_h);
        switch (ret) {
            case 0 :
                // no error
                break;
            default :
                error_print_owner_msg(&er_h);
                error_mark_inactive(&er_h);
                return ret;
        }
    }

    if (strlen(file_name) > FILE_NAME_MAX) {
        printf("%s : target name too long\n", name);
        return WRONG_ARGS;
    }

    if (is_pointing_to_root(&tar_dir)) {
        printf("%s : target directory outside of a database\n", name);
        return WRONG_ARGS;
    }

    tar_dh = tar_dir.dh;
    tar_entry = tar_dir.entry;

    if (mode == MAKE_MODE_MKDIR) {
        // enforce that group type entry cannot be created under file type entry
        if (tar_entry->type == ENTRY_FILE) {
            printf("%s : cannot create group type entry under file type entry\n", name);
            return WRONG_ARGS;
        }
    }

    // generate entry id
    do {
        gen_entry_id(eid, eid_str);
        lookup_entry_id_via_dh(tar_dh, eid_str, &temp_entry_find);
        number_of_tries++;
    } while (temp_entry_find && number_of_tries < ID_GEN_MAX_TRIES);

    if (number_of_tries == ID_GEN_MAX_TRIES) {
        printf("%s : failed to generate unique entry id\n", name);
        return GEN_ID_FAIL;
    }

    // grab space for entry
    ret = add_entry_to_layer2_arr(&tar_dh->l2_entry_arr, &temp_entry, NULL);
    if (ret) {
        printf("%s : failed to get space for entry\n", name);
        return ret;
    }

    // set type
    if (mode == MAKE_MODE_TOUCH) {
        temp_entry->type = ENTRY_FILE;
    }
    else if (mode == MAKE_MODE_MKDIR) {
        temp_entry->type = ENTRY_GROUP;
    }

    // set created by flag
    temp_entry->created_by = CREATED_BY_USR;

    // set depth
    temp_entry->depth = tar_entry->depth + 1;

    // fill in entry id
    for (i = 0; i < EID_LEN; i++) {
        temp_entry->entry_id[i] = eid[i];
    }
    strcpy(temp_entry->entry_id_str, eid_str);

    // fill in file name and length
    strcpy(temp_entry->file_name, file_name);
    temp_entry->file_name_len = strlen(file_name);

    // put into parent as a new child
    put_into_children_array(tar_entry, temp_entry);

    // set has parent flag
    if (tar_entry == &tar_dh->tree) {   // head of branch
        temp_entry->has_parent = 0;
    }
    else {
        temp_entry->has_parent = 1;
    }

    // fill in branch id
    if (temp_entry->has_parent) {
        // copy branch id from parent
        for (i = 0; i < EID_LEN; i++) {
            temp_entry->branch_id[i] = temp_entry->parent->branch_id[i];
        }
        strcpy(temp_entry->branch_id_str, temp_entry->parent->branch_id_str);
    }
    else {
        // fill in branch id using entry id
        for (i = 0; i < EID_LEN; i++) {
            temp_entry->branch_id[i] = temp_entry->entry_id[i];
        }
        strcpy(temp_entry->branch_id_str, temp_entry->entry_id_str);
    }

    link_entry_to_file_name_structures(tar_dh, temp_entry);

    link_entry_to_entry_id_structures(tar_dh, temp_entry);

    // get current time
    time(&raw_time);
    time_info = gmtime(&raw_time);

    // fill in time of addition
    memcpy(
            &temp_entry->tod_utc,
            time_info,

            sizeof(struct tm)
          );

    link_entry_to_date_time_tree(tar_dh, temp_entry, DATE_TOD);

    // fill in time of modification
    memcpy(
            &temp_entry->tom_utc,
            time_info,

            sizeof(struct tm)
          );

    link_entry_to_date_time_tree(tar_dh, temp_entry, DATE_TOM);

    MARK_DB_UNSAVED(tar_dh);

    ret = verify_entry(tar_dh, temp_entry, 0x0);
    if (ret) {
        printf("%s : verify entry detected errors\n", name);
        printf("Please report to developer as this should not happen\n");
        printf("It is recommended that you revert to previous version of database as this may indicate your database is now corrupted\n");
        printf("Sorry for the inconvenience\n");
        return ret;
    }

    return 0;
}

int touch(term_info* info, dir_info* dir, int argc, char* argv[]) {
    return make_entry(info, dir, argc, argv, MAKE_MODE_TOUCH);
}

int ffp_mkdir(term_info* info, dir_info* dir, int argc, char* argv[]) {
    return make_entry(info, dir, argc, argv, MAKE_MODE_MKDIR);
}

int edit(term_info* info, dir_info* dir, int argc, char* argv[]) {
    int ret;

    const char time_format[] = "%Y-%m-%d_%H:%M:%S";

    int i;

    int mode;
    int modify_flag = MODIFY_WRITE_MODE;

    char* key;
    char* val;
    unsigned char has_arrow;

    char* num;
    uint64_t sect_num;

    str_len_int str_len;

    str_len_int tag_num, tag_num2;
    str_len_int tag_min_len, tag_max_len;

    char* str;

    int cur_index;
    int keyvalpair_start_index;

    dir_info result_dir;

    database_handle* tar_dh;
    linked_entry* tar_entry;
    file_data* tar_file_data;
    section* tar_section;

    time_t raw_time_local;
    struct tm local_time;
    struct tm* utc_time_p;

    time_t raw_time;
    struct tm* time_info;

    error_handle er_h;

    struct tm temp_time;

    unsigned char e_field_specified[EDIT_E_NUM];
    unsigned char f_field_specified[EDIT_F_NUM];
    unsigned char s_field_specified[EDIT_S_NUM];

    uint64_t temp_size;
    uint64_t temp_pos;

    error_mark_owner(&er_h, "edit");

    mem_wipe_sec(e_field_specified, sizeof(e_field_specified));
    mem_wipe_sec(f_field_specified, sizeof(f_field_specified));
    mem_wipe_sec(s_field_specified, sizeof(s_field_specified));

    if (argc < 3) {
        printf("edit : too few arguments\n");
        return WRONG_ARGS;
    }

    // parse locator
    str = argv[0];
    ret = locator_to_dir(info, dir, str, &result_dir, &er_h);
    switch (ret) {
        case 0:
            break;
        default:
            error_print_owner_msg(&er_h);
            error_mark_inactive(&er_h);
            return ret;
    }

    if (is_pointing_to_root(&result_dir)) {
        printf("edit : cannot modify root\n");
        return WRONG_ARGS;
    }
    else if (is_pointing_to_db(&result_dir)) {
        printf("edit : cannot modify database\n");
        return WRONG_ARGS;
    }

    tar_dh = result_dir.dh;
    tar_entry = result_dir.entry;

    cur_index = 1;

    // parse mode
    str = argv[cur_index];
    if (        strcmp(str, "entry")    == 0) {
        mode = UPDATE_ENTRY_MODE;

        cur_index++;
    }
    else if (   strcmp(str, "file")     == 0) {
        if (!tar_entry->data) {
            printf("edit : entry does not have file data attached, see help attach\n");
            return WRONG_ARGS;
        }
        mode = UPDATE_FILE_MODE;

        tar_file_data = tar_entry->data;

        cur_index++;
    }
    else if (   strcmp(str, "sect")     == 0) {
        if (!tar_entry->data) {
            printf("edit : entry does not have file data attached, see help attach\n");
            return WRONG_ARGS;
        }
        mode = UPDATE_SECT_MODE;

        tar_file_data = tar_entry->data;

        if (argc < 4) {
            printf("edit : too few arguments\n");
            return WRONG_ARGS;
        }

        cur_index++;

        num = argv[cur_index];
        if (sscanf(num, "%"PRIu64"", &sect_num) != 1) {
            printf("edit : invalid section number\n");
            return WRONG_ARGS;
        }

        if (sect_num + 1 > tar_file_data->section_num) {
            printf("edit : section number out of range\n");
            return WRONG_ARGS;
        }

        tar_section = tar_file_data->section[sect_num];

        cur_index++;
    }
    else {
        printf("edit : unknown mode\n");
        return WRONG_ARGS;
    }

    // parse modify flag if any
    str = argv[cur_index];
    if (strlen(str) == 1) {
        if (str[0] == 'w') {
            modify_flag = MODIFY_WRITE_MODE;
        }
        else if (str[0] == 'a') {
            modify_flag = MODIFY_APPEND_MODE;
        }
        else {
            printf("edit : unknown modify flag\n");
            return WRONG_ARGS;
        }

        if (argc < 4) {
            printf("edit : missing keyvalpairs\n");
            return WRONG_ARGS;
        }

        cur_index++;
    }

    keyvalpair_start_index = cur_index;

    // verify all key value pairs
    for (i = keyvalpair_start_index; i < argc; i++) {
        str = argv[i];

        ret = parse_keyval_pair(str, &has_arrow, &key, &val);
        if (ret) {      // format error
            if (!has_arrow) {
                printf("edit : arrow missing in \"%s\"\n", str);
                return WRONG_ARGS;
            }
            else if (key == NULL && val == NULL) {
                printf("edit : both key and value are missing in \"%s\"\n", str);
                return WRONG_ARGS;
            }
            else if (key == NULL) {
                printf("edit : key is missing in \"%s\"\n", str);
                return WRONG_ARGS;
            }
            else if (val == NULL) {
                printf("edit : val is missing in \"%s\"\n", str);
                return WRONG_ARGS;
            }
        }

        // check if keys exist and if values are valid
        switch (mode) {
            case UPDATE_ENTRY_MODE :
                if (        strcmp(key, "name")     == 0) {
                    if (modify_flag == MODIFY_APPEND_MODE) {
                        printf("edit : append error : cannot append name\n");
                        return WRONG_ARGS;
                    }

                    str_len = strlen(val);
                    if (str_len == 0) {
                        printf("edit : name cannot be empty\n");
                        return WRONG_ARGS;
                    }
                    if (str_len > FILE_NAME_MAX) {
                        printf("edit : name too long\n");
                        return WRONG_ARGS;
                    }

                    if (e_field_specified[EDIT_E_NAME]) {
                        printf("edit : name already specified\n");
                        return WRONG_ARGS;
                    }
                    e_field_specified[EDIT_E_NAME] = 1;
                }
                else if (   strcmp(key, "timeaddutc")   == 0
                        ||  strcmp(key, "timeaddloc")   == 0
                        )
                {
                    if (modify_flag == MODIFY_APPEND_MODE) {
                        if (tar_entry->tod_utc_used) {
                            printf("edit : append error : time of addition already recorded\n");
                            return WRONG_ARGS;
                        }
                    }

                    if (strptime(val, time_format, &temp_time) == NULL) {
                        printf("edit : invalid date time format : %s\n", val);
                        return WRONG_ARGS;
                    }

                    if (e_field_specified[EDIT_E_TADD]) {
                        printf("edit : time of addition already specified\n");
                        return WRONG_ARGS;
                    }
                    e_field_specified[EDIT_E_TADD] = 1;
                }
                else if (   strcmp(key, "timemodutc")   == 0
                        ||  strcmp(key, "timemodloc")   == 0
                        )
                {
                    if (modify_flag == MODIFY_APPEND_MODE) {
                        if (tar_entry->tom_utc_used) {
                            printf("edit : append error : time of modification already recorded\n");
                            return WRONG_ARGS;
                        }
                    }

                    if (strptime(val, time_format, &temp_time) == NULL) {
                        printf("edit : invalid date time format : %s\n", val);
                        return WRONG_ARGS;
                    }

                    if (e_field_specified[EDIT_E_TMOD]) {
                        printf("edit : time of modification already specified\n");
                        return WRONG_ARGS;
                    }
                    e_field_specified[EDIT_E_TMOD] = 1;
                }
                else if (   strcmp(key, "timeusrutc")   == 0
                        ||  strcmp(key, "timeusrloc")   == 0
                        )
                {
                    if (modify_flag == MODIFY_APPEND_MODE) {
                        if (tar_entry->tusr_utc_used) {
                            printf("edit : append error : user specified time already recorded\n");
                            return WRONG_ARGS;
                        }
                    }

                    if (strptime(val, time_format, &temp_time) == NULL) {
                        printf("edit : invalid date time format : %s\n", val);
                        return WRONG_ARGS;
                    }

                    if (e_field_specified[EDIT_E_TUSR]) {
                        printf("edit : user specified time already specified\n");
                        return WRONG_ARGS;
                    }
                    e_field_specified[EDIT_E_TUSR] = 1;
                }
                else if (   strcmp(key, "type")     == 0) {
                    if (modify_flag == MODIFY_APPEND_MODE) {
                        printf("edit : append error : cannot append type\n");
                        return WRONG_ARGS;
                    }

                    if (        strcmp(val, "file")     != 0
                            &&  strcmp(val, "group")    != 0
                       )
                    {
                        printf("edit : unknown type : %s\n", val);
                        return WRONG_ARGS;
                    }

                    if (e_field_specified[EDIT_E_TYPE]) {
                        printf("edit : type already specified\n");
                        return WRONG_ARGS;
                    }
                    e_field_specified[EDIT_E_TYPE] = 1;
                }
                else if (   strcmp(key, "madeby")   == 0) {
                    if (modify_flag == MODIFY_APPEND_MODE) {
                        printf("edit : append error : cannot append made by flag\n");
                        return WRONG_ARGS;
                    }
                    if (        strcmp(val, "user")  != 0
                            &&  strcmp(val, "system")  != 0
                       )
                    {
                        printf("edit : unknown made by flag : %s\n", val);
                        return WRONG_ARGS;
                    }

                    if (e_field_specified[EDIT_E_MADEBY]) {
                        printf("edit : madeby flag already specified\n");
                        return WRONG_ARGS;
                    }
                    e_field_specified[EDIT_E_MADEBY] = 1;
                }
                else if (   strcmp(key, "tag")      == 0) {
                    str_len = strlen(val);
                    if (str_len <= 2) {
                        printf("edit : tag string too short\n");
                        return WRONG_ARGS;
                    }
                    if (    str_len > 0
                            && 
                            (   val[0] != '|'
                                ||
                                val[str_len-1] != '|'
                                ||
                                (       val[str_len-1] == '|'
                                    &&  val[str_len-2] == '\\'
                                )
                            )
                       )
                    {
                        printf("edit : tag string does not have proper format\n");
                        return WRONG_ARGS;
                    }

                    tag_count(val, &tag_num, &tag_min_len, &tag_max_len);
                    if (modify_flag == MODIFY_WRITE_MODE) {
                        if (tag_num > TAG_MAX_NUM) {
                            printf("edit : too many tags\n");
                            return WRONG_ARGS;
                        }
                    }
                    else if (modify_flag == MODIFY_APPEND_MODE) {
                        tag_count(tar_entry->tag_str, &tag_num2, NULL, NULL); // count existing tags
                        if (tag_num + tag_num2 > TAG_MAX_NUM) {
                            printf("edit : too many tags\n");
                            return WRONG_ARGS;
                        }
                    }
                    if (tag_max_len > TAG_LEN_MAX) {
                        printf("edit : one of the tags is too long\n");
                        return WRONG_ARGS;
                    }
                    if (tag_min_len == 0) {
                        printf("edit : one of the tags is empty\n");
                    }

                    if (e_field_specified[EDIT_E_TAG]) {
                        printf("edit : tag string already specified\n");
                        return WRONG_ARGS;
                    }
                    e_field_specified[EDIT_E_TAG] = 1;
                }
                else if (   strcmp(key, "msg")      == 0) {
                    str_len = strlen(val);
                    if (modify_flag == MODIFY_WRITE_MODE) {
                        if (str_len > USER_MSG_MAX) {
                            printf("edit : user message too long\n");
                            return WRONG_ARGS;
                        }
                    }
                    else if (modify_flag == MODIFY_APPEND_MODE) {
                        if (str_len + strlen(tar_entry->user_msg) > USER_MSG_MAX) {
                            printf("edit : user message too long\n");
                            return WRONG_ARGS;
                        }
                    }

                    if (e_field_specified[EDIT_E_MSG]) {
                        printf("edit : user message already specified\n");
                        return WRONG_ARGS;
                    }
                    e_field_specified[EDIT_E_MSG] = 1;
                }
                else {
                    printf("edit : unknown key : %s\n", key);
                    return WRONG_ARGS;
                }
                break;
            case UPDATE_FILE_MODE :
                if (        strcmp(key, "size")     == 0) {
                    if (modify_flag == MODIFY_APPEND_MODE) {
                        printf("edit : append error : cannot append file size\n");
                        return WRONG_ARGS;
                    }

                    if (sscanf(val, "%"PRIu64"", &temp_size) != 1) {
                        printf("edit : invalid size\n");
                        return WRONG_ARGS;
                    }

                    if (f_field_specified[EDIT_F_SIZE]) {
                        printf("edit : file size already specified\n");
                        return WRONG_ARGS;
                    }
                    f_field_specified[EDIT_F_SIZE] = 1;
                }
                else if (   strcmp(key, "sha1")     == 0) {
                    if (modify_flag == MODIFY_APPEND_MODE) {
                        if (        tar_file_data->checksum[CHECKSUM_SHA1_INDEX].type
                                !=  CHECKSUM_UNUSED
                           )
                        {
                            printf("edit : append error : file sha1 checksum already recorded\n");
                            return WRONG_ARGS;
                        }
                    }

                    str_len = strlen(val);
                    if (str_len < 2 * SHA_DIGEST_LENGTH) {
                        printf("edit : file sha1 checksum too short\n");
                        return WRONG_ARGS;
                    }
                    else if (str_len > 2 * SHA_DIGEST_LENGTH) {
                        printf("edit : file sha1 checksum too long\n");
                        return WRONG_ARGS;
                    }

                    if (f_field_specified[EDIT_F_SHA1]) {
                        printf("edit : file sha1 checksum already specified\n");
                        return WRONG_ARGS;
                    }
                    f_field_specified[EDIT_F_SHA1] = 1;
                }
                else if (   strcmp(key, "sha256")   == 0) {
                    if (modify_flag == MODIFY_APPEND_MODE) {
                        if (        tar_file_data->checksum[CHECKSUM_SHA256_INDEX].type
                                !=  CHECKSUM_UNUSED
                           )
                        {
                            printf("edit : append error : file sha256 checksum already recorded\n");
                            return WRONG_ARGS;
                        }
                    }

                    str_len = strlen(val);
                    if (str_len < 2 * SHA256_DIGEST_LENGTH) {
                        printf("edit : file sha256 checksum too short\n");
                        return WRONG_ARGS;
                    }
                    else if (str_len > 2 * SHA256_DIGEST_LENGTH) {
                        printf("edit : file sha256 checksum too long\n");
                        return WRONG_ARGS;
                    }

                    if (f_field_specified[EDIT_F_SHA256]) {
                        printf("edit : file sha256 checksum already specified\n");
                        return WRONG_ARGS;
                    }
                    f_field_specified[EDIT_F_SHA256] = 1;
                }
                else if (   strcmp(key, "sha512")   == 0) {
                    if (modify_flag == MODIFY_APPEND_MODE) {
                        if (        tar_file_data->checksum[CHECKSUM_SHA512_INDEX].type
                                !=  CHECKSUM_UNUSED
                           )
                        {
                            printf("edit : append error : file sha512 checksum already recorded\n");
                            return WRONG_ARGS;
                        }
                    }

                    str_len = strlen(val);
                    if (str_len < 2 * SHA512_DIGEST_LENGTH) {
                        printf("edit : file sha512 checksum too short\n");
                        return WRONG_ARGS;
                    }
                    else if (str_len > 2 * SHA512_DIGEST_LENGTH) {
                        printf("edit : file sha512 checksum too long\n");
                        return WRONG_ARGS;
                    }

                    if (f_field_specified[EDIT_F_SHA512]) {
                        printf("edit : file sha512 checksum already specified\n");
                        return WRONG_ARGS;
                    }
                    f_field_specified[EDIT_F_SHA512] = 1;
                }
                else {
                    printf("edit : unknown key : %s\n", key);
                    return WRONG_ARGS;
                }
                break;
            case UPDATE_SECT_MODE :
                if (        strcmp(key, "startpos") == 0) {
                    if (modify_flag == MODIFY_APPEND_MODE) {
                        printf("edit : append error : cannot append section start position\n");
                        return WRONG_ARGS;
                    }

                    if (sscanf(val, "%"PRIu64"", &temp_pos) != 1) {
                        printf("edit : invalid start position\n");
                        return WRONG_ARGS;
                    }

                    if (s_field_specified[EDIT_S_STARTPOS]) {
                        printf("edit : section start position already specified\n");
                        return WRONG_ARGS;
                    }
                    s_field_specified[EDIT_S_STARTPOS] = 1;
                }
                else if (   strcmp(key, "endpos")   == 0) {
                    if (modify_flag == MODIFY_APPEND_MODE) {
                        printf("edit : append error : cannot append section end position\n");
                        return WRONG_ARGS;
                    }

                    if (sscanf(val, "%"PRIu64"", &temp_pos) != 1) {
                        printf("edit : invalid end position\n");
                        return WRONG_ARGS;
                    }

                    if (s_field_specified[EDIT_S_ENDPOS]) {
                        printf("edit : section end position already specified\n");
                        return WRONG_ARGS;
                    }
                    s_field_specified[EDIT_S_ENDPOS] = 1;
                }
                else if (   strcmp(key, "sha1")     == 0) {
                    if (modify_flag == MODIFY_APPEND_MODE) {
                        if (        tar_section->checksum[CHECKSUM_SHA1_INDEX].type
                                !=  CHECKSUM_UNUSED
                           )
                        {
                            printf("edit : append error : section sha1 checksum already recorded\n");
                            return WRONG_ARGS;
                        }
                    }

                    str_len = strlen(val);
                    if (str_len < 2 * SHA_DIGEST_LENGTH) {
                        printf("edit : section sha1 checksum too short\n");
                        return WRONG_ARGS;
                    }
                    else if (str_len > 2 * SHA_DIGEST_LENGTH) {
                        printf("edit : section sha1 checksum too long\n");
                        return WRONG_ARGS;
                    }

                    if (s_field_specified[EDIT_S_SHA1]) {
                        printf("edit : section sha1 checksum already specified\n");
                        return WRONG_ARGS;
                    }
                    s_field_specified[EDIT_S_SHA1] = 1;
                }
                else if (   strcmp(key, "sha256")   == 0) {
                    if (modify_flag == MODIFY_APPEND_MODE) {
                        if (        tar_section->checksum[CHECKSUM_SHA256_INDEX].type
                                !=  CHECKSUM_UNUSED
                           )
                        {
                            printf("edit : append error : section sha256 checksum already recorded\n");
                            return WRONG_ARGS;
                        }
                    }

                    str_len = strlen(val);
                    if (str_len < 2 * SHA256_DIGEST_LENGTH) {
                        printf("edit : section sha256 checksum too short\n");
                        return WRONG_ARGS;
                    }
                    else if (str_len > 2 * SHA256_DIGEST_LENGTH) {
                        printf("edit : section sha256 checksum too long\n");
                        return WRONG_ARGS;
                    }

                    if (s_field_specified[EDIT_S_SHA256]) {
                        printf("edit : section sha256 checksum already specified\n");
                        return WRONG_ARGS;
                    }
                    s_field_specified[EDIT_S_SHA256] = 1;
                }
                else if (   strcmp(key, "sha512")   == 0) {
                    if (modify_flag == MODIFY_APPEND_MODE) {
                        if (        tar_section->checksum[CHECKSUM_SHA512_INDEX].type
                                !=  CHECKSUM_UNUSED
                           )
                        {
                            printf("edit : append error : section sha512 checksum already recorded\n");
                            return WRONG_ARGS;
                        }
                    }

                    str_len = strlen(val);
                    if (str_len < 2 * SHA512_DIGEST_LENGTH) {
                        printf("edit : section sha512 checksum too short\n");
                        return WRONG_ARGS;
                    }
                    else if (str_len > 2 * SHA512_DIGEST_LENGTH) {
                        printf("edit : section sha512 checksum too long\n");
                        return WRONG_ARGS;
                    }

                    if (s_field_specified[EDIT_S_SHA512]) {
                        printf("edit : section sha512 checksum already specified\n");
                        return WRONG_ARGS;
                    }
                    s_field_specified[EDIT_S_SHA512] = 1;
                }
                else {
                    printf("edit : unknown key : %s\n", key);
                    return WRONG_ARGS;
                }
                break;
            default:
                printf("edit : logic error\n");
                return WRONG_ARGS;
        }

        repair_keyval_pair(has_arrow, key, val);
    }

    // start processing the key value pairs
    for (i = keyvalpair_start_index; i < argc; i++) {
        str = argv[i];

        parse_keyval_pair(str, &has_arrow, &key, &val);

        switch (mode) {
            case UPDATE_ENTRY_MODE :
                if (        strcmp(key, "name")     == 0) {
                    // unlink from same file name double linked list
                    del_e_from_fn_to_e_chain
                        (
                         &tar_dh->fn_to_e,
                         &tar_dh->fn_mat,
                         &tar_dh->l2_fn_to_e_arr,
                         tar_entry
                        );

                    // update file name
                    strcpy(tar_entry->file_name, val);
                    tar_entry->file_name_len = strlen(val);

                    // link back to same file name related structures
                    link_entry_to_file_name_structures(tar_dh, tar_entry);
                }
                else if (   strcmp(key, "timeaddutc")  == 0) {
                    // unlink from time of addition tree
                    if (tar_entry->tod_utc_used) {
                        unlink_entry_from_date_time_tree(tar_dh, tar_entry, DATE_TOD);
                    }

                    // update time of addition
                    strptime(val, time_format, &tar_entry->tod_utc);

                    // link back to time of addition tree
                    link_entry_to_date_time_tree(tar_dh, tar_entry, DATE_TOD);
                }
                else if (   strcmp(key, "timeaddloc")  == 0) {
                    // unlink from time of addition tree
                    if (tar_entry->tod_utc_used) {
                        unlink_entry_from_date_time_tree(tar_dh, tar_entry, DATE_TOD);
                    }

                    // parse local time of addition
                    strptime(val, time_format, &local_time);

                    // convert from local time to utc
                    raw_time_local = mktime(&local_time);

                    utc_time_p = gmtime(&raw_time_local);
                    memcpy(
                            &tar_entry->tod_utc,
                            utc_time_p,

                            sizeof(struct tm)
                          );

                    // link back to time of addition tree
                    link_entry_to_date_time_tree(tar_dh, tar_entry, DATE_TOD);
                }
                else if (   strcmp(key, "timemodutc")  == 0) {
                    // unlink from time of modification tree
                    if (tar_entry->tom_utc_used) {
                        unlink_entry_from_date_time_tree(tar_dh, tar_entry, DATE_TOM);
                    }

                    // update time of modification
                    strptime(val, time_format, &tar_entry->tom_utc);

                    // link back to time of modification tree
                    link_entry_to_date_time_tree(tar_dh, tar_entry, DATE_TOM);
                }
                else if (   strcmp(key, "timemodloc")  == 0) {
                    // unlink from time of modification tree
                    if (tar_entry->tom_utc_used) {
                        unlink_entry_from_date_time_tree(tar_dh, tar_entry, DATE_TOM);
                    }

                    // parse local time of modification
                    strptime(val, time_format, &local_time);

                    // convert from local time to utc
                    raw_time_local = mktime(&local_time);

                    utc_time_p = gmtime(&raw_time_local);
                    memcpy(
                            &tar_entry->tom_utc,
                            utc_time_p,

                            sizeof(struct tm)
                          );

                    // link back to time of modification tree
                    link_entry_to_date_time_tree(tar_dh, tar_entry, DATE_TOM);
                }
                else if (   strcmp(key, "timeusrutc")  == 0) {
                    // unlink from user specified time tree
                    if (tar_entry->tusr_utc_used) {
                        unlink_entry_from_date_time_tree(tar_dh, tar_entry, DATE_TUSR);
                    }

                    // update user specified time
                    strptime(val, time_format, &tar_entry->tusr_utc);

                    // link back to user specified time tree
                    link_entry_to_date_time_tree(tar_dh, tar_entry, DATE_TUSR);
                }
                else if (   strcmp(key, "timeusrloc")  == 0) {
                    // unlink from user specified time tree
                    if (tar_entry->tusr_utc_used) {
                        unlink_entry_from_date_time_tree(tar_dh, tar_entry, DATE_TUSR);
                    }

                    // parse local user specified time
                    strptime(val, time_format, &local_time);

                    // convert from local time to utc
                    raw_time_local = mktime(&local_time);

                    utc_time_p = gmtime(&raw_time_local);
                    memcpy(
                            &tar_entry->tusr_utc,
                            utc_time_p,

                            sizeof(struct tm)
                          );

                    // link back to user specified time tree
                    link_entry_to_date_time_tree(tar_dh, tar_entry, DATE_TUSR);
                }
                else if (   strcmp(key, "type")     == 0) {
                    if (        strcmp(val, "file")     == 0) {
                        tar_entry->type = ENTRY_FILE;
                    }
                    else if (   strcmp(val, "group")    == 0) {
                        tar_entry->type = ENTRY_GROUP;
                    }
                }
                else if (   strcmp(key, "madeby")   == 0) {
                    if (        strcmp(val, "user")  == 0) {
                        tar_entry->created_by = CREATED_BY_USR;
                    }
                    else if (   strcmp(val, "system")  == 0) {
                        tar_entry->created_by = CREATED_BY_SYS;
                    }
                }
                else if (   strcmp(key, "tag")      == 0) {
                    // unlink from same tag double linked list
                    if (tar_entry->tag_to_e) {  // if previously linked
                        del_e_from_tag_to_e_chain
                            (
                             &tar_dh->tag_to_e,
                             &tar_dh->tag_mat,
                             &tar_dh->l2_tag_to_e_arr,
                             tar_entry
                            );
                    }

                    // update tag string
                    if (modify_flag == MODIFY_WRITE_MODE) {
                        strcpy(tar_entry->tag_str, val);
                        tar_entry->tag_str_len = strlen(val);
                    }
                    else if (modify_flag == MODIFY_APPEND_MODE) {
                        str_len = tar_entry->tag_str_len;
                        strcpy(tar_entry->tag_str + str_len - 1, val);
                        tar_entry->tag_str_len = strlen(tar_entry->tag_str);
                    }

                    // link back to same tag double linked list
                    link_entry_to_tag_str_structures(tar_dh, tar_entry);
                }
                else if (   strcmp(key, "msg")      == 0) {
                    // update user message
                    strcpy(tar_entry->user_msg, val);
                    tar_entry->user_msg_len = strlen(val);
                }
                break;
            case UPDATE_FILE_MODE :
                if (        strcmp(key, "size")     == 0) {
                    // unlink from file size structures
                    if (tar_file_data->f_size_to_fd) {
                        del_fd_from_f_size_to_fd_chain
                            (
                             &tar_dh->f_size_to_fd,
                             &tar_dh->f_size_mat,
                             &tar_dh->l2_f_size_to_fd_arr,
                             tar_file_data
                            );
                    }

                    // update size
                    sscanf(val, "%"PRIu64"", &tar_file_data->file_size);
                    sprintf(tar_file_data->file_size_str, "%"PRIu64"", tar_file_data->file_size);

                    // link back to file size structures
                    link_file_data_to_file_size_structures(tar_dh, tar_file_data);
                }
                else if (   strcmp(key, "sha1")     == 0) {
                    // unlink from sha1 structures
                    if (tar_file_data->sha1f_to_fd) {
                        del_fd_from_sha1f_to_fd_chain
                            (
                             &tar_dh->sha1f_to_fd,
                             &tar_dh->sha1f_mat,
                             &tar_dh->l2_sha1f_to_fd_arr,
                             tar_file_data
                            );
                    }

                    // update sha1 checksum
                    strcpy
                        (
                         tar_file_data->checksum[CHECKSUM_SHA1_INDEX].checksum_str,
                         val
                        );
                    hex_str_to_bytes
                        (
                         tar_file_data->checksum[CHECKSUM_SHA1_INDEX].checksum,
                         val
                        );

                    // link file data back to sha1 structures
                    link_file_data_to_sha1_structures(tar_dh, tar_file_data, tar_file_data->checksum + CHECKSUM_SHA1_INDEX);
                }
                else if (   strcmp(key, "sha256")   == 0) {
                    // unlink from sha256 structures
                    if (tar_file_data->sha256f_to_fd) {
                        del_fd_from_sha256f_to_fd_chain
                            (
                             &tar_dh->sha256f_to_fd,
                             &tar_dh->sha256f_mat,
                             &tar_dh->l2_sha256f_to_fd_arr,
                             tar_file_data
                            );
                    }

                    // update sha256 checksum
                    strcpy
                        (
                         tar_file_data->checksum[CHECKSUM_SHA256_INDEX].checksum_str,
                         val
                        );
                    hex_str_to_bytes
                        (
                         tar_file_data->checksum[CHECKSUM_SHA256_INDEX].checksum,
                         val
                        );

                    // link file data back to sha256 structures
                    link_file_data_to_sha256_structures(tar_dh, tar_file_data, tar_file_data->checksum + CHECKSUM_SHA256_INDEX);
                }
                else if (   strcmp(key, "sha512")   == 0) {
                    // unlink from sha512 structures
                    if (tar_file_data->sha512f_to_fd) {
                        del_fd_from_sha512f_to_fd_chain
                            (
                             &tar_dh->sha512f_to_fd,
                             &tar_dh->sha512f_mat,
                             &tar_dh->l2_sha512f_to_fd_arr,
                             tar_file_data
                            );
                    }

                    // update sha512 checksum
                    strcpy
                        (
                         tar_file_data->checksum[CHECKSUM_SHA512_INDEX].checksum_str,
                         val
                        );
                    hex_str_to_bytes
                        (
                         tar_file_data->checksum[CHECKSUM_SHA512_INDEX].checksum,
                         val
                        );

                    // link file data back to sha512 structures
                    link_file_data_to_sha512_structures(tar_dh, tar_file_data, tar_file_data->checksum + CHECKSUM_SHA512_INDEX);
                }
                break;
            case UPDATE_SECT_MODE :
                if (        strcmp(key, "startpos") == 0) {
                    // update start position
                    sscanf(val, "%"PRIu64"", &tar_section->start_pos);
                }
                else if (   strcmp(key, "endpos")   == 0) {
                    // update end position
                    sscanf(val, "%"PRIu64"", &tar_section->end_pos);
                }
                else if (   strcmp(key, "sha1")     == 0) {
                    // unlink from sha1 structures
                    if (tar_section->sha1s_to_s) {
                        del_s_from_sha1s_to_s_chain
                            (
                             &tar_dh->sha1s_to_s,
                             &tar_dh->sha1s_mat,
                             &tar_dh->l2_sha1s_to_s_arr,
                             tar_section
                            );
                    }

                    // update sha1 checksum
                    strcpy
                        (
                         tar_section->checksum[CHECKSUM_SHA1_INDEX].checksum_str,
                         val
                        );
                    hex_str_to_bytes
                        (
                         tar_section->checksum[CHECKSUM_SHA1_INDEX].checksum,
                         val
                        );

                    // link file data back to sha1 structures
                    link_sect_to_sha1_structures(tar_dh, tar_section, tar_section->checksum + CHECKSUM_SHA1_INDEX);
                }
                else if (   strcmp(key, "sha256")   == 0) {
                    // unlink from sha256 structures
                    if (tar_section->sha256s_to_s) {
                        del_s_from_sha256s_to_s_chain
                            (
                             &tar_dh->sha256s_to_s,
                             &tar_dh->sha256s_mat,
                             &tar_dh->l2_sha256s_to_s_arr,
                             tar_section
                            );
                    }

                    // update sha256 checksum
                    strcpy
                        (
                         tar_section->checksum[CHECKSUM_SHA256_INDEX].checksum_str,
                         val
                        );
                    hex_str_to_bytes
                        (
                         tar_section->checksum[CHECKSUM_SHA256_INDEX].checksum,
                         val
                        );

                    // link file data back to sha256 structures
                    link_sect_to_sha256_structures(tar_dh, tar_section, tar_section->checksum + CHECKSUM_SHA256_INDEX);
                }
                else if (   strcmp(key, "sha512")   == 0) {
                    // unlink from sha512 structures
                    if (tar_section->sha512s_to_s) {
                        del_s_from_sha512s_to_s_chain
                            (
                             &tar_dh->sha512s_to_s,
                             &tar_dh->sha512s_mat,
                             &tar_dh->l2_sha512s_to_s_arr,
                             tar_section
                            );
                    }

                    // update sha512 checksum
                    strcpy
                        (
                         tar_section->checksum[CHECKSUM_SHA512_INDEX].checksum_str,
                         val
                        );
                    hex_str_to_bytes
                        (
                         tar_section->checksum[CHECKSUM_SHA512_INDEX].checksum,
                         val
                        );

                    // link file data back to sha512 structures
                    link_sect_to_sha512_structures(tar_dh, tar_section, tar_section->checksum + CHECKSUM_SHA512_INDEX);
                }
                break;
        }
    }

    // update time of modification if it was not one of the things to be edited
    if (!e_field_specified[EDIT_E_TMOD]) {
        // unlink from time of modification tree
        if (tar_entry->tom_utc_used) {
            unlink_entry_from_date_time_tree(tar_dh, tar_entry, DATE_TOM);
        }

        // get current time
        time(&raw_time);
        time_info = gmtime(&raw_time);
        memcpy(
                &tar_entry->tom_utc,
                time_info,

                sizeof(struct tm)
              );

        // link back to time of modification tree
        link_entry_to_date_time_tree(tar_dh, tar_entry, DATE_TOM);
    }

    // verify entry
    ret = verify_entry(tar_dh, tar_entry, 0x0);
    if (ret) {
        printf("edit : warning, entry failed verification\n");
    }

    return 0;
}

static int verify_recursive(database_handle* dh, linked_entry* entry, ffp_eid_int* rem_depth) {
    ffp_eid_int i;
    int ret;
    linked_entry* temp;

    if (rem_depth && *rem_depth == 0) {
        return 0;
    }

    if (rem_depth) {
        *rem_depth = *rem_depth - 1;
    }

    ret = verify_entry(dh, entry, 0x0);
    if (ret) {
        printf("entry failed verification ^ : %s\n", entry->entry_id_str);
    }

    temp = entry;
    while (temp->child_num == 1) {
        ret = verify_entry(dh, temp, 0x0);
        if (ret) {
            printf("entry failed verification ^ : %s\n", temp->entry_id_str);
        }

        temp = temp->child[0];
    }

    if (temp->child_num > 1) {
        for (i = 0; i < temp->child_num; i++) {
            verify_recursive(dh, temp->child[i], rem_depth);
        }
    }

    if (rem_depth) {
        *rem_depth = *rem_depth + 1;
    }

    return 0;
}

int verify(term_info* info, dir_info* dir, int argc, char* argv[]) {
    int i;
    int ret;

    char* str;

    dir_info result_dir;
    error_handle er_h;

    ffp_eid_int* depth_p = NULL;
    ffp_eid_int depth;

    error_mark_owner(&er_h, "verify");

    if (argc < 1) {
        printf("verify : too few arguments\n");
        return WRONG_ARGS;
    }

    for (i = 0; i < argc; i++) {
        if (IS_CHAR_OPTION(argv[i])) {
        }
        else if (IS_WORD_OPTION(argv[i])) {
            str = argv[i] + 2;
            if (    strcmp(str, "depth") == 0) {
                if (depth_p) {
                    printf("verify : depth already specified\n");
                    return WRONG_ARGS;
                }

                if (sscanf(str, "%"PRIu64"", &depth) != 1) {
                    printf("verify : invalid depth\n");
                    return WRONG_ARGS;
                }
                depth_p = &depth;
            }
        }
    }

    for (i = 0; i < argc; i++) {
        // parse locator
        ret = locator_to_dir(info, dir, argv[i], &result_dir, &er_h);
        switch (ret) {
            case 0:
                break;
            default:
                error_print_owner_msg(&er_h);
                error_mark_inactive(&er_h);
                return ret;
        }

        verify_recursive(result_dir.dh, result_dir.entry, depth_p);
    }

    return 0;
}

int attach(term_info* info, dir_info* dir, int argc, char* argv[]) {
    database_handle* dh;
    linked_entry* entry;

    char* str;

    uint64_t i;

    uint64_t sect_num;

    int ret;

    int cur_index;

    file_data* temp_file_data;
    section* temp_section;

    error_handle er_h;

    dir_info result_dir;

    error_mark_owner(&er_h, "attach");

    if (argc < 2) {
        printf("attach : too few arguments\n");
        return WRONG_ARGS;
    }

    // parse locator
    cur_index = 0;
    str = argv[cur_index];
    ret = locator_to_dir(info, dir, str, &result_dir, &er_h);
    switch (ret) {
        case 0:
            break;
        default:
            error_print_owner_msg(&er_h);
            error_mark_inactive(&er_h);
            return ret;
    }

    dh = result_dir.dh;
    entry = result_dir.entry;

    cur_index++;

    // parse component
    str = argv[cur_index];
    if (        strcmp(str, "file")     == 0) {
        if (argc > 2) {
            printf("attach : too many arguments\n");
            return WRONG_ARGS;
        }

        if (entry->data) {
            printf("attach : file data already attached\n");
            return WRONG_ARGS;
        }

        // grab space for file data
        ret = add_file_data_to_layer2_arr(&dh->l2_file_data_arr, &temp_file_data, NULL);
        if (ret) {
            printf("attach : failed to get space for file data\n");
            return ret;
        }

        entry->data = temp_file_data;
    }
    else if (   strcmp(str, "sect")     == 0) {
        if (argc > 3) {
            printf("attach : too many arguments\n");
            return WRONG_ARGS;
        }

        if (!entry->data) {
            printf("attach : no file data attached, cannot attach sections\n");
            return WRONG_ARGS;
        }

        temp_file_data = entry->data;

        cur_index++;

        // parse number
        str = argv[cur_index];
        if (sscanf(str, "%"PRIu64"", &sect_num) != 1) {
            printf("attach : invalid number of sections\n");
            return WRONG_ARGS;
        }

        // make space for sections
        grow_section_array(temp_file_data, sect_num);
        for (i = 0; i < sect_num; i++) {
            ret = add_section_to_layer2_arr(&dh->l2_section_arr, &temp_section, NULL);
            if (ret) {
                printf("attach : failed to get space for section\n");
                return ret;
            }

            put_into_section_array(temp_file_data, temp_section);
        }
    }
    else {
        printf("attach : unknown component\n");
        return WRONG_ARGS;
    }

    return 0;
}

int detach(term_info* info, dir_info* dir, int argc, char* argv[]) {
    database_handle* dh;
    linked_entry* entry;

    char* str;

    bit_index bit_count;
    bit_index map_indx;
    bit_index skip_to_bit;
    str_len_int i, j;
    str_len_int str_len;

    str_len_int minus_count;
    char* val_left, * val_right;

    uint64_t sect_num, sect_num2;
    uint64_t sect_start_indx, sect_end_indx; // sect_end_indx is inclusive
    uint64_t sect_indx, sect_indx2;
    uint64_t sect_fill_indx;

    int ret;

    int cur_index;

    file_data* temp_file_data;
    section* temp_section;

    error_handle er_h;

    dir_info result_dir;

    error_mark_owner(&er_h, "detach");

    if (argc < 2) {
        printf("detach : too few arguments\n");
        return WRONG_ARGS;
    }

    // parse locator
    cur_index = 0;
    str = argv[cur_index];
    ret = locator_to_dir(info, dir, str, &result_dir, &er_h);
    switch (ret) {
        case 0:
            break;
        default:
            error_print_owner_msg(&er_h);
            error_mark_inactive(&er_h);
            return ret;
    }

    dh = result_dir.dh;
    entry = result_dir.entry;

    cur_index++;

    // parse component
    str = argv[cur_index];
    if (        strcmp(str, "file")     == 0) {
        if (argc > 2) {
            printf("detach : too many arguments\n");
            return WRONG_ARGS;
        }

        if (!entry->data) {
            printf("detach : no file data attached\n");
            return WRONG_ARGS;
        }

        del_file_data(dh, entry->data);

        entry->data = NULL;
    }
    else if (   strcmp(str, "sect")     == 0) {
        if (argc > 3) {
            printf("attach : too many arguments\n");
            return WRONG_ARGS;
        }

        if (!entry->data) {
            printf("attach : no file data attached, cannot attach sections\n");
            return WRONG_ARGS;
        }

        temp_file_data = entry->data;

        cur_index++;

        // prepare bitmap
        ffp_grow_bitmap(&info->map_buf, temp_file_data->section_num);
        bitmap_zero(&info->map_buf);

        // parse range
        str = argv[cur_index];
        str_len = strlen(str);
        for (i = 0; i < str_len; i++) { // go through comma separated list
            for (j = i; j < str_len; j++) {
                if (str[j] == ',') {
                    str[j] = '\0';
                    break;
                }
            }

            parse_range(str + i, &minus_count, &val_left, &val_right);

            if (minus_count == 0) {
                // parse number directly
                if (sscanf(str + i, "%"PRIu64"", &sect_num) != 1) {
                    printf("detach : invalid number\n");
                    return WRONG_ARGS;
                }

                // check if out of range
                if (sect_num + 1 > temp_file_data->section_num) {
                    printf("detach : section number out of range\n");
                    return WRONG_ARGS;
                }

                // mark in bitmap
                bitmap_write(&info->map_buf, sect_num, 1);
            }
            else if (minus_count == 1) {
                if (val_left == NULL) {
                    printf("detach : missing left value in range specifier\n");
                    return WRONG_ARGS;
                }
                if (val_right == NULL) {
                    printf("detach : missing right value in range specifier\n");
                    return WRONG_ARGS;
                }

                // parse range
                if (sscanf(val_left,  "%"PRIu64"", &sect_num)   != 1) {
                    printf("detach : invalid number\n");
                    return WRONG_ARGS;
                }
                if (sscanf(val_right, "%"PRIu64"", &sect_num2)  != 1) {
                    printf("detach : invalid number\n");
                    return WRONG_ARGS;
                }

                // check if out of range
                if (sect_num  + 1   > temp_file_data->section_num) {
                    printf("detach : section number out of range\n");
                    return WRONG_ARGS;
                }
                if (sect_num2 + 1   > temp_file_data->section_num) {
                    printf("detach : section number out of range\n");
                    return WRONG_ARGS;
                }

                sect_start_indx = ffp_min(sect_num, sect_num2);
                sect_end_indx   = ffp_max(sect_num, sect_num2);

                // mark in bitmap
                for (   sect_indx = sect_start_indx;
                        sect_indx <= sect_end_indx; /* sect_end_indx is inclusive */
                        sect_indx++
                    )
                {
                    bitmap_write(&info->map_buf, sect_indx, 1);
                }
            }

            i = j;      // skip processed substring
        }

        // go through bitmap
        for (   bit_count = 0, skip_to_bit = 0;
                bit_count < info->map_buf.number_of_ones;
                bit_count++
            )
        {
            bitmap_first_one_bit_index(&info->map_buf, &map_indx, skip_to_bit);
            skip_to_bit = map_indx + 1;

            // delete section
            del_section(dh, temp_file_data->section[map_indx]);

            temp_file_data->section[map_indx] = NULL;
        }

        // collapse the section array by moving all remaining sections to start
        // and update the section_num and section_free_num
        sect_fill_indx = 0;
        for (sect_indx = 0; sect_indx < temp_file_data->section_num; sect_indx++) {
            temp_section = temp_file_data->section[sect_indx];
            if (!temp_section) {    // empty slot
                sect_fill_indx = sect_indx;

                // skip over empty slots
                for (   sect_indx2 = sect_indx;
                        sect_indx2 < temp_file_data->section_num;
                        sect_indx2++
                    )
                {
                    if (temp_file_data->section[sect_indx2]) {
                        break;
                    }
                }

                sect_indx = sect_indx2 - 1;
            }
            else {
                if (sect_fill_indx == sect_indx) {
                    ;;  // do nothing
                }
                else {
                    temp_file_data
                    ->section[sect_fill_indx] = temp_file_data->section[sect_indx];

                    temp_file_data->section[sect_indx] = NULL;
                }

                sect_fill_indx++;
            }
        }

        temp_file_data->section_num       -= info->map_buf.number_of_ones;
        temp_file_data->section_free_num  += info->map_buf.number_of_ones;
    }
    else {
        printf("detach : unknown component\n");
        return WRONG_ARGS;
    }

    return 0;
}

static void print_entry_eid(linked_entry* entry) {
    char msg[] = "entry id";

    printf("%s%.*s : %s\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad, entry->entry_id_str);
}

static void print_entry_bid(linked_entry* entry) {
    char msg[] = "branch id";

    if (!entry->has_parent) {   // head of branch
        printf("%s%.*s : %s  (head)\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad, entry->branch_id_str);
    }
    else {  // other entries
        printf("%s%.*s : %s\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad, entry->branch_id_str);
    }
}

static void print_entry_name(linked_entry* entry) {
    char msg[] = "name";

    printf("%s%.*s : %s\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad, entry->file_name);
}

static void print_entry_date(linked_entry* entry) {
    time_t raw_time_utc;
    struct tm* local_time_p;
    struct tm local_time;

    const char time_format[] = "%Y-%m-%d %H:%M:%S";

    char buf[100];
    char msg[100];

    if (entry->tod_utc_used) {
        strftime(buf, 100, time_format, &entry->tod_utc);
        strcpy(msg, "time of addition UTC");
        printf("%s%.*s : %s\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad, buf);

        raw_time_utc = timegm(&entry->tod_utc);

        local_time_p = localtime(&raw_time_utc);
        memcpy(
                &local_time,
                local_time_p,

                sizeof(struct tm)
              );

        strftime(buf, 100, time_format, &local_time);
        strcpy(msg, "time of addition local timezone");
        printf("%s%.*s : %s\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad, buf);
    }
    else {
        printf("No - time of addition - recorded\n");
    }

    if (entry->tom_utc_used) {
        strftime(buf, 100, time_format, &entry->tom_utc);
        strcpy(msg, "time of modification UTC");
        printf("%s%.*s : %s\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad, buf);

        raw_time_utc = timegm(&entry->tom_utc);

        local_time_p = localtime(&raw_time_utc);
        memcpy(
                &local_time,
                local_time_p,

                sizeof(struct tm)
              );

        strftime(buf, 100, time_format, &local_time);
        strcpy(msg, "time of modification local timezone");
        printf("%s%.*s : %s\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad, buf);
    }
    else {
        printf("No - time of addition - recorded\n");
    }

    if (entry->tusr_utc_used) {
        strftime(buf, 100, time_format, &entry->tusr_utc);
        strcpy(msg, "user specified time UTC");
        printf("%s%.*s : %s\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad, buf);

        raw_time_utc = timegm(&entry->tusr_utc);

        local_time_p = localtime(&raw_time_utc);
        memcpy(
                &local_time,
                local_time_p,

                sizeof(struct tm)
              );

        strftime(buf, 100, time_format, &local_time);
        strcpy(msg, "user specified time local timezone");
        printf("%s%.*s : %s\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad, buf);
    }
    else {
        printf("No - user specified time - recorded\n");
    }
}

static void print_entry_type(linked_entry* entry) {
    char msg[] = "type";

    switch (entry->type) {
        case ENTRY_FILE :
            printf("%s%.*s : file\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad);
            break;
        case ENTRY_GROUP :
            printf("%s%.*s : group\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad);
            break;
        default :
            printf("%s%.*s : unknown\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad);
            break;
    }
}

static void print_entry_created_by(linked_entry* entry) {
    char msg[] = "created by";

    switch (entry->created_by) {
        case CREATED_BY_SYS :
            printf("%s%.*s : system\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad);
            break;
        case CREATED_BY_USR:
            printf("%s%.*s : user\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad);
            break;
        default :
            printf("%s%.*s : unknown\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad);
            break;
    }
}

static void print_entry_tag(linked_entry* entry) {
    char tag[TAG_LEN_MAX+1];
    char* str;
    char msg[100];

    int count;

    str_len_int i;

    str_len_int len_read;

    if (entry->tag_str_len > 0) {
        for (   i = 0, count = 0, len_read = 0;
                i < entry->tag_str_len; 
                i += len_read + 1
            )
        {
            str = entry->tag_str + i;
            grab_until_sep(tag, str, entry->tag_str_len - i, &len_read, NULL, NO_IGNORE_ESCAPE_CHAR, '|');

            if (len_read > 0) {
                sprintf(msg, "tag #%d", count);
                printf("%s%.*s : %s\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad, tag);
                count++;
            }
        }
    }
    else {
        printf("No - tags - recorded\n");
    }
}

static void print_entry_msg(linked_entry* entry) {
    if (entry->user_msg_len > 0) {
        printf("user message :\n");
        printf("---------- user message begin ----------\n");
        printf("%s\n", entry->user_msg);
        printf("----------  user message end  ----------\n");
    }
    else {
        printf("No - user message - recorded\n");
    }
}

static void print_entry_childnum(linked_entry* entry) {
    char msg[] = "no. of children";
    printf("%s%.*s : %"PRIu64"\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad, entry->child_num);
}

static void print_file_size(file_data* data) {
    char msg[] = "file size";
    printf("%s%.*s : %s\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad, data->file_size_str);
}

static void print_extract(extract_sample* extr) {
    char msg_pos[] = "extract position";
    char msg_len[] = "extract length";
    char msg_con[] = "extract content (hex)";
    char extr_buf[2 * EXTRACT_SIZE_MAX + 1];

    printf("%s%.*s : %"PRIu64"\n", msg_pos, calc_pad(msg_pos, PRINT_PAD_SIZE), space_pad, extr->position);
    printf("%s%.*s : %"PRIu16"\n", msg_len, calc_pad(msg_len, PRINT_PAD_SIZE), space_pad, extr->len);

    bytes_to_hex_str(extr_buf, extr->extract, extr->len);
    printf("%s%.*s : %s\n", msg_con, calc_pad(msg_con, PRINT_PAD_SIZE), space_pad, extr_buf);
}

static void print_file_extr(file_data* data) {
    int i;

    if (data->extract_num == 0) {
        printf("No - extracts - recorded\n");
    }
    else {
        for (i = 0; i < data->extract_num; i++) {
            printf("extract : %*d\n", NUMBER_SHIFT, i);

            print_extract(data->extract + i);
        }
    }
}

static void print_file_sha1(file_data* data) {
    char msg[] = "file sha1 checksum";
    if (data->checksum[CHECKSUM_SHA1_INDEX].type == CHECKSUM_UNUSED) {
        printf("No - sha1 checksum - recorded\n");
    }
    else {
        printf("%s :\n", msg);
        printf("    %s\n", data->checksum[CHECKSUM_SHA1_INDEX].checksum_str);
    }
}

static void print_file_sha256(file_data* data) {
    char msg[] = "file sha256 checksum";
    if (data->checksum[CHECKSUM_SHA256_INDEX].type == CHECKSUM_UNUSED) {
        printf("No - sha256 checksum - recorded\n");
    }
    else {
        printf("%s :\n", msg);
        printf("    %s\n", data->checksum[CHECKSUM_SHA256_INDEX].checksum_str);
    }
}

static void print_file_sha512(file_data* data) {
    char msg[] = "file sha512 checksum";
    if (data->checksum[CHECKSUM_SHA512_INDEX].type == CHECKSUM_UNUSED) {
        printf("No - sha512 checksum - recorded\n");
    }
    else {
        printf("%s :\n", msg);
        printf("    %s\n", data->checksum[CHECKSUM_SHA512_INDEX].checksum_str);
    }
}

static void print_file_sectnum(file_data* data) {
    char msg[] = "number of sections";
    printf("%s%.*s : %"PRIu64"\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad, data->section_num);
}

static void print_file_sectsize(file_data* data) {
    char msg[100];

    if (data->norm_sect_size == 0 && data->last_sect_size == 0) {
        printf("Non-continuous section partitioning is used\n");
    }
    else {
        strcpy(msg, "normal section size");
        printf("%s%.*s : %"PRIu64"\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad, data->norm_sect_size);
        strcpy(msg, "last section size");
        printf("%s%.*s : %"PRIu64"\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad, data->last_sect_size);
    }
}

static void print_sect_size(section* sect) {
    char msg[] = "section size";

    uint64_t size;

    size = sect->end_pos - sect->start_pos + 1;

    printf("%s%.*s : %"PRIu64"\n", msg, calc_pad(msg, PRINT_PAD_SIZE), space_pad, size);
}

static void print_sect_extr(section* sect) {
    int i;

    if (sect->extract_num == 0) {
        printf("No - extracts - recorded\n");
    }
    else {
        for (i = 0; i < sect->extract_num; i++) {
            printf("extract : %*d\n", NUMBER_SHIFT, i);

            print_extract(sect->extract + i);
        }
    }
}

static void print_sect_pos(section* sect) {
    char msg1[] = "section start pos";
    char msg2[] = "section end pos";

    printf("%s%.*s : %"PRIu64"\n", msg1, calc_pad(msg1, PRINT_PAD_SIZE), space_pad, sect->start_pos);
    printf("%s%.*s : %"PRIu64"\n", msg2, calc_pad(msg2, PRINT_PAD_SIZE), space_pad, sect->end_pos);
}

static void print_sect_sha1(section* sect) {
    char msg[] = "section sha1 checksum";
    if (sect->checksum[CHECKSUM_SHA1_INDEX].type == CHECKSUM_UNUSED) {
        printf("No - sha1 checksum - recorded\n");
    }
    else {
        printf("%s :\n", msg);
        printf("    %s\n", sect->checksum[CHECKSUM_SHA1_INDEX].checksum_str);
    }
}

static void print_sect_sha256(section* sect) {
    char msg[] = "section sha256 checksum";
    if (sect->checksum[CHECKSUM_SHA256_INDEX].type == CHECKSUM_UNUSED) {
        printf("No - sha256 checksum - recorded\n");
    }
    else {
        printf("%s :\n", msg);
        printf("    %s\n", sect->checksum[CHECKSUM_SHA256_INDEX].checksum_str);
    }
}

static void print_sect_sha512(section* sect) {
    char msg[] = "section sha512 checksum";
    if (sect->checksum[CHECKSUM_SHA512_INDEX].type == CHECKSUM_UNUSED) {
        printf("No - sha512 checksum - recorded\n");
    }
    else {
        printf("%s :\n", msg);
        printf("    %s\n", sect->checksum[CHECKSUM_SHA512_INDEX].checksum_str);
    }
}

/*
 Design for show command:

 show locator FIELD1 FIELD2 ...
 FIELD := all | basic | eid | bid | name | date | type | madeby | tag | msg | childnum | file | sect
 choosing all will display all entry info, all file info, and all sect info
 choosing basic will display only the basic info of entry(does not print file info)
 defaults to basic

 show locator ... file FIELD1 FIELD2 ...
 FIELD := all | size | sha1 | sha256 | sha512 | sectnum | sectsize
 choosing all will display all file info
 defaults to all

 show locator ... sect OPT FIELD1 FIELD2 ...
 OPT := N | ALL
 N is any non-negative number which indicates the Nth section
 ALL will select all sections (can produce very lengthy output beware)

 FIELD := all | size | sha1 | sha256 | sha512
 defaults to all
 */
int show(term_info* info, dir_info* dir, int argc, char* argv[]) {
    linked_entry* temp_entry_find;
    database_handle* temp_dh_find;

    file_data* temp_file_data;
    section* temp_section;

    int i, ret;

    dir_info result_dir;

    char* str;

    char all_string[] = "all";

    uint64_t s_loop_cur, s_loop_upper_bound;  // upper bound is inclusive

    unsigned char mode = SHOW_ENTRY_MODE;

    char*           e_argv[SHOW_MAX_ARG];
    int             e_argc = 0;
    unsigned char   e_ignore = 0;

    unsigned char   f_specified = 0;
    char*           f_argv[SHOW_MAX_ARG];
    int             f_argc      = 0;
    unsigned char   f_ignore    = 0;

    unsigned char   s_specified     = 0;
    char*           s_argv[SHOW_MAX_ARG];
    int             s_argc          = 0;
    unsigned char   s_ignore        = 0;
    unsigned char   s_display_all   = 0;    // default to not showing all sections
    uint64_t        s_num;

    error_handle er_h;

    error_mark_inactive(&er_h);
    error_mark_owner(&er_h, "show");

    if (argc < 1) {
        printf("show : too few arguments\n");
        return WRONG_ARGS;
    }
    if (argc > SHOW_MAX_ARG) {
        printf("show : too many arguments\n");
        return WRONG_ARGS;
    }

    // handle target locator
    ret = locator_to_dir(info, dir, argv[0], &result_dir, &er_h);
    switch (ret) {
        case 0 :
            // do nothing
            break;
        default:
            error_print_owner_msg(&er_h);
            error_mark_inactive(&er_h);
            return ret;
    }

    if (is_pointing_to_root(&result_dir)) {
        printf("show : cannot show root - nothing to show\n");
        return WRONG_ARGS;
    }
    else if (is_pointing_to_db(&result_dir)) {
        printf("show : cannot show database, use showdb instead\n");
        return WRONG_ARGS;
    }

    temp_dh_find = result_dir.dh;
    temp_entry_find = result_dir.entry;

    // categorise fields
    for (i = 1; i < argc; i++) {    // entry info
        str = argv[i];

        if (        strcmp(str, "file")     == 0 ) {
            if (f_specified) {
                printf("show : fields categoriser \"file\" already specified\n");
                return WRONG_ARGS;
            }
            f_specified = 1;
            mode = SHOW_FILE_MODE;
            continue;
        }
        else if (   strcmp(str, "sect")     == 0 ) {
            if (s_specified) {
                printf("show : fields categoriser \"sect\" already specified\n");
                return WRONG_ARGS;
            }
            if ((argc-1) - i < 1) { // must at least specify OPT
                printf("show : sect : missing OPT\n");
                return WRONG_ARGS;
            }

            i++;
            str = argv[i];

            if (    strcmp(str, "ALL")    == 0) {
                s_display_all = 1;
            }
            else {
                // convert to number and see if it makes sense
                if (sscanf(str, "%"PRIu64"", &s_num) != 1) {
                    printf("show : sect : invalid OPT : %s\n", str);
                    return WRONG_ARGS;
                }

                s_display_all = 0;
            }
            s_specified = 1;
            mode = SHOW_SECT_MODE;
            continue;
        }
       
        switch (mode) {
            case SHOW_ENTRY_MODE :
                if (strcmp(str, "all") == 0) {
                    // put "all" in all arg lists
                    e_argv[0] = str;
                    e_argc = 1;
                    f_argv[0] = str;
                    f_argc = 1;
                    s_argv[0] = str;
                    s_argc = 1;
                    s_display_all = 1;
                    goto finish_categorise;
                }
                if (e_ignore) {
                    break;
                }
                if (strcmp(str, "basic") == 0) {
                    e_argv[0] = str;
                    e_argc = 1;
                    e_ignore = 1;
                }
                else {
                    if (        strcmp(str, "eid"       )   != 0
                            &&  strcmp(str, "bid"       )   != 0
                            &&  strcmp(str, "name"      )   != 0
                            &&  strcmp(str, "date"      )   != 0
                            &&  strcmp(str, "type"      )   != 0
                            &&  strcmp(str, "madeby"    )   != 0
                            &&  strcmp(str, "tag"       )   != 0
                            &&  strcmp(str, "msg"       )   != 0
                            &&  strcmp(str, "childnum"  )   != 0
                            &&  strcmp(str, "file"      )   != 0
                            &&  strcmp(str, "sect"      )   != 0
                       )
                    {
                        printf("show : unknown entry field : %s\n", str);
                        return WRONG_ARGS;
                    }
                    e_argv[e_argc++] = str;
                }
                break;
            case SHOW_FILE_MODE :
                if (f_ignore) {
                    break;
                }
                if (strcmp(str, "all") == 0) {
                    f_argv[0] = str;
                    f_argc = 1;
                    f_ignore = 1;
                }
                else {
                    if (        strcmp(str, "info"      )   != 0
                            &&  strcmp(str, "size"      )   != 0
                            &&  strcmp(str, "extr"      )   != 0
                            &&  strcmp(str, "sha1"      )   != 0
                            &&  strcmp(str, "sha256"    )   != 0
                            &&  strcmp(str, "sha512"    )   != 0
                            &&  strcmp(str, "sectnum"   )   != 0
                            &&  strcmp(str, "sectsize"  )   != 0
                       )
                    {
                        printf("show : unknown file field : %s\n", str);
                        return WRONG_ARGS;
                    }
                    f_argv[f_argc++] = str;
                }
                break;
            case SHOW_SECT_MODE :
                if (s_ignore) {
                    break;
                }
                if (strcmp(str, "all") == 0) {
                    s_argv[0] = str;
                    s_argc = 1;
                    s_ignore = 1;
                }
                else {
                    if (        strcmp(str, "size"      )   != 0
                            &&  strcmp(str, "extr"      )   != 0
                            &&  strcmp(str, "pos"       )   != 0
                            &&  strcmp(str, "sha1"      )   != 0
                            &&  strcmp(str, "sha256"    )   != 0
                            &&  strcmp(str, "sha512"    )   != 0
                       )
                    {
                        printf("show : unknown sect field : %s\n", str);
                        return WRONG_ARGS;
                    }
                    s_argv[s_argc++] = str;
                }
                break;
             default :
                return LOGIC_ERROR;
        }
    }

    // set default to all if no option specified
    if (        e_argc == 0
            &&  !f_specified
            &&  !s_specified
       )
    {
        e_argv[e_argc++] = all_string;
        f_argv[f_argc++] = all_string;
    }
    else {
        if (f_specified && f_argc == 0) {
            f_argv[f_argc++] = all_string;
        }
        if (s_specified && s_argc == 0) {
            s_argv[s_argc++] = all_string;
        }
    }

finish_categorise:

    if (s_argc > 0) {
        if (temp_entry_find->data) {
            if (!s_display_all) {
                if (s_num + 1 > temp_entry_find->data->section_num) {
                    printf("show : section number out of range\n");
                    return WRONG_ARGS;
                }
            }
        }
    }

    // process entry fields
    if (e_argc > 0) {
        printf("===== entry fields START =====\n");

        for (i = 0; i < e_argc; i++) {
            str = e_argv[i];
            if (        strcmp(str, "all"   )   == 0
                    ||  strcmp(str, "basic" )   == 0 
               )
            {
                print_entry_eid         (temp_entry_find);
                print_entry_bid         (temp_entry_find);
                print_entry_name        (temp_entry_find);
                print_entry_date        (temp_entry_find);
                print_entry_type        (temp_entry_find);
                print_entry_created_by  (temp_entry_find);
                print_entry_tag         (temp_entry_find);
                print_entry_msg         (temp_entry_find);
                print_entry_childnum    (temp_entry_find);
            }
            else if (   strcmp(str, "eid")      == 0    ) {
                print_entry_eid         (temp_entry_find);
            }
            else if (   strcmp(str, "bid")      == 0    ) {
                print_entry_bid         (temp_entry_find);
            }
            else if (   strcmp(str, "name")     == 0    ) {
                print_entry_name        (temp_entry_find);
            }
            else if (   strcmp(str, "date")     == 0    ) {
                print_entry_date        (temp_entry_find);
            }
            else if (   strcmp(str, "type")     == 0    ) {
                print_entry_type        (temp_entry_find);
            }
            else if (   strcmp(str, "madeby")     == 0    ) {
                print_entry_created_by  (temp_entry_find);
            }
            else if (   strcmp(str, "tag")      == 0    ) {
                print_entry_tag         (temp_entry_find);
            }
            else if (   strcmp(str, "msg")      == 0    ) {
                print_entry_msg         (temp_entry_find);
            }
            else if (   strcmp(str, "childnum") == 0    ) {
                print_entry_childnum    (temp_entry_find);
            }
        }

        printf("===== entry fields END =====\n");
    }

    temp_file_data = temp_entry_find->data;

    // process file fields
    if (f_argc > 0) {
        if (!temp_file_data) {
            printf("===== No file fields recorded =====\n");
        }
        else {
            printf("===== file fields START =====\n");

            for (i = 0; i < f_argc; i++) {
                str = f_argv[i];

                if (        strcmp(str, "all"       )   == 0    ) {
                    print_file_size     (temp_file_data);
                    print_file_extr     (temp_file_data);
                    print_file_sha1     (temp_file_data);
                    print_file_sha256   (temp_file_data);
                    print_file_sha512   (temp_file_data);
                    print_file_sectnum  (temp_file_data);
                    print_file_sectsize (temp_file_data);
                }
                else if (   strcmp(str, "size"      )   == 0    ) {
                    print_file_size     (temp_file_data);
                }
                else if (   strcmp(str, "extr"      )   == 0    ) {
                    print_file_extr     (temp_file_data);
                }
                else if (   strcmp(str, "sha1"      )   == 0    ) {
                    print_file_sha1     (temp_file_data);
                }
                else if (   strcmp(str, "sha256"    )   == 0    ) {
                    print_file_sha256   (temp_file_data);
                }
                else if (   strcmp(str, "sha512"    )   == 0    ) {
                    print_file_sha512   (temp_file_data);
                }
                else if (   strcmp(str, "sectnum"   )   == 0    ) {
                    print_file_sectnum  (temp_file_data);
                }
                else if (   strcmp(str, "sectsize"  )   == 0    ) {
                    print_file_sectsize (temp_file_data);
                }
            }

            printf("===== file fields END =====\n");
        }
    }

    // process section fields
    if (s_argc > 0) {
        if (!temp_file_data || temp_file_data->section_num == 0) {
            printf("===== No section fields recorded =====\n");
        }
        else {
            printf("===== section fields START =====\n");

            if (s_display_all) {
                s_loop_cur = 0;
                s_loop_upper_bound = temp_file_data->section_num - 1;
            }
            else {
                s_loop_cur = s_num;
                s_loop_upper_bound = s_num;
            }
            // upper bound is inclusive, use of <= is deliberate
            for (   /* initialised in above lines */;
                    s_loop_cur <= s_loop_upper_bound;
                    s_loop_cur++
                )
            {
                printf("--------------------------------------------------\n");

                printf("section : %*"PRIu64"\n", NUMBER_SHIFT, s_loop_cur);

                printf("--------------------------------------------------\n");

                temp_section = temp_file_data->section[s_loop_cur];

                for (i = 0; i < s_argc; i++) {
                    str = s_argv[i];
                    if (        strcmp(str, "all"       )   == 0    ) {
                        print_sect_size     (temp_section);
                        print_sect_pos      (temp_section);
                        print_sect_extr     (temp_section);
                        print_sect_sha1     (temp_section);
                        print_sect_sha256   (temp_section);
                        print_sect_sha512   (temp_section);
                    }
                    else if (   strcmp(str, "size"      )   == 0    ) {
                        print_sect_size     (temp_section);
                    }
                    else if (   strcmp(str, "pos"       )   == 0    ) {
                        print_sect_pos      (temp_section);
                    }
                    else if (   strcmp(str, "extr"      )   == 0    ) {
                        print_sect_extr     (temp_section);
                    }
                    else if (   strcmp(str, "sha1"      )   == 0    ) {
                        print_sect_sha1     (temp_section);
                    }
                    else if (   strcmp(str, "sha256"    )   == 0    ) {
                        print_sect_sha256   (temp_section);
                    }
                    else if (   strcmp(str, "sha512"    )   == 0    ) {
                        print_sect_sha512   (temp_section);
                    }
                }
            }

            printf("===== section fields END =====\n");
        }
    }

    return 0;
}

static void print_commands() {
    printf("<< command list >>\n");
    // database related
    printf("== database ==\n");
    printf("    newdb       - created a new database\n");
    printf("    loaddb      - load a database from file into memory\n");
    printf("    unloadb     - unload database from memory\n");
    printf("    savedb      - save a database into file (does not unload)\n");
    printf("    savealldb   - save all databases into files (does not unload)\n");

    // logical directory traversal related
    printf("\n");
    printf("== logical directory(not your file system) ==\n");
    printf("    -- traversal --\n");
    printf("        pwd     - show current working directory\n");
    printf("        ls      - list content of directory\n");
    printf("        cd      - change directory\n");
    printf("\n");
    printf("    -- manipulation --\n");
    printf("        cp      - copy entries\n");
    printf("        rm      - remove entries\n");
    printf("        mv      - move entries\n");

    // entry related
    printf("\n");
    printf("== entry ==\n");
    printf("    -- creation(manual) --\n");
    printf("        touch   - create a file type entry\n");
    printf("        mkdir   - create a group type entry\n");
    printf("\n");
    printf("    -- reading --\n");
    printf("        show    - show entry info\n");
    printf("\n");
    printf("    -- modification & verification --\n");
    printf("        edit    - edit fields of entry\n");
    printf("        verify  - verify entries and report violations\n");
    printf("        attach  - attach components to entry\n");
    printf("        detach  - detach components from entry\n");
    printf("\n");
    printf("    -- lookup & comparison --\n");
    printf("        find    - search within database using values,\n");
    printf("                  search within database using files, or\n");
    printf("                  search on disk using fingerprints stored\n");
    printf("        cmp     - compare between fingerprints (recursively), or\n");
    printf("                  compare between fingerprints and files\n");

    // data collection
    printf("\n");
    printf("== data collection ==\n");
    printf("    fp      - fingerprint file or directory\n");

    // file system
    printf("\n");
    printf("== file system ==\n");
    printf("    fpwd    - show current working directory of ffprinter\n");
    printf("    fls     - list files in file system\n");
    printf("    fcd     - change directory in file system\n");

    // misc
    printf("\n");
    printf("== misc ==\n");
    printf("    help    - provide help\n");
    printf("    scanmem - scan memory of ffprinter for a given pattern\n");
    printf("    exit    - exit ffprinter\n");
}

static void print_topics() {
    printf("<< topic list >>\n");
    printf("== locator ==\n");
    printf("    Note on partial matching:\n");
    printf("        the result of partial matching is used only\n");
    printf("        when it is uniquely matched to one entry\n");
    printf("\n");
    printf("    locator is of one of the following form:\n");
    printf("\n");
    printf("        a:aliasname\n");
    printf("            access entry previously recorded as an alias\n");
    printf("\n");
    printf("            partial matching is done on aliasname\n");
    printf("\n");
    printf("        d:[dbname]->entryid\n");
    printf("            directly access entry using database name and its entry id\n");
    printf("\n");
    printf("            when dbname is not provided, and you are somewhere in a database\n");
    printf("            it will default to the database you are currently residing in\n");
    printf("\n");
    printf("            partial matching is done on both dbname and entryid\n");
    printf("\n");
    printf("        e:path\n");
    printf("            access entry via path constructed using entry id\n");
    printf("\n");
    printf("            path can be relative or absolute\n");
    printf("            path structure is same as what you would use in FS\n");
    printf("\n");
    printf("            partial matching is done on each level of the path\n");
    printf("\n");
    printf("        f:path or path\n");
    printf("            access entry via path constructed using entry (file)name\n");
    printf("\n");
    printf("            path can be relative or absolute\n");
    printf("            path structure is same as what you would use in FS\n");
    printf("\n");
    printf("            partial matching is done on each level of the path\n");
}

int help(term_info* info, dir_info* dir, int argc, char* argv[]) {
    char* str;

    if (argc > 1) {
        printf("help : too many arguments, please enter only one command to look for\n");
        return WRONG_ARGS;
    }

    if (argc == 0) {
        printf("type \"help command\" for list of commands\n");
        printf("type \"help topic\" for list of topic\n");
    }
    else {
        str = argv[0];

        if (        strcmp(str, "command")  == 0) {
            print_commands();
        }
        else if (   strcmp(str, "topic")    == 0) {
            print_topics();
        }
        /* == database == */
        else if (   strcmp(str, "newdb")    == 0) {
            printf("******************************\n");
            printf("Usage: newdb dbname\n");
            printf("Creates a database with dbname as its name\n");
            printf("Note:\n");
            printf("    All database are located under root\n");
            printf("    use help database for more info\n");
            printf("******************************\n");
        }
        else if (   strcmp(str, "loaddb")   == 0) {
            printf("******************************\n");
            printf("Usage: loaddb dbname[.ffp]\n");
            printf("Loads database file with dbname as name\n");
            printf("Note:\n");
            printf("    .ffp suffix is optional\n");
            printf("    if suffix is missing, it will be added automatically\n");
            printf("******************************\n");
        }
        else if (   strcmp(str, "unloaddb") == 0) {
            printf("******************************\n");
            printf("Usage: unloaddb dbname\n");
            printf("Unloads(clears) database from memory\n");
            printf("******************************\n");
        }
        else if (   strcmp(str, "savedb")   == 0) {
            printf("******************************\n");
            printf("Usage: savedb dbname [filename]\n");
            printf("Saves database to file in current working directory\n");
            printf("Note:\n");
            printf("    If no filename is supplied, dbname.ffp will be the file name\n");
            printf("\n");
            printf("    If filename is supplied and filename is missing .ffp suffix,\n");
            printf("    then .ffp suffix will be added automatically to filename\n");
            printf("******************************\n");
        }
        else if (   strcmp(str, "savealldb")    == 0) {
            printf("******************************\n");
            printf("Usage: savealldb\n");
            printf("Saves all databases to file in current working directory\n");
            printf("Note:\n");
            printf("    All databases are saved using the name of the database\n");
            printf("    The behaviour is exactly same as savedb with no file name\n");
            printf("    specified\n");
            printf("******************************\n");
        }
        /* == logical directory == */
        //      -- traversal --
        else if (   strcmp(str, "pwd")      == 0) {
            printf("******************************\n");
            printf("Usage: pwd [eid]\n");
            printf("Shows current working directory(not in FS)\n");
            printf("eid mode constructs path using entry id instead of names\n");
            printf("******************************\n");
        }
        else if (   strcmp(str, "ls")       == 0) {
            printf("******************************\n");
            printf("Usage: ls option locator\n");
            printf("List the entry that locator points to\n");
            printf("Format:\n");
            printf("    option:\n");
            printf("        -l      list in detail(shows name, entry id, last modification date)\n");
            printf("        -d      list directory\n");
            printf("******************************\n");
        }
        else if (   strcmp(str, "cd")       == 0) {
            printf("******************************\n");
            printf("Usage: cd locator\n");
            printf("Changes current working directory to locator\n");
            printf("******************************\n");
        }
        //      -- manipulation --
        else if (   strcmp(str, "rm")       == 0) {
            printf("******************************\n");
            printf("Usage: rm locator\n");
            printf("Removes entry that the locator points to\n");
            printf("******************************\n");
        }
        else if (   strcmp(str, "mv")       == 0) {
            printf("******************************\n");
            printf("Usage: mv locator locatordst\n");
            printf("Moves entry pointed to by locator to\n");
            printf("under entry pointed to by locatordst\n");
            printf("******************************\n");
        }
        /* == entry == */
        //      -- creation --
        else if (   strcmp(str, "touch")    == 0) {
            printf("******************************\n");
            printf("Usage: mkdir [locator/]name\n");
            printf("******************************\n");
        }
        else if (   strcmp(str, "mkdir")    == 0) {
            printf("******************************\n");
            printf("Usage: mkdir [locator/]name\n");
            printf("******************************\n");
        }
        //      -- reading --
        else if (   strcmp(str, "show")     == 0) {
            printf("******************************\n");
            printf("Usage: show locator [entryfield...] [file filefield...] [sect SOPT sectionfield...]\n");
            printf("Format:\n");
            printf("    entryfield  := all | basic | eid | bid | name | date | type\n");
            printf("                   madeby | tag | msg | childnum\n");
            printf("\n");
            printf("        choosing all will display all entry info\n");
            printf("        all file info, and all section info\n");
            printf("        choosing basic will display only the info of entry\n");
            printf("        defaults to basic\n");
            printf("\n");
            printf("    filefield   := all | size | extr | sha1 | sha256 | sha512\n");
            printf("                   sectnum | sectsize\n");
            printf("\n");
            printf("        choosing all will display all file info\n");
            printf("        defaults to all\n");
            printf("\n");
            printf("    SOPT        := N | ALL\n");
            printf("\n");
            printf("        N is any non-negative integer which denotes the Nth section\n");
            printf("        ALL will select all sections (can produce very lengthy output)\n");
            printf("\n");
            printf("    sectfield   := all | size | extr | pos | sha1 | sha256 | sha512\n");
            printf("\n");
            printf("        choosing all will display all section info\n");
            printf("        defaults to all\n");
            printf("\n");
            printf("******************************\n");
        }
        //      -- modification & verification --
        else if (   strcmp(str, "edit")   == 0) {
            printf("******************************\n");
            printf("Usage: edit locator mode [modifyflag] keyvalpair...\n");
            printf("Edit fields with specified key value pairs\n");
            printf("Format:\n");
            printf("    mode        := entry | file | sect N\n");
            printf("        entry       - entry related fields\n");
            printf("        file        - file data related fields\n");
            printf("        sect N      - section #N, section related fields\n");
            printf("    modifyflag  := w | a\n");
            printf("        w       - overwrite mode\n");
            printf("        a       - append mode\n");
            printf("    keyvalpair is of format \"key->val\"\n");
            printf("\n");
            printf("Possible keys, and value formats:\n");
            printf("    entry mode :\n");
            printf("        name        - name of entry\n");
            printf("            format : name\n");
            printf("        timeaddutc  - time of addition in UTC\n");
            printf("            format : YYYY/MM/DD_hh:mm:ss\n");
            printf("        timeaddloc  - time of addition in local timezone\n");
            printf("            format : YYYY/MM/DD_hh:mm:ss\n");
            printf("        timemodutc  - time of addition in UTC\n");
            printf("            format : YYYY/MM/DD_hh:mm:ss\n");
            printf("        timemodloc  - time of addition in local timezone\n");
            printf("            format : YYYY/MM/DD_hh:mm:ss\n");
            printf("        timeusrutc  - time of addition in UTC\n");
            printf("            format : YYYY/MM/DD_hh:mm:ss\n");
            printf("        timeusrloc  - time of addition in local timezone\n");
            printf("            format : YYYY/MM/DD_hh:mm:ss\n");
            printf("        type        - type of entry\n");
            printf("            format : file | group\n");
            printf("        madeby      - entry made by whom\n");
            printf("            format : user | system\n");
            printf("        tag         - tags for indexing entry\n");
            printf("            format : |tag1|tag2|tag3|, | can be escaped using \\\n");
            printf("        msg         - user message\n");
            printf("            format : message\n");
            printf("    file data mode :\n");
            printf("        size        - file size\n");
            printf("        sha1        - sha1 checksum of file\n");
            printf("        sha256      - sha256 checksum of file\n");
            printf("        sha512      - sha512 checksum of file\n");
            printf("    section mode(not normally set manually) :\n");
            printf("        startpos    - starting position of section\n");
            printf("        endpos      - ending position of section(inclusive)\n");
            printf("        sha1        - sha1 checksum of section\n");
            printf("        sha256      - sha256 checksum of section\n");
            printf("        sha512      - sha512 checksum of section\n");
            printf("\n");
            printf("Note:\n");
            printf("    edit overwrite fields by default\n");
            printf("    use append flag for adding to fields like tags\n");
            printf("    if field can only contain one element, but already filled\n");
            printf("    append mode will fail, while overwrite mode will not\n");
            printf("******************************\n");
        }
        else if (   strcmp(str, "verify")   == 0) {
            printf("******************************\n");
            printf("Usage: verify [opt] locator...\n");
            printf("Verifies entries pointed to by the locator\n");
            printf("Format:\n");
            printf("    Options:\n");
            printf("        -r      recursive\n");
            printf("******************************\n");
        }
        else if (   strcmp(str, "attach")   == 0) {
            printf("******************************\n");
            printf("Usage: attach locator component\n");
            printf("Attaches blank components to entry pointed to by locator\n");
            printf("Format:\n");
            printf("    component := file | sect N\n");
            printf("        file    - file data\n");
            printf("        sect N  - section, N being number of sections to add\n");
            printf("Note:\n");
            printf("    target must have file data attached before\n");
            printf("    any sections can be attached to it\n");
            printf("******************************\n");
        }
        else if (   strcmp(str, "detach")   == 0) {
            printf("******************************\n");
            printf("Usage: detach locator component\n");
            printf("Remove existing components from entry pointed to by locator\n");
            printf("Format:\n");
            printf("    component := file | sect range\n");
            printf("        file        - file data\n");
            printf("        sect range  - sections to remove\n");
            printf("            you can specify multiple index using ranges and comma separated lists:\n");
            printf("            if you want to include space, please quote the string\n");
            printf("            e.g. sect 0-4,9-12 or sect \"0-4, 9-12\"\n");
            printf("Note:\n");
            printf("    target must have file data attached before\n");
            printf("    any sections can be attached to it\n");
            printf("******************************\n");
        }
        //      -- lookup & comparison --
        else if (   strcmp(str, "find")     == 0) {
            printf("******************************\n");
            printf("Usage: find TARGET in SCOPE via MEANS ARG... [score SCORE]\n");
            printf("Execute lookup/searching operation based on mode\n");
            printf("Format:\n");
            printf("    TARGET := entry | file\n");
            printf("    SCOPE  := locator | pathinFS\n");
            printf("    MEANS  := file | entry | value\n");
            printf("    ARG... -  see below\n");
            printf("    SCORE  := value in range of 0.0 - 100.0\n");
            printf("        minimum score of matching\n");
            printf("        defaults to 100.0\n");
            printf("        only precise to 2 decimal places\n");
            printf("\n");
            printf("    Only the following combinations are valid\n");
            printf("        entry in locator via file ARG1 ARG2...\n");
            printf("                supply a file to ffprinter in ARG1\n");
            printf("                indicate which fields to use in ARG2...\n");
            printf("                and find entry with matching fingerprints\n");
            printf("                defaults to using all possible fields\n");
            printf("                see list [1] for possible fields\n");
            printf("\n");
            printf("        entry in locator via entry ARG1 ARG2...\n");
            printf("                supply a entry locator in ARG1\n");
            printf("                indicate which fields to use in ARG2...\n");
            printf("                and find entry with matching fingerprints\n");
            printf("                this is equivalent to finding duplicates\n");
            printf("                see list [1] for possible fields\n");
            printf("\n");
            printf("        entry in locator via value ARG...\n");
            printf("                supply keyval pairs to ffprinter in ARG...\n");
            printf("                and find entry with matching fingerprints\n");
            printf("                see list [2] for possible keyval pairs\n");
            printf("\n");
            printf("        file in pathinFS via entry ARG1 ARG2...\n");
            printf("                supply a entry locator in ARG1\n");
            printf("                indicate which fields to use in ARG2...\n");
            printf("                and find file with matching fingerprints\n");
            printf("                see list [1] for possible fields\n");
            printf("\n");
            printf("        list [1] possible fields:\n");
            printf("              all       - use all following fields\n");
            printf("            e:name      - name of entry\n");
            printf("            f:size      - file size\n");
            printf("            f:extr      - file extracts recorded in entry\n");
            printf("                          (not used when used to find entry via entry)\n");
            printf("            f:sha1      - file sha1 checksum\n");
            printf("            f:sha256    - file sha256 checksum\n");
            printf("            f:sha512    - file sha512 checksum\n");
            printf("            s:extr      - section extracts\n");
            printf("                          (not used when used to find entry via entry)\n");
            printf("            s:sha1      - section sha1 checksum\n");
            printf("            s:sha256    - section sha256 checksum\n");
            printf("            s:sha512    - section sha512 checksum\n");
            printf("\n");
            printf("        list [2] possible keys and value formats:\n");
            printf("            e:name          - name of entry\n");
            printf("                format : name\n");
            printf("            e:timeaddutc    - time of addition in UTC\n");
            printf("                format : YYYY/MM/DD_hh:mm\n");
            printf("            e:timeaddloc    - time of addition in local timezone\n");
            printf("                format : YYYY/MM/DD_hh:mm\n");
            printf("            e:timemodutc    - time of addition in UTC\n");
            printf("                format : YYYY/MM/DD_hh:mm\n");
            printf("            e:timemodloc    - time of addition in local timezone\n");
            printf("                format : YYYY/MM/DD_hh:mm\n");
            printf("            e:timeusrutc    - time of addition in UTC\n");
            printf("                format : YYYY/MM/DD_hh:mm\n");
            printf("            e:timeusrloc    - time of addition in local timezone\n");
            printf("                format : YYYY/MM/DD_hh:mm\n");
            printf("\n");
            printf("            Search syntax for date time (examples):\n");
            printf("                before some date:\n");
            printf("                    <2016/8/9\n");
            printf("                    <=2016/8/9\n");
            printf("                after some date:\n");
            printf("                    >2016/8/9\n");
            printf("                    >=2016/8/9\n");
            printf("                within range (inclusive):\n");
            printf("                    \"2016/1/1 .. 2016/12/31\"\n");
            printf("\n");
            printf("            e:tag           - tags for indexing entry\n");
            printf("                format : |tag1|tag2|tag3|, | can be escaped using \\\n");
            printf("\n");
            printf("            f:size          - file size\n");
            printf("            f:sha1          - sha1 checksum of file\n");
            printf("            f:sha256        - sha256 checksum of file\n");
            printf("            f:sha512        - sha512 checksum of file\n");
            printf("\n");
            printf("            s:sha1          - sha1 checksum of section\n");
            printf("            s:sha256        - sha256 checksum of section\n");
            printf("            s:sha512        - sha512 checksum of section\n");
            printf("******************************\n");
        }
        else if (   strcmp(str, "cmp")      == 0) {
            printf("******************************\n");
            printf("******************************\n");
        }
        /* == data collection == */
        else if (   strcmp(str, "fp")       == 0) {
            printf("******************************\n");
            printf("Usage: fp [OPT] mode targetinFS dir\n");
            printf("fingerprints targetinFS and stores resulted entries in dir\n");
            printf("Format:\n");
            printf("    mode := new | update\n");
            printf("    OPT  :\n");
            printf("        -r              recursive\n");
            printf("        --depth N       depth to traverse\n");
            printf("\n");
            printf("        --name          include file name\n");
            printf("        --f:size        include file size\n");
            printf("        --f:extr        include file extracts\n");
            printf("        --f:sha1        include file wise sha1 checksum\n");
            printf("        --f:sha256      include file wise sha256 checksum\n");
            printf("        --f:sha512      include file wise sha512 checksum\n");
            printf("\n");
            printf("        --s:extr        include section extracts\n");
            printf("        --s:sha1        include file wise sha1 checksum\n");
            printf("        --s:sha256      include file wise sha256 checksum\n");
            printf("        --s:sha512      include file wise sha512 checksum\n");
            printf("\n");
            printf("Note:\n");
            printf("    If entryname is absent and name is included\n");
            printf("    targetinFS will be used as entry name\n");
            printf("    if entryname is absent and name is not included\n");
            printf("    the replacement name will be used as entry name\n");
            printf("******************************\n");
        }
        /* == file system == */
        else if (   strcmp(str, "fpwd")     == 0) {
            printf("******************************\n");
            printf("Usage: fpwd\n");
            printf("Displays ffprinter's current working directory in file system\n");
            printf("******************************\n");
        }
        else if (   strcmp(str, "fls")      == 0) {
            printf("******************************\n");
            printf("Usage: fls [opt] [dir]\n");
            printf("List content in directory\n");
            printf("Format:\n");
            printf("    opt:\n");
            printf("        -a      show all\n");
            printf("        -A      show almost all(hide . and ..)\n");
            printf("\n");
            printf("    dir defaults to \".\", if unspecified\n");
            printf("******************************\n");
        }
        else if (   strcmp(str, "fcd")      == 0) {
            printf("******************************\n");
            printf("Usage: fcd dir\n");
            printf("Change to specified directory\n");
            printf("******************************\n");
        }
        /* == misc == */
        else if (   strcmp(str, "scanmem")  == 0) {
            printf("******************************\n");
            printf("Usage: scanmem scope mode pattern\n");
            printf("Format:\n");
            printf("    scope   := all | db name | stack size\n");
            printf("    mode    := hex | str | str0\n");
            printf("\n");
            printf("    For scope db name\n");
            printf("    if name is missing in db:name, current db you're residing in\n");
            printf("    will be used\n");
            printf("    This scans the db object itself, all layer 2 arrays\n");
            printf("    and all memory allocation records tracked that are linked\n");
            printf("    to the database\n");
            printf("\n");
            printf("    For scope stack size\n");
            printf("    size refers to size in KiB to scan in stack\n");
            printf("    this will affect how many recursive calls to make\n");
            printf("    to traverse into the stack for scanning\n");
            printf("    (the scanned area will be close to , but not exactly same as\n");
            printf("    the specified size)\n");
            printf("\n");
            printf("    If the size is too large, it is possible that you will crash\n");
            printf("    ffprinter due to memory access violation\n");
            printf("    Please save all your work prior to doing this\n");
            printf("\n");
            printf("    Note that it is likely that some portion of original content\n");
            printf("    on stack is overwritten while it's scanning\n");
            printf("\n");
            printf("    For hex mode, pattern of hex digits can be space separated\n");
            printf("    but must be quoted\n");
            printf("    and any hex prefix(0x) is ignored\n");
            printf("    hex digits are read case-insensitive\n");
            printf("    i.e. the following are exactly the same\n");
            printf("    scanmem scope hex 0xABCD\n");
            printf("    scanmem scope hex \"0xAB 0xCD\"\n");
            printf("    scanmem scope hex \"0xAB CD\"\n");
            printf("    scanmem scope hex \"AB 0xCD\"\n");
            printf("    scanmem scope hex ABCD\n");
            printf("    scanmem scope hex aBcD\n");
            printf("    scanmem scope hex abcd\n");
            printf("    scanmem scope hex \"a     b     c     d\"\n");
            printf("\n");
            printf("    For string mode,\n");
            printf("    str vs str0,\n");
            printf("    null terminator is not matched in str mode\n");
            printf("    To match null terminator as well, use str0 mode\n");
            printf("******************************\n");
        }
        else if (   strcmp(str, "exit")     == 0) {
            printf("******************************\n");
            printf("Usage: exit\n");
            printf("Cleans up resources and terminate ffprinter\n");
            printf("******************************\n");
        }
        else {
            printf("Unknown command/topic\n");
        }
    }

    return 0;
}

int set (term_info* info, dir_info* dir, int argc, char* argv[]) {


    return 0;
}

/*int scanmem(term_info* info, dir_info* dir, int argc, char* argv[]) {
    int i;

    const int SCOPE_ALL = 0, SCOPE_DB = 1, SCOPE_STACK = 2;
    const int MODE_HEX  = 0, MODE_STR = 1, MODE_STR0   = 2;

    int mode;
    int scope;

    char* str;
    char* name;
    char* num;

    char pattern_string[SCANMEM_PATTERN_MAX+1];
    str_len_int pattern_string_index = 0;
    unsigned char pattern_bytes[SCANMEM_PATTERN_MAX];
    str_len_int pattern_bytes_index = 0;

    uint32_t depth;

    str_len_int str_len;

    if (argc < 3) {
        printf("scanmem : too few arguments\n");
        return WRONG_ARGS;
    }

    // determine scope
    str = argv[0];
    if (        strcmp(str, "all")      == 0) {
        scope = SCOPE_ALL;
    }
    else if (   strncmp(str, "db", strlen("db"))    == 0) {
        // check if the colon exists
        str_len = strlen(str);
        for (i = 0; i < str_len; i++) {
            if (str[i] == ':') {
                name = str + i + 1;
                break;
            }
        }
        if (i == str_len) {
            printf("scanmem : \":\" missing in %s\n", argv[0]);
            return WRONG_ARGS;
        }
        // look up database
        lookup_db_name(dir->root->db, name, &temp_dh_find);
        if (!temp_dh_find) {
            printf("scanmem : no such database : %s\n", name);
            return WRONG_ARGS;
        }
        scope = SCOPE_DB;
    }
    else if (   strncmp(str, "stack", strlen("stack"))  == 0) {
        // check if colon exists
        str_len = strlen(str);
        for (i = 0; i < str_len; i++) {
            if (str[i] == ':') {
                num = str + i + 1;
                break;
            }
        }
        if (i == str_len) {
            printf("scanmem : \":\" missing in %s\n", str);
            return WRONG_ARGS;
        }
        // parse depth
        if (sscanf(num, "%"PRIu32"", &depth) != 1) {
            printf("scanmem : invalid depth\n");
            return WRONG_ARGS;
        }
        scope = SCOPE_STACK;
    }
    else {
        printf("scanmem : invalid scope\n");
        return WRONG_ARGS;
    }

    // determine mode
    str = argv[1];
    if (        strcmp(str, "hex")      == 0) {
        mode = MODE_HEX;
    }
    else if (   strcmp(str, "str")      == 0) {
        mode = MODE_STR;
    }
    else if (   strcmp(str, "str0")     == 0) {
        mode = MODE_STR0;
    }
    else {
        printf("scanmem : invalid mode\n");
        return WRONG_ARGS;
    }

    // stitch up the byte array or string
    if (mode == MODE_HEX) {
        for (i = 2; i < argc; i++) {
            str = argv[i];
            str_len = strlen(str);
            for (j = 0; j < str_len; j++) {
                if (str[j] == '0' && str[j+1]
            }
        }
    }
    else if (mode == MODE_STR || mode == MODE_STR0) {
        if (argc > 3) {
            printf("scanmem : too many strings\n");
            return WRONG_ARGS;
        }
    }

    return 0;
}*/

int ffp_exit(term_info* info, dir_info* dir, int argc, char* argv[]) {
    database_handle* iter_dh, * iter_dh_temp;

    uint32_t unsaved_db_count = 0;
    uint32_t db_count = 0;

    const int buf_size = 10;
    char buf[buf_size+1];
    int i, c;

    HASH_ITER(hh, dir->root->db, iter_dh, iter_dh_temp) {
        if (iter_dh->unsaved) {
            unsaved_db_count++;
        }
        db_count++;
    }

    if (unsaved_db_count > 0) {
        printf("There are %"PRIu32" databases with unsaved changes\n", unsaved_db_count);
        printf("Type exit again to confirm and force exit : ");

        i = 0;
        while ((i < buf_size) && (c = getchar()) != EOF && c != '\n') {
            buf[i++] = c;
        }
        buf[i] = 0;

        if (c != '\n') {
            discard_input_buffer();
        }

        if (strcmp(buf, "exit") != 0) {
            printf("exit cancelled\n");
            return 0;
        }
    }

    // cleanup
    printf("cleaning up databases\n");
    HASH_ITER(hh, dir->root->db, iter_dh, iter_dh_temp) {
        db_count--;
        printf("cleaning up database : %s%.*s %*"PRIu32" left\n", iter_dh->name, calc_pad(iter_dh->name, PRINT_PAD_SIZE), space_pad, NUMBER_SHIFT, db_count);

        HASH_DEL(dir->root->db, iter_dh);

        fres_database_handle(iter_dh);

        mem_wipe_sec(iter_dh, sizeof(database_handle));

        free(iter_dh);
    }

    printf("cleaning up root_dir\n");
    mem_wipe_sec(dir->root, sizeof(root_dir));

    printf("cleaning up dir_info\n");
    mem_wipe_sec(dir, sizeof(dir_info));

    printf("cleaning up term info\n");
    mem_wipe_sec
        (
         info->map_buf.base,

         sizeof(map_block)
         * get_bitmap_map_block_number(info->map_buf.length)
        );
    mem_wipe_sec
        (
         info->map_buf2.base,

         sizeof(map_block)
         * get_bitmap_map_block_number(info->map_buf2.length)
        );
    mem_wipe_sec(info, sizeof(term_info));

    printf("clean up complete\n");

    return QUIT_REQUESTED;
}

int fpwd(term_info* info, dir_info* dir, int argc, char* argv[]) {  // ignore all arguments
    char cwd[1024];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        printf("fpwd : failed to get current directory\n");
    }
    else {
        printf("%s\n", cwd);
    }

    return 0;
}

int fls_cleanup() {
    if (fls_dirp) {
        return closedir(fls_dirp);
    }

    return 0;
}

int fls(term_info* info, dir_info* dir, int argc, char* argv[]) {
    struct dirent* dp;

    char* path;

    char cur_path[] = ".";

    int i, j;

    int tar_count = 0;

    unsigned char opt_flag[FLS_OPT_NUM];

    for (i = 0; i < FLS_OPT_NUM; i++) {
        opt_flag[i] = 0;
    }

    for (i = 0; i < argc; i++) {
        if (IS_CHAR_OPTION(argv[i])) {
            for (j = 1; j < strlen(argv[i]); j++) {
                switch (argv[i][j]) {
                    case 'a' :
                        opt_flag[FLS_OPT_a] = 1;
                        break;
                    case 'A' :
                        opt_flag[FLS_OPT_A] = 1;
                        break;
                    default :
                        printf("fls : unknown option\n");
                        return NO_SUCH_OPT;
                }
            }
        }
        else if (IS_WORD_OPTION(argv[i])) {
            printf("fls : unknown option\n");
            return NO_SUCH_OPT;
        }
        else {
            tar_count++;
        }
    }

    if (tar_count == 0) {
        path = cur_path;
        i = argc;
        goto list_dir;
    }

    for (i = 0; i < argc; i++) {
        if (IS_OPTION(argv[i])) {
            continue;
        }

        path = argv[i];

list_dir:

        SET_NOT_INTERRUPTABLE();

        fls_dirp = opendir(path);

        MARK_NEED_CLEANUP();

        SET_INTERRUPTABLE();

        if (fls_dirp == NULL) {
            printf("fls : no such file or directory\n");
            continue;
        }

        while ((dp = readdir(fls_dirp))) {
            if (dp->d_name[0] == '.') {
                if (       !opt_flag[FLS_OPT_A]
                        && !opt_flag[FLS_OPT_a]
                   )
                {
                    continue;
                }

                if (        strcmp(dp->d_name, ".")
                        ||  strcmp(dp->d_name, "..")
                   )
                {
                    if (!opt_flag[FLS_OPT_a]) {
                        continue;
                    }
                }
            }

            printf("%s\n", dp->d_name);
        }

        SET_NOT_INTERRUPTABLE();

        closedir(fls_dirp);

        MARK_NO_NEED_CLEANUP();

        SET_INTERRUPTABLE();
    }

    return 0;
}

int fcd(term_info* info, dir_info* dir, int argc, char* argv[]) {
    int i, j;

    int tar_count;

    int tar_index;

    unsigned char opt_flag[FCD_OPT_NUM];

    for (i = 0; i < FCD_OPT_NUM; i++) {
        opt_flag[i] = 0;
    }

    tar_count = 0;
    for (i = 0; i < argc; i++) {
        if (IS_CHAR_OPTION(argv[i])) {
            for (j = 1; j < strlen(argv[i]); j++) {
                printf("fcd : unknown option\n");
                return NO_SUCH_OPT;
            }
        }
        else if (IS_WORD_OPTION(argv[i])) {
            printf("fcd : unknown option\n");
            return NO_SUCH_OPT;
        }
        else {
            tar_count++;
            tar_index = i;
        }
    }

    if (tar_count > 1) {
        printf("fcd : too many targets\n");
        return WRONG_ARGS;
    }

    if (chdir(argv[tar_index])) {
        perror("fcd ");
        return FFP_GENERAL_FAIL;
    }

    return 0;
}

int fp_cleanup() {
    dirp_record* temp_dirp_record;

    int ret = 0;

    bit_index i;
    int j;

    if (chdir(cwd_backup)) {
        perror("fp_cleanup ");
        ret = FFP_GENERAL_FAIL;
    }

    if (file_being_used) {
        if (fclose(file_being_used)) {
            perror("fp_cleanup ");
            ret =  FFP_GENERAL_FAIL;
        }

        file_being_used = NULL;
    }

    if (entry_being_used) {
        if (!dh_being_used) {
            printf("fp_cleanup : dh being used not recorded, but an entry was recorded for cleanup\n");
            ret = FFP_GENERAL_FAIL;
        }

        if ((ret = del_entry(dh_being_used, entry_being_used))) {
            printf("fp_cleanup : failed to delete entry\n");
            printf("fp_cleanup : error code : %d\n", ret);
            ret = FFP_GENERAL_FAIL;
        }

        dh_being_used = NULL;
        entry_being_used = NULL;
    }

    for (i = 0; i < max_dirp_record_index; i++) {
        j = get_dirp_record_from_layer2_arr(&l2_dirp_record_arr, &temp_dirp_record, i);

        if (j != 0) {
            continue;
        }

        if ((j = closedir(temp_dirp_record->dirp))) {
            printf("fp_cleanup : failed to close directory\n");
            printf("fp_cleanup : error code : %d\n", j);

            ret = FFP_GENERAL_FAIL;
        }

        del_dirp_record_from_layer2_arr(&l2_dirp_record_arr, temp_dirp_record->obj_arr_index);
    }

    max_dirp_record_index = 0;

    return ret;
}

int fp(term_info* info, dir_info* dir, int argc, char* argv[]) {
    int i, j;

    int ret;

    char* str;

    unsigned char fs_tar_index_set = 0;
    int fs_tar_index;
    unsigned char entry_tar_index_set = 0;
    int entry_tar_index;

    linked_entry* tar_entry;
    database_handle* tar_dh;

    dir_info result_dir;

    int tar_count;

    error_handle er_h;

    uint32_t flags = 0x0;
    unsigned char flags_modifier_specified = 0;

    ffp_eid_int depth;
    ffp_eid_int* depth_p;
    unsigned char depth_specified = 0;

    unsigned char opt_flag[FP_OPT_NUM];

    error_mark_owner(&er_h, "fp");

    for (i = 0; i < FP_OPT_NUM; i++) {
        opt_flag[i] = 0;
    }

    tar_count = 0;
    for (i = 0; i < argc; i++) {
        if (IS_CHAR_OPTION(argv[i])) {
            for (j = 1; j < strlen(argv[i]); j++) {
                switch (argv[i][j]) {
                    case 'r' :
                        opt_flag[FP_OPT_r] = 1;
                        break;
                    default :
                        printf("fp : unknown option\n");
                        return NO_SUCH_OPT;
                }
            }
        }
        else if (IS_WORD_OPTION(argv[i])) {
            str = argv[i] + 2;
            if (        strcmp(str, "depth")        == 0) {
                if (i + 1 >= argc) {
                    printf("fp : please specify depth\n");
                    return WRONG_ARGS;
                }
                else {
                    if (sscanf(argv[i+1], "%"PRIu64"", &depth) != 1) {
                        printf("fp : invalid depth\n");
                        return WRONG_ARGS;
                    }
                    depth_specified = 1;

                    i++;
                }
            }
            else if (   strcmp(str, "name")         == 0) {
                flags |= FPRINT_USE_F_NAME;
                flags_modifier_specified = 1;
            }
            else if (   strcmp(str, "f:size")       == 0) {
                flags |= FPRINT_USE_F_SIZE;
                flags_modifier_specified = 1;
            }
            else if (   strcmp(str, "f:extr")       == 0) {
                flags |= FPRINT_USE_F_EXTR;
                flags_modifier_specified = 1;
            }
            else if (   strcmp(str, "f:sha1")       == 0) {
                flags |= FPRINT_USE_F_SHA1;
                flags_modifier_specified = 1;
            }
            else if (   strcmp(str, "f:sha256")     == 0) {
                flags |= FPRINT_USE_F_SHA256;
                flags_modifier_specified = 1;
            }
            else if (   strcmp(str, "f:sha512")     == 0) {
                flags |= FPRINT_USE_F_SHA512;
                flags_modifier_specified = 1;
            }
            else if (   strcmp(str, "f:allsum")     == 0) {     // awesome, hahahahahaha... okay i will walk myself out
                flags |=    FPRINT_USE_F_SHA1
                    |       FPRINT_USE_F_SHA256
                    |       FPRINT_USE_F_SHA512;
                flags_modifier_specified = 1;
            }
            else if (   strcmp(str, "s:extr")   == 0) {
                flags |= FPRINT_USE_S_EXTR;
                flags |= FPRINT_USE_F_SIZE;
                flags_modifier_specified = 1;
            }
            else if (   strcmp(str, "s:sha1")   == 0) {
                flags |= FPRINT_USE_S_SHA1;
                flags |= FPRINT_USE_F_SIZE;
                flags_modifier_specified = 1;
            }
            else if (   strcmp(str, "s:sha256")   == 0) {
                flags |= FPRINT_USE_S_SHA256;
                flags |= FPRINT_USE_F_SIZE;
                flags_modifier_specified = 1;
            }
            else if (   strcmp(str, "s:sha512")   == 0) {
                flags |= FPRINT_USE_S_SHA512;
                flags |= FPRINT_USE_F_SIZE;
                flags_modifier_specified = 1;
            }
            else if (   strcmp(str, "s:allsum")   == 0) {
                flags |=    FPRINT_USE_S_SHA1
                    |       FPRINT_USE_S_SHA256
                    |       FPRINT_USE_S_SHA512;
                flags |= FPRINT_USE_F_SIZE;
                flags_modifier_specified = 1;
            }
            else {
                printf("fp : unknown option\n");
                return NO_SUCH_OPT;
            }
        }
        else {
            tar_count++;
            if (!fs_tar_index_set) {
                fs_tar_index = i;
                fs_tar_index_set = 1;
            }
            else if (!entry_tar_index_set) {
                entry_tar_index = i;
                entry_tar_index_set = 1;
            }
        }
    }

    if (tar_count < 1) {
        printf("fp : too few targets\n");
        return WRONG_ARGS;
    }
    else if (tar_count > 2) {
        printf("fp : too many targets\n");
        return WRONG_ARGS;
    }

    // initialise loose resource pointer
    dh_being_used = NULL;
    entry_being_used = NULL;
    file_being_used = NULL;
    max_dirp_record_index = 0;

    // default to using everything
    if (!flags_modifier_specified) {
        flags = FPRINT_USE_F_NAME
            |   FPRINT_USE_F_SIZE
            |   FPRINT_USE_F_EXTR
            |   FPRINT_USE_F_SHA1
            |   FPRINT_USE_F_SHA256
            |   FPRINT_USE_F_SHA512
            |   FPRINT_USE_S_EXTR
            |   FPRINT_USE_S_SHA1
            |   FPRINT_USE_S_SHA256
            |   FPRINT_USE_S_SHA512;
    }

    // backup current working directory
    if (getcwd(cwd_backup, sizeof(cwd_backup)) == NULL) {
        printf("fp : failed to backup current directory\n");
        return FFP_GENERAL_FAIL;
    }

    ret = update_dir_pointers(dir, &er_h);
    if (ret) {
        error_print_owner_msg(&er_h);
        error_mark_inactive(&er_h);
        return ret;
    }

    if (!entry_tar_index_set) {     // no destination entry specified, default to current entry
        copy_dir(&result_dir, dir);
    }
    else {      // parse locator
        ret = locator_to_dir(info, dir, argv[entry_tar_index], &result_dir, &er_h);
        switch (ret) {
            case 0:
                break;
            default:
                error_print_owner_msg(&er_h);
                error_mark_inactive(&er_h);
                return ret;
        }
    }

    if (is_pointing_to_root(&result_dir)) {
        printf("fp : cannot put resulted entries in root directory\n");
        return WRONG_ARGS;
    }

    tar_dh = result_dir.dh;
    tar_entry = result_dir.entry;

    if (depth_specified) {
        depth_p = &depth;
    }
    else {
        depth_p = NULL;
    }

    // mark needing cleanup before calling gen tree
    MARK_NEED_CLEANUP();

    dh_being_used = tar_dh;

    ret = gen_tree(tar_dh, argv[fs_tar_index], tar_entry, flags, opt_flag[FP_OPT_r], depth_p, &er_h, &entry_being_used, &file_being_used, &l2_dirp_record_arr, &max_dirp_record_index);
    if (ret) {
        error_print_owner_msg(&er_h);
        error_mark_inactive(&er_h);
        return ret;
    }

    // everything finished successfully, no need for cleanup
    MARK_NO_NEED_CLEANUP();

    return 0;
}

/* local macros */
#define find_parse_fields() \
    for (   /* no initialisation needed */;                         \
            cur_indx <= argv_end_indx;   /* inclusive */            \
            cur_indx++                                              \
        )                                                           \
    {                                                               \
        str = argv[cur_indx];                                       \
        if (        strcmp(str, "all")      == 0) {                 \
            for (i = 0; i < FIND_FIELD_NUM; i++) {                  \
                field_specified[i] = 1;                             \
            }                                                       \
            break;                                                  \
        }                                                           \
        else if (   strcmp(str, "name")     == 0) {                 \
            if (field_specified[FIND_FIELD_E_NAME]) {               \
                printf("find : field \"name\" already specified\n");\
                return WRONG_ARGS;                                  \
            }                                                       \
            field_specified[FIND_FIELD_E_NAME] = 1;                 \
        }                                                           \
        else if (   strcmp(str, "f:size")   == 0) {                 \
            if (field_specified[FIND_FIELD_F_SIZE]) {               \
                printf("find : field \"f:size\" already specified\n");\
                return WRONG_ARGS;                                  \
            }                                                       \
            field_specified[FIND_FIELD_F_SIZE] = 1;                 \
        }                                                           \
        else if (   strcmp(str, "f:sha1")   == 0) {                 \
            if (field_specified[FIND_FIELD_F_SHA1]) {               \
                printf("find : field \"f:sha1\" already specified\n");\
                return WRONG_ARGS;                                  \
            }                                                       \
            field_specified[FIND_FIELD_F_SHA1] = 1;                 \
        }                                                           \
        else if (   strcmp(str, "f:sha256") == 0) {                 \
            if (field_specified[FIND_FIELD_F_SHA256]) {             \
                printf("find : field \"f:sha256\" already specified\n");\
                return WRONG_ARGS;                                  \
            }                                                       \
            field_specified[FIND_FIELD_F_SHA256] = 1;               \
        }                                                           \
        else if (   strcmp(str, "f:sha512") == 0) {                 \
            if (field_specified[FIND_FIELD_F_SHA512]) {             \
                printf("find : field \"f:sha512\" already specified\n");\
                return WRONG_ARGS;                                  \
            }                                                       \
            field_specified[FIND_FIELD_F_SHA512] = 1;               \
        }                                                           \
        else if (   strcmp(str, "s:sha1")   == 0) {                 \
            if (field_specified[FIND_FIELD_S_SHA1]) {               \
                printf("find : field \"s:sha1\" already specified\n");\
                return WRONG_ARGS;                                  \
            }                                                       \
            field_specified[FIND_FIELD_S_SHA1] = 1;                 \
        }                                                           \
        else if (   strcmp(str, "s:sha256") == 0) {                 \
            if (field_specified[FIND_FIELD_S_SHA256]) {             \
                printf("find : field \"s:sha256\" already specified\n");\
                return WRONG_ARGS;                                  \
            }                                                       \
            field_specified[FIND_FIELD_S_SHA256] = 1;               \
        }                                                           \
        else if (   strcmp(str, "s:sha512") == 0) {                 \
            if (field_specified[FIND_FIELD_S_SHA512]) {             \
                printf("find : field \"s:sha512\" already specified\n");\
                return WRONG_ARGS;                                  \
            }                                                       \
            field_specified[FIND_FIELD_S_SHA512] = 1;               \
        }                                                           \
        else {                                                      \
            printf("find : unknown field : %s\n", str);             \
            return WRONG_ARGS;                                      \
        }                                                           \
    }

int find(term_info* info, dir_info* dir, int argc, char* argv[]) {
    int i;
    int ret;

    const char time_format[] = "%Y-%m-%d_%H:%M:%S";

    int argv_end_indx;  // inclusive
    int cur_indx;

    int keyval_indx;

    char* str;

    char* path;

    struct stat file_stat;

    float score_float;
    unsigned int score_uint;

    dir_info result_dir;

    error_handle er_h;

    unsigned char target;

    unsigned char field_specified[FIND_FIELD_NUM];

    field_record rec;

    struct tm temp_time;

    mem_wipe_sec(field_specified, sizeof(field_specified));

    error_mark_owner(&er_h, "find");

    mem_wipe_sec(&rec, sizeof(field_record));

    if (argc < 6) {
        printf("find : too few arguments\n");
        return WRONG_ARGS;
    }

    // check if score is specified
    if (strcmp(argv[(argc-1) -1], "score")  == 0) {
        if (argc-2 < 6) {
            printf("find : too few arguments\n");
            return WRONG_ARGS;
        }

        // parse score
        if (sscanf(argv[argc-1], "%f", &score_float) != 1) {
            printf("find : invalid score\n");
            return WRONG_ARGS;
        }

        if (score_float < 0.0 || 100.0 < score_float) {
            printf("find : score out of range\n");
            return WRONG_ARGS;
        }

        // convert to integer
        score_uint = score_float * 100; // precise to 2 decimal place
        if (score_uint > 100 * 100) {
            printf("find : score out of range\n");
            return WRONG_ARGS;
        }

        argv_end_indx = (argc-1) - 2;
    }
    else {
        argv_end_indx = argc - 1;
    }

    cur_indx = 0;

    // parse target
    str = argv[cur_indx];
    if (        strcmp(str, "entry")    == 0) {
        target = FIND_TARGET_ENTRY;
    }
    else if (   strcmp(str, "file")     == 0) {
        target = FIND_TARGET_FILE;
    }
    else {
        printf("find : unknown target\n");
        return WRONG_ARGS;
    }

    cur_indx++;

    // check for the word "in"
    str = argv[cur_indx];
    if (strcmp(str, "in") != 0) {
        printf("find : syntax error - missing \"in\"\n");
        return WRONG_ARGS;
    }

    cur_indx++;

    // parse scope
    str = argv[cur_indx];
    if (target == FIND_TARGET_ENTRY) {
        // parse locator
        ret = locator_to_dir(info, dir, str, &result_dir, &er_h);
        switch (ret) {
            case 0:
                // no error
                break;
            default:
                error_print_owner_msg(&er_h);
                error_mark_inactive(&er_h);
                return ret;
        }
    }
    else if (target == FIND_TARGET_FILE) {
        // check if path is directory
        path = str;
        if (stat(path, &file_stat)) {
            printf("find : failed to get stats of file - file may not exist");
            return FS_FILE_ACCESS_FAIL;
        }

        if (        S_ISDIR(file_stat.st_mode)) {
            ;;  // all good
        }
        else if (   S_ISREG(file_stat.st_mode)) {
            printf("find : specified a file instead of a directory for scope\n");
            printf("       please use cmp instead for file vs entry comparison\n");
            printf("       (see help cmp)\n");
            return WRONG_ARGS;
        }
        else {
            printf("find : unsupported file type\n");
            return WRONG_ARGS;
        }
    }
    else {
        printf("find : logical error\n");
        return LOGIC_ERROR;
    }

    cur_indx++;

    // check for the word "via"
    str = argv[cur_indx];
    if (strcmp(str, "via") != 0) {
        printf("find : syntax error - missing \"via\"\n");
        return WRONG_ARGS;
    }

    cur_indx++;

    // parse means
    str = argv[cur_indx];
    if (        strcmp(str, "file")     == 0) {

        switch (target) {
            case FIND_TARGET_ENTRY:     // find entry via file
                // confirm there are at least 2 arguments remaining
                if (argv_end_indx - cur_indx < 2) {
                    printf("find : too few arguments\n");
                    return WRONG_ARGS;
                }

                cur_indx++;

                // parse path
                str = argv[cur_indx];
                path = str;
                if (stat(path, &file_stat)) {
                    printf("find : failed to get stats - file may not exist\n");
                    return WRONG_ARGS;
                }

                // check if file type is supported
                if (        !S_ISDIR(file_stat.st_mode)
                        &&  !S_ISREG(file_stat.st_mode)
                   )
                {
                    printf("find : unsupported file type\n");
                    return WRONG_ARGS;
                }

                cur_indx++;

                // parse fields
                find_parse_fields();

                // fill in field record
                // does NOT use extract fields
                // does NOT use section fields
                if (field_specified[FIND_FIELD_E_NAME]) {
                    rec.field_usage_map |= FIELD_REC_USE_E_NAME;
                }
                if (field_specified[FIND_FIELD_F_SIZE]) {
                    rec.field_usage_map |= FIELD_REC_USE_F_SIZE;
                }
                if (field_specified[FIND_FIELD_F_SHA1]) {
                    rec.field_usage_map |= FIELD_REC_USE_F_SHA1;
                }
                if (field_specified[FIND_FIELD_F_SHA256]) {
                    rec.field_usage_map |= FIELD_REC_USE_F_SHA256;
                }
                if (field_specified[FIND_FIELD_F_SHA512]) {
                    rec.field_usage_map |= FIELD_REC_USE_F_SHA512;
                }
                break;
            case FIND_TARGET_FILE:      // find file via file
                printf("find : cannot find file via file\n");
                printf("       please use external tools for this purpose, such as find\n");
                return WRONG_ARGS;
        }
    }
    else if (   strcmp(str, "entry")    == 0) {
        switch (target) {
            case FIND_TARGET_ENTRY:     // find entry via entry
                // confirm there are at least 2 arguments remaining
                if (argv_end_indx - cur_indx < 2) {
                    printf("find : too few arguments\n");
                    return WRONG_ARGS;
                }

                cur_indx++;

                // parse locator
                str = argv[cur_indx];
                ret = locator_to_dir(info, dir, str, &result_dir, &er_h);
                switch (ret) {
                    case 0:
                        break;
                    default:
                        error_print_owner_msg(&er_h);
                        error_mark_inactive(&er_h);
                        return ret;
                }

                cur_indx++;

                // parse fields
                find_parse_fields();

                // fill in field record
                // does NOT use extract fields
                if (field_specified[FIND_FIELD_E_NAME]) {
                    rec.field_usage_map |= FIELD_REC_USE_E_NAME;
                }
                if (field_specified[FIND_FIELD_F_SIZE]) {
                    rec.field_usage_map |= FIELD_REC_USE_F_SIZE;
                }
                if (field_specified[FIND_FIELD_F_SHA1]) {
                    rec.field_usage_map |= FIELD_REC_USE_F_SHA1;
                }
                if (field_specified[FIND_FIELD_F_SHA256]) {
                    rec.field_usage_map |= FIELD_REC_USE_F_SHA256;
                }
                if (field_specified[FIND_FIELD_F_SHA512]) {
                    rec.field_usage_map |= FIELD_REC_USE_F_SHA512;
                }
                if (field_specified[FIND_FIELD_S_SHA1]) {
                    rec.field_usage_map |= FIELD_REC_USE_S_SHA1;
                }
                if (field_specified[FIND_FIELD_S_SHA256]) {
                    rec.field_usage_map |= FIELD_REC_USE_S_SHA256;
                }
                if (field_specified[FIND_FIELD_S_SHA512]) {
                    rec.field_usage_map |= FIELD_REC_USE_S_SHA512;
                }
                break;
            case FIND_TARGET_FILE:      // find file via entry
                cur_indx++;

                // parse fields
                find_parse_fields();

                // fill in field record
                // use all fields
                if (field_specified[FIND_FIELD_E_NAME]) {
                    rec.field_usage_map |= FIELD_REC_USE_E_NAME;
                }
                if (field_specified[FIND_FIELD_F_SIZE]) {
                    rec.field_usage_map |= FIELD_REC_USE_F_SIZE;
                }
                if (field_specified[FIND_FIELD_F_SHA1]) {
                    rec.field_usage_map |= FIELD_REC_USE_F_SHA1;
                }
                if (field_specified[FIND_FIELD_F_SHA256]) {
                    rec.field_usage_map |= FIELD_REC_USE_F_SHA256;
                }
                if (field_specified[FIND_FIELD_F_SHA512]) {
                    rec.field_usage_map |= FIELD_REC_USE_F_SHA512;
                }
                if (field_specified[FIND_FIELD_S_SHA1]) {
                    rec.field_usage_map |= FIELD_REC_USE_S_SHA1;
                }
                if (field_specified[FIND_FIELD_S_SHA256]) {
                    rec.field_usage_map |= FIELD_REC_USE_S_SHA256;
                }
                if (field_specified[FIND_FIELD_S_SHA512]) {
                    rec.field_usage_map |= FIELD_REC_USE_S_SHA512;
                }
                break;
        }
    }
    else if (   strcmp(str, "value")    == 0) {
        switch (target) {
            case FIND_TARGET_ENTRY:     // find entry via value
                cur_indx++;

                // verify all keyval pairs
                for (keyval_indx = i; keyval_indx <= argv_end_indx; keyval_indx++) {
                    str = argv[keyval_indx];

                    ret = parse_keyval_pair(str, &has_arrow, &key, &val);
                    if (ret) {
                        if (!has_arrow) {
                            printf("find : arrow missing in \"%s\"\n", str);
                            return WRONG_ARGS;
                        }
                        else if (key == NULL && val == NULL) {
                            printf("find : both key and value are missing in \"%s\"\n", str);
                            return WRONG_ARGS;
                        }
                        else if (key == NULL) {
                            printf("find : key is missing in \"%s\"\n", str);
                            return WRONG_ARGS;
                        }
                        else if (val == NULL) {
                            printf("edit : val is missing in \"%s\"\n", str);
                            return WRONG_ARGS;
                        }
                    }

                    // check if keys exist and if values are valid
                    if (        strcmp(key, "e:name")           == 0) {
                        str_len = strlen(val);
                        if (str_len == 0) {
                            printf("find : name cannot be empty\n");
                            return WRONG_ARGS;
                        }
                        if (str_len > FILE_NAME_MAX) {
                            printf("find : name too long\n");
                            return WRONG_ARGS;
                        }

                        if (field_specified[FIND_FIELD_E_NAME]) {
                            printf("find : name already specified\n");
                            return WRONG_ARGS;
                        }
                        field_specified[FIND_FIELD_E_NAME] = 1;
                    }
                    else if (   strcmp(key, "e:timeaddutc")     == 0
                            ||  strcmp(key, "e:timeaddloc")     == 0
                            )
                    {
                        if (strptime(val, time_format, &temp_time) == NULL) {
                            printf("find : invalid date time format : %s\n", val);
                            return WRONG_ARGS;
                        }

                        if (field_specified[FIND_FIELD_E_TADD]) {
                            printf("find : time of addition already specified\n");
                            return WRONG_ARGS;
                        }
                        field_specified[FIND_FIELD_E_TADD] = 1;
                    }

                    repair_keyval_pair(has_arrow, key, val);
                }
                break;
            case FIND_TARGET_FILE:
                printf("find : cannot find file via value\n");
                return WRONG_ARGS;
        }
    }

    return 0;
}

int cmp(term_info* info, dir_info* dir, int argc, char* argv[]) {
    

    return 0;
}
