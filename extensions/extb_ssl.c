/* SSL extban type: matches ssl users */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "ircd.h"

static int _modinit(void);
static void _moddeinit(void);
static int eb_ssl(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

DECLARE_MODULE_AV1(extb_ssl, _modinit, _moddeinit, NULL, NULL, NULL, "SporksIRCD development team");

static int
_modinit(void)
{
	return extban_add('z', eb_ssl);
}

static void
_moddeinit(void)
{
	extban_delete('z');
}

static int eb_ssl(const char *data, struct Client *client_p,
		struct Channel *chptr, long mode_type)
{

	(void)chptr;
	(void)mode_type;
	if (data != NULL)
		return EXTBAN_INVALID;
	return IsSSLClient(client_p) ? EXTBAN_MATCH : EXTBAN_NOMATCH;
}
