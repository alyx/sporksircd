/*
 *  ircd-ratbox: A slightly useful ircd.
 *  modules.c: A module loader.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 */

#include "stdinc.h"


#include "modules.h"
#include "logger.h"
#include "ircd.h"
#include "client.h"
#include "chmode.h"		/* cmodes_[abcd] */
#include "send.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "numeric.h"
#include "parse.h"
#include "ircd_defs.h"
#include "match.h"
#include "s_user.h"
#include "s_serv.h"



#ifndef STATIC_MODULES
#include "ltdl.h"

static char unknown_ver[] = "<unknown>";
struct module **modlist = NULL;

static const char *core_module_table[] = {
	"m_ban",
	"m_die",
	"m_error",
	"m_join",
	"m_kick",
	"m_kill",
	"m_message",
	"m_metadata",
	"m_mode",
	"m_nick",
	"m_part",
	"m_quit",
	"m_server",
	"m_squit",
	NULL
};

#define MODS_INCREMENT 10
int num_mods = 0;
int max_mods = MODS_INCREMENT;

static rb_dlink_list mod_paths;

static int mo_modload(struct Client *, struct Client *, int, const char **);
static int mo_modlist(struct Client *, struct Client *, int, const char **);
static int mo_modreload(struct Client *, struct Client *, int, const char **);
static int mo_modunload(struct Client *, struct Client *, int, const char **);
static int mo_modrestart(struct Client *, struct Client *, int, const char **);

static int me_modload(struct Client *, struct Client *, int, const char **);
static int me_modlist(struct Client *, struct Client *, int, const char **);
static int me_modreload(struct Client *, struct Client *, int, const char **);
static int me_modunload(struct Client *, struct Client *, int, const char **);
static int me_modrestart(struct Client *, struct Client *, int, const char **);

static int do_modload(struct Client *, const char *, bool);
static int do_modunload(struct Client *, const char *, bool);
static int do_modreload(struct Client *, const char *, bool);
static int do_modlist(struct Client *, const char *);
static int do_modrestart(struct Client *, bool);

static void increase_modlist(void);

struct Message modload_msgtab = {
	"MODLOAD", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_modload, 2}, {mo_modload, 2}}
};

struct Message modunload_msgtab = {
	"MODUNLOAD", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_modunload, 2}, {mo_modunload, 2}}
};

struct Message modreload_msgtab = {
	"MODRELOAD", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_modreload, 2}, {mo_modreload, 2}}
};

struct Message modlist_msgtab = {
	"MODLIST", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_modlist, 0}, {mo_modlist, 0}}
};

struct Message modrestart_msgtab = {
	"MODRESTART", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_modrestart, 0}, {mo_modrestart, 0}}
};

extern struct Message error_msgtab;

void
modules_init(void)
{
#ifndef STATIC_MODULES
	if(lt_dlinit())
	{
		ierror("lt_dlinit failed");
		exit(0);
	}

	modlist = rb_malloc(sizeof(struct module) * (MODS_INCREMENT));
	mod_add_cmd(&modload_msgtab);
	mod_add_cmd(&modunload_msgtab);
	mod_add_cmd(&modreload_msgtab);
	mod_add_cmd(&modlist_msgtab);
	mod_add_cmd(&modrestart_msgtab);

	/* Add the default paths we look in to the module system --nenolod */
	mod_add_path(MODPATH);
	mod_add_path(AUTOMODPATH);
#endif
}

/* mod_find_path()
 *
 * input	- path
 * output	- none
 * side effects - returns a module path from path
 */
static struct module_path *
mod_find_path(const char *path)
{
	rb_dlink_node *ptr;
	struct module_path *mpath;

	RB_DLINK_FOREACH(ptr, mod_paths.head)
	{
		mpath = ptr->data;

		if(!strcmp(path, mpath->path))
			return mpath;
	}

	return NULL;
}

/* mod_add_path
 *
 * input	- path
 * ouput	- 
 * side effects - adds path to list
 */
void
mod_add_path(const char *path)
{
	struct module_path *pathst;

	if(mod_find_path(path))
		return;

	pathst = rb_malloc(sizeof(struct module_path));

	rb_strlcpy(pathst->path, path, sizeof(pathst->path));
	rb_dlinkAddAlloc(pathst, &mod_paths);
}

