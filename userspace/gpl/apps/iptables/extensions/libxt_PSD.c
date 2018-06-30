/* Shared library add-on to iptables to add PSD target support. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <xtables.h>
#include <linux/netfilter/x_tables.h>

/* Function which prints out usage message. */
static void PSD_help(void)
{
	printf(
"PSD target v%s takes no options\n",
IPTABLES_VERSION);
}

/* Function which parses command options; returns true if it
   ate an option */
static int
PSD_parse(int c, char **argv, int invert, unsigned int *flags,
              const void *entry, struct xt_entry_target **target)
{
	return 0;
}

static struct xtables_target psd_target = {
	.family		= AF_INET,
	.name		= "PSD",
	.version	= IPTABLES_VERSION,
	.size		= XT_ALIGN(0),
	.userspacesize	= XT_ALIGN(0),
	.help		= PSD_help,
	.parse		= PSD_parse,
};

void _init(void)
{
	xtables_register_target(&psd_target);
}
