#include <config.h>
#include "xwrits.h"
#include <math.h>

static int clock_displaying = 0;


static void
pop_up_hand(Hand *h)
{
  set_slideshow(h, ocurrent->slideshow, 0);
  XMapRaised(display, h->w);
  XFlush(display);
  if (clock_displaying)
    draw_clock(h, 0);
}


static int
switch_options(Options *opt, struct timeval now)
{
  Hand *h;
  Alarm *a;
  
  ocurrent = opt;
  
  set_all_slideshows(hands, opt->slideshow);
  set_all_slideshows(icon_hands, opt->icon_slideshow);
  
  if (opt->multiply) {
    a = new_alarm(A_MULTIPLY);
    xwADDTIME(a->timer, now, opt->multiply_delay);
    schedule(a);
  } else unschedule(A_MULTIPLY);
  
  if (opt->clock && !clock_displaying) {
    draw_all_clocks(&now);
    a = new_alarm(A_CLOCK);
    xwADDTIME(a->timer, now, clock_tick);
    schedule(a);
    clock_displaying = 1;
  } else if (!opt->clock && clock_displaying) {
    unschedule(A_CLOCK);
    erase_all_clocks();
    clock_displaying = 0;
  }
  
  for (h = hands; h; h = h->next)
    if ((opt->never_iconify && !h->mapped) ||
	(!opt->appear_iconified && h->icon->mapped))
      XMapRaised(display, h->w);
  
  if (opt->beep)
    XBell(display, 0);

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
    if (active_hands() < ocurrent->max_hands)
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
check_raise_window(Hand *h)
{
  Window root, parent, *children = 0;
  unsigned nchildren;
  unsigned i;
  int hx, hy, ox, oy;
  int hwidth, hheight, owidth, oheight;
  unsigned border, depth;
  Hand *trav;
  int bad_overlap;
  
  /* find our geometry */
  XGetGeometry(display, h->root_child, &root, &hx, &hy,
	       (unsigned *)&hwidth, (unsigned *)&hheight, &border, &depth);
  
  /* find our position in the stacking order */
  if (!XQueryTree(display, port.root_window, &root, &parent,
		  &children, &nchildren)) {
    if (children) XFree(children);
    return 0;
  }
  for (i = 0; i < nchildren; i++)
    if (children[i] == h->root_child)
      break;
  
  /* examine higher windows in the stacking order */
  bad_overlap = 0;
  for (i++; i < nchildren && !bad_overlap; i++) {
    /* overlap by other hands is OK */
    for (trav = hands; trav; trav = trav->next)
      if (children[i] == trav->root_child)
	goto overlap_ok;
    /* check to see that this window actually overlaps us */
    XGetGeometry(display, children[i], &root, &ox, &oy,
		 (unsigned *)&owidth, (unsigned *)&oheight, &border, &depth);
    if (ox > hx + hwidth || oy > hy + hheight
	|| ox + owidth < hx || oy + oheight < hy)
      goto overlap_ok;
    /* if we get here, we found an illegal overlap */
    bad_overlap = 1;
   overlap_ok: ;
  }
  
  if (children) XFree(children);
  return bad_overlap;
}

static int
warning_x_loop(XEvent *e, struct timeval *now)
{
  Alarm *a;
  Hand *h;
  switch (e->type) {
    
   case MapNotify:
    h = window_to_hand(e->xunmap.window, 1);
    if (h && h->is_icon && ocurrent->never_iconify)
      XMapRaised(display, h->icon->w);
    break;
    
   case ClientMessage:
    /* Check for window manager deleting last xwrits window */
    if (active_hands() == 0)
      return TRAN_CANCEL;
    break;
    
   case ButtonPress:
    /* OK; we can rest now. */
    return TRAN_REST;
    
   case KeyPress:
    if ((a = grab_alarm(A_IDLE_CHECK))) {
      last_key_time = *now;
      xwADDTIME(a->timer, last_key_time, idle_time);
      schedule(a);
    }
    break;
    
   case VisibilityNotify:
    if ((h = window_to_hand(e->xvisibility.window, 0)) && h->obscured
	&& ocurrent->top)
      if (check_raise_window(h))
	XRaiseWindow(display, h->w);
    break;
    
  }
  return 0;
}


int
warning(int was_lock)
{
  struct timeval now;
  int val;
  
  clock_displaying = 0;
  clock_zero_time = first_warning_time;
  
  xwGETTIME(now);
  val = switch_options(ocurrent, now);
  if (val == TRAN_LOCK && !was_lock)
    return TRAN_LOCK;
  
  pop_up_hand(hands);
  
  if (check_idle && !xwTIMELEQ0(idle_time)) {
    Alarm *a = new_alarm(A_IDLE_CHECK);
    xwADDTIME(a->timer, last_key_time, idle_time);
    schedule(a);
  }
  
  val = loopmaster(warning_alarm_loop, warning_x_loop);
  unschedule(A_FLASH | A_MULTIPLY | A_CLOCK | A_NEXT_OPTIONS | A_IDLE_CHECK);
  
  erase_all_clocks();
  return val;
}
