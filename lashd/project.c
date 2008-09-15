/*
 *   LASH
 *
 *   Copyright (C) 2008 Juuso Alasuutari <juuso.alasuutari@gmail.com>
 *   Copyright (C) 2002 Robert Ham <rah@bash.sh>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define _GNU_SOURCE

#include "../config.h"

#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <dbus/dbus.h>
#include <jack/jack.h>

#include "project.h"
#include "client.h"
#include "client_dependency.h"
#include "store.h"
#include "file.h"
#include "jack_patch.h"
#include "server.h"
#include "dbus_iface_control.h"
#include "common/safety.h"
#include "common/debug.h"

#ifdef HAVE_JACK_DBUS
# include "jackdbus_mgr.h"
#else
# include "jack_mgr.h"
#endif

#ifdef HAVE_ALSA
#include <alsa/asoundlib.h>
#include "alsa_patch.h"
#endif

static const char *
project_get_client_dir(project_t *project,
                       client_t  *client);

static const char *
project_get_client_config_dir(project_t *project,
                              client_t  *client);

project_t *
project_new(void)
{
	project_t *project;

	project = lash_calloc(1, sizeof(project_t));

	INIT_LIST_HEAD(&project->clients);
	INIT_LIST_HEAD(&project->lost_clients);

	return project;
}

static client_t *
project_get_client_by_name(project_t  *project,
                           const char *name)
{
	struct list_head *node;
	client_t *client;

	list_for_each(node, &project->clients) {
		client = list_entry(node, client_t, siblings);

		if (client && client->name
		    && strcmp(client->name, name) == 0)
			return client;
	}

	return NULL;
}

static char *
project_get_unique_client_name(project_t *project,
                               client_t  *client)
{
	uint8_t i;
	char *str = lash_malloc(1, strlen(client->class) + 4);

	lash_debug("Creating a unique name for client %s based on its class",
	           client->id_str);

	if (!project_get_client_by_name(project, client->class)) {
		strcpy(str, client->class);
		return str;
	}

	for (i = 1; i < 100; ++i) {
		sprintf(str, "%s %02u", client->class, i);
		if (!project_get_client_by_name(project, str)) {
			return str;
		}
	}

	lash_error("Could not create a unique name for client %s. Do you have "
	           "100 clients of class %s open?", client->id_str,
	           client->class);

	lash_free(&str);

	return NULL;
}

client_t *
project_get_client_by_id(struct list_head *client_list,
                         uuid_t            id)
{
	struct list_head *node;
	client_t *client;

	list_for_each(node, client_list) {
		client = list_entry(node, client_t, siblings);

		if (uuid_compare(client->id, id) == 0)
			return client;
	}

	return NULL;
}

static void
project_name_client(project_t  *project,
                    client_t   *client,
                    const char *name)
{
	char *unique_name = NULL;

	/* If no name was supplied we'll assume that
	   the caller wants a new unique name. */
	if (!name) {
		unique_name = project_get_unique_client_name(project, client);
		if (!unique_name)
			return;
		name = (const char *) unique_name;
	}

	lash_debug("Attempting to give client %s the name '%s'",
	           client->id_str, name);

	/* Check if the client already has the requested name */
	if (client->name && strcmp(name, client->name) == 0) {
		lash_debug("Client %s is already named '%s'; no need to rename",
		           client->id_str, name);
		lash_free(&unique_name);
		return;
	}

	lash_strset(&client->name, name);
	lash_free(&unique_name);

	lash_info("Client %s set its name to '%s'", client->id_str, client->name);
}

void
project_new_client(project_t *project,
                   client_t  *client)
{
	uuid_generate(client->id);
	uuid_unparse(client->id, client->id_str);

	/* Set the client's data path */
	lash_strset(&client->data_path,
	            project_get_client_dir(project, client));

	lash_debug("New client now has id %s", client->id_str);

	if (CLIENT_CONFIG_DATA_SET(client))
		client_store_open(client,
		                  project_get_client_config_dir(project,
		                                                client));

	client->project = project;
	list_add(&client->siblings, &project->clients);

	lash_info("Added client %s of class '%s' to project '%s'",
	          client->id_str, client->class, project->name);

	lash_create_dir(client->data_path);

	/* Give the client a unique name */
	project_name_client(project, client, NULL);

	// TODO: Swap 2nd and 3rd parameter of this signal
	lashd_dbus_signal_emit_client_appeared(client->id_str, project->name,
	                                       client->name);
}

void
project_satisfy_client_dependency(project_t *project,
                                  client_t  *client)
{
	struct list_head *node;
	client_t *lost_client;

	list_for_each(node, &project->lost_clients) {
		lost_client = list_entry(node, client_t, siblings);

		if (!list_empty(&lost_client->unsatisfied_deps)) {
			client_dependency_remove(&lost_client->unsatisfied_deps,
			                         client->id);

			if (list_empty(&lost_client->unsatisfied_deps)) {
				lash_debug("Dependencies for client '%s' "
				           "are now satisfied",
				           lost_client->name);
				project_launch_client(project, lost_client);
			}
		}
	}
}

