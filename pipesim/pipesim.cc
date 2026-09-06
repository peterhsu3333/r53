
#include <ncurses.h>

#include "caveat.h"
#include "hart.h"
#include "pipesim.h"

long long cycle = 0;		// current time


option<int> conf_iu ("iu",  3, "Integer unit latency");
option<int> conf_fpu("fpu", 1, "Floating point unit latency");
option<int> conf_mem("mem", 10, "Memory unit latency");

option<int> conf_history("history", 1000, "Cycles remembered for display");


core_t::core_t(int argc, const char* argv[], const char* envp[])
  : hart_t(argc, argv, envp),
    iu("iu", conf_iu()),
    fpu("fpu", conf_fpu()),
    mem("mem", conf_mem())
{
  busy = 0LL;
  history = new history_t[HISTORY];
}


int makelabel()
{
  static long counter = 0;
  int label = counter++ % (2*26);
  return label<26 ? label+'A' : label-26+'a';
}

bool core_t::issue(history_t* h)
{
  // "fetch" instruction
  Insn_t insn = decoder(s.pc);
  
  // check for busy registers
  uint64_t regs = 1LL << insn.op_rd | 1LL << insn.op_rs1;
  if (! insn.longimmed())
    regs |= 1LL << insn.op.rs2 | 1LL << insn.op.rs3;
  regs &= ~1LL;			// ignore NOREG==0
  if (regs & busy)
    return false;

  // assign functional unit
  ATTR_bv_t attr = ATTR[insn.opcode()];
  pipeline_t* unit;
  if (attr == 0)		// integer operation most common
    unit = &iu;
  else if (attr & ATTR_fp)
    unit = &fpu;
  else if (attr & (ATTR_ld | ATTR_st | ATTR_rmw))
    unit = &mem;
  else				// everything else goes to integer unit
    unit = &iu;
  int latency = unit->depth;

  // enter into appropriate pipeline
  unit->stage[cycle % max_pipe_depth] = h;
  unit->countdown[cycle % max_pipe_depth] = latency;

  // issue instruction, immediate execution in simulator
  h->cycle = cycle - 1;		// note!
  h->label = makelabel();
  h->pc = s.pc;
  h->insn = insn;
  reg_t values[2];
  execute_instruction(insn, values);
  h->val = values[0].x;
  
  busy |= 1LL << insn.op_rd;	// mark output register busy
  busy &= ~1LL;			// but x0 always not busy
  return true;
}

void core_t::clock_pipeline(pipeline_t* unit)
{
  for (int k=0; k<unit->depth; ++k) {
    if (unit->stage[unit->index(k)] == 0)
      continue;
    if (--unit->countdown[unit->index(k)] == 0) {
      busy &= ~(1LL << unit->stage[unit->index(k)]->insn.op_rd);
      unit->stage[unit->index(k)] = 0;	// indicate unused
    }
  }
}

int main(int argc, const char* argv[], const char* envp[])
{
  parse_options(argc, argv, "pipesim: single issue in-order");
  if (argc == 0)
    help_exit();

  core_t* cpu = new core_t(argc, argv, envp);
  cpu->simulator = 0;
  cpu->riscv_syscall = default_riscv_syscall;
  interactive(cpu);
}
