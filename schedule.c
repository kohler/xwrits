#include <config.h>
#include "xwrits.h"
#include <stdlib.h>
#include <stdio.h>


Alarm *alarms;


/*****************************************************************************/
/*  Idle functions							     */

/* Idea and motivation from xautolock, by Stefan De Troch, detroch@imec.be, and
   Michel Eyckmans (MCE), eyckmans@imec.be. However, the algorithm presented
   here is a little different from theirs, and therefore probably doesn't
   work! */

/* Support for Xidle is *not* included. */

struct timeval register_keystrokes_delay;
struct timeval register_keystrokes_gap;
int check_idle;
static unsigned long created_count;
static unsigned long key_press_selected_count;


int
x_error_handler(Display *d, XErrorEvent *error)
{
  if (d != display ||
      (error->error_code != BadWindow && error->error_code != BadDrawable)) {
    char buffer[BUFSIZ];
    XGetErrorText(display, error->error_code, buffer, BUFSIZ);
    fprintf(stderr, "X Error of failed request: %s\n", buffer);
    abort();
  }
  
  /* Maybe someone created a window then destroyed it immediately!
     I don't think there's any way of working around this. */
  unschedule_data(A_IDLE_SELECT, (void *)((Window)error->resourceid));
  return 0;
}


void
watch_keystrokes(Window w, struct timeval *now)
{
  Window root, parent, *children;
  unsigned i, nchildren;
  static struct timeval next_watch_keystrokes;
  Alarm *a;
  
  if (window_to_hand(w) || icon_window_to_hand(w))
    return; /* don't need to worry about our own windows */
  
  if (XQueryTree(display, w, &root, &parent, &children, &nchildren) == 0)
    return; /* the window doesn't exist */
  
  XSelectInput(display, w, SubstructureNotifyMask);
  created_count++;
  
  /* Better processing for the creation of new windows: we don't want to
     do idle processing for all of them all at once, in case we are doing
     something else as well (e.g., "spectacular effects").
     This code ensures that:
     - at least register_keystrokes_delay elapses before selection is done
       on the window. We want to wait so we can make sure the window
       selects for KeyPress events.
     - at least register_keystrokes_gap elapses between selection on any
       2 windows. */
  a = new_alarm_data(A_IDLE_SELECT, (void *)w);
  xwADDTIME(a->timer, *now, register_keystrokes_delay);
  if (xwTIMEGT(a->timer, next_watch_keystrokes))
    next_watch_keystrokes = a->timer;
  else
    a->timer = next_watch_keystrokes;
  schedule(a);
  xwADDTIME(next_watch_keystrokes, next_watch_keystrokes,
	    register_keystrokes_gap);
  
  for (i = 0; i < nchildren; i++)
    watch_keystrokes(children[i], now);
  
  if (children) XFree(children);
}


void
register_keystrokes(Window w)
{
  /* Before I only selected KeyPress if someone else had selected KeyPress on
     the window (indicated by the `or' of all_event_masks and
     do_not_propagate_mask). That seems obviously wrong! What if they only
     selected KeyRelease? What if they selected KeyPress later? So I've
     removed it. Hopefully this won't cause any strange behavior. */
  
  /* Even though I didn't test long enough to see if it causes strange
     behavior, I'm going back to the old method (we select KeyPress only if
     someone else could receive it); this is a good idea because there are so
     many window manager windows which otherwise we would select events on,
     and not selecting events on 'em would seem to help performance. */
  
  XWindowAttributes attr;
  if (XGetWindowAttributes(display, w, &attr) == 0)
    return;
  if (attr.root == w
      || ((attr.all_event_masks | attr.do_not_propagate_mask)
	  & KeyPressMask)) {
    key_press_selected_count++;
    XSelectInput(display, w, SubstructureNotifyMask | KeyPressMask);
  }
}


/*****************************************************************************/
/*  Scheduling and alarm functions					     */


Alarm *
new_alarm_data(int action, void *data)
{
  Alarm *a = xwNEW(Alarm);
  a->action = action;
  a->data = data;
  return a;
}


Alarm *
grab_alarm_data(int action, void *data)
{
  Alarm *prev = 0, *a;
  for (a = alarms; a; prev = a, a = a->next)
    if (a->action == action && a->data == data) {
      if (prev)	prev->next = a->next;
      else alarms = a->next;
      return a;
    }
  return 0;
}


