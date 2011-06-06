/* 
 * SporksIRCD: the ircd for discerning transsexual quilting bees.
 * chm_censor.c: Censor words from being said on the channel
 * 
 * Copyright (c) 2011 Andrew Wilcox.  All rights reserved.
 * Developed by: Andrew Wilcox of Wilcox Technologies
 *		 for SporksIRCD
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal with the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimers.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimers in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the names of Andrew Wilcox, Wilcox Technologies, SporksIRCD,
 * nor the names of its contributors may be used to endorse or promote products
 * derived from this Software without specific prior written permission.
 * 4. You agree to use this for good and not evil. If you whine about this
 * clause in any way, your licence to use this software is revoked.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * WITH THE SOFTWARE.
 * 
 */

#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "channel.h"
#include "list.h"
#include "chmode.h"
#include "numeric.h"
#include "s_serv.h"
#include "newconf.h"

static void is_message_censored(hook_data_channel_approval *);
static int match_censor(struct Channel *chptr, const char *text);
static bool valid_censor(struct Channel *chptr, char *item);

mapi_hfn_list_av1 censor_hook_fnlist[] = {
	{ "can_send",		(hookfn)is_message_censored },
	{ NULL, NULL }
};

static unsigned int mode_flag;
static unsigned int max_censors = 25, max_censors_large = 50;

static struct list_mode censormode = {
	.c = 'y',
	.name = "censor",
	.mems = ALL_MEMBERS,
	.rpl_list = ":%s 940 %s %s %s %s %lu",
	.rpl_endlist = ":%s 941 %s %s :End of Channel Censor List",
	.maxlen = &max_censors,
	.caps = 0,
	.item_callback = &valid_censor,
};

static void
conf_set_max_censors(void *data)
{
	int value = *((int *)data);
	if (value < 5)
	{
		conf_report_error("censor::max_censors is set to an absurd value, setting to 10");
		max_censors = 10;
		return;
	}
	max_censors = value;
}

static void
conf_set_max_censors_large(void *data)
{
	int value = *((int *)data);
	if (value < 5)
	{
		conf_report_error("censor::max_censors_large is set to an absurd value, setting to 10");
		max_censors_large = 10;
		return;
	}
	max_censors_large = value;
}

static int
conf_end_censor(struct TopConf *tc)
{
	if (max_censors_large < max_censors)
	{
		conf_report_error("censor::max_censors_large is less than censor::max_censors, setting to censor::max_censors");
		max_censors_large = max_censors;
		return 0;
	}

	return 0;
}

static struct ConfEntry conf_censor_list[] =
{
	{ "max_censors",	CF_INT, conf_set_max_censors,		0, NULL },
	{ "max_censors_large",	CF_INT, conf_set_max_censors_large,	0, NULL },
};

/*
 * _modinit ()
 *
 * inputs	- none
 * outputs	- 0 on successful load; -1 on failure.
 * side effects	- all channels on the ircd will be added to the dictionary
 */
static int
_modinit(void)
{
	add_top_conf("censor", NULL, conf_end_censor, conf_censor_list);

	mode_flag = add_list_mode(&censormode);

	if (mode_flag == 0)
		return -1;

	return 0;
}

/*
 * _moddeinit
 *
 * inputs	- none
 * outputs	- none
 * side effects	- Cleans up the dictionary and orphans the 'y' mode.
 */
static void
_moddeinit(void)
{
	remove_conf_item("censor", "max_censors");
	remove_list_mode('y');
	return;
}

DECLARE_MODULE_AV1(censor, _modinit, _moddeinit, NULL, NULL,
		   censor_hook_fnlist, "SporksIRCD development team");

/*
 * is_message_censored
 *
 * inputs	- The hook data.
 * outputs	- none
 * side effects	- Determines if a message is censored.
 * note		- This function is called whenever a message is said on
 * 		  a channel.  Therefore it needs to be very fast and as
 * 		  optimised as possible.
 * 		  Remember this before you try and do stupid fancy
 * 		  iterations or calculate prime numbers in here.
 */
static void
is_message_censored(hook_data_channel_approval *data)
{
	/* The proper way to do this is to always check for what you expect to
	   be the case most of the time.  We don't expect many channels to have
	   censor lists, or hell even have +W, so instead of checking if the
	   list /isn't/ 0, we check if it /is/ and return quickly. */
	if (rb_dlink_list_length(get_channel_list(data->chptr, 'y')) == 0)
		return;
	
	if (((strchr(ConfigChannel.exemptchanops, 'y') == NULL) || !is_any_op(data->msptr)))
	{
		/* 2 == unprintable */
		char *text = ((char **)data->data)[2];
		
		if(!EmptyString(text))
		{
			if(match_censor(data->chptr, text))
			{
				sendto_one_numeric(data->client, 404,
						   "%s :Cannot send to channel - Your message was censored",
						   data->chptr->chname);
				data->approved = 1;
			}
		}
	}
	
	return;
}

/*
 * match_censor
 *
 * inputs       - pointer to channel
 *              - text to match
 * output       - 1 on censor match, 0 on none
 * side effects - none
 */
static int
match_censor(struct Channel *chptr, const char *text)
{
	struct mode_list_t *censorptr;
	rb_dlink_list *list = get_channel_list(chptr, 'y');
	rb_dlink_node *ptr;
	
	RB_DLINK_FOREACH(ptr, list->head)
	{
		censorptr = ptr->data;
		
		if(match(censorptr->maskstr, text))
			return 1;
	}
	
	return 0;
}

/*
 * valid_censor
 *
 * inputs	- channel pointer, censor item
 * output	- YES on censor valid, NO on none
 * side effects	- none
 */
static bool 
valid_censor(struct Channel *chptr, char *item)
{
	rb_dlink_node *ptr;
	rb_dlink_list *list = get_channel_list(chptr, 'y');

	if((rb_dlink_list_length(list) >= (chptr->mode.mode & MODE_EXLIMIT ?
	    max_censors_large : max_censors)))
	{
		return NO;
	}

	RB_DLINK_FOREACH(ptr, list->head)
	{
		struct mode_list_t *listptr = ptr->data;
		if(irccmp(listptr->maskstr, item))
			return NO;
	}

	return YES;
}

