#ifndef __HAZE_CONNECTION_CAPABILITIES_H__
#define __HAZE_CONNECTION_CAPABILITIES_H__
/*
 * connection-capabilities.h - Capabilities interface headers of HazeConnection
 * Copyright (C) 2009 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#include <glib-object.h>

void haze_connection_capabilities_iface_init (gpointer g_iface,
                                              gpointer iface_data);
void haze_connection_contact_capabilities_iface_init (gpointer g_iface,
                                                      gpointer iface_data);
void haze_connection_capabilities_class_init (GObjectClass *object_class);
void haze_connection_capabilities_init (GObject *object);
void haze_connection_capabilities_finalize (GObject *object);

#endif
