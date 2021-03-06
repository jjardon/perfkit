/* pk-util.h
 *
 * Copyright (C) 2010 Christian Hergert <chris@dronelabs.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __PK_UTIL_H__
#define __PK_UTIL_H__

#include <glib.h>

G_BEGIN_DECLS

#define ASYNC_NEW(_o, _c, _d, _f)                                           \
    g_simple_async_result_new(G_OBJECT(_o), _c, _f, _d)

#define ASYNC_IS_VALID(_r, _o, _f)                                          \
    g_simple_async_result_is_valid(G_ASYNC_RESULT((_r)),                    \
                            G_OBJECT((_o)), _f)

#define ASYNC_COMPLETE(_r)                                                  \
    G_STMT_START {                                                          \
        if (g_main_context_is_owner(g_main_context_default())) {            \
            g_simple_async_result_complete(G_SIMPLE_ASYNC_RESULT(_r));      \
        } else {                                                            \
            g_simple_async_result_complete_in_idle(                         \
                    G_SIMPLE_ASYNC_RESULT(_r));                             \
        }                                                                   \
    } G_STMT_END

#define ASYNC_ERROR_IF_NOT_CONNECTED(_r, _c)                                \
    G_STMT_START {                                                          \
        if (!(_c) || !pk_connection_is_connected((_c))) {                   \
            g_simple_async_result_set_error(_r, PK_CONNECTION_ERROR,        \
                                            PK_CONNECTION_ERROR_STATE,      \
                                            "Not connected to Agent");      \
            ASYNC_COMPLETE((_r));                                           \
            return;                                                         \
        }                                                                   \
    } G_STMT_END

G_END_DECLS

#endif /* __PK_UTIL_H__ */
