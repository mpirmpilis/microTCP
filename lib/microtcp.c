/*
 * microtcp, a lightweight implementation of TCP for teaching,
 * and academic purposes.
 *
 * Copyright (C) 2015-2017  Manolis Surligas <surligas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */




/* Georgios Mpirmpilis
 * (cs335a) microTCP Netwroks Project - Phase A
 */ 

#include "microtcp.h"
#include "../utils/crc32.h"

microtcp_sock_t microtcp_socket (int domain, int type, int protocol) {
  microtcp_sock_t sock;
  struct timeval timeout;

  sock.sd = socket(domain,type,protocol);
  if (sock.sd == -1) {
    perror("error in creating the socket");
    return sock;  /* return only socket fd which is -1 and terminate from main program */
  }

  sock.seq_number = 0;
  sock.ack_number = 0;
  sock.packets_send = 0;
  sock.packets_received = 0;
  sock.packets_lost = 0;
  sock.bytes_send = 0;
  sock.bytes_received = 0;
  sock.bytes_lost = 0;
  sock.state = UNKNOWN;


  /* set timeout */
  timeout.tv_sec = 0;
  timeout.tv_usec = MICROTCP_ACK_TIMEOUT_US;
  if (setsockopt(sock.sd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval)) < 0) {
    perror("setsockopt");
  }

  return sock;
}



int microtcp_bind (microtcp_sock_t *socket, const struct sockaddr *address,
               socklen_t address_len)
{
  if (bind(socket->sd,address,address_len) == -1) {
    socket->state = INVALID;
    perror("error in binding the local address to socket");
    return -1;
  }
  socket->state = LISTEN;
  return 0; /* successful call on bind returns 0 */
}



/* this is where 3-way handshake takes place... */
int microtcp_connect (microtcp_sock_t *socket, const struct sockaddr *address,
                  socklen_t address_len)
{
  microtcp_header_t sendToServer, receiveFromServer;
  ssize_t bytes_sent = 0, bytes_recvd = -1;

  srand(time(NULL));  /* for generating the random sequence number */

  /* update socket fields before starting 3-way handshake */
  socket->id = CLIENT;
  socket->seq_number =rand();  /* random SYN number */
  socket->ack_number = 0;      /* ack should not have a value, only SYN */
  socket->cwnd = MICROTCP_INIT_CWND;  /* the window size */

  /* setup 1st header with SYN message (send from client) */
  sendToServer.seq_number  = htonl(socket->seq_number);
  sendToServer.ack_number  = htonl(socket->ack_number);
  sendToServer.control     = htons(SYN);  /* first message is SYN */
  sendToServer.window      = htons(MICROTCP_WIN_SIZE);  /* how much data to receive */
  sendToServer.data_len    = htonl(DATA_LENGTH);  /* set to 32 bytes */
  sendToServer.future_use0 = htonl(0);
  sendToServer.future_use1 = htonl(0);
  sendToServer.future_use2 = htonl(0);

  bytes_sent = sendto(socket->sd, &sendToServer, sizeof(microtcp_header_t), 0, address, address_len);
  if (bytes_sent < 0) {
      socket->state = INVALID;
      perror("Error sending SYN to server");
      return -1;
  } else {
      socket->packets_send++;
      socket->bytes_send += bytes_sent;
  }




  /* get the header from server with an expected SYNACK message */
  while (bytes_recvd < 0) {
  	bytes_recvd = recvfrom(socket->sd, &receiveFromServer, sizeof(microtcp_header_t), 0, (struct sockaddr *)address, &address_len);
  }
  
  socket->packets_received++;
  socket->bytes_received += bytes_recvd;



  /* check if ack is seq + 1 and if the response is actually SYN_ACK */
  if (ntohl(receiveFromServer.ack_number) == ntohl(sendToServer.seq_number) + 1) {
    if (ntohl(receiveFromServer.control) == SYN_ACK) {
      socket->seq_number = ntohl(receiveFromServer.ack_number);
      socket->ack_number = ntohl(receiveFromServer.seq_number) + 1;
      socket->init_win_size = ntohs(receiveFromServer.window);
      socket->curr_win_size = ntohs(receiveFromServer.window);


      /* setup header to send ACK after the SYN_ACK we got from the server */
      sendToServer.seq_number = htonl(socket->seq_number);
      sendToServer.ack_number = htonl(socket->ack_number);
      sendToServer.control    = htons(ACK);
    } else {
        socket->state = INVALID;
        perror("Error --> Server did not send SYN_ACK");
        return -1;
    }
  } else {
      socket->state = INVALID;
      perror("Error in +1 in SYN(client) ACK(server)");
      return -1;
  }

  /* the last header to send with ACK message and hoping to establish a connection... */
  bytes_sent = sendto(socket->sd, &sendToServer, sizeof(microtcp_header_t), 0, address, address_len);
  if (bytes_sent < 0) {
      socket->state = INVALID;
      perror("Error sending ACK to server");
      return -1;
  } else {
      socket->packets_send++;
      socket->bytes_send += bytes_sent;
  }  

  memcpy(&(socket->address), address, sizeof(struct sockaddr));
  memcpy(&(socket->address_len), &address_len, sizeof(socklen_t));
  socket->state = ESTABLISHED;  /* connection is made! */
  return 0;
}




