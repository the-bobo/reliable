
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <poll.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>

#include "rlib.h"

//main is in rlib.c line 904

/* rich_packet structure - includes additional useful per-packet state */
struct rich_packet {
  packet_t packet;
  _Bool has_been_ackd; /* if this is a packet i sent, has it been ackd? */
  int time_sent;  /* when was this packet sent? */
  _Bool is_full; /* is this full? 1 = yes, 0 = no. on delete, set to 0 */
};

struct reliable_state {
  rel_t *next;			/* Linked list for traversing all connections */
  rel_t **prev;
  conn_t *c;			/* This is the connection object */
  int window_size; 
  int my_ackno; 
  int last_seqno_sent;
  packet_t * lastPacketTouched;                                            //is the last packet either sent or received 
  struct rich_packet rcv_window_buffer[512000];                                   //QUESTION - is this correct? trying to make an array of packets...
  struct rich_packet snd_window_buffer[512000];                                   //hardcoded to 1000 full DATA packets worth of bytes
  char *user_input[512000];                                                       //Same question for me, but trying to handle user input
  _Bool rcvd_EOF;

};

rel_t *rel_list;
int global_timer = 0;   //this is a global timer, every time rel_timer is called, increment global_timer by 1

/* Creates a new reliable protocol session, returns NULL on failure.
 * Exactly one of c and ss should be NULL.  (ss is NULL when called
 * from rlib.c, while c is NULL when this function is called from
 * rel_demux.) */
rel_t *                                                                 //rel_create will return a pointer to a rel_t object
rel_create (conn_t *c, const struct sockaddr_storage *ss,       
	    const struct config_common *cc)
{
  rel_t *r;                                                             //r points to an object of type rel_t

  r = malloc (sizeof (*r));                                            //gives r memory that is the size of the object r points to
  memset (r, 0, sizeof (*r));                                           //initializes the value of the memory space starting at r to 0

  r->window_size = cc->window;      
  r->my_ackno = 1;
  r->last_seqno_sent = 0;
  //r->rcv_window_buffer[r->window_size];                               //QUESTION - is this correct?
  //r->snd_window_buffer[r->window_size];                               //hardcoded above in reliable_struct to 1000 worth of packets
  r->rcvd_EOF = 0;

  if (!c) {                                                             //if our connection object "c" does not exist, create it
    c = conn_create (r, ss);
    if (!c) {
      free (r);
      return NULL;
    }
  }

  r->c = c;
  r->next = rel_list;
  r->prev = &rel_list;
  if (rel_list)
    rel_list->prev = &r->next;
  rel_list = r;

  /* Do any other initialization you need here */


  return r;
}

void
rel_destroy (rel_t *r)
{
  if (r->next)
    r->next->prev = r->prev;
  *r->prev = r->next;
  conn_destroy (r->c);
/*
  if (r->rcvd_EOF == 1)
  {
    if (conn_input(r->c) == -1)
    {
      if (all packets i have sent have been ackd)
      {
        if (i have no data left to write with conn_output)
        {
          conn_destroy (r->c);
        }
      }
    }
  }
  */   
  /* Free any other allocated memory here */
}


/* This function only gets called when the process is running as a
 * server and must handle connections from multiple clients.  You have
 * to look up the rel_t structure based on the address in the
 * sockaddr_storage passed in.  If this is a new connection (sequence
 * number 1), you will need to allocate a new conn_t using rel_create
 * ().  (Pass rel_create NULL for the conn_t, so it will know to
 * allocate a new connection.)
 */
void
rel_demux (const struct config_common *cc,
	   const struct sockaddr_storage *ss,
	   packet_t *pkt, size_t len)
{
}

