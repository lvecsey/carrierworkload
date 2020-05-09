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

#ifndef CARRIERWORKLOAD_H
#define CARRIERWORKLOAD_H

#include <stdint.h>

#include <netdb.h>

enum { CW_NONE, CW_WORKLOAD };

enum { WORKLOADSTATE_NONE, WORKLOADSTATE_STARTING, WORKLOADSTATE_RUNNING, WORKLOADSTATE_FINISHED };

typedef struct {

  uint64_t cmd;

  uint64_t process_id;
  
} p2p_cmd;

enum { THREAD_NONE, THREAD_RUNNING, THREAD_COMPLETE };

typedef struct {

  uint64_t process_id;

  struct sockaddr_in dest_addr;  
  
} node;

struct nodelist {

  node n;
  
  struct nodelist *next;
  
};

typedef struct {

  uint64_t state;

  int s;
  
  int rnd_fd;

  uint64_t found_processid;

  long int num_nodes;
  
  node *nodes;

} threadpack;

typedef struct {

  uint64_t cmd;
  
  uint64_t numseq;

  uint64_t workload_procs;

  uint64_t *process_ids;

  uint64_t workload_state;
  uint64_t startend_processid;
  unsigned char final_ipv6[16];
  uint64_t final_port;

  unsigned char payload[32];
  
  uint64_t itercount;

} cw;

#define MAX_PKTSIZE 1536

int fill_outbuf(cw *out, uint64_t *nodes_ptr, unsigned char *outbuf, size_t *len);

#endif
