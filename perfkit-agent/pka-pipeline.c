/* pka-pipeline.c
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <glib/gthread.h>
#include <stdlib.h>

#include "pka-config.h"
#include "pka-pipeline.h"
#include "pka-source-simple.h"

/**
 * SECTION:pka-pipeline
 * @title: Pipeline
 * @short_description: Agent management and event pipeline
 *
 * The pipeline module provides access to managing the perfkit-agent process.
 * It has hooks into the various source and listener plugins as well as
 * channels and subscriptions.  It is responsible for notifying listeners of
 * various pipeline events which may be of interest.
 *
 * The pipeline also manages the mainloop for the application through
 * pka_pipeline_run() and pka_pipeline_quit().
 */

static GList     *channels = NULL;
static GList     *encoders = NULL;
static GList     *encoder_infos = NULL;
static GPtrArray *listeners = NULL;
static GMainLoop *loop = NULL;
static GList     *sources = NULL;
static GList     *source_infos = NULL;
static GPtrArray *subscriptions = NULL;

/*
 * Mutable data locks.
 */
G_LOCK_DEFINE(channels);
G_LOCK_DEFINE(encoders);
G_LOCK_DEFINE(encoder_infos);
G_LOCK_DEFINE(listeners);
G_LOCK_DEFINE(sources);
G_LOCK_DEFINE(source_infos);
G_LOCK_DEFINE(subscriptions);

/*
 * Internal pipeline callback methods.
 */
extern void pka_listener_channel_added      (PkaListener     *listener,
                                             PkaChannel      *channel);
extern void pka_listener_encoder_added      (PkaListener     *listener,
                                             PkaEncoder      *encoder);
extern void pka_listener_encoder_info_added (PkaListener     *listener,
                                             PkaEncoderInfo  *encoder_info);
extern void pka_listener_source_added       (PkaListener     *listener,
                                             PkaSource       *source);
extern void pka_listener_source_info_added  (PkaListener     *listener,
                                             PkaSourceInfo   *source_info);
extern void pka_listener_subscription_added (PkaListener     *listener,
                                             PkaSubscription *subscription);

/**
 * pka_pipeline_init:
 *
 * Initializes the pipeline subsystem.
 *
 * Side effects:
 *   The mainloop is initialized.
 */
void
pka_pipeline_init (void)
{
	static gsize init = 0;

	if (g_once_init_enter(&init)) {
		g_message("Initializing pipeline.");

		/*
		 * Create main loop and needed arrays.
		 */
		loop = g_main_loop_new(NULL, FALSE);
		listeners = g_ptr_array_sized_new(4);
		subscriptions = g_ptr_array_sized_new(4);

		/*
		 * Ensure certain types are loaded.
		 */
		g_message("Registering type %s.", g_type_name(PKA_TYPE_SOURCE_SIMPLE));

		/*
		 * Dummy value storage for g_once_*().
		 */
		g_once_init_leave(&init, (gsize)loop);
	}
}

/**
 * pka_pipeline_run:
 *
 * Runs the mainloop for the agent.  This method blocks until a call to
 * pka_pipeline_quit() as been made.
 *
 * Side effects: None.
 */
void
pka_pipeline_run (void)
{
	GError *error;
	const gchar *name;
	gint i;

	g_return_if_fail(loop != NULL);

	if (listeners->len == 0) {
		g_message("No listeners configured! Exiting gracefully.");
		return;
	}

	g_message("Starting active listeners.");
	for (i = 0 ; i < listeners->len ; i++) {
		error = NULL;
		name = g_type_name(G_TYPE_FROM_INSTANCE(listeners->pdata[i]));

		if (!pka_listener_start(listeners->pdata[i], &error)) {
			g_warning("%s: %s", name, error->message);
			g_error_free(error);
		} else {
			g_message("%s started.", name);
		}
	}

	g_message("Starting mainloop.");
	g_main_loop_run(loop);
}

/**
 * pka_pipeline_quit:
 *
 * Quits the mainloop for the agent.  The blocked thread who called
 * pka_pipeline_run() will resume execution.
 *
 * Side effects: The caller of pka_pipeline_run() will resume.
 */
void
pka_pipeline_quit (void)
{
	if (!loop || !g_main_loop_is_running(loop)) {
		exit(EXIT_SUCCESS);
	}

	g_message("Stopping mainloop.");
	g_main_loop_quit(loop);
}

/**
 * pka_pipeline_shutdown:
 *
 * Cleans up the process prior to exit.
 */
