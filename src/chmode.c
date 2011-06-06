/*
 * SporksIRCD: the ircd for discerning transsexual quilting bees.
 * chmode.c: channel mode management
 *
 * Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 * Copyright (C) 1996-2002 Hybrid Development Team
 * Copyright (C) 2002-2005 ircd-ratbox development team
 * Copyright (C) 2005-2006 charybdis development team
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
 *
 */

#include "stdinc.h"
#include "channel.h"
#include "list.h"
#include "client.h"
#include "common.h"
#include "hash.h"
#include "hook.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "s_serv.h"
#include "s_user.h"
#include "send.h"
#include "whowas.h"
#include "s_conf.h"		/* ConfigFileEntry, ConfigChannel */
#include "s_newconf.h"
#include "logger.h"
#include "chmode.h"
#include "supported.h"

/* Contains A-Za-z except beoqvI etc. */
static int maxmodes_simple;

struct ChModeChange mode_changes[BUFSIZE];
int mode_count;
int mode_limit;
static int mode_limit_simple;
static int mask_pos;
static int removed_mask_pos;

/* XXX there must be a better way... */
char cmodes_a[128];
char cmodes_b[128];
char cmodes_c[128];
char cmodes_d[128];
char cmodes_prefix[128];
char cmodes_all[128];
char cmodes_params[128];

int chmode_flags[128];

extern int h_get_channel_access;
extern int h_channel_metadata_delete;

/*
 * generate_params_string
 *
 * inputs	- NONE
 * outputs	- NONE
 * side effects	- the cmode_params string is regenerated
 */
static void
generate_params_string(void)
{
	char *params_string = cmodes_params;
	int mode;

	for(mode = 0; mode < 128; mode++)
	{
		switch (chmode_table[mode].isupport_type)
		{
		case ISUPPORT_A:
		case ISUPPORT_B:
		case ISUPPORT_C:
		case ISUPPORT_PREFIX:
			*params_string++ = (char) mode;
		default:
			continue;
		}
	}

	/* Regenerate the ISUPPORT cache */
	if(isupport_regen)
		cache_isupport();

	return;
}

/*
 * generate_cmode_string
 *
 * inputs	- ISUPPORT type of mode that is being added/removed
 * outputs	- NONE
 * side effects	- the cmodes_X and cmodes_all strings are regenerated
 */
void
generate_cmode_string(void)
{
	char *all_string = cmodes_all;
	char *string_a, *string_b, *string_c, *string_d, *string_prefix;
	int mode;

	string_a = cmodes_a;
	string_b = cmodes_b;
	string_c = cmodes_c;
	string_d = cmodes_d;
	string_prefix = cmodes_prefix;

	/* We're rebuilding maxmodes_simple too
	 * XXX does this belong here */
	maxmodes_simple = 0;

	for(mode = 0; mode < 128; mode++)
	{
		/* By default chmode_flags for this mode is 0 */
		chmode_flags[mode] = 0;

		/* Update the appropriate string */
		switch (chmode_table[mode].isupport_type)
		{
			/* Note that maxmodes_simple is NOT incremented for
			 * ISUPPORT_A or ISUPPORT_PREFIX. --Elizabeth
			 */
		case ISUPPORT_INVALID:
			/* We can't do anything with invalid modes */
			continue;
		case ISUPPORT_A:
			*string_a++ = (char) mode;
			break;
		case ISUPPORT_B:
			*string_b++ = (char) mode;
			maxmodes_simple++;
			break;
		case ISUPPORT_C:
			*string_c++ = (char) mode;
			maxmodes_simple++;
			break;
		case ISUPPORT_PREFIX:
			*string_prefix++ = (char) mode;
			break;
		case ISUPPORT_D:
			*string_d++ = (char) mode;
			/* Update chmode_flags */
			chmode_flags[mode] = chmode_table[mode].mode_type;
			maxmodes_simple++;
			break;
		default:
			iwarn("generate_cmode_string() called with unknown type %ld!",
			      chmode_table[mode].isupport_type);
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
					       "generate_cmode_string() called with an unknown type (%ld)!",
					       chmode_table[mode].isupport_type);
			continue;
		}

		/* Update cmodes_all as this is a valid mode */
		*all_string++ = (char) mode;
	}

	/* Null terminator */
	*all_string++ = '\0';
	*string_a++ = '\0';
	*string_b++ = '\0';
	*string_c++ = '\0';
	*string_d++ = '\0';
	*string_prefix++ = '\0';

	generate_params_string();

	return;
}

/*
 * find_umode_slot
 *
 * inputs       - NONE
 * outputs      - an available cmode bitmask or
 *		0 if no cmodes are available
 * side effects - NONE
 */
static unsigned int
find_cmode_slot(void)
{
	unsigned int all_cmodes = 0, my_cmode = 0, i;

	for(i = 0; i < 128; i++)
		all_cmodes |= chmode_flags[i];

	for(my_cmode = 1; my_cmode && (all_cmodes & my_cmode); my_cmode <<= 1);

	return my_cmode;
}

unsigned int
cmode_add(char c_, ChannelModeFunc function, long isupport_type)
{
	int c = (unsigned char) c_;

	if(chmode_table[c].set_func != chm_nosuch && chmode_table[c].set_func != chm_orphaned)
		return 0;

	if(chmode_table[c].set_func == chm_nosuch)
		chmode_table[c].mode_type = find_cmode_slot();
	if(chmode_table[c].mode_type == 0)
		return 0;
	chmode_table[c].set_func = function;
	chmode_table[c].isupport_type = isupport_type;
	generate_cmode_string();
	return chmode_table[c].mode_type;
}

void
cmode_orphan(char c_)
{
	int c = (unsigned char) c_;
	long old_isupport_type;

	s_assert(chmode_flags[c] != 0);
	chmode_table[c].set_func = chm_orphaned;
	old_isupport_type = chmode_table[c].isupport_type;
	chmode_table[c].isupport_type = ISUPPORT_INVALID;
	generate_cmode_string();
}

int
get_channel_access(struct Client *source_p, struct membership *msptr)
{
	hook_data_channel_approval moduledata;

	if(!MyClient(source_p))
		return CHFL_FOUNDER;

	if(msptr == NULL)
		return CHFL_PEON;

	moduledata.client = source_p;
	moduledata.chptr = msptr->chptr;
	moduledata.msptr = msptr;
	moduledata.cmd = 0;
	moduledata.target = NULL;
	moduledata.data = NULL;

	if(is_founder(msptr))
		moduledata.approved = CHFL_FOUNDER;
	else if(is_admin(msptr))
		moduledata.approved = CHFL_ADMIN;
	else if(is_chanop(msptr))
		moduledata.approved = CHFL_CHANOP;
	else if(is_halfop(msptr))
		moduledata.approved = CHFL_HALFOP;
	else
		moduledata.approved = CHFL_PEON;

	call_hook(h_get_channel_access, &moduledata);
	return moduledata.approved;
}

