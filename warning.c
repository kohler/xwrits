#include <config.h>
#include "xwrits.h"
#include <math.h>
#include <assert.h>
#include <X11/Xatom.h>

static int clock_displaying = 0;


static void
pop_up_hand(Hand *h)
{
  assert(h && !h->is_icon);
  set_slideshow(h, ocurrent->slideshow, 0);
  set_slideshow(h->icon, ocurrent->icon_slideshow, 0);
  h->clock = ocurrent->clock;
  XMapRaised(h->port->display, h->w);
  XFlush(h->port->display);
}

static void
ensure_slave_port_hand(Port *p)
{
    Hand *h;
    for (h = p->master->hands; h; h = h->next)
	if (h->x >= p->left && h->x < p->left + p->width
	    && h->y >= p->top && h->y < p->top + p->height) {
	    pop_up_hand(h);
	    return;
	}
    (void) new_hand(p, NEW_HAND_CENTER, NEW_HAND_CENTER);
}


static int
switch_options(Options *opt, const struct timeval *option_switch_time,
	       const struct timeval *now)
{
  Hand *h;
  Alarm *a;
  int i;
  
  ocurrent = opt;

  for (i = 0; i < nports; i++) {
    set_all_slideshows(ports[i]->hands, opt->slideshow);
    set_all_slideshows(ports[i]->icon_hands, opt->icon_slideshow);
  }
  
  if (opt->multiply) {
    a = new_alarm(A_MULTIPLY);
    xwADDTIME(a->timer, *now, opt->multiply_delay);
    schedule(a);
  } else
    unschedule(A_MULTIPLY);
  
  if (opt->clock && !clock_displaying) {
    draw_all_clocks(now);
    a = new_alarm(A_CLOCK);
    xwADDTIME(a->timer, *now, clock_tick);
    schedule(a);
    clock_displaying = 1;
  } else if (!opt->clock && clock_displaying) {
    unschedule(A_CLOCK);
    erase_all_clocks();
    clock_displaying = 0;
  }

  for (i = 0; i < nports; i++) {
    for (h = ports[i]->hands; h; h = h->next)
      if ((opt->never_iconify && !h->mapped) ||
	  (!opt->appear_iconified && h->icon->mapped))
	XMapRaised(ports[i]->display, h->w);
    if (opt->beep && ports[i]->master == ports[i])
      XBell(ports[i]->display, 0);
    XFlush(ports[i]->display);
  }
  
  if (opt->next) {
    a = new_alarm(A_NEXT_OPTIONS);
    xwADDTIME(a->timer, *option_switch_time, opt->next_delay);
    schedule(a);
  }
  
  if (opt->lock)
    return TRAN_LOCK;
  else
    return 0;
}


static int
warn_alarm_loop(Alarm *a, const struct timeval *now)
{
  switch (a->action) {
    
   case A_MULTIPLY:
    if (active_hands() < ocurrent->max_hands * nports)
      pop_up_hand(new_hand(NEW_HAND_RANDOM_PORT, NEW_HAND_RANDOM, NEW_HAND_RANDOM));
    xwADDTIME(a->timer, a->timer, ocurrent->multiply_delay);
    schedule(a);
    break;
    
   case A_NEXT_OPTIONS:
    return switch_options(ocurrent->next, now, now);
    
   case A_IDLE_CHECK:
    return TRAN_REST;
    
  }
  return 0;
}


