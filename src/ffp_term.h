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
#include "ffp_directory.h"
#include "ffp_file.h"
#include "ffp_error.h"
#include "ffp_fingerprint.h"
#include <signal.h>
#include <setjmp.h>
#include <readline/readline.h>
#include <readline/history.h>

#define PROMPT_STRING   "ffp > "

#define SETTING_NUM     2
#define SETTING_AMBIG   1

#define FUNC_NAME_MAX           40
#define FUNC_NUM_MAX            30

#define LOCATOR_LEN_MAX         200

#define ALIAS_NAME_MAX          50

#define TERM_LOOKUP_BUF_SIZE    50

#define PRINT_PAD_SIZE          35

#define NUMBER_SHIFT            6

#define LS_OPT_NUM      2
#define LS_OPT_l        0
#define LS_OPT_d        1

#define CD_OPT_NUM      0

#define CP_OPT_NUM      1
#define CP_OPT_r        2

#define RM_OPT_NUM      1
#define RM_OPT_r        0

#define MV_OPT_NUM      1
#define MV_OPT_r        0

#define SHOW_MAX_ARG    40
#define SHOW_ENTRY_MODE 0
#define SHOW_FILE_MODE  1
#define SHOW_SECT_MODE  2

#define UPDATE_ENTRY_MODE   0
#define UPDATE_FILE_MODE    1
#define UPDATE_SECT_MODE    2

#define MODIFY_WRITE_MODE   0
#define MODIFY_APPEND_MODE  1

#define EDIT_E_NUM      8
#define EDIT_E_NAME     0
#define EDIT_E_TADD     1
#define EDIT_E_TMOD     2
#define EDIT_E_TUSR     3
#define EDIT_E_TYPE     4
#define EDIT_E_MADEBY   5
#define EDIT_E_TAG      6
#define EDIT_E_MSG      7

#define EDIT_F_NUM      4
#define EDIT_F_SIZE     0
#define EDIT_F_SHA1     1
#define EDIT_F_SHA256   2
#define EDIT_F_SHA512   3

#define EDIT_S_NUM      4
#define EDIT_S_STARTPOS 0
#define EDIT_S_ENDPOS   1
#define EDIT_S_SHA1     1
#define EDIT_S_SHA256   2
#define EDIT_S_SHA512   3

#define FIND_TARGET_ENTRY   0
#define FIND_TARGET_FILE    1

#define FIND_FIELD_NUM          14
#define FIND_FIELD_E_NAME       0
#define FIND_FIELD_E_TADD       1
#define FIND_FIELD_E_TMOD       2
#define FIND_FIELD_E_TUSR       3
#define FIND_FIELD_E_TAG        4
#define FIND_FIELD_F_SIZE       5
#define FIND_FIELD_F_EXTR       6
#define FIND_FIELD_F_SHA1       7
#define FIND_FIELD_F_SHA256     8
#define FIND_FIELD_F_SHA512     9
#define FIND_FIELD_S_EXTR       10
#define FIND_FIELD_S_SHA1       11
#define FIND_FIELD_S_SHA256     12
#define FIND_FIELD_S_SHA512     13

#define ADD_OPT_NUM     16
#define ADD_OPT_man     0
#define ADD_OPT_v       1
#define ADD_OPT_o       2
#define ADD_OPT_h       3

#define FLS_OPT_NUM     5
#define FLS_OPT_a       0
#define FLS_OPT_A       1

#define FCD_OPT_NUM     2
#define FCD_OPT_L       0
#define FCD_OPT_P       1

#define FP_OPT_NUM      5
#define FP_OPT_r        0

#define MAKE_MODE_TOUCH     0
#define MAKE_MODE_MKDIR     1

// option related
#define IS_CHAR_OPTION(str)     ((str)[0] && (str)[1] && (str)[0] == '-' && (str)[1] != '-')
#define IS_WORD_OPTION(str)     ((str)[0] && (str)[1] && (str)[2] && (str)[0] == '-' && (str)[1] == '-')
#define IS_OPTION(str)          ((str)[0] && (str)[1] && (str)[0] == '-')

// locator related
#define IS_ALIAS(str)           ((str)[0] && (str)[1] && (str)[2] && (str)[0] == 'a' && (str)[1] == ':')
#define IS_DB_ID_PAIR(str)      ((str)[0] && (str)[1] && (str)[2] && (str)[0] == 'd' && (str)[1] == ':')
#define IS_ENTRY_ID(str)        ((str)[0] && (str)[1] && (str)[2] && (str)[0] == 'e' && (str)[1] == ':')
#define IS_FORCE_FILE(str)      ((str)[0] && (str)[1] && (str)[2] && (str)[0] == 'f' && (str)[1] == ':')
#define IS_ARROW(str)           ((str)[0] && (str)[1] && (str)[0] == '-' && (str)[1] == '>')
#define SKIP_PREFIX(str)        (str + 2)