static __inline__ void
project_load_file(project_t *project,
                  client_t  *client)
{
	lash_debug("Requesting client '%s' to load data from disk",
	           client_get_identity(client));

	client->pending_task = (++g_server->task_iter);
	client->task_type = LASH_Restore_File;
	client->task_progress = 0;

	method_call_new_valist(g_server->dbus_service, NULL,
	                       method_default_handler, false,
	                       client->dbus_name,
	                       "/org/nongnu/LASH/Client",
	                       "org.nongnu.LASH.Client",
	                       "Load",
	                       DBUS_TYPE_UINT64, &client->pending_task,
	                       DBUS_TYPE_STRING, &client->data_path,
	                       DBUS_TYPE_INVALID);
}

/* Send a LoadDataSet method call to the client */
static __inline__ void
project_load_data_set(project_t *project,
                      client_t  *client)
{
	if (!client_store_open(client, project_get_client_config_dir(project,
	                                                             client))) {
		lash_error("Could not open client's store; "
		           "not sending data set");
		return;
	}

	lash_debug("Sending client '%s' its data set",
	           client_get_identity(client));

	if (list_empty(&client->store->keys)) {
		lash_debug("No data found in store");
		return;
	}

	method_msg_t new_call;
	DBusMessageIter iter, array_iter;
	dbus_uint64_t task_id;

	if (!method_call_init(&new_call, g_server->dbus_service,
	                      NULL,
	                      method_default_handler,
	                      client->dbus_name,
	                      "/org/nongnu/LASH/Client",
	                      "org.nongnu.LASH.Client",
	                      "LoadDataSet")) {
		lash_error("Failed to initialise LoadDataSet method call");
		return;
	}

	dbus_message_iter_init_append(new_call.message, &iter);

	task_id = (++g_server->task_iter);

	if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT64, &task_id)) {
		lash_error("Failed to write task ID");
		goto fail;
	}

	if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &array_iter)) {
		lash_error("Failed to open config array container");
		goto fail;
	}

	if (!store_create_config_array(client->store, &array_iter)) {
		lash_error("Failed to create config array");
		goto fail;
	}

	if (!dbus_message_iter_close_container(&iter, &array_iter)) {
		lash_error("Failed to close config array container");
		goto fail;
	}

	if (!method_send(&new_call, false)) {
		lash_error("Failed to send LoadDataSet method call");
		/* method_send has unref'd the message for us */
		return;
	}

	client->pending_task = task_id;
	client->task_type = LASH_Restore_Data_Set;
	client->task_progress = 0;

	return;

fail:
	dbus_message_unref(new_call.message);
}

static __inline__ void
project_resume_client(project_t *project,
                      client_t  *client,
                      client_t  *lost_client)
{
	lash_event_t *event;
	char *name;

	lash_debug("Attempting to resume client of class '%s'",
	           lost_client->class);

	/* Get all the necessary data from the lost client */
	name = lost_client->name;
	lost_client->name = NULL;
	client->alsa_patches = lost_client->alsa_patches;
	lost_client->alsa_patches = NULL;
	client->jack_patches = lost_client->jack_patches;
	lost_client->jack_patches = NULL;
	client->flags = lost_client->flags;
	uuid_copy(client->id, lost_client->id);
	memcpy(client->id_str, lost_client->id_str, 37);
	lash_free(&client->working_dir);
	client->working_dir = lost_client->working_dir;
	lost_client->working_dir = NULL;

	/* Set the client's data path */
	if (lost_client->data_path) {
		client->data_path = lost_client->data_path;
		lost_client->data_path = NULL;
	} else {
		lash_strset(&client->data_path,
		            project_get_client_dir(project, client));
	}

	/* Create the data path if necessary */
	if (CLIENT_CONFIG_FILE(client) || CLIENT_CONFIG_DATA_SET(client))
		lash_create_dir(client->data_path);

	/* Steal the dependencies from the lost client */
	list_splice_init(&lost_client->dependencies, &client->dependencies);

	/* Kill the lost client */
	list_del(&lost_client->siblings);
	client_destroy(lost_client);

	/* Tell the client to load its state if it was saved previously */
	if (CLIENT_SAVED(client)) {
		// TODO: Implement task queue so that we can
		//       give a client both tasks
		if (CLIENT_CONFIG_FILE(client))
			project_load_file(project, client);
		else if (CLIENT_CONFIG_DATA_SET(client))
			project_load_data_set(project, client);
	} else
		lash_debug("Client '%s' has no data to load", client_get_identity(client));

	client->project = project;
	list_add(&client->siblings, &project->clients);

	/* Name the resumed client */
	project_name_client(project, client, name);
	lash_free(&name);

	lash_info("Resumed client %s of class '%s' in project '%s'",
	          client->id_str, client->class, project->name);

	lashd_dbus_signal_emit_client_appeared(client->id_str, project->name,
	                                       client->name);

	/* Clients with nothing to load need to notify about
	   their completion as soon as they appear */
	if (!CLIENT_SAVED(client))
		project_client_task_completed(project, client);
}

