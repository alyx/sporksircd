/*
 * Copyright (C) 2011 Elizabeth Jennifer Myers
 * Copyright (C) 2010-2011 SporksIRCD development team
 */

#ifndef INCLUDED_list_h
#define INCLUDED_list_h

#include "channel.h" /* Bleah, is_valid_ban */

struct list_mode
{
	/* Our letter - modules set this */
	char c;

	/* Name of the mode - modules set this */
	const char *name;

	/* Who to broadcast to - Should be ALL_MEMBERS - modules set this */
	int mems;

	/* error value to use - used in chm_list - modules do NOT set this */
	int errorval;

	/* rpl_list format str to use - used in chm_list - modules set this */
	const char *rpl_list;

	/* rpl_endlist format str to use - used in chm_list - modules set this */
	const char *rpl_endlist;
	
	/* Caps needed for this to be used - modules set this */
	int caps;

	/* Maximum length of this list - modules set this */
	unsigned int *maxlen;

	/* This dictionary contains all of the channel lists
	 * modules do NOT set this
	 */
	struct rb_dictionary *list_dict;

	/* Called to check if list item being added is valid.
	 * Return 1 on success, 0 on failure
	 * Modules set this.
	 */
	is_valid_item item_callback;
};

extern struct list_mode *listmodes[128];

extern void init_list_modes(void);
extern int add_list_mode(struct list_mode *mode);
extern int remove_list_mode(char c);
extern struct list_mode * get_list_mode(char c);
extern void create_list_for_channel(struct Channel *chptr, struct rb_dictionary *channel_dict);
extern void destroy_list_for_channel(struct Channel *chptr, struct rb_dictionary *channel_dict);
extern rb_dlink_list * list_for_channel(struct Channel *chptr, struct rb_dictionary *channel_dict);
extern rb_dlink_list * get_channel_list(struct Channel *chptr, const char mode);
extern void channel_create_list(struct Channel *chptr);
extern void channel_destroy_list(struct Channel *chptr);

#endif
