/*
 *  Display all network interface names
 */
#include <stdio.h>            //printf, perror
#include <string.h>           //memset, strlen
#include <stdlib.h>           //exit
#include <unistd.h>           //close
#include <sys/socket.h>       //msghdr
#include <arpa/inet.h>        //inet_ntop
#include <linux/netlink.h>    //sockaddr_nl
#include <linux/rtnetlink.h>  //rtgenmsg,ifinfomsg
#include <errno.h>
#include <net/if.h>
#include <sys/ioctl.h>

#define BUFLEN 8192
#define BUFSIZE 8192

struct nl_req_s {
  struct nlmsghdr hdr;
  struct rtgenmsg gen;
};

void die(char *s)
{
    perror(s);
    exit(1);
}

void rtnl_print_link(struct nlmsghdr * h)
{
    struct ifinfomsg * iface;
    struct rtattr * attr;
    int len;

    iface = NLMSG_DATA(h);
    len = RTM_PAYLOAD(h);

    /* loop over all attributes for the NEWLINK message */
    for (attr = IFLA_RTA(iface); RTA_OK(attr, len); attr = RTA_NEXT(attr, len))
    {
        switch (attr->rta_type)
        {
        case IFLA_IFNAME:
            printf("Interface %d : %s\n", iface->ifi_index, (char *)RTA_DATA(attr));
            break;
        default:
            break;
        }
    }
}

char *strim(char *str) //去除首尾的空格
{
    char *end, *sp, *ep;
    int len;
    sp = str;
    end = str + strlen(str) - 1;
    ep = end;

    while (sp <= end && isspace(*sp))
        sp++;
    while (ep >= sp && isspace(*ep))
        ep--;
    len = (ep < sp) ? 0 : (ep - sp) + 1; //(ep < sp)判断是否整行都是空格
    sp[len] = '\0';
    return sp;
}

// 通过/proc/net/dev 来查询网卡
static int get_name_by_mac(const char *in_mac, char *out_if_name, int name_size)
{
    int ret = -1;
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sockfd == -1)
    {
        // SPM_LOG_ERR("get_name_by_mac socket error");
        return ret;
    }

    FILE *fp = NULL;
    char line_buf[512] = {'\0'};
    char dev_name[128] = {'\0'};
    fp = fopen("/proc/net/dev", "r");
    if (fp == NULL)
    {
        close(sockfd);
        // SPM_LOG_ERR("get_name_by_mac open /proc/net/dev err %s", strerror(errno));
        return ret;
    }

    int len = sizeof(line_buf) - 1;
    while (fgets(line_buf, len, fp) != NULL)
    {
        char *pos = strstr(line_buf, ":");
        if (pos == NULL)
        {
            memset(line_buf, '\0', sizeof(line_buf));
            continue;
        }
        memset(dev_name, '\0', sizeof(dev_name));
        strncpy(dev_name, line_buf, pos - line_buf);
        char *tmp = strim(dev_name);
        struct ifreq ifr;
        strcpy(ifr.ifr_name, tmp);
        if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) == 0)
        {
            // don't count loopback
            if (!(ifr.ifr_flags & IFF_LOOPBACK))
            {
                if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == 0)
                {
                    unsigned char *ptr = (unsigned char *)&ifr.ifr_ifru.ifru_hwaddr.sa_data[0];
                    char if_mac_buff[32] = {'\0'};
                    snprintf(if_mac_buff, 32, "%02x%02x%02x%02x%02x%02x", *ptr, *(ptr + 1), *(ptr + 2), *(ptr + 3), *(ptr + 4), *(ptr + 5));
                    if (strcasecmp(if_mac_buff, in_mac) == 0)
                    {
                        if (strlen(ifr.ifr_name) + 1 > name_size)
                        {
                            // SPM_LOG_ERR("get_name_by_mac if name:%s is too long", ifr.ifr_name);
                            goto end;
                        }
                        strcpy(out_if_name, ifr.ifr_name);
                        ret = 0;
                        goto end;
                    }
                }
            }
        }
        memset(line_buf, '\0', sizeof(line_buf));
    }
end:
    close(sockfd);
    fclose(fp);
    return ret;
}

