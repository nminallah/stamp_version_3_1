/*
 *  Copyright (c) 2010 Luca Abeni
 *  Copyright (c) 2010 Csaba Kiraly
 *
 *  This is free software; see lgpl-2.1.txt
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <root.h>
#include "net_helper.h"

#define NW_EMULATION

#ifdef NW_EMULATION
#define MAX_NW_QUEUE	6500

int upload_queue_len[MAX_NW_QUEUE] = {0};
unsigned int upload_timestamp[MAX_NW_QUEUE] = {0};
int upload_queue_indx = 0;
int upload_first_time = 0;
unsigned int upload_drop_count = 0;

int download_queue_len[MAX_NW_QUEUE] = {0};
unsigned int download_timestamp[MAX_NW_QUEUE] = {0};
int download_queue_indx = 0;
int download_first_time = 0;
unsigned int download_drop_count = 0;

extern int wan_emulation_enable;
extern int upload_bitrate_limit;
extern int download_bitrate_limit;

extern unsigned int	 nw_upload_data_size;
extern unsigned int	 nw_download_data_size;

#endif

extern unsigned int upload_data_size;
extern unsigned int download_data_size;

struct nodeID {
  struct sockaddr_in addr;
  int fd;
  uint8_t *buff;
  int bufflen;
};

int wait4data(const struct nodeID *s, struct timeval *tout, int *user_fds)
{
  fd_set fds;
  int i, res, max_fd;

  FD_ZERO(&fds);
  if (s) {
    max_fd = s->fd;
    FD_SET(s->fd, &fds);
  } else {
    max_fd = -1;
  }
  if (user_fds) {
    for (i = 0; user_fds[i] != -1; i++) {
      FD_SET(user_fds[i], &fds);
      if (user_fds[i] > max_fd) {
        max_fd = user_fds[i];
      }
    }
  }
  res = select(max_fd + 1, &fds, NULL, NULL, tout);
  if (res <= 0) {
    return res;
  }
  if (s && FD_ISSET(s->fd, &fds)) {
    return 1;
  }

  /* If execution arrives here, user_fds cannot be 0
     (an FD is ready, and it's not s->fd) */
  for (i = 0; user_fds[i] != -1; i++) {
    if (!FD_ISSET(user_fds[i], &fds)) {
      user_fds[i] = -2;
    }
  }

  return 2;
}

struct nodeID *create_server_node(int port)
{
  struct nodeID *s;
  int res;

  s = malloc(sizeof(struct nodeID));
  memset(s, 0, sizeof(struct nodeID));
  s->addr.sin_family = AF_INET;
  s->addr.sin_port = htons(port);
  s->addr.sin_addr.s_addr = INADDR_ANY;
  s->buff = malloc(60 * 1024 + 1);

  s->fd = -1;

  return s;
}

struct nodeID *create_node(const char *IPaddr, int port)
{
  struct nodeID *s;
  int res;

  s = malloc(sizeof(struct nodeID));
  memset(s, 0, sizeof(struct nodeID));
  s->addr.sin_family = AF_INET;
  s->addr.sin_port = htons(port);
  res = inet_aton(IPaddr, &s->addr.sin_addr);
  if (res == 0) {
    free(s);

    return NULL;
  }
  s->buff = malloc(60 * 1024 + 1);

  s->fd = -1;

  return s;
}

struct nodeID *net_helper_init(const char *my_addr, int port, const char *config)
{
  int res;
  struct nodeID *myself;

  myself = create_server_node(port);
  if (myself == NULL) {
    fprintf(stderr, "Error creating my socket (%s:%d)!\n", my_addr, port);
  }
  myself->fd =  socket(AF_INET, SOCK_DGRAM, 0);
  if (myself->fd < 0) {
    free(myself);

    return NULL;
  }
  fprintf(stderr, "My sock: %d\n", myself->fd);

  res = bind(myself->fd, (struct sockaddr *)&myself->addr, sizeof(struct sockaddr_in));
  if (res < 0) {
    /* bind failed: not a local address... Just close the socket! */
    close(myself->fd);
    free(myself);

    return NULL;
  }

  inet_aton(my_addr, &myself->addr.sin_addr);

  int rcvBufSize = 4*1024*1024;
  setsockopt(myself->fd, SOL_SOCKET, SO_RCVBUF, (char*)&rcvBufSize, sizeof(rcvBufSize));