/* add_id()
 *
 * inputs	- client, channel, id to add, type, callback to determine validity
 * outputs	- 0 on failure, 1 on success
 * side effects - given id is added to the appropriate list
 */
int
add_id(struct Client *source_p, struct Channel *chptr, const char *maskid, const char *forward,
       rb_dlink_list * list, long mode_type, is_valid_item item_callback)
{
	struct mode_list_t *actualModeItem;
	static char who[USERHOST_REPLYLEN];
	char *realmask = LOCAL_COPY(maskid);
	rb_dlink_node *ptr;

	/* dont let local clients overflow the banlist, or set redundant
	 * bans
	 */
	if(MyClient(source_p))
	{
		if(mode_type == CHFL_BAN || mode_type == CHFL_QUIET || mode_type == CHFL_EXCEPTION)
		{
			if((rb_dlink_list_length(&chptr->banlist) +
			    rb_dlink_list_length(&chptr->exceptlist) +
			    rb_dlink_list_length(&chptr->invexlist) +
			    rb_dlink_list_length(&chptr->quietlist)) >=
			   (chptr->mode.mode & MODE_EXLIMIT ? ConfigChannel.
			    max_bans_large : ConfigChannel.max_bans))
			{
				sendto_one(source_p, form_str(ERR_BANLISTFULL), me.name,
					   source_p->name, chptr->chname, realmask);
				return 0;

			}

			RB_DLINK_FOREACH(ptr, list->head)
			{
				actualModeItem = ptr->data;
				if(mask_match(actualModeItem->maskstr, realmask))
					/* XXX should return an error */
					return 0;
			}
		}
		else
		{
			if(item_callback != NULL)
			{
				/* Pass it to the callback */
				if(!item_callback(chptr, realmask))
					return 0;
			}
		}
	}
	/* dont let remotes set duplicates */
	else
	{
		RB_DLINK_FOREACH(ptr, list->head)
		{
			actualModeItem = ptr->data;
			if(!irccmp(actualModeItem->maskstr, realmask))
				return 0;
		}
	}


	if(IsPerson(source_p))
		rb_sprintf(who, "%s!%s@%s", source_p->name, source_p->username, source_p->host);
	else
		rb_strlcpy(who, source_p->name, sizeof(who));

	actualModeItem = allocate_list_item(realmask, who, forward);
	actualModeItem->when = rb_current_time();

	rb_dlinkAdd(actualModeItem, &actualModeItem->node, list);

	/* invalidate the can_send() cache */
	if(mode_type == CHFL_BAN || mode_type == CHFL_QUIET || mode_type == CHFL_EXCEPTION)
		chptr->bants++;

	return 1;
}

/* del_id()
 *
 * inputs	- channel, id to remove, type
 * outputs	- 0 on failure, 1 on success
 * side effects - given id is removed from the appropriate list
 *
 * XXX - make it return some damn errors to the user...
 */
struct mode_list_t *
del_id(struct Client *source_p, struct Channel *chptr, const char *maskid, rb_dlink_list * list,
       long mode_type)
{
	rb_dlink_node *ptr;
	struct mode_list_t *listptr;

	if(EmptyString(maskid))
		return NULL;

	RB_DLINK_FOREACH(ptr, list->head)
	{
		listptr = ptr->data;

		if(irccmp(maskid, listptr->maskstr) == 0)
		{
			rb_dlinkDelete(&listptr->node, list);

			/* invalidate the can_send() cache */
			if(mode_type == CHFL_BAN || mode_type == CHFL_QUIET || mode_type == CHFL_EXCEPTION)
				chptr->bants++;

			return listptr;
		}
	}

	return NULL;
}

/* check_string()
 *
 * input	- string to check
 * output	- pointer to 'fixed' string, or "*" if empty
 * side effects - any white space found becomes \0
 */
static char *
check_string(char *s)
{
	char *str = s;
	static char splat[] = "*";
	if(!(s && *s))
		return splat;

	for(; *s; ++s)
	{
		if(IsSpace(*s))
		{
			*s = '\0';
			break;
		}
	}
	return str;
}

/* pretty_mask()
 *
 * inputs	- mask to pretty
 * outputs	- better version of the mask
 * side effects - mask is chopped to limits, and transformed:
 *                x!y@z => x!y@z
 *                y@z   => *!y@z
 *                x!y   => x!y@*
 *                x     => x!*@*
 *                z.d   => *!*@z.d
 */
