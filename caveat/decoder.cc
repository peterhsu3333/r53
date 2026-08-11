/*
  Copyright (c) 2023 Peter Hsu.  All Rights Reserved.  See LICENCE file for details.
*/
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "caveat.h"

inline bool match(uint32_t b, uint32_t mask, uint32_t code, Opcode_t op, Insn_t& i, int rd, int rs1, int rs2, int rs3, int immed) {
  if ((b & mask) == code && (ISA[op] & this_isa) == ISA[op]) {
    i.op_code = op;
    i.op_rd = rd;
    i.op_rs1 = rs1;
    i.op.rs2 = rs2;
    i.op.rs3 = rs3;
    i.setimm(immed);
    return true;
  }
  return false;
}

inline bool match(uint32_t b, uint32_t mask, uint32_t code, Opcode_t op, Insn_t& i, int rd, int rs1, int immed) {
  if ((b & mask) == code && (ISA[op] & this_isa) == ISA[op]) {
    i.op_code = op;
    i.op_rd = rd;
    i.op_rs1 = rs1;
    i.op_longimm = immed;
    return true;
  }
  return false;
}


Insn_t decoder(uintptr_t pc)
{
  int32_t b = *(int32_t*)pc;
  Insn_t i;
  
#define x( lo, len) ((b >> lo) & ((1 << len)-1))
#define xs(lo, len) (b << (32-lo-len) >> (32-len))
  
#include "decoder.h"

  fprintf(stderr, "Illegal instruction pc=%lx, %08x\n", pc, *(unsigned*)pc);
  i.op_code = Op_ILLEGAL;
  return i;
}