void
project_launch_client(project_t *project,
                      client_t  *client)
{
	lash_debug("Launching client %s", client->id_str);

	loader_execute(client, false);

	dbus_free_string_array(client->argv);
	client->argv = NULL;
	client->argc = 0;
}

void
project_add_client(project_t *project,
                  client_t   *client)
{
	struct list_head *node;
	client_t *lost_client;

	lash_debug("Adding client to project '%s'", project->name);

	if (CLIENT_NO_AUTORESUME(client) || list_empty(&project->lost_clients))
		goto new_client;

	/*
	 * Try and find a client we can resume
	 */

	/* See if this is a launched client */
	list_for_each (node, &project->lost_clients) {
		lost_client = list_entry(node, client_t, siblings);

		lash_debug("Checking client with PID %u "
		           "against lost client with PID %u",
		           client->pid, lost_client->pid);

		if (lost_client->pid && client->pid == lost_client->pid)
			goto resume_client;
	}

	lash_debug("Cannot match PID %u with any lost client, "
	           "trying to match client class instead");

	/* See if this is a recovering client */
	list_for_each (node, &project->lost_clients) {
		lost_client = list_entry(node, client_t, siblings);

		lash_debug("Checking client of class '%s' "
		           "against lost client of class '%s'",
		           client->class, lost_client->class);

		if (strcmp(client->class, lost_client->class) == 0)
			goto resume_client;
	}

	lash_debug("Could not resume client, adding as new client");

new_client:
	project_new_client(project, client);
	return;

resume_client:
	project_resume_client(project, client, lost_client);
}

static const char *
project_get_client_dir(project_t *project,
                       client_t  *client)
{
	get_store_and_return_fqn(lash_get_fqn(project->directory,
	                                      PROJECT_ID_DIR),
	                         client->id_str);
}

static const char *
project_get_client_config_dir(project_t *project,
                              client_t  *client)
{
	get_store_and_return_fqn(project_get_client_dir(project, client),
	                         PROJECT_CONFIG_DIR);
}

// TODO: - Needs to check for errors so that we can
//         report failures back to the control app
//       - Needs to be less fugly
void
project_move(project_t  *project,
             const char *new_dir)
{
	struct list_head *node;
	client_t *client;
	DIR *dir = NULL;

	if (!new_dir || !new_dir[0]
	    || strcmp(new_dir, project->directory) == 0)
		return;

	/* Check to be sure directory is acceptable
	 * FIXME: thorough enough error checking? */
	dir = opendir(new_dir);

	if (dir || errno == ENOTDIR) {
		lash_error("Cannot move project to %s: Target exists",
		           new_dir);
		closedir(dir);
		return;
	} else if (errno == ENOENT) {
		/* This is what we want... */
		/*printf("Directory %s does not exist, creating.\n", new_dir);*/
	}

	/* close all the clients' stores */
	list_for_each (node, &project->clients) {
		client = list_entry(node, client_t, siblings);
		client_store_close(client);
		/* FIXME: check for errors */
	}

	/* move the directory */

	if (rename(project->directory, new_dir)) {
		lash_error("Cannot move project to %s: %s",
		           new_dir, strerror(errno));
	} else {
		lash_info("Project '%s' moved from %s to %s",
		          project->name, project->directory, new_dir);

		lash_strset(&project->directory, new_dir);
		lashd_dbus_signal_emit_project_path_changed(project->name, new_dir);

		/* open all the clients' stores again */
		list_for_each (node, &project->clients) {
			client = list_entry(node, client_t, siblings);
			client_store_open(client,
			                  project_get_client_config_dir(project,
			                                                client));
			/* FIXME: check for errors */
		}
	}
}

/* This is the handler to use when calling a client's Save method.
   At the moment it isn't used but it will be, so don't delete. */
static void
project_save_client_handler(DBusPendingCall *pending,
                            void            *data)
{
	DBusMessage *msg = dbus_pending_call_steal_reply(pending);
	client_t *client = data;

	if (msg) {
		const char *err_str;

		if (!method_return_verify(msg, &err_str)) {
			lash_error("Client save request failed: %s", err_str);
			client->pending_task = 0;
			client->task_type = 0;
			client->task_progress = 0;
		} else {
			lash_debug("Client save request succeeded");
			++client->project->client_tasks_total;
			/* Now we can start waiting for the client to send
			   a success or failure report of the save.
			   We will not reset pending_task until the report
			   arrives. */
		}
		dbus_message_unref(msg);
	} else {
		lash_error("Cannot get method return from pending call");
		client->pending_task = 0;
		client->task_type = 0;
		client->task_progress = 0;
	}

	dbus_pending_call_unref(pending);
}