void
rel_recvpkt (rel_t *r, packet_t *pkt, size_t n)                       //size_t n is the size of packet length in bytes
{
  
  /* Packet size check vs. available buffer */
  //if pkt->seqno is outside of receiver window then drop.
  
  if (ntohl(pkt->seqno) < r->my_ackno)
  {
    return;                                                           //drops packet, does not send ack
  }

  int upper_window = r->my_ackno + r->window_size;
  if (ntohl(pkt->seqno) > upper_window)
  {
    return;                                                           //drops packet, does not send ack
  }

/* Do not think this is necessary
  if (n > conn_bufspace(r->c))                                        //r is an instance of rel_t, c is a instance of conn_t
  {
    return;                                                           //drops packet, does not send ack
  }
*/
  //if (n <= conn_bufspace(r->c))
 // {
    /* Check cksum */
    int to_be_compared_cksum = pkt->cksum;                            //pkt->cksum is in network order, and the cksum() function
                                                                      //returns a network order number, so no htonl needs to be called
    pkt->cksum = 0;
    if (cksum(pkt, n) != to_be_compared_cksum)
    {
      return;                                                         //drops packet, does not send ack
    }
    else if (cksum(pkt, n) == to_be_compared_cksum)
    {
      /* Data Packet Handling */
      if (n > 12)
      {
        if (ntohl(pkt->seqno) == r->my_ackno)
        {
          r->my_ackno++;
          /* Construct ACK packet */
          packet_t *ackPacket = malloc(sizeof (struct packet));       //uses a "packet_t" type defined in rlib.c line 446
          ackPacket->len = 8;
          ackPacket->ackno = htonl(r->my_ackno);
          ackPacket->cksum = cksum(ackPacket, 8);
          conn_sendpkt (r->c, ackPacket, ackPacket->len);             //send ACK packet with my_ackno

          /* Pass to rel_output */
          pkt->cksum = to_be_compared_cksum;
          r->lastPacketTouched = pkt;
          rel_output (r);
        }

        /* Out of Order Data Packet Handling */
        else if (ntohl(pkt->seqno) > r->my_ackno)
        {
          if((ntohl(pkt->seqno) - r->my_ackno) > r->window_size)
          {
            return;                                                   //packet is outside of receive window
          }
          /* add packet to buffer */
          if((ntohl(pkt->seqno) - r->my_ackno) <= r->window_size)     //packet is ahead of my_ackno, but w/i receive window
          {
            int position = ntohl(pkt->seqno) % r->window_size;
            if (r->rcv_window_buffer[position].is_full == 0)
            {
              r->rcv_window_buffer[position].packet = *pkt;
              r->rcv_window_buffer[position].is_full = 1;
            }
            else if (r->rcv_window_buffer[position].is_full == 1)
            {
              return;                                                 //no place in rcv_window_buffer for this packet
            }

            /* send DUPACK */
            packet_t *ackPacket = malloc(sizeof (struct packet));     //uses a "packet_t" type defined in rlib.c line 446
            ackPacket->len = 8;
            ackPacket->ackno = htonl(r->my_ackno);
            ackPacket->cksum = cksum(ackPacket, 8);
            conn_sendpkt (r->c, ackPacket, ackPacket->len);           //send DUPACK packet with my_ackno
          }

        }
        else if (ntohl(pkt->seqno) < r->my_ackno)
        {
          return;                                                   //packet is below receive window
        }
      }

      /* ACK Packet Handling */
      if (n == 8)
      {
        if(ntohl(pkt->ackno) == r->last_seqno_sent + 1)
        {
          //delete the packet from the send buffer, as it has been acked
          r->snd_window_buffer[r->last_seqno_sent].has_been_ackd = 1;
          /* send next packets from send window buffer */
          rel_read(r);
        }

        if(ntohl(pkt->ackno) < r->last_seqno_sent + 1)
        {
          return;
          /* currently doing nothing and waiting for timeout to retransmit, not retransmitting the packet whose seqno == pkt->ackno */
        }

        if(ntohl(pkt->ackno) > r->last_seqno_sent + 1)
        {
          return;                                                     //bad ACKNO, reject packet
        }
      }

      /* EOF Packet Handling */
      if (n == 12)
      {
        r->rcvd_EOF = 1;
        rel_output(r);
      }
    }

 // }

  /* X - logic to evaluate conn_bufspace, which gives buffer available to conn_output.
  if conn_bufspace is full, reject packet and do not send ACK */

  /* X - logic to evaluate checksum; if checskum matches, continue,
  else do not send ACK and reject packet */

  /* X - if packet size > 12, DATA received: First, check seqno.
  If dupe, send DUPACK[seqno] for correct value of seqno (cumulative ACK).
  If not dupe, check to see if in order. If in order, pass to rel_output and send
  ACK. If out of order, buffer and send DUPACK[seqno] for cumulative ACK. */

  /* X-  if packet size == 8, ACK received, ACK logic */
    /* if this_rcvd.ackno < last_sent.seqno, resend packet for which its seqno == this_rcvd.ackno */
 
  /* if packet size == 12, EOF condition reached, EOF logic + send EOF pkt to other side (conn_output w/ length 0) */
 
}


