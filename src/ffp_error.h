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

#include <stdio.h>

#ifndef FFP_EXCEPTION_H
#define FFP_EXCEPTION_H

/* all text are truncated if length exceeds maximum
 */

#define EXCEPTION_IDENTITY_MAX_LEN  50
#define EXCEPTION_MSG_MAX_LEN       100

#define error_mark_active(er_h) \
    (er_h)->active = 1

#define error_mark_inactive(er_h) \
    (er_h)->active = 0

typedef struct error_handle error_handle;

struct error_handle {
    unsigned char active;
    char owner_name[EXCEPTION_IDENTITY_MAX_LEN+1];
    char starter_name[EXCEPTION_IDENTITY_MAX_LEN+1];
    char msg[EXCEPTION_MSG_MAX_LEN+1];
};

int error_mark_owner(error_handle* er_h, const char* owner_name);

int error_mark_starter(error_handle* er_h, const char* starter_name);

int error_write(error_handle* er_h, const char* msg);

int error_print(error_handle* er_h);

int error_print_if_active(error_handle* er_h);

int error_print_owner_msg(error_handle* er_h);

int error_print_starter_msg(error_handle* er_h);

int error_print_owner_starter_msg(error_handle* er_h);

#endif
