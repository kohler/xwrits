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
    Alarm *a = new_alarm(Flash);
    unschedule_data(Flash, h);
    a->data = h;
    xwGETTIME(a->timer);
    xwADDTIME(a->timer, a->timer, h->slideshow->delay[h->slide]);
    schedule(a);
  }
  if (ocurrent->top) {
    Alarm *a = new_alarm(Raise);
    a->data = h;
    xwGETTIME(a->timer);
    xwADDTIME(a->timer, a->timer, ocurrent->top_delay);
    schedule(a);
  }
}


static int
switch_options(Options *opt, struct timeval now, int lockfailed)
{
  Hand *h;
  Alarm *a;
  int need_refresh = 0;
  
  ocurrent = opt;
  
  blend_slideshow(opt->slideshow);
  
  if (opt->multiply) {
    a = new_alarm(Multiply);
    xwADDTIME(a->timer, now, opt->multiply_delay);
    schedule(a);
  } else unschedule(Multiply);
  
  if (opt->clock && !clock_displaying) {
    draw_clock(&now);
    a = new_alarm(Clock);
    xwADDTIME(a->timer, now, clock_tick);
    schedule(a);
    clock_displaying = 1;
    need_refresh = 1;
  } else if (!opt->clock && clock_displaying) {
    unschedule(Clock);
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
  
  if (!opt->top) unschedule(Raise);
  /* for (a = alarms; a; a = a->next)
    if (a->action == Flash && opt->neverobscured)
      a->action = FlashRaise;
    else if (a->action == FlashRaise && !opt->neverobscured)
      a->action = Flash; */
  
  if (opt->beep)
    XBell(display, 0);

  if (need_refresh)
    refresh_hands();
  
  XFlush(display);
  
  if (opt->next) {
    a = new_alarm(NextOptions);
    xwADDTIME(a->timer, now, opt->next_delay);
    schedule(a);
  }
  
  if (opt->lock && !lockfailed)
    return WarnLock;
  else
    return 0;
}


static int
warning_alarm_loop(Alarm *a, struct timeval *now)
{
  switch (a->action) {
    
   case Multiply:
    if (active_hands < ocurrent->max_hands)
      pop_up_hand(new_hand(NHRandom, NHRandom));
    xwADDTIME(a->timer, a->timer, ocurrent->multiply_delay);
    schedule(a);
    break;
    
   case NextOptions:
    return switch_options(ocurrent->next, *now, 0);
    
   case IdleCheck:
    return WarnRest;
    
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
    return WarnRest;
    
   case ClientMessage:
    /* Window manager deleted the only xwrits window. Disappear, but
       come back later. */
    return WarnCancelled;
    
   case KeyPress:
    if ((a = grab_alarm(IdleCheck))) {
      xwGETTIME(last_key_time);
      xwADDTIME(a->timer, last_key_time, idle_time);
      schedule(a);
    }
    break;
    
  }
  return 0;
}


int
warning(int lockfailed)
{
  int val;
  
  xwGETTIME(clock_zero_time);
  val = switch_options(ocurrent, clock_zero_time, lockfailed);
  if (val == WarnLock) return val;
  pop_up_hand(hands);
  
  if (check_idle && !xwTIMELEQ0(idle_time)) {
    Alarm *a = new_alarm(IdleCheck);
    xwADDTIME(a->timer, last_key_time, idle_time);
    schedule(a);
  }
  
  val = loopmaster(warning_alarm_loop, warning_x_loop);
  unschedule(Flash | Raise | Multiply | Clock | NextOptions | IdleCheck);
  
  return val;
}
