/* pkd-channel-priv.h
 * 
 * Copyright (C) 2009 Christian Hergert
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

#ifndef __PKD_CHANNEL_PRIV_H__
#define __PKD_CHANNEL_PRIV_H__

#include "pkd-channel.h"
#include "pkd-source.h"
#include "pkd-sample.h"

G_BEGIN_DECLS

void pkd_channel_add_source (PkdChannel *channel,
                             PkdSource  *source);
void pkd_channel_deliver    (PkdChannel *channel,
                             PkdSource  *source,
                             PkdSample  *sample);

G_END_DECLS

#endif /* __PKD_CHANNEL_PRIV_H__ */