static char *
pretty_mask(const char *idmask)
{
	static char mask_buf[BUFSIZE];
	int old_mask_pos;
	char *nick, *user, *host, *forward = NULL;
	char splat[] = "*";
	char *t, *at, *ex, *ex2;
	char ne = 0, ue = 0, he = 0, fe = 0;	/* save values at nick[NICKLEN], et all */
	char e2 = 0;				/* save value that delimits forward channel */
	char *mask;

	mask = LOCAL_COPY(idmask);
	mask = check_string(mask);
	collapse(mask);

	nick = user = host = splat;

	if((size_t) BUFSIZE - mask_pos < strlen(mask) + 5)
		return NULL;

	old_mask_pos = mask_pos;

	if (*mask == '$')
	{
		mask_pos += rb_sprintf(mask_buf + mask_pos, "%s", mask) + 1;
		t = mask_buf + old_mask_pos + 1;
		if (*t == '!')
			*t = '~';
		if (*t == '~')
			t++;
		*t = ToLower(*t);
		return mask_buf + old_mask_pos;
	}

	at = ex = ex2 = NULL;
	if((t = strchr(mask, '@')) != NULL)
	{
		at = t;
		*t++ = '\0';
		if(*t != '\0')
			host = t;

		if((t = strchr(mask, '!')) != NULL)
		{
			ex = t;
			*t++ = '\0';
			if(*t != '\0')
				user = t;
			if(*mask != '\0')
				nick = mask;
		}
		else
		{
			if(*mask != '\0')
				user = mask;
		}

		if((t = strchr(host, '!')) != NULL || (t = strchr(host, '$')) != NULL)
		{
			ex2 = t;
			e2 = *t;
			*t++= '\0';
			if (*t != '\0')
				forward = t;
		}
	}
	else if((t = strchr(mask, '!')) != NULL)
	{
		ex = t;
		*t++ = '\0';
		if(*mask != '\0')
			nick = mask;
		if(*t != '\0')
			user = t;
	}
	else if(strchr(mask, '.') != NULL || strchr(mask, ':') != NULL || strchr(mask, '/') != NULL)
	{
		if(*mask != '\0')
			host = mask;
	}
	else
	{
		if(*mask != '\0')
			nick = mask;
	}

	/* truncate values to max lengths */
	if(strlen(nick) > NICKLEN - 1)
	{
		ne = nick[NICKLEN - 1];
		nick[NICKLEN - 1] = '\0';
	}
	if(strlen(user) > USERLEN)
	{
		ue = user[USERLEN];
		user[USERLEN] = '\0';
	}
	if(strlen(host) > HOSTLEN)
	{
		he = host[HOSTLEN];
		host[HOSTLEN] = '\0';
	}
	if(forward && strlen(forward) > CHANNELLEN)
	{
		fe = forward[CHANNELLEN];
		forward[CHANNELLEN] = '\0';
	}

	if (forward)
		mask_pos += rb_sprintf(mask_buf + mask_pos, "%s!%s@%s$%s", nick, user, host, forward) + 1;
	else
		mask_pos += rb_sprintf(mask_buf + mask_pos, "%s!%s@%s", nick, user, host) + 1;

	/* restore mask, since we may need to use it again later */
	if(at)
		*at = '@';
	if(ex)
		*ex = '!';
	if(ex2)
		*ex2 = e2;
	if(ne)
		nick[NICKLEN - 1] = ne;
	if(ue)
		user[USERLEN] = ue;
	if(he)
		host[HOSTLEN] = he;
	if(fe)
		forward[CHANNELLEN] = fe;

	return mask_buf + old_mask_pos;
}

/* fix_key()
 *
 * input	- key to fix
 * output	- the same key, fixed
 * side effects - anything below ascii 13 is discarded, ':' discarded,
 *		high ascii is dropped to lower half of ascii table
 */
static char *
fix_key(char *arg)
{
	unsigned char *s, *t, c;

	for(s = t = (unsigned char *) arg; (c = *s); s++)
	{
		c &= 0x7f;
		if(c != ':' && c != ',' && c > ' ')
			*t++ = c;
	}

	*t = '\0';
	return arg;
}

/* fix_key_remote()
 *
 * input	- key to fix
 * ouput	- the same key, fixed
 * side effects - high ascii dropped to lower half of table,
 *		CR/LF/':' are dropped
 */
static char *
fix_key_remote(char *arg)
{
	unsigned char *s, *t, c;

	for(s = t = (unsigned char *) arg; (c = *s); s++)
	{
		c &= 0x7f;
		if((c != 0x0a) && (c != ':') && (c != ',') && (c != 0x0d) && (c != ' '))
			*t++ = c;
	}

	*t = '\0';
	return arg;
}

/* chm_*()
 *
 * The handlers for each specific mode.
 */
void
chm_nosuch(struct Client *source_p, struct Channel *chptr, int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type)
{
	if(*errors & SM_ERR_UNKNOWN)
		return;
	*errors |= SM_ERR_UNKNOWN;
	sendto_one(source_p, form_str(ERR_UNKNOWNMODE), me.name, source_p->name, c);
}

void
chm_simple(struct Client *source_p, struct Channel *chptr, int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type)
{
	struct Metadata *md;
	struct rb_dictionaryIter iter;

	if(alevel < CHFL_HALFOP)
	{
		if(!(*errors & SM_ERR_NOOPS))
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
				   source_p->name, chptr->chname);
		*errors |= SM_ERR_NOOPS;
		return;
	}

	if(MyClient(source_p) && (++mode_limit_simple > maxmodes_simple))
		return;

	/* setting + */
	if((dir == MODE_ADD) && !(chptr->mode.mode & mode_type))
	{
		/* if +f is disabled, ignore an attempt to set +QF locally */
		if(!ConfigChannel.use_forward && MyClient(source_p) && (c == 'Q' || c == 'F'))
			return;

		chptr->mode.mode |= mode_type;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count++].arg = NULL;
	}
	else if((dir == MODE_DEL) && (chptr->mode.mode & mode_type))
	{
		/* cleanup metadata when the related mode is removed */
		hook_data_metadata_change hookdata;

		hookdata.chptr = chptr;
		hookdata.mode = c;
		hookdata.dir = dir;

		call_hook(h_channel_metadata_delete, &hookdata);

		if(c == 'J')
		{
			RB_DICTIONARY_FOREACH(md, &iter, chptr->metadata)
			{
				if(!strcmp(md->value, "KICKNOREJOIN"))
					channel_metadata_delete(chptr, md->name, 0);
			}
		}

		chptr->mode.mode &= ~mode_type;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = NULL;
	}
}

void
chm_orphaned(struct Client *source_p, struct Channel *chptr, int alevel, int parc, int *parn,
	     const char **parv, int *errors, int dir, char c, long mode_type)
{
	if(MyClient(source_p))
	{
		sendto_one_numeric(source_p, 469, "Mode %c is disabled.", c);
		return;
	}

	if((dir == MODE_ADD) && !(chptr->mode.mode & mode_type))
	{
		chptr->mode.mode |= mode_type;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count++].arg = NULL;
	}
	else if((dir == MODE_DEL) && (chptr->mode.mode & mode_type))
	{
		chptr->mode.mode &= ~mode_type;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = NULL;
	}
}

void
chm_staff(struct Client *source_p, struct Channel *chptr, int alevel, int parc, int *parn,
	  const char **parv, int *errors, int dir, char c, long mode_type)
{
	if(!IsOper(source_p) && !IsServer(source_p))
	{
		if(!(*errors & SM_ERR_NOPRIVS))
			sendto_one_numeric(source_p, ERR_NOPRIVILEGES, form_str(ERR_NOPRIVILEGES));
		*errors |= SM_ERR_NOPRIVS;
		return;
	}
	if(MyClient(source_p) && !IsOperResv(source_p))
	{
		if(!(*errors & SM_ERR_NOPRIVS))
			sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name,
				   "resv");
		*errors |= SM_ERR_NOPRIVS;
		return;
	}

	if(alevel < CHFL_CHANOP)
	{
		if(!(*errors & SM_ERR_NOOPS))
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
				   source_p->name, chptr->chname);
		*errors |= SM_ERR_NOOPS;
		return;
	}

	if(MyClient(source_p) && (++mode_limit_simple > maxmodes_simple))
		return;

	/* setting + */
	if((dir == MODE_ADD) && !(chptr->mode.mode & mode_type))
	{
		chptr->mode.mode |= mode_type;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count++].arg = NULL;
	}
	else if((dir == MODE_DEL) && (chptr->mode.mode & mode_type))
	{
		chptr->mode.mode &= ~mode_type;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = NULL;
	}
}