static __inline__ void
project_save_clients(project_t *project)
{
	struct list_head *node;
	client_t *client;

	list_for_each (node, &project->clients) {
		client = list_entry(node, client_t, siblings);
		if (client->pending_task) {
			lash_error("Clients have pending tasks, not sending "
			           "save request");
			return;
		}
	}

	project->task_type = LASH_TASK_SAVE;
	project->client_tasks_total = 0;
	project->client_tasks_progress = 0;
	++g_server->task_iter;

	lash_debug("Signaling all clients of project '%s' to save (task %llu)",
	           project->name, g_server->task_iter);
	signal_new_valist(g_server->dbus_service,
	                  "/", "org.nongnu.LASH.Server", "Save",
	                  DBUS_TYPE_STRING, &project->name,
	                  DBUS_TYPE_UINT64, &g_server->task_iter,
	                  DBUS_TYPE_INVALID);

	list_for_each (node, &project->clients) {
		client = list_entry(node, client_t, siblings);

		client->pending_task = g_server->task_iter;
		client->task_type = (CLIENT_CONFIG_FILE(client))
		                    ? LASH_Save_File
		                    : LASH_Save_Data_Set;
		client->task_progress = 0;
		client->flags |= LASH_Saved;
		++project->client_tasks_total;
	}

	project->client_tasks_pending = project->client_tasks_total;
}

static void
project_create_client_jack_patch_xml(project_t  *project,
                                     client_t   *client,
                                     xmlNodePtr  clientxml)
{
	xmlNodePtr jack_patch_set;
	lash_list_t *patches, *node;
	jack_patch_t *patch;

#ifdef HAVE_JACK_DBUS
	patches =
	  lashd_jackdbus_mgr_get_client_patches(g_server->jackdbus_mgr,
	                                        client->id);
#else
	jack_mgr_lock(g_server->jack_mgr);
	patches =
	  jack_mgr_get_client_patches(g_server->jack_mgr, client->id);
	jack_mgr_unlock(g_server->jack_mgr);
#endif

	if (!patches)
		return;

	jack_patch_set =
		xmlNewChild(clientxml, NULL, BAD_CAST "jack_patch_set", NULL);

	for (node = patches; node; node = lash_list_next(node)) {
		patch = (jack_patch_t *) node->data;

		jack_patch_create_xml(patch, jack_patch_set);

		jack_patch_destroy(patch);
	}

	lash_list_free(patches);
}

#ifdef HAVE_ALSA
static void
project_create_client_alsa_patch_xml(project_t  *project,
                                     client_t   *client,
                                     xmlNodePtr  clientxml)
{
	xmlNodePtr alsa_patch_set;
	lash_list_t *patches, *node;
	alsa_patch_t *patch;

	alsa_mgr_lock(g_server->alsa_mgr);
	patches =
		alsa_mgr_get_client_patches(g_server->alsa_mgr, client->id);
	alsa_mgr_unlock(g_server->alsa_mgr);

	if (!patches)
		return;

	alsa_patch_set =
		xmlNewChild(clientxml, NULL, BAD_CAST "alsa_patch_set", NULL);

	for (node = patches; node; node = lash_list_next(node)) {
		patch = (alsa_patch_t *) node->data;

		alsa_patch_create_xml(patch, alsa_patch_set);

		alsa_patch_destroy(patch);
	}

	lash_list_free(patches);
}
#endif

static void
project_create_client_dependencies_xml(client_t   *client,
                                       xmlNodePtr  parent)
{
	struct list_head *node;
	client_dependency_t *dep;
	xmlNodePtr deps_xml;
	char id_str[37];

	deps_xml = xmlNewChild(parent, NULL, BAD_CAST "dependencies", NULL);

	list_for_each (node, &client->dependencies) {
		dep = list_entry(node, client_dependency_t, siblings);
		uuid_unparse(dep->client_id, id_str);
		xmlNewChild(deps_xml, NULL, BAD_CAST "id",
		            BAD_CAST id_str);
	}
}

