/*
 * chat-channel.c - HazeChatChannel source
 * Copyright (C) 2007 Will Thompson
 * Copyright (C) 2007-2008 Collabora Ltd.
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

#include <telepathy-glib/channel-iface.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/exportable-channel.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/svc-generic.h>

#include "chat-channel.h"
#include "connection.h"
#include "debug.h"

/* properties */
enum
{
  PROP_CONNECTION = 1,
  PROP_OBJECT_PATH,
  PROP_CHANNEL_TYPE,
  PROP_HANDLE_TYPE,
  PROP_HANDLE,
  PROP_TARGET_ID,
  PROP_INTERFACES,
  PROP_INITIATOR_HANDLE,
  PROP_INITIATOR_ID,
  PROP_REQUESTED,
  PROP_CHANNEL_PROPERTIES,
  PROP_CHANNEL_DESTROYED,

  LAST_PROPERTY
};

struct _HazeChatChannelPrivate
{
    HazeConnection *conn;
    char *object_path;
    TpHandle handle;
    TpHandle initiator;

    PurpleConversation *conv;

    gboolean closed;
    gboolean dispose_has_run;
};

static void channel_iface_init (gpointer, gpointer);
/*static void group_iface_init (gpointer, gpointer);*/
static void destroyable_iface_init (gpointer g_iface, gpointer iface_data);
static void chat_state_iface_init (gpointer g_iface, gpointer iface_data);
static gboolean haze_chat_channel_remove_member_with_reason (GObject *obj,
                                                      TpHandle handle,
                                                      const gchar *message,
                                                      guint reason,
                                                      GError **error);

G_DEFINE_TYPE_WITH_CODE(HazeChatChannel, haze_chat_channel, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL, channel_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_TYPE_TEXT,
        tp_message_mixin_text_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_MESSAGES,
        tp_message_mixin_messages_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_CHANNEL_IFACE, NULL);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_DESTROYABLE,
        destroyable_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_CHAT_STATE,
        chat_state_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
        tp_dbus_properties_mixin_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_GROUP,
      tp_group_mixin_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_EXPORTABLE_CHANNEL, NULL))

static void
haze_chat_channel_close (TpSvcChannel *iface,
                       DBusGMethodInvocation *context)
{
    HazeChatChannel *self = HAZE_CHAT_CHANNEL (iface);
    HazeChatChannelPrivate *priv = self->priv;

    if (priv->closed)
    {
        DEBUG ("Already closed");
        goto out;
    }

    /* requires support from TpChannelManager */
    if (tp_message_mixin_has_pending_messages ((GObject *) self, NULL))
    {
        if (priv->initiator != priv->handle)
        {
            g_assert (priv->initiator != 0);
            g_assert (priv->handle != 0);

            priv->initiator = priv->handle;
        }

        tp_message_mixin_set_rescued ((GObject *) self);
    }
    else
    {
        purple_conversation_destroy (priv->conv);
        priv->conv = NULL;
        priv->closed = TRUE;
    }

    tp_svc_channel_emit_closed (iface);

out:
    tp_svc_channel_return_from_close(context);
}

static void
haze_chat_channel_get_channel_type (TpSvcChannel *iface,
                                  DBusGMethodInvocation *context)
{
    tp_svc_channel_return_from_get_channel_type (context,
        TP_IFACE_CHANNEL_TYPE_TEXT);
}

static void
haze_chat_channel_get_handle (TpSvcChannel *iface,
                            DBusGMethodInvocation *context)
{
    HazeChatChannel *self = HAZE_CHAT_CHANNEL (iface);

    tp_svc_channel_return_from_get_handle (context, TP_HANDLE_TYPE_CONTACT,
        self->priv->handle);
}

static gboolean
_chat_state_available (HazeChatChannel *chan)
{
    PurplePluginProtocolInfo *prpl_info =
        PURPLE_PLUGIN_PROTOCOL_INFO (chan->priv->conn->account->gc->prpl);

    return (prpl_info->send_typing != NULL);
}

static const char * const*
_haze_chat_channel_interfaces (HazeChatChannel *chan)
{
  static const char * const interfaces[] = {
      TP_IFACE_CHANNEL_INTERFACE_CHAT_STATE,
      TP_IFACE_CHANNEL_INTERFACE_MESSAGES,
      TP_IFACE_CHANNEL_INTERFACE_DESTROYABLE,
      NULL
  };

  if (_chat_state_available (chan))
    return interfaces;
  else
    return interfaces + 1;
}

