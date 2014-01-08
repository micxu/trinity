#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <linux/types.h>
#include <arpa/inet.h>
#include <linux/mroute.h>
#include "sanitise.h"
#include "compat.h"
#include "maps.h"
#include "net.h"
#include "config.h"
#include "random.h"
#include "utils.h"	// ARRAY_SIZE

/* workaround for <linux/in.h> vs. <netinet/in.h> */
#ifndef IP_MULTICAST_ALL
#define IP_MULTICAST_ALL 49
#endif

in_addr_t random_ipv4_address(void)
{
	int addr = 0;
	int class = 0;

	switch (rand() % 9) {
	case 0:	addr = 0;		/* 0.0.0.0 */
		class = 8;
		break;
	case 1:	addr = 0x0a000000;	/* 10.0.0.0/8 */
		class = 8;
		break;
	case 2:	addr = 0x7f000001;	/* 127.0.0.0/8 */
		class = 8;
		break;
	case 3:	addr = 0xa9fe0000;	/* 169.254.0.0/16 (link-local) */
		class = 16;
		break;
	case 4:	addr = 0xac100000;	/* 172.16.0.0/12 */
		class = 12;
		break;
	case 5:	addr = 0xc0586300;	/* 192.88.99.0/24 (6to4 anycast) */
		class = 24;
		break;
	case 6:	addr = 0xc0a80000;	/* 192.168.0.0/16 */
		class = 16;
		break;
	case 7:	addr = 0xe0000000;	/* 224.0.0.0/24 (multicast)*/
		class = 24;
		break;
	case 8:	addr = 0xffffffff;	/* 255.255.255.255 */
		break;
	default:
		break;
	}

	switch (class) {
	case 8:	addr |= rand() % 0xffffff;
		break;
	case 12: addr |= rand() % 0xfffff;
		break;
	case 16: addr |= rand() % 0xffff;
		break;
	case 24: addr |= rand() % 0xff;
		break;
	default: break;
	}
	return htonl(addr);
}

void ipv4_gen_sockaddr(struct sockaddr **addr, socklen_t *addrlen)
{
	struct sockaddr_in *ipv4;

	ipv4 = malloc(sizeof(struct sockaddr_in));
	if (ipv4 == NULL)
		return;

	ipv4->sin_family = PF_INET;
	ipv4->sin_addr.s_addr = random_ipv4_address();
	ipv4->sin_port = htons(rand() % 65535);
	*addr = (struct sockaddr *) ipv4;
	*addrlen = sizeof(struct sockaddr_in);
}

void inet_rand_socket(struct socket_triplet *st)
{
	switch (rand() % 3) {
	case 0: st->type = SOCK_STREAM;     // TCP
		if (rand_bool())
			st->protocol = 0;
		else
			st->protocol = IPPROTO_TCP;
		break;

	case 1: st->type = SOCK_DGRAM;      // UDP
		if (rand_bool())
			st->protocol = 0;
		else
			st->protocol = IPPROTO_UDP;
		break;

	case 2: st->type = SOCK_RAW;
		st->protocol = rand() % PROTO_MAX;
		break;

	default:break;
	}
}

#define NR_SOL_IP_OPTS ARRAY_SIZE(ip_opts)
static const unsigned int ip_opts[] = { IP_TOS, IP_TTL, IP_HDRINCL, IP_OPTIONS,
	IP_ROUTER_ALERT, IP_RECVOPTS, IP_RETOPTS, IP_PKTINFO,
	IP_PKTOPTIONS, IP_MTU_DISCOVER, IP_RECVERR, IP_RECVTTL,
	IP_RECVTOS, IP_MTU, IP_FREEBIND, IP_IPSEC_POLICY,
	IP_XFRM_POLICY, IP_PASSSEC, IP_TRANSPARENT,
	IP_ORIGDSTADDR, IP_MINTTL, IP_NODEFRAG,
	IP_MULTICAST_IF, IP_MULTICAST_TTL, IP_MULTICAST_LOOP,
	IP_ADD_MEMBERSHIP, IP_DROP_MEMBERSHIP,
	IP_UNBLOCK_SOURCE, IP_BLOCK_SOURCE,
	IP_ADD_SOURCE_MEMBERSHIP, IP_DROP_SOURCE_MEMBERSHIP,
	IP_MSFILTER,
	MCAST_JOIN_GROUP, MCAST_BLOCK_SOURCE, MCAST_UNBLOCK_SOURCE, MCAST_LEAVE_GROUP, MCAST_JOIN_SOURCE_GROUP, MCAST_LEAVE_SOURCE_GROUP, MCAST_MSFILTER,
	IP_MULTICAST_ALL, IP_UNICAST_IF,
	MRT_INIT, MRT_DONE, MRT_ADD_VIF, MRT_DEL_VIF,
	MRT_ADD_MFC, MRT_DEL_MFC, MRT_VERSION, MRT_ASSERT,
	MRT_PIM, MRT_TABLE, MRT_ADD_MFC_PROXY, MRT_DEL_MFC_PROXY,
};