/* server side function : wait for incoming connections */
int
microtcp_accept (microtcp_sock_t *socket, struct sockaddr *address,
                 socklen_t address_len)
{
  microtcp_header_t sendToClient, receiveFromClient;
  ssize_t bytes_sent = 0, bytes_recvd = -1;

  socket->id = SERVER;
  socket->init_win_size = MICROTCP_WIN_SIZE;
  socket->curr_win_size = MICROTCP_WIN_SIZE;

  /* receive the first packet from client (should be SYN) */
  while (bytes_recvd < 0) {
    bytes_recvd = recvfrom(socket->sd, &receiveFromClient, sizeof(microtcp_header_t), 0, address, &address_len);
  }

  socket->packets_received++;
  socket->bytes_received += bytes_recvd;


  /* server side: check if client message was SYN */
  if (ntohs(receiveFromClient.control) != SYN) {
      socket->state = INVALID;
      perror("Error --> Expected SYN from client");
      return -1;
  }

  srand(time(NULL));
  /* update server's socket info */
  socket->seq_number = rand();
  socket->ack_number = ntohl(receiveFromClient.seq_number) + 1;
  socket->packets_received++;
  socket->bytes_received += bytes_recvd;
  socket->init_win_size = MICROTCP_WIN_SIZE;
  socket->curr_win_size = MICROTCP_WIN_SIZE;


  /* setup server response header to client's SYN with SYN_ACK */
  sendToClient.seq_number = htonl(socket->seq_number);
  sendToClient.ack_number = ntohl(socket->ack_number);
  sendToClient.control    = htons(SYN_ACK);
  sendToClient.window     = htons(socket->curr_win_size);

  bytes_sent = sendto(socket->sd, &sendToClient, sizeof(microtcp_header_t), 0, address, address_len);
  if (bytes_sent < 0) {
      socket->state = INVALID;
      perror("Error sending SYN_ACK to client");
      return -1;
  } else {
    socket->packets_send++;
    socket->bytes_send += bytes_sent;
  }




  /* expect ACK from client on receive */
  bytes_recvd = -1;
  while (bytes_recvd < 0) {
    bytes_recvd = recvfrom(socket->sd, &receiveFromClient, sizeof(microtcp_header_t), 0, address, &address_len);
  }
  socket->packets_received++;
  socket->bytes_received += bytes_recvd;

  /* check if client's ACK is server's seq incremented by */
  if (ntohl(receiveFromClient.ack_number) == ntohl(sendToClient.seq_number) + 1) {
    if (ntohs(receiveFromClient.control) == ACK) {  /* we received the ACK and ready for connection */
      socket->seq_number = ntohl(receiveFromClient.ack_number);
      socket->state = ESTABLISHED;
    }
  }


  return 0;  /* SUCCESSFULL accept and connection ESTABLISHED!! */
}