static void
haze_chat_channel_get_interfaces (TpSvcChannel *iface,
                                DBusGMethodInvocation *context)
{
    tp_svc_channel_return_from_get_interfaces (context,
        (const char **)_haze_chat_channel_interfaces (HAZE_CHAT_CHANNEL (iface)));
}

static void
channel_iface_init (gpointer g_iface, gpointer iface_data)
{
    TpSvcChannelClass *klass = (TpSvcChannelClass *)g_iface;

#define IMPLEMENT(x) tp_svc_channel_implement_##x (\
    klass, haze_chat_channel_##x)
    IMPLEMENT(close);
    IMPLEMENT(get_channel_type);
    IMPLEMENT(get_handle);
    IMPLEMENT(get_interfaces);
#undef IMPLEMENT
}

/*static void*/
/*haze_chat_channel_add_members (TpSvcChannelInterfaceGroup *self,*/
                             /*const GArray *in_Contacts,*/
                             /*const gchar *in_Message,*/
                             /*DBusGMethodInvocation *context)*/
/*{*/
 /*HazeChatChannel *chan = HAZE_CHAT_CHANNEL (self);*/
 /*PurpleConvChat *chat = PURPLE_CONV_CHAT (chan->priv->conv);*/
 /*TpBaseConnection *conn = TP_BASE_CONNECTION (self);*/

 /*if (chat && in_Contacts->len > 0)*/
 /*{*/
    /*guint i;*/
    /*GList *contacts = NULL, *messages = NULL, *flags = NULL;*/
    /*TpHandle handle;*/
    /*const char *bname;*/

    /*TpHandleRepoIface *contact_handles =*/
      /*tp_base_connection_get_handles (conn, TP_HANDLE_TYPE_CONTACT);*/

    /*for (i = 0 ; i < in_Contacts->len; i++)*/
    /*{*/
      /*handle = g_array_index (in_Contacts, TpHandle, i);*/
      /*bname = tp_handle_inspect (contact_handles, handle);*/
      /*contacts = g_list_append (contacts, (void *) bname);*/
      /*messages = g_list_append (messages, (char *) in_Message);*/
      /*flags = g_list_append (flags, 0);*/
    /*}*/

    /*purple_conv_chat_add_users (chat, contacts, messages, flags, TRUE);*/
    /*g_list_free (contacts);*/
    /*g_list_free (messages);*/
    /*g_list_free (flags);*/
 /*}*/

 /*tp_svc_channel_interface_group_return_from_add_members (context);*/
/*}*/

static gboolean
haze_chat_channel_add_member (GObject *object,
                                TpHandle handle,
                                const gchar *message,
                                GError **error)
{
 HazeChatChannel *chan = HAZE_CHAT_CHANNEL (object);
 PurpleConvChat *chat = PURPLE_CONV_CHAT (chan->priv->conv);
 TpBaseConnection *conn = TP_BASE_CONNECTION (chan);
 TpIntset *empty, *remote_pending;

 if (chat)
 {
    const char *bname;

    TpHandleRepoIface *contact_handles = tp_base_connection_get_handles (conn, TP_HANDLE_TYPE_CONTACT);

    bname = tp_handle_inspect (contact_handles, handle);

    purple_conv_chat_add_user (chat, bname, message, 0, TRUE);

  /* Set the contact as remote pending */
  empty = tp_intset_new ();
  remote_pending = tp_intset_new ();
  tp_intset_add (remote_pending, handle);
  tp_group_mixin_change_members (object, "", empty, empty, empty,
      remote_pending, tp_base_connection_get_self_handle (conn),
      TP_CHANNEL_GROUP_CHANGE_REASON_INVITED);
  tp_intset_destroy (empty);
  tp_intset_destroy (remote_pending);

 }
    return TRUE;
}

const char *remove_reasons[] = {
    "Disconnected",
    "NetworkError",
    "Not Available",
    "PermissionAvailable",
    "Invalid Handle",
    "Invalid Argument"
};

/*static void*/
/*haze_chat_channel_remove_members_with_reason (TpSvcChannelInterfaceGroup *self,*/
                                            /*const GArray *in_Contacts,*/
                                            /*const gchar *in_Message,*/
                                            /*guint in_Reason,*/
                                            /*DBusGMethodInvocation *context)*/