void
destroy_alarm(Alarm *adestroy)
{
  Alarm *a;
  if (alarms == adestroy) alarms = adestroy->next;
  for (a = alarms; a; a = a->next)
    if (a->next == adestroy)
      a->next = adestroy->next;
  xfree(adestroy);
}


void
schedule(Alarm *newalarm)
{
  Alarm *prev = 0, *a = alarms;
  while (a && xwTIMEGEQ(newalarm->timer, a->timer)) {
    prev = a;
    a = a->next;
  }
  if (prev) prev->next = newalarm;
  else alarms = newalarm;
  newalarm->next = a;
  newalarm->scheduled = 1;
}


void
unschedule_data(int actions, void *data)
{
  Alarm *prev = 0, *a = alarms;
  while (a) {
    
    if ((a->action & actions) != 0 && (a->data == data || data == 0)) {
      Alarm *n = a->next;
      if (prev) prev->next = n;
      else alarms = n;
      xfree(a);
      a = n;
      continue;
    }
    
    prev = a;
    a = a->next;
  }
}


int
loopmaster(Alarmloopfunc alarm_looper, Xloopfunc x_looper)
{
  struct timeval timeout, now, *timeoutptr;
  fd_set xfds;
  int pending;
  int ret_val = 0;

  /* 26 May 1998: Changed logic to avoid race conditions. Now we always flush
     the output queue and check if there are any pending X events before
     entering select() to wait for data. I wouldn't have noticed this, since
     the effects were so transient, if it hadn't been for the --animate option
     to gifview, which exercised this code more strenuously. */
  
  xwGETTIME(now);
  FD_ZERO(&xfds);
  
  while (1) {
    while (alarms && xwTIMEGEQ(now, alarms->timer)) {
      Alarm *a = alarms;
      Hand *h = (Hand *)a->data;
      Slideshow *ss;
      
      a->scheduled = 0;
      alarms = a->next;
      
      switch (a->action) {
	
       case A_RAISE:
	if (h->mapped && h->obscured) {
	  XMapRaised(display, h->w);
	  h->obscured = 0;
	}
	while (xwTIMEGEQ(now, a->timer))
	  xwADDTIME(a->timer, a->timer, ocurrent->top_delay);
	schedule(a);
	break;
	
       case A_FLASH:
	ss = h->slideshow;
	while (xwTIMEGEQ(now, a->timer)) {
	  h->slide = (h->slide + 1) % ss->nslides;
	  xwADDTIME(a->timer, a->timer, ss->delay[h->slide]);
	}
	set_picture(h, ss, h->slide);
	schedule(a);
	break;
	
       case A_CLOCK:
	draw_clock(&now);
	refresh_hands();
	xwADDTIME(a->timer, a->timer, clock_tick);
	schedule(a);
	break;
	
       case A_AWAKE:
	ret_val = TRAN_AWAKE;
	break;
	
       case A_IDLE_SELECT:
	register_keystrokes((Window)a->data);
	break;
	
       default:
	ret_val = alarm_looper(a, &now);
	break;
	
      }
      
      if (!a->scheduled) xfree(a);
      if (ret_val != 0) return ret_val;
    }
    
    if (alarms) {
      timeoutptr = &timeout;
      xwSUBTIME(timeout, alarms->timer, now);
    } else
      timeoutptr = 0;
    
    pending = XPending(display);
    if (!pending) {
      int result;
      FD_SET(port.x_socket, &xfds);
      result = select(port.x_socket + 1, &xfds, 0, 0, timeoutptr);
      pending = (result <= 0 ? 0 : FD_ISSET(port.x_socket, &xfds));
    }
    
    if (pending)
      while (XPending(display)) {
	XEvent event;
	XNextEvent(display, &event);
	default_x_processing(&event);
	if (x_looper) ret_val = x_looper(&event);
	if (ret_val != 0) return ret_val;
      }
    
    /* Behave robustly when the system clock is adjusted backwards. The idea:
       estimate the duration of the backwards jump and subtract that from
       genesis_time. This will compensate for the jump in any new times
       returned from xwGETTIME. */
    {
      struct timeval new_now;
      xwGETTIME(new_now);
      if (xwTIMEGT(now, new_now)) {
	xwSUBTIME(new_now, now, new_now);
	if (timeoutptr) xwSUBTIME(new_now, new_now, timeout);
	xwSUBTIME(genesis_time, genesis_time, new_now);
	xwGETTIME(new_now);
      }
      now = new_now;
    }
  }
}
