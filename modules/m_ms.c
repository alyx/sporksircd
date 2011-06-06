/*
 * Copyright (C) 2011 Elizabeth J. Myers. All rights reserved.
 * Copyright (C) 2011 the SporksIRCD team. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "stdinc.h"
#include "modules.h"
#include "numeric.h"
#include "client.h"
#include "chmode.h"
#include "ircd.h"
#include "send.h"
#include "s_user.h"
#include "s_serv.h"

static int ms_ms(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

static int modinit(void);
static void modfini(void);

struct Message ms_msgtab = {
	"MS", 0, 0, 0, MFLG_SLOW,
	{mg_ignore, mg_ignore, mg_ignore, {ms_ms, 6}, mg_ignore, mg_ignore}
};

mapi_clist_av1 ms_clist[] = { &ms_msgtab, NULL };

DECLARE_MODULE_AV1(ms, modinit, modfini, ms_clist, NULL, NULL, "SporksIRCD development team");

static int
modinit(void)
{
	add_capability("MS", CAP_MS, YES, NO);
	return 0;
}

static void
modfini(void)
{
	delete_capability("MS");
}

static int ms_ms(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	/* No parameters - we'll assume it's a query */
	if (parc == 1)
	{
		sendto_one(client_p, ":%s MS %s %s %s %s %s", me.id,
			   cmodes_a, cmodes_b, cmodes_c, cmodes_d, umodebuf);
		return 0;
	}
	else if (parc < 5)
	{
		exit_client(client_p, client_p, &me, "Insufficient MS parameters");
		return 0;
	}

	/* parv[1] = ISUPPORT A
	 * parv[2] = ISUPPORT B
	 * parv[3] = ISUPPORT C
	 * parv[4] = ISUPPORT D
	 * parv[5] = UMODES (not found for older servers)
	 *
	 * We only drop for incompatible A/B/C modes. D modes aren't dropped because
	 * I don't see the point. TBD: Maybe we should drop? --Elizabeth
	 */

	if(strcmp(parv[1], cmodes_a) != 0)
	{
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "Incompatible class A modes (ours: %s theirs: %s)",
				       cmodes_a, parv[1]);
		exit_client(client_p, client_p, &me, "Incompatible class A modes");
		return 0;
	}
	if(strcmp(parv[2], cmodes_b) != 0)
	{
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "Incompatible class B modes (ours: %s theirs: %s)",
				       cmodes_b, parv[2]);
		exit_client(client_p, client_p, &me, "Incompatible class B modes");
		return 0;
	}
	if(strcmp(parv[3], cmodes_c) != 0)
	{
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "Incompatible class C modes (ours: %s theirs: %s)",
				       cmodes_c, parv[3]);
		exit_client(client_p, client_p, &me, "Incompatible class C modes");
		return 0;
	}
	
	/* Let's warn anyway */
	if(strcmp(parv[4], cmodes_d) != 0)
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
				       "Warning: server %s's class D modes don't match (ours: %s theirs: %s)! Linking anyway.",
				       client_p->name, cmodes_d, parv[4]);

	/* Warn about incompatible umodes */
	if(parc > 5 && strcmp(parv[5], umodebuf) != 0)
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
				       "Warning: server %s's user modes don't match (ours: %s theirs: %s)! Linking anyway.",
				       client_p->name, umodebuf, parv[5]);

	return 0;
}