/*{*/
 /*HazeChatChannel *chan = HAZE_CHAT_CHANNEL (self);*/
 /*PurpleConvChat *chat = PURPLE_CONV_CHAT (chan->priv->conv);*/
 /*TpBaseConnection *conn = TP_BASE_CONNECTION (self);*/

 /*if (chat && in_Contacts->len > 0)*/
 /*{*/
    /*guint i;*/
    /*GList *contacts = NULL;*/
    /*[>char **messages;<]*/
    /*TpHandle handle;*/
    /*const char *bname;*/
    /*TpHandleRepoIface *contact_handles =*/
      /*tp_base_connection_get_handles (conn, TP_HANDLE_TYPE_CONTACT);*/

    /*[>messages = g_new (char *, in_Contacts->len);<]*/

    /*for (i = 0 ; i < in_Contacts->len; i++)*/
    /*{*/
      /*handle = g_array_index (in_Contacts, TpHandle, i);*/
      /*bname = tp_handle_inspect (contact_handles, handle);*/
      /*contacts = g_list_append (contacts, (void *) bname);*/
      /*[> FIXME should sent message before removing messages[i] = (char *) in_Message;<]*/
    /*}*/

    /*purple_conv_chat_remove_users (chat, contacts, remove_reasons[in_Reason]);*/
 /*}*/

  /*tp_svc_channel_interface_group_return_from_remove_members_with_reason (context);*/
/*}*/

static gboolean haze_chat_channel_remove_member_with_reason (GObject *obj,
                                                      TpHandle handle,
                                                      const gchar *message,
                                                      guint reason,
                                                      GError **error)
{
    HazeChatChannel *chan = HAZE_CHAT_CHANNEL (obj);
    PurpleConvChat *chat = PURPLE_CONV_CHAT (chan->priv->conv);
    TpBaseConnection *conn = TP_BASE_CONNECTION (chan);
    TpHandleRepoIface *contact_handles = tp_base_connection_get_handles (conn, TP_HANDLE_TYPE_CONTACT);
    TpIntset *empty, *changes;

    const char *bname = tp_handle_inspect (contact_handles, handle);
    empty = tp_intset_new ();
    changes = tp_intset_new ();

    purple_conv_chat_remove_user (chat, bname, remove_reasons[reason]);
    tp_intset_add (changes, handle);

    tp_group_mixin_change_members (obj,
                                    "",
                                    empty,
                                    changes,
                                    empty, empty,
                                    0,
                                    TP_CHANNEL_GROUP_CHANGE_REASON_NONE);
    tp_intset_destroy (changes);
    tp_intset_destroy (empty);

    return TRUE;
}
/*
struct_typedefatic void
haze_chat_channel_remove_members (TpSvcChannelInterfaceGroup *self,
                                const GArray *in_Contacts,
                                const gchar *in_Message,
                                DBusGMethodInvocation *context)
{
haze_chat_channel_remove_members_with_reason (self, in_Contacts, in_Message, 0, context );

 tp_svc_channel_interface_group_return_from_remove_members (context);
}
*/
/*
static void
group_iface_init (gpointer g_iface, gpointer iface_data)
{
    TpSvcChannelInterfaceGroupClass *klass = (TpSvcChannelInterfaceGroupClass *)g_iface;

#define IMPLEMENT(x) tp_svc_channel_interface_group_implement_##x (\
    klass, haze_chat_channel_##x)
    IMPLEMENT (add_members);
    IMPLEMENT (remove_members);
    IMPLEMENT (remove_members_with_reason);
#undef IMPLEMENT
}*/

/**
 * haze_chat_channel_destroy
 *
 * Implements D-Bus method Destroy
 * on interface org.freedesktop.Telepathy.Channel.Interface.Destroyable
 */
static void
haze_chat_channel_destroy (TpSvcChannelInterfaceDestroyable *iface,
                         DBusGMethodInvocation *context)
{
    HazeChatChannel *self = HAZE_CHAT_CHANNEL (iface);

    g_assert (HAZE_IS_CHAT_CHANNEL (self));

    DEBUG ("called on %p", self);

    /* Clear out any pending messages */
    tp_message_mixin_clear ((GObject *) self);

    /* Close() and Destroy() have the same signature, so we can safely
     * chain to the other function now */
    haze_chat_channel_close ((TpSvcChannel *) self, context);
}

