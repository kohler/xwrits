#include "xwrits.h"
#include <math.h>

static int clockdisplaying = 0;
static struct timeval lastkeytime;


static void
popuphand(Hand *h)
{
  setpicture(h, ocurrent->slideshow, 0);
  XMapRaised(display, h->w);
  XFlush(display);
  if (h->slideshow->nslides > 1) {
    Alarm *a = newalarm(Flash);
    unscheduledata(Flash, h);
    a->data = h;
    xwGETTIME(a->timer);
    xwADDTIME(a->timer, a->timer, h->slideshow->delay[h->slide]);
    schedule(a);
  }
  if (ocurrent->top) {
    Alarm *a = newalarm(Raise);
    a->data = h;
    xwGETTIME(a->timer);
    xwADDTIME(a->timer, a->timer, ocurrent->topdelay);
    schedule(a);
  }
}


static int
switchoptions(Options *opt, struct timeval now, int lockfailed)
{
  Hand *h;
  Alarm *a;
  Picture *p;
  
  ocurrent = opt;
  
  blendslideshow(opt->slideshow);
  
  if (opt->multiply) {
    a = newalarm(Multiply);
    xwADDTIME(a->timer, now, opt->multiplydelay);
    schedule(a);
  } else unschedule(Multiply);
  
  if (opt->clock && !clockdisplaying) {
    a = newalarm(Clock);
    a->timer = now;
    schedule(a);
    clockdisplaying = 1;
  } else if (!opt->clock && clockdisplaying) {
    unschedule(Clock);
    eraseclock();
    refreshhands();
    clockdisplaying = 0;
  }
  
  for (h = hands; h; h = h->next)
    if ((opt->nevericonify && !h->mapped) ||
	(opt->top && h->mapped) ||
	(!opt->appeariconified && h->iconified))
      XMapRaised(display, h->w);
  
  if (!opt->top) unschedule(Raise);
  /* for (a = alarms; a; a = a->next)
    if (a->action == Flash && opt->neverobscured)
      a->action = FlashRaise;
    else if (a->action == FlashRaise && !opt->neverobscured)
      a->action = Flash; */
  
  if (opt->beep)
    XBell(display, 0);
  
  XFlush(display);
  
  if (opt->next) {
    a = newalarm(NextOptions);
    xwADDTIME(a->timer, now, opt->nextdelay);
    schedule(a);
  }
  
  if (opt->lock && !lockfailed)
    return WarnLock;
  else
    return 0;
}


static int
warningalarmloop(Alarm *a, struct timeval *now)
{
  switch (a->action) {
    
   case Multiply:
    if (activehands < ocurrent->maxhands)
      popuphand(newhand(NHRandom, NHRandom));
    xwADDTIME(a->timer, a->timer, ocurrent->multiplydelay);
    schedule(a);
    break;
    
   case NextOptions:
    return switchoptions(ocurrent->next, *now, 0);
    
   case IdleCheck:
    return WarnRest;
    
  }
  return 0;
}


static int
warningxloop(XEvent *e)
{
  Alarm *a;
  switch (e->type) {
    
   case UnmapNotify:
    if (ocurrent->nevericonify && windowtohand(e->xunmap.window))
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
    if ((a = grabalarm(IdleCheck))) {
      xwGETTIME(lastkeytime);
      xwADDTIME(a->timer, lastkeytime, idlecheckdelay);
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
  
  xwGETTIME(clockzerotime);
  lastkeytime = clockzerotime;
  val = switchoptions(ocurrent, clockzerotime, lockfailed);
  if (val == WarnLock) return val;
  popuphand(hands);
  
  if (checkidle && !xwTIMELEQ0(idlecheckdelay)) {
    Alarm *a = newalarm(IdleCheck);
    xwADDTIME(a->timer, lastkeytime, idlecheckdelay);
    schedule(a);
  }
  
  val = loopmaster(warningalarmloop, warningxloop);
  unschedule(Flash | Raise | Multiply | Clock | NextOptions | IdleCheck);
  
  return val;
}
