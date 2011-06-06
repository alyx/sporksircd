/*
 * Server name extban type: bans all users using a certain server
 * -- jilles
 *
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "ircd.h"

static int _modinit(void);
static void _moddeinit(void);
static int eb_server(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

DECLARE_MODULE_AV1(extb_server, _modinit, _moddeinit, NULL, NULL, NULL, "SporksIRCD development team");

static int
_modinit(void)
{
	return extban_add('s', eb_server);
}

static void
_moddeinit(void)
{
	extban_delete('s');
}

static int eb_server(const char *data, struct Client *client_p,
		struct Channel *chptr, long mode_type)
{

	(void)chptr;
	/* This type is not safe for exceptions */
	if (mode_type == CHFL_EXCEPTION || mode_type == CHFL_INVEX)
		return EXTBAN_INVALID;
	if (data == NULL)
		return EXTBAN_INVALID;
	return match(data, me.name) ? EXTBAN_MATCH : EXTBAN_NOMATCH;
}
