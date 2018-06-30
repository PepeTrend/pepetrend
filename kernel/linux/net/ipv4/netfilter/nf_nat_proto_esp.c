/*
<:copyright-gpl
 Copyright 2008 Broadcom Corp. All Rights Reserved.

 This program is free software; you can distribute it and/or modify it
 under the terms of the GNU General Public License (Version 2) as
 published by the Free Software Foundation.

 This program is distributed in the hope it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
:>
*/
/******************************************************************************
Filename:       nf_nat_proto_esp.c
Author:         Pavan Kumar
Creation Date:  05/27/04

Description:
    Implements the ESP ALG connectiontracking.
    Migrated to kernel 2.6.21.5 on April 16, 2008 by Dan-Han Tsai.
*****************************************************************************/
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>

#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_nat_rule.h>
#include <net/netfilter/nf_nat_protocol.h>
#include <linux/netfilter/nf_conntrack_proto_esp.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Harald Welte <laforge@gnumonks.org>");
MODULE_DESCRIPTION("Netfilter NAT protocol helper module for ESP");

#if 0
#define DEBUGP(format, args...) printk(KERN_DEBUG "%s:%s: " format, __FILE__, \
				       __FUNCTION__, ## args)
#else
#define DEBUGP(x, args...)
#endif

/* is spi in given range between min and max */
//<< [CTFN-NMIS-008] Charles Wei, Support the unspecified source port for IPSec pass-through 2011/04/18
bool
//>> [CTFN-NMIS-008] End
esp_in_range(const struct nf_conntrack_tuple *tuple,
	     enum nf_nat_manip_type maniptype,
	     const union nf_conntrack_man_proto *min,
	     const union nf_conntrack_man_proto *max)
{
   return 1;
}

/* generate unique tuple ... */
//<< [CTFN-NMIS-008] Charles Wei, Support the unspecified source port for IPSec pass-through 2011/04/18
bool
//>> [CTFN-NMIS-008] End
esp_unique_tuple(struct nf_conntrack_tuple *tuple,
                 const struct nf_nat_range *range,
                 enum nf_nat_manip_type maniptype,
                 const struct nf_conn *conntrack)
{
   return 0;
}

/* manipulate a ESP packet according to maniptype */
//<< [CTFN-NMIS-008] Charles Wei, Support the unspecified source port for IPSec pass-through 2011/04/18
bool
esp_manip_pkt(struct sk_buff *pskb, unsigned int iphdroff,
              const struct nf_conntrack_tuple *tuple,
              enum nf_nat_manip_type maniptype)
//>> [CTFN-NMIS-008] End
{
   struct esphdr *esph;
//<< [CTFN-NMIS-008] Charles Wei, Support the unspecified source port for IPSec pass-through 2011/04/18
   struct iphdr *iph = (struct iphdr *)((pskb)->data + iphdroff);
//>> [CTFN-NMIS-008] End
   unsigned int hdroff = iphdroff + iph->ihl * 4;
   __be32 oldip, newip;

   if (!skb_make_writable(pskb, hdroff + sizeof(*esph)))
      return 0;

   if (maniptype == IP_NAT_MANIP_SRC) 
   {
      /* Get rid of src ip and src pt */
      oldip = iph->saddr;
      newip = tuple->src.u3.ip;
   } 
   else 
   {
      /* Get rid of dst ip and dst pt */
      oldip = iph->daddr;
      newip = tuple->dst.u3.ip;
   }

   return 1;
}

//<< [CTFN-NMIS-008] Charles Wei, Support the unspecified source port for IPSec pass-through 2011/04/18
static struct nf_nat_protocol esp;


int __init nf_nat_proto_esp_init(void)
{
   // esp.name = "ESP";
   esp.protonum = IPPROTO_ESP;
   esp.me = THIS_MODULE;
   esp.manip_pkt = esp_manip_pkt;
   esp.in_range = esp_in_range;
   esp.unique_tuple = esp_unique_tuple;
//>> [CTFN-NMIS-008] End
   return nf_nat_protocol_register(&esp);
}

void __exit nf_nat_proto_esp_fini(void)
{
   nf_nat_protocol_unregister(&esp);
}

module_init(nf_nat_proto_esp_init);
module_exit(nf_nat_proto_esp_fini);
