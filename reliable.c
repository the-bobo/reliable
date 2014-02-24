
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
  int window_size = 2 * c.window;                             //permits SWS = RWS

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
rel_recvpkt (rel_t *r, packet_t *pkt, size_t n)                 //size_t n is the size of packet length in bytes
{
  /* logic to evaluate conn_bufspace, which gives buffer available to conn_output.
  if conn_bufspace is full, reject packet and do not send ACK */

  /* logic to evaluate checksum; if checskum matches, continue,
  else do not send ACK and reject packet */

  /* if packet size == 12, EOF condition reached, EOF logic + send EOF pkt to other side (conn_output w/ length 0) */

  /* if packet size == 8, ACK received, ACK logic */

  /* if packet size > 12, DATA received: First, check seqno.
  If dupe, send DUPACK[seqno] for correct value of seqno (cumulative ACK).
  If not dupe, check to see if in order. If in order, pass to rel_read and send
  ACK. If out of order, buffer and send DUPACK[seqno] for cumulative ACK, as well
  as ACK for individual packet. */
}


void
rel_read (rel_t *s)
{
  /* while conn_input > 0, drain it */
  /* ensure that the amount you're draining from conn_input is !> reliable_state.window_size) */
  /* ***when an EOF is rcvd, conn_input returns -1 -- is it already checking cksum and len? */
}

void
rel_output (rel_t *r)
{
  /* call conn_output, make sure you're not trying to call conn_output for more than the value conn_bufspace returns */
}

void
rel_timer ()
{
  /* Retransmit any packets that need to be retransmitted */
  /* Should keep track of received ACKs and outstanding ACKs, retransmit
  as needed, and throttle send rate to match available window space */
}