void
pka_pipeline_shutdown (void)
{
	gint i;

	g_message("Cleaning up after runtime.");

	for (i = 0; i < listeners->len; i++) {
		g_object_ref(listeners->pdata[i]);
		pka_listener_stop(listeners->pdata[i]);
		g_object_unref(listeners->pdata[i]);
	}

	g_message("Listeners stopped.");

	g_list_foreach(source_infos, (GFunc)g_object_unref, NULL);
	g_list_free(source_infos);

	g_list_foreach(encoder_infos, (GFunc)g_object_unref, NULL);
	g_list_free(encoder_infos);

	g_list_foreach(sources, (GFunc)g_object_unref, NULL);
	g_list_free(sources);

	g_list_foreach(channels, (GFunc)g_object_unref, NULL);
	g_list_free(channels);

	g_ptr_array_foreach(subscriptions, (GFunc)pka_subscription_unref, NULL);
	g_ptr_array_unref(subscriptions);

	g_ptr_array_foreach(listeners, (GFunc)g_object_unref, NULL);
	g_ptr_array_unref(listeners);

	g_main_loop_unref(loop);
}

/**
 * pka_pipeline_add_source_info:
 * @source_info: A #PkaSourceInfo
 *
 * Adds a #PkaSourceInfo to the list of known source type descriptions.
 */
void
pka_pipeline_add_source_info (PkaSourceInfo *source_info)
{
	g_return_if_fail(source_info != NULL);

	g_message("Registering %s source type.",
	          pka_source_info_get_uid(source_info));

	G_LOCK(source_infos);
	source_infos = g_list_append(source_infos, g_object_ref(source_info));
	G_UNLOCK(source_infos);

	source_info = g_object_ref(source_info);
	g_ptr_array_foreach(listeners,
	                    (GFunc)pka_listener_source_info_added,
	                    source_info);
	g_object_unref(source_info);
}

/**
 * pka_pipeline_add_source:
 * @source: A #PkaSource
 *
 * Adds a #PkaSource to the list of known sources.
 */
void
pka_pipeline_add_source (PkaSource *source)
{
	g_return_if_fail(PKA_IS_SOURCE(source));

	g_message("Registering source %d.", pka_source_get_id(source));

	G_LOCK(sources);
	sources = g_list_prepend(sources, g_object_ref(source));
	G_UNLOCK(sources);

	g_object_ref(source);
	g_ptr_array_foreach(listeners,
	                    (GFunc)pka_listener_source_added,
	                    source);
	g_object_unref(source);
}

/**
 * pka_pipeline_add_channel:
 * @channel: A #PkaChannel
 *
 * Adds a #PkaChannel to the list of known channels.
 */
void
pka_pipeline_add_channel (PkaChannel *channel)
{
	g_return_if_fail(PKA_IS_CHANNEL(channel));

	g_message("Registering channel %d.", pka_channel_get_id(channel));

	G_LOCK(channels);
	channels = g_list_append(channels, g_object_ref(channel));
	G_UNLOCK(channels);

	channel = g_object_ref(channel);
	g_ptr_array_foreach(listeners,
	                    (GFunc)pka_listener_channel_added,
	                    channel);
	g_object_unref(channel);
}

/**
 * pka_pipeline_add_listener:
 * @listener: A #PkaListener
 *
 * Add a listener to the pipeline.  This is typically called from the
 * "pka_listener_register" symbol in a listener shared object.
 */
void
pka_pipeline_add_listener (PkaListener *listener)
{
	g_return_if_fail(listeners != NULL);
	g_return_if_fail(PKA_IS_LISTENER(listener));

	G_LOCK(listeners);
	g_ptr_array_add(listeners, g_object_ref(listener));
	G_UNLOCK(listeners);
}

/**
 * pka_pipeline_add_subscription:
 * @subscription: A #PkaSubscription
 *
 * Adds a subscription to the pipeline.  Listeners are notified of the
 * subscription creation.
 */
void
pka_pipeline_add_subscription (PkaSubscription *subscription)
{
	g_return_if_fail(subscriptions != NULL);
	g_return_if_fail(subscription != NULL);

	g_message("Registering subscription %d.",
	          pka_subscription_get_id(subscription));

	G_LOCK(subscriptions);
	g_ptr_array_add(subscriptions, pka_subscription_ref(subscription));
	G_UNLOCK(subscriptions);

	subscription = pka_subscription_ref(subscription);
	g_ptr_array_foreach(listeners,
						(GFunc)pka_listener_subscription_added,
						subscription);
	pka_subscription_unref(subscription);
}

/**
 * pka_pipeline_add_encoder:
 * @encoder: A #PkaEncoder
 *
 * Registers a #PkaEncoder instance with the pipeline.  This allows listeners
 * to be notified of the encoder in the case they want to notify clients.
 *
 * Side effects: None.
 */
void
pka_pipeline_add_encoder (PkaEncoder *encoder)
{
	g_return_if_fail(PKA_IS_ENCODER(encoder));

	G_LOCK(encoders);
	encoders = g_list_append(encoders, g_object_ref(encoder));
	G_UNLOCK(encoders);

	encoder = g_object_ref(encoder);
	g_ptr_array_foreach(listeners,
	                    (GFunc)pka_listener_encoder_added,
	                    encoder);
	g_object_unref(encoder);
}

