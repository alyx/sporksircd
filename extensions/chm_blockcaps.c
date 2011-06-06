#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_serv.h"
#include "numeric.h"
#include "chmode.h"
#include "match.h"
#include "newconf.h"

static void h_can_send(void *vdata);
static void conf_set_threshold(void *data);

mapi_hfn_list_av1 blockcaps_hfnlist[] = {
	{ "can_send", (hookfn) h_can_send },
	{ NULL, NULL }
};

static unsigned int mymode;
static int conf_threshold = 75;

static struct ConfEntry conf_blockcaps_list[] =
{
	{ "threshold", CF_INT, conf_set_threshold, 0, NULL },
};

static int
_modinit(void)
{
	mymode = cmode_add('G', chm_simple, ISUPPORT_D);
	if (mymode == 0)
		return -1;

	add_top_conf("blockcaps", NULL, NULL, conf_blockcaps_list);
	
	return 0;
}

static void
_moddeinit(void)
{
	cmode_orphan('G');
	remove_top_conf("blockcaps");
}

DECLARE_MODULE_AV1(chm_blockcaps, _modinit, _moddeinit, NULL, NULL, blockcaps_hfnlist, "SporksNet coding committee");

static void
conf_set_threshold(void *data)
{
	conf_threshold = *(int *) data;

	if(conf_threshold < 1)
	{
		conf_report_error
			("channel::caps_threshold is an absurdly low value of %d -- using 1%.",
			 conf_threshold);
		conf_threshold = 1;
	}
	else if(conf_threshold > 100)
	{
		conf_report_error
			("channel::caps_threshold is an absurdly high value of %d -- using 100%.",
			 conf_threshold);
		conf_threshold = 100;
	}
}

static void
h_can_send(void *vdata)
{
	char *text;
	size_t contor;
	int caps = 0;
	int len = 0;

	hook_data_channel_approval *data = (hook_data_channel_approval *) vdata;

	if ((data->chptr->mode.mode & mymode) &&
	   ((strchr(ConfigChannel.exemptchanops, 'G') == NULL) ||
	    !is_any_op(data->msptr)))
	{
		/* 2 == unprintable */
		text = ((char **)data->data)[2];

		if(strlen(text) < 10)
			return;

		for(contor = 0; contor < strlen(text); contor++)
		{
			if(IsUpper(text[contor]) && !isdigit(text[contor]))
				caps++; 
			len++;
		}
		/* Added divide by 0 check --alxbl */ 
		if(len > 0 && ((caps * 100) / len) >= conf_threshold)
		{
			if(data->cmd == COMMAND_PRIVMSG)
				sendto_one_numeric(data->client, 404,
					"%s :Cannot send to channel - Your message contains more than %d%% capital letters (+G set)", 
					data->chptr->chname, conf_threshold);
			data->approved = CAN_SEND_NO_NONOTIFY;
			return;
		}
	}

	return;
}