void
rel_read (rel_t *s)                                                                   //this is actually rel_send
{
  // Call conn_input and put the result in user_input
  // QUESTION: Do we call this in a loop? It doesn't seem like it;
  // it seems that rel_read is called each time in conn_poll (rlib.c line 498), which
  // is in a loop in the main method
  int input = conn_input(s->c, user_input, s->window_size)

  // If conn_input returns -1, EOF; destroy
  if (input == -1) {
    //rel_destroy();
  }

  // If no data lies in buffer to be sent, return
  if (input == 0) {
    return;
  }

  // If the amount drained from conn_input is > window size, return
  if (conn_input > s->window_size) {
    return;
  }

  size_t i;

  // For every 500-byte chunk of data in the buffer...
  for (i = 0; i < input; i+500) {

  /* make sure when you send a data packet you increment r->last_seqno_sent FIRST before you assign it to
    be the seqno of the packet (since we initialize it to 0 in rel_create) */
    
    // Get seqno and increment it
    s -> last_seqno_sent++;
    int new_seqno = s -> last_seqno_sent;

    // Initialize a new rich_packet
    rich_packet *packet_to_send = malloc(sizeof(rich_packet));

    // Initialize a new packet_t within rich_packet
    packet_to_send->packet = malloc(sizeof(packet_t));

    // Question: how do we go about dynamically-sized packets?
    // I've left packet length as 512 bytes for now; obviously, 
    // this isn't optimal.
    packet_to_send->packet->data = char data[500];
    packet_to_send->packet->len = 512;
    packet_to_send->packet->seqno = htons(new_seqno);

    /* make sure when you send a data packet it still sets the ACKNO field to my_ackno on it -- Hongze */
    packet_to_send->packet->ackno = htons(my_ackno);

    /* compute checksum, again 500 bytes */
    packet_to_send->packet->cksum = cksum(packet_to_send->packet->data, 500)

    /* How best to take data from the buffer and make it into packets? */
    /* I'm going to use memcpy for now, but I need some help here */
    memcpy(packet_to_send->packet->data, user_input, 500)

    /* when you send a packet, edit its timestamp in the snd_window_buffer by setting time_sent to global_timer */
    packet_to_send->time_sent = global_timer;
   
    /* when you send a packet, add it to the snd_window_buffer, and make its index position in that array == its seqno */
    snd_window_buffer[new_seqno] = packet_to_send;  

        /* ---- SENDING PACKET ---- */

    // Do we want to do this here? Should this be done out of the buffer?
    conn_sendpkt(s, rich_packet->packet, 512);

  }

}

void
rel_output (rel_t *r)
{

  /* when r->rcvd_EOF == 1, send an EOF to output by calling conn_output with a len of 0 */
  if(r->rcvd_EOF == 1) {
    conn_output(r->c, );
  }

  /* when you output to screen, change the rcv_window_buffer[position].is_full to 0 */
  r->rcv_window_buffer[my_ackno]->is_full = 0;
  
  /* make sure when you output to screen you check the window_buffer to see if there are packets waiting in there. the buffer
  can accept packets with seqno from (my_ackno + 1) to (my_ackno + window_size) inclusive, so you could be passed a
  data packet with the ackno we've been waiting for and need to print it and flush the buffer also. */


  /* r->lastPacketTouched will contain the last packet received, which you will need to prepend to a full window_buffer if a full
  window_buffer exists */
  

  /* call conn_output, make sure you're not trying to call conn_output for more than the value conn_bufspace returns */
  co

}

void
rel_timer ()
{
  global_timer++;

  /* I want to talk about exactly how to handle received and outstanding ACKs */

  /* Retransmit any packets that need to be retransmitted */
  /* Should keep track of received ACKs and outstanding ACKs, retransmit
  as needed, and throttle send rate to match available window space */
}