static xmlDocPtr
project_create_xml(project_t *project)
{
	xmlDocPtr doc;
	xmlNodePtr lash_project, clientxml, arg_set;
	struct list_head *node;
	client_t *client;
	char num[16];
	int i;

	doc = xmlNewDoc(BAD_CAST XML_DEFAULT_VERSION);

	/* DTD */
	xmlCreateIntSubset(doc, BAD_CAST "lash_project", NULL,
	                   BAD_CAST "http://www.nongnu.org/lash/lash-project-1.0.dtd");

	/* Root node */
	lash_project = xmlNewDocNode(doc, NULL, BAD_CAST "lash_project", NULL);
	xmlAddChild((xmlNodePtr) doc, lash_project);

	xmlNewChild(lash_project, NULL, BAD_CAST "version",
	            BAD_CAST PROJECT_XML_VERSION);

	xmlNewChild(lash_project, NULL, BAD_CAST "name",
	            BAD_CAST project->name);

	xmlNewChild(lash_project, NULL, BAD_CAST "description",
	            BAD_CAST project->description);

	list_for_each (node, &project->clients) {
		client = list_entry(node, client_t, siblings);

		clientxml = xmlNewChild(lash_project, NULL, BAD_CAST "client",
		                        NULL);

		xmlNewChild(clientxml, NULL, BAD_CAST "class",
		            BAD_CAST client->class);

		xmlNewChild(clientxml, NULL, BAD_CAST "id",
		            BAD_CAST client->id_str);

		xmlNewChild(clientxml, NULL, BAD_CAST "name",
		            BAD_CAST client->name);

		sprintf(num, "%d", client->flags);
		xmlNewChild(clientxml, NULL, BAD_CAST "flags", BAD_CAST num);

		xmlNewChild(clientxml, NULL, BAD_CAST "working_directory",
		            BAD_CAST client->working_dir);

		arg_set = xmlNewChild(clientxml, NULL, BAD_CAST "arg_set", NULL);
		for (i = 0; i < client->argc; i++)
			xmlNewChild(arg_set, NULL, BAD_CAST "arg",
			            BAD_CAST client->argv[i]);

		if (client->jack_client_name)
			project_create_client_jack_patch_xml(project, client,
			                                     clientxml);
#ifdef HAVE_ALSA
		if (client->alsa_client_id)
			project_create_client_alsa_patch_xml(project, client,
			                                     clientxml);
#endif
		if (!list_empty(&client->dependencies))
			project_create_client_dependencies_xml(client,
			                                       clientxml);
	}

	return doc;
}

static __inline__ bool
project_write_info(project_t *project)
{
	xmlDocPtr doc;
	const char *filename;

	doc = project_create_xml(project);

	if (project->doc)
		xmlFree(project->doc);
	project->doc = doc;

	filename = lash_get_fqn(project->directory, PROJECT_INFO_FILE);

	if (xmlSaveFormatFile(filename, doc, 1) == -1) {
		lash_error("Cannot save project data to file %s: %s",
		           filename, strerror(errno));
		return false;
	}

	return true;
}

static void
project_clear_lost_clients(project_t *project)
{
	struct list_head *head, *node;
	client_t *client;

	head = &project->lost_clients;
	node = head->next;

	while (node != head) {
		client = list_entry(node, client_t, siblings);
		node = node->next;

		if (lash_dir_exists(client->data_path))
			lash_remove_dir(client->data_path);

		list_del(&client->siblings);
		client_destroy(client);
	}
}

static __inline__ void
project_load_notes(project_t *project)
{
	char *filename;
	char *data;

	filename = lash_dup_fqn(project->directory, PROJECT_NOTES_FILE);

	if (!lash_read_text_file((const char *) filename, &data))
		lash_error("Failed to read project notes from '%s'", filename);
	else {
		project->notes = data;
		lash_debug("Project notes: \"%s\"", data);
	}

	free(filename);
}

static __inline__ bool
project_save_notes(project_t *project)
{
	char *filename;
	FILE *file;
	size_t size;
	bool ret;

	ret = true;

	filename = lash_dup_fqn(project->directory, PROJECT_NOTES_FILE);

	file = fopen(filename, "w");
	if (!file) {
		lash_error("Failed to open '%s' for writing: %s",
		           filename, strerror(errno));
		ret = false;
		goto exit;
	}

	if (project->notes) {
		size = strlen(project->notes);

		if (fwrite(project->notes, size, 1, file) != 1) {
			lash_error("Failed to write %ld bytes of data "
			           "to file '%s'", size, filename);
			ret = false;
		}
	}

	fclose(file);

exit:
	free(filename);
	return ret;
}

void
project_save(project_t *project)
{
	if (project->task_type) {
		lash_error("Another task is in progress, cannot save right now");
		return;
	}

	lash_info("Saving project '%s' ...", project->name);

	/* Initialise the controllers' progress display */
	lashd_dbus_signal_emit_progress(0);

	if (list_empty(&project->siblings_all)) {
		/* this is first save for new project, add it to available for loading list */
		list_add_tail(&project->siblings_all, &g_server->all_projects);
	}

	if (!lash_dir_exists(project->directory)) {
		lash_create_dir(project->directory);
		lash_info("Created project directory %s", project->directory);
	}

	project_save_clients(project);

#ifdef HAVE_JACK_DBUS
	lashd_jackdbus_mgr_get_graph(g_server->jackdbus_mgr);
#endif

	if (!project_write_info(project)) {
		lash_error("Error writing info file for project '%s'; "
		           "aborting save", project->name);
		return;
	}

	if (!project_save_notes(project))
		lash_error("Error writing notes file for project '%s'", project->name);

	project_clear_lost_clients(project);
}

