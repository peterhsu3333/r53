/*
  Copyright (c) 2023 Peter Hsu.  All Rights Reserved.  See LICENCE file for details.
*/
#include <limits.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <sys/mman.h>
#include <signal.h>

#include "caveat.h"
#include "hart.h"
#include "arithmetic.h"

bool hart_t::execute_instruction(Insn_t insn, reg_t* ap)
{
  _executed++;
  s.reg[0].x = 0;
#if 0
  labelpc(s.pc);
  disasm(s.pc, &insn);
#endif
  /*
    Abbreviations to keep isa.def semantics short
  */

#define imm	          insn.immed()
#define uimm	(unsigned)insn.immed()
#define wrd(e)	(*ap++)=s.reg[insn.rd()].x=(e)
#define r1	s.reg[insn.rs1()].x
#define r2	s.reg[insn.rs2()].x
#define r3	s.reg[insn.rs3()].x
#define wud(e)	(*ap++)=s.reg[insn.rd()].u=(e)
#define u1	s.reg[insn.rs1()].u
#define u2	s.reg[insn.rs2()].u
#define wfd(e)	(*ap++)=s.reg[insn.rd()].f=(e)
#define f1	s.reg[insn.rs1()].f
#define f2	s.reg[insn.rs2()].f
#define f3	s.reg[insn.rs3()].f
#define wdd(e)	(*ap++)=s.reg[insn.rd()].d=(e)
#define d1	s.reg[insn.rs1()].d
#define d2	s.reg[insn.rs2()].d
#define d3	s.reg[insn.rs3()].d
	
#define LOAD(T, a)     *(T*)(*ap++=a)
#define STORE(T, a, v) *(T*)(*ap++=a)=(v)

#define load_reserved(T, a)         *(T*)(*ap++=a)
#define store_conditional(T, a, v)  wrd( (*(T*)(*ap++=a)=(v), 0) )

#define cas32(a, b, c, d) cas<int32_t>(a, b, c, d)
#define cas64(a, b, c, d) cas<int64_t>(a, b, c, d)
      
#define fence(x)
#define fence_i(x)
      
    //#define ebreak() return true
#define ebreak() kill(tid(), SIGTRAP)

#define branch(test, taken, fall)  { s.pc=(test)?(taken):(fall); return (test); }
#define jump(npc)  { s.pc=(npc); return true; }
#define reg_jump(npc)  { s.pc=(npc); return true; }
    
  switch (insn.opcode()) {
  case Op_ZERO:	die("Should never see Op_ZERO at pc=%lx", s.pc);
#include "semantics.h"
  case Op_ILLEGAL:  die("Op_ILLEGAL opcode, i=%08x, pc=%lx", *(unsigned*)s.pc, s.pc);
  case Op_UNKNOWN:  die("Op_UNKNOWN opcode, i=%08x, pc=%lx", *(unsigned*)s.pc, s.pc);
  default:  die("undefined opcode, i=%08x, pc=%lx", *(unsigned*)s.pc, s.pc);
  }
  return false;
}
