#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include "xwrits.h"
#include "defaults.h"


#define xwSETTIME(timeval, sec, usec) do { (timeval).tv_sec = (sec); (timeval).tv_usec = (usec); } while (0)

#define NODELTAS	-0x8000

static Options onormal;
Options *ocurrent;

struct timeval genesistime;
struct timeval typedelay;
struct timeval breakdelay;
struct timeval idlecheckdelay;
static struct timeval zero = {0, 0};

Hand *hands;
int activehands = 0;

Slideshow *slideshow[MaxState];

Display *display;
int screennumber;
Window rootwindow;
int display_width, display_height;
XFontStruct *font;
int xsocket;
int wmdeltax = NODELTAS, wmdeltay = NODELTAS;

static char *displayname = 0;

static int icon_width, icon_height;


static void
determinewmdeltas(Hand *h)
{
  Window root, parent, pparent, *children;
  XWindowAttributes attr;
  unsigned int nchildren;
  root = None;
  parent = h->w;
  do {
    pparent = parent;
    XQueryTree(display, pparent, &root, &parent, &children, &nchildren);
    XFree(children);
  } while (parent != root);
  XGetWindowAttributes(display, pparent, &attr);
  wmdeltax = attr.width - h->width;
  wmdeltay = attr.height - h->height;
}


static void
usage(void)
{
  fprintf(stderr, "\
usage: xwrits [display=d] [typetime=time] [breaktime=time] [after=time]\n\
              [+/-beep] [+/-breakclock] [+/-clock] [+/-finger] [flashtime=time]\n\
              [+/-flipoff] [+/-iconified] [-/+idle[=time]] [+/-lock[=time]]\n\
              [maxhands=#] [+/-multiply[=time]] [+/-noiconify]\n\
              [password=password] [+/-top]\n");
  exit(1);
}


void
error(char *x)
{
  fprintf(stderr, "xwrits: %s.\n", x);
  exit(1);
}


static void
optionerror(char *x, char *option)
{
  fprintf(stderr, "xwrits: ");
  fprintf(stderr, x, option);
  fprintf(stderr, ".\n");
  exit(1);
}


static void
geticonsize()
{
  XIconSize *ic;
  int nic;
  icon_width = IconWidth;
  icon_height = IconHeight;
  if (XGetIconSizes(display, rootwindow, &ic, &nic) == 0)
    return;
  if (nic != 0) {
    if (icon_width < ic->min_width) icon_width = ic->min_width;
    if (icon_width < ic->min_height) icon_height = ic->min_height;
    if (icon_width > ic->max_width) icon_width = ic->max_width;
    if (icon_height > ic->max_height) icon_height = ic->max_height;
  }
  XFree(ic);
}


#define xwmax(i, j) ((i) > (j) ? (i) : (j))
#define xwmin(i, j) ((i) < (j) ? (i) : (j))

/* getbestposition: gets the best (x, y) pair from the list of pairs stored
     in xlist and ylist (num pairs overall). Best means `covering smallest
     number of existing hands.' Returns it in *retx and *rety */

static void
getbestposition(int *xlist, int *ylist, int num, int width, int height,
		int *retx, int *rety)
{
  unsigned int bestpenalty = 0x8000U;
  unsigned int penalty;
  int i, overw, overh, best = 0;
  Hand *h;
  for (i = 0; i < num; i++) {
    int x1 = xlist[i], y1 = ylist[i];
    int x2 = x1 + width, y2 = y1 + height;
    penalty = 0;
    for (h = hands; h; h = h->next)
      if (h->mapped) {
	overw = xwmin(x2, h->x + h->width) - xwmax(x1, h->x);
	overh = xwmin(y2, h->y + h->height) - xwmax(y1, h->y);
	if (overw > 0 && overh > 0) penalty += overw * overh;
      }
    if (penalty < bestpenalty) {
      bestpenalty = penalty;
      best = i;
    }
  }
  *retx = xlist[best];
  *rety = ylist[best];
}