#define ASK_FOR_ANS(ans, max_len, i, c)    do { \
            i = 0;              \
            while ((i < max_len) && (c = getchar()) != EOF && c != '\n') { \
                ans[i++] = c;   \
            }                   \
            ans[i] = 0;         \
        } while (0)

typedef struct str_to_func str_to_func;
typedef struct entry_alias entry_alias;
typedef struct term_info term_info;

struct str_to_func {
    char name[FUNC_NAME_MAX];
    int (*handler) (term_info* info, dir_info* dir, int argc, char* argv[]);
    unsigned char interruptable;
    int (*cleaner) ();
};

struct entry_alias {
    entry_alias* prev;
    entry_alias* next;

    char name[ALIAS_NAME_MAX];

    dir_info dir;

    UT_hash_handle hh;
};

struct term_info {
    str_to_func func_arr[FUNC_NUM_MAX];
    entry_alias* alias;

    /* setting */
    int setting[SETTING_NUM];

    /* buffers required for lookups */
    simple_bitmap map_buf;
    simple_bitmap map_buf2;

    field_match_bitmap match_map;

    /* pool structures */
    layer2_sect_field_arr l2_sect_field_arr;
};

int add_alias(term_info* info, entry_alias* alias);

int lookup_alias(term_info* info, const char* name, entry_alias** result_alias);

int func_match(term_info* info, char* str, int (**handler) (term_info* info, dir_info* dir, int argc, char* argv[]), unsigned char* interruptable, int (**cleaner) ());

int process_cmd(term_info* info, char* cmd, dir_info* dir);

void inthandler(int sig);

int prompt(term_info* info, dir_info* dir);

int add_func(term_info* info, char* name, int (*handler) (term_info* info, dir_info* dir, int argc, char* argv[]), unsigned char interruptable, int (*cleaner) ());

int init_func_arr (term_info* info);

int init_term_info (term_info* info);

int locator_to_dir(term_info* info, dir_info* dir, const char* locator, dir_info* ret_dir, error_handle* er_h);

// database related
int newdb       (term_info* info, dir_info* dir, int argc, char* argv[]);
int loaddb      (term_info* info, dir_info* dir, int argc, char* argv[]);
int unloaddb    (term_info* info, dir_info* dir, int argc, char* argv[]);
int savedb      (term_info* info, dir_info* dir, int argc, char* argv[]);
int savealldb   (term_info* info, dir_info* dir, int argc, char* argv[]);

// logical directory traversal related
int pwd         (term_info* info, dir_info* dir, int argc, char* argv[]);
int ls          (term_info* info, dir_info* dir, int argc, char* argv[]);
int cd          (term_info* info, dir_info* dir, int argc, char* argv[]);

int cp          (term_info* info, dir_info* dir, int argc, char* argv[]);
int rm          (term_info* info, dir_info* dir, int argc, char* argv[]);
int mv          (term_info* info, dir_info* dir, int argc, char* argv[]);

// entry related
int touch       (term_info* info, dir_info* dir, int argc, char* argv[]);
int ffp_mkdir   (term_info* info, dir_info* dir, int argc, char* argv[]);

int show        (term_info* info, dir_info* dir, int argc, char* argv[]);

int edit        (term_info* info, dir_info* dir, int argc, char* argv[]);
int verify      (term_info* info, dir_info* dir, int argc, char* argv[]);
int attach      (term_info* info, dir_info* dir, int argc, char* argv[]);
int detach      (term_info* info, dir_info* dir, int argc, char* argv[]);

int find        (term_info* info, dir_info* dir, int argc, char* argv[]);
int cmp         (term_info* info, dir_info* dir, int argc, char* argv[]);

// data collection related
int fp_cleanup();
int fp          (term_info* info, dir_info* dir, int argc, char* argv[]);
int fpwd        (term_info* info, dir_info* dir, int argc, char* argv[]);
int fls_cleanup();
int fls         (term_info* info, dir_info* dir, int argc, char* argv[]);
int fcd         (term_info* info, dir_info* dir, int argc, char* argv[]);

// misc
int help        (term_info* info, dir_info* dir, int argc, char* argv[]);
int scanmem     (term_info* info, dir_info* dir, int argc, char* argv[]);
int ffp_exit    (term_info* info, dir_info* dir, int argc, char* argv[]);
