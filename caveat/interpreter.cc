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

/*
  int vl = insn.immed()&0x1f;
  if (s.reg[rs1()].x < vl)
    vl = s.reg[rs1()].x;
  int rdv = last_insn.rd();
  int rs1v = last_insn.rs1();
  int rs2v = last_insn.rs2();
  int rs3v = last_insn.rs3();
  unsigned incmask = insn.immed()>>5);
  for (int k=1; k<vl; ++k) {
    if (incmask & (1<<0)) ++rdv;
    if (incmask & (1<<1)) ++rs1v;
    if (incmask & (1<<2)) ++rs2v;
    if (incmask & (1<<3)) ++rs3v;
*/  

bool hart_t::execute_instruction(Insn_t insn, reg_t* ap)
{
  _executed++;
  s.reg[0].x = 0;
  //fprintf(stderr, "%lx ", s.pc);
#if 0
  labelpc(s.pc);
  disasm(s.pc, &insn);
#endif
  /*
    Abbreviations to keep isa.def semantics short
  */
	
#define LOAD(T, a)     *(T*)(*ap++=a)
#define STORE(T, a, v) *(T*)(*ap++=a)=(v)

#define imm	         insn.immed()
#define uimm	(uxlen_t)insn.immed()
#define wrd(e)	(*ap++)=s.reg[insn.rd()].x=(e)
#define r1	s.reg[insn.rs1()].x
#define r2	s.reg[insn.rs2()].x
#define r3	s.reg[insn.rs3()].x
#define wud(e)	(*ap++)=s.reg[insn.rd()].u=(e)
#define u1	s.reg[insn.rs1()].u
#define u2	s.reg[insn.rs2()].u
#define wfd(e)	(*ap++)=s.reg[insn.rd()].raw=((uint64_t)-1<<32)|(e.v)
#define f1	s.reg[insn.rs1()].f
#define f2	s.reg[insn.rs2()].f
#define f3	s.reg[insn.rs3()].f
#define wdd(e)	(*ap++)=s.reg[insn.rd()].raw=(e.v)
#define d1	s.reg[insn.rs1()].d
#define d2	s.reg[insn.rs2()].d
#define d3	s.reg[insn.rs3()].d

#define load_reserved(T, a)         *(T*)(*ap++=a)
#define store_conditional(T, a, v)  wrd( (*(T*)(*ap++=a)=(v), 0) )

#define cas32(a, b, c, d) cas<int32_t>(a, b, c, d)
#define cas64(a, b, c, d) cas<int64_t>(a, b, c, d)
      
#define fence(x)
#define fence_i(x)
      
    //#define ebreak() return true
#define ebreak() kill(tid(), SIGTRAP)

  //#define do_return(jumped) fprintf(stderr, "%lx\n", s.reg[insn.rd()].x); return (jumped);
#define do_return(jumped) return (jumped);

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