#define NHTries 6
#define NHOffScreenAllowance 100
static Atom wm_delete_window_atom;
static Atom wm_protocols_atom;
static Atom mwm_hints_atom;

Hand *
newhand(int x, int y)
{
  static XClassHint classh;
  static XSizeHints *xsh;
  static XWMHints *xwmh;
  static XTextProperty windowname, iconname;
  static u_int32_t *mwm_hints;
  Hand *nh = xwNEW(Hand);
  
  if (x == NHCenter)
    x = (display_width - WindowWidth) / 2;
  if (y == NHCenter)
    y = (display_height - WindowHeight) / 2;
  
  if (x == NHRandom || y == NHRandom) {
    int xs[NHTries], ys[NHTries], i;
    int xdist = display_width - WindowWidth;
    int ydist = display_height - WindowHeight;
    int xrand = x == NHRandom;
    int yrand = y == NHRandom; /* gcc bug here */
    for (i = 0; i < NHTries; i++) {
      if (xrand) xs[i] = (rand() >> 4) % xdist;
      else xs[i] = x;
      if (yrand) ys[i] = (rand() >> 4) % ydist;
      else ys[i] = y;
    }
    getbestposition(xs, ys, NHTries, WindowWidth, WindowHeight, &x, &y);
  }
  
  if (!xsh) {
    char *stringlist[2];
    stringlist[0] = "xwrits";
    stringlist[1] = NULL;
    XStringListToTextProperty(stringlist, 1, &windowname);
    XStringListToTextProperty(stringlist, 1, &iconname);
    classh.res_name = "xwrits";
    classh.res_class = "XWrits";
    
    xsh = XAllocSizeHints();
    xsh->flags = USPosition | PMinSize | PMaxSize;
    xsh->min_width = xsh->max_width = WindowWidth;
    xsh->min_height = xsh->max_height = WindowHeight;
    
    xwmh = XAllocWMHints();
    xwmh->flags = InputHint | StateHint | IconWindowHint;
    xwmh->input = True;
    
    geticonsize();
    
    wm_delete_window_atom = XInternAtom(display, "WM_DELETE_WINDOW", False);
    wm_protocols_atom = XInternAtom(display, "WM_PROTOCOLS", False);
    mwm_hints_atom = XInternAtom(display, "_MOTIF_WM_HINTS", False);
    
    /* Silly hackery to get the MWM appearance *just right*: ie., no resize
       handles or maximize button, no Resize or Maximize entries in window
       menu. The constitution of the property itself was inferred from data
       in <Xm/MwmUtil.h> and output of xprop. */
    mwm_hints = xwNEWARR(u_int32_t, 4);
    mwm_hints[0] = (1L << 0) | (1L << 1);
    /* flags = MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS */
    mwm_hints[1] = (1L << 2) | (1L << 3) | (1L << 5);
    /* functions = MWM_FUNC_MOVE | MWM_FUNC_MINIMIZE | MWM_FUNC_CLOSE */
    mwm_hints[2] = (1L << 1) | (1L << 3) | (1L << 4) | (1L << 5);
    /* decorations = MWM_DECOR_BORDER | MWM_DECOR_TITLE | MWM_DECOR_MENU
       | MWM_DECOR_MINIMIZE */
    mwm_hints[3] = ~(0L);
    
  }
  
  /* setattr.colormap = colormap;
     setattr.background_pixel = BlackPixel(display, screennumber);
     setattr.backing_store = NotUseful;
     setattr.save_under = False;
     setattr.border_pixel = BlackPixel(display, screennumber);
     nh->w = XCreateWindow
     (display, rootwindow,
     x, y, WindowWidth, WindowHeight, 1,
     depth, InputOutput, visual, CWColormap | CWBackPixel
     | CWSaveUnder | CWBackingStore | CWBorderPixel, &setattr); */
  nh->w = XCreateSimpleWindow
    (display, rootwindow,
     x, y, WindowWidth, WindowHeight, 0, 1, 0);
  
  xwmh->icon_window = nh->iconw = XCreateSimpleWindow
    (display, rootwindow,
     x, y, icon_width, icon_height, 0, 1, 0);
  XSelectInput(display, nh->iconw, StructureNotifyMask);
  
  xsh->x = x;
  xsh->y = y;
  xwmh->initial_state = ocurrent->appeariconified ? IconicState : NormalState;
  XSetWMProperties(display, nh->w, &windowname, &iconname,
		   NULL, 0, xsh, xwmh, &classh);
  XSetWMProtocols(display, nh->w, &wm_delete_window_atom, 1);
  XChangeProperty(display, nh->w, mwm_hints_atom, mwm_hints_atom, 32,
		  PropModeReplace, (unsigned char *)mwm_hints, 4);
  
  XSelectInput(display, nh->w, ButtonPressMask | StructureNotifyMask |
	       (checkidle ? KeyPressMask : 0) | VisibilityChangeMask);
  
  nh->mapped = 0;
  nh->iconified = 0;
  nh->next = hands;
  nh->prev = 0;
  nh->configured = 0;
  nh->slideshow = 0;
  
  if (hands) hands->prev = nh;
  hands = nh;
  activehands++;
  
  return nh;
}