static
int link_up(	int 				      a_Fd,
            	struct sockaddr_nl 	*a_pSockaddr_nl,
            	int 				      a_Domain,
            	int                  a_UP,
            	int                  a_IndexDevice)
{
   char vBuf[BUFLEN];
   //int vLen  = 0;
   int               vLinkAttribut  = 0;
   struct rtattr*    pRtAttr        = 0;
   struct nlmsghdr*  pNlmsghdr      = 0;
   struct ifinfomsg* pIfinfomsg     = 0;

   memset(vBuf, 0, BUFLEN);

   // assemble the message according to the netlink protocol
   pNlmsghdr 				   = (struct nlmsghdr*)vBuf;
   pNlmsghdr->nlmsg_len 	= NLMSG_LENGTH(sizeof(struct ifinfomsg));
   pNlmsghdr->nlmsg_type 	= RTM_NEWLINK;;
   // we request kernel to send back ack for result checking
   pNlmsghdr->nlmsg_flags 	= NLM_F_REQUEST | NLM_F_ACK;

   pIfinfomsg = (struct ifinfomsg*)NLMSG_DATA(pNlmsghdr);

   pIfinfomsg->ifi_family = AF_UNSPEC;
   pIfinfomsg->ifi_index  = a_IndexDevice;
   pIfinfomsg->ifi_change = 0xffffffff; /* ??? */

   //   action up => state = link down  sans cable
   //   pIfinfomsg->ifi_flags  = 0;
   //   pIfinfomsg->ifi_flags  = IFF_LOWER_UP;                       // eth1: <BROADCAST> mtu 777 qdisc pfifo_fast state DOWN mode DEFAULT group default qlen 1000
   //   pIfinfomsg->ifi_flags  = IFF_RUNNING ;                       // eth1: <BROADCAST> mtu 777 qdisc pfifo_fast state DOWN mode DEFAULT group default qlen 1000
   //   pIfinfomsg->ifi_flags  = IFF_RUNNING | IFF_LOWER_UP;         // eth1: <BROADCAST> mtu 777 qdisc pfifo_fast state DOWN mode DEFAULT group default qlen 1000
   //   pIfinfomsg->ifi_flags  = IFF_UP;                             // eth1: <NO-CARRIER,BROADCAST,UP> mtu 777 qdisc pfifo_fast state DOWN mode DEFAULT group default qlen 1000
   //   pIfinfomsg->ifi_flags  = IFF_UP | IFF_RUNNING ;              // eth1: <NO-CARRIER,BROADCAST,UP> mtu 777 qdisc pfifo_fast state DOWN mode DEFAULT group default qlen 1000
   //   pIfinfomsg->ifi_flags  = IFF_UP | IFF_LOWER_UP;              // eth1: <NO-CARRIER,BROADCAST,UP> mtu 777 qdisc pfifo_fast state DOWN mode DEFAULT group default qlen 1000
   //   pIfinfomsg->ifi_flags  = IFF_UP | IFF_RUNNING | IFF_LOWER_UP;// eth1: <NO-CARRIER,BROADCAST,UP> mtu 777 qdisc pfifo_fast state DOWN mode DEFAULT group default qlen 1000

   //   action = up => state = link down  avec cable
   //   pIfinfomsg->ifi_flags  = 0;
   //   pIfinfomsg->ifi_flags  = IFF_LOWER_UP;                       // eth1: <BROADCAST> mtu 777 qdisc pfifo_fast state DOWN mode DEFAULT group default qlen 1000
   //   pIfinfomsg->ifi_flags  = IFF_RUNNING ;                       // eth1: <BROADCAST> mtu 777 qdisc pfifo_fast state DOWN mode DEFAULT group default qlen 1000
   //   pIfinfomsg->ifi_flags  = IFF_RUNNING | IFF_LOWER_UP;         // eth1: <BROADCAST> mtu 777 qdisc pfifo_fast state DOWN mode DEFAULT group default qlen 1000
   //   pIfinfomsg->ifi_flags  = IFF_UP;                             // eth1: <BROADCAST,UP,LOWER_UP> mtu 777 qdisc pfifo_fast state UP mode DEFAULT group default qlen 1000
   //   pIfinfomsg->ifi_flags  = IFF_UP | IFF_RUNNING ;              // eth1: <BROADCAST,UP,LOWER_UP> mtu 777 qdisc pfifo_fast state UP mode DEFAULT group default qlen 1000
   //   pIfinfomsg->ifi_flags  = IFF_UP | IFF_LOWER_UP;              // eth1: <BROADCAST,UP,LOWER_UP> mtu 777 qdisc pfifo_fast state UP mode DEFAULT group default qlen 1000
   //   pIfinfomsg->ifi_flags  = IFF_UP | IFF_RUNNING | IFF_LOWER_UP;// eth1: <BROADCAST,UP,LOWER_UP> mtu 777 qdisc pfifo_fast state UP mode DEFAULT group default qlen 1000


   //   action = down => state = link up  avec cable
   //   pIfinfomsg->ifi_flags  = 0;
   //   pIfinfomsg->ifi_flags  = IFF_LOWER_UP;                       // eth1: <BROADCAST> mtu 777 qdisc pfifo_fast state DOWN group default qlen 1000
   //   pIfinfomsg->ifi_flags  = IFF_RUNNING ;                       // eth1: <BROADCAST> mtu 777 qdisc pfifo_fast state DOWN group default qlen 1000
   //   pIfinfomsg->ifi_flags  = IFF_RUNNING | IFF_LOWER_UP;         // eth1: <BROADCAST> mtu 777 qdisc pfifo_fast state DOWN group default qlen 1000
   //   pIfinfomsg->ifi_flags  = IFF_UP;                             // eth1: <BROADCAST,UP,LOWER_UP> mtu 777 qdisc pfifo_fast state UP group default qlen 1000
   //   pIfinfomsg->ifi_flags  = IFF_UP | IFF_RUNNING ;              // eth1: <BROADCAST,UP,LOWER_UP> mtu 777 qdisc pfifo_fast state UP group default qlen 1000
   //   pIfinfomsg->ifi_flags  = IFF_UP | IFF_LOWER_UP;              // eth1: <BROADCAST,UP,LOWER_UP> mtu 777 qdisc pfifo_fast state UP group default qlen 1000
   //   pIfinfomsg->ifi_flags  = IFF_UP | IFF_RUNNING | IFF_LOWER_UP;// eth1: <BROADCAST,UP,LOWER_UP> mtu 777 qdisc pfifo_fast state UP group default qlen 1000

   if( a_UP )
   {
      pIfinfomsg->ifi_flags  = IFF_UP | IFF_RUNNING;
   }
   else
   {
      pIfinfomsg->ifi_flags  = 0;                       // eth1: <BROADCAST> mtu 777 qdisc pfifo_fast state DOWN group default qlen 1000
   }
   vLinkAttribut           = a_UP ? 1 : 0 ;


   pRtAttr                 = (struct rtattr*)IFLA_RTA(pIfinfomsg);
   pRtAttr->rta_type       =  IFLA_LINK ;
   memcpy(RTA_DATA(pRtAttr),&vLinkAttribut,sizeof(vLinkAttribut));

   pRtAttr->rta_len        = RTA_LENGTH(sizeof(vLinkAttribut));
   // update nlmsghdr length
   pNlmsghdr->nlmsg_len    = NLMSG_ALIGN(pNlmsghdr->nlmsg_len) + pRtAttr->rta_len;


   // prepare struct msghdr for sending.
   struct iovec iov 	   = { pNlmsghdr, pNlmsghdr->nlmsg_len };
   struct msghdr msg 	= { a_pSockaddr_nl, sizeof(*a_pSockaddr_nl), &iov, 1, NULL, 0, 0 };

   // send netlink message to kernel.
   int r = sendmsg(a_Fd, &msg, 0);
   return (r < 0) ? -1 : 0;
}

