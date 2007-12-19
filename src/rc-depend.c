/*
   rc-depend
   rc service dependency and ordering
   */

/* 
 * Copyright 2007 Roy Marples
 * All rights reserved

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <getopt.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtins.h"
#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"
#include "strlist.h"

static const char *applet;

rc_depinfo_t *_rc_deptree_load (int *regen) {
	if (rc_deptree_update_needed ()) {
		int retval;

		if (regen)
			*regen = 1;

		ebegin ("Caching service dependencies");
		retval = rc_deptree_update ();
		eend (retval ? 0 : -1, "Failed to update the dependency tree");
	}

	return (rc_deptree_load ());
}

#include "_usage.h"
#define getoptstring "t:suT" getoptstring_COMMON
static struct option longopts[] = {
	{ "type",     1, NULL, 't'},
	{ "notrace",  0, NULL, 'T'},
	{ "strict",   0, NULL, 's'},
	{ "update",   0, NULL, 'u'},
	longopts_COMMON
};
static const char * const longopts_help[] = {
	"Type(s) of dependency to list",
	"Don't trace service dependencies",
	"Only use what is in the runlevels",
	"Force an update of the dependency tree",
	longopts_help_COMMON
};
#include "_usage.c"

int rc_depend (int argc, char **argv)
{
	char **types = NULL;
	char **services = NULL;
	char **depends = NULL;
	char **list;
	rc_depinfo_t *deptree = NULL;
	char *service;
	int options = RC_DEP_TRACE;
	bool first = true;
	int i;
	bool update = false;
	char *runlevel = xstrdup( getenv ("RC_SOFTLEVEL"));
	int opt;
	char *token;

	applet = cbasename (argv[0]); 

	while ((opt = getopt_long (argc, argv, getoptstring,
							   longopts, (int *) 0)) != -1)
	{
		switch (opt) {
			case 's':
				options |= RC_DEP_STRICT;
				break;
			case 't':
				while ((token = strsep (&optarg, ",")))
					rc_strlist_addu (&types, token);
				break;
			case 'u':
				update = true;
				break;
			case 'T':
				options &= RC_DEP_TRACE;
				break;

			case_RC_COMMON_GETOPT
		}
	}

	if (update) {
		bool u = false;
		ebegin ("Caching service dependencies");
		u = rc_deptree_update ();
		eend (u ? 0 : -1, "%s: %s", applet, strerror (errno));
		if (! u)
			eerrorx ("Failed to update the dependency tree");
	}

	if (! (deptree = _rc_deptree_load (NULL)))
		eerrorx ("failed to load deptree");

	if (! runlevel)
		runlevel = rc_runlevel_get ();

	while (optind < argc) {
		list = NULL;
		rc_strlist_add (&list, argv[optind]);
		errno = 0;
		depends = rc_deptree_depends (deptree, NULL, (const char **) list,
									  runlevel, 0);
		if (! depends && errno == ENOENT)
			eerror ("no dependency info for service `%s'", argv[optind]);
		else
			rc_strlist_add (&services, argv[optind]);

		rc_strlist_free (depends);
		rc_strlist_free (list);
		optind++;
	}

	if (! services) {
		rc_strlist_free (types);
		rc_deptree_free (deptree);
		free (runlevel);
		if (update)
			return (EXIT_SUCCESS);
		eerrorx ("no services specified");
	}

	/* If we don't have any types, then supply some defaults */
	if (! types) {
		rc_strlist_add (&types, "ineed");
		rc_strlist_add (&types, "iuse");
	}

	depends = rc_deptree_depends (deptree, (const char **) types,
								  (const char **) services, runlevel, options);

	if (depends) {
		STRLIST_FOREACH (depends, service, i) {
			if (first)
				first = false;
			else
				printf (" ");

			if (service)
				printf ("%s", service);

		}
		printf ("\n");
	}

	rc_strlist_free (types);
	rc_strlist_free (services);
	rc_strlist_free (depends);
	rc_deptree_free (deptree);
	free (runlevel);
	return (EXIT_SUCCESS);
}