static void
destroyable_iface_init (gpointer g_iface,
                        gpointer iface_data)
{
    TpSvcChannelInterfaceDestroyableClass *klass = g_iface;

#define IMPLEMENT(x) tp_svc_channel_interface_destroyable_implement_##x (\
    klass, haze_chat_channel_##x)
    IMPLEMENT(destroy);
#undef IMPLEMENT
}

const gchar *chat_typing_state_names[] = {
    "not typing",
    "typing",
    "typed"
};

static gboolean
resend_typing_cb (gpointer data)
{
    PurpleConversation *conv = (PurpleConversation *)data;
    HazeConversationUiData *ui_data = PURPLE_CONV_GET_HAZE_UI_DATA (conv);
    PurpleConnection *gc = purple_conversation_get_gc (conv);
    const gchar *who = purple_conversation_get_name (conv);
    PurpleTypingState typing = ui_data->active_state;

    DEBUG ("resending '%s' to %s", chat_typing_state_names[typing], who);
    if (serv_send_typing (gc, who, typing))
    {
        return TRUE; /* Let's keep doing this thang. */
    }
    else
    {
        DEBUG ("clearing resend_typing_cb timeout");
        ui_data->resend_typing_timeout_id = 0;
        return FALSE;
    }
}


static void
haze_chat_channel_set_chat_state (TpSvcChannelInterfaceChatState *self,
                                guint state,
                                DBusGMethodInvocation *context)
{
    HazeChatChannel *chan = HAZE_CHAT_CHANNEL (self);

    PurpleConversation *conv = chan->priv->conv;
    HazeConversationUiData *ui_data = PURPLE_CONV_GET_HAZE_UI_DATA (conv);
    PurpleConnection *gc = purple_conversation_get_gc (conv);
    const gchar *who = purple_conversation_get_name (conv);

    GError *error = NULL;
    PurpleTypingState typing = PURPLE_NOT_TYPING;
    guint timeout;

    g_assert (_chat_state_available (chan));

    if (ui_data->resend_typing_timeout_id)
    {
        DEBUG ("clearing existing resend_typing_cb timeout");
        g_source_remove (ui_data->resend_typing_timeout_id);
        ui_data->resend_typing_timeout_id = 0;
    }

    switch (state)
    {
        case TP_CHANNEL_CHAT_STATE_GONE:
            DEBUG ("The Gone state may not be explicitly set");
            g_set_error (&error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
                "The Gone state may not be explicitly set");
            break;
        case TP_CHANNEL_CHAT_STATE_INACTIVE:
        case TP_CHANNEL_CHAT_STATE_ACTIVE:
            typing = PURPLE_NOT_TYPING;
            break;
        case TP_CHANNEL_CHAT_STATE_PAUSED:
            typing = PURPLE_TYPED;
            break;
        case TP_CHANNEL_CHAT_STATE_COMPOSING:
            typing = PURPLE_TYPING;
            break;
        default:
            DEBUG ("Invalid chat state: %u", state);
            g_set_error (&error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
                "Invalid chat state: %u", state);
    }

    if (error)
    {
          dbus_g_method_return_error (context, error);
          g_error_free (error);
          return;
    }

    DEBUG ("sending '%s' to %s", chat_typing_state_names[typing], who);

    ui_data->active_state = typing;
    timeout = serv_send_typing (gc, who, typing);
    /* Apparently some protocols need you to repeatedly set the typing state,
     * so let's rig up a callback to do that.  serv_send_typing returns the
     * number of seconds till the state times out, or 0 if states don't time
     * out.
     *
     * That said, it would be stupid to repeatedly send not typing, so let's
     * not do that.
     */
    if (timeout && typing != PURPLE_NOT_TYPING)
    {
        ui_data->resend_typing_timeout_id = g_timeout_add (timeout * 1000,
            resend_typing_cb, conv);
    }

    tp_svc_channel_interface_chat_state_return_from_set_chat_state (context);
}

static void
chat_state_iface_init (gpointer g_iface, gpointer iface_data)
{
    TpSvcChannelInterfaceChatStateClass *klass =
        (TpSvcChannelInterfaceChatStateClass *)g_iface;
#define IMPLEMENT(x) tp_svc_channel_interface_chat_state_implement_##x (\
    klass, haze_chat_channel_##x)
    IMPLEMENT(set_chat_state);
#undef IMPLEMENT
}

