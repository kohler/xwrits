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

struct timeval idleselectdelay;
struct timeval idlegapdelay;
int checkidle;


int
xerrorhandler(Display *d, XErrorEvent *error)
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
  unscheduledata(IdleSelect, (void *)((Window)error->resourceid));
  return 0;
}


void
idlecreate(Window w, struct timeval *now)
{
  Window root, parent, *children;
  unsigned int nchildren;
  static struct timeval nextidlecreate;
  Alarm *a;
  int i;
  
  if (windowtohand(w) || iconwindowtohand(w))
    return; /* don't need to worry about our own windows */
  
  if (XQueryTree(display, w, &root, &parent, &children, &nchildren) == 0)
    return; /* the window doesn't exist */
  
  XSelectInput(display, w, SubstructureNotifyMask);
  
  /* Better processing for the creation of new windows: we don't want to
     do idle processing for all of them all at once, in case we are doing
     something else as well (e.g., "spectacular effects").
     This code ensures that:
     - at least idleselectdelay elapses before selection is done on the window.
     - at least idlegapdelay elapses between selection on any 2 windows. */
  a = newalarmdata(IdleSelect, (void *)w);
  xwADDTIME(a->timer, *now, idleselectdelay);
  if (xwTIMEGT(a->timer, nextidlecreate))
    nextidlecreate = a->timer;
  else
    a->timer = nextidlecreate;
  schedule(a);
  xwADDTIME(nextidlecreate, nextidlecreate, idlegapdelay);
  
  for (i = 0; i < nchildren; i++)
    idlecreate(children[i], now);
  
  if (children) XFree(children);
}


void
idleselect(Window w)
{
  XWindowAttributes attr;
  
  if (XGetWindowAttributes(display, w, &attr) == 0)
    return;
  if (attr.root == w)
    attr.all_event_masks = KeyPressMask;
  
  XSelectInput(display, w, SubstructureNotifyMask |
	       ((attr.all_event_masks | attr.do_not_propagate_mask)
		& KeyPressMask));
}


/*****************************************************************************/
/*  Scheduling and alarm functions					     */


Alarm *
newalarmdata(int action, void *data)
{
  Alarm *a = xwNEW(Alarm);
  a->action = action;
  a->data = data;
  return a;
}


Alarm *
grabalarmdata(int action, void *data)
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
destroyalarm(Alarm *adestroy)
{
  Alarm *a;
  if (alarms == adestroy) alarms = adestroy->next;
  for (a = alarms; a; a = a->next)
    if (a->next == adestroy)
      a->next = adestroy->next;
  free(adestroy);
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
unscheduledata(int actions, void *data)
{
  Alarm *prev = 0, *a = alarms;
  while (a) {
    
    if ((a->action & actions) != 0 && (a->data == data || data == 0)) {
      Alarm *n = a->next;
      if (prev) prev->next = n;
      else alarms = n;
      free(a);
      a = n;
      continue;
    }
    
    prev = a;
    a = a->next;
  }
}


int
loopmaster(Alarmloopfunc alarmlooper, Xloopfunc xlooper)
{
  struct timeval timeout, now, *timeoutptr;
  XEvent event;
  fd_set xfds;
  int retval = 0;
  
  xwGETTIME(now);
  FD_ZERO(&xfds);
  
  while (1) {
    int flushneeded = 0;
    
    while (alarms && xwTIMEGEQ(now, alarms->timer)) {
      Alarm *a = alarms;
      Hand *h = (Hand *)a->data;
      Slideshow *ss;
      
      a->scheduled = 0;
      alarms = a->next;
      
      switch (a->action) {
	
       case Raise:
	if (h->mapped && h->obscured) {
	  XMapRaised(display, h->w);
	  h->obscured = 0;
	}
	while (xwTIMEGEQ(now, a->timer))
	  xwADDTIME(a->timer, a->timer, ocurrent->topdelay);
	schedule(a);
	flushneeded = 1;
	break;
	
       case Flash:
	ss = h->slideshow;
	while (xwTIMEGEQ(now, a->timer)) {
	  h->slide = (h->slide + 1) % ss->nslides;
	  xwADDTIME(a->timer, a->timer, ss->delay[h->slide]);
	}
	setpicture(h, ss, h->slide);
	schedule(a);
	flushneeded = 1;
	break;
	
       case Clock:
	drawclock(&now);
	refreshhands();
	xwADDTIME(a->timer, a->timer, clocktick);
	schedule(a);
	flushneeded = 0;
	break;
	
       case Return:
	retval = Return;
	break;
	
       case IdleSelect:
	idleselect((Window)a->data);
	break;
	
       default:
	retval = alarmlooper(a, &now);
	break;
	
      }
      
      if (!a->scheduled) free(a);
      if (retval != 0) return retval;
    }
    
    if (flushneeded) XFlush(display);
    
    if (alarms) {
      timeoutptr = &timeout;
      xwSUBTIME(timeout, alarms->timer, now);
    } else timeoutptr = 0;
    
    FD_SET(xsocket, &xfds);
    select(xsocket + 1, &xfds, 0, 0, timeoutptr);
    
    if (FD_ISSET(xsocket, &xfds))
      while (XPending(display)) {
	XNextEvent(display, &event);
	defaultxprocessing(&event);
	if (xlooper) retval = xlooper(&event);
	if (retval != 0) return retval;
      }
    
    xwGETTIME(now);
    
  }
}
