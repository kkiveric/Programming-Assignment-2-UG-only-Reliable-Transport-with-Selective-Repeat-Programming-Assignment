#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"

/* ******************************************************************
   Go Back N protocol.  Adapted from J.F.Kurose
   ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.2

   Network properties:
   - one way network delay averages five time units (longer if there
   are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
   or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
   (although some can be lost).

   Modifications:
   - removed bidirectional GBN code and other code not used by prac.
   - fixed C style to adhere to current programming style
   - added GBN implementation
**********************************************************************/

#define RTT  16.0       /* round trip time.  MUST BE SET TO 16.0 when submitting assignment */
#define WINDOWSIZE 6    /* the maximum number of buffered unacked packet
                          MUST BE SET TO 6 when submitting assignment */
#define SEQSPACE 7      /* the min sequence space for GBN must be at least windowsize + 1 */
#define NOTINUSE (-1)   /* used to fill header fields that are not being used */

/* generic procedure to compute the checksum of a packet.  Used by both sender and receiver
   the simulator will overwrite part of your packet with 'z's.  It will not overwrite your
   original checksum.  This procedure must generate a different checksum to the original if
   the packet is corrupted.
*/
int ComputeChecksum(struct pkt packet)
{
  int checksum = 0;
  int i;

  checksum = packet.seqnum;
  checksum += packet.acknum;
  for ( i=0; i<20; i++ )
    checksum += (int)(packet.payload[i]);

  return checksum;
}

bool IsCorrupted(struct pkt packet)
{
  if (packet.checksum == ComputeChecksum(packet))
    return (false);
  else
    return (true);
}


/********* Sender (A) variables and functions ************/

static struct pkt buffer[SEQSPACE];
static bool acked[SEQSPACE];
static float timers[SEQSPACE]; //used to simulate per-packet timing
static int A_base = 0;
static int A_nextseqnum = 0;

/* called from layer 5 (application layer), passed the message to be sent to other side */
void A_output(struct msg message) {
    if (((A_nextseqnum + SEQSPACE - A_base) % SEQSPACE) < WINDOWSIZE) {
        struct pkt sendpkt;
        sendpkt.seqnum = A_nextseqnum;
        sendpkt.acknum = NOTINUSE;
        for (int i = 0; i < 20; i++)
            sendpkt.payload[i] = message.data[i];
        sendpkt.checksum = ComputeChecksum(sendpkt);

        buffer[A_nextseqnum] = sendpkt;
        acked[A_nextseqnum] = false;

        tolayer3(A, sendpkt);
        starttimer(A, RTT); //shared timer triggers checking/resending 

        if (TRACE > 0)
            printf("----A: sent packet %d\n", sendpkt.seqnum);

        A_nextseqnum = (A_nextseqnum + 1) % SEQSPACE;
    } else {
        if (TRACE > 0)
            printf("----A: window is full, cannot send\n");
        window_full++;
    }
}

void A_input(struct pkt packet) {
    if (!IsCorrupted(packet)) {
        int acknum = packet.acknum;
        if (!acked[acknum]) {
            acked[acknum] = true;
            if (TRACE > 0)
                printf("----A: ACK %d received\n", acknum);
            total_ACKs_received++;
            new_ACKs++;
        }

        // Slide window
        while (acked[A_base]) {
            acked[A_base] = false;
            A_base = (A_base + 1) % SEQSPACE;
        }
    } else {
        if (TRACE > 0)
            printf("----A: corrupted ACK received, ignored\n");
    }
}

void A_timerinterrupt(void) {
    if (TRACE > 0)
        printf("----A: Timer interrupt, checking unACKed packets\n");

    for (int i = 0; i < WINDOWSIZE; i++) {
        int seq = (A_base + i) % SEQSPACE;
        if (!acked[seq] && ((A_nextseqnum + SEQSPACE - A_base) % SEQSPACE) > i) {
            tolayer3(A, buffer[seq]);
            packets_resent++;
            if (TRACE > 0)
                printf("----A: Resent packet %d\n", seq);
        }
    }
    starttimer(A, RTT); // restart shared timer
}

void A_init(void) {
    A_base = 0;
    A_nextseqnum = 0;
    for (int i = 0; i < SEQSPACE; i++)
        acked[i] = false;
}


/********* Receiver (B) variables and functions ************/

static int B_expected = 0;
static struct pkt B_buffer[SEQSPACE];
static bool B_received[SEQSPACE];

void B_input(struct pkt packet) {
    if (!IsCorrupted(packet)) {
        if (TRACE > 0)
            printf("----B: Packet %d received correctly\n", packet.seqnum);

        packets_received++;

        int seq = packet.seqnum;
        if (!B_received[seq]) {
            B_received[seq] = true;
            B_buffer[seq] = packet;
        }

        // Deliver in order
        while (B_received[B_expected]) {
            tolayer5(B, B_buffer[B_expected].payload);
            B_received[B_expected] = false;
            B_expected = (B_expected + 1) % SEQSPACE;
        }

        // Send ACK for every received packet
        struct pkt ackpkt;
        ackpkt.seqnum = 0;
        ackpkt.acknum = seq;
        for (int i = 0; i < 20; i++) ackpkt.payload[i] = '0';
        ackpkt.checksum = ComputeChecksum(ackpkt);
        tolayer3(B, ackpkt);

    } else {
        if (TRACE > 0)
            printf("----B: Corrupted packet received, ignored\n");
    }
}

void B_init(void) {
    B_expected = 0;
    for (int i = 0; i < SEQSPACE; i++)
        B_received[i] = false;
}

/******************************************************************************
 * The following functions need be completed only for bi-directional messages *
 *****************************************************************************/

/* Note that with simplex transfer from a-to-B, there is no B_output() */
void B_output(struct msg message)
{
}

/* called when B's timer goes off */
void B_timerinterrupt(void)
{
}
