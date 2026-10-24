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


bool hart_t::execute_instruction(Insn_t insn, Result_t* ap)
{
  uxlen_t tmp;
  _executed++;
  s.reg[0].l = 0;
#if 0
  //fprintf(stderr, "%lx ", s.pc);
  labelpc(s.pc);
  disasm(s.pc, &insn);
#endif
  /*
    Abbreviations to keep isa.def semantics short
  */
	
#define LOAD(T, a)     *(T*)(ap->addr=a)
#define STORE(T, a, v) *(T*)(ap->addr=a)=(v)

#define sp		s.reg[2].l
  //#define rreg(n)		s.reg[n].l
#define wpc(e)		s.pc=(e)

#define wreg(n, e)	ap->data=s.reg[n]=(e)
#define wrd(e)		wreg(insn.rd(), (e))
#define wfd(e)		wreg(insn.rd(), (e))
  
#define imm	         insn.immed()
#define uimm	(uxlen_t)insn.immed()
  
#define load_reserved(T, a)         *(T*)(ap->addr=a)
#define store_conditional(T, a, v)  *(T*)(ap->addr=a)=(v), 0

#define cas32(a, b, c, d) cas<int32_t>(a, b, c, d)
#define cas64(a, b, c, d) cas<int64_t>(a, b, c, d)
      
#define fence(x)
#define fence_i(x)
      
    //#define ebreak() return true
#define ebreak() kill(tid(), SIGTRAP)

#define do_return(jumped) return (jumped);
  //#define do_return(jumped) fprintf(stderr, "%lx\n", s.reg[insn.rd()].ul); return (jumped);

#define branch(test, taken, fall)  { s.pc=(test)?(taken):(fall); do_return(test); }
#define jump(npc)  { s.pc=(npc); do_return(true); }
#define reg_jump(npc)  { s.pc=(npc); do_return(true); }

#define dorepeat(x, y) { fprintf(stderr, "repeat(%s, maxvl=%ld, regincs=0x%lx)\n", reg_name[insn.rs1()], insn.immed()&0x1f, insn.immed()>>5); exit(0); }
    
  switch (insn.opcode()) {
  case Op_ZERO:	die("Should never see Op_ZERO at pc=%lx", s.pc);
#include "semantics.h"
  case Op_ILLEGAL:  die("Op_ILLEGAL opcode, i=%08x, pc=%lx", *(unsigned*)s.pc, s.pc);
  case Op_UNKNOWN:  die("Op_UNKNOWN opcode, i=%08x, pc=%lx", *(unsigned*)s.pc, s.pc);
  default:  die("undefined opcode, i=%08x, pc=%lx", *(unsigned*)s.pc, s.pc);
  }
  do_return(false);
}
