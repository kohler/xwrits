#include <config.h>
#include "xwrits.h"
#include <stdlib.h>
#include <assert.h>

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


/* wait for break */

static struct timeval wait_over_time;

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
      if (check_idle && xwTIMEGEQ(quota_allotment, idle_time))
	return TRAN_REST;
    }
    
    /* wake up if time to warn */
    return (xwTIMEGEQ(*now, wait_over_time) ? TRAN_WARN : 0);
    
  } else
    return 0;
}

static int
adjust_wait_time(struct timeval *wait_began_time)
     /* Adjust the time to wake up to reflect the length of the break, if
        under check_quota. Want to be able to type for slightly longer if
        you've been taking mini-breaks. */
{
  struct timeval this_break_time;
  struct timeval break_end_time;
  assert(check_quota);

  /* Find the time when this break should end = beginning of wait + type delay
     + break delay */
  xwADDTIME(break_end_time, *wait_began_time, type_delay);
  xwADDTIME(break_end_time, break_end_time, ocurrent->break_time);
  
  /* Find the length of this break */
  xwSUBTIME(this_break_time, ocurrent->break_time, quota_allotment);
  if (xwTIMEGEQ(ocurrent->min_break_time, this_break_time))
    this_break_time = ocurrent->min_break_time;
  
  /* Subtract to find when we should start warning */
  xwSUBTIME(break_end_time, break_end_time, this_break_time);
  
  /* Check against wait_over_time; if <=, wait is over */
  if (xwTIMEGEQ(wait_over_time, break_end_time))
    return TRAN_WARN;
  
  /* Set wait_over_time and return 0 -- we still need to wait */
  wait_over_time = break_end_time;
  return 0;
}

int
wait_for_break(void)
{
  int val;
  struct timeval wait_began_time;

  /* Schedule wait_over_time */
  xwGETTIME(wait_began_time);
  xwADDTIME(wait_over_time, wait_began_time, type_delay);
  if (check_quota)
    xwSETTIME(quota_allotment, 0, 0);

  /* Pretend there's been a keystroke */
  last_key_time = wait_began_time;
  
  val = 0;
  while (val != TRAN_WARN && val != TRAN_REST) {
    /* If !check_idle, we want to appear even if no keystroke happens, so
       schedule an alarm */
    if (!check_idle) {
      Alarm *a = new_alarm(A_AWAKE);
      a->timer = wait_over_time;
      unschedule(A_AWAKE);
      schedule(a);
    }
    
    /* Wait */
    val = loopmaster(0, wait_x_loop);
    if (val == TRAN_AWAKE) val = TRAN_WARN; /* patch A_AWAKE case */
    
    /* Adjust the wait time if necessary */
    assert(val == TRAN_WARN || val == TRAN_REST);
    if (val == TRAN_WARN && check_quota)
      val = adjust_wait_time(&wait_began_time);
  }
  
  unschedule(A_AWAKE);
  return val;
}


/* rest */

static int current_cheats;
static struct timeval break_over_time;

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
  struct timeval this_break_time;
  Alarm *a;
  int tran;
  
  /* determine length of this break. usually break_time; can be different if
     check_quota is on */
  this_break_time = ocurrent->break_time;
  if (check_quota) {
    xwSUBTIME(this_break_time, this_break_time, quota_allotment);
    if (xwTIMEGEQ(ocurrent->min_break_time, this_break_time))
      this_break_time = ocurrent->min_break_time;
  }
  
  /* determine when to end the break */
  xwGETTIME(now);
  if (!check_idle)
    xwADDTIME(break_over_time, now, this_break_time);
  else
    xwADDTIME(break_over_time, last_key_time, this_break_time);
  
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


/* ready */

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


/* unmap all windows */

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
