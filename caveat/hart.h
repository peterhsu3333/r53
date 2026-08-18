/*
  Copyright (c) 2021 Peter Hsu.  All Rights Reserved.  See LICENCE file for details.
*/

extern option<> conf_gdb;
extern option<bool> conf_show;



/*
  RISC-V processor state.
*/
union reg_t {
  xlen_t  x;			// signed integer view
  uxlen_t u;			// unsigned integer view
  float   f;			// single-precision float view
  double  d;			// double-precision float view
  xlen_t  operator=(xlen_t  e) { x=e; return x; }
  uxlen_t operator=(uxlen_t e) { x=e; return x; }
  float   operator=(float   e) { f=e; return x; }
  double  operator=(double  e) { d=e; return x; }
};

struct processor_state_t {
  xlen_t pc;			// program counter
  reg_t reg[SCALAR_REGS];	// all scalar registers
  uint16_t fflags;		// floating point status flags
  uint16_t frm;			// floating point rounding mode
};


struct insn_t {
  uint64_t bits;
  insn_t(uint64_t x) { bits=x; }
};

#define READ_REG(n)   s.reg[n]
#define READ_FREG(n)  s.reg[n]

#define WRITE_REG(n, v)   s.reg[n] = (v)
#define WRITE_FREG(n, v)  s.reg[n] = (v)

#define CSR_FFLAGS 0x1
#define CSR_FRM 0x2
#define CSR_FCSR 0x3

#define FP_RD_NE  0
#define FP_RD_0   1
#define FP_RD_DN  2
#define FP_RD_UP  3
#define FP_RD_NMM 4

#define FSR_RD_SHIFT 5
#define FSR_RD   (0x7 << FSR_RD_SHIFT)

#define FPEXC_NX 0x01
#define FPEXC_UF 0x02
#define FPEXC_OF 0x04
#define FPEXC_DZ 0x08
#define FPEXC_NV 0x10

#define FSR_AEXC_SHIFT 0
#define FSR_NVA  (FPEXC_NV << FSR_AEXC_SHIFT)
#define FSR_OFA  (FPEXC_OF << FSR_AEXC_SHIFT)
#define FSR_UFA  (FPEXC_UF << FSR_AEXC_SHIFT)
#define FSR_DZA  (FPEXC_DZ << FSR_AEXC_SHIFT)
#define FSR_NXA  (FPEXC_NX << FSR_AEXC_SHIFT)
#define FSR_AEXC (FSR_NVA | FSR_OFA | FSR_UFA | FSR_DZA | FSR_NXA)



class hart_t {
  static int _num_harts;	// how many have been cloned
  static hart_t* _list;		// for find() using thread id
  hart_t* _next;		// list of hart_t
  int sid;			// strand index number
  int _tid;			// Linux thread number
  pthread_t ptnum;		// pthread handle
  
  void initialize();		// used by constructor functions

  friend void controlled_by_gdb(const char* host_port, hart_t* cpu);
  friend void thread_interpreter(hart_t* me);
  friend void terminate_threads();
  friend int clone_thread(hart_t* child);
  friend void exit_func();

  static thread_local Header_t mismatch_header;
  static thread_local Header_t* mismatch_target;
  static thread_local Header_t** target;
    
public:
  processor_state_t s;		// includes pc
  long _executed;		// number of instructions
  //  Header_t* bb;			// basic block cooresponding to pc
  //  Insn_t* i;			// predecoded instruction at pc
  
  Tcache_t tcache;
  
  simfunc_t simulator;		// function pointer for simulation
  clonefunc_t clone;		// function pointer just for clone system call
  interpreterfunc_t interpreter;// function pointer for interpreter
  syscallfunc_t riscv_syscall;	// function pointer for system calls
  
  hart_t(int argc, const char* argv[], const char* envp[]);
  hart_t(hart_t* p);
  ~hart_t();

  Header_t* find_bb(xlen_t pc);
  bool run_basic_block(reg_t* values);
  bool execute_instruction(Insn_t insn, reg_t* ap);
  
  void print(uintptr_t pc, Insn_t* i, FILE* out =stderr);
  long executed() { return _executed; }
  void count_insn(int n =1) { _executed += n; }
  long flushed() { return tcache.flushed(); }

  static hart_t* list() { return _list; }
  hart_t* next() { return _next; }
  int tid() { return _tid; }
  static hart_t* find(int tid); // hart given Linux thread ID
  static int num_harts() { return _num_harts; }

  xlen_t get_csr(int which, insn_t insn, bool write, bool peek =0);
  xlen_t get_csr(int which) { return get_csr(which, insn_t(0), false, true); }
  void set_csr(int which, xlen_t val);

  template<typename op>	xlen_t csr_func(xlen_t what, op f) {
    xlen_t old = get_csr(what);
    set_csr(what, f(old));
    return old;
  }
  template<class T> bool cas(long r1, T replace, T expect, reg_t*& ap)
  {
    T* ptr = (T*)r1;
    T oldval = __sync_val_compare_and_swap(ptr, expect, replace);
    *ap++ = (xlen_t)ptr;
    return (oldval != expect);
  }
  template<typename op>	int32_t amo_int32(xlen_t a, op f, reg_t*& ap) {
    int32_t lhs, *ptr = (int32_t*)a;
    do lhs = *ptr;
    while (!__sync_bool_compare_and_swap(ptr, lhs, f(lhs)));
    *ap++ = (xlen_t)ptr;
    return lhs;
  }
  template<typename op>	int64_t amo_int64(xlen_t a, op f, reg_t*& ap) {
    int64_t lhs, *ptr = (int64_t*)a;
    do lhs = *ptr;
    while (!__sync_bool_compare_and_swap(ptr, lhs, f(lhs)));
    *ap++ = (xlen_t)ptr;
    return lhs;
  }
};


long default_riscv_syscall(hart_t* h, long a0);
long proxy_syscall(long rvnum, long a0, long a1, long a2, long a3, long a4, long a5, hart_t* me);
