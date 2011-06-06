/* 
 * SporksIRCD: the ircd for discerning transsexual quilting bees.
 * chm_censor_operonly.c: Limit censors to operators only via +W.
 * 
 * Copyright (c) 2011 Elizabeth Jennifer Myers. All rights reserved.
 * Developed by: Elizabeth Jennifer Myers of the SporksIRCD project
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
 * XXX - note: this module is going away at some point in the future!
 */

#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "channel.h"
#include "list.h"
#include "chmode.h"
#include "numeric.h"
#include "s_serv.h"
#include "inline/stringops.h"

static int mode_flag;
static void can_list(hook_data_list_approval *data);
static struct list_mode *censor_mode;

mapi_hfn_list_av1 censor_operonly_hook_fnlist[] = {
	{ "can_list",	(hookfn)can_list },
	{ NULL, NULL }
};

/*
 * _modinit ()
 *
 * inputs	- none
 * outputs	- 0 on successful load; -1 on failure.
 * side effects	- mode added to the mode table 
 */
static int
_modinit(void)
{
	if((censor_mode = get_list_mode('y')) == NULL)
		return -1;

	mode_flag = cmode_add('W', chm_staff, ISUPPORT_D);
	if (!mode_flag)
		return -1;
	else
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
	cmode_orphan('W');
	return;
}

static void
can_list(hook_data_list_approval *data)
{
	/* Not interested */
	if (data->mode != 'y' || !data->approved)
		return;

	if (!(data->chptr->mode.mode & mode_flag))
	{
		data->approved = 0;
		if(!(*(data->errors) & censor_mode->errorval))
			sendto_one(data->client, form_str(ERR_CANTUSECENSOR),
				   me.name, data->client->name,
				   data->chptr->chname);
		*(data->errors) |= censor_mode->errorval;
		return;
	}
}

DECLARE_MODULE_AV1(censor_operonly, _modinit, _moddeinit, NULL, NULL,
		   censor_operonly_hook_fnlist, "SporksIRCD development team");