int
microtcp_shutdown (microtcp_sock_t *socket, int how)
{
  microtcp_header_t client_h, server_h;
  ssize_t bytes_sent = 0, bytes_recvd = -1;
  struct sockaddr_in addr = socket->address;   
  socklen_t addr_len = socket->address_len;




  /* check if a connection exists before attempting to shutdown */
  if (socket->state != ESTABLISHED) {
    socket->state = INVALID;
    perror("Error --> Cannot shutdown a non established connection");
    return -1;
  }

  /* setup client header with FIN_ACK to server 
   * this is the first message for terminating the connection
   */
  srand(time(NULL));  
  client_h.seq_number = htonl(rand());
  client_h.ack_number = htonl(0);
  client_h.control    = htons(FIN_ACK);
  client_h.window     = htons(MICROTCP_WIN_SIZE);
  client_h.data_len   = htonl(32);
/* future_used members are not used so no need to initialize them.... */

  bytes_sent = sendto(socket->sd, &client_h, sizeof(microtcp_header_t), 0, (struct sockaddr *)&addr, addr_len);
  if (bytes_sent < 0) {
      socket->state = INVALID;
      perror("Error sending FIN_ACK to server for terminating connection");
      return -1;
  } else {
    socket->packets_send++;
    socket->bytes_send += bytes_sent;
  }

  

  while (bytes_recvd < 0) {
  	bytes_recvd = recvfrom(socket->sd, &server_h, sizeof(microtcp_header_t), 0, (struct sockaddr *)&addr, &addr_len);
  }
  socket->packets_received++;
  socket->bytes_received += bytes_recvd;
  socket->state = CLOSING_BY_PEER;



  /* client-side: check if server responded with ACK on client's FIN_ACK */
  if (socket->id == CLIENT && ntohs(server_h.control) == ACK
    && ntohl(server_h.ack_number) == ntohl(client_h.seq_number) + 1) {
    socket->state = CLOSING_BY_HOST;
  } else {
      socket->state = INVALID;
      perror("Error in closing connection");
      return -1;
  }


  /* any other things to be done before server sending FIN_ACK
   * will change the state when it gets called the 2nd time 
   */
  if (socket->id == SERVER) {
    return 0;
  } else {
  	  bytes_recvd = -1;
      while (bytes_recvd < 0) {
        bytes_recvd = recvfrom(socket->sd, &server_h, sizeof(microtcp_header_t), 0, (struct sockaddr *)&addr, &addr_len);
      }
      socket->packets_received++;
      socket->bytes_received += bytes_recvd;
      /* checking if server sent FINACK message */
      if (ntohs(server_h.control) != FIN_ACK) {
        perror("Error sending FIN_ACK in shutdown");
        return -1;
      }


      /* should pass here if FIN_ACK was sent */
      /* setup final ACK response to server's FIN_ACK message */
      client_h.seq_number = server_h.ack_number;
      client_h.ack_number = htonl(ntohl(server_h.seq_number) + 1);
      client_h.control = htons(ACK);
      bytes_sent = sendto(socket->sd, &client_h, sizeof(microtcp_header_t), 0, (struct sockaddr *)&addr, addr_len);
      
      if (bytes_sent < 0) {
        perror("Error sending ACK to server (in response of FIN_ACK) in shutdown");
        return -1;
      } else {
        socket->packets_send++;
        socket->bytes_send += bytes_sent;
      }

      socket->state = CLOSED;   /* connection CLOSED!! */

  }

  return 0;
}





/* ------> For the 2nd phase <------ */


/* determines how many bytes to send based on the current congestion window */
int get_max_bytes(int remaining_bytes, int cwnd, int curr_window) {
  int size = 0;

  if (remaining_bytes < cwnd && remaining_bytes < curr_window) {
    size = remaining_bytes;
  } else if (cwnd < curr_window) {
    size = cwnd;
  } else {
    size = curr_window;
  }

  return size;
}


/* for timeout case */
int min(int MSS, int SSTHRESH) {
  if (MSS < SSTHRESH)
    return MSS;
  else
    return SSTHRESH;
}