bool
project_load(project_t *project)
{
	xmlNodePtr projectnode, xmlnode;
	xmlChar *content = NULL;

	for (projectnode = project->doc->children; projectnode;
	     projectnode = projectnode->next) {
		if (projectnode->type == XML_ELEMENT_NODE
		    && strcmp((const char *) projectnode->name, "lash_project") == 0)
			break;
	}

	if (!projectnode) {
		lash_error("No root node in project XML document");
		return false;
	}

	for (xmlnode = projectnode->children; xmlnode;
	     xmlnode = xmlnode->next) {
		if (strcmp((const char *) xmlnode->name, "version") == 0) {
			/* FIXME: check version */
		} else if (strcmp((const char *) xmlnode->name, "name") == 0) {
			content = xmlNodeGetContent(xmlnode);
			lash_strset(&project->name, (const char *) content);
			xmlFree(content);
		} else if (strcmp((const char *) xmlnode->name, "client") == 0) {
			client_t *client;

			client = client_new();
			client_parse_xml(project, client, xmlnode);

			// TODO: reject client if its data doesn't contain
			//       the basic stuff (id, class, etc.)

			list_add(&client->siblings, &project->lost_clients);
		}
	}

	if (!project->name) {
		lash_error("No name node in project XML document");
		project_unload(project);
		return false;
	}

	project_load_notes(project);

#ifdef LASH_DEBUG
	{
		struct list_head *node;
		client_t *client;
		lash_list_t *list;
		int i;

		lash_debug("Restored project with:");
		lash_debug("  directory: '%s'", project->directory);
		lash_debug("  name:      '%s'", project->name);
		lash_debug("  clients:");
		list_for_each (node, &project->lost_clients) {
			client = list_entry(node, client_t, siblings);

			lash_debug("  ------");
			lash_debug("    id:          '%s'", client->id_str);
			lash_debug("    name:        '%s'", client->name);
			lash_debug("    working dir: '%s'", client->working_dir);
			lash_debug("    flags:       %d", client->flags);
			lash_debug("    argc:        %d", client->argc);
			lash_debug("    args:");
			for (i = 0; i < client->argc; i++) {
				lash_debug("      %d: '%s'", i, client->argv[i]);
			}
#ifdef HAVE_ALSA
			if (client->alsa_patches) {
				lash_debug("    alsa patches:");
				for (list = client->alsa_patches; list; list = list->next) {
					lash_debug("      %s",
					           alsa_patch_get_desc((alsa_patch_t *) list->data));
				}
			} else
				lash_debug("    no alsa patches");
#endif

			if (client->jack_patches) {
				lash_debug("    jack patches:");
				for (list = client->jack_patches; list; list = list->next) {
					lash_debug("      '%s' -> '%s'",
					           ((jack_patch_t *)list->data)->src_desc,
					           ((jack_patch_t *)list->data)->dest_desc);
				}
			} else
				lash_debug("    no jack patches");
		}
	}
#endif

	project->modified_status = false;

	return true;
}

project_t *
project_new_from_disk(const char *parent_dir,
                      const char *project_dir)
{
	project_t *project;
	struct stat st;
	char *filename = NULL;
	xmlNodePtr projectnode, xmlnode;
	xmlChar *content = NULL;

	project = project_new();

	INIT_LIST_HEAD(&project->siblings_loaded);

	lash_strset(&project->name, project_dir);

	project->directory = lash_dup_fqn(parent_dir, project_dir);

	if (stat(project->directory, &st) == 0) {
		project->last_modify_time = st.st_mtime;
	} else {
		lash_error("failed to stat '%s'", project->directory);
		goto fail;
	}

	filename = lash_dup_fqn(project->directory, PROJECT_INFO_FILE);
	if (!lash_file_exists(filename)) {
		lash_error("Project '%s' has no info file", project->name);
		goto fail;
	}

	project->doc = xmlParseFile(filename);
	if (project->doc == NULL) {
		lash_error("Could not parse file %s", filename);
		goto fail;
	}

	lash_free(&filename);

	for (projectnode = project->doc->children; projectnode;
	     projectnode = projectnode->next) {
		if (projectnode->type == XML_ELEMENT_NODE
		    && strcmp((const char *) projectnode->name, "lash_project") == 0)
			break;
	}

	if (!projectnode) {
		lash_error("No root node in project XML document");
		goto fail;
	}

	for (xmlnode = projectnode->children; xmlnode;
	     xmlnode = xmlnode->next) {
		if (strcmp((const char *) xmlnode->name, "version") == 0) {
			/* FIXME: check version */
		} else if (strcmp((const char *) xmlnode->name, "name") == 0) {
			content = xmlNodeGetContent(xmlnode);
			lash_strset(&project->name, (const char *) content);
			xmlFree(content);
		} else if (strcmp((const char *) xmlnode->name, "description") == 0) {
			content = xmlNodeGetContent(xmlnode);
			lash_strset(&project->description, (const char *)content);
			xmlFree(content);
		}
	}

	if (!project->name) {
		lash_error("No name node in project XML document");
		goto fail;
	}

	return project;

fail:
	lash_free(&filename);
	project_destroy(project);
	return NULL;
}