/* mod_clear_paths()
 *
 * input	-
 * output	-
 * side effects - clear the lists of paths
 */
void
mod_clear_paths(void)
{
	rb_dlink_node *ptr, *next_ptr;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, mod_paths.head)
	{
		rb_free(ptr->data);
		rb_free_rb_dlink_node(ptr);
	}

	mod_paths.head = mod_paths.tail = NULL;
	mod_paths.length = 0;
}

/* findmodule_byname
 *
 * input        -
 * output       -
 * side effects -
 */

int
findmodule_byname(const char *name)
{
	int i;

	for(i = 0; i < num_mods; i++)
	{
		if(!irccmp(modlist[i]->name, name))
			return i;
	}
	return -1;
}

/* load_all_modules()
 *
 * input        -
 * output       -
 * side effects -
 */
void
load_all_modules(bool warn)
{
	static const char *shext = SHLIBEXT;
	DIR *system_module_dir = NULL;
	struct dirent *ldirent = NULL;
	char module_fq_name[PATH_MAX + 1];
	char module_dir_name[PATH_MAX + 1];
	int len;
	int ext_len = strlen(SHLIBEXT);
	modules_init();

	max_mods = MODS_INCREMENT;
	rb_strlcpy(module_dir_name, AUTOMODPATH, sizeof(module_dir_name));
	system_module_dir = opendir(module_dir_name);

	if(system_module_dir == NULL)
	{
		rb_strlcpy(module_dir_name, ConfigFileEntry.dpath, sizeof(module_dir_name));
		rb_strlcat(module_dir_name, "/modules/autoload", sizeof(module_dir_name));
		system_module_dir = opendir(module_dir_name);
	}

	if(system_module_dir == NULL)
	{
		ierror("Could not load modules from %s: %m", AUTOMODPATH);
		return;
	}

	while((ldirent = readdir(system_module_dir)) != NULL)
	{
		len = strlen(ldirent->d_name);

		if((len > ext_len) && !strcmp(ldirent->d_name + len - ext_len, shext))
		{
			(void) rb_snprintf(module_fq_name, sizeof(module_fq_name), "%s/%s",
					   module_dir_name, ldirent->d_name);
			(void) load_a_module(module_fq_name, warn, 0);
		}
	}
	(void) closedir(system_module_dir);
}

/* load_core_modules()
 *
 * input        -
 * output       -
 * side effects - core modules are loaded, if any fail, kill ircd
 */
void
load_core_modules(bool warn)
{
	char module_name[PATH_MAX + 1];
	char dir_name[PATH_MAX + 1];
	DIR *core_dir;
	int i;

	core_dir = opendir(MODPATH);
	if(core_dir == NULL)
	{
		rb_snprintf(dir_name, sizeof(dir_name), "%s/modules", ConfigFileEntry.dpath);
		core_dir = opendir(dir_name);
	}
	else
	{
		rb_strlcpy(dir_name, MODPATH, sizeof(dir_name));
	}

	if(core_dir == NULL)
	{
		ierror("Cannot find where core modules are located(tried %s and %s): terminating ircd",
		       MODPATH, dir_name);
		exit(0);
	}

	for(i = 0; core_module_table[i]; i++)
	{
		rb_snprintf(module_name, sizeof(module_name), "%s/%s%s", dir_name,
			    core_module_table[i], SHLIBEXT);

		if(load_a_module(module_name, warn, 1) == -1)
		{
			ierror("Error loading core module %s%s: terminating ircd",
			       core_module_table[i], SHLIBEXT);
			exit(0);
		}
	}
	closedir(core_dir);
}

/* load_one_module()
 *
 * input        -
 * output       -
 * side effects -
 */