static int
check_raise_window(Hand *h)
{
  Port *port = h->port;
  Window root, parent, *children = 0;
  unsigned nchildren;
  unsigned i;
  int hx, hy, ox, oy;
  int hwidth, hheight, owidth, oheight;
  unsigned border, depth;
  Hand *trav;
  int bad_overlap;
  
  /* find our geometry */
  XGetGeometry(port->display, h->root_child, &root, &hx, &hy,
	       (unsigned *)&hwidth, (unsigned *)&hheight, &border, &depth);
  
  /* find our position in the stacking order */
  if (!XQueryTree(port->display, port->root_window, &root, &parent,
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
    for (trav = port->hands; trav; trav = trav->next)
      if (children[i] == trav->root_child)
	goto overlap_ok;
    /* check to see that this window actually overlaps us */
    XGetGeometry(port->display, children[i], &root, &ox, &oy,
		 (unsigned *)&owidth, (unsigned *)&oheight, &border, &depth);
    if (ox > hx + hwidth || oy > hy + hheight
	|| ox + owidth < hx || oy + oheight < hy)
      goto overlap_ok;
    /* check to see if it belongs to another xwrits process */
    if (check_xwrits_window(port, children[i]))
      goto overlap_ok;
    /* if we get here, we found an illegal overlap */
    bad_overlap = 1;
   overlap_ok: ;
  }
  
  if (children)
    XFree(children);
  return bad_overlap;
}

static int
warn_x_loop(XEvent *e, const struct timeval *now)
{
  Alarm *a;
  Hand *h;
  switch (e->type) {
    
   case MapNotify: {
     Port *port = find_port(e->xunmap.display, e->xunmap.window);
     h = window_to_hand(port, e->xunmap.window, 1);
     if (h && h->is_icon && ocurrent->never_iconify)
       XMapRaised(port->display, h->icon->w);
     break;
   }
   
   case Xw_DeleteWindow:
    /* Check for window manager deleting last xwrits window */
    if (active_hands() == 0)
      return TRAN_CANCEL;
    break;
    
   case ButtonPress:
    /* OK; we can rest now. */
    /* 10.Aug.1999 - treat the mouse click as a keypress */
    last_key_time = *now;
    /* 18.Apr.2001 - explicitly inform peers */
    notify_peers_rest();
    return TRAN_REST;

   case Xw_TakeBreak:
    /* informed that a peer is resting */
    last_key_time = *now;
    return TRAN_REST;
    
   case KeyPress:
   case MotionNotify:
    last_key_time = *now;
    if ((a = grab_alarm(A_IDLE_CHECK))) {
      xwADDTIME(a->timer, last_key_time, warn_idle_time);
      schedule(a);
    }
    break;
    
   case VisibilityNotify: {
     Port *port = find_port(e->xvisibility.display, e->xvisibility.window);
     h = window_to_hand(port, e->xvisibility.window, 0);
     if (h && h->obscured && ocurrent->top)
       if (check_raise_window(h))
	 XRaiseWindow(port->display, h->w);
     break;
   }
   
  }
  return 0;
}


int
warn(int was_lock, Options *onormal)
{
  struct timeval now, option_switch_time;
  int i, val;
  
  clock_displaying = 0;
  clock_zero_time = first_warn_time;

  /* find correct options based on elapsed time since first_warn_time */
  xwGETTIME(now);
  ocurrent = onormal;
  option_switch_time = first_warn_time;
  while (ocurrent->next) {
    struct timeval next;
    xwADDTIME(next, option_switch_time, ocurrent->next_delay);
    if (xwTIMEGT(next, now))
      break;
    option_switch_time = next;
    ocurrent = ocurrent->next;
  }

  /* switch to those options */
  /* This will always set_slideshows: good -- later set_slideshows will start
     from scratch. */
  val = switch_options(ocurrent, &option_switch_time, &now);

  /* lock now if we should */
  if (val == TRAN_LOCK && !was_lock)
    goto done;

  for (i = 0; i < nports; i++)
      if (ports[i]->hands)
	  pop_up_hand(ports[i]->hands);
      else
	  ensure_slave_port_hand(ports[i]);
  
  if (check_idle) {
    Alarm *a = new_alarm(A_IDLE_CHECK);
    xwADDTIME(a->timer, last_key_time, warn_idle_time);
    schedule(a);
  }
  
  val = loopmaster(warn_alarm_loop, warn_x_loop);

 done:
  unschedule(A_FLASH | A_MULTIPLY | A_CLOCK | A_NEXT_OPTIONS | A_IDLE_CHECK);
  erase_all_clocks();
  assert(val == TRAN_CANCEL || val == TRAN_REST || val == TRAN_LOCK);
  return val;
}
