#include <config.h>
#include "xwrits.h"
#include <stdlib.h>

static struct timeval wait_over_time;
static struct timeval break_over_time;


static int
wait_x_loop(XEvent *e, struct timeval *now)
{
  struct timeval diff;
  
  if (e->type == KeyPress || e->type == MotionNotify) {
    xwSUBTIME(diff, *now, last_key_time);
    last_key_time = *now;
    
    if (xwTIMEGEQ(diff, break_delay))
      return TRAN_REST;
    else if (xwTIMEGEQ(*now, wait_over_time))
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
  
  xwGETTIME(last_key_time);
  if (!check_idle) {
    /* Oops! Bug fix (8/17/98): If check_idle is off, we won't get keypresses;
       so schedule an alarm for exactly type_delay in the future. */
    Alarm *a = new_alarm(A_AWAKE);
    xwADDTIME(a->timer, last_key_time, type_delay);
    schedule(a);
  }
  
  if (check_mouse) {
    Alarm *a = new_alarm(A_MOUSE);
    xwGETTIME(a->timer);
    schedule(a);
    last_mouse_root = None;
  }
  
  do {
    xwGETTIME(wait_over_time);
    xwADDTIME(wait_over_time, wait_over_time, type_delay);
    val = loopmaster(0, wait_x_loop);
  } while (val == TRAN_REST);
  
  unschedule(A_AWAKE | A_MOUSE);
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

static int current_cheats;

static int
rest_x_loop(XEvent *e, struct timeval *now)
{
  /* If the break is over, wake up. */
  if (xwTIMEGEQ(*now, break_over_time))
    return TRAN_AWAKE;
  
  if (e->type == ClientMessage && active_hands() == 0)
    /* Window manager deleted last xwrits window. Consider break over. */
    return TRAN_CANCEL;
  else if (e->type == KeyPress || e->type == MotionNotify) {
    last_key_time = *now;
    current_cheats++;
    return (current_cheats > max_cheats ? TRAN_FAIL : 0);
  } else
    return 0;
}

int
rest(void)
{
  struct timeval now;
  Alarm *a;
  int tran;
  
  set_all_slideshows(hands, resting_slideshow);
  set_all_slideshows(icon_hands, resting_icon_slideshow);
  ensure_one_hand();
  current_cheats = 0;
  
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

  if (check_mouse) {
    a = new_alarm(A_MOUSE);
    a->timer = now;
    a->timer.tv_sec += 5;	/* allow 5 seconds for people to jiggle the
				   mouse before we save its position */
    schedule(a);
    last_mouse_root = None;
  }
  
  if (ocurrent->break_clock) {
    clock_zero_time = a->timer;
    draw_all_clocks(&now);
    a = new_alarm(A_CLOCK);
    xwADDTIME(a->timer, now, clock_tick);
    schedule(a);
  }
  
  XFlush(display);
  tran = loopmaster(0, rest_x_loop);
  
  unschedule(A_AWAKE | A_CLOCK | A_MOUSE);
  erase_all_clocks();
  return tran;
}


static int
ready_x_loop(XEvent *e, struct timeval *now)
{
  if (e->type == ButtonPress)
    return 1;
  else if (e->type == KeyPress || e->type == MotionNotify) {
    /* if they typed, disappear automatically */
    last_key_time = *now;
    return 1;
  } else
    return 0;
}

void
ready(void)
{
  set_all_slideshows(hands, ready_slideshow);
  set_all_slideshows(icon_hands, ready_icon_slideshow);
  ensure_one_hand();
  
  if (ocurrent->beep)
    XBell(display, 0);
  XFlush(display);

  if (check_mouse) {
    Alarm *a = new_alarm(A_MOUSE);
    xwGETTIME(a->timer);
    schedule(a);
    last_mouse_root = None;
  }
  
  loopmaster(0, ready_x_loop);
  
  unschedule(A_MOUSE);
}


void
unmap_all(void)
{
  XEvent event;
  
  while (hands->next)
    destroy_hand(hands);
  destroy_hand(hands);		/* final destroy_hand() doesn't remove the
                                   hand from the list, it unmaps it. */
  
  XFlush(display);
  while (XPending(display)) {
    XNextEvent(display, &event);
    default_x_processing(&event);
  }
}
