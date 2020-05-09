/*
    Stand up a set of servers and communicate between them with a workload.
    Copyright (C) 2020  Lester Vecsey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>

#include <sys/socket.h>
#include <netdb.h>

#include <endian.h>
#include <string.h>

#include <pthread.h>

#include "carrierworkload.h"

void *udp_routine(void *extra) {

  void *ret;

  unsigned char *buf;

  unsigned char *outbuf;

  uint64_t counter;

  size_t len;

  socklen_t addrlen;

  ssize_t bytes_read;

  threadpack *tp;
  
  struct sockaddr_in dest_addr, target_addr, *target_addrp;
  
  int flags;

  unsigned char *rptr;

  long int procno;

  unsigned char ip_address[4];

  node *target_np;

  int retval;
  
  uint64_t *nodes_ptr;

  uint64_t val;
  
  cw in;
  
  ret = NULL;

  tp = (threadpack*) extra;

  buf = malloc(MAX_PKTSIZE);
  if (buf == NULL) {
    perror("malloc");
    return ret;
  }

  outbuf = malloc(MAX_PKTSIZE);
  if (outbuf == NULL) {
    perror("malloc");
    free(buf);
    return ret;
  }

  flags = 0;
  
  counter = 0;

  for (;;) {

    addrlen = sizeof(struct sockaddr_in);
    bytes_read = recvfrom(tp->s, buf, MAX_PKTSIZE, flags, (struct sockaddr*) &dest_addr, &addrlen); 

    if (bytes_read >= sizeof(in.cmd)) {

      memcpy(&val, buf, sizeof(in.cmd));
      in.cmd = be64toh(val);
      
      printf("[%lu] Received packet, bytes_read %ld\n", counter, bytes_read);

      switch(in.cmd) {
      case CW_WORKLOAD:

	{

	  uint16_t port;
	    
	  cw out;

	  rptr = buf + sizeof(uint64_t);

	  memcpy(&val, rptr, sizeof(uint64_t));
	  in.numseq = be64toh(val);
	  rptr += sizeof(uint64_t);

	  memcpy(&val, rptr, sizeof(uint64_t));
	  in.workload_procs = be64toh(val);
	  rptr += sizeof(uint64_t);

	  if (in.workload_procs > tp->num_nodes) {
	    printf("Too many nodes specified.\n");
	    continue;
	  }
	  
	  nodes_ptr = (uint64_t*) (buf + sizeof(uint64_t) * 3);
	  rptr += sizeof(uint64_t) * in.workload_procs;

	  memcpy(&val, rptr, sizeof(uint64_t));
	  in.workload_state = be64toh(val);	  
	  rptr += sizeof(uint64_t);

	  memcpy(&val, rptr, sizeof(uint64_t));
	  in.startend_processid = be64toh(val);
	  rptr += sizeof(uint64_t);
	    
	  memcpy(in.final_ipv6, rptr, sizeof(in.final_ipv6));
	  rptr += sizeof(in.final_ipv6);

	  memcpy(&val, rptr, sizeof(uint64_t));
	  in.final_port = be64toh(val);
	  rptr += sizeof(uint64_t);

	  memcpy(in.payload, rptr, sizeof(in.payload));
	  rptr += sizeof(in.payload);
	  
	  memcpy(&val, rptr, sizeof(uint64_t));
	  in.itercount = be64toh(val);
	  rptr += sizeof(uint64_t);

	  printf("[cmd=WORKLOAD] itercount %lu\n", in.itercount);
	    
	  if (!(in.itercount)) {

	    if (in.startend_processid == tp->found_processid) {

	      memset(&target_addr, 0, sizeof(target_addr));
	      target_addr.sin_family = AF_INET;
	      
	      port = in.final_port;
	      target_addr.sin_port = htons(port);
	      ip_address[0] = in.final_ipv6[0];
	      ip_address[1] = in.final_ipv6[1];
	      ip_address[2] = in.final_ipv6[2];
	      ip_address[3] = in.final_ipv6[3];
	      memcpy(&(target_addr.sin_addr.s_addr), ip_address, 4);

	      if (!ip_address[0] && !ip_address[1] && !ip_address[2] && !ip_address[3] && !port) {
		printf("Stopping flow of traffic; no final address in packet.\n");
		continue;
	      }
	      
	      target_addrp = &target_addr;
	      
	    }

	    else {

	      uint64_t target_processid;
		
	      target_processid = in.startend_processid;
		
	      if (target_processid >= tp->num_nodes) {
		printf("target_processid %lu greater than num_nodes %lu\n", target_processid, tp->num_nodes);
		break;
	      }
	    
	      target_np = tp->nodes + target_processid;

	      target_addrp = &(target_np->dest_addr);
	      
	    }
	      
	    out.cmd = CW_WORKLOAD;
	    
	    out.numseq = in.numseq;

	    out.workload_procs = in.workload_procs;

	    switch(in.workload_state) {

	    case WORKLOADSTATE_STARTING:
		
	    case WORKLOADSTATE_RUNNING:
	      
	      out.workload_state = WORKLOADSTATE_FINISHED;
	      
	      break;

	    case WORKLOADSTATE_FINISHED:

	      {

		printf("Reached WORKLOADSTATE_FINISHED.\n");

		if (tp->found_processid == in.startend_processid) {

		  long int chno;
		  
		  printf("Payload: ");

		  for (chno = 0; chno < sizeof(in.payload); chno++) {
		    printf("%02x", in.payload[chno]);
		  }

		  putchar('\n');
		  
		  continue;
		  
		}

		else {

		  printf("Sending final packet back to the requesting instance.\n");

		  for (procno = 0; procno < tp->num_nodes; procno++) {
		    if (tp->nodes[procno].process_id == in.startend_processid) {
		      target_addrp = &(tp->nodes[procno].dest_addr);
		      break;
		    }
		  }

		  if (procno == tp->num_nodes) {
		    printf("Problem finding process_id for final hop.\n");
		    continue;
		  }
		  
		}
		
	      }

	      break;
	      
	    }
	    
	    out.startend_processid = in.startend_processid;

	    memcpy(out.final_ipv6, in.final_ipv6, sizeof(in.final_ipv6));		
	    out.final_port = in.final_port;
	    out.itercount = in.itercount;

	    memcpy(out.payload, in.payload, sizeof(out.payload));
	    
	    retval = fill_outbuf(&out, nodes_ptr, outbuf, &len); 
	    if (retval == -1) {
	      printf("Trouble filling outbuf.\n");
	      continue;
	    }
	    
	    {
	      unsigned char ip_address[4];
	      uint16_t port;
	      
	      memcpy(ip_address, &(target_addrp->sin_addr.s_addr), 4);
	      port = ntohs(target_addrp->sin_port);

	      printf("Sending to %u.%u.%u.%u:%u\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3], port);

	    } 

	    retval = sendto(tp->s, outbuf, len, 0, (const struct sockaddr*) target_addrp, addrlen);

	    if (retval == -1) {
	      perror("sendto");
	      exit(EXIT_FAILURE);
	    }
	    
	    printf("(workload itercount 0) Sent packet, len %lu\n", len);

	    break;
	    
	  }
	  
	  if (in.itercount >= 1 && in.workload_state == WORKLOADSTATE_STARTING) {
		
	    out.workload_state = WORKLOADSTATE_RUNNING;

	  }

	  else {

	    memcpy(out.final_ipv6, in.final_ipv6, sizeof(in.final_ipv6));
	    out.final_port = in.final_port;	    

	    out.workload_state = WORKLOADSTATE_FINISHED;
	    
	  }
	    
	  {

	    uint64_t target_processid;

	    memcpy(&target_processid, buf + sizeof(uint64_t) * 3 + in.numseq * sizeof(uint64_t), sizeof(uint64_t));

	    if (target_processid >= tp->num_nodes) {
	      printf(".. target_processid %lu greater than or equal to num_procs %lu\n", target_processid, tp->num_nodes);
	      break;
	    }

	    target_np = tp->nodes + target_processid;

	    target_addrp = &(target_np->dest_addr);

	  }

	  out.cmd = CW_WORKLOAD;

	  out.numseq = ((in.numseq + 1) % in.workload_procs);
	  out.workload_procs = in.workload_procs;
	  out.startend_processid = in.startend_processid;

	  memcpy(out.final_ipv6, in.final_ipv6, sizeof(in.final_ipv6));		
	  out.final_port = in.final_port;

	  {

	    unsigned char values[32];

	    long int chno;
		
	    bytes_read = read(tp->rnd_fd, values, sizeof(values));
	    if (bytes_read != sizeof(values)) {
	      printf("Could not read random values completely.\n");
	      continue;
	    }

	    for (chno = 0; chno < sizeof(values); chno++) {

	      out.payload[chno] = in.payload[chno] ^ values[chno];
		  
	    }
		
	  }
	  
	  out.itercount = (in.itercount - 1);
  
	  retval = fill_outbuf(&out, nodes_ptr, outbuf, &len); 
	  if (retval == -1) {
	    printf("Trouble filling outbuf.\n");
	    continue;
	  }
	  
	  if (!out.final_ipv6[0] && !out.final_ipv6[1] && !out.final_ipv6[2] && !out.final_ipv6[3] && !out.final_port) {
	    printf("Dropping packet, because we no longer have a final_ipv6 or final_port in packet.\n");
	    continue;
	  }

	  retval = sendto(tp->s, outbuf, len, 0, (const struct sockaddr*) target_addrp, addrlen);

	  printf("Sent packet len %lu\n", len);
	    
	  break;
	  
	}

      }
	  
    }
    
    if (bytes_read <= 0) break;

    counter++;

  }

  free(outbuf);

  free(buf);
  
  return ret;
    
}

int cmp_node(const void *a, const void *b) {

  node *first;
  node *second;

  first = (node*) a;
  second = (node*) b;

  if (first->process_id < second->process_id) {
    return -1;
  }

  if (first->process_id > second->process_id) {
    return 1;
  }

  return 0;

}

int fill_outbuf(cw *out, uint64_t *nodes_ptr, unsigned char *outbuf, size_t *len) {

  unsigned char *wptr;

  uint64_t val;
  
  wptr = outbuf;

  val = htobe64(out->cmd);
  memcpy(wptr, &val, sizeof(uint64_t));
  wptr += sizeof(uint64_t);

  val = htobe64(out->numseq);
  memcpy(wptr, &val, sizeof(uint64_t));
  wptr += sizeof(uint64_t);

  val = htobe64(out->workload_procs);
  memcpy(wptr, &val, sizeof(uint64_t));
  wptr += sizeof(uint64_t);

  memcpy(wptr, nodes_ptr, sizeof(uint64_t) * out->workload_procs);
  wptr += sizeof(uint64_t) * out->workload_procs;
    
  val = htobe64(out->workload_state);
  memcpy(wptr, &val, sizeof(uint64_t));
  wptr += sizeof(uint64_t);

  val = htobe64(out->startend_processid);  
  memcpy(wptr, &val, sizeof(uint64_t));
  wptr += sizeof(uint64_t);

  memcpy(wptr, out->final_ipv6, sizeof(out->final_ipv6));
  wptr += sizeof(out->final_ipv6);

  val = htobe64(out->final_port);  
  memcpy(wptr, &val, sizeof(out->final_port));
  wptr += sizeof(out->final_port);

  memcpy(wptr, out->payload, sizeof(out->payload));
  wptr += sizeof(out->payload);
  
  val = htobe64(out->itercount);  
  memcpy(wptr, &val, sizeof(uint64_t));
  wptr += sizeof(uint64_t);
  
  len[0] = (wptr - outbuf);
  
  return 0;
  
}

int main(int argc, char *argv[]) {

  char *process_fn;

  int retval;

  char *bind_ipaddrstr;

  char *public_ipaddrstr;
  
  struct protoent *pent;

  struct sockaddr_in bind_addr, public_addr;

  struct sockaddr_in *target_addrp;

  socklen_t addrlen;
  
  int s;

  size_t len;

  long int procno;

  int rnd_fd;

  unsigned char ip_address[4];

  uint16_t port;
  
  pthread_t udp_thread;
  
  FILE *fp;
  char *line;

  long int num_nodes;
  
  struct nodelist nl_root;

  struct nodelist *nl;

  long int input_ipaddress[4];

  long int input_port;

  int verbose;

  unsigned char *buf;

  uint64_t *nodes_ptr;
  
  process_fn = argc>1 ? argv[1] : "processes.txt";

  bind_ipaddrstr = argc>2 ? argv[2] : NULL;

  public_ipaddrstr = argc>3 ? argv[3] : NULL;  

  verbose = 0;
  
  {

    retval = sscanf(bind_ipaddrstr, "%ld.%ld.%ld.%ld:%ld", input_ipaddress+0, input_ipaddress+1, input_ipaddress+2, input_ipaddress+3, &input_port);
    if (retval != 5) {
      printf("Trouble matching ipv4 address with port.\n");
      return -1;
    }

    {

      memset(&bind_addr, 0, sizeof(bind_addr));

      bind_addr.sin_family = AF_INET;
      bind_addr.sin_port = htons(input_port);
      ip_address[0] = input_ipaddress[0];
      ip_address[1] = input_ipaddress[1];
      ip_address[2] = input_ipaddress[2];
      ip_address[3] = input_ipaddress[3]; 
      memcpy(&bind_addr.sin_addr.s_addr, ip_address, 4); 

    }

    retval = sscanf(public_ipaddrstr, "%ld.%ld.%ld.%ld:%ld", input_ipaddress+0, input_ipaddress+1, input_ipaddress+2, input_ipaddress+3, &input_port);
    if (retval != 5) {
      printf("Trouble matching ipv4 address with port.\n");
      return -1;
    }

    {

      memset(&public_addr, 0, sizeof(public_addr));

      public_addr.sin_family = AF_INET;
      public_addr.sin_port = htons(input_port);
      ip_address[0] = input_ipaddress[0];
      ip_address[1] = input_ipaddress[1];
      ip_address[2] = input_ipaddress[2];
      ip_address[3] = input_ipaddress[3]; 
      memcpy(&(public_addr.sin_addr.s_addr), ip_address, 4); 

    }

  }      
  
  fp = fopen(process_fn, "r");
  if (fp == NULL) {
    perror("fopen");
    return -1;
  }

  line = NULL;
  len = 0;
  
  pent = getprotobyname("UDP");
  if (pent==NULL) {
    perror("getprotobyname");
    return -1;
  }

  s = socket(AF_INET, SOCK_DGRAM, pent->p_proto); 
  if (s==-1) {
    perror("socket");
    return -1;
  }

  addrlen = sizeof(struct sockaddr_in);
  
  retval = bind(s, (const struct sockaddr*) &bind_addr, addrlen);
  if (retval == -1) {
    perror("bind");
    return -1;
  }

  {

    struct nodelist *np;
    
    fp = fopen(process_fn, "r");
    line = NULL;
    len = 0;

    num_nodes = 0;

    nl_root.next = NULL;

    nl = &nl_root;
    
    while ((retval = getline(&line,&len,fp)) != -1) {

      long int process_id;
      
      retval = sscanf(line, "%ld->%ld.%ld.%ld.%ld:%ld", &process_id, input_ipaddress+0, input_ipaddress+1, input_ipaddress+2, input_ipaddress+3, &input_port);

      if (retval != 6) {
	fprintf(stderr, "Error in processes file %s\n", process_fn);
	return -1;
      }

      np = malloc(sizeof(struct nodelist));
      if (np == NULL) {
	perror("malloc");
	return -1;
      }

      np->n.process_id = process_id;

      memset(&(np->n.dest_addr), 0, sizeof(np->n.dest_addr));
      np->n.dest_addr.sin_family = AF_INET;
      np->n.dest_addr.sin_port = htons(input_port);
      ip_address[0] = input_ipaddress[0];
      ip_address[1] = input_ipaddress[1];
      ip_address[2] = input_ipaddress[2];
      ip_address[3] = input_ipaddress[3];
      memcpy(&(np->n.dest_addr.sin_addr.s_addr), ip_address, 4); 

      np->next = nl_root.next;

      nl_root.next = np;
      
      num_nodes++;
      
    }

    free(line);

  }
  
  {

    threadpack tp;
    
    rnd_fd = open("/dev/urandom", O_RDONLY);
    if (rnd_fd == -1) {
      perror("open");
      return -1;
    }

    tp.state = THREAD_RUNNING;
    tp.s = s;
    tp.rnd_fd = rnd_fd;

    tp.num_nodes = num_nodes;
    
    tp.nodes = malloc(tp.num_nodes * sizeof(node));
    if (tp.nodes == NULL) {
      perror("malloc");
      return -1;
    }

    nl = nl_root.next;
    
    for (procno = 0; procno < tp.num_nodes && nl != NULL; procno++) {

      tp.nodes[procno] = nl->n;

      nl = nl->next;
      
    }

    qsort(tp.nodes, num_nodes, sizeof(node), cmp_node);

    for (procno = 0; procno < num_nodes; procno++) {

      memcpy(ip_address, &(tp.nodes[procno].dest_addr.sin_addr.s_addr), 4);
      port = ntohs(tp.nodes[procno].dest_addr.sin_port);

      if (verbose) {
	printf("Matching against %u.%u.%u.%u:%u\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3], port);
      }

      if (port == ntohs(public_addr.sin_port) && !memcmp(ip_address, &(public_addr.sin_addr.s_addr), 4)) {
	tp.found_processid = procno;
	break;
      }
      
    }

    if (procno == num_nodes) {
      fprintf(stderr, "Failed to match public ip in %s\n", process_fn);
      return -1;
    }

    printf("Matched for process_id %ld\n", tp.found_processid);

    nodes_ptr = malloc(sizeof(uint64_t) * num_nodes);
    if (nodes_ptr == NULL) {
      perror("malloc");
      return -1;
    }

    for (procno = 0; procno < num_nodes; procno++) {
      nodes_ptr[procno] = tp.nodes[procno].process_id;
    }
    
    retval = pthread_create(&udp_thread, NULL, udp_routine, &tp);

    buf = malloc(MAX_PKTSIZE);
    if (buf == NULL) {
      perror("malloc");
      return -1;
    }
    
    for (;;) {
    
      fp = fopen("cw.fifo", "r");
      if (fp == NULL) {
	printf("Running in server only mode, since no fifo file found.\n");
	break;
      }
      line = NULL;
      len = 0;

      if (fp != NULL) {

	typedef struct {
	  long int process_id;
	  long int itercount;
	} fifo_cmd;

	fifo_cmd fc;
	
	cw wl;

	int flags;
      
	while ((retval = getline(&line,&len,fp)) != -1) {

	  if (len<=0) {
	    continue;
	  }

	  retval = sscanf(line, "%ld %ld", &(fc.process_id), &(fc.itercount));

	  if(retval != 2) {
	    continue;
	  }	
      
	  printf("FIFO: cmd %s", line);

	  wl.cmd = CW_WORKLOAD;

	  wl.numseq = fc.process_id;
        
	  wl.workload_procs = num_nodes;

	  wl.workload_state = WORKLOADSTATE_STARTING;

	  wl.startend_processid = tp.found_processid;

	  {
	    unsigned char final_ipv6[16];
	    uint64_t final_port;
	    memset(final_ipv6, 0, sizeof(final_ipv6));
	    memcpy(ip_address, &(public_addr.sin_addr.s_addr), 4);
	    final_ipv6[0] = ip_address[0];
	    final_ipv6[1] = ip_address[1];
	    final_ipv6[2] = ip_address[2];	  
	    final_ipv6[3] = ip_address[3];

	    final_port = ntohs(public_addr.sin_port);

	    printf("Setting final ip:port to %u.%u.%u.%u:%lu\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3], final_port);
	    
	  }

	  memset(wl.payload, 0, sizeof(wl.payload));
	  
	  wl.itercount = fc.itercount;

	  retval = fill_outbuf(&wl, nodes_ptr, buf, &len);
	  if (retval == -1) {
	    printf("Trouble with call to fill_outbuf.\n");
	    return -1;
	  }
	    
	  flags = 0;

	  addrlen = sizeof(struct sockaddr_in);

	  target_addrp = &(tp.nodes[fc.process_id].dest_addr);
	
	  retval = sendto(s, buf, len, flags, (const struct sockaddr*) target_addrp, addrlen);

	  fprintf(stderr, "%s: sendto returned %d.\n", __FUNCTION__, retval);
    
	  if (retval == -1) {
	    perror("sendto");
	  }
	
	}

	free(line);
	
	retval = fclose(fp);
	if (retval == -1) {
	  perror("fclose");
	  return -1;
	}

      }
	
    }

    free(buf);
    
    pthread_join(udp_thread, NULL);
    
  }

  free(nodes_ptr);
  
  retval = close(rnd_fd);
  if (retval == -1) {
    perror("close");
    return -1;
  }
  
  return 0;

}
