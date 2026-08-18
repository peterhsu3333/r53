#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <pthread.h>
#include <ctype.h>

#include "caveat.h"
#include "hart.h"

option<>     conf_arch  ("arch", "rv64gc", "rv64gc", "RISC-V architecture and extensions");

ISA_bv_t  this_isa = 0;


option<size_t>	conf_tcache("tcache",	1000000L,		"Binary translation cache size");
option<size_t>	conf_hash  ("hash",	997L,			"Hash table size, best if prime number");
option<bool>	conf_show  ("show",	false, true,		"Show instruction trace");
option<>	conf_gdb   ("gdb",	0, "localhost:1234",	"Remote GDB connection");
option<bool>	conf_calls ("calls",	false, true,		"Show function calls and returns");






// in loader.cc
long emulate_execve(const char* filename, int argc, const char* argv[], const char* envp[], xlen_t& pc);

hart_t* hart_t::_list =0;
int hart_t::_num_harts =0;

thread_local Header_t hart_t::mismatch_header = Header_t(0, 0, 0, false);
thread_local Header_t* hart_t::mismatch_target = &mismatch_header;
thread_local Header_t** hart_t::target = &mismatch_target;







hart_t* hart_t::find(int tid)
{
  for (hart_t* p=_list; p; p=p->_next)
    if (p->tid() == tid)
      return (hart_t*)p;
  return 0;
}

void hart_t::initialize()
{
  do {  // atomically attach to list of strands
    _next = _list;
  } while (!__sync_bool_compare_and_swap(&_list, _next, this));
  sid = __sync_fetch_and_add(&_num_harts, 1);
}

hart_t::hart_t(int argc, const char* argv[], const char* envp[])
{
  // figure out architecture options
  if (strncmp(conf_arch(), "rv64", 4) == 0)
    this_isa |= ISA_64;
  else if (strncmp(conf_arch(), "rv32", 4) == 0)
    this_isa |= ISA_32;
  else
    die("arch must be either rv64 or rv32");
  for (int k=4; conf_arch()[k]; ++k) {
    switch (toupper(conf_arch()[k])) {
    case 'G':  this_isa |= ISA_G;  break;
    case 'I':  this_isa |= ISA_I;  break;
    case 'M':  this_isa |= ISA_M;  break;
    case 'A':  this_isa |= ISA_A;  break;
    case 'F':  this_isa |= ISA_F;  break;
    case 'D':  this_isa |= ISA_D;  break;
    case 'C':  this_isa |= ISA_C;  break;
    default:  die("Invalid architecture %s", conf_arch());
    }
  }
  
  memset(&s, 0, sizeof(processor_state_t));
  uintptr_t stack_pointer = emulate_execve(argv[0], argc, argv, envp, s.pc);
  s.reg[2].x = stack_pointer;
  _tid = gettid();
  ptnum = pthread_self();
  simulator = 0;		// must be filled in by deriving class
  clone = 0;			// same
  interpreter = 0;
  riscv_syscall = 0;
  initialize(); // do at end because there are atomic stuff in initialize()
}

hart_t::hart_t(hart_t* from)
{
  memcpy(&s, &from->s, sizeof(processor_state_t));
  s.pc = from->s.pc;		// not in state structure
  simulator = from->simulator;
  clone = from->clone;
  interpreter = from->interpreter;
  riscv_syscall = from->riscv_syscall;
  initialize();
}

hart_t::~hart_t()
{
}

void hart_t::print(uintptr_t pc, Insn_t* i, FILE* out)
{
  fprintf(out, "[%d] ", gettid());
  if (i->rd() == NOREG) {
    if (ATTR[i->opcode()] & ATTR_st)
      fprintf(out, "%4s[%016lx] ", reg_name[i->rs2()], s.reg[i->rs2()].x); 
    else if ((ATTR[i->opcode()] & (ATTR_cj|ATTR_uj)) && (i->rs1() != NOREG))
      fprintf(out, "%4s[%016lx] ", reg_name[i->rs1()], s.reg[i->rs1()].x); 
    else if (ATTR[i->opcode()] & ATTR_ex)
      fprintf(out, "%4s[%016lx] ", reg_name[10], s.reg[10].x);
    else
      fprintf(out, "%4s[%16s] ", "", "");
  }
  else
    fprintf(out, "%4s[%016lx] ", reg_name[i->rd()], s.reg[i->rd()].x);
  labelpc(pc, stdout);
  disasm(pc, i, "\n", stdout);
}




