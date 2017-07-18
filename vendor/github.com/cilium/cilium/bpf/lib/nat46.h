/*
 *  Copyright (C) 2016-2017 Authors of Cilium
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
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef __LIB_NAT46__
#define __LIB_NAT46__

#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/icmpv6.h>
#include "common.h"
#include "ipv6.h"
#include "eth.h"
#include "dbg.h"

#if defined ENABLE_NAT46
#if defined ENABLE_IPV4 && defined CONNTRACK
#define LXC_NAT46
#else
#warning "ENABLE_NAT46 requires ENABLE_IPv4 and CONNTRACK"
#undef LXC_NAT46
#endif
#else
#undef LXC_NAT46
#endif

static inline int get_csum_offset(__u8 protocol)
{
	int csum_off;

	switch (protocol) {
	case IPPROTO_TCP:
		csum_off = TCP_CSUM_OFF;
		break;
	case IPPROTO_UDP:
		csum_off = UDP_CSUM_OFF;
		break;
	case IPPROTO_ICMP:
		csum_off = (offsetof(struct icmphdr, checksum));
		break;
	case IPPROTO_ICMPV6:
		csum_off = (offsetof(struct icmp6hdr, icmp6_cksum));
		break;
	default:
		return DROP_UNKNOWN_L4;
	}

	return csum_off;
}

static inline int icmp4_to_icmp6(struct __sk_buff *skb, int nh_off)
{
	struct icmphdr icmp4;
	struct icmp6hdr icmp6 = {};

	if (skb_load_bytes(skb, nh_off, &icmp4, sizeof(icmp4)) < 0)
		return DROP_INVALID;
	else
		icmp6.icmp6_cksum = icmp4.checksum;

	switch(icmp4.type) {
	case ICMP_ECHO:
		icmp6.icmp6_type = ICMPV6_ECHO_REQUEST;
		icmp6.icmp6_identifier = icmp4.un.echo.id;
		icmp6.icmp6_sequence = icmp4.un.echo.sequence;
		break;
	case ICMP_ECHOREPLY:
		icmp6.icmp6_type = ICMPV6_ECHO_REPLY;
		icmp6.icmp6_identifier = icmp4.un.echo.id;
		icmp6.icmp6_sequence = icmp4.un.echo.sequence;
		break;
	case ICMP_DEST_UNREACH:
		icmp6.icmp6_type = ICMPV6_DEST_UNREACH;
		switch(icmp4.code) {
		case ICMP_NET_UNREACH:
		case ICMP_HOST_UNREACH:
			icmp6.icmp6_code = ICMPV6_NOROUTE;
			break;
		case ICMP_PROT_UNREACH:
			icmp6.icmp6_type = ICMPV6_PARAMPROB;
			icmp6.icmp6_code = ICMPV6_UNK_NEXTHDR;
			icmp6.icmp6_pointer = 6;
			break;
		case ICMP_PORT_UNREACH:
			icmp6.icmp6_code = ICMPV6_PORT_UNREACH;
			break;
		case ICMP_FRAG_NEEDED:
			icmp6.icmp6_type = ICMPV6_PKT_TOOBIG;
			icmp6.icmp6_code = 0;
			/* FIXME */
			if (icmp4.un.frag.mtu)
				icmp6.icmp6_mtu = htonl(ntohs(icmp4.un.frag.mtu));
			else
				icmp6.icmp6_mtu = htonl(1500);
			break;
		case ICMP_SR_FAILED:
			icmp6.icmp6_code = ICMPV6_NOROUTE;
			break;
		case ICMP_NET_UNKNOWN:
		case ICMP_HOST_UNKNOWN:
		case ICMP_HOST_ISOLATED:
		case ICMP_NET_UNR_TOS:
		case ICMP_HOST_UNR_TOS:
			icmp6.icmp6_code = 0;
			break;
		case ICMP_NET_ANO:
		case ICMP_HOST_ANO:
		case ICMP_PKT_FILTERED:
			icmp6.icmp6_code = ICMPV6_ADM_PROHIBITED;
			break;
		default:
			return DROP_UNKNOWN_ICMP_CODE;
		}
		break;
	case ICMP_TIME_EXCEEDED:
		icmp6.icmp6_type = ICMPV6_TIME_EXCEED;
		break;
	case ICMP_PARAMETERPROB:
		icmp6.icmp6_type = ICMPV6_PARAMPROB;
		/* FIXME */
		icmp6.icmp6_pointer = 6;
		break;
	default:
		return DROP_UNKNOWN_ICMP_TYPE;
	}

	if (skb_store_bytes(skb, nh_off, &icmp6, sizeof(icmp6), 0) < 0)
		return DROP_WRITE_ERROR;

	icmp4.checksum = 0;
	icmp6.icmp6_cksum = 0;
	return csum_diff(&icmp4, sizeof(icmp4), &icmp6, sizeof(icmp6), 0);
}

