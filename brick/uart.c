#ifndef __UNIT_TEST__
  // Do not load avr headers in unit test code
  #include <avr/io.h>
  #include <avr/interrupt.h>
  #include <avr/pgmspace.h>


  #include <rdbuf.h>
#endif

#include "uart.h"

#include <string.h> /* nessessary? */
#define bzero(dst, size) memset(dst, 0, size)

#define BAUD_RATE 9600
#define UART_BITTIME(bit) (((1+2*bit)*F_CPU)/(2L*BAUD_RATE*UA_TMR_PRESCALE_NUM))

/* For ATtiny85 */
#define TX_DDR  DDRB
#define TX_PORT PORTB
#define TX_NUM  PB4

#define RX_DDR  DDRB
#define RX_PIN  PINB
#define RX_NUM  PB3

#define UA_TMR_PRESCALE_REG (_BV(CS01) | _BV(CS00))
#define UA_TMR_PRESCALE_NUM 64
#define UA_BYTE_GAP_TIME UART_BITTIME(9) / 2 // About half a byte of delay


#define PING_TMR_PRESCALE_REG (_BV(CS13) | _BV(CS11))
#define PING_TMR_PRESCALE_NUM 512

// For F_CPU ATtiny clock and prescaler value of UA_TMR_PRESCALE_NUM
const uint8_t uart_times[] PROGMEM= {
  UART_BITTIME(0),  // Start bit
  UART_BITTIME(1), UART_BITTIME(2), UART_BITTIME(3), UART_BITTIME(4), // Data bits
  UART_BITTIME(5), UART_BITTIME(6), UART_BITTIME(7), UART_BITTIME(8),
  UART_BITTIME(9), // Stop bit
};


//Accepting a new transmission and continuing it
ISR(PCINT0_vect)
{
  if (RX_PIN & _BV(RX_NUM)) {
    /*
     * Verify that pin is low.
     * Search for #USBSerialConvertersSuck to find
     * out why this is nessessary.
     * Also upon startup the pin status might be unkown
     */

    return;
  }

  // Reset and start the bit timer
  OCR0A= pgm_read_byte(&uart_times[0]);
  TIMSK|= _BV(OCIE0A); /* Timer/Counter0 Output Compare Match A Interrupt Enable */
  TCNT0= 0;
  TCCR0B= UA_TMR_PRESCALE_REG; /* start timer */

  // This is a dirty fix to make sure the stop bit
  // is sent even if the sender is in a hurry and sends the
  // next start bit a bit early #USBSerialConvertersSuck
  TX_PORT|= _BV(TX_NUM);

  // Disable Pin change interrupts while the transmission
  // is active
  GIMSK&= ~_BV(PCIE);

  if (!uart.flags.transmission) {
    // This is not a byte expected by an active
    // transmission. So start one
    bzero(&uart.hdr_rvcd, sizeof(uart.hdr_rvcd));

    uart.flags.transmission= 1;
    uart.flags.active_clock= 0;
    uart.flags.forward= 0;
    uart.flags.backward= 1;
    uart.flags.rcving_header= 1;

    uart.rcvd_index= 0;

    uart.bitnum= 0;
    return;
  }
  // may check if aq later
  else if (uart.rcvd_index== 1) {
    // aq
    if(uart.hdr_rvcd[0]==0x00 && uart.hdr_rvcd[1]==0x01) {
      uart.flags.forward= 1;
    }
    //ping
    else if(uart.hdr_rvcd[0]==0x00 && uart.hdr_rvcd[1]==0x02) {

      uart.flags.ping_rcvd= 1;
      TCCR0B= 0;

      /* wait for aq */
      GIMSK|= _BV(PCIE);
      uart.flags.transmission= 0;
    }
  }
  else if (uart.rcvd_index== 3) {
    uart.passive_len = (uint16_t)(uart.hdr_rvcd[2] << 8)
      | (uint16_t)uart.hdr_rvcd[3];
  }

  else if (uart.rcvd_index==5){
    // assumes aq, may check cksum later
    uart.flags.rcving_header= 0;
  }

  /* This should be the last PC interrupt. */
  if (uart.rcvd_index==uart.passive_len + 2) {
    uart.flags.active_clock= 1;
  }

  uart.rcvd_index++;
  uart.bitnum= 0;
}


ISR(TIMER1_OVF_vect)
{
  /* time is over, there was no ping ->
   * Starting a new transmission manual */
   if (!uart.flags.ping_rcvd) {

     /* starts make_aq */
     uart.flags.first_brick= 1;

     OCR0A= pgm_read_byte(&uart_times[0]);
     TIMSK|= _BV(OCIE0A);
     TCNT0= 0;
     TCCR0B= UA_TMR_PRESCALE_REG;

     /* Disable Pin change interrupts while the transmission
      * is active */
     GIMSK&= ~_BV(PCIE);

     uart.flags.transmission= 1;
     uart.flags.active_clock= 1;
     uart.flags.forward= 1;
     uart.flags.backward= 0;

     uart.bitnum= 0;

     /* make_aq() should start now, because
      * uart.flags.first_brick && !uart.flags.aq_complete */
   }
   else {
     /* Disable this interrupt */
     TIMSK&= ~_BV(TOIE1);

     /* Stop timer */
     TCCR1= 0;
   }
}