static void
haze_chat_channel_send (GObject *obj,
                      TpMessage *message,
                      TpMessageSendingFlags send_flags)
{
  HazeChatChannel *self = HAZE_CHAT_CHANNEL (obj);
  const GHashTable *header, *body;
  const gchar *content_type, *text;
  guint type = 0;
  PurpleMessageFlags flags = 0;
  gchar *escaped, *line_broken, *reapostrophised;
  GError *error = NULL;

  if (tp_message_count_parts (message) != 2)
    {
      error = g_error_new (TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
          "messages must have a single plain-text part");
      goto err;
    }

  header = tp_message_peek (message, 0);
  body = tp_message_peek (message, 1);

  type = tp_asv_get_uint32 (header, "message-type", NULL);
  content_type = tp_asv_get_string (body, "content-type");
  text = tp_asv_get_string (body, "content");

  if (tp_strdiff (content_type, "text/plain"))
    {
      error = g_error_new (TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
          "messages must have a single plain-text part");
      goto err;
    }

  if (text == NULL)
    {
      error = g_error_new (TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
          "message body must be a UTF-8 string");
      goto err;
    }

  switch (type)
    {
    case TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION:
      /* XXX this is not good enough for prpl-irc, which has a slash-command
       *     for actions and doesn't do special stuff to messages which happen
       *     to start with "/me ".
       */
      text = g_strconcat ("/me ", text, NULL);
      break;
    case TP_CHANNEL_TEXT_MESSAGE_TYPE_AUTO_REPLY:
      flags |= PURPLE_MESSAGE_AUTO_RESP;
      /* deliberate fall-through: */
    case TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL:
      text = g_strdup (text);
      break;
    /* TODO: libpurple should probably have a NOTICE flag, and then we could
     * support TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE.
     */
    default:
      error = g_error_new (TP_ERROR, TP_ERROR_NOT_IMPLEMENTED,
          "unsupported message type: %u", type);
      goto err;
    }

  escaped = g_markup_escape_text (text, -1);
  /* avoid line breaks being swallowed! */
  line_broken = purple_strreplace (escaped, "\n", "<br>");
  /* This is a workaround for prpl-yahoo, which in libpurple <= 2.3.1 could
   * not deal with &apos; and would send it literally.
   * TODO: When we depend on new enough libpurple, remove this workaround.
   */
  reapostrophised = purple_strreplace (line_broken, "&apos;", "'");

  purple_conv_im_send_with_flags (PURPLE_CONV_IM (self->priv->conv),
      reapostrophised, flags);

  g_free (reapostrophised);
  g_free (line_broken);
  g_free (escaped);

  tp_message_mixin_sent (obj, message, 0, "", NULL);
  return;

err:
  g_assert (error != NULL);
  tp_message_mixin_sent (obj, message, 0, NULL, error);
  g_error_free (error);
}