  int sndBufSize = 4*1024*1024;
  setsockopt(myself->fd, SOL_SOCKET, SO_SNDBUF, (char*)&sndBufSize, sizeof(sndBufSize));

  return myself;
}

void bind_msg_type (uint8_t msgtype)
{
}

#if 1
int send_to_peer(const struct nodeID *from, struct nodeID *to, const uint8_t *buffer_ptr, int buffer_size)
{
  int res, len;
 char toaddr[1024];
 char fromaddr[1024];
 char toIP[1024];
 int start_byte = 1;

 struct nodeID newto;
 
  node_addr(to, toaddr, 256);
  node_addr(from, fromaddr, 256);

  newto= *to;

//Players behind same WAN
#if 1
  node_ip(to, toIP, 256);

  //if toaddr ip = external ip, it means both are behind same wan ip
  if(strcmp(toIP, get_external_ip()) == 0)
  {
	const struct nodeID **neighbours;
	pmetaDataStruct_t pmetaDataParam;	   
	const uint8_t *mdata;
       int i, n;
	int msize;
	int found_index = -1;
	
	neighbours = topoGetNeighbourhood(&n);
	mdata = topoGetMetadata(&msize);
	for (i = 0; i < n; i++) 
	{
		//fprintf(stderr, "i: %d\n", i);
		if(nodeid_cmp(to, neighbours[i]) == 0)
		{
			//fprintf(stderr, "found_index: %d\n", i);
			found_index = i;
			break;
		}
	}

	if(found_index != -1)
	{
		const int *d;
		d = (const int*)(mdata+found_index*msize);
		pmetaDataParam = (pmetaDataStruct_t)d;

		//printf("pmetaDataParam: %d %d %d %d %s %d\n", pmetaDataParam->id, 
		//	pmetaDataParam->is_source, pmetaDataParam->number_of_BFrames, pmetaDataParam->video_layer_number, pmetaDataParam->ip, pmetaDataParam->port);
		if(strlen(pmetaDataParam->ip) > 0)
		{
			//printf("override send IP: %s:%d\n", pmetaDataParam->ip, pmetaDataParam->port);
			newto.addr.sin_addr.s_addr 		= inet_addr(pmetaDataParam->ip);
			newto.addr.sin_port 			= htons(pmetaDataParam->port);
		}
	}

  }
#endif  

  //printf("send: (from: %s-> to: %s): size: %d ++\n", fromaddr, toaddr, buffer_size);
  //printf("send_to_peer++\n");  
  do {
    if (buffer_size > (MAX_UDP_PACKET_SIZE-1)) {
      len = (MAX_UDP_PACKET_SIZE-1);
      //from->buff[0] = 0;
      if(start_byte)
	  from->buff[0] = 0x2; // Start
      else
	  from->buff[0] = 0x0; // Continuation
      start_byte = 0;
    } else {
      len = buffer_size;
//      from->buff[0] = 1;
      if(start_byte)
	  from->buff[0] = 0x3; // Start+End
      else
	  from->buff[0] = 0x1; // End
      start_byte = 0;
    }
    //printf("\t len: %d marker: %d \n", len, from->buff[0]);  
    memcpy(from->buff + 1, buffer_ptr, len);
	//printf("\tsend: (from: %s-> to: %s): hdr: %d len: %d sum: %d\n", fromaddr, toaddr, from->buff[0], len, buffer_size);
    buffer_size -= len;
    buffer_ptr += len;


   upload_data_size += (len+1);

#ifdef NW_EMULATION
   if(wan_emulation_enable == 1)
   {
   	unsigned 	int sum_bytes = 0;
	int i;

   	for(i=0; i<upload_queue_indx;i++)
   	{
   		sum_bytes += upload_queue_len[i];
		//printf("\t upload_queue[%d]: %d sum_bytes: %d\n", i, upload_queue_len[i], sum_bytes);
   	}

//*
	if((sum_bytes*8) > upload_bitrate_limit)
	{
	   	upload_queue_len[upload_queue_indx] = 0;
		res = len + 1;
		upload_drop_count++;
		//printf("upload_drop_count: %d sum_bytes: %d upload_bitrate_limit: %d upload_queue_indx: %d\n", upload_drop_count, (sum_bytes*8), upload_bitrate_limit, upload_queue_indx);
	}
	else
//*/
	{
	   	upload_queue_len[upload_queue_indx] = len+1;

	 	res = sendto(from->fd, from->buff, len + 1, 0, &newto.addr, sizeof(struct sockaddr_in));

		nw_upload_data_size += (len+1);
	}
	
	upload_timestamp[upload_queue_indx] = timer_msec();
	//printf("sum_bytes: %d len: %d (%d) upload_bitrate_limit: %d upload_queue_indx: %d time: %d\n", (sum_bytes*8), (len+1)*8, (len+1), upload_bitrate_limit, upload_queue_indx, (upload_timestamp[upload_queue_indx] - upload_timestamp[0]));
	
	//printf("ts: %u - %u = %u first_time: %d\n", send_router_timestamp[snd_rIndx] , send_router_timestamp[0], (send_router_timestamp[snd_rIndx] - send_router_timestamp[0]), first_time);

//	if( ((upload_timestamp[upload_queue_indx] - upload_timestamp[0]) > 1000 ) && (upload_first_time == 1))
	if( upload_first_time == 1 )
	{
		int one_sec_indx = 0;
		int new_len = 0;
	   	for(i=upload_queue_indx-1; i>=0;i--)
   		{
   			if((upload_timestamp[upload_queue_indx] - upload_timestamp[i]) > 1000)
			{
				//printf("**** One sec index: %d ****\n", i);
				one_sec_indx = i;
				break;
			}
   		}
			
		
		//printf("bitrate: %d upload_drop_count: %d\n", (sum_bytes*8), upload_drop_count);
		new_len = 0;
	   	for(i=one_sec_indx; i<=upload_queue_indx;i++)
	   	{
	   		upload_queue_len[new_len]=upload_queue_len[i];
			upload_timestamp[new_len] = upload_timestamp[i];
			new_len++;
	   	}
		upload_queue_indx = new_len;
		
		//upload_queue_len[upload_queue_indx] = 0;
		//upload_timestamp[upload_queue_indx] = 0;
	}
	else
	{
		upload_queue_indx++;
		upload_first_time = 1;
	}
	
	if(upload_queue_indx >= MAX_NW_QUEUE)
	{
		printf("Send router index exceeded !!!\n");
		exit(-1);
	}

   }
   else
   {
   	//upload_data_size += (len+1);
	res = sendto(from->fd, from->buff, len + 1, 0, &newto.addr, sizeof(struct sockaddr_in));
   }
#else
   //upload_data_size += (len+1);
    res = sendto(from->fd, from->buff, len + 1, 0, &newto.addr, sizeof(struct sockaddr_in));
#endif
  } while (buffer_size > 0);
  //printf("send: (from: %s-> to: %s)--\n",fromaddr, toaddr);

  return res;
}

