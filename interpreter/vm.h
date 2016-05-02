#pragma once

#include <avr/io.h>
#include <avr/pgmspace.h>

#define VM_ERR 0
#define VM_OK 1

#define VM_JMP_BWD 0
#define VM_JMP_FWD 1


struct vm_status_t {
  uint8_t regs[3];
  uint8_t mem[3];

  uint16_t pc;

  uint16_t prog_len;
  uint8_t *prog;
};

extern struct vm_status_t vm_status;

inline uint8_t vm_verify_jmp (uint8_t dir, uint16_t len)
{
  if (dir == VM_JMP_FWD) {
    return ((vm_status.prog_len - vm_status.pc) > len ? VM_OK : VM_ERR);
  }
  else {
    return (vm_status.pc >= len ? VM_OK : VM_ERR);
  }    
}

inline uint8_t vm_do_jmp (uint8_t dir, uint16_t len)
{
  if (vm_verify_jmp(dir, len) == VM_OK) {
    if (dir == VM_JMP_BWD) {
      vm_status.pc -= len;
    }
    else {
      vm_status.pc += len;
    }

    return (VM_OK);
  }
  else {
    return (VM_ERR);
  }
}

inline uint8_t vm_next_op (void)
{
  // Should there be a check here ?
  // How would it indicate an error ?
  
  return (vm_status.prog[++vm_status.pc]);
}
