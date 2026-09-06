#include <limits.h>
#include <unistd.h>
#include <ncurses.h>

#include "caveat.h"
#include "hart.h"
#include "pipesim.h"

option<int> conf_framerate("framerate",  20000, "Display framerate in microseconds");

static long long stop_cycle = 0;	// when to stop free running

void core_t::showreg(WINDOW* win, int r, char& sep)
{
  if (r != NOREG) {
    wprintw(win, "%c", sep);
    if ((1LL<<r) & busy) wattron(win, A_REVERSE);
    wprintw(win, "%s", reg_name[r]);
    if ((1LL<<r) & busy) wattroff(win, A_REVERSE);
    sep = ',';
  }
}

void wdisasm(WINDOW* win, uintptr_t pc, const Insn_t* i, core_t* cpu)
{
  int n = 0;
  if (i->opcode() == Op_ZERO) {
    wprintw(win,  "Nothing here");
    return;
  }
  uint32_t b = *(uint32_t*)pc;
  if (i->compressed())
    wprintw(win, "    %04x  ", b&0xFFFF);
  else
    wprintw(win, "%08x  ",     b);
  wprintw(win, "%-23s", op_name[i->opcode()]);
  char sep = ' ';
  cpu->showreg(win, i->rd(), sep);
  cpu->showreg(win, i->rs1(), sep);
  if (i->longimmed())
    wprintw(win, "%c%ld", sep, i->immed());
  else {
    cpu->showreg(win, i->rs2(), sep);
    cpu->showreg(win, i->rs3(), sep);
    wprintw(win, "%c%ld", sep, i->immed());
  }
}

void paint_instruction(WINDOW* win, int y, int x, history_t* h, core_t* cpu)
{
  char buf[256];
  slabelpc(buf, h->pc);
  wmove(win, y, x);
  wprintw(win, "%7lld %c [%16lx] %s", h->cycle, h->label, h->val, buf);
  wdisasm(win, h->pc, &h->insn, cpu);
}

void paint_pipeline(WINDOW* win, pipeline_t& pipe)
{
  wprintw(win, "%s[", pipe.name);
  for (int k=0; k<pipe.depth; ++k) {
    int i = pipe.index(k);
    char c = ' ';
    if (pipe.stage[i])
      c = pipe.stage[i]->label;
    wprintw(win, "%c", c);
  }
  wprintw(win, "] ");
}

void paint_busy_regs(WINDOW* win, uint64_t busy)
{
  char first[65], second[65];
  for (int r=0; r<64; ++r) {
    if (busy & (1LL<<r)) {
      second[r] = r%10 + '0';
      first[r] = r<10 ? ' ' : (r/10)+'0';
    }
    else
      first[r] = second[r] = ' ';
  }
  first[64] = second[64] = 0;
  wprintw(win, "%s\n%s", first, second);
}

void paint_state(WINDOW* win, core_t* cpu)
{
  int lines = 0;
  wclear(win);
  wmove(win, 0, 0);
  wprintw(win, "%7lld ", cycle);
  paint_pipeline(win, cpu->iu);
  paint_pipeline(win, cpu->mem);
  paint_pipeline(win, cpu->fpu);
  ++lines;
  //  wmove(win, lines, 0);
  //  paint_busy_regs(win, cpu->busy);
  long now = cycle;
  int i = (cpu->executed()-1) % HISTORY;
  history_t* h = &cpu->history[i];
  for (int k=0; --now>=0 && k<LINES-lines-3; ++k) {
    if (h->cycle == now) {
      paint_instruction(win, now%(LINES-lines-3)+lines, 0, h, cpu);
      i = (i - 1 + HISTORY) % HISTORY;
      h = &cpu->history[i];
    }
  }
}
 
void interactive(core_t* cpu)
{
  fprintf(stderr, "Starting ncurses\n");
  initscr();			// start ncurses
  keypad(stdscr, true);		// use all keys
  nonl();
  cbreak();			// line buffer disabled
  noecho();
  nodelay(stdscr, true);

  WINDOW** snapshot = new WINDOW*[HISTORY];
  for (int k=0; k<HISTORY; ++k)
    snapshot[k] = newwin(LINES, COLS, 0, 0);

  int ch;			// key pressed
  int number = 0;		// entered from keyboard
  long framerate = conf_framerate();
  int behind = 1;		// showing the past

  while (1) {			// infinite loop

    // loop until any key pressed or target cycle reached
    while ((ch=getch()) == ERR) {
      if (cycle <= stop_cycle) {
	// advance clock cycle
	cpu->clock_pipeline(&cpu->iu);
	cpu->clock_pipeline(&cpu->fpu);
	cpu->clock_pipeline(&cpu->mem);
	++cycle;
	history_t* h = &cpu->history[cpu->executed() % HISTORY];
	cpu->issue(h);
	paint_state(snapshot[(cycle-behind) % HISTORY], cpu);
	redrawwin(snapshot[(cycle-behind) % HISTORY]);
	wrefresh(snapshot[(cycle-behind) % HISTORY]);
      }
      if (framerate)
	usleep(framerate);
    }
    stop_cycle = 0;
    framerate = conf_framerate();
    redrawwin(snapshot[(cycle-behind) % HISTORY]);
    wrefresh(snapshot[(cycle-behind) % HISTORY]);
    
    switch (ch) {
    case 'q':			// quit
      endwin();
      return;
    case 'b':			// go back
      dieif(behind<0, "behind<0");
      if (behind < HISTORY && (cycle-behind) > 0)
	++behind;
      redrawwin(snapshot[(cycle-behind) % HISTORY]);
      wrefresh(snapshot[(cycle-behind) % HISTORY]);
      break;
    case 'f':			// go forward
      dieif(behind<0, "behind<0");
      if (--behind <= 0) {
	stop_cycle = cycle;
	behind = 1;
      }
      redrawwin(snapshot[(cycle-behind) % HISTORY]);
      wrefresh(snapshot[(cycle-behind) % HISTORY]);
      break;
    case '0'...'9':
      number = 10*number + (ch-'0');
      continue;			// don't reset number
    case 'c':			// continue free running
      stop_cycle = number ? number : LLONG_MAX;
      behind = 1;
      break;
    case 'C':			// continue free running
      stop_cycle = number ? number : LLONG_MAX;
      behind = 1;
      framerate = 0;
      break;
    }
    number = 0;
  } // infinite loop
}
