/*
  Copyright (c) 2021 Peter Hsu.  All Rights Reserved.  See LICENCE file for details.
*/
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <pthread.h>
#include <sys/mman.h>

#include "caveat.h"
#include "hart.h"

option<long> conf_report("report", 1, "Status report per second");


void status_report()
{
  //  static long n = 1;
  //  fprintf(stderr, "\r%12ld", n++);
  //  return;
  
  double realtime = elapse_time();
  long total = 0;
  long flushed = 0;
  for (hart_t* p=hart_t::list(); p; p=p->next()) {
    total += p->executed();
    flushed += p->flushed();
  }
  //fprintf(stderr, "\r\33[2K%12ld insns ", total);
  return;
  
  static double last_time;
  static long last_total;
  fprintf(stderr, "\r\33[2K%12ld insns %3.1fs(%ld) MIPS(%3.1f,%3.1f) ", total, realtime, flushed,
	  (total-last_total)/1e6/(realtime-last_time), total/1e6/realtime);
  last_time = realtime;
  last_total = total;
  if (hart_t::num_harts() <= 16 && total > 0) {
    char separator = '(';
    for (hart_t* p=hart_t::list(); p; p=p->next()) {
      fprintf(stderr, "%c", separator);
      fprintf(stderr, "%1ld%%", 100*p->executed()/total);
      separator = ',';
    }
    fprintf(stderr, ")");
  }
  else if (hart_t::num_harts() > 1)
    fprintf(stderr, "(%d cores)", hart_t::num_harts());
}

int my_clone_proxy(class hart_t* parent)
{
  hart_t* child = new hart_t(parent);
  return clone_thread(child);
}

void my_interpreter(hart_t* h)
{
  reg_t values[256];
  while (1)
    h->run_basic_block(values);
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
  exit(-1);
  //  longjmp(return_to_top_level, 1);
}
#endif

void* status_thread(void* arg)
{
  while (1) {
    usleep(1000000/conf_report());
    status_report();
  }
}

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
      exit(-1);
    }
  }
#endif

  if (conf_gdb())
    controlled_by_gdb(conf_gdb(), mycpu);
  else if (conf_show()) {
    while (1) {
      Insn_t insn = decoder(mycpu->s.pc);
      reg_t values[2];
      xlen_t oldpc = mycpu->s.pc;
      mycpu->execute_instruction(insn, values);
      mycpu->print(oldpc, &insn);
    }
  }
  else {
    if (conf_report() > 0) {
      pthread_t tnum;
      dieif(pthread_create(&tnum, 0, status_thread, 0), "failed to launch status_report thread");
    }
    atexit(exitfunc);
    my_interpreter(mycpu);
  }
}