static inline int icmp6_to_icmp4(struct __sk_buff *skb, int nh_off)
{
	struct icmphdr icmp4 = {};
	struct icmp6hdr icmp6;

	if (skb_load_bytes(skb, nh_off, &icmp6, sizeof(icmp6)) < 0)
		return DROP_INVALID;
	else
		icmp4.checksum = icmp6.icmp6_cksum;

	switch(icmp6.icmp6_type) {
	case ICMPV6_ECHO_REQUEST:
		icmp4.type = ICMP_ECHO;
		icmp4.un.echo.id = icmp6.icmp6_identifier;
		icmp4.un.echo.sequence = icmp6.icmp6_sequence;
		break;
	case ICMPV6_ECHO_REPLY:
		icmp4.type = ICMP_ECHOREPLY;
		icmp4.un.echo.id = icmp6.icmp6_identifier;
		icmp4.un.echo.sequence = icmp6.icmp6_sequence;
		break;
	case ICMPV6_DEST_UNREACH:
		icmp4.type = ICMP_DEST_UNREACH;
		switch(icmp6.icmp6_code) {
		case ICMPV6_NOROUTE:
		case ICMPV6_NOT_NEIGHBOUR:
		case ICMPV6_ADDR_UNREACH:
			icmp4.code = ICMP_HOST_UNREACH;
			break;
		case ICMPV6_ADM_PROHIBITED:
			icmp4.code = ICMP_HOST_ANO;
			break;
		case ICMPV6_PORT_UNREACH:
			icmp4.code = ICMP_PORT_UNREACH;
			break;
		default:
			return DROP_UNKNOWN_ICMP6_CODE;
		}
	case ICMPV6_PKT_TOOBIG:
		icmp4.type = ICMP_DEST_UNREACH;
		icmp4.code = ICMP_FRAG_NEEDED;
		/* FIXME */
		if (icmp6.icmp6_mtu)
			icmp4.un.frag.mtu = htons(ntohl(icmp6.icmp6_mtu));
		else
			icmp4.un.frag.mtu = htons(1500);
		break;
	case ICMPV6_TIME_EXCEED:
		icmp4.type = ICMP_TIME_EXCEEDED;
		icmp4.code = icmp6.icmp6_code;
		break;
	case ICMPV6_PARAMPROB:
		switch(icmp6.icmp6_code) {
		case ICMPV6_HDR_FIELD:
			icmp4.type = ICMP_PARAMETERPROB;
			icmp4.code = 0;
			break;
		case ICMPV6_UNK_NEXTHDR:
			icmp4.type = ICMP_DEST_UNREACH;
			icmp4.code = ICMP_PROT_UNREACH;
			break;
		default:
			return DROP_UNKNOWN_ICMP6_CODE;
		}
	default:
		return DROP_UNKNOWN_ICMP6_TYPE;
	}

	if (skb_store_bytes(skb, nh_off, &icmp4, sizeof(icmp4), 0) < 0)
		return DROP_WRITE_ERROR;

	icmp4.checksum = 0;
	icmp6.icmp6_cksum = 0;
	return csum_diff(&icmp6, sizeof(icmp6), &icmp4, sizeof(icmp4), 0);
}

