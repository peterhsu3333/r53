/*
  Copyright (c) 2023 Peter Hsu.  All Rights Reserved.  See LICENCE file for details.
*/
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "caveat.h"
#include "hart.h"

inline bool match(uint32_t b, uint32_t mask, uint32_t code, Opcode_t op)
{
  return (b & mask) == code && (ISA[op] & this_isa) == ISA[op];
}

#include "decoder.h"

  fprintf(stderr, "Illegal instruction pc=%lx, %08x\n", pc, *(unsigned*)pc);
  i.op_code = Op_ILLEGAL;
  return i;
}
