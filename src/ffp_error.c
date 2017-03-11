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

#include "ffp_error.h"

int error_mark_owner(error_handle* er_h, const char* owner_name) {
    int i;

    // record owner name, truncate if too long
    for (   i = 0;
            owner_name[i]     && i < EXCEPTION_IDENTITY_MAX_LEN;
            i++
        )
    {
        er_h->owner_name[i] = owner_name[i];
    }
    er_h->owner_name[i] = 0;

    return 0;
}

int error_mark_starter(error_handle* er_h, const char* starter_name) {
    int i;

    // record starter name, truncate if too long
    for (   i = 0;
            starter_name[i]     && i < EXCEPTION_IDENTITY_MAX_LEN;
            i++
        )
    {
        er_h->starter_name[i] = starter_name[i];
    }
    er_h->starter_name[i] = 0;

    return 0;
}

int error_write(error_handle* er_h, const char* msg) {
    int i;

    // record message, truncate if too long
    for (   i = 0;
            msg[i]              && i < EXCEPTION_MSG_MAX_LEN;
            i++
        )
    {
        er_h->msg[i] = msg[i];
    }
    er_h->msg[i] = 0;

    error_mark_active(er_h);

    return 0;
}

int error_print(error_handle* er_h) {
    if (!er_h->active) {    // if not active
        printf("error inactive, owner : %s\n", er_h->owner_name);
    }
    else {  // if active
        printf(
                "error active, [OWNER : %s], [STARTER : %s], [MSG : %s]\n",
                er_h->owner_name,
                er_h->starter_name,
                er_h->msg
              );
    }

    return 0;
}

int error_print_if_active(error_handle* er_h) {
    if (er_h->active) {
        error_print(er_h);
    }

    return 0;
}

int error_print_owner_msg(error_handle* er_h) {
    if (er_h->active) {
        printf("%s : %s\n", er_h->owner_name, er_h->msg);
    }
    else {
        error_print(er_h);
    }

    return 0;
}

int error_print_starter_msg(error_handle* er_h) {
    if (er_h->active) {
        printf("%s : %s\n", er_h->starter_name, er_h->msg);
    }
    else {
        error_print(er_h);
    }

    return 0;
}

int error_print_owner_starter_msg(error_handle* er_h) {
    if (er_h->active) {
        printf("%s : %s : %s\n", er_h->owner_name, er_h->starter_name, er_h->msg);
    }
    else {
        error_print(er_h);
    }

    return 0;
}
