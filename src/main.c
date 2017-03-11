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
#include "ffp_term.h"

int main (void) {
    root_dir root;
    dir_info dir;
    term_info info;
    int ret;

    init_root_dir(&root);

    init_dir_info(&dir, &root, 0);

    init_term_info(&info);

    srand(time(NULL));

    ret = 0;
    while (ret != QUIT_REQUESTED) {
        ret = prompt(&info, &dir);
    }

    return 0;
}
