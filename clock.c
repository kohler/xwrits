#include <stdlib.h>
#include <math.h>
#include "xwrits.h"

#define ClockWidth 30
#define ClockHeight 30
#define ClockHour 7.3
#define ClockMin 11
#define HandWidth 1.5
#define HandOffset 1.5

static GC clock_fore_gc;
static GC clock_back_gc;
static GC clock_hand_gc;

struct timeval clock_zero_time;
struct timeval clock_tick;


static void
draw_hand(Picture *p, int xin, int yin, int hand_length,
	  int value, int value_cycle)
{
  double sinv = sin(value * 2 * M_PI / value_cycle);
  double cosv = cos(value * 2 * M_PI / value_cycle);

  XDrawLine(display, p->large, clock_hand_gc, xin, yin,
	    (int)(xin + hand_length * sinv), (int)(yin - hand_length * cosv));

#if 0
  double x = xin - HandOffset * sinv + 1;
  double y = yin + HandOffset * cosv + 1;
  XPoint points[4];
  points[0].x = (int)(x + HandWidth * cosv);
  points[0].y = (int)(y + HandWidth * sinv);
  points[1].x = (int)(x + handlength * sinv);
  points[1].y = (int)(y - handlength * cosv);
  points[2].x = (int)(x - HandWidth * cosv);
  points[2].y = (int)(y - HandWidth * sinv);
  points[4] = points[0];
  XDrawLines(display, p->large, clock_fore_gc, points, 3, CoordModeOrigin);
#endif
}


static void
draw_1_clock(Picture *p, int seconds)
{
  int x, y;
  int hour, min;
  
  p->clock = 1;
  if (!p->large) return;
  x = p->clock_x_off;
  y = p->clock_y_off;
  
  XFillArc(display, p->large, clock_back_gc, x, y, ClockWidth,
	   ClockHeight, 0, 23040);
  XDrawArc(display, p->large, clock_fore_gc, x, y, ClockWidth,
	   ClockHeight, 0, 23040);
  x += ClockWidth / 2;
  y += ClockHeight / 2;
  min = (seconds + 5) / SecPerMin;
  hour = min / MinPerHr;
  min %= MinPerHr;
  
  if (hour)
    draw_hand(p, x, y, ClockHour, hour, HrPerCycle);
  draw_hand(p, x, y, ClockMin, min, MinPerHr);
}


void
draw_clock(struct timeval *now)
{
  struct timeval diff;
  int sec, i;
  xwSUBTIME(diff, *now, clock_zero_time);
  sec = diff.tv_sec < 0 ? -diff.tv_sec : diff.tv_sec;
  if (diff.tv_usec >= 500000) sec++;
  for (i = 0; i < current_slideshow->nslides; i++) {
    Picture *p = current_slideshow->picture[i];
    draw_1_clock(p, sec);
  }
}


static void
erase_1_clock(Picture *p)
{
  p->clock = 0;
  if (!p->large) return;
  XSetForeground(display, clock_back_gc, p->background);
  XFillRectangle(display, p->large, clock_back_gc,
		 p->clock_x_off - 1, p->clock_y_off - 1,
		 ClockWidth + 11, ClockHeight + 11);
}


void
erase_clock(void)
{
  int i;
  for (i = 0; i < current_slideshow->nslides; i++) {
    Picture *p = current_slideshow->picture[i];
    if (p->clock) erase_1_clock(p);
  }
  XSetForeground(display, clock_back_gc, white_pixel);
}


void
init_clock(Drawable drawable)
{
  XGCValues gcv;
  XFontStruct *font;
  
  gcv.foreground = black_pixel;
  gcv.line_width = 3;
  gcv.cap_style = CapRound;
  font = XLoadQueryFont(display,
			"-adobe-helvetica-bold-r-normal--*-140-75-75-*");
  if (!font) font = XLoadQueryFont(display, "fixed");
  gcv.font = font->fid;
  clock_fore_gc = XCreateGC(display, drawable, GCForeground | GCLineWidth |
			    GCCapStyle | GCFont, &gcv);

  clock_hand_gc = clock_fore_gc;
  
  gcv.foreground = white_pixel;
  clock_back_gc = XCreateGC(display, drawable, GCForeground, &gcv);
}