static void
haze_chat_channel_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
    HazeChatChannel *chan = HAZE_CHAT_CHANNEL (object);
    HazeChatChannelPrivate *priv = chan->priv;
    TpBaseConnection *base_conn = (TpBaseConnection *) priv->conn;

    switch (property_id) {
        case PROP_OBJECT_PATH:
            g_value_set_string (value, priv->object_path);
            break;
        case PROP_CHANNEL_TYPE:
            g_value_set_static_string (value, TP_IFACE_CHANNEL_TYPE_TEXT);
            break;
        case PROP_HANDLE_TYPE:
            g_value_set_uint (value, TP_HANDLE_TYPE_CONTACT);
            break;
        case PROP_HANDLE:
            g_value_set_uint (value, priv->handle);
            break;
        case PROP_TARGET_ID:
        {
            TpHandleRepoIface *repo = tp_base_connection_get_handles (base_conn,
                TP_HANDLE_TYPE_CONTACT);

            g_value_set_string (value, tp_handle_inspect (repo, priv->handle));
            break;
        }
        case PROP_INITIATOR_HANDLE:
            g_value_set_uint (value, priv->initiator);
            break;
        case PROP_INITIATOR_ID:
        {
            TpHandleRepoIface *repo = tp_base_connection_get_handles (base_conn,
                TP_HANDLE_TYPE_CONTACT);

            g_value_set_string (value, tp_handle_inspect (repo, priv->initiator));
            break;
        }
        case PROP_REQUESTED:
            g_value_set_boolean (value,
                (priv->initiator == base_conn->self_handle));
            break;
        case PROP_CONNECTION:
            g_value_set_object (value, priv->conn);
            break;
        case PROP_INTERFACES:
            g_value_set_boxed (value, _haze_chat_channel_interfaces (chan));
            break;
        case PROP_CHANNEL_DESTROYED:
            g_value_set_boolean (value, priv->closed);
            break;
        case PROP_CHANNEL_PROPERTIES:
            g_value_take_boxed (value,
                tp_dbus_properties_mixin_make_properties_hash (object,
                    TP_IFACE_CHANNEL, "TargetHandle",
                    TP_IFACE_CHANNEL, "TargetHandleType",
                    TP_IFACE_CHANNEL, "ChannelType",
                    TP_IFACE_CHANNEL, "TargetID",
                    TP_IFACE_CHANNEL, "InitiatorHandle",
                    TP_IFACE_CHANNEL, "InitiatorID",
                    TP_IFACE_CHANNEL, "Requested",
                    TP_IFACE_CHANNEL, "Interfaces",
                    TP_IFACE_CHANNEL_INTERFACE_MESSAGES,
                        "MessagePartSupportFlags",
                    TP_IFACE_CHANNEL_INTERFACE_MESSAGES,
                        "DeliveryReportingSupport",
                    TP_IFACE_CHANNEL_INTERFACE_MESSAGES,
                        "SupportedContentTypes",
                    TP_IFACE_CHANNEL_INTERFACE_MESSAGES, "MessageTypes",
                    NULL));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
haze_chat_channel_set_property (GObject     *object,
                              guint        property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
    HazeChatChannel *chan = HAZE_CHAT_CHANNEL (object);
    HazeChatChannelPrivate *priv = chan->priv;

    switch (property_id) {
        case PROP_OBJECT_PATH:
            g_free (priv->object_path);
            priv->object_path = g_value_dup_string (value);
            break;
        case PROP_HANDLE:
            /* we don't ref it here because we don't have access to the
             * contact repo yet - instead we ref it in the constructor.
             */
            priv->handle = g_value_get_uint (value);
            break;
        case PROP_INITIATOR_HANDLE:
            /* similarly we can't ref this yet */
            priv->initiator = g_value_get_uint (value);
            break;
        case PROP_CHANNEL_TYPE:
        case PROP_HANDLE_TYPE:
            /* this property is writable in the interface, but not actually
             * meaningfully changable on this channel, so we do nothing.
             */
            break;
        case PROP_CONNECTION:
            priv->conn = g_value_get_object (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static const TpChannelTextMessageType supported_message_types[] = {
    TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
    TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION,
    TP_CHANNEL_TEXT_MESSAGE_TYPE_AUTO_REPLY,
};

static const gchar * const supported_content_types[] = {
    "text/plain",
    NULL
};

static GObject *
haze_chat_channel_constructor (GType type, guint n_props,
                             GObjectConstructParam *props)
{
    GObject *obj;
    HazeChatChannel *chan;
    HazeChatChannelPrivate *priv;
    TpBaseConnection *conn;
    TpDBusDaemon *bus;
    TpHandleRepoIface *contact_repo;
    TpHandle self_handle = 0;/* FIXME*/

    obj = G_OBJECT_CLASS (haze_chat_channel_parent_class)->
        constructor (type, n_props, props);
    chan = HAZE_CHAT_CHANNEL (obj);
    priv = chan->priv;
    conn = (TpBaseConnection *) (priv->conn);
    contact_repo = tp_base_connection_get_handles (conn, TP_HANDLE_TYPE_CONTACT);
    /*self_handle = */

    g_assert (priv->initiator != 0);

    tp_message_mixin_init (obj, G_STRUCT_OFFSET (HazeChatChannel, messages),
        conn);
    tp_message_mixin_implement_sending (obj, haze_chat_channel_send, 3,
        supported_message_types, 0, 0, supported_content_types);

    bus = tp_base_connection_get_dbus_daemon (conn);
    tp_dbus_daemon_register_object (bus, priv->object_path, obj);

    priv->closed = FALSE;
    priv->dispose_has_run = FALSE;

    tp_group_mixin_init (obj,
                         G_STRUCT_OFFSET (HazeChatChannel, group),
                         contact_repo, self_handle);


    return obj;
}

static void
haze_chat_channel_dispose (GObject *obj)
{
    HazeChatChannel *chan = HAZE_CHAT_CHANNEL (obj);
    HazeChatChannelPrivate *priv = chan->priv;

    if (priv->dispose_has_run)
        return;
    priv->dispose_has_run = TRUE;

    if (!priv->closed)
    {
        purple_conversation_destroy (priv->conv);
        priv->conv = NULL;
        tp_svc_channel_emit_closed (obj);
        priv->closed = TRUE;
    }

    g_free (priv->object_path);
    tp_message_mixin_finalize (obj);

    G_OBJECT_CLASS (haze_chat_channel_parent_class)->dispose (obj);
}

static void
haze_chat_channel_class_init (HazeChatChannelClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GParamSpec *param_spec;

    static gboolean properties_mixin_initialized = FALSE;
    static TpDBusPropertiesMixinPropImpl channel_props[] = {
        { "TargetHandleType", "handle-type", NULL },
        { "TargetHandle", "handle", NULL },
        { "TargetID", "target-id", NULL },
        { "ChannelType", "channel-type", NULL },
        { "Interfaces", "interfaces", NULL },
        { "Requested", "requested", NULL },
        { "InitiatorHandle", "initiator-handle", NULL },
        { "InitiatorID", "initiator-id", NULL },
        { NULL }
    };
    static TpDBusPropertiesMixinIfaceImpl prop_interfaces[] = {
        { TP_IFACE_CHANNEL,
          tp_dbus_properties_mixin_getter_gobject_properties,
          NULL,
          channel_props,
        },
        { NULL }
    };


    g_type_class_add_private (klass, sizeof (HazeChatChannelPrivate));

    object_class->get_property = haze_chat_channel_get_property;
    object_class->set_property = haze_chat_channel_set_property;
    object_class->constructor = haze_chat_channel_constructor;
    object_class->dispose = haze_chat_channel_dispose;

    g_object_class_override_property (object_class, PROP_OBJECT_PATH,
        "object-path");
    g_object_class_override_property (object_class, PROP_CHANNEL_TYPE,
        "channel-type");
    g_object_class_override_property (object_class, PROP_HANDLE_TYPE,
        "handle-type");
    g_object_class_override_property (object_class, PROP_HANDLE,
        "handle");
    g_object_class_override_property (object_class, PROP_CHANNEL_DESTROYED,
        "channel-destroyed");
    g_object_class_override_property (object_class, PROP_CHANNEL_PROPERTIES,
        "channel-properties");

    param_spec = g_param_spec_object ("connection", "HazeConnection object",
        "Haze connection object that owns this IM channel object.",
        HAZE_TYPE_CONNECTION,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property (object_class, PROP_CONNECTION, param_spec);

    param_spec = g_param_spec_boxed ("interfaces", "Extra D-Bus interfaces",
        "Additional Channel.Interface.* interfaces",
        G_TYPE_STRV,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property (object_class, PROP_INTERFACES, param_spec);

    param_spec = g_param_spec_string ("target-id", "Other person's username",
        "The username of the other person in the conversation",
        NULL,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property (object_class, PROP_TARGET_ID, param_spec);

    param_spec = g_param_spec_boolean ("requested", "Requested?",
        "True if this channel was requested by the local user",
        FALSE,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property (object_class, PROP_REQUESTED, param_spec);

    param_spec = g_param_spec_uint ("initiator-handle", "Initiator's handle",
        "The contact who initiated the channel",
        0, G_MAXUINT32, 0,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property (object_class, PROP_INITIATOR_HANDLE,
        param_spec);

    param_spec = g_param_spec_string ("initiator-id", "Initiator's ID",
        "The string obtained by inspecting the initiator-handle",
        NULL,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property (object_class, PROP_INITIATOR_ID,
        param_spec);


    if (!properties_mixin_initialized)
    {
        properties_mixin_initialized = TRUE;
        klass->properties_class.interfaces = prop_interfaces;
        tp_dbus_properties_mixin_class_init (object_class,
            G_STRUCT_OFFSET (HazeChatChannelClass, properties_class));

        tp_message_mixin_init_dbus_properties (object_class);

        tp_group_mixin_class_init (object_class,
          G_STRUCT_OFFSET (HazeChatChannelClass, group_class),
          haze_chat_channel_add_member,
          NULL);
        tp_group_mixin_class_set_remove_with_reason_func (object_class,
            haze_chat_channel_remove_member_with_reason);
        tp_group_mixin_class_allow_self_removal (object_class);
        tp_group_mixin_init_dbus_properties (object_class);


    }
}

static void
haze_chat_channel_init (HazeChatChannel *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, HAZE_TYPE_CHAT_CHANNEL,
                                              HazeChatChannelPrivate);
}

void
haze_chat_channel_start (HazeChatChannel *self)
{
    const char *recipient;
    HazeChatChannelPrivate *priv = self->priv;
    TpHandleRepoIface *contact_handles;
    TpBaseConnection *base_conn = (TpBaseConnection *) priv->conn;

    contact_handles = tp_base_connection_get_handles (base_conn,
        TP_HANDLE_TYPE_CONTACT);
    recipient = tp_handle_inspect(contact_handles, priv->handle);
    priv->conv = purple_conversation_new (PURPLE_CONV_TYPE_IM,
                                          priv->conn->account,
                                          recipient);
}

static TpMessage *
_make_message (HazeChatChannel *self,
               char *text_plain,
               PurpleMessageFlags flags,
               time_t mtime)
{
  TpBaseConnection *base_conn = (TpBaseConnection *) self->priv->conn;
  TpMessage *message = tp_cm_message_new (base_conn, 2);
  TpChannelTextMessageType type = TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL;
  time_t now = time (NULL);

  if (flags & PURPLE_MESSAGE_AUTO_RESP)
    type = TP_CHANNEL_TEXT_MESSAGE_TYPE_AUTO_REPLY;
  else if (purple_message_meify (text_plain, -1))
    type = TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION;

  tp_cm_message_set_sender (message, self->priv->handle);
  tp_message_set_uint32 (message, 0, "message-type", type);

  /* FIXME: the second half of this test shouldn't be necessary but prpl-jabber
   *        or the test are broken.
   */
  if (flags & PURPLE_MESSAGE_DELAYED || mtime != now)
    tp_message_set_int64 (message, 0, "message-sent", mtime);

  tp_message_set_int64 (message, 0, "message-received", now);

  /* Body */
  tp_message_set_string (message, 1, "content-type", "text/plain");
  tp_message_set_string (message, 1, "content", text_plain);

  return message;
}

static TpMessage *
_make_delivery_report (HazeChatChannel *self,
                       char *text_plain)
{
  TpBaseConnection *base_conn = (TpBaseConnection *) self->priv->conn;
  TpMessage *report = tp_cm_message_new (base_conn, 2);

  /* "MUST be the intended recipient of the original message" */
  tp_cm_message_set_sender (report, self->priv->handle);
  tp_message_set_uint32 (report, 0, "message-type",
      TP_CHANNEL_TEXT_MESSAGE_TYPE_DELIVERY_REPORT);
  /* FIXME: we don't know that the failure is temporary */
  tp_message_set_uint32 (report, 0, "delivery-status",
      TP_DELIVERY_STATUS_TEMPORARILY_FAILED);

  /* Put libpurple's localized human-readable error message both into the debug
   * info field in the header, and as the delivery report's body.
   */
  tp_message_set_string (report, 0, "delivery-error-message", text_plain);
  tp_message_set_string (report, 1, "content-type", "text/plain");
  tp_message_set_string (report, 1, "content", text_plain);

  return report;
}

void
haze_chat_channel_receive (HazeChatChannel *self,
                         const char *xhtml_message,
                         PurpleMessageFlags flags,
                         time_t mtime)
{
  gchar *line_broken, *text_plain;

  /* Replaces newline characters with <br>, which then get turned back into
   * newlines by purple_markup_strip_html (which replaces "\n" with " ")...
   */
  line_broken = purple_strdup_withhtml (xhtml_message);
  text_plain = purple_markup_strip_html (line_broken);
  g_free (line_broken);

  if (flags & PURPLE_MESSAGE_RECV)
    tp_message_mixin_take_received ((GObject *) self,
        _make_message (self, text_plain, flags, mtime));
  else if (flags & PURPLE_MESSAGE_SEND)
    {
      /* Do nothing: the message mixin emitted sent for us. */
    }
  else if (flags & PURPLE_MESSAGE_ERROR)
    tp_message_mixin_take_received ((GObject *) self,
        _make_delivery_report (self, text_plain));
  else
    DEBUG ("channel %u: ignoring message %s with flags %u",
        self->priv->handle, text_plain, flags);

  g_free (text_plain);
}
