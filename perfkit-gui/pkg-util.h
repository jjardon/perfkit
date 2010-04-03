/* pkg-util.h
 *
 * Copyright (C) 2010 Christian Hergert <chris@dronelabs.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (__PERFKIT_INSIDE__) && !defined (PERFKIT_COMPILATION)
#error "Only <perfkit-gui/perfkit-gui.h> can be included directly."
#endif

#ifndef __PKG_UTIL_H__
#define __PKG_UTIL_H__

#include <gtk/gtk.h>
#include <clutter-gtk/clutter-gtk.h>

G_BEGIN_DECLS

GtkBuilder* pkg_util_get_builder (const gchar *name);
void        pkg_util_reparent    (GtkBuilder  *builder,
                                  const gchar *name,
                                  GtkWidget   *parent);

G_END_DECLS

#endif /* __PKG_UTIL_H__ */
