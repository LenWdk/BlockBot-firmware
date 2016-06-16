#ifndef __UNIT_TEST__
  // Do not load avr headers in unit test code
  #include <avr/io.h>
  #include <avr/interrupt.h>
  #include <avr/pgmspace.h>
  #include <rdbuf.h>


  #include <rdbuf.h>
  #include "uart.h"
  #include "pkt_lib.h"
#endif

#define BRICK_CONT 0x0100

#define AQ_HDR_LEN 6 /* Mnemonic, payload_length, CKSUM */
#define EEPROM_HDR_LEN 4 /* Mnemonic, payload_length */
#define EEPROM_SPACE 512 /* bytes */

struct {
  uint16_t index;
  uint16_t len; /* without BRICK_PREP */
  uint16_t total_len;
} brick_cont;

int main (void)
{

  init_brick_cont();
  uart_init();
  sei();

  for(;;){

    /* AQ received */
    if(uart.flags.forward==1
        && uart.pkt_header_rcvd[0]==0x0
          && uart.pkt_header_rcvd[1]==0x1){

      if(rdbuf_len(&uart.buf)==0){

        make_aq();
      } /* else finishing transmission */
      /* When the buffer ran empty immediatelly uart.flags.forward==0,
         therefore this shouldnt lead to an unwanted second iteration */
    }
  }
  return (0);
}

uint8_t init_brick_cont(){

  /* Get BRICK_CONT index */
  int16_t signed_brick_cont_index =
             nth_pkt_by_type(BRICK_CONT, 0, EEPROM_SPACE, 1);
  /* Is probably 0 */

  /* Check if valid */
  if(signed_brick_cont_index>0){
    brick_cont.index = (uint16_t)signed_brick_cont_index;
  } else {
    return (-1); /* No BRICK_CONT found */
  }

  brick_cont.total_len = (uint16_t)(eeprom_read_byte(brick_cont.index+2) << 8)
                          | (uint16_t)(eeprom_read_byte(brick_cont.index+3);

  /* Get BRICK_CONT length */
  int16_t signed_brick_cont_payload_len =
              brick_cont_len_without_prep(brick_cont_index);

  /* if(signed_brick_cont_payload_len>0){ */
    brick_cont.len =
            (uint16_t)signed_brick_cont_payload_len;
  /* }  else { Impossible Error } */

  return (0);
}

uint8_t make_aq(){

  /* Reserve the precalculated len */
  rdbuf_reserve(&uart.buf,
            AQ_HDR_LEN + EEPROM_HDR_LEN + brick_cont.len);
        /*                BRICK_CONT_HDR                   */


  /* init */
  uint16_t pkt_index = brick_cont.index + EEPROM_HDR_LEN;
  uint16_t brick_word = (uint16_t)eeprom_read_byte(pkt_index);
  uint16_t resv_index = AQ_HDR_LEN + EEPROM_HDR_LEN;


  /* push everything that isnt BRICK_PREP */
  for(pkt_index+= 1;
        pkt_index < brick_cont.index + EEPROM_HDR_LEN +  brick_cont.total_len;
        pkt_index++){

    shift(&brick_word, eeprom_read_byte(pkt_index));

    if(brick_word==BRICK_PREP){

      /* Skip the paket
       *     payload_length     HDR_restlenght */
      pkt_index += (uint16_t)(eeprom_read_byte(pkt_index+1) << 8) +
                     (uint16_t)(eeprom_read_byte(pkt_index+2)) + 3;

      /* init brick_word for next loop */
      brick_word = (uint16_t)eeprom_read_byte(pkt_index);;
    }
    else {
      /* If not BRICK_PREP, put byte in buffer */
      rdbuf_put_resv(&uart.buf, resv_index, (uint8_t)(brick_word >> 8));
      resv_index++;
    }
  }


  /* init */
  pkt_index = brick_cont.index + EEPROM_HDR_LEN;
  brick_word = (uint16_t)eeprom_read_byte(pkt_index);

  /* run BRICK_PREP */
  for(pkt_index+=1;
         pkt_index < brick_cont.index + EEPROM_HDR_LEN + brick_cont.total_len;
         pkt_index++){

    shift(&brick_word, eeprom_read_byte(pkt_index));

    if(brick_word == BRICK_PREP){
      //rdbuf_put_resv(&uart.buf, pos, val);
      /* TODO Leonard */


      /* Skip the paket
       *     payload_length     HDR_restlenght */
       pkt_index += (uint16_t)(eeprom_read_byte(pkt_index+1) << 8) +
                      (uint16_t)(eeprom_read_byte(pkt_index+2)) + 3;

      /* init brick_word for next loop */
      brick_word = (uint16_t)eeprom_read_byte(pkt_index);;
    }
  }


  /* Create AQ header
   * As the length of the receiving paket must be known first
   * this happens at last */
  rdbuf_put_resv(&uart.buf, 0, 0x0); /* AQ1 */
  rdbuf_put_resv(&uart.buf, 1, 0x1); /* AQ2 */
  uint16_t aq_len = brick_cont.len +
  (uint16_t)(uart.pkt_header_rcvd[2] << 8) | (uint16_t)(uart.pkt_header_rcvd[3]); /* CKSUM in pkt_header_rcvd */
  rdbuf_put_resv(&uart.buf, 2, (uint8_t)(8 >> aq_len));
  rdbuf_put_resv(&uart.buf, 3, (uint8_t)(aq_len&0xFF));

  /* TODO CKSUM */
  rdbuf_put_resv(&uart.buf, 4, 0x0);
  rdbuf_put_resv(&uart.buf, 5, 0x0);

  rdbuf_fin_resv(&uart.buf);
}
