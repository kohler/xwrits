#include "xwrits.h"
#include <stdlib.h>

static struct timeval lastkeytime;
static struct timeval waitovertime;


static int
waitxloop(XEvent *e)
{
  struct timeval now;
  struct timeval diff;
  
  if (e->type == KeyPress) {
    xwGETTIME(now);
    xwSUBTIME(diff, now, lastkeytime);
    lastkeytime = now;
    
    if (xwTIMEGEQ(diff, breakdelay))
      return WarnRest;
    else if (xwTIMEGEQ(now, waitovertime))
      return Return;
    else
      return 0;
    
  } else
    return 0;
}


void
waitforbreak(void)
{
  int val;
  
  /* Hack to make "spectacular effects" work as expected.
     Without this code, xwrits t=0 would not do anything until a keypress. */
  if (xwTIMELEQ0(typedelay)) return;
  
  xwGETTIME(lastkeytime);
  
  do {
    xwGETTIME(waitovertime);
    xwADDTIME(waitovertime, waitovertime, typedelay);
    val = loopmaster(0, waitxloop);
  } while (val == WarnRest);
}


static void
ensureonehand(void)
{
  Hand *h;
  int nummapped = 0;
  for (h = hands; h; h = h->next)
    if (h->mapped)
      nummapped++;
  if (nummapped == 0)
    XMapRaised(display, hands->w);
}


static int
restxloop(XEvent *e)
{
  if (e->type == ClientMessage)
    /* Window manager deleted only xwrits window. Consider break over. */
    return RestCancelled;
  else if (e->type == KeyPress)
    return RestFailed;
  else
    return 0;
}


int
rest(void)
{
  struct timeval now;
  Alarm *a;
  int val;
  
  blendslideshow(slideshow[Resting]);
  ensureonehand();
  
  xwGETTIME(now);
  a = newalarm(Return);
  xwADDTIME(a->timer, now, breakdelay);
  schedule(a);
  
  if (ocurrent->breakclock) {
    clockzerotime = a->timer;
    drawclock(&now);
    a = newalarm(Clock);
    xwADDTIME(a->timer, now, clocktick);
    schedule(a);
  }
  
  XFlush(display);
  val = loopmaster(0, restxloop);
  
  unschedule(Return | Clock);
  return val == Return ? RestOK : val;
}


static int
readyxloop(XEvent *e)
{
  if (e->type == ButtonPress)
    return 1;
  else if (e->type == ClientMessage)
    /* last xwrits window deleted by window manager */
    return 1;
  else if (e->type == KeyPress)
    /* if they typed, disappear automatically */
    return 1;
  else
    return 0;
}


void
ready(void)
{
  blendslideshow(slideshow[Ready]);
  ensureonehand();
  
  if (ocurrent->beep)
    XBell(display, 0);
  XFlush(display);
  
  loopmaster(0, readyxloop);
}


void
unmapall(void)
{
  XEvent event;
  Hand *h;
  
  while (hands->next)
    destroyhand(hands);
  h = hands;
  
  XUnmapWindow(display, h->w);
  /* Synthetic UnmapNotify required by ICCCM to withdraw the window */
  event.type = UnmapNotify;
  event.xunmap.event = rootwindow;
  event.xunmap.window = h->w;
  event.xunmap.from_configure = False;
  XSendEvent(display, rootwindow, False,
	     SubstructureRedirectMask | SubstructureNotifyMask, &event);
  
  blendslideshow(slideshow[Warning]);
  
  XFlush(display);
  while (XPending(display)) {
    XNextEvent(display, &event);
    defaultxprocessing(&event);
  }
}
