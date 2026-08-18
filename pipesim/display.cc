#include <limits.h>
#include <unistd.h>
#include <ncurses.h>

#include "caveat.h"
#include "hart.h"
#include "display.h"

option<int> conf_framerate("framerate",  20000, "Display framerate in microseconds");

static long long stop_cycle = 0;	// when to stop free running


void paint_instructions(WINDOW* w, int y, int x, int lines,
			history_t history[], int history_length, long long now)
{
  for (int k=0; k<lines-3; ++k) {
    if (--now < 0)		// current cycle time not yet executed
      break;
    history_t* h = &history[now % history_length];
    //    if (h == 0)
    //      continue;
    char buf[256];
    int len = slabelpc(buf, h->pc);
    sdisasm(buf+len, h->pc, &h->insn);
    wmove(w, y+now%lines, x);
    //wprintw(w, "%7lld %lx, %s", h->cycle, h->pc, buf);
    wprintw(w, "%7lld %c [%16lx] %s", h->cycle, h->label, h->val, buf);
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

  int ch;			// key pressed
  int number = 0;		// entered from keyboard
  long framerate = conf_framerate();
  int behind = 0;		// showing the past

  while (1) {			// infinite loop

    // loop until any key pressed or target cycle reached
    while ((ch=getch()) == ERR) {
      if (cycle <= stop_cycle) {
#if 0
	// advance clock cycle
	cpu->clock_pipeline(cpu->iu);
	cpu->clock_pipeline(cpu->fpu);
	cpu->clock_pipeline(cpu->mem);
#endif
	history_t* h = &cpu->history[cycle % cpu->history_length];
	h->cycle = cycle;

#if 0
	//h->label = makelabel();
	h->label = cpu->executed() % 26 + 'A';
	cpu->single_step(h->pc, h->insn, h->val);
	++cycle;
#endif
	
	
	//if (!cpu->issue(h))
	//  ++cycle;
	clear();
	paint_instructions(stdscr, 1, 0, LINES-1, cpu->history, cpu->history_length, cycle-behind);
	refresh();
      }
      if (framerate)
	usleep(framerate);
    }
    stop_cycle = 0;
    framerate = conf_framerate();
    clear();
    paint_instructions(stdscr, 1, 0, LINES-1, cpu->history, cpu->history_length, cycle-behind);
    refresh();
    
    switch (ch) {
    case 'q':			// quit
      endwin();
      return;
    case 'b':			// go back
      dieif(behind<0, "behind<0");
      if (behind < cpu->history_length && (cycle-behind) > 0)
	++behind;
      clear();
      paint_instructions(stdscr, 1, 0, LINES-1, cpu->history, cpu->history_length, cycle-behind);
      refresh();
      break;
    case 'f':			// go forward
      dieif(behind<0, "behind<0");
      if (--behind < 0) {
	stop_cycle = cycle;
	behind = 0;
      }
      clear();
      paint_instructions(stdscr, 1, 0, LINES-1, cpu->history, cpu->history_length, cycle-behind);
      refresh();
      break;
    case '0'...'9':
      number = 10*number + (ch-'0');
      continue;			// don't reset number
    case 'c':			// continue free running
      stop_cycle = number ? number : LLONG_MAX;
      behind = 0;
      break;
    case 'C':			// continue free running
      stop_cycle = number ? number : LLONG_MAX;
      behind = 0;
      framerate = 0;
      break;
    }
    number = 0;
  } // infinite loop
}
