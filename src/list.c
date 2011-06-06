/*
 * SporksIRCD: the ircd for discerning transsexual quilting bees.
 * list.c: list modes management
 *
 * Copyright (C) 2011 Elizabeth Jennifer Myers
 * Copyright (c) 2011 Andrew Wilcox
 * Copyright (C) 2010-2011 SporksIRCD development team
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 2.Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3.The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "stdinc.h"
#include "channel.h"
#include "client.h"
#include "common.h"
#include "hash.h"
#include "hook.h"
#include "numeric.h"
#include "chmode.h"
#include "list.h"


/* The basic design of the list mode system is something like this:
 *
 * - Each list mode has a dictionary, storing channel names as keys
 *   (Note: this is subject to change)
 * - For each channel key, there is a list stored. This list holds all of the
 *   current entries.
 * - When adding a list mode, the mode letter is added in the usual way (with
 *   ISUPPORT A class), the SM_ERR number is bumped (used in error stuff), and
 *   the list mode is added to the listmodes array. The list dictionary is then
 *   initalised with all the current existing channels and empty lists.
 * - On channel creation, the channel is added to every list mode's dictionary.
 *   It is given an empty list for each mode.
 * - On channel destruction, the channel is removed from the dictionary, its
 *   lists are freed, and every reference to the channel in the list
 *   dictionaries are removed.
 * - Upon list mode removal, the mode letter is orphaned in the usual way, the
 *   SM_ERR number bumped down, each relevant list is removed for each channel,
 *   and the list mode is NULL'd from the listmodes array.
 * - Lists are always bursted with BMASK following normal TS rules otherwise.
 *   This is subject to change.
 *
 * Caveats:
 * - Unloading the module results in destruction of all lists, fine on its own,
 *   but this brings us to the next caveat:
 * - If loaded after netjoin, the list modes for this mode are not picked up
 *   from other servers. This can lead to a VERY nasty desync. The solution atm
 *   is the desync is allowed for servers that don't have the MODESUPPORT
 *   capability, otherwise the servers will likely split.
 */

/* SM_ERR counter
 * Set to the last free SM_ERR */
static int current_sm_err = 0x00001000;

/* Big listmodes list */
struct list_mode *listmodes[128];

/* Greenpeace says: free the malloc's! */
static void free_list_cb(struct rb_dictionaryElement *ptr, void *unused);

/*
 * init_list_modes - initalise list mode stuff
 *
 * inputs - None
 * output - None
 * side effects - listmodes is zeroed out
 */
void
init_list_modes(void)
{
	int i;
	for(i = 0; i < 128; i++)
		listmodes[i] = NULL;
}

/*
 * add_list_mode - add a list mode to the list
 *
 * inputs - list_mode struct pointer
 * output - mode slot on success, 0 on failure
 * side effects - Adds a list mode (+beqI etc) to listmodes
 */
int
add_list_mode(struct list_mode *mode)
{
	static bool overflow = NO;
	int retval;
	rb_dlink_node *chan;

	/* Sanity check */
	if(mode->name == NULL)
		return 0;

	/* Did we overflow? */
	if(overflow)
	{
		iwarn("Attempting to load list mode %s (%c): no more list modes may be loaded.",
		      mode->name, mode->c);
		return 0;
	}

	listmodes[(int)mode->c] = mode;

	if(!(retval = cmode_add(mode->c, chm_list, ISUPPORT_A)))
	{
		/* We failed */
		listmodes[(int)mode->c] = NULL;
		return 0;
	}

	/* Set to latest sm_err value */
	mode->errorval = current_sm_err;

	/* Bump up */
	current_sm_err *= 2;

	/* Uh oh... overflow */
	if(current_sm_err < mode->errorval)
		overflow = YES;

	/* Now create the dict */
	mode->list_dict = rb_dictionary_create_named(mode->name, irccmp);

	/* Init the dict for each *existing* channel */
	RB_DLINK_FOREACH(chan, global_channel_list.head)
	{
		struct Channel *chptr = chan->data;
		create_list_for_channel(chptr, mode->list_dict);
	}

	return retval;
}

