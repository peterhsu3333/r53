#include <limits.h>
#include <unistd.h>
#include <ncurses.h>

#include "caveat.h"
#include "hart.h"
#include "display.h"

option<int> conf_framerate("framerate",  20000, "Display framerate in microseconds");

static long long stop_cycle = 0;	// when to stop free running


void paint_instruction(WINDOW* win, int y, int x, history_t* h)
{
  char buf[256];
  int len = slabelpc(buf, h->pc);
  sdisasm(buf+len, h->pc, &h->insn);
  wmove(win, y, x);
  wprintw(win, "%7lld %c [%16lx] %s", h->cycle, h->label, h->val, buf);
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

void paint_state(WINDOW* win, core_t* cpu)
{
  wclear(win);
  wmove(win, 0, 0);
  paint_pipeline(win, cpu->iu);
  paint_pipeline(win, cpu->mem);
  paint_pipeline(win, cpu->fpu);
  wmove(win, 2, 0);
  long now = cycle;
  int lines = LINES - 2;
  for (int k=0; k<lines-3; ++k) {
    if (--now < 0)		// current cycle time not yet executed
      break;
    paint_instruction(win, now%lines + 2, 0, &cpu->history[now % HISTORY]);
  }
  //  wrefresh(win);
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
	
	history_t* h = &cpu->history[cycle % HISTORY];
	h->cycle = cycle;
	h->pc = cpu->s.pc;
	h->insn = decoder(cpu->s.pc);
	h->label = cpu->executed() % 26 + 'A';
	reg_t value[2];
	cpu->execute_instruction(h->insn, value);
	h->val = value[0].x;
	paint_state(snapshot[cycle % HISTORY], cpu);
	++cycle;
      }
      redrawwin(snapshot[(cycle-behind) % HISTORY]);
      wrefresh(snapshot[(cycle-behind) % HISTORY]);
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