bool
project_is_loaded(project_t *project)
{
	return !list_empty(&project->siblings_loaded);
}

void
project_lose_client(project_t *project,
                    client_t  *client)
{
	lash_list_t *patches;

	lash_info("Losing client '%s'", client_get_identity(client));

	if (CLIENT_CONFIG_DATA_SET(client) && client->store) {
		if (!list_empty(&client->store->keys))
			store_write(client->store);
		else if (lash_dir_exists(client->store->dir))
			lash_remove_dir(client->store->dir);
	}

	if (CLIENT_CONFIG_DATA_SET(client) || CLIENT_CONFIG_FILE(client)) {
		const char *dir = (const char *) client->data_path;
		if (lash_dir_exists(dir) && lash_dir_empty(dir))
			lash_remove_dir(dir);

		dir = lash_get_fqn(project->directory, PROJECT_ID_DIR);
		if (lash_dir_exists(dir) && lash_dir_empty(dir))
			lash_remove_dir(dir);
	}

	if (client->jack_client_name) {
#ifdef HAVE_JACK_DBUS
		lashd_jackdbus_mgr_remove_client(g_server->jackdbus_mgr,
		                                 client->id, &patches);
#else
		jack_mgr_lock(g_server->jack_mgr);
		patches = jack_mgr_remove_client(g_server->jack_mgr, client->id);
		jack_mgr_unlock(g_server->jack_mgr);
#endif
		if (patches) {
			client->jack_patches = lash_list_concat(client->jack_patches, patches);
#ifdef LASH_DEBUG
			lash_debug("Backed-up JACK patches:");
			jack_patch_list(client->jack_patches);
#endif
		}
	}

#ifdef HAVE_ALSA
	if (client->alsa_client_id) {
		alsa_mgr_lock(g_server->alsa_mgr);
		patches = alsa_mgr_remove_client(g_server->alsa_mgr, client->id);
		alsa_mgr_unlock(g_server->alsa_mgr);
		if (patches)
			client->alsa_patches = lash_list_concat(client->alsa_patches, patches);
	}
#endif

	dbus_free_string_array(client->argv);
	client->argc = 0;
	client->argv = NULL;

	client->pid = 0;

	list_add(&client->siblings, &project->lost_clients);
	lashd_dbus_signal_emit_client_disappeared(client->id_str, project->name);
}

void
project_unload(project_t *project)
{
	struct list_head *head, *node;
	lash_list_t *patches, *pnode;
	client_t *client;

	list_del_init(&project->siblings_loaded);

	lash_debug("Signaling all clients of project '%s' to quit",
	           project->name);

	signal_new_single(g_server->dbus_service,
	                  "/", "org.nongnu.LASH.Server", "Quit",
	                  DBUS_TYPE_STRING, &project->name);

	head = &project->clients;
	node = head->next;

	while (node != head) {
		client = list_entry(node, client_t, siblings);
		node = node->next;

		if (client->jack_client_name) {
#ifdef HAVE_JACK_DBUS
			lashd_jackdbus_mgr_remove_client(g_server->jackdbus_mgr,
			                                 client->id, NULL);
#else
			jack_mgr_lock(g_server->jack_mgr);
			patches =
			  jack_mgr_remove_client(g_server->jack_mgr,
			                         client->id);
			jack_mgr_unlock(g_server->jack_mgr);

			for (pnode = patches; pnode; pnode = lash_list_next(pnode))
				jack_patch_destroy((jack_patch_t *) pnode->data);
			lash_list_free(patches);
#endif
		}

#ifdef HAVE_ALSA
		if (client->alsa_client_id) {
			alsa_mgr_lock(g_server->alsa_mgr);
			patches =
			  alsa_mgr_remove_client(g_server->alsa_mgr, client->id);
			alsa_mgr_unlock(g_server->alsa_mgr);

			for (pnode = patches; pnode; pnode = lash_list_next(pnode))
				alsa_patch_destroy((alsa_patch_t *) pnode->data);
			lash_list_free(patches);
		}
#endif

		list_del(&client->siblings);
		client_destroy(client);
	}

	head = &project->lost_clients;
	node = head->next;

	while (node != head) {
		client = list_entry(node, client_t, siblings);
		node = node->next;
		list_del(&client->siblings);
		// TODO: Do lost clients also need to have their
		//       JACK and ALSA patches destroyed?
		client_destroy(client);
	}

	lash_info("Project '%s' unloaded", project->name);

	if (project->doc == NULL && lash_dir_exists(project->directory))
	{
		lash_info("Removing directory '%s' of closed newborn project '%s'", project->directory, project->name);
		lash_remove_dir(project->directory);
	}
}