void
chm_list(struct Client *source_p, struct Channel *chptr, int alevel, int parc, int *parn,
	 const char **parv, int *errors, int dir, char c, long mode_type)
{
	const char *mask;
	const char *raw_mask, *send_mask;
	char *forward;
	rb_dlink_list *list;
	rb_dlink_node *ptr;
	struct list_mode *mode = NULL;
	struct mode_list_t *listptr;
	int errorval;
	int rpl_list;
	int rpl_endlist;
	bool rpl_custom = false;
	const char *list_formstr, *endlist_formstr;
	int caps;
	int mems;
	is_valid_item item_callback = NULL;
	bool do_mask_checks = NO;
	hook_data_list_approval moddata;

	switch (mode_type)
	{
	case CHFL_BAN:
		list = &chptr->banlist;
		errorval = SM_ERR_RPL_B;
		rpl_list = RPL_BANLIST;
		rpl_endlist = RPL_ENDOFBANLIST;
		mems = ALL_MEMBERS;
		caps = 0;
		do_mask_checks = YES;
		break;

	case CHFL_EXCEPTION:
		/* if +e is disabled, allow all but +e locally */
		if(!ConfigChannel.use_except && MyClient(source_p) &&
		   ((dir == MODE_ADD) && (parc > *parn)))
			return;

		list = &chptr->exceptlist;
		errorval = SM_ERR_RPL_E;
		rpl_list = RPL_EXCEPTLIST;
		rpl_endlist = RPL_ENDOFEXCEPTLIST;
		caps = CAP_EX;
		do_mask_checks = YES;

		if(ConfigChannel.use_except || (dir == MODE_DEL))
			mems = ONLY_HALFOPSANDUP;
		else
			mems = ONLY_SERVERS;
		break;

	case CHFL_INVEX:
		/* if +I is disabled, allow all but +I locally */
		if(!ConfigChannel.use_invex && MyClient(source_p)
		   && ((dir == MODE_ADD) && (parc > *parn)))
			return;

		list = &chptr->invexlist;
		errorval = SM_ERR_RPL_I;
		rpl_list = RPL_INVITELIST;
		rpl_endlist = RPL_ENDOFINVITELIST;
		caps = CAP_IE;
		do_mask_checks = YES;

		if(ConfigChannel.use_invex || (dir == MODE_DEL))
			mems = ONLY_HALFOPSANDUP;
		else
			mems = ONLY_SERVERS;
		break;

	case CHFL_QUIET:
		list = &chptr->quietlist;
		errorval = SM_ERR_RPL_Q;
		rpl_list = RPL_QUIETLIST;
		rpl_endlist = RPL_ENDOFQUIETLIST;
		mems = ALL_MEMBERS;
		caps = 0;
		do_mask_checks = YES;
		break;

	default:
		if((mode = get_list_mode(c)) == NULL)
		{
			iwarn("chm_list() called with unknown type %c!", c);
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
					       "chm_list() called with unknown type %c!", c);
			return;
		}
		list = get_channel_list(chptr, c);
		errorval = mode->errorval;
		rpl_list = rpl_endlist = 0;
		rpl_custom = true;
		mems = mode->mems;
		caps = mode->caps;
		item_callback = mode->item_callback;
		break;
	}

	if (rpl_custom)
	{
		list_formstr = mode->rpl_list;
		endlist_formstr = mode->rpl_endlist;
	}
	else
	{
		list_formstr = form_str(rpl_list);
		endlist_formstr = form_str(rpl_endlist);
	}

	moddata.client = source_p;
	moddata.chptr = chptr;
	moddata.mode = c;
	moddata.mode_type = mode_type;
	moddata.errors = errors;
	moddata.approved = 1;
	moddata.dir = dir;

	call_hook(h_can_list, &moddata);

	/* Unapproved for consumption. */
	if(!moddata.approved)
		return;

	if(dir == 0 || parc <= *parn)
	{
		if((*errors & errorval) != 0)
			return;
		*errors |= errorval;

		/* non-ops cant see +eI lists.. */
		if(mems != ALL_MEMBERS && !(alevel & mems))
		{
			if(!(*errors & SM_ERR_NOOPS))
				sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
					   source_p->name, chptr->chname);
			*errors |= SM_ERR_NOOPS;
			return;
		}

		RB_DLINK_FOREACH(ptr, list->head)
		{
			char buf[BANLEN];
			listptr = ptr->data;

			if(do_mask_checks && listptr->forward)
				rb_snprintf(buf, sizeof(buf), "%s$%s", listptr->maskstr, listptr->forward);
			else
				rb_strlcpy(buf, listptr->maskstr, sizeof(buf));

			sendto_one(source_p, list_formstr,
				   me.name, source_p->name, chptr->chname, buf,
				   listptr->who, listptr->when);
		}
		sendto_one(source_p, endlist_formstr, me.name, source_p->name, chptr->chname);
		return;
	}

	if(alevel < CHFL_HALFOP)
	{
		if(!(*errors & SM_ERR_NOOPS))
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
				   source_p->name, chptr->chname);
		*errors |= SM_ERR_NOOPS;
		return;
	}

	if(MyClient(source_p) && (++mode_limit > MAXMODEPARAMS))
		return;

	raw_mask = parv[(*parn)];
	(*parn)++;

	/* empty ban, or starts with ':' which messes up s2s, ignore it */
	if(EmptyString(raw_mask) || *raw_mask == ':')
		return;

	if(!MyClient(source_p))
	{
		if(strchr(raw_mask, ' '))
			return;

		mask = LOCAL_COPY(raw_mask);
		send_mask = raw_mask;
	}
	else
	{
		if(do_mask_checks)
		{
			mask = pretty_mask(raw_mask);
			send_mask = mask;
		}
		else
			send_mask = mask = raw_mask;
	}

	/* Look for a $ after the first character.
	 * As the first character, it marks an extban; afterwards
	 * it delimits a forward channel.
	 */
	if(do_mask_checks && ConfigChannel.use_forward)
	{
		if((forward = strchr(mask + 1, '$')) != NULL)
		{
			*forward++ = '\0';
			if (*forward == '\0')
				forward = NULL;
		}
	}
	else
		forward = NULL;

	/* we'd have problems parsing this, hyb6 does it too
	 * also make sure it will always fit on a line with channel
	 * name etc.
	 */
	if(strlen(mask) > IRCD_MIN(BANLEN, MODEBUFLEN - 5))
		return;

	/* if we're adding a NEW id */
	if(dir == MODE_ADD)
	{
		/* Only do extban checks on "ban" types */
		if(do_mask_checks && *mask == '$' && MyClient(source_p))
		{
			if(!valid_extban(mask, source_p, chptr, mode_type))
				/* XXX perhaps return an error message here */
				return;
		}

		if (forward)
		{
			struct Channel *targptr = NULL;
			struct membership *msptr;

			/* Forwards only make sense for bans. */
			if(mode_type != CHFL_BAN)
				return;

			/* Do the whole lot of forward checks for the target channel. */
			if(!check_channel_name(forward) ||
			   (MyClient(source_p) && (strlen(forward) > LOC_CHANNELLEN || hash_find_resv(forward))))
			{
				sendto_one_numeric(source_p, ERR_BADCHANNAME, form_str(ERR_BADCHANNAME), forward);
				return;
			}
			if(chptr->chname[0] == '#' && forward[0] == '&')
			{
				sendto_one_numeric(source_p, ERR_BADCHANNAME, form_str(ERR_BADCHANNAME), forward);
				return;
			}
			if(MyClient(source_p) && (targptr = find_channel(forward)) == NULL)
			{
				sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
						   form_str(ERR_NOSUCHCHANNEL), forward);
				return;
			}
			if(MyClient(source_p) && !(targptr->mode.mode & MODE_FREETARGET))
			{
				if((msptr = find_channel_membership(targptr, source_p)) == NULL ||
				   get_channel_access(source_p, msptr) < CHFL_HALFOP)
				{
						sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
							   me.name, source_p->name, targptr->chname);
						return;
				}
			}
		}

		/* dont allow local clients to overflow the banlist, dont
		 * let remote servers set duplicate bans
		 */
		if(!add_id(source_p, chptr, mask, forward, list, mode_type, item_callback))
			return;

		if(forward)
			forward[-1]= '$';

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].caps = caps;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = mems;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = send_mask;
	}
	else if(dir == MODE_DEL)
	{
		struct mode_list_t *removed;		
		static char buf[BANLEN * MAXMODEPARAMS];
		int old_removed_mask_pos = removed_mask_pos;

		if((removed = del_id(source_p, chptr, mask, list, mode_type)) == NULL)
		{
			/* mask isn't a valid ban, check raw_mask */
			if(do_mask_checks && ((removed = del_id(source_p, chptr, raw_mask, list, mode_type)) != NULL))
				send_mask = raw_mask;
		}

		if(removed && removed->forward)
			removed_mask_pos += rb_snprintf(buf + removed_mask_pos, sizeof(buf), "%s$%s", removed->maskstr, removed->forward) + 1;
		else
			removed_mask_pos += rb_strlcpy(buf + removed_mask_pos, send_mask, sizeof(buf)) + 1;

		if(removed)
		{
			free_list_item(removed);
			removed = NULL;
		}

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].caps = caps;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = mems;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = buf + old_removed_mask_pos;
	}
}