int
load_one_module(const char *path, bool coremodule)
{
	char modpath[MAXPATHLEN];
	rb_dlink_node *pathst;
	struct module_path *mpath;

	struct stat statbuf;

	if(server_state_foreground == 1)
		inotice("loading module %s ...", path);

	RB_DLINK_FOREACH(pathst, mod_paths.head)
	{
		mpath = pathst->data;

		rb_snprintf(modpath, sizeof(modpath), "%s/%s", mpath->path, path);
		if((strstr(modpath, "../") == NULL) && (strstr(modpath, "/..") == NULL))
		{
			if(stat(modpath, &statbuf) == 0)
			{
				if(S_ISREG(statbuf.st_mode))
				{
					/* Regular files only please */
					if(coremodule)
						return load_a_module(modpath, 1, 1);
					else
						return load_a_module(modpath, 1, 0);
				}
			}

		}
	}

	sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "Cannot locate module %s", path);
	iwarn("Cannot locate module %s", path);
	return -1;
}


/* load a module .. */
static int
mo_modload(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	bool broadcast = YES;

	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "admin");
		return 0;
	}

	if(parc > 2)
	{
		/* There's a good reason to bother with this -- it is so we don't split the entire
		 * network into pieces when we do a modunload/modload globally --Elizabeth */
		if(match_all_servers(parv[2]))
			broadcast = NO;

		sendto_match_servs(source_p, parv[2], CAP_ENCAP, NOCAPS, "ENCAP %s MODLOAD %s",
				   parv[2], parv[1]);
		if(match(parv[2], me.name) == 0)
			return 0;
	}

	return do_modload(source_p, parv[1], broadcast);
}

static int
me_modload(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	bool broadcast = YES;

	if(!find_shared_conf
	   (source_p->username, source_p->host, source_p->servptr->name, SHARED_MODULE))
	{
		sendto_one_notice(source_p,
				  ":*** You do not have an appropriate shared block "
				  "to load modules on this server.");
		return 0;
	}

	if(match_all_servers(parv[0]))
		broadcast = NO;

	return do_modload(source_p, parv[1], broadcast);
}

static int
do_modload(struct Client *source_p, const char *module, bool broadcast)
{
	char *m_bn = rb_basename(module);
	char oldmodes[BUFSIZE];

	/* Track old modes -- we really need a better way... */
	rb_snprintf(oldmodes, BUFSIZE, "%s %s %s %s %s", cmodes_a, cmodes_b, cmodes_c, cmodes_d, umodebuf);

	if(findmodule_byname(m_bn) != -1)
	{
		sendto_one_notice(source_p, ":Module %s is already loaded", m_bn);
		rb_free(m_bn);
		return 0;
	}

	load_one_module(module, 0);

	if(broadcast)
	{
		char newmodes[BUFSIZE];
		rb_snprintf(newmodes, BUFSIZE, "%s %s %s %s %s", cmodes_a, cmodes_b, cmodes_c, cmodes_d, umodebuf);

		/* Really need a much better way to do this... */
		if(strcpy(oldmodes, newmodes))
			sendto_server(&me, NULL, CAP_TS6 | CAP_MS, NOCAPS, ":%s MS %s",
				      me.id, newmodes);
	}

	rb_free(m_bn);
	rehash(0);
	return 0;
}

/* unload a module .. */
static int
mo_modunload(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	bool broadcast = YES;

	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "admin");
		return 0;
	}

	if(parc > 2)
	{
		if(match_all_servers(parv[0]))
			broadcast = NO;

		sendto_match_servs(source_p, parv[2], CAP_ENCAP, NOCAPS, "ENCAP %s MODUNLOAD %s",
				   parv[2], parv[1]);
		if(match(parv[2], me.name) == 0)
			return 0;
	}

	return do_modunload(source_p, parv[1], broadcast);
}

static int
me_modunload(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	bool broadcast = YES;

	if(!find_shared_conf
	   (source_p->username, source_p->host, source_p->servptr->name, SHARED_MODULE))
	{
		sendto_one_notice(source_p,
				  ":*** You do not have an appropriate shared block "
				  "to load modules on this server.");
		return 0;
	}

	if(match_all_servers(parv[0]))
		broadcast = NO;

	return do_modunload(source_p, parv[1], broadcast);
}