void ip_setsockopt(struct sockopt *so)
{
	unsigned char val;
	struct ip_mreqn *mr;
	struct ip_mreq_source *ms;
	int mcaddr;

	so->level = SOL_IP;

	val = rand() % NR_SOL_IP_OPTS;
	so->optname = ip_opts[val];

	switch (ip_opts[val]) {
	case IP_PKTINFO:
	case IP_RECVTTL:
	case IP_RECVOPTS:
	case IP_RECVTOS:
	case IP_RETOPTS:
	case IP_TOS:
	case IP_TTL:
	case IP_HDRINCL:
	case IP_MTU_DISCOVER:
	case IP_RECVERR:
	case IP_ROUTER_ALERT:
	case IP_FREEBIND:
	case IP_PASSSEC:
	case IP_TRANSPARENT:
	case IP_MINTTL:
	case IP_NODEFRAG:
	case IP_UNICAST_IF:
	case IP_MULTICAST_TTL:
	case IP_MULTICAST_ALL:
	case IP_MULTICAST_LOOP:
	case IP_RECVORIGDSTADDR:
		if (rand_bool())
			so->optlen = sizeof(int);
		else
			so->optlen = sizeof(char);
		break;

	case IP_OPTIONS:
		so->optlen = rand() % 40;
		break;

	case IP_MULTICAST_IF:
	case IP_ADD_MEMBERSHIP:
	case IP_DROP_MEMBERSHIP:
		mcaddr = 0xe0000000 | rand() % 0xff;

		mr = malloc(sizeof(struct ip_mreqn));
		if (!mr)
			break;
		memset(mr, 0, sizeof(struct ip_mreqn));
		mr->imr_multiaddr.s_addr = mcaddr;
		mr->imr_address.s_addr = random_ipv4_address();
		mr->imr_ifindex = rand32();

		so->optval = (unsigned long) mr;
		so->optlen = sizeof(struct ip_mreqn);
		break;

	case MRT_ADD_VIF:
	case MRT_DEL_VIF:
		so->optlen = sizeof(struct vifctl);
		break;

	case MRT_ADD_MFC:
	case MRT_ADD_MFC_PROXY:
	case MRT_DEL_MFC:
	case MRT_DEL_MFC_PROXY:
		so->optlen = sizeof(struct mfcctl);
		break;

	case MRT_TABLE:
		so->optlen = sizeof(__u32);
		break;

	case IP_MSFILTER:
		//FIXME: Read size from sysctl /proc/sys/net/core/optmem_max
		so->optlen = rand() % sizeof(unsigned long)*(2*UIO_MAXIOV+512);
		so->optlen |= IP_MSFILTER_SIZE(0);
		break;

	case IP_BLOCK_SOURCE:
	case IP_UNBLOCK_SOURCE:
	case IP_ADD_SOURCE_MEMBERSHIP:
	case IP_DROP_SOURCE_MEMBERSHIP:
		mcaddr = 0xe0000000 | rand() % 0xff;

		ms = malloc(sizeof(struct ip_mreq_source));
		if (!ms)
			break;
		memset(ms, 0, sizeof(struct ip_mreq_source));
		ms->imr_multiaddr.s_addr = mcaddr;
		ms->imr_interface.s_addr = random_ipv4_address();
		ms->imr_sourceaddr.s_addr = random_ipv4_address();

		so->optval = (unsigned long) ms;
		so->optlen = sizeof(struct ip_mreq_source);
		break;

	case MCAST_JOIN_GROUP:
	case MCAST_LEAVE_GROUP:
		so->optlen = sizeof(struct group_req);
		break;

	case MCAST_JOIN_SOURCE_GROUP:
	case MCAST_LEAVE_SOURCE_GROUP:
	case MCAST_BLOCK_SOURCE:
	case MCAST_UNBLOCK_SOURCE:
		so->optlen = sizeof(struct group_source_req);
		break;

	case MCAST_MSFILTER:
		//FIXME: Read size from sysctl /proc/sys/net/core/optmem_max
		so->optlen = rand() % sizeof(unsigned long)*(2*UIO_MAXIOV+512);
		so->optlen |= GROUP_FILTER_SIZE(0);
		break;

	default:
		break;
	}
}
