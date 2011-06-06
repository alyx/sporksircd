/*
 * m_remove: implement ircd-seven style REMOVE
 * Derived from m_kick.
 *
 * Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 * Copyright (C) 1996-2002 Hybrid Development Team
 * Copyright (C) 2002-2005 ircd-ratbox development team
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 */

#include "stdinc.h"
#include "channel.h"
#include "client.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "msg.h"
#include "modules.h"
#include "parse.h"
#include "hash.h"
#include "packet.h"
#include "s_serv.h"
#include "s_conf.h"
#include "hook.h"

static int m_remove(struct Client *, struct Client *, int, const char **);
#define mg_remove { m_remove, 3 }

static int
_modinit(void)
{
	add_capability("REMOVE", CAP_REMOVE, YES, NO);
	return 0;
}

static void
_modfini(void)
{
	delete_capability("REMOVE");
}

struct Message remove_msgtab = {
	"REMOVE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_remove, mg_remove, mg_remove, mg_ignore, mg_remove}
};

mapi_clist_av1 remove_clist[] = { &remove_msgtab, NULL };

DECLARE_MODULE_AV1(remove, _modinit, _modfini, remove_clist, NULL, NULL, "SporksIRCD development team");

static int
m_remove(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct membership *msptr;
	struct Client *who;
	struct Channel *chptr;
	int chasing = 0;
	char *comment;
	const char *name = parv[1], *user = parv[2];
	char *p = NULL;
	char text[10];
	static char buf[BUFSIZE];

	if(MyClient(source_p) && !IsFloodDone(source_p))
		flood_endgrace(source_p);

	*buf = '\0';
	if((p = strchr(parv[1], ',')))
		*p = '\0';

	if((chptr = find_channel(name)) == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL, form_str(ERR_NOSUCHCHANNEL), name);
		return 0;
	}

	if(!(who = find_chasing(source_p, user, &chasing)))
		return 0;

	if(!IsServer(source_p))
	{
		if(((msptr = find_channel_membership(chptr, source_p)) == NULL) && MyConnect(source_p))
		{
			sendto_one_numeric(source_p, ERR_NOTONCHANNEL,
					   form_str(ERR_NOTONCHANNEL), name);
			return 0;
		}

		if(!can_kick(source_p, msptr, find_channel_membership(chptr, who)))
		{
			if(MyConnect(source_p))
			{
				sendto_one(source_p, ":%s 482 %s %s :You do not have the proper privileges to remove this user",
					   me.name, source_p->name, name);
				return 0;
			}

			/* If its a TS 0 channel, do it the old way */
			else if(chptr->channelts == 0)
			{
				sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
					   get_id(&me, source_p), get_id(source_p, source_p), name);
				return 0;
			}
		}
	}

	if((p = strchr(parv[2], ',')))
		*p = '\0';

	if((msptr = find_channel_membership(chptr, who)) != NULL)
	{
		if(MyClient(source_p) && IsService(who))
		{
			sendto_one(source_p, form_str(ERR_ISCHANSERVICE),
				   me.name, source_p->name, who->name, chptr->chname);
			return 0;
		}

		if(MyClient(source_p) && (chptr->mode.mode & MODE_NOKICK))
		{
			sendto_one_numeric(source_p, ERR_NOKICK,
					form_str(ERR_NOKICK),
					chptr->chname);
			return 0;
		}

		if (MyClient(source_p) && (chptr->mode.mode & MODE_NOOPERKICK && IsOper(who)))
		{
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
					"Overriding remove from %s on %s in %s (channel is +M)",
					source_p->name, who->name, chptr->chname);
			sendto_one_numeric(source_p, ERR_ISCHANSERVICE,
					"%s %s :Cannot remove IRC operators from that channel.",
					who->name, chptr->chname);
			return 0;
		}

		if(MyClient(source_p))
		{
			hook_data_channel_approval hookdata;

			hookdata.client = source_p;
			hookdata.chptr = chptr;
			hookdata.msptr = msptr;
			hookdata.target = who;
			hookdata.approved = 1;

			call_hook(h_can_kick, &hookdata);

			if (!hookdata.approved)
				return 0;
		}

		comment = LOCAL_COPY((EmptyString(parv[3])) ? who->name : parv[3]);
		if(strlen(comment) > (size_t) REASONLEN)
			comment[REASONLEN] = '\0';

		sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s PART %s :requested by %s (%s)",
				     who->name, who->username, who->host, name, source_p->name,
				     comment);

		sendto_server(client_p, chptr, CAP_REMOVE, NOCAPS,
			      ":%s REMOVE %s %s :%s",
			      use_id(source_p), chptr->chname, use_id(who), comment);

		sendto_server(client_p, chptr, NOCAPS, CAP_REMOVE,
			      ":%s KICK %s %s :%s",
			      use_id(source_p), chptr->chname, use_id(who), comment);

		remove_user_from_channel(msptr);

		rb_snprintf(text, sizeof(text), "K%s", who->id);

		/* we don't need to track NOREJOIN stuff unless it's our client being removed */
		if(MyClient(who) && chptr->mode.mode & MODE_NOREJOIN)
			channel_metadata_time_add(chptr, text, rb_current_time(), "KICKNOREJOIN");
	}
	else if (MyClient(source_p))
		sendto_one_numeric(source_p, ERR_USERNOTINCHANNEL,
				   form_str(ERR_USERNOTINCHANNEL), user, name);

	return 0;
}