ISR(TIMER0_COMPA_vect)
{
  static uint8_t b_send, b_rcvd= 0x00; /* rcvd==received */


  if (uart.bitnum== 0) { // start bit
    b_rcvd= 0x00;
    //printf("b_rcvd: %X\n", (char)b_rcvd);

    if (uart.flags.forward) {
      // Load next buffer byte

      int8_t bufstat= rdbuf_pop(&uart.buf, (char *) &b_send);


      if (bufstat==BUFFER_EMPTY && uart.flags.active_clock) {
        // The buffer ran empty
        uart.flags.transmission= 0;
        // Transmission complete
        if(uart.hdr_rvcd[0]==0x0
              && uart.hdr_rvcd[1]==0x1){
                uart.flags.aq_complete= 1;
        }
        else { /* ping complete, continue */
          GIMSK|= _BV(PCIE);
        }

      }
      else if (bufstat==HIT_RESV) {
        /* transmit nothing until
         * resv is finished */
        b_send= 0xFF;
      }
      else /* if (bufstat>0) */ {
        // Forward the start bit if forwarding is requested
        TX_PORT&= ~_BV(TX_NUM);
      }
    }
  }

  else if (uart.bitnum >= 9) { // stop bit
    if (uart.flags.forward) {
      // Forward the stop bit if forwarding is requested
      TX_PORT|= _BV(TX_NUM);
    }

    /* Dont verify that a correct stop bit was received
     * #USBSerialConvertersSuck */

    if (uart.flags.backward) {
      if (!uart.flags.rcving_header) {
         /* The header is not meant for the buffer */

        if (rdbuf_push(&uart.buf, b_rcvd) < 0) {
          /* The buffer is full. Everything will break.
           * This should not happen */
           // panic();
        }
      }
      else {
        uart.hdr_rvcd[uart.rcvd_index]= b_rcvd;
        if (uart.rcvd_index >= 5) {
          uart.flags.rcving_header= 0;
        }
      }
    }

    if (uart.flags.active_clock) {
      if (rdbuf_len(&uart.buf)>0) {

        // Prepare interrupt for next cycle, compare match int is still enabled
        OCR0A= pgm_read_byte(&uart_times[0]);

        /* The counter will count up to 255, then wrap around
         * to zero, reach uart_times[0] and start a new cycle */
        TCNT0= 255 - UA_BYTE_GAP_TIME;

        uart.bitnum= 0;
      }
      /* When everything is transfered */
      else {
        GIMSK|= _BV(PCIE); /* Enable Pin Change interrupt */
        TCCR0B= 0; /* Stop timer */
        /* There is no need to set anything else because everything
           is set in the next Pin Change Interrupt */
      }
    }
    else {
      /* Disable Timer/Counter0 Output Compare Match A Interrupt */
      TIMSK&= ~_BV(OCIE0A);
    }
  }
  else { // data bit

    if (uart.flags.backward) {
      b_rcvd>>=1;

      if (RX_PIN & _BV(RX_NUM)) {
        // Sender sent a high bit
        b_rcvd|= _BV(7);
      }
      //printf("b_rcvd: %x\n", b_rcvd);
    }

    if (uart.flags.forward) {
      // Forward the data bit if forwarding is requested

      if (b_send & 0x01) TX_PORT|=  _BV(TX_NUM);
      else               TX_PORT&= ~_BV(TX_NUM);

      b_send>>=1;
    }

    if (uart.bitnum== 8 && !uart.flags.active_clock) {
      /*
       * #USBSerialConvertersSuck
       * The sender might decide to send the next
       * start bit while the receiver is still
       * waiting for the stop bit.
       * To deal with this case the pin change interrupt is
       * re-enabled when the last data bit was received.
       * It might trigger because of a transition from the
       * last bit to the stop bit but that case should be filtered
       * out by the line state check at the beginning of the interrupt
       * handler
       */

      GIMSK|= _BV(PCIE);
    }
  }

  // Shedule next bit
  uart.bitnum++;
  OCR0A= pgm_read_byte(&uart_times[uart.bitnum]);
}




void uart_init(void)
{
  uart.flags.transmission= 0;
  uart.flags.ping_rcvd= 0;
  uart.flags.first_brick= 0;
  uart.flags.aq_complete= 0;

  // Set TX pin to driven high state
  TX_PORT&= ~_BV(TX_NUM);
  TX_DDR|= _BV(TX_NUM);

  /* Pin Change Interrupt Enable */
  PCMSK|= _BV(RX_NUM);
  GIMSK|= _BV(PCIE);

}

void communicate(void)
{

  /* Buffer should be empty */
  if (rdbuf_len(&uart.buf)==0 && !uart.flags.transmission){

    /* TODO Wait for some random time */

    /*  Timer/Counter1 Overflow Interrupt Enable */
    TIMSK|= _BV(TOIE1);
    /* start ping_rcvd timer */
    TCNT1= 0;
    TCCR1= PING_TMR_PRESCALE_REG;

    /* ping */
    rdbuf_push(&uart.buf, 0x00);
    rdbuf_push(&uart.buf, 0x02);

    /* Send ping */
    OCR0A= pgm_read_byte(&uart_times[0]);
    TIMSK|= _BV(OCIE0A);
    TCNT0= 0;
    TCCR0B= UA_TMR_PRESCALE_REG;

    /* Disable Pin change interrupts while the transmission
     * is active */
    GIMSK&= ~_BV(PCIE);

    uart.flags.transmission= 1;
    uart.flags.active_clock= 1;
    uart.flags.forward= 1;
    uart.flags.backward= 0;

    uart.bitnum= 0;

  } /* else there will be no ping */
}
