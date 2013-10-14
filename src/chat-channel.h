#ifndef __HAZE_CHAT_CHANNEL_H__
#define __HAZE_CHAT_CHANNEL_H__
/*
 * im-channel.h - HazeImChannel header
 * Copyright (C) 2007 Will Thompson
 * Copyright (C) 2007 Collabora Ltd.
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

#include <telepathy-glib/telepathy-glib.h>

#include <libpurple/conversation.h>

G_BEGIN_DECLS

typedef struct _HazeChatChannel HazeChatChannel;
typedef struct _HazeChatChannelPrivate HazeChatChannelPrivate;
typedef struct _HazeChatChannelClass HazeChatChannelClass;


struct _HazeChatChannelClass {
    GObjectClass parent_class;

    TpDBusPropertiesMixinClass properties_class;
    TpGroupMixinClass group_class;
};

struct _HazeChatChannel {
    GObject parent;

    TpMessageMixin messages;
    TpGroupMixin group;

    HazeChatChannelPrivate *priv;
};

GType haze_chat_channel_get_type (void);

/* TYPE MACROS */
#define HAZE_TYPE_CHAT_CHANNEL \
  (haze_chat_channel_get_type ())
#define HAZE_CHAT_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), HAZE_TYPE_CHAT_CHANNEL, \
                              HazeChatChannel))
#define HAZE_CHAT_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), HAZE_TYPE_CHAT_CHANNEL, \
                           HazeChatChannelClass))
#define HAZE_IS_CHAT_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), HAZE_TYPE_CHAT_CHANNEL))
#define HAZE_IS_CHAT_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), HAZE_TYPE_CHAT_CHANNEL))
#define HAZE_CHAT_CHANNEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), HAZE_TYPE_CHAT_CHANNEL, \
                              HazeChatChannelClass))

typedef struct _HazeConversationUiData HazeConversationUiData;

struct _HazeConversationUiData
{
    TpHandle contact_handle;

    PurpleTypingState active_state;
    guint resend_typing_timeout_id;
};

#define PURPLE_CONV_GET_HAZE_UI_DATA(conv) \
    ((HazeConversationUiData *) conv->ui_data)

void haze_chat_channel_start (HazeChatChannel *self);

void haze_chat_channel_receive (HazeChatChannel *self, const char *xhtml_message,
    PurpleMessageFlags flags, time_t mtime);

G_END_DECLS

#endif /* #ifndef __HAZE_CHAT_CHANNEL_H__*/
