/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <locale.h>
#include <signal.h>
#include <libgen.h>

#include <haka/packet_module.h>
#include <haka/thread.h>
#include <haka/error.h>
#include <haka/alert.h>
#include <luadebug/debugger.h>

#include "app.h"


static struct thread_pool *thread_states;
static char *configuration_file;
static int ret_rc = 0;


void basic_clean_exit()
{
	set_configuration_script(NULL);

	if (thread_states) {
		thread_pool_cleanup(thread_states);
		thread_states = NULL;
	}

	set_packet_module(NULL);
	remove_all_logger();
}

static void fatal_error_signal(int sig)
{
	static volatile sig_atomic_t fatal_error_in_progress = 0;

	printf("\n");

	if (sig == SIGINT) {
		if (luadebug_debugger_breakall()) {
			message(HAKA_LOG_FATAL, L"debug", L"break (hit ^C again to kill)");
			return;
		}
		else {
			luadebug_debugger_shutdown();
		}
	}

	if (fatal_error_in_progress)
		raise(sig);
	fatal_error_in_progress = 1;

	if (sig != SIGTERM) {
		messagef(HAKA_LOG_FATAL, L"core", L"fatal signal received (sig=%d)", sig);
	}
	else {
		messagef(HAKA_LOG_INFO, L"core", L"terminate signal received");
	}

	if (thread_states) {
		if (thread_pool_issingle(thread_states)) {
			clean_exit();
			exit(1);
		}
		else {
			thread_pool_cancel(thread_states);
			ret_rc = 1;
		}
	}
	else {
		clean_exit();
		exit(1);
	}
}

static void handle_sighup()
{
	enable_stdout_logging(false);
	enable_stdout_alert(false);
}

const char *haka_path()
{
	const char *haka_path = getenv("HAKA_PATH");
	return haka_path ? haka_path : HAKA_PREFIX;
}

void initialize()
{
	/* Set locale */
	setlocale(LC_ALL, "");

	/* Install signal handler */
	signal(SIGTERM, fatal_error_signal);
	signal(SIGINT, fatal_error_signal);
	signal(SIGQUIT, fatal_error_signal);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, handle_sighup);

	/* Default module path */
	{
		static const char *HAKA_CORE_PATH = "/share/haka/core/*";
		static const char *HAKA_MODULE_PATH = "/share/haka/modules/*";

		size_t path_len;
		char *path;
		const char *haka_path_s = haka_path();

		path_len = 2*strlen(haka_path_s) + strlen(HAKA_CORE_PATH) + 1 +
				strlen(HAKA_MODULE_PATH) + 1;

		path = malloc(path_len);
		if (!path) {
			fprintf(stderr, "memory allocation error\n");
			clean_exit();
			exit(1);
		}

		snprintf(path, path_len, "%s%s;%s%s", haka_path_s, HAKA_CORE_PATH,
				haka_path_s, HAKA_MODULE_PATH);

		module_set_path(path);

		free(path);
	}
}

void prepare(int threadcount, bool attach_debugger)
{
	struct packet_module *packet_module = get_packet_module();
	assert(packet_module);

	if (threadcount == -1) {
		threadcount = thread_get_packet_capture_cpu_count();
		if (!packet_module->multi_threaded()) {
			threadcount = 1;
		}
	}

	messagef(HAKA_LOG_INFO, L"core", L"loading rule file '%s'", configuration_file);

	/* Add module path to the configuration folder */
	{
		char *module_path;

		module_path = malloc(strlen(configuration_file) + 3);
		assert(module_path);
		strcpy(module_path, configuration_file);
		dirname(module_path);
		strcat(module_path, "/*");

		module_add_path(module_path);
		if (check_error()) {
			message(HAKA_LOG_FATAL, L"core", clear_error());
			free(module_path);
			clean_exit();
			exit(1);
		}

		free(module_path);
		module_path = NULL;
	}

	thread_states = thread_pool_create(threadcount, packet_module, attach_debugger);
	if (check_error()) {
		message(HAKA_LOG_FATAL, L"core", clear_error());
		clean_exit();
		exit(1);
	}

	if (threadcount > 1) {
		messagef(HAKA_LOG_INFO, L"core", L"starting multi-threaded processing on %i threads\n", threadcount);
	}
	else {
		message(HAKA_LOG_INFO, L"core", L"starting single threaded processing\n");
	}
}

void start()
{
	thread_pool_start(thread_states);
	if (check_error()) {
		message(HAKA_LOG_FATAL, L"core", clear_error());
		clean_exit();
		exit(1);
	}

	if (ret_rc) {
		clean_exit();
		exit(ret_rc);
	}
}

struct thread_pool *get_thread_pool()
{
	return thread_states;
}

int set_configuration_script(const char *file)
{
	free(configuration_file);
	configuration_file = NULL;

	if (file) {
		configuration_file = strdup(file);
	}

	return 0;
}

const char *get_configuration_script()
{
	return configuration_file;
}

char directory[1024];

const char *get_app_directory()
{
	return directory;
}

void dump_stat(FILE *file)
{
	thread_pool_dump_stat(thread_states, file);
}
