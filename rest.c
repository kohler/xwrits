#include "xwrits.h"
#include <stdlib.h>

static struct timeval last_key_time;
static struct timeval wait_over_time;


static int
wait_x_loop(XEvent *e)
{
  struct timeval now;
  struct timeval diff;
  
  if (e->type == KeyPress) {
    xwGETTIME(now);
    xwSUBTIME(diff, now, last_key_time);
    last_key_time = now;
    
    if (xwTIMEGEQ(diff, break_delay))
      return WarnRest;
    else if (xwTIMEGEQ(now, wait_over_time))
      return Return;
    else
      return 0;
    
  } else
    return 0;
}


void
wait_for_break(void)
{
  int val;
  
  /* Hack to make "spectacular effects" work as expected.
     Without this code, xwrits t=0 would not do anything until a keypress. */
  if (xwTIMELEQ0(type_delay)) return;
  
  xwGETTIME(last_key_time);
  
  do {
    xwGETTIME(wait_over_time);
    xwADDTIME(wait_over_time, wait_over_time, type_delay);
    val = loopmaster(0, wait_x_loop);
  } while (val == WarnRest);
}


static void
ensure_one_hand(void)
{
  Hand *h;
  int nummapped = 0;
  for (h = hands; h; h = h->next)
    if (h->mapped)
      nummapped++;
  if (nummapped == 0)
    XMapRaised(display, hands->w);
}


static int
rest_x_loop(XEvent *e)
{
  if (e->type == ClientMessage)
    /* Window manager deleted only xwrits window. Consider break over. */
    return RestCancelled;
  else if (e->type == KeyPress)
    return RestFailed;
  else
    return 0;
}


int
rest(void)
{
  struct timeval now;
  Alarm *a;
  int val;
  
  blend_slideshow(slideshow[Resting]);
  ensure_one_hand();
  
  xwGETTIME(now);
  a = new_alarm(Return);
  xwADDTIME(a->timer, now, break_delay);
  schedule(a);
  
  if (ocurrent->break_clock) {
    clock_zero_time = a->timer;
    draw_clock(&now);
    refresh_hands();
    a = new_alarm(Clock);
    xwADDTIME(a->timer, now, clock_tick);
    schedule(a);
  }
  
  XFlush(display);
  val = loopmaster(0, rest_x_loop);
  
  unschedule(Return | Clock);
  return val == Return ? RestOK : val;
}


static int
ready_x_loop(XEvent *e)
{
  if (e->type == ButtonPress)
    return 1;
  else if (e->type == ClientMessage)
    /* last xwrits window deleted by window manager */
    return 1;
  else if (e->type == KeyPress)
    /* if they typed, disappear automatically */
    return 1;
  else
    return 0;
}


void
ready(void)
{
  blend_slideshow(slideshow[Ready]);
  ensure_one_hand();
  
  if (ocurrent->beep)
    XBell(display, 0);
  XFlush(display);
  
  loopmaster(0, ready_x_loop);
}


void
unmap_all(void)
{
  XEvent event;
  Hand *h;
  
  while (hands->next)
    destroy_hand(hands);
  h = hands;
  
  XUnmapWindow(display, h->w);
  /* Synthetic UnmapNotify required by ICCCM to withdraw the window */
  event.type = UnmapNotify;
  event.xunmap.event = port.root_window;
  event.xunmap.window = h->w;
  event.xunmap.from_configure = False;
  XSendEvent(display, port.root_window, False,
	     SubstructureRedirectMask | SubstructureNotifyMask, &event);
  
  blend_slideshow(slideshow[Warning]);
  
  XFlush(display);
  while (XPending(display)) {
    XNextEvent(display, &event);
    default_x_processing(&event);
  }
}
