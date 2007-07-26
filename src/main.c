/*
 * pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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

#include <string.h>
#include <errno.h>

#include <glib.h>

#include <account.h>

#include <core.h>
#include <blist.h>
#include <debug.h>
#include <eventloop.h>
#include <prefs.h>
#include <util.h>

#include <telepathy-glib/run.h>
#include <telepathy-glib/debug.h>

#include "defines.h"
#include "connection-manager.h"

/* Copied verbatim from nullclient.  */
#define PURPLE_GLIB_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define PURPLE_GLIB_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

typedef struct _PurpleGLibIOClosure {
	PurpleInputFunction function;
	guint result;
	gpointer data;
} PurpleGLibIOClosure;

static void purple_glib_io_destroy(gpointer data)
{
	g_free(data);
}

static gboolean purple_glib_io_invoke(GIOChannel *source, GIOCondition condition, gpointer data)
{
	PurpleGLibIOClosure *closure = data;
	PurpleInputCondition purple_cond = 0;

	if (condition & PURPLE_GLIB_READ_COND)
		purple_cond |= PURPLE_INPUT_READ;
	if (condition & PURPLE_GLIB_WRITE_COND)
		purple_cond |= PURPLE_INPUT_WRITE;

	closure->function(closure->data, g_io_channel_unix_get_fd(source),
			  purple_cond);

	return TRUE;
}

static guint glib_input_add(gint fd, PurpleInputCondition condition, PurpleInputFunction function,
							   gpointer data)
{
	PurpleGLibIOClosure *closure = g_new0(PurpleGLibIOClosure, 1);
	GIOChannel *channel;
	GIOCondition cond = 0;

	closure->function = function;
	closure->data = data;

	if (condition & PURPLE_INPUT_READ)
		cond |= PURPLE_GLIB_READ_COND;
	if (condition & PURPLE_INPUT_WRITE)
		cond |= PURPLE_GLIB_WRITE_COND;

	channel = g_io_channel_unix_new(fd);
	closure->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, cond,
					      purple_glib_io_invoke, closure, purple_glib_io_destroy);

	g_io_channel_unref(channel);
	return closure->result;
}

static PurpleEventLoopUiOps glib_eventloops = 
{
	g_timeout_add,
	g_source_remove,
	glib_input_add,
	g_source_remove,
	NULL,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};
/*** End of the eventloop functions. ***/

static void
haze_ui_init ()
{
    /* We don't actually have any UI ops for anything besides debug, core and
     * eventloop...
     */
}

static char *debug_level_names[] =
{
    "all",
    "misc",
    "info",
    "warning",
    "error",
    "fatal"
};

static void
haze_debug_print (PurpleDebugLevel level,
                  const char *category,
                  const char *arg_s)
{
    char *argh = g_strdup(arg_s);
    g_debug("[%s] %s: %s", debug_level_names[level], category,
            g_strchomp(argh));
    g_free(argh);
}

static gboolean
haze_debug_is_enabled (PurpleDebugLevel level,
                       const char *category)
{
    return level != PURPLE_DEBUG_MISC && strcmp (category, "jabber");
}

static PurpleDebugUiOps haze_debug_uiops =
{
    haze_debug_print,
    haze_debug_is_enabled,
    /* padding */
    NULL,
    NULL,
    NULL,
    NULL,
};

static void
haze_debug_init()
{
    purple_debug_set_ui_ops(&haze_debug_uiops);
}

static PurpleCoreUiOps haze_core_uiops = 
{
    NULL,
    haze_debug_init,
    haze_ui_init,
    NULL,

    /* padding */
    NULL,
    NULL,
    NULL,
    NULL
};

static void
init_libpurple()
{
    /* FIXME: clean up this directory on close. */
    char *user_dir = g_strconcat (g_get_tmp_dir (), G_DIR_SEPARATOR_S,
                                  "haze-XXXXXX", NULL);
    
    if (!mkdtemp (user_dir)) {
        g_error ("Couldn't make temporary conf directory: %s",
                 strerror (errno));
    }
    
    purple_util_set_user_dir (user_dir);

    /* Disable spewing debug information directly to the terminal.  The debug
     * uiops deal with it.
     */
    purple_debug_set_enabled(FALSE);

    purple_core_set_ui_ops(&haze_core_uiops);

    purple_eventloop_set_ui_ops(&glib_eventloops);

    if (!purple_core_init(UI_ID)) {
        g_error ("libpurple initialization failed. Dumping core."
                 "Please report this!");
    }

    purple_set_blist(purple_blist_new());
    purple_blist_load();

    purple_prefs_load();
}

static TpBaseConnectionManager *
get_cm (void)
{
    GLogLevelFlags fatal_mask;

    /* libpurple throws critical errors all over the place because of
     * g_return_val_if_fail().
     * Particularly in MSN.
     * I hate MSN.
     */
    fatal_mask = g_log_set_always_fatal (G_LOG_FATAL_MASK);
    fatal_mask &= ~G_LOG_LEVEL_CRITICAL;
    g_log_set_always_fatal (fatal_mask);

    return (TpBaseConnectionManager *) haze_connection_manager_get ();
}

int main(int argc, char **argv)
{
    int ret = 0;

    g_set_prgname(UI_ID);

    init_libpurple();
    g_debug("libpurple initialized.");

    tp_debug_set_all_flags();
    ret = tp_run_connection_manager(UI_ID, "0.0.1", get_cm, argc, argv);

    purple_core_quit();
    return ret;
}