void
destroyhand(Hand *h)
{
  XDestroyWindow(display, h->w);
  XDestroyWindow(display, h->iconw);
  if (h->prev) h->prev->next = h->next;
  else hands = h->next;
  if (h->next) h->next->prev = h->prev;
  activehands--;
  unscheduledata(Flash | Raise, h);
  free(h);
}


void
setpicture(Hand *h, Slideshow *ss, int n)
{
  Picture *p = ss->picture[n];
  
  if (p->large) XSetWindowBackgroundPixmap(display, h->w, p->large);
  else XSetWindowBackground(display, h->w, p->background);
  XClearWindow(display, h->w);
  
  if (h->iconw) {
    if (p->icon) XSetWindowBackgroundPixmap(display, h->iconw, p->icon);
    else if (p->large) XSetWindowBackgroundPixmap(display, h->iconw, p->large);
    else XSetWindowBackground(display, h->iconw, p->background);
    if (h->iconified) XClearWindow(display, h->iconw);
  }
  
  h->slideshow = ss;
  h->slide = n;
}


void
refreshhands(void)
{
  Hand *h;
  for (h = hands; h; h = h->next)
    XClearWindow(display, h->w);
  XFlush(display);
}


Hand *
windowtohand(Window w)
{
  Hand *h;
  for (h = hands; h; h = h->next)
    if (h->w == w)
      break;
  return h;
}


Hand *
iconwindowtohand(Window w)
{
  Hand *h;
  for (h = hands; h; h = h->next)
    if (h->iconw == w)
      break;
  return h;
}


