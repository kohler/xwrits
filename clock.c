#include <stdlib.h>
#include <math.h>
#include "xwrits.h"

#define ClockWidth 30
#define ClockHeight 30
#define ClockHour 8.3
#define ClockMin 12.3
#define HandWidth 1.2
#define HandOffset 1.2

static GC clockforegc;
static GC clockbackgc;
static GC clockhandgc;

struct timeval clock_zero_time;
struct timeval clock_tick;


static void
draw_hand(Picture *p, int xin, int yin, int handlength,
	 int value, int valuecycle)
{
  double sinv = sin(value * 2 * M_PI / valuecycle);
  double cosv = cos(value * 2 * M_PI / valuecycle);
  XPoint points[4];
  
  double x = xin - HandOffset * sinv + 1;
  double y = yin + HandOffset * cosv + 1;
  points[0].x = (int)(x + HandWidth * cosv);
  points[0].y = (int)(y + HandWidth * sinv);
  points[1].x = (int)(x + handlength * sinv);
  points[1].y = (int)(y - handlength * cosv);
  points[2].x = (int)(x - HandWidth * cosv);
  points[2].y = (int)(y - HandWidth * sinv);
  points[4] = points[0];
  XDrawLine(display, p->large, clockhandgc, x, y, points[1].x, points[1].y);
  
  /*XDrawLines(display, p->large, clockforegc, points, 3, CoordModeOrigin);*/
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
  
  XFillArc(display, p->large, clockbackgc, x, y, ClockWidth,
	   ClockHeight, 0, 23040);
  XDrawArc(display, p->large, clockforegc, x, y, ClockWidth,
	   ClockHeight, 0, 23040);
  x += ClockWidth / 2;
  y += ClockHeight / 2;
  /*min = (seconds + 29) / SecPerMin;*/
  min = seconds;
  hour = min / MinPerHr;
  min %= MinPerHr;
  
  if (hour)
    draw_hand(p, x, y, ClockHour, hour, HrPerCycle);
  draw_hand(p, x, y, ClockMin, min, MinPerHr);

  /*
    XDrawLine(display, p->large, clockforegc, x, y,
	      (int)(x + ClockHour * sin(hour * 2 * M_PI / HrPerCycle)),
	      (int)(y - ClockHour * cos(hour * 2 * M_PI / HrPerCycle)));
  XDrawLine(display, p->large, clockforegc, x, y,
	    (int)(x + ClockMin * sin(min * 2 * M_PI / MinPerHr)),
	    (int)(y - ClockMin * cos(min * 2 * M_PI / MinPerHr)));*/
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
  XSetForeground(display, clockbackgc, p->background);
  XFillRectangle(display, p->large, clockbackgc,
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
  XSetForeground(display, clockbackgc, WhitePixel(display, screen_number));
}


void
init_clock(Drawable drawable)
{
  XGCValues gcv;
  XFontStruct *font;
  
  gcv.foreground = BlackPixel(display, screen_number);
  gcv.line_width = 3;
  gcv.cap_style = CapRound;
  font = XLoadQueryFont(display,
			"-adobe-helvetica-bold-r-normal--*-140-75-75-*");
  if (!font) font = XLoadQueryFont(display, "fixed");
  gcv.font = font->fid;
  clockforegc = XCreateGC(display, drawable, GCForeground | GCLineWidth |
			  GCCapStyle | GCFont, &gcv);
  
  gcv.line_width = 2;
  gcv.cap_style = CapButt;
  clockhandgc = XCreateGC(display, drawable, GCForeground | GCLineWidth |
			  GCCapStyle | GCFont, &gcv);
  
  gcv.foreground = WhitePixel(display, screen_number);
  clockbackgc = XCreateGC(display, drawable, GCForeground, &gcv);
}
