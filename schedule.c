#include <config.h>
#include "xwrits.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

static Alarm alarm_sentinel;

/*****************************************************************************/
/*  Idle functions							     */

/* Idea and motivation from xautolock, by Stefan De Troch, detroch@imec.be, and
   Michel Eyckmans (MCE), eyckmans@imec.be. However, the algorithm presented
   here is a little different from theirs, and therefore probably doesn't
   work! */

/* Support for Xidle is *not* included. */

struct timeval register_keystrokes_delay;
static unsigned long created_count;
static unsigned long key_press_selected_count;


int
x_error_handler(Display *d, XErrorEvent *error)
{
  if (error->error_code != BadWindow && error->error_code != BadDrawable) {
    char buffer[BUFSIZ];
    XGetErrorText(d, error->error_code, buffer, BUFSIZ);
    fprintf(stderr, "X Error of failed request: %s\n", buffer);
    abort();
  }
  
  /* Maybe someone created a window then destroyed it immediately!
     I don't think there's any way of working around this. */
  if (error->error_code == BadWindow) {
    Window window = (Window)error->resourceid;
    unschedule_data(A_IDLE_SELECT, (void *)find_port(error->display, window),
		    (void *)window);
  }
  return 0;
}


void
watch_keystrokes(Port *port, Window w, const struct timeval *now)
{
  Display *display = port->display;
  Window root, parent, *children;
  unsigned i, nchildren;
  Alarm *a;
  
  /* Don't pay attention to our own windows */
  if (window_to_hand(port, w, 1))
    return;
  
  if (XQueryTree(display, w, &root, &parent, &children, &nchildren) == 0)
    return; /* the window doesn't exist */
  
  XSelectInput(display, w, SubstructureNotifyMask);
  created_count++;
  if (verbose)
      fprintf(stderr, "Window 0x%x: watching for subwindows\n", (unsigned)w);
  
  /* This code ensures that at least register_keystrokes_delay elapses before
     we listen for KeyPress events on the window. We want to wait so we can
     make sure the window selects them first. */
  a = new_alarm_data(A_IDLE_SELECT, (void *)port, (void *)w);
  xwADDTIME(a->timer, *now, register_keystrokes_delay);
  schedule(a);
  
  for (i = 0; i < nchildren; i++)
    watch_keystrokes(port, children[i], now);
  
  if (children) XFree(children);
}

void
register_keystrokes(Port *port, Window w)
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
  Window peer;
  
  if (XGetWindowAttributes(port->display, w, &attr) == 0)
    return;

  /* check if this is an xwrits window */
  peer = check_xwrits_window(port, w);
  if (peer)
    add_peer(port, peer);
  
  if (attr.root == w
      || ((attr.all_event_masks | attr.do_not_propagate_mask)
	  & (KeyPressMask | KeyReleaseMask))) {
    key_press_selected_count++;
    XSelectInput(port->display, w, SubstructureNotifyMask | KeyPressMask);
    if (verbose)
      fprintf(stderr, "Window 0x%x: listening for keystrokes\n", (unsigned)w);
  } else if (verbose)
    fprintf(stderr, "Window 0x%x: skipping keystrokes\n", (unsigned)w);
}


/*****************************************************************************/
/*  Scheduling and alarm functions					     */

Alarm *
new_alarm_data(int action, void *data1, void *data2)
{
  Alarm *a = xwNEW(Alarm);
  a->action = action;
  a->data1 = data1;
  a->data2 = data2;
  a->scheduled = 0;
  return a;
}

Alarm *
grab_alarm_data(int action, void *data1, void *data2)
{
  Alarm *a;
  for (a = alarm_sentinel.next; a != &alarm_sentinel; a = a->next)
    if (a->action == action && a->data1 == data1 && a->data2 == data2) {
      a->prev->next = a->next;
      a->next->prev = a->prev;
      a->scheduled = 0;
      return a;
    }
  return 0;
}

void
destroy_alarm(Alarm *a)
{
  if (a->scheduled) {
    a->prev->next = a->next;
    a->next->prev = a->prev;
  }
  xfree(a);
}


void
init_scheduler(void)
{
  alarm_sentinel.next = alarm_sentinel.prev = &alarm_sentinel;
}

void
schedule(Alarm *newalarm)
{
  Alarm *a = alarm_sentinel.prev;
  assert(!newalarm->scheduled);
  while (a != &alarm_sentinel && xwTIMEGT(a->timer, newalarm->timer))
    a = a->prev;
  newalarm->prev = a;
  newalarm->next = a->next;
  a->next->prev = newalarm;
  a->next = newalarm;
  newalarm->scheduled = 1;
}