int
defaultxprocessing(XEvent *e)
{
  Hand *h;
  
  switch (e->type) {
    
   case ConfigureNotify:
    h = windowtohand(e->xconfigure.window);
    if (!h) break;
    h->configured = 1;
    h->x = e->xconfigure.x;
    h->y = e->xconfigure.y;
    h->width = e->xconfigure.width;
    h->height = e->xconfigure.height;
    if (wmdeltax == NODELTAS) determinewmdeltas(h);
    break;
    
   case MapNotify:
    if ((h = windowtohand(e->xmap.window)))
      h->mapped = 1;
    else if ((h = iconwindowtohand(e->xmap.window)))
      h->iconified = 1;
    break;
    
   case UnmapNotify:
    if ((h = windowtohand(e->xmap.window)))
      h->mapped = 0;
    else if ((h = iconwindowtohand(e->xmap.window)))
      h->iconified = 0;
    break;
    
   case VisibilityNotify:
    if ((h = windowtohand(e->xvisibility.window))) {
      if (e->xvisibility.state == VisibilityUnobscured)
	h->obscured = 0;
      else
	h->obscured = 1;
    }
    break;
    
   case ClientMessage:
    if (e->xclient.message_type != wm_protocols_atom ||
	e->xclient.data.l[0] != wm_delete_window_atom)
      e->type = 0;
    else if (activehands > 1) {
      h = windowtohand(e->xclient.window);
      if (h) destroyhand(h);
      e->type = 0;
    }
    /* We leave e->type == ClientMessage only if it was a DELETE_WINDOW which
       was trying to delete the last remaining xwrits window. */
    break;
    
    /* for idle processing */
   case CreateNotify:
    {
      struct timeval now;
      xwGETTIME(now);
      idlecreate(e->xcreatewindow.window, &now);
      break;
    }
    
   case DestroyNotify:
    /* We must unschedule any IdleSelect events for fear of selecting input
       on a destroyed window. There may be a race condition here... */
    unscheduledata(IdleSelect, (void *)e->xdestroywindow.window);
    break;
    
  }
  
  return 0;
}


static int
strtointerval(char *s, char **stores, struct timeval *iv)
{
  long min = 0;
  double sec = 0;
  int ok = 0;
  if (isdigit(*s)) min = strtol(s, &s, 10), ok = 1;
  if (*s == ':') {
    ++s;
    if (isdigit(*s) || *s == '.') sec = strtod(s, &s), ok = 1;
  } else if (*s == '.') {
    sec = min + strtod(s, &s);
    min = 0;
    ok = 1;
  }
  if (stores) *stores = s;
  if (!ok || *s != 0) return 0;
  iv->tv_sec = min * SecPerMin + floor(sec);
  iv->tv_usec = MicroPerSec * (sec - (long)floor(sec));
  return 1;
}


#define ARGSHIFT do { argc--; argv++; } while (0)


static int argc;
static char **argv;


static int optparse_yesno;

static int
optparse(char *arg, char *option, int unique, char *format, ...)
{
  va_list val;
  int separate = 0;
  char *opt = option;
  struct timeval *timeptr;
  char **charptr;
  int *intptr;
    
  va_start(val, format);
  
  if (*format == 't') {
    /* Toggle switch. -[option] means off; +[option] or [option] means on.
       Arguments must be given with = syntax. */
    if (*arg == '-') optparse_yesno = 0, arg++;
    else if (*arg == '+') optparse_yesno = 1, arg++;
    else optparse_yesno = 1;
    
  } else if (*format == 's') {
    /* Set option. -[option], +[option] and [option] are acceptable.
       Arguments may be given with = syntax or as separate entities. */
    if (*arg == '-' || *arg == '+') arg++;
    separate = 1;
  }
  format++;
  
  while (*arg && *arg != '=' && *opt) {
    if (*arg++ != *opt++) return 0;
    unique--;
  }
  if (unique > 0) return 0;
  
  if (*arg == '=') {
    arg++;
    separate = 0;
    if (!*format) optionerror("too many arguments to %s", option);
  }
  
  if (separate && *format) {
    ARGSHIFT;
    if (argc == 0) goto doneargs;
    arg = argv[0];
  } else if (!*arg) goto doneargs;
  
  switch (*format++) {
    
   case 't': /* required time */
   case 'T': /* optional time */
    timeptr = va_arg(val, struct timeval *);
    if (!strtointerval(arg, &arg, timeptr))
      optionerror("incorrect time format in %s argument", option);
    break;
    
   case 's': /* string */
    charptr = va_arg(val, char **);
    *charptr = arg;
    break;
    
   case 'i': /* integer */
    intptr = va_arg(val, int *);
    *intptr = (int)strtol(arg, &arg, 10);
    if (*arg)
      optionerror("argument to %s must be integer", option);
    break;
    
  }
  
 doneargs:
  ARGSHIFT;
  while (*format)
    switch (*format++) {
     case 't':
     case 's':
      optionerror("%s not given enough arguments", option);
    }
  va_end(val);
  return 1;
}