static int
do_modunload(struct Client *source_p, const char *module, bool broadcast)
{
	int modindex;
	char *m_bn = rb_basename(module);
	char oldmodes[BUFSIZE];

	/* Track old modes */
	rb_snprintf(oldmodes, BUFSIZE, "%s %s %s %s %s", cmodes_a, cmodes_b, cmodes_c, cmodes_d, umodebuf);

	if((modindex = findmodule_byname(m_bn)) == -1)
	{
		sendto_one_notice(source_p, ":Module %s is not loaded", m_bn);
		rb_free(m_bn);
		return 0;
	}

	if(modlist[modindex]->core == 1)
	{
		sendto_one_notice(source_p, ":Module %s is a core module and may not be unloaded",
				  m_bn);
		rb_free(m_bn);
		return 0;
	}

	if(unload_one_module(m_bn, 1) == -1)
	{
		sendto_one_notice(source_p, ":Module %s is not loaded", m_bn);
		rb_free(m_bn);
		return 0;
	}

	if(broadcast)
	{
		char newmodes[BUFSIZE];
		rb_snprintf(newmodes, BUFSIZE, "%s %s %s %s %s", cmodes_a, cmodes_b, cmodes_c, cmodes_d, umodebuf);

		if(strcmp(oldmodes, newmodes))
			sendto_server(&me, NULL, CAP_TS6 | CAP_MS, NOCAPS, ":%s MS %s",
				      me.id, newmodes);
	}

	rb_free(m_bn);

	/* Let's not rehash here because it might just reload our module on conf read, not what the user
	 * necessarily wants. Also there may be stale conf items that the user will likely know about but
	 * doesn't want to be bothered with. --Elizabeth
	 */

	return 0;
}

/* unload and load in one! */
static int
mo_modreload(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	bool broadcast = YES;

	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "admin");
		return 0;
	}

	if(parc > 2)
	{
		if(match_all_servers(parv[2]))
			broadcast = NO;

		sendto_match_servs(source_p, parv[2], CAP_ENCAP, NOCAPS, "ENCAP %s MODRELOAD %s",
				   parv[2], parv[1]);
		if(match(parv[2], me.name) == 0)
			return 0;
	}

	return do_modreload(source_p, parv[1], broadcast);
}

static int
me_modreload(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	bool broadcast = YES;

	if(!find_shared_conf
	   (source_p->username, source_p->host, source_p->servptr->name, SHARED_MODULE))
	{
		sendto_one_notice(source_p,
				  ":*** You do not have an appropriate shared block "
				  "to load modules on this server.");
		return 0;
	}

	if(match_all_servers(parv[0]))
		broadcast = NO;

	return do_modreload(source_p, parv[1], broadcast);
}

static int
do_modreload(struct Client *source_p, const char *module, bool broadcast)
{
	int modindex;
	int check_core;
	char *m_bn = rb_basename(module);
	char oldmodes[BUFSIZE];
	rb_snprintf(oldmodes, BUFSIZE, "%s %s %s %s %s", cmodes_a, cmodes_b, cmodes_c, cmodes_d, umodebuf);

	if((modindex = findmodule_byname(m_bn)) == -1)
	{
		sendto_one_notice(source_p, ":Module %s is not loaded", m_bn);
		rb_free(m_bn);
		return 0;
	}

	check_core = modlist[modindex]->core;

	if(unload_one_module(m_bn, 1) == -1)
	{
		sendto_one_notice(source_p, ":Module %s is not loaded", m_bn);
		rb_free(m_bn);
		return 0;
	}

	/* After unloading, send removed modes since the state of some modes (like list modes) may be cleared */
	if(broadcast)
	{
		char newmodes[BUFSIZE];
		rb_snprintf(newmodes, BUFSIZE, "%s %s %s %s %s", cmodes_a, cmodes_b, cmodes_c, cmodes_d, umodebuf);

		if(strcmp(oldmodes, newmodes))
			sendto_server(&me, NULL, CAP_TS6 | CAP_MS, NOCAPS, ":%s MS %s",
				      me.id, newmodes);
	}

	if((load_one_module(m_bn, check_core) == -1) && check_core)
	{
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
				       "Error reloading core module: %s: terminating ircd", m_bn);
		ilog(L_MAIN, "Error loading core module %s: terminating ircd", m_bn);
		exit(0);
	}

	rb_free(m_bn);
	rehash(0);
	return 0;
}