static inline int ipv6_prefix_match(struct in6_addr *addr,
				    union v6addr *v6prefix)
{
	if (addr->in6_u.u6_addr32[0] == v6prefix->p1 &&
	    addr->in6_u.u6_addr32[1] == v6prefix->p2 &&
	    addr->in6_u.u6_addr32[2] == v6prefix->p3)
		return 1;
	else
		return 0;
}

/*
 * ipv4 to ipv6 stateless nat
 * (s4,d4) -> (s6,d6)
 * s6 = nat46_prefix<s4>
 * d6 = nat46_prefix<d4> or v6_dst if non null
 */
static inline int ipv4_to_ipv6(struct __sk_buff *skb, struct iphdr *ip4,
			       int nh_off, union v6addr *v6_dst)
{
	struct ipv6hdr v6 = {};
	struct iphdr v4;
	int csum_off;
	__be32 csum;
	__be16 v4hdr_len;
	__be16 protocol = htons(ETH_P_IPV6);
	__u64 csum_flags = BPF_F_PSEUDO_HDR;
	union v6addr nat46_prefix = NAT46_PREFIX;
	
	if (skb_load_bytes(skb, nh_off, &v4, sizeof(v4)) < 0)
		return DROP_INVALID;

	if (ipv4_hdrlen(ip4) != sizeof(v4))
		return DROP_INVALID_EXTHDR;

	/* build v6 header */
	v6.version = 0x6;
	v6.saddr.in6_u.u6_addr32[0] = nat46_prefix.p1;
	v6.saddr.in6_u.u6_addr32[1] = nat46_prefix.p2;
	v6.saddr.in6_u.u6_addr32[2] = nat46_prefix.p3;
	v6.saddr.in6_u.u6_addr32[3] = v4.saddr;

	if (v6_dst) {
		v6.daddr.in6_u.u6_addr32[0] = v6_dst->p1;
		v6.daddr.in6_u.u6_addr32[1] = v6_dst->p2;
		v6.daddr.in6_u.u6_addr32[2] = v6_dst->p3;
		v6.daddr.in6_u.u6_addr32[3] = v6_dst->p4;
	} else {
		v6.daddr.in6_u.u6_addr32[0] = nat46_prefix.p1;
		v6.daddr.in6_u.u6_addr32[1] = nat46_prefix.p2;
		v6.daddr.in6_u.u6_addr32[2] = nat46_prefix.p3;
		v6.daddr.in6_u.u6_addr32[3] = htonl((ntohl(nat46_prefix.p4) & 0xFFFF0000) | (ntohl(v4.daddr) & 0xFFFF));
	}

	if (v4.protocol == IPPROTO_ICMP)
		v6.nexthdr = IPPROTO_ICMPV6;
	else
		v6.nexthdr = v4.protocol;
	v6.hop_limit = v4.ttl;
	v4hdr_len = (v4.ihl << 2);
	v6.payload_len = htons(ntohs(v4.tot_len) - v4hdr_len);

	if (skb_change_proto(skb, htons(ETH_P_IPV6), 0) < 0) {
#ifdef DEBUG_NAT46
		printk("v46 NAT: skb_modify failed\n");
#endif
		return DROP_WRITE_ERROR;
	}

	if (skb_store_bytes(skb, nh_off, &v6, sizeof(v6), 0) < 0 ||
	    skb_store_bytes(skb, nh_off - 2, &protocol, 2, 0) < 0)
		return DROP_WRITE_ERROR;

	if (v4.protocol == IPPROTO_ICMP) {
		csum = icmp4_to_icmp6(skb, nh_off + sizeof(v6));
		csum = ipv6_pseudohdr_checksum(&v6, IPPROTO_ICMPV6,
					       ntohs(v6.payload_len), csum);
	} else {
		csum = 0;
		csum = csum_diff(&v4.saddr, 4, &v6.saddr, 16, csum);
		csum = csum_diff(&v4.daddr, 4, &v6.daddr, 16, csum);
		if (v4.protocol == IPPROTO_UDP)
			csum_flags |= BPF_F_MARK_MANGLED_0;
	}

	/* 
	 * get checksum from inner header tcp / udp / icmp
	 * undo ipv4 pseudohdr checksum and
	 * add  ipv6 pseudohdr checksum
	 */
	csum_off = get_csum_offset(v6.nexthdr);
	if (csum_off < 0)
		return csum_off;
	else
		csum_off += sizeof(struct ipv6hdr);

	if (l4_csum_replace(skb, nh_off + csum_off, 0, csum, csum_flags) < 0)
		return DROP_CSUM_L4;

#ifdef DEBUG_NAT46
	printk("v46 NAT: nh_off %d, csum_off %d\n", nh_off, csum_off);
#endif
	return 0;
}