int recv_from_peer(const struct nodeID *local, struct nodeID **remote, uint8_t *buffer_ptr, int buffer_size)
{
  int res, recv, len, addrlen;
  struct sockaddr_in raddr;
  int once = 1;
  int start_byte = 1;
 
 char toaddr[1024];
 char fromaddr[1024];

  node_addr(local, toaddr, 256);

  *remote = malloc(sizeof(struct nodeID));
  if (*remote == NULL) {
    return -1;
  }

  //printf("recv_from_peer++\n");  
  recv = 0;
  do {
    if (buffer_size > MAX_UDP_PACKET_SIZE) {
      len = MAX_UDP_PACKET_SIZE;
    } else {
      len = buffer_size;
    }
    addrlen = sizeof(struct sockaddr_in);

#ifdef NW_EMULATION
   if(wan_emulation_enable == 1)
   {
   	unsigned 	int sum_bytes = 0;
	int i;
	
       res = recvfrom(local->fd, local->buff, len+1, 0, &raddr, &addrlen);
       if(res < 0)
		return -1;

	download_data_size += res;

	sum_bytes += res;
   	for(i=0; i<download_queue_indx;i++)
   	{
   		sum_bytes += download_queue_len[i];
		//printf("\t upload_queue[%d]: %d sum_bytes: %d\n", i, upload_queue_len[i], sum_bytes);
   	}


	if((sum_bytes*8) > download_bitrate_limit)
	{
	   	download_queue_len[download_queue_indx] = 0;
		download_drop_count++;
		//printf("******** download_drop_count: %d sum_bytes: %d download_bitrate_limit: %d download_queue_indx: %d *********\n", download_drop_count, (sum_bytes*8), download_bitrate_limit, download_queue_indx);
	}
	else
	{
	   	download_queue_len[download_queue_indx] = res;
	}
	
	download_timestamp[download_queue_indx] = timer_msec();
	//printf("sum_bytes: %d len: %d (%d) download_bitrate_limit: %d upload_queue_indx: %d time: %d\n", (sum_bytes*8), (res)*8, (res), download_bitrate_limit, download_queue_indx, (download_timestamp[download_queue_indx] - download_timestamp[0]));
	
	//printf("ts: %u - %u = %u first_time: %d\n", send_router_timestamp[snd_rIndx] , send_router_timestamp[0], (send_router_timestamp[snd_rIndx] - send_router_timestamp[0]), first_time);

//	if( ((upload_timestamp[upload_queue_indx] - upload_timestamp[0]) > 1000 ) && (upload_first_time == 1))
	if( download_first_time == 1 )
	{
		int one_sec_indx = 0;
		int new_len = 0;
	   	for(i=download_queue_indx-1; i>=0;i--)
   		{
   			if((download_timestamp[download_queue_indx] - download_timestamp[i]) > 1000)
			{
				//printf("**** One sec index: %d ****\n", i);
				one_sec_indx = i;
				break;
			}
   		}
			
		
		//printf("bitrate: %d download_drop_count: %d\n", (sum_bytes*8), download_drop_count);
		new_len = 0;
	   	for(i=one_sec_indx; i<=download_queue_indx;i++)
	   	{
	   		download_queue_len[new_len]=download_queue_len[i];
			download_timestamp[new_len] = download_timestamp[i];
			new_len++;
	   	}
		download_queue_indx = new_len;
		
		//upload_queue_len[upload_queue_indx] = 0;
		//upload_timestamp[upload_queue_indx] = 0;
	}
	else
	{
		download_queue_indx++;
		download_first_time = 1;
	}
	
	if((sum_bytes*8) > download_bitrate_limit)
		continue;

	nw_download_data_size += res;

	if(download_queue_indx >= MAX_NW_QUEUE)
	{
		printf("Send router index exceeded !!!\n");
		exit(-1);
	}

   }
   else
   {
	    res = recvfrom(local->fd, local->buff, len+1, 0, &raddr, &addrlen);
	    if(res < 0)
		return -1;

	    download_data_size += res;
   }
#else
    res = recvfrom(local->fd, local->buff, len+1, 0, &raddr, &addrlen);
    if(res < 0)
	return -1;

    download_data_size += res;
	
#endif

  //memcpy(&(*remote)->addr, &raddr, addrlen);
  //node_addr(*remote, fromaddr, 256);
  //if(once)
  //printf("receive: (from: %s to: %s): ++\n", fromaddr, toaddr);
  //once=0;
	//printf("\treceive: (from: %s to: %s): hdr: %d len: %d sum: %d\n", fromaddr, toaddr, local->buff[0], res-1, recv);

    //printf("\t len: %d marker: %d \n", res, local->buff[0]);  
    if(start_byte)
    {
    	if( (local->buff[0] == 0x2) || (local->buff[0] == 0x3 ) ) // First packet Start
	{
	}
	else
	{
    		continue;
	}

       start_byte = 0;
    }
    else
    {
    	if( (local->buff[0] & 0x2) == 0x2 ) // Start packet again received
    		break;    		
    }

    memcpy(buffer_ptr, local->buff + 1, res - 1);
    buffer_size -= (res - 1);
    buffer_ptr += (res - 1);
    recv += (res - 1);

    if( (local->buff[0] & 0x1) == 0x1 ) // End
	break;

  } while (buffer_size > 0);//((local->buff[0] == 0) && (buffer_size > 0));
  memcpy(&(*remote)->addr, &raddr, addrlen);
  (*remote)->fd = -1;
  //printf("receive: (from: %s to: %s): size: %d--\n",fromaddr, toaddr, recv);

  return recv;
}
#else
int send_to_peer(const struct nodeID *from, struct nodeID *to, const uint8_t *buffer_ptr, int buffer_size)
{
  struct msghdr msg = {0};
  uint8_t my_hdr;
  struct iovec iov[2];
  int res;

  iov[0].iov_base = &my_hdr;
  iov[0].iov_len = 1;
  msg.msg_name = &to->addr;
  msg.msg_namelen = sizeof(struct sockaddr_in);
  msg.msg_iovlen = 2;
  msg.msg_iov = iov;

  do {
    iov[1].iov_base = buffer_ptr;
    if (buffer_size > MAX_UDP_PACKET_SIZE) {
      iov[1].iov_len = MAX_UDP_PACKET_SIZE;
      my_hdr = 0;
    } else {
      iov[1].iov_len = buffer_size;
      my_hdr = 1;
    }
    buffer_size -= iov[1].iov_len;
    buffer_ptr += iov[1].iov_len;
    res = sendmsg(from->fd, &msg, 0);
  } while (buffer_size > 0);

  return res;
}

