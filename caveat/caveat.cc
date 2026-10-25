/*
  Copyright (c) 2021 Peter Hsu.  All Rights Reserved.  See LICENCE file for details.
*/
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/mman.h>

#include "caveat.h"
#include "hart.h"

option<long> conf_report("report", 0, "Status report every n million instructions");

void status_report()
{
  double realtime = elapse_time();
  long long total = 0;
  long flushed = 0;
  for (hart_t* p=hart_t::list(); p; p=p->next()) {
    total += p->executed();
    flushed += p->flushed();
  }
  fprintf(stderr, "\r\33[2K%12lld insns %3.1fs %3.1f MIPS (%ld T-cache flushes)",
	  total, realtime, total/1e6/realtime, flushed);
}

int my_clone_proxy(class hart_t* parent)
{
  hart_t* child = new hart_t(parent);
  return clone_thread(child);
}

void my_interpreter(hart_t* h)
{
  if (conf_report() > 0) {
    long report_epoch = conf_report() * 1000000;
    //long report_epoch = conf_report() * 1000;
    long next_report = report_epoch;
    do {
      while (h->executed() < next_report)
	h->run_basic_block();
      status_report();
      next_report += report_epoch;
    } while (1);
  }
  else {
    while (1) {
      h->run_basic_block();
    }
  }
}

static jmp_buf return_to_top_level;

static void segv_handler(int, siginfo_t*, void*) {
  longjmp(return_to_top_level, 1);
}

  
  
hart_t* mycpu;
thread_local long cycle = 0;

#ifdef DEBUG
void signal_handler(int nSIGnum, siginfo_t* si, void* vcontext)
{
  //  ucontext_t* context = (ucontext_t*)vcontext;
  //  context->uc_mcontext.gregs[]
  fprintf(stderr, "\n\nsignal_handler, signum=%d, tid=%d\n", nSIGnum, gettid());
  hart_t* thisCPU = hart_t::find(gettid());
  longjmp(return_to_top_level, 1);
  exit(-1);
}
#endif

void exitfunc()
{
  fprintf(stderr, "\nNormal exit\n");
  status_report();
  fprintf(stderr, "\n");
}

int main(int argc, const char* argv[], const char* envp[])
{
  parse_options(argc, argv, "caveat: user-mode RISC-V interpreter");
  if (argc == 0)
    help_exit();
  
  // before creating harts
  mycpu = new hart_t(argc, argv, envp);
  mycpu->simulator = 0;
  mycpu->clone = my_clone_proxy;
  mycpu->riscv_syscall = default_riscv_syscall;
  mycpu->interpreter = my_interpreter;
  start_time();

#ifdef DEBUG
  if (!conf_gdb()) {
    static struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    sigemptyset(&action.sa_mask);
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    //    action.sa_sigaction = segv_handler;
    action.sa_sigaction = signal_handler;
    sigaction(SIGSEGV, &action, NULL);
    sigaction(SIGABRT, &action, NULL);
    sigaction(SIGINT,  &action, NULL);
    if (setjmp(return_to_top_level) != 0) {
      fprintf(stderr, "SIGSEGV signal was caught\n");
      fprintf(stderr, "%ld instructions executed, pc=0x%lx\n", mycpu->executed(), mycpu->s.pc);
      if (conf_show())
	exit(0);
#ifdef DEBUG
      int now = mycpu->executed() % DEBUG_RING;
      int k = now;
      do {
	Debug_t* d = &mycpu->debug[k];
	prtinsn(d->pc, &d->insn, d->result);
	k = (k+1) % DEBUG_RING;
      } while (k != now);
#endif
      exit(0);
    }
  }
#endif

  if (conf_gdb())
    controlled_by_gdb(conf_gdb(), mycpu);
  else if (conf_show()) {
    while (1) {
      Insn_t insn = decoder(mycpu->s.pc);
      Result_t result;
      Addr_t oldpc = mycpu->s.pc;
      mycpu->execute_instruction(insn, &result);
      prtinsn(oldpc, &insn, result);
    }
  }
  else {
    atexit(exitfunc);
    my_interpreter(mycpu);
  }
}
