#include "config.h"
#include "xwrits.h"
#include <math.h>

static int clock_displaying = 0;


static void
pop_up_hand(Hand *h)
{
  set_picture(h, ocurrent->slideshow, 0);
  XMapRaised(display, h->w);
  XFlush(display);
  if (h->slideshow->nslides > 1) {
    Alarm *a = new_alarm(A_FLASH);
    unschedule_data(A_FLASH, h);
    a->data = h;
    xwGETTIME(a->timer);
    xwADDTIME(a->timer, a->timer, h->slideshow->delay[h->slide]);
    schedule(a);
  }
  if (ocurrent->top) {
    Alarm *a = new_alarm(A_RAISE);
    a->data = h;
    xwGETTIME(a->timer);
    xwADDTIME(a->timer, a->timer, ocurrent->top_delay);
    schedule(a);
  }
}


static int
switch_options(Options *opt, struct timeval now)
{
  Hand *h;
  Alarm *a;
  int need_refresh = 0;
  
  ocurrent = opt;
  
  blend_slideshow(opt->slideshow);
  
  if (opt->multiply) {
    a = new_alarm(A_MULTIPLY);
    xwADDTIME(a->timer, now, opt->multiply_delay);
    schedule(a);
  } else unschedule(A_MULTIPLY);
  
  if (opt->clock && !clock_displaying) {
    draw_clock(&now);
    a = new_alarm(A_CLOCK);
    xwADDTIME(a->timer, now, clock_tick);
    schedule(a);
    clock_displaying = 1;
    need_refresh = 1;
  } else if (!opt->clock && clock_displaying) {
    unschedule(A_CLOCK);
    erase_clock();
    clock_displaying = 0;
    need_refresh = 1;
  }

  if (active_hands == 0)
    need_refresh = 0;
  for (h = hands; h; h = h->next)
    if ((opt->never_iconify && !h->mapped) ||
	(opt->top && h->mapped) ||
	(!opt->appear_iconified && h->iconified))
      XMapRaised(display, h->w);
  
  if (!opt->top) unschedule(A_RAISE);
  /* for (a = alarms; a; a = a->next)
    if (a->action == A_FLASH && opt->neverobscured)
      a->action = A_FLASH_RAISE;
    else if (a->action == A_FLASH_RAISE && !opt->neverobscured)
      a->action = A_FLASH; */
  
  if (opt->beep)
    XBell(display, 0);

  if (need_refresh)
    refresh_hands();
  
  XFlush(display);
  
  if (opt->next) {
    a = new_alarm(A_NEXT_OPTIONS);
    xwADDTIME(a->timer, now, opt->next_delay);
    schedule(a);
  }
  
  if (opt->lock)
    return TRAN_LOCK;
  else
    return 0;
}


static int
warning_alarm_loop(Alarm *a, struct timeval *now)
{
  switch (a->action) {
    
   case A_MULTIPLY:
    if (active_hands < ocurrent->max_hands)
      pop_up_hand(new_hand(NHRandom, NHRandom));
    xwADDTIME(a->timer, a->timer, ocurrent->multiply_delay);
    schedule(a);
    break;
    
   case A_NEXT_OPTIONS:
    return switch_options(ocurrent->next, *now);
    
   case A_IDLE_CHECK:
    return TRAN_REST;
    
  }
  return 0;
}


static int
warning_x_loop(XEvent *e)
{
  Alarm *a;
  switch (e->type) {
    
   case UnmapNotify:
    if (ocurrent->never_iconify && window_to_hand(e->xunmap.window))
      XMapRaised(display, e->xunmap.window);
    break;
    
   case ButtonPress:
    /* OK; we can rest now. */
    return TRAN_REST;
    
   case ClientMessage:
    /* Window manager deleted the only xwrits window. Disappear, but
       come back later. */
    return TRAN_CANCEL;
    
   case KeyPress:
    if ((a = grab_alarm(A_IDLE_CHECK))) {
      xwGETTIME(last_key_time);
      xwADDTIME(a->timer, last_key_time, idle_time);
      schedule(a);
    }
    break;
    
  }
  return 0;
}


int
warning(int was_lock)
{
  int val;
  
  xwGETTIME(clock_zero_time);
  val = switch_options(ocurrent, clock_zero_time);
  if (val == TRAN_LOCK && !was_lock)
    return TRAN_LOCK;
  pop_up_hand(hands);
  
  if (check_idle && !xwTIMELEQ0(idle_time)) {
    Alarm *a = new_alarm(A_IDLE_CHECK);
    xwADDTIME(a->timer, last_key_time, idle_time);
    schedule(a);
  }
  
  val = loopmaster(warning_alarm_loop, warning_x_loop);
  unschedule(A_FLASH | A_RAISE | A_MULTIPLY | A_CLOCK | A_NEXT_OPTIONS
	     | A_IDLE_CHECK);
  
  return val;
}
