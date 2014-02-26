
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

struct reliable_state {
  rel_t *next;			/* Linked list for traversing all connections */
  rel_t **prev;

  conn_t *c;			/* This is the connection object */
  int window_size; 
  int my_ackno; 
  int last_seqno_sent;
  packet_t * lastPacketTouched;                                 //is the last packet either sent or received 

  /* Add your own data fields below this */

};
rel_t *rel_list;

/* Creates a new reliable protocol session, returns NULL on failure.
 * Exactly one of c and ss should be NULL.  (ss is NULL when called
 * from rlib.c, while c is NULL when this function is called from
 * rel_demux.) */
rel_t *                                                         //rel_create will return a pointer to a rel_t object
rel_create (conn_t *c, const struct sockaddr_storage *ss,       
	    const struct config_common *cc)
{
  rel_t *r;                                                     //r points to an object of type rel_t

  r = xmalloc (sizeof (*r));                                    //gives r memory that is the size of the object r points to
  memset (r, 0, sizeof (*r));                                   //initializes the value of the memory space starting at r to 0

  r->window_size = cc->window;      
  r->my_ackno = 1;
  r->last_seqno_sent = 0;

  if (!c) {                                                     //if our connection object "c" does not exist, create it
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
  
  if (pkt->seqno < r->my_ackno)
  {
    return;                                                           //drops packet, does not send ack
  }

  int upper_window = r->my_ackno + r->window_size;
  if (pkt->seqno > upper_window)
  {
    return;                                                          //drops packet, does not send ack
  }


  if (n > conn_bufspace(r->c))                                        //r is an instance of rel_t, c is a instance of conn_t
  {
    return;                                                           //drops packet, does not send ack
  }

  if (n <= conn_bufspace(r->c))
  {
    /* Check cksum */
    int to_be_compared_cksum = pkt->cksum;
    pkt->cksum = 0;
    if (cksum(pkt, n) != to_be_compared_cksum)
    {
      return;                                                         //drops packet, does not send ack
    }
    else if (cksum(pkt, n) == to_be_compared_cksum)
    {
      if (n > 12)
      {
        if (pkt->seqno == r->my_ackno)
        {
          r->my_ackno++;
          /* Construct ACK packet */
          packet_t *ackPacket = malloc(sizeof (struct packet));       //uses a "packet_t" type defined in rlib.c line 446
          ackPacket->len = 8;
          ackPacket->ackno = r->my_ackno;
          ackPacket->cksum = cksum(ackPacket, 8);
          conn_sendpkt (r->c, ackPacket, ackPacket->len);             //send ACK packet with my_ackno

          /* Pass to rel_output */
          r->lastPacketTouched = pkt;
          rel_output (r);
          
        }
        else if (pkt->seqno < r->my_ackno)
          return;                                                     //drops packet, does not send ack
        else if (pkt->seqno > r->my_ackno)
        {
          /* add packet to buffer */
        }
      }
    }

  }

  /* - must use an initializer function to return a pointer to a newly constructed ack_packet */

  /* X - logic to evaluate conn_bufspace, which gives buffer available to conn_output.
  if conn_bufspace is full, reject packet and do not send ACK */

  /* X - logic to evaluate checksum; if checskum matches, continue,
  else do not send ACK and reject packet */

  /* if packet size > 12, DATA received: First, check seqno.
  If dupe, send DUPACK[seqno] for correct value of seqno (cumulative ACK).
  If not dupe, check to see if in order. If in order, pass to rel_output and send
  ACK. If out of order, buffer and send DUPACK[seqno] for cumulative ACK, as well
  as ACK for individual packet. */

  /* if packet size == 8, ACK received, ACK logic */
    /* if this_rcvd.ackno < last_sent.seqno, resend packet for which its seqno == this_rcvd.ackno */
 
  /* if packet size == 12, EOF condition reached, EOF logic + send EOF pkt to other side (conn_output w/ length 0) */
 
}


void
rel_read (rel_t *s)                                                                   //this is actually rel_send
{
  /* make sure when you send a data packet it still sets the ACKNO field to my_ackno on it -- Hongze */
  /* while conn_input > 0, drain it */
  /* ensure that the amount you're draining from conn_input is !> reliable_state.window_size) */
  /* ***when an EOF is rcvd, conn_input returns -1 -- is it already checking cksum and len? */
}

void
rel_output (rel_t *r)
{
  /* your data is in r->lastPacketTouched */
  /* call conn_output, make sure you're not trying to call conn_output for more than the value conn_bufspace returns */
}

void
rel_timer ()
{
  /* Retransmit any packets that need to be retransmitted */
  /* Should keep track of received ACKs and outstanding ACKs, retransmit
  as needed, and throttle send rate to match available window space */
}
