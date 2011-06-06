#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "hook.h"
#include "ircd.h"
#include "send.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "s_serv.h"
#include "numeric.h"

static void h_hints_channel_join(hook_data_channel_activity *);
static int mo_hints(struct Client *client_p, struct Client *source_p, int parc,
		    const char *parv[]);
static int me_hints(struct Client *client_p, struct Client *source_p, int parc,
		    const char *parv[]);

mapi_hfn_list_av1 hints_hfnlist[] = {
	    { "channel_join", (hookfn) h_hints_channel_join },
	    { NULL, NULL }
};

#define HINTSPATH ETCPATH "/hints.list"

/* May not be portable but I don't care right now --Elizabeth */
#ifdef WIN32
#	define FNEWLINE "\r\n"
#else
#	define FNEWLINE "\n"
#endif

static const char *hintsfile = HINTSPATH;
rb_dlink_list hintslist = { NULL, NULL, 0 };
static int maxid = 0;
static int hints_probability = 10; /* 1/10 */

typedef struct {
	char *text;
	char *hostmask;
	int registered;
	int unregistered;
} hint;

/* Add a hint */
static void
add_a_hint(const char *text, int registered, int unregistered, const char *hostmask)
{
	hint *h = rb_malloc(sizeof(hint));

	h->text = rb_strdup(text);
	h->registered = registered;
	h->unregistered = unregistered;
	h->hostmask = (hostmask == NULL || EmptyString(hostmask) ? NULL : rb_strdup(hostmask));

	/* Add to list */
	rb_dlinkAddAlloc(h, &hintslist);
}

/* Delete hints from the list */
static void
delete_hints(void)
{
	rb_dlink_node *ptr, *nptr;

	RB_DLINK_FOREACH_SAFE(ptr, nptr, hintslist.head)
	{
		hint *h = ptr->data;
		free(h->text);
		free(h->hostmask);
		free(h);
		rb_dlinkDestroy(ptr, &hintslist);
	}

	maxid = 0;
}

/* Load the hints */
static int
load_hints(void)
{
	FILE *fptr = fopen(hintsfile, "r");

	if (fptr == NULL)
	{
		/* No hints file, this is fatal */
		return -1;
	}

	/* Delete before doing this */
	delete_hints();

	while (1)
	{
		char text[REASONLEN];
		char flags[REASONLEN] = "";
		char *flptr, *p;

		/* Read in line */
		if (fgets(text, REASONLEN, fptr) == NULL)
			return 0;
		/* Read in flags */
		if (fgets(flags, REASONLEN, fptr) == NULL)
			return -1; /* Fail on premature end */

		/* Blow away newlines */
		if ((p = strpbrk(text, FNEWLINE)) != NULL)
			*p = '\0';
		if ((p = strpbrk(flags, FNEWLINE)) != NULL)
			*p = '\0';

		/* Parse flags - XXX well aware this needs a real parser */
		flptr = flags;
		
		/* Delimit the fields */
		flptr[1] = flptr[3] = '\0';

		add_a_hint(text, atoi(&flptr[0]), atoi(&flptr[2]), &flptr[4]);
	}
	
	fclose(fptr);
}

static void
list_hints_to_client(struct Client *client_p)
{
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, hintslist.head)
	{
		hint *h = ptr->data;
		sendto_one_notice(client_p, ":reg: %d unreg: %d hostmask: %s text: %s",
				  h->registered, h->unregistered,
				  h->hostmask ? h->hostmask : "<none>", h->text);
	}
}

static int
_modinit(void)
{
	return load_hints();
}

static void
_moddeinit(void)
{
	delete_hints();
}

struct Message hints_msgtab = {
	"HINTS", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_hints, 1}, {mo_hints, 1}}
};

mapi_clist_av1 hints_clist[] = { &hints_msgtab, NULL };

DECLARE_MODULE_AV1(hints, _modinit, _moddeinit, hints_clist, NULL, hints_hfnlist, "SporksIRCD development team");

static void
h_hints_channel_join(hook_data_channel_activity *data)
{
	char nuh[BUFSIZE];
	rb_dlink_node *ptr;
	int a = 0;
	hint **applicable;

	if (hints_probability < rand() % 100)
		return;

	if (rb_dlink_list_length(&hintslist) == 0)
		return;

	applicable = rb_malloc(rb_dlink_list_length(&hintslist) * sizeof(hint));
	memset(&applicable[0], 0, sizeof(applicable));

	rb_snprintf(nuh, BUFSIZE, "%s!%s@%s", data->client->name, data->client->username, data->client->host);

	RB_DLINK_FOREACH(ptr, hintslist.head)
	{
		hint *h = ptr->data;
		int registered = !EmptyString(data->client->user->suser);

		if (h->hostmask && !match(h->hostmask, nuh))
			continue;

		if ((registered && !h->registered) || (!registered && !h->unregistered))
			continue;

		applicable[a++] = h;
	}

	sendto_one(data->client, ":%s NOTICE %s :[%s-info] %s", me.name,
		   data->chptr->chname, ServerInfo.network_name,
		   applicable[rand() % a]->text);

	rb_free(applicable);
}

static int
mo_hints(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if (!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "hint");
		return 0;
	}

	/* No params == query */
	if (parc < 2)
	{
		list_hints_to_client(source_p);
		return 0;
	}

	if (!strcasecmp(parv[1], "RELOAD"))
	{
		load_hints();
		if (parc > 2)
			sendto_match_servs(source_p, parv[2], CAP_ENCAP, NOCAPS,
					   "ENCAP %s HINTS %s", parv[2], parv[1]);
	}
	else
		sendto_one_notice(source_p, ":HINT: unknown subcommand %s", parv[1]);

	return 0;
}

static int
me_hints(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	/* No params == query */
	if (parc < 2)
	{
		list_hints_to_client(source_p);
		return 0;
	}

	if (!strcasecmp(parv[1], "RELOAD"))
		load_hints();

	/* Just drop invalid commands here */

	return 0;
}