/* list modules .. */
static int
mo_modlist(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "admin");
		return 0;
	}

	if(parc > 2)
	{
		sendto_match_servs(source_p, parv[2], CAP_ENCAP, NOCAPS, "ENCAP %s MODLIST %s",
				   parv[2], parv[1]);
		if(match(parv[2], me.name) == 0)
			return 0;
	}

	return do_modlist(source_p, parc > 1 ? parv[1] : 0);
}

static int
me_modlist(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!find_shared_conf
	   (source_p->username, source_p->host, source_p->servptr->name, SHARED_MODULE))
	{
		sendto_one_notice(source_p,
				  ":*** You do not have an appropriate shared block "
				  "to load modules on this server.");
		return 0;
	}

	return do_modlist(source_p, parv[1]);
}

static int
do_modlist(struct Client *source_p, const char *pattern)
{
	int i;

	for(i = 0; i < num_mods; i++)
	{
		if(pattern)
		{
			if(match(pattern, modlist[i]->name))
			{
				sendto_one(source_p, form_str(RPL_MODLIST), me.name, source_p->name,
					   modlist[i]->name, modlist[i]->address,
					   modlist[i]->version, modlist[i]->core ? "(core)" : "");
			}
		}
		else
		{
			sendto_one(source_p, form_str(RPL_MODLIST), me.name, source_p->name,
				   modlist[i]->name, modlist[i]->address, modlist[i]->version,
				   modlist[i]->core ? "(core)" : "");
		}
	}

	sendto_one(source_p, form_str(RPL_ENDOFMODLIST), me.name, source_p->name);
	return 0;
}

/* unload and reload all modules */
static int
mo_modrestart(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	bool broadcast = YES;

	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "admin");
		return 0;
	}

	if(parc > 1)
	{
		if(match_all_servers(parv[1]))
			broadcast = NO;

		sendto_match_servs(source_p, parv[1], CAP_ENCAP, NOCAPS, "ENCAP %s MODRESTART",
				   parv[1]);
		if(match(parv[1], me.name) == 0)
			return 0;
	}

	return do_modrestart(source_p, broadcast);
}

static int
me_modrestart(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	bool broadcast = YES;

	if(!find_shared_conf
	   (source_p->username, source_p->host, source_p->servptr->name, SHARED_MODULE))
	{
		sendto_one_notice(source_p,
				  ":*** You do not have an appropriate shared block "
				  "to load modules on this server.");
		return 0;
	}

	if(match_all_servers(parv[0]))
		broadcast = NO;

	return do_modrestart(source_p, broadcast);
}

static int
do_modrestart(struct Client *source_p, bool broadcast)
{
	int modnum;
	char oldmodes[BUFSIZE];

	rb_snprintf(oldmodes, BUFSIZE, "%s %s %s %s %s", cmodes_a, cmodes_b, cmodes_c, cmodes_d, umodebuf);

	sendto_one_notice(source_p, ":Reloading all modules");

	modnum = num_mods;
	while(num_mods)
		unload_one_module(modlist[0]->name, 0);

	load_all_modules(0);
	load_core_modules(0);
	rehash(0);

	if(broadcast)
	{
		char newmodes[BUFSIZE];
		rb_snprintf(newmodes, BUFSIZE, "%s %s %s %s %s", cmodes_a, cmodes_b, cmodes_c, cmodes_d, umodebuf);
		
		if(strcmp(oldmodes, newmodes))
			sendto_server(&me, NULL, CAP_TS6 | CAP_MS, NOCAPS, ":%s MS %s",
				      me.id, newmodes);
	}

	sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
			       "Module Restart: %d modules unloaded, %d modules loaded", modnum,
			       num_mods);
	ilog(L_MAIN, "Module Restart: %d modules unloaded, %d modules loaded", modnum, num_mods);
	return 0;
}

/* unload_one_module()
 *
 * inputs	- name of module to unload
 *		- 1 to say modules unloaded, 0 to not
 * output	- 0 if successful, -1 if error
 * side effects	- module is unloaded
 */