ssize_t
microtcp_send (microtcp_sock_t *socket, const void *buffer, size_t length,
               int flags)
{
  int i, chunks = 0, packet_size = 0, bytes_sent = 0, bytes_received = 0, bytes_lost = 0, prev_ack = 0, dup_acks = 0;
  size_t remaining_bytes = length, data_sent = 0;
  socklen_t address_len = socket->address_len;  /* get the address for the server */
  

  struct sockaddr_in address = socket->address;
  /* buffer has size of the header + the data to be sent/received */
  void *sending_buffer   = malloc(sizeof(microtcp_header_t) + MICROTCP_MSS);
  void *receiving_buffer = malloc(sizeof(microtcp_header_t) + MICROTCP_MSS);

  microtcp_header_t sendToServer, receiveFromServer;
  memset(&sendToServer,0,sizeof(microtcp_header_t));



  /* keep sending until you meet the specified length */
  while (data_sent < length) {
    packet_size = get_max_bytes(remaining_bytes, socket->cwnd, socket->curr_win_size);
    chunks = packet_size / MICROTCP_MSS;   /* how many of those to send based on the bytes */
    

    /* start sending the chunks to server.... */
    for (i = 0; i < chunks; i++) {
      memset(sending_buffer,0,sizeof(sizeof(microtcp_header_t) + MICROTCP_MSS));  /* initialize buffer before setup */
      ((microtcp_header_t *)sending_buffer)->data_len   = htonl(MICROTCP_MSS);
      ((microtcp_header_t *)sending_buffer)->seq_number = htonl(socket->seq_number + i * MICROTCP_MSS);  /* pair seq with the corresponding chunk */
      ((microtcp_header_t *)sending_buffer)->checksum = htonl(crc32(sending_buffer, sizeof(microtcp_header_t) + MICROTCP_MSS));  /* provide the checksum */

      /* copy the contents of the incoming buffer + data sent (for ACK later) to our custom buffer that will be sent over up to the MSS */
      memcpy(sending_buffer+sizeof(microtcp_header_t), buffer+data_sent, MICROTCP_MSS);
      

      bytes_sent = sendto(socket->sd, sending_buffer, sizeof(microtcp_header_t)+MICROTCP_MSS,0, (struct sockaddr *)&address, address_len);
      socket->packets_send++;
      socket->bytes_send += bytes_sent;

      data_sent += MICROTCP_MSS;  /* that's the max length to be sent with each packet so increase by that */
      remaining_bytes = length - data_sent;  /* calc how much left */
    }


    /* 1. check for any leftovers of the remaining that didn't fit to last segment */
    if (packet_size % MICROTCP_MSS) {
      memset(sending_buffer, 0, sizeof(microtcp_header_t) + MICROTCP_MSS);  /* clear the buffer before sending the final packet */
      ((microtcp_header_t*)sending_buffer)->data_len   = htonl(packet_size % MICROTCP_MSS);  /* whatever is left to be sent */
      ((microtcp_header_t*)sending_buffer)->seq_number = htonl(socket->seq_number + i * MICROTCP_MSS);
      ((microtcp_header_t*)sending_buffer)->checksum   = htonl(crc32(sending_buffer, sizeof(microtcp_header_t) + packet_size % MICROTCP_MSS));

      memcpy(sending_buffer+sizeof(microtcp_header_t), buffer+data_sent, MICROTCP_MSS);
      bytes_sent = sendto(socket->sd,sending_buffer,sizeof(microtcp_header_t) + packet_size % MICROTCP_MSS,0,(struct sockaddr *)&address,address_len);
      socket->packets_send++;
      socket->bytes_send += bytes_sent;

      data_sent += packet_size % MICROTCP_MSS;
      chunks++;  /* this is the final chunk to be sent [increase num of chunks] */
    }


    /* 2. check for any dup acks [retransmissions here] */
    for (i = 0; i < chunks; i++) {   /* receive the acks from the server */
      bytes_received = recvfrom(socket->sd, &receiveFromServer, sizeof(microtcp_header_t), 0, &(socket->address), &(socket->address_len));
      if (bytes_received < 0) {      /* we have a timeout and need to retransmit*/
        socket->ssthresh = socket->cwnd / 2;
        socket->cwnd = min(MICROTCP_MSS, socket->ssthresh);

        bytes_lost = socket->seq_number - ntohl(receiveFromServer.ack_number);  /* because seq-ack differ in num of bytes to be sent */
        socket->seq_number    = ntohl(receiveFromServer.ack_number);
        socket->curr_win_size = ntohl(receiveFromServer.window);
        data_sent -= bytes_lost;

        if (ntohs(receiveFromServer.control) != ACK) {
          continue;
        } else if (ntohl(receiveFromServer.ack_number) > socket->seq_number) {  /* ack if bigger than seq so we proceed */
          socket->seq_number = ntohl(receiveFromServer.ack_number);
          socket->curr_win_size = ntohl(receiveFromServer.window);
          prev_ack = ntohl(receiveFromServer.ack_number);  /* keep track of it to check against the next two incoming acks */
          dup_acks = 0;
        } else if (prev_ack == ntohl(receiveFromServer.ack_number)) {
          dup_acks++;  /* found a dup ack!! */
        } else if (dup_acks == 3) {  /* fast retransmit activated */
          bytes_lost = socket->seq_number - ntohl(receiveFromServer.ack_number);
          socket->seq_number    = ntohl(receiveFromServer.ack_number);  /* re-initialize with server's ack */
          socket->curr_win_size = ntohl(receiveFromServer.window);
          data_sent -= bytes_lost;
        }
      }


      /* 3. Slow Start - Congestion Avoidance */
      if (ntohl(receiveFromServer.ack_number) > socket->seq_number) {  /* correct ACK -> slow start */
          socket->cwnd += MICROTCP_MSS;
      } else if (socket->cwnd > socket->ssthresh){ /* Congestion Avoidance */
          socket->ssthresh = socket->cwnd / 2;
          socket->cwnd = (socket->cwnd / 2) + 1;
      }


      /* 4. Flow Control */ 
       if (!socket->curr_win_size) {  /* send empty payload to get back to normal */
        sendto(socket->sd, sending_buffer, sizeof(microtcp_header_t), 0, (struct sockaddr *)&address, address_len);
        chunks++;
        usleep(rand()%MICROTCP_ACK_TIMEOUT_US);
       }
    }

  }

  return data_sent;
}




