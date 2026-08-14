

#include "caveat.h"
#include "hart.h"
#include "display.h"

long long cycle = 0;		// current time
int insn_issued=  0;


option<int> conf_iu ("iu",  1, "Integer unit latency");
option<int> conf_fpu("fpu", 3, "Floating point unit latency");
option<int> conf_mem("mem", 4, "Memory unit latency");

option<int> conf_history("history", 1000, "Cycles remembered for display");


core_t::core_t(int argc, const char* argv[], const char* envp[])
  : hart_t(argc, argv, envp)
{
  busy = 0LL;
  iu   = new pipeline_t("IU",  conf_iu());
  fpu  = new pipeline_t("FPU", conf_fpu());
  mem  = new pipeline_t("MEM", conf_mem());
  history_length = conf_history();
  history = new history_t[history_length];
  memset(history, 0, history_length*sizeof(history_t));
}


int makelabel()
{
  static long counter = 0;
  int label = counter++ % (2*26);
  return label < 26 ? label+'A' : label+'a';
}

bool core_t::issue(history_t* h)
{
  // "fetch" instruction
  Insn_t insn = decoder(pc);
  
  // check for busy registers
  uint64_t regs = 1LL << insn.op_rd | 1LL << insn.op_rs1;
  if (! insn.longimmed())
    regs |= 1LL << insn.op.rs2 | 1LL << insn.op.rs3;
  regs &= ~1LL;			// ignore NOREG==0
  //if (regs & busy)
  //  return false;

  // assign functional unit
  ATTR_bv_t attr = ATTR[insn.opcode()];
  pipeline_t* unit;
  if (attr == 0)		// integer operation most common
    unit = iu;
  else if (attr & ATTR_fp)
    unit = fpu;
  else if (attr & (ATTR_ld | ATTR_st | ATTR_rmw))
    unit = mem;
  else				// everything else goes to integer unit
    unit = iu;
  int latency = unit->depth;

  // enter into appropriate pipeline
  unit->pipe[cycle % unit->depth] = h;
  unit->countdown[cycle % unit->depth] = latency;

  // issue instruction, immediate execution in simulator
  h->cycle = cycle;
  h->label = makelabel();
  single_step(h->pc, h->insn, h->val);
  busy |= 1LL << insn.op_rd;	// mark output register busy
  
  return true;
}

void core_t::clock_pipeline(pipeline_t* unit)
{
  for (int k=0; k<unit->depth; ++k) {
    if (unit->pipe[k] == 0)
      continue;
    if (--unit->countdown[k] == 0) {
      busy &= ~(1LL << unit->pipe[k]->insn.op_rd);
      unit->pipe[k] = 0;	// indicate unused
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
