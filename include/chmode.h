/*
 *  charybdis: An advanced ircd.
 *  chmode.h: The ircd channel header.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
 *  Copyright (C) 2008 charybdis development team
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
 */

#ifndef INCLUDED_chmode_h
#define INCLUDED_chmode_h

/* something not included in messages.tab
 * to change some hooks behaviour when needed
 * -- dwr
 */
#define ERR_CUSTOM 1000

extern int chmode_flags[128];

extern void chm_nosuch(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);
extern void chm_orphaned(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);
extern void chm_simple(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);
extern void chm_list(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);
extern void chm_staff(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);
extern void chm_forward(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);
extern void chm_throttle(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);
extern void chm_key(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);
extern void chm_limit(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);
extern void chm_privs(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type);

extern unsigned int cmode_add(char c, ChannelModeFunc function, long isupport_type);
extern void cmode_orphan(char c);
extern void generate_cmode_string(void);
extern char cmodes_a[128];
extern char cmodes_b[128];
extern char cmodes_c[128];
extern char cmodes_d[128];
extern char cmodes_prefix[128];
extern char cmodes_all[128];
extern char cmodes_params[128];
extern int chmode_flags[128];

extern struct ChModeChange mode_changes[BUFSIZE];
extern int mode_count;
extern int mode_limit;


/* Copied from chmode.c
 * bitmasks for error returns, so we send once per call
 */
#define SM_ERR_NOTS		0x00000001	/* No TS on channel */
#define SM_ERR_NOOPS		0x00000002	/* No chan ops */
#define SM_ERR_UNKNOWN		0x00000004
#define SM_ERR_RPL_C		0x00000008
#define SM_ERR_RPL_B		0x00000010
#define SM_ERR_RPL_E		0x00000020
#define SM_ERR_NOTONCHANNEL	0x00000040	/* Not on channel */
#define SM_ERR_RPL_I		0x00000080
#define SM_ERR_NOPRIVS		0x00000100
#define SM_ERR_RPL_Q		0x00000200
#define SM_ERR_RPL_F		0x00000400
#define SM_ERR_MLOCK		0x00000800

#endif