/*
 * chm_privs()
 * Add or remove privileges for a user
 *
 * XXX document this in detail...
 * */

void
chm_privs(struct Client *source_p, struct Channel *chptr, int alevel, int parc, int *parn,
	  const char **parv, int *errors, int dir, char c, long mode_type)
{
	struct membership *mstptr;
	const char *target_nick;
	struct Client *targ_p;

	switch (mode_type)
	{
	case CHFL_FOUNDER:
		if(!ConfigChannel.use_founder)
		{
			if(*errors & SM_ERR_UNKNOWN)
				return;
			*errors |= SM_ERR_UNKNOWN;
			sendto_one(source_p, form_str(ERR_UNKNOWNMODE), me.name, source_p->name, c);
			return;
		}
		/* Only founders can found other users */
		if(alevel != CHFL_FOUNDER)
		{
			if(!(*errors & SM_ERR_NOOPS))
				sendto_one(source_p, ":%s 482 %s %s :You're not a channel founder",
					   me.name, source_p->name, chptr->chname);
			*errors |= SM_ERR_NOOPS;
			return;
		}
		break;
	case CHFL_ADMIN:
		if(!ConfigChannel.use_admin)
		{
			if(*errors & SM_ERR_UNKNOWN)
				return;
			*errors |= SM_ERR_UNKNOWN;
			sendto_one(source_p, form_str(ERR_UNKNOWNMODE), me.name, source_p->name, c);
			return;
		}
		/* Only founders or admins can found/admin other users */
		if(alevel != CHFL_ADMIN && alevel != CHFL_FOUNDER)
		{
			if(!(*errors & SM_ERR_NOOPS))
				sendto_one(source_p,
					   ":%s 482 %s %s :You're not a channel administrator",
					   me.name, source_p->name, chptr->chname);
			*errors |= SM_ERR_NOOPS;
			return;
		}
		break;
	case CHFL_HALFOP:
		if(!ConfigChannel.use_halfop)
		{
			if(*errors & SM_ERR_UNKNOWN)
				return;
			*errors |= SM_ERR_UNKNOWN;
			sendto_one(source_p, form_str(ERR_UNKNOWNMODE), me.name, source_p->name, c);
			return;
		}
		/* And fall through (halfops can't halfop other users or above) --awilcox */
	default:
		/* Only chanops can halfop/op other users */
		if(alevel < CHFL_CHANOP)
		{
			if(!(*errors & SM_ERR_NOOPS))
				sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
					   source_p->name, chptr->chname);
			*errors |= SM_ERR_NOOPS;
			return;
		}
		break;
	}

	/* Querying for modes */
	if((dir == MODE_QUERY) || (parc <= *parn))
		return;

	target_nick = parv[(*parn)];
	(*parn)++;

	/* empty nick */
	if(EmptyString(target_nick))
	{
		sendto_one_numeric(source_p, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK), "*");
		return;
	}

	/* Chase the user through the nick history */
	if((targ_p = find_chasing(source_p, target_nick, NULL)) == NULL)
		return;

	/* Find the channel membership for the target */
	mstptr = find_channel_membership(chptr, targ_p);

	if(mstptr == NULL)
	{
		/* No membership */
		if(!(*errors & SM_ERR_NOTONCHANNEL) && MyClient(source_p))
			sendto_one_numeric(source_p, ERR_USERNOTINCHANNEL,
					   form_str(ERR_USERNOTINCHANNEL), target_nick,
					   chptr->chname);
		*errors |= SM_ERR_NOTONCHANNEL;
		return;
	}

	/* Do not allow clients to overflow MAXMODEPARAMS */
	if(MyClient(source_p) && (++mode_limit > MAXMODEPARAMS))
		return;

	/* If we're adding a mode */
	if(dir == MODE_ADD)
	{
		/* Does the target already have the flags? */
		if(targ_p == source_p && mstptr->flags & mode_type)
			return;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = targ_p->id;
		mode_changes[mode_count].arg = targ_p->name;
		mode_changes[mode_count++].client = targ_p;

		mstptr->flags |= mode_type;
	}
	else			/* If we're removing a mode */
	{
		/* Don't remove flags from services if local
		 * remote can do this because remote could be services.
		 */
		if(MyClient(source_p) && IsService(targ_p))
		{
			sendto_one(source_p, form_str(ERR_ISCHANSERVICE), me.name, source_p->name,
				   targ_p->name, chptr->chname);
			return;
		}

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = targ_p->id;
		mode_changes[mode_count].arg = targ_p->name;
		mode_changes[mode_count++].client = targ_p;

		mstptr->flags &= ~mode_type;
	}
}

