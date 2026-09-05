
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
  int pipes;			// index into pipestates;
  char label;			// printable tag
};

const int HISTORY = 256;
const int max_pipe_depth = 20;

struct pipeline_t {
  history_t* stage[max_pipe_depth];
  int countdown[max_pipe_depth]; // time until instruction finished
  int depth;
  const char* name;
  pipeline_t(const char* n, int N) {
    name=n; depth=N;
  }
  int index(int k) { return (cycle-k+max_pipe_depth) % max_pipe_depth; }
};

struct core_t : public hart_t {
  uint64_t busy;		// busy bits for int, fp registers
  pipeline_t iu;		// integer unit
  pipeline_t fpu;		// floating point unit
  pipeline_t mem;		// memory unit
  core_t(int argc, const char* argv[], const char* envp[]);
  void clock_pipeline(pipeline_t* unit);
  bool issue(history_t* h);

  long issued;
  history_t* history;
  friend void simulator(hart_t* h, Header_t* bb, uintptr_t* ap);
};




void paint_instructions(int y, int x, int lines, history_t history[], int begin);

void interactive(core_t* cpu);


void display_simulator(hart_t* h, Header_t* bb, uintptr_t* ap);
