#include <config.h>
#include "xwrits.h"
#include <stdlib.h>

static struct timeval wait_over_time;
static struct timeval break_over_time;


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
      return TRAN_REST;
    else if (xwTIMEGEQ(now, wait_over_time))
      return TRAN_WARN;
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
  if (!check_idle) {
    /* Oops! Bug fix (8/17/98): If check_idle is off, we won't get keypresses;
       so schedule an alarm for exactly type_delay in the future. */
    Alarm *a = new_alarm(A_AWAKE);
    xwADDTIME(a->timer, last_key_time, type_delay);
    schedule(a);
  }
  
  do {
    xwGETTIME(wait_over_time);
    xwADDTIME(wait_over_time, wait_over_time, type_delay);
    val = loopmaster(0, wait_x_loop);
  } while (val == TRAN_REST);

  unschedule(A_AWAKE);
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
  struct timeval now;
  int break_over;
  xwGETTIME(now);
  break_over = xwTIMEGEQ(now, break_over_time);
  
  if (e->type == ClientMessage)
    /* Window manager deleted only xwrits window. Consider break over. */
    return (break_over ? TRAN_AWAKE : TRAN_CANCEL);
  
  else if (e->type == KeyPress) {
    last_key_time = now;
    return (break_over ? TRAN_AWAKE : TRAN_FAIL);
    
  } else
    return 0;
}

int
rest(void)
{
  struct timeval now;
  Alarm *a;
  int tran;
  
  blend_slideshow(slideshow[Resting]);
  ensure_one_hand();
  
  xwGETTIME(now);
  if (!check_idle)
    xwADDTIME(break_over_time, now, break_delay);
  else
    xwADDTIME(break_over_time, last_key_time, break_delay);
  if (xwTIMEGEQ(now, break_over_time))
    return TRAN_AWAKE;
  
  a = new_alarm(A_AWAKE);
  a->timer = break_over_time;
  schedule(a);
  
  if (ocurrent->break_clock) {
    clock_zero_time = a->timer;
    draw_clock(&now);
    refresh_hands();
    a = new_alarm(A_CLOCK);
    xwADDTIME(a->timer, now, clock_tick);
    schedule(a);
  }
  
  XFlush(display);
  tran = loopmaster(0, rest_x_loop);
  
  unschedule(A_AWAKE | A_CLOCK);
  return tran;
}


static int
ready_x_loop(XEvent *e)
{
  if (e->type == ButtonPress)
    return 1;
  else if (e->type == ClientMessage)
    /* last xwrits window deleted by window manager */
    return 1;
  else if (e->type == KeyPress) {
    /* if they typed, disappear automatically */
    xwGETTIME(last_key_time);
    return 1;
  } else
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