static void
parseoptions(int pargc, char **pargv)
{
  char *s;
  char *slideshowtext = DefaultSlideshow;
  struct timeval flashdelay;
  Slideshow *slideshow = 0;
  Options *o = &onormal;
  Options *p;
  
  argc = pargc;
  argv = pargv;
  
  xwSETTIME(flashdelay, DefaultFlashdelay, 0);
  
  while (argc > 0) {
    
    s = argv[0];
    
    if (optparse(s, "after", 1, "sT", &o->nextdelay)) {
      if (!slideshow) slideshow = parseslideshow(slideshowtext, &flashdelay);
      o->slideshow = slideshow;
      p = xwNEW(Options);
      *p = *o;
      o->next = p;
      o = p;
      
    } else if (optparse(s, "breaktime", 1, "st", &breakdelay))
      ;
    else if (optparse(s, "beep", 2, "t"))
      o->beep = optparse_yesno;
    else if (optparse(s, "breakclock", 6, "t"))
      o->breakclock = optparse_yesno;
    
    else if (optparse(s, "clock", 1, "t"))
      o->clock = optparse_yesno;
    
    else if (optparse(s, "display", 1, "ss", &displayname))
      ;
    
    else if (optparse(s, "finger", 1, "t")) {
      slideshowtext =
	optparse_yesno ? DefaultSlideshowFinger : DefaultSlideshow;
      slideshow = 0;
    } else if (optparse(s, "flashtime", 3, "st", &flashdelay))
      slideshow = 0;
    else if (optparse(s, "flipoff", 1, "t")) {
      slideshowtext =
	optparse_yesno ? DefaultSlideshowFinger : DefaultSlideshow;
      slideshow = 0;
      
    } else if (optparse(s, "iconified", 2, "t"))
      o->appeariconified = optparse_yesno;
    else if (optparse(s, "idle", 1, "t"))
      checkidle = optparse_yesno;
    
    else if (optparse(s, "lock", 1, "tT", &o->lockbouncedelay))
      o->lock = optparse_yesno;
    
    else if (optparse(s, "multiply", 1, "tT", &o->multiplydelay))
      o->multiply = optparse_yesno;
    else if (optparse(s, "maxhands", 2, "si", &o->maxhands))
      ;
    
    else if (optparse(s, "noiconify", 3, "t"))
      o->nevericonify = optparse_yesno;
    else if (optparse(s, "nooffscreen", 3, "t"))
      /* No longer supported because it never really worked well, wasn't
	 useful, and screwed over virtual desktop users. To resupport it,
	 change the flash() and switchoptions() subroutines and add a clause
	 on ConfigureNotify to warningxloop(). Use constraintoscreen(). */
      error("The nooffscreen option is no longer supported");
    
    else if (optparse(s, "password", 1, "ss", &lockpassword))
      ;
    
    else if (optparse(s, "typetime", 1, "st", &typedelay))
      ;
    else if (optparse(s, "top", 2, "tT", &o->topdelay))
      o->top = optparse_yesno;
    
    else
      usage();
  }
  
  /* Set up the slideshow for the last set of options. */
  if (!slideshow) slideshow = parseslideshow(slideshowtext, &flashdelay);
  o->slideshow = slideshow;
}


static void
checkoptions(Options *o)
{
  if (xwTIMELEQ0(o->multiplydelay)) o->multiplydelay = zero;
  if (xwTIMELEQ0(o->topdelay)) o->topdelay = zero;
    
  if (o->maxhands < 1) o->maxhands = 1;
  if (o->maxhands > MaxHands) o->maxhands = MaxHands;
  
  while (xwTIMELEQ0(o->nextdelay)) {
    Options *p = o->next;
    /* If the next set of options is supposed to appear before this one,
       replace this one with the next set. Iterate. */
    *o = *p;
    free(p);
  }
}


