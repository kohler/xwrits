#include <config.h>
#include "xwrits.h"
#include <stdlib.h>

static struct timeval wait_over_time;
static struct timeval break_over_time;


static int
wait_x_loop(XEvent *e, struct timeval *now)
{
  struct timeval diff;
  
  if (e->type == KeyPress || e->type == MotionNotify
      || e->type == ButtonPress) {
    xwSUBTIME(diff, *now, last_key_time);
    last_key_time = *now;

    /* if check_idle is on, long idle periods are the same as breaks */
    if (check_idle && xwTIMEGEQ(diff, idle_time))
      return TRAN_REST;

    /* if check_quota is on, mini-breaks add up over time */
    if (check_quota && xwTIMEGEQ(diff, quota_time)) {
      xwADDTIME(quota_allotment, quota_allotment, diff);
      if (xwTIMEGEQ(quota_allotment, break_delay))
	return TRAN_REST;
    }
    
    /* wake up if time to warn */
    return (xwTIMEGEQ(*now, wait_over_time) ? TRAN_WARN : 0);
    
  } else
    return 0;
}

void
wait_for_break(void)
{
  int val;
  
  xwGETTIME(last_key_time);
  if (!check_idle) {
    /* Oops! Bug fix (8/17/98): If check_idle is off, schedule an alarm for
       exactly type_delay in the future. */
    Alarm *a = new_alarm(A_AWAKE);
    xwADDTIME(a->timer, last_key_time, type_delay);
    schedule(a);
  }
  
  do {
    xwGETTIME(wait_over_time);
    xwADDTIME(wait_over_time, wait_over_time, type_delay);
    if (check_quota) xwSETTIME(quota_allotment, 0, 0);
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
  else if (e->type == KeyPress || e->type == MotionNotify
	   || e->type == ButtonPress) {
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
  
  /* determine when to end the break */
  xwGETTIME(now);
  if (!check_idle)
    xwADDTIME(break_over_time, now, break_delay);
  else
    xwADDTIME(break_over_time, last_key_time, break_delay);
  /* if check_quota is on, reduce break length by quota_allotment */
  if (check_quota)
    xwSUBTIME(break_over_time, break_over_time, quota_allotment);
  /* if break already over, return */
  if (xwTIMEGEQ(now, break_over_time))
    return TRAN_AWAKE;
  a = new_alarm(A_AWAKE);
  a->timer = break_over_time;
  schedule(a);
  
  /* set up pictures */
  set_all_slideshows(hands, resting_slideshow);
  set_all_slideshows(icon_hands, resting_icon_slideshow);
  ensure_one_hand();
  current_cheats = 0;

  /* reschedule mouse position query timing: allow 5 seconds for people to
     jiggle the mouse before we save its position */
  if (check_mouse) {
    a = grab_alarm_data(A_MOUSE, 0);
    a->timer = now;
    a->timer.tv_sec += 5;
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
  
  unschedule(A_AWAKE | A_CLOCK);
  erase_all_clocks();
  return tran;
}


static int
ready_x_loop(XEvent *e, struct timeval *now)
{
  if (e->type == KeyPress || e->type == MotionNotify
      || e->type == ButtonPress) {
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

  loopmaster(0, ready_x_loop);
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
