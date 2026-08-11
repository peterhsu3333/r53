#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <pthread.h>
#include <ctype.h>

#include "caveat.h"
#include "hart.h"

extern "C" {
#include "specialize.h"
#include "internals.h"
};

option<>     conf_arch  ("arch", "rv64gc", "rv64gc", "RISC-V architecture and extensions");

ISA_bv_t  this_isa = 0;


option<size_t>	conf_tcache("tcache",	1000000L,		"Binary translation cache size");
option<size_t>	conf_hash  ("hash",	997L,			"Hash table size, best if prime number");
option<bool>	conf_show  ("show",	false, true,		"Show instruction trace");
option<>	conf_gdb   ("gdb",	0, "localhost:1234",	"Remote GDB connection");
option<bool>	conf_calls ("calls",	false, true,		"Show function calls and returns");

// in loader.cc
long emulate_execve(const char* filename, int argc, const char* argv[], const char* envp[], uintptr_t& pc);

hart_t* hart_t::_list =0;
int hart_t::_num_harts =0;

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
  uintptr_t stack_pointer = emulate_execve(argv[0], argc, argv, envp, pc);
  s.xrf[2] = stack_pointer;
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
  pc = from->pc;		// not in state structure
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
      fprintf(out, "%4s[%016lx] ", reg_name[i->rs2()], s.xrf[i->rs2()]); 
    else if ((ATTR[i->opcode()] & (ATTR_cj|ATTR_uj)) && (i->rs1() != NOREG))
      fprintf(out, "%4s[%016lx] ", reg_name[i->rs1()], s.xrf[i->rs1()]); 
    else if (ATTR[i->opcode()] & ATTR_ex)
      fprintf(out, "%4s[%016lx] ", reg_name[10], s.xrf[10]);
    else
      fprintf(out, "%4s[%16s] ", "", "");
  }
  else
    fprintf(out, "%4s[%016lx] ", reg_name[i->rd()], s.xrf[i->rd()]);
  labelpc(pc, stdout);
  disasm(pc, i, "\n", stdout);
}




#define state s

reg_t hart_t::get_csr(int which, insn_t insn, bool write, bool peek)
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

void hart_t::set_csr(int which, reg_t val)
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