/**
 * pka_pipeline_add_encoder_info:
 * @encoder_info: A #PkaEncoderInfo
 *
 * Adds a #PkaEncoderInfo to the pipeline.  Listeners are notified of the newly
 * available encoder.
 */
void
pka_pipeline_add_encoder_info (PkaEncoderInfo *encoder_info)
{
	g_return_if_fail(PKA_IS_ENCODER_INFO(encoder_info));

	g_message("Registering %s encoder.",
	          pka_encoder_info_get_uid(encoder_info));

	G_LOCK(encoder_infos);
	encoder_infos = g_list_append(encoder_infos, g_object_ref(encoder_info));
	G_UNLOCK(encoder_infos);

	encoder_info = g_object_ref(encoder_info);
	g_ptr_array_foreach(listeners,
	                    (GFunc)pka_listener_encoder_info_added,
	                    encoder_info);
	g_object_unref(encoder_info);
}

/**
 * pka_pipeline_get_channels:
 *
 * Retrieves a list of channels that are currently registered in the pipeline.
 * The returned list is a copy which should be freed with g_list_free() after
 * unref'ing the contents of the list with g_object_unref() for each item.
 *
 * [[
 * GList *list = pka_pipeline_get_channels();
 * // do something ...
 * g_list_foreach(list, (GFunc)g_object_unref, NULL);
 * g_list_free(list);
 * ]]
 *
 * Returns: A newly created list with the channels reference count incremented.
 *
 * Side effects: None.
 */
GList*
pka_pipeline_get_channels (void)
{
	GList *list;

	G_LOCK(channels);
	list = g_list_copy(channels);
	g_list_foreach(list, (GFunc)g_object_ref, NULL);
	G_UNLOCK(channels);

	return list;
}

/**
 * pka_pipeline_get_encoder_plugins:
 *
 * Retrieves a list of encoder plugins that are current registered in the
 * pipeline.  The returned list is a copy which should be freed using
 * g_list_free() only after unref'ing the contents of the list with
 * g_object_unref().
 *
 * [[
 * GList *list = pka_pipeline_get_encoder_plugins();
 * // do something ...
 * g_list_foreach(list, (GFunc)g_object_unref, NULL);
 * g_list_free(list);
 * ]]
 *
 * Returns: A newly created list with the #PkaEncoderInfo<!-- -->'s reference
 *   count incremented.
 *
 * Side effects: None.
 */
GList*
pka_pipeline_get_encoder_plugins (void)
{
	GList *list;

	G_LOCK(encoder_infos);

	list = g_list_copy(encoder_infos);
	g_list_foreach(list, (GFunc)g_object_ref, NULL);

	G_UNLOCK(encoder_infos);

	return list;
}

/**
 * pka_pipeline_get_source_plugins:
 *
 * Retrieves a list of source plugins that are currently registered in the
 * pipeline.  The returned list is a copy which should be freed using
 * g_list_free() only after unref'ing the contents of the list with
 * g_object_unref().
 *
 * [[
 * GList *list = pka_pipeline_get_source_plugins();
 * // do something ...
 * g_list_foreach(list, (GFunc)g_object_unref, NULL);
 * g_list_free(list);
 * ]]
 *
 * Returns: A newly created list with the #PkaSourceInfo<!-- -->'s reference
 *   count incremented.
 *
 * Side effects: None.
 */
GList*
pka_pipeline_get_source_plugins (void)
{
	GList *list;

	G_LOCK(source_infos);
	list = g_list_copy(source_infos);
	g_list_foreach(list, (GFunc)g_object_ref, NULL);
	G_UNLOCK(source_infos);

	return list;
}

/**
 * pka_pipeline_remove_channel:
 * @channel: A #PkaChannel
 *
 * Removes a channel from the pipeline.
 *
 * Side effects:
 *    Channel is stopped if needed.
 */
void
pka_pipeline_remove_channel (PkaChannel *channel)
{
	gboolean found;
	GError *error = NULL;

	g_return_if_fail(PKA_IS_CHANNEL(channel));

	g_message("Removing channel %d.", pka_channel_get_id(channel));

	/*
	 * Remove the channel from the list.
	 */
	G_LOCK(channels);
	found = (g_list_find(channels, channel) != NULL);
	channels = g_list_remove(channels, channel);
	G_UNLOCK(channels);

	/*
	 * If we removed it from the list, we need to ensure the channel
	 * has stopped and then loose our reference.
	 *
	 * XXX:
	 *
	 *   We should create a policy for killing the process or not.
	 *
	 */
	if (found) {
		if (!pka_channel_stop(channel, FALSE, &error)) {
			g_warning("%s", error->message);
			g_error_free(error);
		}
		g_message("Channel %d removed.", pka_channel_get_id(channel));
		g_object_unref(channel);
	}

}