/*
 * ipv6 to ipv4 stateless nat
 * (s6,d6) -> (s4,d4)
 * s4 = <ipv4-range>.<lxc-id>
 * d4 = d6[96 .. 127]
 */
static inline int ipv6_to_ipv4(struct __sk_buff *skb, int nh_off, __be32 saddr)
{
	struct ipv6hdr v6;
	struct iphdr v4 = {};
	int csum_off;
	__be32 csum = 0;
	__be16 protocol = htons(ETH_P_IP);
	__u64 csum_flags = BPF_F_PSEUDO_HDR;

	if (skb_load_bytes(skb, nh_off, &v6, sizeof(v6)) < 0)
		return DROP_INVALID;

	/* Drop frames which carry extensions headers */
	if (ipv6_hdrlen(skb, nh_off, &v6.nexthdr) != sizeof(v6))
		return DROP_INVALID_EXTHDR;

	/* build v4 header */
	v4.ihl = 0x5;
	v4.version = 0x4;
	v4.saddr = saddr;
	v4.daddr = v6.daddr.in6_u.u6_addr32[3];
	if (v6.nexthdr == IPPROTO_ICMPV6)
		v4.protocol = IPPROTO_ICMP;
	else
		v4.protocol = v6.nexthdr;
	v4.ttl = v6.hop_limit;
	v4.tot_len = htons(ntohs(v6.payload_len) + sizeof(v4));
	csum_off = offsetof(struct iphdr, check);
	csum = csum_diff(NULL, 0, &v4, sizeof(v4), csum);

	if (skb_change_proto(skb, htons(ETH_P_IP), 0) < 0) {
#ifdef DEBUG_NAT46
		printk("v46 NAT: skb_modify failed\n");
#endif
		return DROP_WRITE_ERROR;
	}

	if (skb_store_bytes(skb, nh_off, &v4, sizeof(v4), 0) < 0 ||
	    skb_store_bytes(skb, nh_off - 2, &protocol, 2, 0) < 0)
		return DROP_WRITE_ERROR;

	if (l3_csum_replace(skb, nh_off + csum_off, 0, csum, 0) < 0)
		return DROP_CSUM_L3;

	if (v6.nexthdr == IPPROTO_ICMPV6) {
		__be32 csum1 = 0;
		csum = icmp6_to_icmp4(skb, nh_off + sizeof(v4));
		csum1 = ipv6_pseudohdr_checksum(&v6, IPPROTO_ICMPV6,
						ntohs(v6.payload_len), 0);
		csum = csum - csum1;
	} else {
		csum = 0;
		csum = csum_diff(&v6.saddr, 16, &v4.saddr, 4, csum);
		csum = csum_diff(&v6.daddr, 16, &v4.daddr, 4, csum);
		if (v4.protocol == IPPROTO_UDP)
			csum_flags |= BPF_F_MARK_MANGLED_0;
	}
	/* 
	 * get checksum from inner header tcp / udp / icmp
	 * undo ipv6 pseudohdr checksum and
	 * add  ipv4 pseudohdr checksum
	 */
	csum_off = get_csum_offset(v4.protocol);
	if (csum_off < 0)
		return csum_off;
	else
		csum_off += sizeof(struct iphdr);

	if (l4_csum_replace(skb, nh_off + csum_off, 0, csum, csum_flags) < 0)
		return DROP_CSUM_L4;

#ifdef DEBUG_NAT46
	printk("v64 NAT: nh_off %d, csum_off %d\n", nh_off, csum_off);
#endif

	return 0;
}
#endif /* __LIB_NAT46__ */