void
chm_limit(struct Client *source_p, struct Channel *chptr, int alevel, int parc, int *parn,
	  const char **parv, int *errors, int dir, char c, long mode_type)
{
	const char *lstr;
	static char limitstr[30];
	int limit;

	if(alevel < CHFL_HALFOP)
	{
		if(!(*errors & SM_ERR_NOOPS))
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
				   source_p->name, chptr->chname);
		*errors |= SM_ERR_NOOPS;
		return;
	}

	if(dir == MODE_QUERY)
		return;

	if(MyClient(source_p) && (++mode_limit_simple > maxmodes_simple))
		return;

	if((dir == MODE_ADD) && parc > *parn)
	{
		lstr = parv[(*parn)];
		(*parn)++;

		if(EmptyString(lstr) || (limit = atoi(lstr)) <= 0)
			return;

		rb_sprintf(limitstr, "%d", limit);

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = limitstr;

		chptr->mode.limit = limit;
	}
	else if(dir == MODE_DEL)
	{
		if(!chptr->mode.limit)
			return;

		chptr->mode.limit = 0;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = NULL;
	}
}

void
chm_throttle(struct Client *source_p, struct Channel *chptr, int alevel, int parc, int *parn,
	     const char **parv, int *errors, int dir, char c, long mode_type)
{
	int joins = 0, timeslice = 0;

	if(alevel < CHFL_HALFOP)
	{
		if(!(*errors & SM_ERR_NOOPS))
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
				   source_p->name, chptr->chname);
		*errors |= SM_ERR_NOOPS;
		return;
	}

	if(dir == MODE_QUERY)
		return;

	if(MyClient(source_p) && (++mode_limit_simple > maxmodes_simple))
		return;

	if((dir == MODE_ADD) && parc > *parn)
	{
		sscanf(parv[(*parn)], "%d:%d", &joins, &timeslice);

		if(!joins || !timeslice)
			return;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = parv[(*parn)];

		(*parn)++;

		chptr->mode.join_num = joins;
		chptr->mode.join_time = timeslice;
	}
	else if(dir == MODE_DEL)
	{
		if(!chptr->mode.join_num)
			return;

		chptr->mode.join_num = 0;
		chptr->mode.join_time = 0;
		chptr->join_count = 0;
		chptr->join_delta = 0;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = NULL;
	}
}

void
chm_forward(struct Client *source_p, struct Channel *chptr, int alevel, int parc, int *parn,
	    const char **parv, int *errors, int dir, char c, long mode_type)
{
	struct Channel *targptr = NULL;
	struct membership *msptr;
	const char *forward;

	/* if +f is disabled, ignore local attempts to set it */
	if(!ConfigChannel.use_forward && MyClient(source_p) && (dir == MODE_ADD) && (parc > *parn))
		return;

	if(dir == MODE_QUERY || (dir == MODE_ADD && parc <= *parn))
	{
		if(!(*errors & SM_ERR_RPL_F))
		{
			if(*chptr->mode.forward == '\0')
				sendto_one_notice(source_p, ":%s has no forward channel",
						  chptr->chname);
			else
				sendto_one_notice(source_p, ":%s forward channel is %s",
						  chptr->chname, chptr->mode.forward);
			*errors |= SM_ERR_RPL_F;
		}
		return;
	}

#ifndef FORWARD_OPERONLY
	if(alevel < CHFL_HALFOP)
	{
		if(!(*errors & SM_ERR_NOOPS))
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
				   source_p->name, chptr->chname);
		*errors |= SM_ERR_NOOPS;
		return;
	}
#else
	if(!IsOper(source_p) && !IsServer(source_p))
	{
		if(!(*errors & SM_ERR_NOPRIVS))
			sendto_one_numeric(source_p, ERR_NOPRIVILEGES, form_str(ERR_NOPRIVILEGES));
		*errors |= SM_ERR_NOPRIVS;
		return;
	}
#endif

	if(MyClient(source_p) && (++mode_limit_simple > maxmodes_simple))
		return;

	if(dir == MODE_ADD && parc > *parn)
	{
		forward = parv[(*parn)];
		(*parn)++;

		if(EmptyString(forward))
			return;
		if(!check_channel_name(forward)
		   || (MyClient(source_p)
		       && (strlen(forward) > LOC_CHANNELLEN || hash_find_resv(forward))))
		{
			sendto_one_numeric(source_p, ERR_BADCHANNAME, form_str(ERR_BADCHANNAME),
					   forward);
			return;
		}
		/* don't forward to inconsistent target -- jilles */
		if(chptr->chname[0] == '#' && forward[0] == '&')
		{
			sendto_one_numeric(source_p, ERR_BADCHANNAME, form_str(ERR_BADCHANNAME),
					   forward);
			return;
		}
		if(MyClient(source_p) && (targptr = find_channel(forward)) == NULL)
		{
			sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL, form_str(ERR_NOSUCHCHANNEL),
					   forward);
			return;
		}
		if(MyClient(source_p) && !(targptr->mode.mode & MODE_FREETARGET))
		{
			if((msptr = find_channel_membership(targptr, source_p)) == NULL
			   || !is_any_op(msptr))
			{
				sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
					   source_p->name, targptr->chname);
				return;
			}
		}

		rb_strlcpy(chptr->mode.forward, forward, sizeof(chptr->mode.forward));

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems =
			ConfigChannel.use_forward ? ALL_MEMBERS : ONLY_SERVERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = forward;
	}
	else if(dir == MODE_DEL)
	{
		if(!(*chptr->mode.forward))
			return;

		*chptr->mode.forward = '\0';

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = NULL;
	}
}