void
project_destroy(project_t *project)
{
	if (project) {
		lash_info("Destroying project '%s'", project->name);

		if (project_is_loaded(project))
			project_unload(project);

		if (project->doc)
			xmlFree(project->doc);

		lash_free(&project->name);
		lash_free(&project->directory);
		lash_free(&project->description);
		lash_free(&project->notes);

		// TODO: Free client lists

		free(project);
	}
}

/* Calculate a new overall progress reading for the project's current task */
void
project_client_progress(project_t *project,
                        client_t  *client,
                        uint8_t    percentage)
{
	if (client->task_progress)
		project->client_tasks_progress -= client->task_progress;

	project->client_tasks_progress += percentage;
	client->task_progress = percentage;

	/* Prevent divide by 0 */
	if (!project->client_tasks_total)
		return;

	uint8_t p = project->client_tasks_progress / project->client_tasks_total;
	lashd_dbus_signal_emit_progress(p > 99 ? 99 : p);
}

/* Set modified_status to new_status, emit signal if status changed */
static void
project_set_modified_status(project_t *project,
                            bool       new_status)
{
	if (project->modified_status == new_status)
		return;

	dbus_bool_t value = new_status;
	project->modified_status = new_status;

	signal_new_valist(g_server->dbus_service,
	                  "/", "org.nongnu.LASH.Control",
	                  "ProjectModifiedStatusChanged",
	                  DBUS_TYPE_STRING, &project->name,
	                  DBUS_TYPE_BOOLEAN, &value,
	                  DBUS_TYPE_INVALID);
}

/* Send the appropriate signal(s) to signify that a client completed a task */
void
project_client_task_completed(project_t *project,
                              client_t  *client)
{
	/* Calculate new progress reading and send Progress signal */
	project_client_progress(project, client, 100);

	/* If the project task is finished emit the appropriate signals */
	if (project->client_tasks_pending && (--project->client_tasks_pending) == 0) {
		uint8_t x;

		project->task_type = 0;
		project->client_tasks_total = 0;
		project->client_tasks_progress = 0;

		/* Send ProjectSaved or ProjectLoaded signal, or return if the task was neither */
		switch (client->task_type) {
		case LASH_Save_Data_Set: case LASH_Save_File:
			lashd_dbus_signal_emit_project_saved(project->name);
			lash_info("Project '%s' saved.", project->name);
			break;
		case LASH_Restore_File: case LASH_Restore_Data_Set:
			lashd_dbus_signal_emit_project_loaded(project->name);
			lash_info("Project '%s' loaded.", project->name);
			break;
		default:
			return;
		}

		project_set_modified_status(project, false);

		/* Reset the controllers' progress display */
		lashd_dbus_signal_emit_progress(0);
	}
}

void
project_rename(project_t  *project,
               const char *new_name)
{
	char *old_name = project->name;
	project->name = lash_strdup(new_name);

	lashd_dbus_signal_emit_project_name_changed(old_name, new_name);

	project_set_modified_status(project, true);

	lash_free(&old_name);
}

void
project_set_description(project_t  *project,
                        const char *description)
{
	lash_strset(&project->description, description);
	lashd_dbus_signal_emit_project_description_changed(project->name, description);

	project_set_modified_status(project, true);
}

void
project_set_notes(project_t  *project,
                  const char *notes)
{
	lash_strset(&project->notes, notes);
	lashd_dbus_signal_emit_project_notes_changed(project->name, notes);

	project_set_modified_status(project, true);
}

void
project_rename_client(project_t  *project,
                      client_t   *client,
                      const char *name)
{
	if (strcmp(client->name, name) == 0)
		return;

	lash_strset(&client->name, name);
	lashd_dbus_signal_emit_client_name_changed(client->id_str, client->name);
}

void
project_clear_id_dir(project_t *project)
{
	if (project) {
		const char *dir = lash_get_fqn(project->directory,
		                               PROJECT_ID_DIR);
		if (lash_dir_exists(dir))
			lash_remove_dir(dir);
	}
}

/* EOF */