int
unload_one_module(const char *name, bool warn)
{
	int modindex;

	if((modindex = findmodule_byname(name)) == -1)
		return -1;

	switch (modlist[modindex]->mapi_version)
	{
	case 1:
		{
			struct mapi_mheader_av1 *mheader = modlist[modindex]->mapi_header;
			if(mheader->mapi_command_list)
			{
				struct Message **m;
				for(m = mheader->mapi_command_list; *m; ++m)
					mod_del_cmd(*m);
			}

			/* hook events are never removed, we simply lose the
			 * ability to call them --fl
			 */
			if(mheader->mapi_hfn_list)
			{
				mapi_hfn_list_av1 *m;
				for(m = mheader->mapi_hfn_list; m->hapi_name; ++m)
					remove_hook(m->hapi_name, m->fn);
			}

			if(mheader->mapi_unregister)
				mheader->mapi_unregister();
			break;
		}
	default:
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
				       "Unknown/unsupported MAPI version %d when unloading %s!",
				       modlist[modindex]->mapi_version, modlist[modindex]->name);
		ilog(L_MAIN, "Unknown/unsupported MAPI version %d when unloading %s!",
		     modlist[modindex]->mapi_version, modlist[modindex]->name);
		break;
	}

	lt_dlclose(modlist[modindex]->address);

	rb_free(modlist[modindex]->name);
	memcpy(&modlist[modindex], &modlist[modindex + 1],
		sizeof(struct module) * ((num_mods - 1) - modindex));

	if(num_mods != 0)
		num_mods--;

	if(warn == YES)
	{
		ilog(L_MAIN, "Module %s unloaded", name);
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "Module %s unloaded", name);
	}

	return 0;
}


/*
 * load_a_module()
 *
 * inputs	- path name of module, int to notice, int of core
 * output	- -1 if error 0 if success
 * side effects - loads a module if successful
 */
int
load_a_module(const char *path, bool warn, int core)
{
	lt_dlhandle tmpptr = NULL;

	char *mod_basename;
	const char *ver;

	void *mapi_base;
	int *mapi_version;

	mod_basename = rb_basename(path);

	tmpptr = lt_dlopen(path);

	if(tmpptr == NULL)
	{
		const char *err = lt_dlerror();

		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "Error loading module %s: %s",
				       mod_basename, err);
		ilog(L_MAIN, "Error loading module %s: %s", mod_basename, err);
		rb_free(mod_basename);
		return -1;
	}


	/*
	 * _mheader is actually a struct mapi_mheader_*, but mapi_version
	 * is always the first member of this structure, so we treate it
	 * as a single int in order to determine the API version.
	 *      -larne.
	 */
	mapi_base = lt_dlsym(tmpptr, "_mheader");
	if(mapi_base == NULL)
	{
		mapi_base = lt_dlsym(tmpptr, "__mheader");
	}

	mapi_version = (int *)mapi_base;

	if(mapi_version == NULL || (MAPI_MAGIC(*mapi_version) != MAPI_MAGIC_HDR))
	{
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
				       "Data format error: module %s has no MAPI header.",
				       mod_basename);
		iwarn("Data format error: module %s has no MAPI header.", mod_basename);
		lt_dlclose(tmpptr);
		rb_free(mod_basename);
		return -1;
	}

	switch (MAPI_VERSION(*mapi_version))
	{
	case 1:
		{
			struct mapi_mheader_av1 *mheader = mapi_base;	/* see above */
			if(mheader->mapi_register && (mheader->mapi_register() == -1))
			{
				ilog(L_MAIN, "Module %s indicated failure during load.",
				     mod_basename);
				sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
						       "Module %s indicated failure during load.",
						       mod_basename);
				lt_dlclose(tmpptr);
				rb_free(mod_basename);
				return -1;
			}
			if(mheader->mapi_command_list)
			{
				struct Message **m;
				for(m = mheader->mapi_command_list; *m; ++m)
					mod_add_cmd(*m);
			}

			if(mheader->mapi_hook_list)
			{
				mapi_hlist_av1 *m;
				for(m = mheader->mapi_hook_list; m->hapi_name; ++m)
					*m->hapi_id = register_hook(m->hapi_name);
			}

			if(mheader->mapi_hfn_list)
			{
				mapi_hfn_list_av1 *m;
				for(m = mheader->mapi_hfn_list; m->hapi_name; ++m)
					add_hook(m->hapi_name, m->fn);
			}

			ver = mheader->mapi_module_version;
			break;
		}

	default:
		iwarn("Module %s has unknown/unsupported MAPI version %d.", mod_basename,
		     MAPI_VERSION(*mapi_version));
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
				       "Module %s has unknown/unsupported MAPI version %d.",
				       mod_basename, *mapi_version);
		lt_dlclose(tmpptr);
		rb_free(mod_basename);
		return -1;
	}

	if(ver == NULL)
		ver = unknown_ver;

	increase_modlist();

	modlist[num_mods] = rb_malloc(sizeof(struct module));
	modlist[num_mods]->address = tmpptr;
	modlist[num_mods]->version = ver;
	modlist[num_mods]->core = core;
	modlist[num_mods]->name = rb_strdup(mod_basename);
	modlist[num_mods]->mapi_header = mapi_version;
	modlist[num_mods]->mapi_version = MAPI_VERSION(*mapi_version);
	num_mods++;

	if(warn == YES)
	{
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
				       "Module %s [version: %s; MAPI version: %d] loaded at 0x%lx",
				       mod_basename, ver, MAPI_VERSION(*mapi_version),
				       (unsigned long) tmpptr);
		ilog(L_MAIN, "Module %s [version: %s; MAPI version: %d] loaded at 0x%lx",
		     mod_basename, ver, MAPI_VERSION(*mapi_version), (unsigned long) tmpptr);
	}
	rb_free(mod_basename);
	return 0;
}