int util_get_msg(int fd, struct sockaddr_nl *sa, void *buf, size_t len)
{
   struct iovec iov;
   struct msghdr msg;
   iov.iov_base = buf;
   iov.iov_len = len;

   memset(&msg, 0, sizeof(msg));
   msg.msg_name = sa;
   msg.msg_namelen = sizeof(*sa);
   msg.msg_iov = &iov;
   msg.msg_iovlen = 1;

   return recvmsg(fd, &msg, 0);
}

uint32_t util_parse_nl_msg_OK(void *buf, size_t len)
{
   struct nlmsghdr *nl = NULL;
   nl = (struct nlmsghdr*)buf;
   if (!NLMSG_OK(nl, len)) return 0;
   return nl->nlmsg_type;
}

//*********************************************************************
//* 通过网卡名up/down网卡
//*********************************************************************
int nettool_link_up(const char* a_DeviceName, int a_UP  )
{
   int                  vSocketFD   = 0;
   int                  vLen        = 0;
   struct sockaddr_nl   vSockaddr_nl;
   char                 vBuf[BUFLEN];
   uint32_t             vNl_msg_type;
   struct nlmsgerr      *pErrorStruct = 0;
   int                  vIndexDevice = 0;

   vIndexDevice = if_nametoindex(a_DeviceName);

   if( vIndexDevice == 0 )
   {
      fprintf(stderr,"\nError device %s  not exits ! \n\n",a_DeviceName);
   }
   else
   {
      // First of all, we need to create a socket with the AF_NETLINK domain
      vSocketFD = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
      //check(fd);

      memset(&vSockaddr_nl, 0, sizeof(vSockaddr_nl));
      vSockaddr_nl.nl_family = AF_NETLINK;
      link_up(vSocketFD, &vSockaddr_nl, AF_INET, a_UP,vIndexDevice);

      // after sending, we need to check the result
      vLen = util_get_msg(vSocketFD, &vSockaddr_nl, vBuf, BUFLEN);
      //check(vLen);

      vNl_msg_type = util_parse_nl_msg_OK(vBuf, vLen);

      if (vNl_msg_type == NLMSG_ERROR)
      {
         pErrorStruct = (struct nlmsgerr*)NLMSG_DATA(vBuf);
         switch (pErrorStruct->error) {
            case 0:
               printf("nettool_link_up : Success\n");
               break;
            case -EADDRNOTAVAIL:
               printf("nettool_link_up : Failed\n");
               break;
            case -EPERM:
               printf("nettool_link_up : Permission denied\n");
               break;
            default:
               printf("nettool_link_up : errno=%d %s\n", errno, strerror(pErrorStruct->error));
         }
      }
      if( vSocketFD)
      {
         close(vSocketFD);
      }
   }

   return 0;
}