/*
 * remove_list_mode - remove a mode from the list
 *
 * input - mode letter
 * output - 0 on success, 1 on failure
 * side effects - removes a list mode (+beqI etc) from listmodes
 */
int
remove_list_mode(char c)
{
	struct list_mode *mode = listmodes[(int)c];

	if(mode == NULL)
		return 1;

	/* Destroy the dict */
	rb_dictionary_destroy(mode->list_dict, free_list_cb, NULL);

	/* Bump down */
	current_sm_err /= 2;

	/* Clear */
	listmodes[(int)c] = NULL;

	/* Orphan */
	cmode_orphan(c);

	return 0;
}

/*
 * get_list_mode - get a list_mode pointer from the listmodes list
 *
 * input - mode letter
 * output - pointer on success, NULL if not found
 * side effects - none
 */
struct list_mode *
get_list_mode(char c)
{
	struct list_mode *mode = listmodes[(int)c];
	return mode;
}

/*
 * channel_create_list - create list stuff for a channel
 *
 * input - channel pointer
 * output - none
 * side effects - lists are created for channel
 */
void
channel_create_list(struct Channel *chptr)
{
	int i;
	for(i = 0; i < 128; i++)
	{
		struct list_mode *mode = listmodes[i];
		if(!mode)
			continue;
		create_list_for_channel(chptr, mode->list_dict);
	}
}


/*
 * channel_destroy_list - destroy list stuff for a channel
 *
 * input - channel pointer
 * output - none
 * side effects - lists are destroyed for channel
 */
void
channel_destroy_list(struct Channel *chptr)
{
	int i;
	for(i = 0; i < 128; i++)
	{
		struct list_mode *mode = listmodes[i];
		if(!mode)
			continue;
		destroy_list_for_channel(chptr, mode->list_dict);
	}
}


/*
 * create_list_for_channel - Create a single list for a channel
 *
 * input - channel pointer and list dict
 * output - none
 * side effects - adds channel entry to the given dict
 */
void
create_list_for_channel(struct Channel *chptr, struct rb_dictionary *list_dict)
{
	rb_dlink_list *list = rb_malloc(sizeof(rb_dlink_list));
	list->head = NULL;
	list->tail = NULL;
	list->length = 0;

	rb_dictionary_add(list_dict, chptr->chname, list);
}

/*
 * destroy_list_for_channel - Destroy a list for a channel
 *
 * input - channel pointer and list dict
 * output - none
 * side effects - removes channel entry from given dict
 */
void
destroy_list_for_channel(struct Channel *chptr, struct rb_dictionary *list_dict)
{
	rb_free(list_for_channel(chptr, list_dict));
	rb_dictionary_delete(list_dict, chptr->chname);
}

/*
 * get_channel_list
 *
 * inputs - channel pointer and mode char
 * output - channel list
 * side effects - none
 *
 * NOTE - this function will probably go away in the future
 */
rb_dlink_list *
get_channel_list(struct Channel *chptr, char c)
{
	struct list_mode *mode = get_list_mode(c);
	return (mode != NULL ? list_for_channel(chptr, mode->list_dict) : NULL);
}

/*
 * list_for_channel
 *
 * inputs       - The channel to search for, and dictionary.
 * outputs      - list for the channel, or NULL if one cannot be found.
 * side effects - will end up segfaulting or aborting (depends on your OS) if
 *                you actually ask for a channel that doesn't exist.
 */
rb_dlink_list *
list_for_channel(struct Channel * chptr, struct rb_dictionary * list_dict)
{
	struct rb_dictionaryElement *elem;

	elem = rb_dictionary_find(list_dict, chptr->chname);

	if(elem == NULL)
	{
		ierror("%s has no entry in list; this is very bad", chptr->chname);
		return NULL;
	}

	return elem->data;
}

/*
 * free_list_cb
 *
 * inputs       - dictionary element
 * output       - none
 * side effects - frees list
 */
static void
free_list_cb(struct rb_dictionaryElement *ptr, void *unused)
{
	rb_dlink_list *list = ptr->data;
	rb_free(list);
}