int
main(int argc, char *argv[])
{
  Hand *hand;
  Options *o;
  int lockpossible = 0;
  
  xwGETTIMEOFDAY(&genesistime);
  
  srand((getpid() + 1) * time(0));
  
  xwSETTIME(typedelay, DefaultTypedelay, 0);
  xwSETTIME(breakdelay, DefaultBreakdelay, 0);
  xwSETTIME(onormal.multiplydelay,
	    DefaultMultiplydelay, DefaultMultiplydelayUsec);
  xwSETTIME(onormal.lockbouncedelay, DefaultLockbouncedelay, 0);
  xwSETTIME(onormal.topdelay, DefaultTopdelay, 0);
  onormal.maxhands = DefaultMaxHands;
  xwSETTIME(onormal.nextdelay, DefaultRudedelay, 0);
  onormal.next = 0;
  
  xwSETTIME(lockmessagedelay, DefaultLockmessagedelay, 0);
  lockpassword = DefaultLockpassword;
  
  xwSETTIME(idleselectdelay, DefaultIdleselectdelay, 0);
  xwSETTIME(idlegapdelay, DefaultIdlegapdelay, DefaultIdlegapdelayUsec);
  checkidle = 1;
  
  /*xwSETTIME(clocktick, SecPerMin, 0);*/
  xwSETTIME(clocktick, 1, 0);
  
  defaultpictures();
  
  /* remove first argument = program name */
  parseoptions(argc - 1, argv + 1);
  
  if (xwTIMELEQ0(typedelay)) typedelay = zero;
  if (xwTIMELEQ0(breakdelay)) breakdelay = zero;
  for (o = &onormal; o; o = o->next) {
    checkoptions(o);
    if (o->lock) lockpossible = 1;
  }
  
  if (strlen(lockpassword) >= MaxPasswordSize)
    optionerror("%s argument too long", "password");
  
  display = XOpenDisplay(displayname);
  if (!display) error("cannot open display");
  xsocket = ConnectionNumber(display);
  screennumber = DefaultScreen(display);
  rootwindow = RootWindow(display, screennumber);
  display_width = DisplayWidth(display, screennumber);
  display_height = DisplayHeight(display, screennumber);
  font = XLoadQueryFont(display, "-*-helvetica-bold-r-*-*-*-180-*");
  
  ocurrent = &onormal;
  
  slideshow[Warning] = onormal.slideshow;
  slideshow[Resting] = parseslideshow("resting", 0);
  slideshow[Ready] = parseslideshow("ready", 0);
  if (lockpossible) slideshow[Locked] = parseslideshow("locked", 0);
  
  hand = newhand(NHCenter, NHCenter);
  loadneededpictures(hand->w, lockpossible);
  initclock(hand->w);
  
  if (checkidle) {
    struct timeval now;
    double icd;
    
    icd = 0.3 * (breakdelay.tv_sec +
		 (breakdelay.tv_usec / (double)MicroPerSec));
    idlecheckdelay.tv_sec = floor(icd);
    idlecheckdelay.tv_usec = MicroPerSec * (icd - (long)floor(icd));
    
    xwGETTIME(now);
    XSetErrorHandler(xerrorhandler);
    idlecreate(rootwindow, &now);
  }
  
  while (1) {
    int val = 0;
    ocurrent = &onormal;
    waitforbreak();
    
    while (val != RestOK && val != RestCancelled && val != WarnCancelled) {
      
      val = warning(val == LockFailed || val == LockCancelled);
      
      if (val == WarnRest)
	val = rest();
      else if (val == WarnLock)
	val = lock();
      
    }
    
    if (val == RestOK)
      ready();
    
    unmapall();
  }
  
  return 0;
}