int recv_from_peer(const struct nodeID *local, struct nodeID **remote, uint8_t *buffer_ptr, int buffer_size)
{
  int res, recv;
  struct sockaddr_in raddr;
  struct msghdr msg = {0};
  uint8_t my_hdr;
  struct iovec iov[2];

  iov[0].iov_base = &my_hdr;
  iov[0].iov_len = 1;
  msg.msg_name = &raddr;
  msg.msg_namelen = sizeof(struct sockaddr_in);
  msg.msg_iovlen = 2;
  msg.msg_iov = iov;

  *remote = malloc(sizeof(struct nodeID));
  if (*remote == NULL) {
    return -1;
  }

  recv = 0;
  do {
    iov[1].iov_base = buffer_ptr;
    if (buffer_size > MAX_UDP_PACKET_SIZE) {
      iov[1].iov_len = MAX_UDP_PACKET_SIZE;
    } else {
      iov[1].iov_len = buffer_size;
    }
    buffer_size -= iov[1].iov_len;
    buffer_ptr += iov[1].iov_len;
    res = recvmsg(local->fd, &msg, 0);
    recv += (res - 1);
  } while ((my_hdr == 0) && (buffer_size > 0));
  memcpy(&(*remote)->addr, &raddr, msg.msg_namelen);
  (*remote)->fd = -1;

  return recv;
}
#endif

