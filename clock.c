#include <config.h>
#include "xwrits.h"
#include <stdlib.h>
#include <math.h>

#define ClockWidth 30
#define ClockHeight 30
#define ClockHour 7
#define ClockMin 11
#define HandWidth 1.5
#define HandOffset 1.5

struct timeval clock_zero_time;
struct timeval clock_tick;


static void
draw_hand(Port *port, Drawable drawable, int xin, int yin, int hand_length,
	  int value, int value_cycle)
{
  double sinv = sin(value * 2 * M_PI / value_cycle);
  double cosv = cos(value * 2 * M_PI / value_cycle);

  XDrawLine(port->display, drawable, port->clock_hand_gc, xin, yin,
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
  XDrawLines(port->display, drawable, port->clock_fore_gc,
	     points, 3, CoordModeOrigin);
#endif
}


static void
draw_1_clock(Hand *hand, int seconds)
{
  Port *port = hand->port;
  Picture *p = (Picture *)(hand->slideshow->images[hand->slide]->user_data);
  int x, y;
  int hour, min;

  x = p->clock_x_off;
  y = p->clock_y_off;
  
  XFillArc(port->display, hand->w, port->white_gc,
	   x, y, ClockWidth, ClockHeight, 0, 23040);
  XDrawArc(port->display, hand->w, port->clock_fore_gc,
	   x, y, ClockWidth, ClockHeight, 0, 23040);
  x += ClockWidth / 2;
  y += ClockHeight / 2;
  min = (seconds + 5) / SEC_PER_MIN;
  hour = min / MIN_PER_HOUR;
  min %= MIN_PER_HOUR;
  
  if (hour)
    draw_hand(port, hand->w, x, y, ClockHour, hour, HOUR_PER_CYCLE);
  draw_hand(port, hand->w, x, y, ClockMin, min, MIN_PER_HOUR);
}


static int
now_to_clock_sec(struct timeval *now_ptr)
{
  struct timeval now;
  struct timeval diff;
  int sec;
  
  if (!now_ptr)
    xwGETTIME(now);
  else
    now = *now_ptr;
  
  xwSUBTIME(diff, now, clock_zero_time);
  sec = diff.tv_sec < 0 ? -diff.tv_sec : diff.tv_sec;
  if (diff.tv_usec >= 500000) sec++;
  return sec;
}

void
draw_clock(Hand *h, struct timeval *now)
{
  draw_1_clock(h, now_to_clock_sec(now));
  h->clock = 1;
}

void
draw_all_clocks(struct timeval *now)
{
  Hand *h;
  int i, sec = now_to_clock_sec(now);
  for (i = 0; i < nports; i++)
    for (h = ports[i].hands; h; h = h->next) {
      if (h->mapped)
	draw_1_clock(h, sec);
      h->clock = 1;
    }
}


void
erase_clock(Hand *hand)
{
  Port *port = hand->port;
  Picture *p = (Picture *)(hand->slideshow->images[hand->slide]->user_data);
  XCopyArea(port->display, p->pix[port->port_number], hand->w,
	    port->clock_fore_gc,
	    p->clock_x_off - 2, p->clock_y_off - 2,
	    ClockWidth + 4, ClockHeight + 4,
	    p->clock_x_off - 2, p->clock_y_off - 2);
  hand->clock = 0;
}

void
erase_all_clocks(void)
{
  Hand *h;
  int i;
  for (i = 0; i < nports; i++)
    for (h = ports[i].hands; h; h = h->next) {
      if (h->mapped && h->clock)
	erase_clock(h);
      h->clock = 0;
    }
}
