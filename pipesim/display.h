
extern long long cycle;		// current time
extern int insn_issued;


/*
 * Record past execution of an instruction.
 */
struct history_t {
  long long cycle;		// time instruction launched
  uintptr_t pc;
  Insn_t insn;			// may have renamed registers
  uintptr_t val;		// could be many things
  char label;			// printable tag
};

const int history_length = 1000;

struct pipeline_t {
  history_t** pipe;
  int* countdown;		// time until instruction finished
  int depth;
  const char* name;
  pipeline_t(const char* n, int N) {
    name=n; pipe=new history_t*[N]; countdown=new int[N]; depth=N;
  }
};

struct core_t : public hart_t {
  uint64_t busy;		// busy bits for int, fp registers
  pipeline_t* iu;		// integer unit
  pipeline_t* fpu;		// floating point unit
  pipeline_t* mem;		// memory unit
  core_t(int argc, const char* argv[], const char* envp[]);
  void clock_pipeline(pipeline_t* unit);
  bool issue(history_t* h);

  long issued;
  history_t* history;
  int history_length;
  friend void simulator(hart_t* h, Header_t* bb, uintptr_t* ap);
};




void paint_instructions(int y, int x, int lines, history_t history[], int begin);

void interactive(core_t* cpu);


void display_simulator(hart_t* h, Header_t* bb, uintptr_t* ap);