/*
 * increase_modlist
 *
 * inputs	- NONE
 * output	- NONE
 * side effects	- expand the size of modlist if necessary
 */
static void
increase_modlist(void)
{
	struct module **new_modlist = NULL;

	if((num_mods + 1) < max_mods)
		return;

	new_modlist =
		(struct module **) rb_malloc(sizeof(struct module) * (max_mods + MODS_INCREMENT));
	memcpy((void *) new_modlist, (void *) modlist, sizeof(struct module) * num_mods);

	rb_free(modlist);
	modlist = new_modlist;
	max_mods += MODS_INCREMENT;
}

#else /* STATIC_MODULES */
extern const struct mapi_header_av2 *static_mapi_headers[];
void
load_static_modules(void)
{
	int x;
	const int *mapi_version;

	modules_init();
	for(x = 0; static_mapi_headers[x] != NULL; x++)
	{
		mapi_version = (const int *)static_mapi_headers[x];
		if(MAPI_MAGIC(*mapi_version) != MAPI_MAGIC_HDR)
		{
			ilog(L_MAIN, "Error: linked in module without a MAPI header..giving up");
			exit(70);
		}
		switch (MAPI_VERSION(*mapi_version))
		{
		case 1:
			{
				const struct mapi_mheader_av1 *mheader =
					(const struct mapi_mheader_av1 *)mapi_version;
				if(mheader->mapi_register && (mheader->mapi_register() == -1))
				{
					ilog(L_MAIN,
					     "Error: linked in module failed loading..giving up");
					exit(70);
				}

				if(mheader->mapi_command_list)
				{
					struct Message **m;
					for(m = mheader->mapi_command_list; *m; ++m)
						mod_add_cmd(*m);
				}

				if(mheader->mapi_hook_list)
				{
					mapi_hlist_av2 *m;
					for(m = mheader->mapi_hook_list; m->hapi_name; ++m)
						*m->hapi_id = register_hook(m->hapi_name);
				}

				if(mheader->mapi_hfn_list)
				{
					mapi_hfn_list_av2 *m;
					for(m = mheader->mapi_hfn_list; m->hapi_name; ++m)
						add_hook(m->hapi_name, m->hookfn);

				}
				break;
			}
		default:
			{
				ilog(L_MAIN,
				     "Error: Unknown MAPI version (%d)in linked in module..giving up",
				     MAPI_VERSION(*mapi_version));
				exit(70);
			}
		}
	}
}

/* load_all_modules()
 *
 * input        -
 * output       -
 * side effects - all the msgtabs are added for static modules
 */
void
load_all_modules(bool warn)
{
	load_static_modules();
}

#endif /* STATIC_MODULES */