int main(int argc, char *argv[])
{
    struct sockaddr_nl kernel;
    int s, end=0, len;
    struct msghdr msg;
    struct nl_req_s req;
    struct iovec io;
    char buf[BUFSIZE];

    //build kernel netlink address
    memset(&kernel, 0, sizeof(kernel));
    kernel.nl_family = AF_NETLINK;
    kernel.nl_groups = 0;

    //create a Netlink socket
    if ((s=socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) < 0)
    {
        die("socket");
    }

    //build netlink message
    memset(&req, 0, sizeof(req));
    req.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
    req.hdr.nlmsg_type = RTM_GETLINK;
    req.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    req.hdr.nlmsg_seq = 1;
    req.hdr.nlmsg_pid = getpid();
    req.gen.rtgen_family = AF_INET;

    memset(&io, 0, sizeof(io));
    io.iov_base = &req;
    io.iov_len = req.hdr.nlmsg_len;

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_name = &kernel;
    msg.msg_namelen = sizeof(kernel);

    //send the message
    if (sendmsg(s, &msg, 0) < 0)
    {
        die("sendmsg");
    }

	if (argc < 2) {
		fprintf(stderr, "usage: netlink_test wlan0 1\n");
		exit(1);
	}

	const char *dev = argv[1];
	int state = atoi(argv[2]);
	nettool_link_up(dev, state);
	printf("set network card:%s state(1:on, 0:off):%d\n", dev, state);
    //parse reply
    while (!end)
    {
        memset(buf, 0, BUFSIZE);
        msg.msg_iov->iov_base = buf;
        msg.msg_iov->iov_len = BUFSIZE;
        if ((len=recvmsg(s, &msg, 0)) < 0)
        {
            die("recvmsg");
        }
        for (struct nlmsghdr * msg_ptr = (struct nlmsghdr *)buf;
             NLMSG_OK(msg_ptr, len); msg_ptr = NLMSG_NEXT(msg_ptr, len))
        {
            switch (msg_ptr->nlmsg_type)
            {
            case NLMSG_DONE:
                end++;
                break;
            case RTM_NEWLINK:
                rtnl_print_link(msg_ptr);
                break;
            default:
                printf("Ignored msg: type=%d, len=%d\n", msg_ptr->nlmsg_type, msg_ptr->nlmsg_len);
                break;
            }
        }
    }

    close(s);
    return 0;
}