int node_addr(const struct nodeID *s, char *addr, int len)
{
  int n;

  if (!inet_ntop(AF_INET, &(s->addr.sin_addr), addr, len)) {
    return -1;
  }
  n = snprintf(addr + strlen(addr), len - strlen(addr) - 1, ":%d", ntohs(s->addr.sin_port));

  return n;
}

struct nodeID *nodeid_dup(struct nodeID *s)
{
  struct nodeID *res;

  res = malloc(sizeof(struct nodeID));
  if (res != NULL) {
    memcpy(res, s, sizeof(struct nodeID));
  }

  return res;
}

int nodeid_equal(const struct nodeID *s1, const struct nodeID *s2)
{
  return (memcmp(&s1->addr, &s2->addr, sizeof(struct sockaddr_in)) == 0);
}

int nodeid_cmp(const struct nodeID *s1, const struct nodeID *s2)
{
  return memcmp(&s1->addr, &s2->addr, sizeof(struct sockaddr_in));
}

int nodeid_dump_include_me(uint8_t *b, const struct nodeID *s, size_t max_write_size)
{
  if (max_write_size < sizeof(struct sockaddr_in)) return -1;

  struct sockaddr_in external_address;

  memset(&external_address, 0, sizeof(struct sockaddr_in));

  external_address.sin_family 				= AF_INET; 
  external_address.sin_addr.s_addr 	= inet_addr(get_external_ip());
  external_address.sin_port 				= htons(get_external_port());

  memcpy(b, &external_address, sizeof(struct sockaddr_in));

  return sizeof(struct sockaddr_in);
}

int nodeid_dump(uint8_t *b, const struct nodeID *s, size_t max_write_size)
{
  if (max_write_size < sizeof(struct sockaddr_in)) return -1;

  memcpy(b, &s->addr, sizeof(struct sockaddr_in));

  return sizeof(struct sockaddr_in);
}

struct nodeID *nodeid_undump(const uint8_t *b, int *len)
{
  struct nodeID *res;
  res = malloc(sizeof(struct nodeID));
  if (res != NULL) {
    memcpy(&res->addr, b, sizeof(struct sockaddr_in));
    res->fd = -1;
  }
  *len = sizeof(struct sockaddr_in);

  return res;
}

void nodeid_free(struct nodeID *s)
{
  free(s);
}

int node_ip(const struct nodeID *s, char *ip, int len)
{
  if (inet_ntop(AF_INET, &(s->addr.sin_addr), ip, len) == 0) {
    return -1;
  }

  return 1;
}

int node_port(const struct nodeID *s)
{
  return ntohs(s->addr.sin_port);
}

int node_set_ip(struct nodeID *s, char *ip)
{
  s->addr.sin_addr.s_addr 		= inet_addr(ip);	
}

int node_set_port(struct nodeID *s, int port)
{
  s->addr.sin_port = htons(port);  
}