ssize_t
microtcp_recv (microtcp_sock_t *socket, void *buffer, size_t length, int flags)
{
  uint32_t seq;
  microtcp_header_t receiveFromServer, sendToClient;
  size_t remaining_bytes = length, bytes_recvd = 0, bytes_sent = 0, total_bytes = 0;
  struct sockaddr_in address = socket->address; /* get the address */
  socklen_t address_len = socket->address_len;
  uint8_t *buff = malloc(sizeof(microtcp_header_t) + MICROTCP_MSS);
  uint8_t *socket_rcv_buffer = socket->recvbuf;




  if (socket->state != ESTABLISHED) {
    perror("Error : Connection not established");
    return -1;
  }


  while (remaining_bytes > 0) {
    memset(buff, 0, sizeof(microtcp_header_t) + MICROTCP_MSS);  /* clean the buffer first */
    bytes_recvd = recvfrom(socket->sd,buff,sizeof(microtcp_header_t) + MICROTCP_MSS,0,(struct sockaddr *)&address,&address_len);

    if (bytes_recvd < 0) {
      socket->state = INVALID;
      perror("Error receiving bytes from client");
      return -1;
    } else {
      socket->packets_received++;
      socket->bytes_received += bytes_recvd;
      total_bytes += bytes_recvd;
    }


    /* check for a shutdown (after transmission has been completed) */
    if(ntohs(((microtcp_header_t*)buff)->control) == FIN_ACK) {
      /* we received a FIN_ACK, answer with ACK */
      seq = ((microtcp_header_t*)buff)->seq_number;
      sendToClient.ack_number = htonl(ntohl(seq));
      sendToClient.control = htons(ACK);
      bytes_sent = sendto(socket->sd,(const void *)&sendToClient,sizeof(microtcp_header_t),0,(struct sockaddr *)&address,address_len);

      if (bytes_sent < 0) {
        socket->state = INVALID;
        perror("Error sending ACK to FIN_ACK");
        return -1;
      } else {
        socket->packets_send++;
        socket->bytes_send += bytes_sent;
      }
      socket->state = CLOSING_BY_PEER;  /* set to this after sending ACK to FIN_ACK */

      /* check for a shutdown from server side */
      if (microtcp_shutdown(socket,0) == 0) {
        socket->state = CLOSED;  /* all done, set it to closed */
      }

    }
  }

  free(buff);
  return total_bytes;


}
