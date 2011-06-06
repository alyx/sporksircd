/*
 * +j snomask: Channel join/part notices.
 *
 * Original by nenolod, augmented for part notices by Elizabeth
 *
 */

#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"

static void
show_channeljoin(hook_data_channel_activity *info)
{
	sendto_realops_snomask(snomask_modes['j'], L_ALL,
		"%s (%s@%s) has joined channel %s", info->client->name,
		info->client->username, info->client->host, info->chptr->chname);
}

static void
show_channelpart(hook_data_channel_activity *info)
{
	sendto_realops_snomask(snomask_modes['j'], L_ALL,
		"%s (%s@%s) has parted channel %s", info->client->name,
		info->client->username, info->client->host, info->chptr->chname);
}

mapi_hfn_list_av1 channeljoin_hfnlist[] = {
        {"channel_join", (hookfn) show_channeljoin},
	{"channel_part", (hookfn) show_channelpart},
        {NULL, NULL}
};

static int
init(void)
{
	snomask_modes['j'] = find_snomask_slot();

	return 0;
}

static void
fini(void)
{
	snomask_modes['j'] = 0;
}

DECLARE_MODULE_AV1(sno_channeljoin, init, fini, NULL, NULL, channeljoin_hfnlist, "SporksIRCD development team");