void
chm_key(struct Client *source_p, struct Channel *chptr, int alevel, int parc, int *parn,
	const char **parv, int *errors, int dir, char c, long mode_type)
{
	char *key;

	if(alevel < CHFL_HALFOP)
	{
		if(!(*errors & SM_ERR_NOOPS))
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED), me.name,
				   source_p->name, chptr->chname);
		*errors |= SM_ERR_NOOPS;
		return;
	}

	if(dir == MODE_QUERY)
		return;

	if(MyClient(source_p) && (++mode_limit_simple > maxmodes_simple))
		return;

	if((dir == MODE_ADD) && parc > *parn)
	{
		key = LOCAL_COPY(parv[(*parn)]);
		(*parn)++;

		if(MyClient(source_p))
			fix_key(key);
		else
			fix_key_remote(key);

		if(EmptyString(key))
			return;

		s_assert(key[0] != ' ');
		rb_strlcpy(chptr->mode.key, key, sizeof(chptr->mode.key));

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = chptr->mode.key;
	}
	else if(dir == MODE_DEL)
	{
		static char splat[] = "*";
		int i;

		if(parc > *parn)
			(*parn)++;

		if(!(*chptr->mode.key))
			return;

		/* hack time.  when we get a +k-k mode, the +k arg is
		 * chptr->mode.key, which the -k sets to \0, so hunt for a
		 * +k when we get a -k, and set the arg to splat. --anfl
		 */
		for(i = 0; i < mode_count; i++)
		{
			if(mode_changes[i].letter == 'k' && mode_changes[i].dir == MODE_ADD)
				mode_changes[i].arg = splat;
		}

		*chptr->mode.key = 0;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].caps = 0;
		mode_changes[mode_count].nocaps = 0;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = "*";
	}
}

/* *INDENT-OFF* */
struct ChannelMode chmode_table[128] =
{
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x00 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x01 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x02 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x03 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x04 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x05 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x06 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x07 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x08 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x09 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x0a */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x0b */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x0c */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x0d */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x0e */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x0f */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x10 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x11 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x12 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x13 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x14 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x15 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x16 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x17 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x18 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x19 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x1a */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x1b */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x1c */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x1d */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x1e */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x1f */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x20 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x21 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x22 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x23 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x24 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x25 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x26 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x27 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x28 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x29 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x2a */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x2b */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x2c */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x2d */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x2e */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x2f */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x30 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x31 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x32 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x33 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x34 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x35 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x36 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x37 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x38 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x39 */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x3a */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x3b */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x3c */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x3d */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x3e */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x3f */

  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* @ */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* A */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* B */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* C */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* D */
  {chm_simple,	MODE_NOKICK,	ISUPPORT_D },		/* E */
  {chm_simple,	MODE_FREETARGET,ISUPPORT_D },		/* F */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* G */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* H */
  {chm_list,	CHFL_INVEX,	ISUPPORT_A },		/* I */
  {chm_simple,	MODE_NOREJOIN,	ISUPPORT_D },		/* J */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* K */
  {chm_staff,	MODE_EXLIMIT,	ISUPPORT_D },		/* L */
  {chm_staff,	MODE_NOOPERKICK,ISUPPORT_D },		/* M */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* N */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* O */
  {chm_staff,	MODE_PERMANENT,	ISUPPORT_D },		/* P */
  {chm_simple,	MODE_DISFORWARD,ISUPPORT_D },		/* Q */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* R */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* S */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* T */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* U */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* V */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* W */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* X */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* Y */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* Z */
  {chm_nosuch,	0,		ISUPPORT_INVALID },
  {chm_nosuch,	0,		ISUPPORT_INVALID },
  {chm_nosuch,	0,		ISUPPORT_INVALID },
  {chm_nosuch,	0,		ISUPPORT_INVALID },
  {chm_nosuch,	0,		ISUPPORT_INVALID },
  {chm_nosuch,	0,		ISUPPORT_INVALID },
  {chm_privs,	CHFL_ADMIN,	ISUPPORT_PREFIX },	/* a */
  {chm_list,	CHFL_BAN,	ISUPPORT_A },		/* b */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* c */
  {chm_simple,	MODE_NONICK,	ISUPPORT_D },		/* d */
  {chm_list,	CHFL_EXCEPTION,	ISUPPORT_A },		/* e */
  {chm_forward,	0,		ISUPPORT_C },		/* f */
  {chm_simple,	MODE_FREEINVITE,ISUPPORT_D },		/* g */
  {chm_privs,	CHFL_HALFOP,	ISUPPORT_PREFIX },	/* h */
  {chm_simple,	MODE_INVITEONLY,ISUPPORT_D },		/* i */
  {chm_throttle, 0,		ISUPPORT_C },		/* j */
  {chm_key,	0,		ISUPPORT_B },		/* k */
  {chm_limit,	0,		ISUPPORT_C },		/* l */
  {chm_simple,	MODE_MODERATED,	ISUPPORT_D },		/* m */
  {chm_simple,	MODE_NOPRIVMSGS,ISUPPORT_D },		/* n */
  {chm_privs,	CHFL_CHANOP,	ISUPPORT_PREFIX },	/* o */
  {chm_simple,	MODE_PRIVATE,	ISUPPORT_D },		/* p */
  {chm_list,	CHFL_QUIET,	ISUPPORT_A },		/* q */
  {chm_simple,	MODE_REGONLY,	ISUPPORT_D },		/* r */
  {chm_simple,	MODE_SECRET,	ISUPPORT_D },		/* s */
  {chm_simple,	MODE_TOPICLIMIT,ISUPPORT_D },		/* t */
  {chm_privs,	CHFL_FOUNDER,	ISUPPORT_PREFIX },	/* u */
  {chm_privs,	CHFL_VOICE,	ISUPPORT_PREFIX },	/* v */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* w */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* x */
  {chm_nosuch,	0,		ISUPPORT_INVALID },	/* y */
  {chm_simple,	MODE_OPMODERATE,ISUPPORT_D },		/* z */

  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x7b */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x7c */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x7d */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x7e */
  {chm_nosuch,  0, ISUPPORT_INVALID },			/* 0x7f */
};