void
unschedule_data(int actions, void *data1, void *data2)
{
  Alarm *a = alarm_sentinel.next;
  while (a != &alarm_sentinel) {
    Alarm *n = a->next;
    
    if ((a->action & actions) != 0
	&& (a->data1 == data1 || data1 == 0)
	&& (a->data2 == data2 || data2 == 0)) {
      a->prev->next = n;
      n->prev = a->prev;
      xfree(a);
    }
    
    a = n;
  }
}


int
loopmaster(Alarmloopfunc alarm_looper, Xloopfunc x_looper)
{
  struct timeval timeout, now, *timeoutptr;
  fd_set xfds;
  int pending, i;
  int ret_val = 0;
  
  /* 26 May 1998: Changed logic to avoid race conditions. Now we always flush
     the output queue and check if there are any pending X events before
     entering select() to wait for data. I wouldn't have noticed this, since
     the effects were so transient, if it hadn't been for the --animate option
     to gifview, which exercised this code more strenuously. */
  
  xwGETTIME(now);
  FD_ZERO(&xfds);
  
  while (1) {
    while (1) {
      Alarm *a = alarm_sentinel.next;
      Hand *h;
      Gif_Stream *gfs;

      if (a == &alarm_sentinel || xwTIMEGT(a->timer, now))
	break;

      h = (Hand *)a->data1;
      alarm_sentinel.next = a->next;
      a->next->prev = &alarm_sentinel;
      a->scheduled = 0;
      
      switch (a->action) {
	
       case A_FLASH:
	gfs = h->slideshow;
	/* cycle through slides */
	while (xwTIMEGEQ(now, a->timer)) {
	  h->slide = (h->slide + 1) % gfs->nimages;
	  xwADDDELAY(a->timer, a->timer, gfs->images[h->slide]->delay);
	  /* handle loopcount */
	  if (h->slide == 0 && gfs->loopcount != 0
	      && ++h->loopcount > gfs->loopcount) {
	    h->slide = gfs->nimages - 1;
	    goto flash_draw;
	  }
	}
	schedule(a);
       flash_draw:
	if (h->mapped) draw_slide(h);
	break;
	
       case A_CLOCK:
	draw_all_clocks(&now);
	xwADDTIME(a->timer, a->timer, clock_tick);
	schedule(a);
	break;
	
       case A_AWAKE:
	ret_val = TRAN_AWAKE;
	break;
	
       case A_IDLE_SELECT:
	register_keystrokes((Port *)a->data1, (Window)a->data2);
	break;
	
       case A_MOUSE: {
	 Window root, child;
	 int root_x, root_y, win_x, win_y;
	 unsigned mask;
	 int i;
	 for (i = 0; i < nports; i++) {
	   if (ports[i]->master != ports[i])
	     continue;
	   XQueryPointer(ports[i]->display, ports[i]->root_window, &root,
			 &child, &root_x, &root_y, &win_x, &win_y, &mask);
	   if (root != ports[i]->last_mouse_root
	       || root_x < ports[i]->last_mouse_x - mouse_sensitivity
	       || root_x > ports[i]->last_mouse_x + mouse_sensitivity
	       || root_y < ports[i]->last_mouse_y - mouse_sensitivity
	       || root_y > ports[i]->last_mouse_y + mouse_sensitivity) {
	     XEvent event;
	     event.type = MotionNotify; /* skeletal MotionNotify event */
	     if (x_looper && ports[i]->last_mouse_root)
	       ret_val = x_looper(&event, &now);
	     ports[i]->last_mouse_root = root;
	     ports[i]->last_mouse_x = root_x;
	     ports[i]->last_mouse_y = root_y;
	   }
	 }
	 xwADDTIME(a->timer, a->timer, check_mouse_time);
	 schedule(a);
	 break;
       }
       
       default:
	if (alarm_looper)
	  ret_val = alarm_looper(a, &now);
	break;
	
      }
      
      if (!a->scheduled) xfree(a);
      if (ret_val != 0) return ret_val;
    }
    
    if (alarm_sentinel.next != &alarm_sentinel) {
      timeoutptr = &timeout;
      xwSUBTIME(timeout, alarm_sentinel.next->timer, now);
    } else
      timeoutptr = 0;

    for (i = pending = 0; !pending && i < nports; i++)
      pending = XPending(ports[i]->display);
    if (!pending) {
      int result;
      xfds = x_socket_set;
      result = select(max_x_socket + 1, &xfds, 0, 0, timeoutptr);
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
	xwSUBTIME(genesis_time, genesis_time, new_now);
	xwGETTIME(new_now);
      }
      now = new_now;
    }
    
    /* Handle X events. */
    for (i = 0; i < nports; i++)
      while (XPending(ports[i]->display)) {
	XEvent event;
	XNextEvent(ports[i]->display, &event);
	default_x_processing(&event);
	if (x_looper)
	    ret_val = x_looper(&event, &now);
	if (ret_val != 0)
	    return ret_val;
      }
  }
}
