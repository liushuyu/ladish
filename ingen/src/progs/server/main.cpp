/* This file is part of Ingen.
 * Copyright (C) 2007 Dave Robillard <http://drobilla.net>
 * 
 * Ingen is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Ingen is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <iostream>
#include <cstddef>
#include <signal.h>
#include "config.h"
#include "module/Module.h"
#include "engine/util.h"
#include "engine/Engine.h"
#include "engine/EventSource.h"
#ifdef HAVE_LASH
#include "engine/LashDriver.h"
#endif
#ifdef BUILD_IN_PROCESS_ENGINE
#include <jack/jack.h>
#include <jack/intclient.h>
#endif
#include "cmdline.h"

using std::cout; using std::endl; using std::cerr;
using namespace Ingen;

SharedPtr<Engine> engine;


void
catch_int(int)
{
	signal(SIGINT, catch_int);
	signal(SIGTERM, catch_int);

	std::cout << "[Main] Ingen interrupted." << std::endl;
	engine->quit();
}


#ifdef BUILD_IN_PROCESS_ENGINE

jack_client_t*   jack_client;
jack_intclient_t jack_intclient;


void
unload_in_process_engine(int)
{
	jack_status_t status;
	int ret = EXIT_SUCCESS;

	cout << "Unloading...";
	status = jack_internal_client_unload(jack_client, jack_intclient);
	if (status & JackFailure) {
		cout << "failed" << endl;
		ret = EXIT_FAILURE;
	} else {
		cout << "done" << endl;
	}
	jack_client_close(jack_client);
	exit(ret);
}


int
load_in_process_engine(const char* port)
{
	int ret = EXIT_SUCCESS;
	
	jack_status_t status;

	if ((jack_client = jack_client_open("om_load", JackNoStartServer,
	                                    &status)) != NULL) {
		jack_intclient =
		    jack_internal_client_load(jack_client, "Ingen",
		                               (jack_options_t)(JackLoadName|JackLoadInit),
		                               &status, "om", port);
		if (status == 0) {
			cout << "Engine loaded" << endl;
			signal(SIGINT, unload_in_process_engine);
			signal(SIGTERM, unload_in_process_engine);

			while (1) {
				sleep(1);
			}
		} else if (status & JackFailure) {
			cerr << "Could not load om.so" << endl;
			ret = EXIT_FAILURE;
		}

		jack_client_close(jack_client);
	} else {
		cerr << "jack_client_open failed" << endl;
		ret = EXIT_FAILURE;
	}
}

#endif // BUILD_IN_PROCESS_ENGINE


int
main(int argc, char** argv)
{
#ifdef HAVE_LASH
	lash_args_t* lash_args = lash_extract_args(&argc, &argv);
#endif

	int ret = EXIT_SUCCESS;

	/* Parse command line options */
	gengetopt_args_info args_info;
	if (cmdline_parser (argc, argv, &args_info) != 0)
		return EXIT_FAILURE;


	if (args_info.in_jackd_flag) {
#ifdef BUILD_IN_PROCESS_ENGINE
		ret = load_in_process_engine(args_info.port_arg);
#else
		cerr << "In-process Jack client support not enabled in this build." << endl;
		ret = EXIT_FAILURE;
#endif
	} else {
		signal(SIGINT, catch_int);
		signal(SIGTERM, catch_int);

		set_denormal_flags();

		SharedPtr<Glib::Module> module = Ingen::Shared::load_module("ingen_engine");

		if (!module) {
			cerr << "Aborting.  If you are running from the source tree, run ingen_dev." << endl;
			return -1;
		}

		Engine* (*new_engine)() = NULL;

		bool found = module->get_symbol("new_engine", (void*&)new_engine);

		if (!found) {
			cerr << "Unable to find module entry point, exiting." << endl;
			return -1;
		}

		SharedPtr<Engine> engine(new_engine());

		engine->start_jack_driver();
		engine->start_osc_driver(args_info.port_arg);

		engine->activate();

#ifdef HAVE_LASH
		lash_driver = new LashDriver(engine, lash_args);
#endif

		engine->main();

		engine->event_source()->deactivate();

#ifdef HAVE_LASH
		delete lash_driver;
#endif

	}
	
	return ret;
}