/* *INDENT-ON* */

/* set_channel_mode()
 *
 * inputs	- client, source, channel, membership pointer, params
 * output	-
 * side effects - channel modes/memberships are changed, MODE is issued
 *
 * Extensively modified to be hotpluggable, 03/09/06 -- nenolod
 */
void
set_channel_mode(struct Client *client_p, struct Client *source_p, struct Channel *chptr,
		 struct membership *msptr, int parc, const char *parv[])
{
	static char cmdbuf[BUFSIZE];
	static char modebuf[BUFSIZE];
	static char parabuf[BUFSIZE];
	char *mbuf;
	char *pbuf;
	int cur_len, mlen, paralen, paracount, arglen, len;
	int i, j, flags;
	int dir = MODE_ADD;
	int parn = 1;
	int errors = 0;
	int alevel;
	const char *ml = parv[0];
	char c;
	struct Client *fakesource_p;
	int flags_list[3] = { ALL_MEMBERS, ONLY_HALFOPSANDUP, ONLY_OPERS };

	mask_pos = 0;
	removed_mask_pos = 0;
	mode_count = 0;
	mode_limit = 0;
	mode_limit_simple = 0;

	alevel = get_channel_access(source_p, msptr);

	/* Hide connecting server on netburst -- jilles */
	if(ConfigServerHide.flatten_links && IsServer(source_p) && !has_id(source_p)
	   && !HasSentEob(source_p))
		fakesource_p = &me;
	else
		fakesource_p = source_p;

	for(; (c = *ml) != 0; ml++)
	{
		switch (c)
		{
		case '+':
			dir = MODE_ADD;
			break;
		case '-':
			dir = MODE_DEL;
			break;
		case '=':
			dir = MODE_QUERY;
			break;
		default:
			/* If this mode char is locked, don't allow local users to change it. */
			if(MyClient(source_p) && chptr->mode_lock && strchr(chptr->mode_lock, c))
			{
				if(!(errors & SM_ERR_MLOCK))
					sendto_one_numeric(source_p, ERR_MLOCKRESTRICTED,
							   form_str(ERR_MLOCKRESTRICTED),
							   chptr->chname, c, chptr->mode_lock);
				errors |= SM_ERR_MLOCK;
				continue;
			}
			chmode_table[(unsigned char) c].set_func(fakesource_p, chptr, alevel, parc,
								 &parn, parv, &errors, dir, c,
								 chmode_table[(unsigned char) c].
								 mode_type);
			break;
		}
	}

	/* bail out if we have nothing to do... */
	if(!mode_count)
		return;

	if(IsServer(source_p))
		rb_sprintf(cmdbuf, ":%s MODE %s ", fakesource_p->name, chptr->chname);
	else
		rb_sprintf(cmdbuf, ":%s!%s@%s MODE %s ", source_p->name, source_p->username,
			   source_p->host, chptr->chname);

	mlen = 0;

	for(j = 0, flags = flags_list[0]; j < 3; j++, flags = flags_list[j])
	{
		cur_len = mlen;
		mbuf = modebuf + mlen;
		pbuf = parabuf;
		parabuf[0] = '\0';
		paracount = paralen = 0;
		dir = MODE_QUERY;

		for(i = 0; i < mode_count; i++)
		{
			if(mode_changes[i].letter == 0 || mode_changes[i].mems != flags)
				continue;

			if(mode_changes[i].arg != NULL)
			{
				arglen = strlen(mode_changes[i].arg);

				if(arglen > MODEBUFLEN - 5)
					continue;
			}
			else
				arglen = 0;

			/* if we're creeping over MAXMODEPARAMSSERV, or over
			 * bufsize (4 == +/-,modechar,two spaces) send now.
			 */
			if(mode_changes[i].arg != NULL &&
			   ((paracount == MAXMODEPARAMSSERV) ||
			    ((cur_len + paralen + arglen + 4) > (BUFSIZE - 3))))
			{
				*mbuf = '\0';

				if(cur_len > mlen)
				{
					sendto_channel_local(flags, chptr, "%s%s %s", cmdbuf,
							     modebuf, parabuf);
				}
				else
					continue;

				paracount = paralen = 0;
				cur_len = mlen;
				mbuf = modebuf + mlen;
				pbuf = parabuf;
				parabuf[0] = '\0';
				dir = MODE_QUERY;
			}

			if(dir != mode_changes[i].dir)
			{
				*mbuf++ = (mode_changes[i].dir == MODE_ADD) ? '+' : '-';
				cur_len++;
				dir = mode_changes[i].dir;
			}

			*mbuf++ = mode_changes[i].letter;
			cur_len++;

			if(mode_changes[i].arg != NULL)
			{
				paracount++;
				len = rb_sprintf(pbuf, "%s ", mode_changes[i].arg);
				pbuf += len;
				paralen += len;
			}
		}

		if(paralen && parabuf[paralen - 1] == ' ')
			parabuf[paralen - 1] = '\0';

		*mbuf = '\0';
		if(cur_len > mlen)
			sendto_channel_local(flags, chptr, "%s%s %s", cmdbuf, modebuf, parabuf);
	}

	/* only propagate modes originating locally, or if we're hubbing */
	if(MyClient(source_p) || rb_dlink_list_length(&serv_list) > 1)
		send_cap_mode_changes(client_p, source_p, chptr, mode_changes, mode_count);
}

/* set_channel_mlock()
 *
 * inputs	- client, source, channel, params
 * output	-
 * side effects - channel mlock is changed / MLOCK is propagated
 */
void
set_channel_mlock(struct Client *client_p, struct Client *source_p, struct Channel *chptr,
		  const char *newmlock, int propagate)
{
	rb_free(chptr->mode_lock);
	chptr->mode_lock = newmlock ? rb_strdup(newmlock) : NULL;

	if(propagate)
	{
		sendto_server(client_p, NULL, CAP_TS6 | CAP_MLOCK, NOCAPS, ":%s MLOCK %ld %s :%s",
			      source_p->id, (long) chptr->channelts, chptr->chname,
			      chptr->mode_lock ? chptr->mode_lock : "");
	}
}
