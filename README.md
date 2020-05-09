Stand up a set of servers and tell one to start collecting work.

A set of random bits from each server is pooled together into a final output.

Edit processes.txt with the addresses of a few servers.
These should be public addresses and in the form of process_id->address:port

```console
  0->ad.dr.es.ss:5346
  1->ad.dr.es.ss:5346
  2->ad.dr.es.ss:5346  
```

Put the same processes.txt on each machine, along with preparing cw.fifo on one or more of the machines.

```console
    mkfifo cw.fifo
```

Start the server on each machine:

```console
      ./carrierworkload ./processes.txt bind_ip:5346 ad.dr.es.ss:5346
```

The program will look through processes.txt and determine, from the public ad.dr.es.ss IP, the process_id to assign itself. You should also specify the bind_ip which can be the same as the public IP for the machine or even a private IP of the machine as long as you are port forwarding the public ip:port to the private ip:port.

The cw.fifo is used to send a command to one of the carrierworkload processes:

```console
    echo 0 100 > ./cw.fifo
```

Tell the locally running carrierworkload process to transmit a udp packet to process_id 0, with an itercount of 100. The process will reduce the itercount by 1 and send a packet to the next process_id, in sequence, wrapping around to the starting process_id as needed. Along the way random data is collected and the result is emitted from the locally running carrierworkload process that initiated the request.

The request will take a varying amount of time depending on the ping times between the servers, and how high you set the itercount. 

All nodes must be running, with firewall and/or port forwarding rules allowing the public port.

Uses the be64toh and htobe64 functions of endian.h

