uint8_t prep_AQ(struct rdbuf_t *buf)
{

  /* find brick container */
  uint16_t brick_cont_index = 0;
  bool found_container = false;

  while(!found_container){
    if(eeprom_read_word(brick_cont_index)==BRICK_CONT){ // reads 16bit, returns uint16_t
      found_container = true;
    }
    else {
      brick_cont_index += EEPROM_HDR_LEN + eeprom_read_word(brick_cont_index+2);

      /* The Attiny85 contains 512bytes of EEPROM */
      if(brick_cont_index>EEPROM_SPACE -5){
        /* BRICK_CONT not found */
        return (-1);
      }
    }
  }


  uint16_t brick_cont_len = eeprom_read_word(brick_cont_index+2);
  /* First we need to find out how long the brick descriptor bytecode is
   * to reserve the needed space in the rdbuffer. */
  uint8_t bytecode_len = 0;

  uint8_t last_byte = eeprom_read_byte(brick_cont_index + EEPROM_HDR_LEN);
  uint8_t cur_byte;
  for(uint16_t i = brick_cont_index + EEPROM_HDR_LEN + 1; i < brick_cont_index + EEPROM_HDR_LEN + brick_cont_len;i++){

    cur_byte = eeprom_read_byte(i);

    /* Scan for BRICK_PREP */
    if((uint16_t)(last_byte << 8) | (uint16_t)(cur_byte) == BRICK_PREP){

      i += eeprom_read_word(i+1) + 2;

      last_byte = eeprom_read_byte(i);
    }
    else {
     bytecode_len++;
     last_byte = cur_byte;
    }
  }
  /* Fix because the last byte was skipped */
  if(bytecode_len>0) bytecode_len++;

  rdbuf_reserve(buf, bytecode_len);


  /* Everything in payload, that isnt BRICK_PREP, is brick descriptor bytecode
   * and belongs in the roundbuffer */
  last_byte = eeprom_read_byte(brick_cont_index + EEPROM_HDR_LEN);
  for(uint16_t i = brick_cont_index + EEPROM_HDR_LEN + 1; i< brick_cont_index + EEPROM_HDR_LEN + brick_cont_len;i++){

    cur_byte = eeprom_read_byte(i);

    /* Scan for BRICK_PREP */
    if((uint16_t)(last_byte << 8) | (uint16_t)(cur_byte) == BRICK_PREP){

      i += eeprom_read_word(i+1) + 2;

      last_byte = eeprom_read_byte(i);
    }
    else {
      rdbuf_put_resv(buf, i - brick_cont_index - EEPROM_HDR_LEN - 1, (char)last_byte);
      last_byte = cur_byte;
    }
  }

   /* Handle BRICK_PREP */
   last_byte = eeprom_read_byte(brick_cont_index + EEPROM_HDR_LEN);
   for(uint16_t i = brick_cont_index + EEPROM_HDR_LEN + 1; i< brick_cont_index + EEPROM_HDR_LEN + brick_cont_len;i++){

    cur_byte = eeprom_read_byte(i);

    /* Scan for BRICK_PREP */
    if((uint16_t)(last_byte << 8) | (uint16_t)(cur_byte) == BRICK_PREP){

      uint16_t brick_prep_len = eeprom_read_word(i+1);
      uint16_t brick_prep_para = eeprom_read_word(i+3);

      /* addresses */
      //TODO
      for(i+=5;i<brick_prep_len;i+=2){
        //rdbuf_put_resv(buf, i - brick_cont_index - EEPROM_HDR_LEN - 1, (char)last_byte);
      }
    }
   }


  rdbuf_finish_resv(buf);
  return (0);
}