#define state s

xlen_t hart_t::get_csr(int which, insn_t insn, bool write, bool peek)
{
  switch (which) {
  case CSR_FFLAGS:
    return state.fflags;
  case CSR_FRM:
    return state.frm;
  case CSR_FCSR:
    return (state.fflags << FSR_AEXC_SHIFT) | (state.frm << FSR_RD_SHIFT);
  default:
    break;
  }
  die("get_csr bad number");
}

void hart_t::set_csr(int which, xlen_t val)
{
  switch (which) {
  case CSR_FFLAGS:
    state.fflags = val & (FSR_AEXC >> FSR_AEXC_SHIFT);
    break;
  case CSR_FRM:
    state.frm = val & (FSR_RD >> FSR_RD_SHIFT);
    break;
  case CSR_FCSR:
    state.fflags = (val & FSR_AEXC) >> FSR_AEXC_SHIFT;
    state.frm = (val & FSR_RD) >> FSR_RD_SHIFT;
    break;
  default:
    die("set_csr bad number");
  }
}




Tcache_t::Tcache_t()
{
  array = new Tentry_t[conf_tcache()];
  table = new uint32_t[conf_hash()];
#if 0
  array = (Tentry_t*)mmap(NULL, conf_tcache()*sizeof(Tentry_t), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  dieif(!array, "mmap of Tcache array failed");
  table = (uint32_t*)mmap(NULL, conf_hash()*sizeof(Tentry_t), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  dieif(!array, "mmap of Tcache hash table failed");
#endif
  memset((void*)array, 0, conf_tcache()*sizeof(Tentry_t));
  memset((void*)table, 0, conf_hash()*sizeof(uint32_t));
  _size = 2;
  _flushed = 0;
}

// copy basic block into cache and insert into hash table
Header_t* Tcache_t::add(Header_t* wbb, size_t n) {
  if (_size+n > conf_tcache()) {
    dieif(n>conf_tcache(), "basic block size %lu bigger than cache %lu", n, conf_tcache());
    clear();
  }
  Header_t* bb = (Header_t*)&array[_size];
  _size += n;
  for (int k=0; k<n; ++k)
    bb[k] = wbb[k];
  //memcpy(bb, wbb, n*sizeof(uint64_t));
  uint32_t h = hashfunction(bb->addr);
  bb->link = table[h];
  table[h] = index(bb);
  return bb;
}
// flush translation cache
void Tcache_t::clear() {
  memset((void*)table, 0, conf_hash()*sizeof(uint32_t));
  _size = 2;			// not necessary to zero cache
  _flushed++;
}
















void substitute_cas(uintptr_t pc, Insn_t* i3);
long proxy_syscall(long rvnum, long a0, long a1, long a2, long a3, long a4, long a5, hart_t* me);

/*
 * Find basic block starting at pc in tcache, pre-decoding instructions as needed.
 */
Header_t* hart_t::find_bb(xlen_t pc)
{
  Header_t* bb = tcache.find(pc);
  if (bb)
    return bb;
  uintptr_t dpc = pc;	// decode pc
  Insn_t buffer[140];
  Header_t* wbb = (Header_t*)buffer;
  Insn_t* j = (Insn_t*)(wbb+1) - 1; // note pre-incremented in loop
  do {
    *++j = decoder(dpc);
    // instructions with attribute '<' must be first in basic block
    if ((stop_before[j->opcode() / 64] >> (j->opcode() % 64) & 0x1L) && (j > insnp(wbb+1))) {
      --j;		// remove ourself for next time
      break;
    }
    dpc += j->compressed() ? 2 : 4;
    // instructions with attribute '>' ends block
    if (stop_after[j->opcode() / 64] >> (j->opcode() % 64) & 0x1L)
      break;
  } while (dpc-pc < 256 && j-insnp(wbb) < 128);
	
  // pattern match store conditional and replace with compare-and-swap
  if (j->opcode()==Op_sc_w || j->opcode()==Op_sc_d)
    substitute_cas(dpc-4, j);
  *wbb = Header_t(pc, dpc-pc, j+1-insnp(wbb+1), (ATTR[j->opcode()] & ATTR_cj)!=0);
  //
  // Always end with one pointer to next basic block
  // Conditional branches have second fall-thru pointer
  //
  *(Header_t**)(++j) = &mismatch_header; // space for next bb pointer
  if (wbb->conditional)
    *(Header_t**)(++j) = &mismatch_header; // space for taken branch pointer
	  
  // atomically add block into tcache
  long n = j - insnp(wbb) + 1;
  bb = tcache.add(wbb, n);
  return bb;
}







/*
 * Interprete one basic block, returning whether ended in taken branch.
 * Argument pointer ap should be array reg_t[256] for worse-case if
 * all loads each having both address and data.
 */
bool hart_t::run_basic_block(reg_t* ap)
{
  Header_t* bb = ((*target)->addr == s.pc) ? *target : find_bb(s.pc);
  *target = bb;
  //
  // execute basic block except last instruction
  //
  const Insn_t* i = insnp(bb+1);
  while (i<insnp(bb+1)+bb->count-1) {
    if (execute_instruction(*i, ap))
      die("unexpected branch!");
    ++i, ++ap;    
  }
  // last instruction
  if (execute_instruction(*i, ap)) { // taken branch
    target = (Header_t**)(i+1);
    return true;
  }
  else if (bb->conditional)	// conditional branch not taken
    target = (Header_t**)(i+2);
  else				// last instruction was not branch
    target = (Header_t**)(i+1);
  return false;
}




long default_riscv_syscall(hart_t* h, long a0)
{
  long a1 = h->s.reg[11].x;
  long a2 = h->s.reg[12].x;
  long a3 = h->s.reg[13].x;
  long a4 = h->s.reg[14].x;
  long a5 = h->s.reg[15].x;
  long rvnum = h->s.reg[17].x;
  long rv = proxy_syscall(rvnum, a0, a1, a2, a3, a4, a5, h);
  return rv;
}

  
void substitute_cas(uintptr_t pc, Insn_t* i3)
{
  dieif(i3->opcode()!=Op_sc_w && i3->opcode()!=Op_sc_d, "0x%lx no SC found in substitute_cas()", pc);

  Insn_t i2 = decoder(pc-4);
  if (i2.opcode() != Op_bne)
    i2 = decoder(pc-2);
  dieif(i2.opcode()!=Op_bne && i2.opcode()!=Op_c_bnez, "0x%lx instruction before SC not bne/bnez", pc);

  Insn_t i1 = decoder(pc-4 - (i2.compressed() ? 2 : 4));
  dieif(i1.opcode()!=Op_lr_w && i1.opcode()!=Op_lr_d, "0x%lx substitute_cas called without LR", pc);
  
  // pattern found, check registers
  int load_reg = i1.rd();
  int addr_reg = i3->rs1();
  int test_reg = (i2.opcode() == Op_c_bnez) ? 0 : i2.rs2();
  int newv_reg = i3->rs2();
  int flag_reg = i3->rd();
  dieif(i1.rs1()!=addr_reg || i2.rs1()!=load_reg, "0x%lx CAS pattern incorrect registers", pc);
  
  // pattern is good
  // note rd, rs1, rs2 stay the same
  i3->op_code = (i3->opcode()==Op_sc_w) ? Op_cas_w : Op_cas_d;
  i3->op.rs3 = test_reg;
}
