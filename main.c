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

struct timeval genesis_time;
struct timeval type_delay;
struct timeval break_delay;
struct timeval idle_check_delay;
static struct timeval zero = {0, 0};

Hand *hands;
int active_hands = 0;

Slideshow *slideshow[MaxState];

Display *display;
int screen_number;
int display_width;
int display_height;
int x_socket;

Visual *visual;
int depth;
Colormap colormap;
unsigned long black_pixel;
unsigned long white_pixel;

Window root_window;
XFontStruct *font;

int wm_delta_x = NODELTAS;
int wm_delta_y = NODELTAS;

static char *display_name = 0;

static int icon_width;
static int icon_height;

static int force_mono = 0;


static void
determine_wm_deltas(Hand *h)
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
  wm_delta_x = attr.width - h->width;
  wm_delta_y = attr.height - h->height;
}


static void
usage(void)
{
  fprintf(stderr, "\
usage: xwrits [display=d] [typetime=time] [breaktime=time] [after=time]\n\
              [+/-beep] [+/-breakclock] [+/-clock] [+/-finger] [flashtime=time]\n\
              [+/-flipoff] [+/-iconified] [-/+idle] [+/-lock[=time]]\n\
              [maxhands=#] [+/-multiply[=time]] [+/-noiconify]\n\
              [password=password] [+/-top] [version]\n");
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
get_icon_size()
{
  XIconSize *ic;
  int nic;
  icon_width = IconWidth;
  icon_height = IconHeight;
  if (XGetIconSizes(display, root_window, &ic, &nic) == 0)
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

/* get_best_position: gets the best (x, y) pair from the list of pairs stored
     in xlist and ylist (num pairs overall). Best means `covering smallest
     number of existing hands.' Returns it in *retx and *rety */

static void
get_best_position(int *xlist, int *ylist, int num, int width, int height,
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
new_hand(int x, int y)
{
  static XClassHint classh;
  static XSizeHints *xsh;
  static XWMHints *xwmh;
  static XTextProperty window_name, icon_name;
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
    get_best_position(xs, ys, NHTries, WindowWidth, WindowHeight, &x, &y);
  }
  
  if (!xsh) {
    char *stringlist[2];
    stringlist[0] = "xwrits";
    stringlist[1] = NULL;
    XStringListToTextProperty(stringlist, 1, &window_name);
    XStringListToTextProperty(stringlist, 1, &icon_name);
    classh.res_name = "xwrits";
    classh.res_class = "XWrits";
    
    xsh = XAllocSizeHints();
    xsh->flags = USPosition | PMinSize | PMaxSize;
    xsh->min_width = xsh->max_width = WindowWidth;
    xsh->min_height = xsh->max_height = WindowHeight;
    
    xwmh = XAllocWMHints();
    xwmh->flags = InputHint | StateHint | IconWindowHint;
    xwmh->input = True;
    
    get_icon_size();
    
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
  
  {
    XSetWindowAttributes setattr;
    unsigned long setattr_mask;
    setattr.colormap = colormap;
    setattr.backing_store = NotUseful;
    setattr.save_under = False;
    setattr.border_pixel = 0;
    setattr.background_pixel = 0;
    setattr_mask = CWColormap | CWBorderPixel | CWBackPixel | CWBackingStore
      | CWSaveUnder;
    
    nh->w = XCreateWindow
      (display, root_window,
       x, y, WindowWidth, WindowHeight, 0,
       depth, InputOutput, visual, setattr_mask, &setattr);
    
    xwmh->icon_window = nh->iconw = XCreateWindow
      (display, root_window,
       x, y, icon_width, icon_height, 0,
       depth, InputOutput, visual, setattr_mask, &setattr);
  }
#if 0
  nh->w = XCreateSimpleWindow
    (display, root_window,
     x, y, WindowWidth, WindowHeight, 0, 1, 0);
  xwmh->icon_window = nh->iconw = XCreateSimpleWindow
    (display, root_window,
     x, y, icon_width, icon_height, 0, 1, 0);
#endif
  
  XSelectInput(display, nh->iconw, StructureNotifyMask);
  
  xsh->x = x;
  xsh->y = y;
  xwmh->initial_state = ocurrent->appear_iconified ? IconicState : NormalState;
  XSetWMProperties(display, nh->w, &window_name, &icon_name,
		   NULL, 0, xsh, xwmh, &classh);
  XSetWMProtocols(display, nh->w, &wm_delete_window_atom, 1);
  XChangeProperty(display, nh->w, mwm_hints_atom, mwm_hints_atom, 32,
		  PropModeReplace, (unsigned char *)mwm_hints, 4);
  
  XSelectInput(display, nh->w, ButtonPressMask | StructureNotifyMask |
	       (check_idle ? KeyPressMask : 0) | VisibilityChangeMask);
  
  nh->mapped = 0;
  nh->iconified = 0;
  nh->next = hands;
  nh->prev = 0;
  nh->configured = 0;
  nh->slideshow = 0;
  
  if (hands) hands->prev = nh;
  hands = nh;
  active_hands++;
  
  return nh;
}


void
destroy_hand(Hand *h)
{
  XDestroyWindow(display, h->w);
  XDestroyWindow(display, h->iconw);
  if (h->prev) h->prev->next = h->next;
  else hands = h->next;
  if (h->next) h->next->prev = h->prev;
  active_hands--;
  unschedule_data(Flash | Raise, h);
  free(h);
}


void
set_picture(Hand *h, Slideshow *ss, int n)
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
refresh_hands(void)
{
  Hand *h;
  for (h = hands; h; h = h->next)
    XClearWindow(display, h->w);
  XFlush(display);
}


Hand *
window_to_hand(Window w)
{
  Hand *h;
  for (h = hands; h; h = h->next)
    if (h->w == w)
      break;
  return h;
}


Hand *
icon_window_to_hand(Window w)
{
  Hand *h;
  for (h = hands; h; h = h->next)
    if (h->iconw == w)
      break;
  return h;
}


int
default_x_processing(XEvent *e)
{
  Hand *h;
  
  switch (e->type) {
    
   case ConfigureNotify:
    h = window_to_hand(e->xconfigure.window);
    if (!h) break;
    h->configured = 1;
    h->x = e->xconfigure.x;
    h->y = e->xconfigure.y;
    h->width = e->xconfigure.width;
    h->height = e->xconfigure.height;
    if (wm_delta_x == NODELTAS) determine_wm_deltas(h);
    break;
    
   case MapNotify:
    if ((h = window_to_hand(e->xmap.window)))
      h->mapped = 1;
    else if ((h = icon_window_to_hand(e->xmap.window)))
      h->iconified = 1;
    break;
    
   case UnmapNotify:
    if ((h = window_to_hand(e->xmap.window)))
      h->mapped = 0;
    else if ((h = icon_window_to_hand(e->xmap.window)))
      h->iconified = 0;
    break;
    
   case VisibilityNotify:
    if ((h = window_to_hand(e->xvisibility.window))) {
      if (e->xvisibility.state == VisibilityUnobscured)
	h->obscured = 0;
      else
	h->obscured = 1;
    }
    break;
    
   case ClientMessage:
    if (e->xclient.message_type != wm_protocols_atom ||
	(Atom)(e->xclient.data.l[0]) != wm_delete_window_atom)
      e->type = 0;
    else if (active_hands > 1) {
      h = window_to_hand(e->xclient.window);
      if (h) destroy_hand(h);
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
      idle_create(e->xcreatewindow.window, &now);
      break;
    }
    
   case DestroyNotify:
    /* We must unschedule any IdleSelect events for fear of selecting input
       on a destroyed window. There may be a race condition here... */
    unschedule_data(IdleSelect, (void *)e->xdestroywindow.window);
    break;
    
  }
  
  return 0;
}


static int
strtointerval(char *s, char **stores, struct timeval *iv)
{
  long min = 0;
  double sec = 0;
  long integral_sec;
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
  integral_sec = (long)(floor(sec));
  iv->tv_sec = min * SecPerMin + integral_sec;
  iv->tv_usec = (int)(MicroPerSec * (sec - integral_sec));
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
  
  /* Allow for long options. --[option] is equivalent to [option]. */
  if (arg[0] == '-' && arg[1] == '-')
    arg += 2;
  
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
parse_options(int pargc, char **pargv)
{
  char *s;
  char *slideshow_text = DefaultSlideshow;
  struct timeval flash_delay;
  Slideshow *slideshow = 0;
  Options *o = &onormal;
  Options *p;
  
  argc = pargc;
  argv = pargv;
  
  xwSETTIME(flash_delay, DefaultFlashdelay, 0);
  
  while (argc > 0) {
    
    s = argv[0];
    
    if (optparse(s, "after", 1, "sT", &o->next_delay)) {
      if (!slideshow)
	slideshow = parse_slideshow(slideshow_text, &flash_delay);
      o->slideshow = slideshow;
      p = xwNEW(Options);
      *p = *o;
      o->next = p;
      o = p;
      
    } else if (optparse(s, "breaktime", 1, "st", &break_delay))
      ;
    else if (optparse(s, "beep", 2, "t"))
      o->beep = optparse_yesno;
    else if (optparse(s, "breakclock", 6, "t"))
      o->break_clock = optparse_yesno;
    
    else if (optparse(s, "clock", 1, "t"))
      o->clock = optparse_yesno;
    
    else if (optparse(s, "display", 1, "ss", &display_name))
      ;
    
    else if (optparse(s, "finger", 1, "t")) {
      slideshow_text =
	optparse_yesno ? DefaultSlideshowFinger : DefaultSlideshow;
      slideshow = 0;
    } else if (optparse(s, "flashtime", 3, "st", &flash_delay))
      slideshow = 0;
    else if (optparse(s, "flipoff", 1, "t")) {
      slideshow_text =
	optparse_yesno ? DefaultSlideshowFinger : DefaultSlideshow;
      slideshow = 0;
      
    } else if (optparse(s, "iconified", 2, "t"))
      o->appear_iconified = optparse_yesno;
    else if (optparse(s, "idle", 1, "t"))
      check_idle = optparse_yesno;
    
    else if (optparse(s, "lock", 1, "tT", &o->lock_bounce_delay))
      o->lock = optparse_yesno;

    else if (optparse(s, "mono", 2, "t"))
      force_mono = optparse_yesno;
    else if (optparse(s, "multiply", 1, "tT", &o->multiply_delay))
      o->multiply = optparse_yesno;
    else if (optparse(s, "maxhands", 2, "si", &o->max_hands))
      ;
    
    else if (optparse(s, "noiconify", 3, "t"))
      o->never_iconify = optparse_yesno;
    else if (optparse(s, "nooffscreen", 3, "t"))
      /* No longer supported because it never really worked well, wasn't
	 useful, and screwed over virtual desktop users. To resupport it,
	 change the flash() and switchoptions() subroutines and add a clause
	 on ConfigureNotify to warningxloop(). Use constraintoscreen(). */
      error("The nooffscreen option is no longer supported");
    
    else if (optparse(s, "password", 1, "ss", &lock_password))
      ;
    
    else if (optparse(s, "typetime", 1, "st", &type_delay))
      ;
    else if (optparse(s, "top", 2, "tT", &o->top_delay))
      o->top = optparse_yesno;

    else if (optparse(s, "version", 1, "s")) {
      printf("Xwrits version %s\n", VERSION);
      printf("\
Copyright (C) 1994-8 Eddie Kohler\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose. That's right: you're on your own!\n");
      exit(0);
      
    } else
      usage();
  }
  
  /* Set up the slideshow for the last set of options. */
  if (!slideshow)
    slideshow = parse_slideshow(slideshow_text, &flash_delay);
  o->slideshow = slideshow;
}


static void
check_options(Options *o)
{
  if (xwTIMELEQ0(o->multiply_delay)) o->multiply_delay = zero;
  if (xwTIMELEQ0(o->top_delay)) o->top_delay = zero;
  
  if (o->max_hands < 1) o->max_hands = 1;
  if (o->max_hands > MaxHands) o->max_hands = MaxHands;
  
  while (xwTIMELEQ0(o->next_delay)) {
    Options *p = o->next;
    /* If the next set of options is supposed to appear before this one,
       replace this one with the next set. Iterate. */
    *o = *p;
    free(p);
  }
}


#if defined(__cplusplus) || defined(c_plusplus)
#define VISUAL_CLASS c_class
#else
#define VISUAL_CLASS class
#endif

static void
choose_visual(void)
{
  XVisualInfo visi_template;
  int nv, i;
  XVisualInfo *v;
  XVisualInfo *best_v = 0;
  VisualID default_visualid = DefaultVisual(display, screen_number)->visualid;
  visi_template.screen = screen_number;
  
  v = XGetVisualInfo(display, VisualScreenMask, &visi_template, &nv);
  for (i = 0; i < nv && !best_v; i++)
    if (v[i].visualid == default_visualid)
      best_v = &v[i];
  
  if (!best_v) {
    visual = DefaultVisual(display, screen_number);
    depth = DefaultDepth(display, screen_number);
    colormap = DefaultColormap(display, screen_number);
  } else {
  
    /* Which visual to choose? This isn't exactly a simple decision, since
       we want to avoid colormap flashing while choosing a nice visual. So
       here's the algorithm: Prefer the default visual, or take a TrueColor
       visual with strictly greater depth. */
    for (i = 0; i < nv; i++)
      if (v[i].depth > best_v->depth && v[i].VISUAL_CLASS == TrueColor)
	best_v = &v[i];
    
    visual = best_v->visual;
    depth = best_v->depth;
    if (best_v->visualid != default_visualid)
      colormap = XCreateColormap(display, root_window, visual, AllocNone);
    else
      colormap = DefaultColormap(display, screen_number);
    
  }
  
  /* set up black_pixel and white_pixel */
  {
    XColor color;
    color.red = color.green = color.blue = 0;
    XAllocColor(display, colormap, &color);
    black_pixel = color.pixel;
    color.red = color.green = color.blue = 0xFFFF;
    XAllocColor(display, colormap, &color);
    white_pixel = color.pixel;
  }
  
  if (v) XFree(v);
}


int
main(int argc, char *argv[])
{
  Hand *hand;
  Options *o;
  int lock_possible = 0;
  
  xwGETTIMEOFDAY(&genesis_time);
  
  srand((getpid() + 1) * time(0));
  
  xwSETTIME(type_delay, DefaultTypedelay, 0);
  xwSETTIME(break_delay, DefaultBreakdelay, 0);
  xwSETTIME(onormal.multiply_delay,
	    DefaultMultiplydelay, DefaultMultiplydelayUsec);
  xwSETTIME(onormal.lock_bounce_delay, DefaultLockbouncedelay, 0);
  xwSETTIME(onormal.top_delay, DefaultTopdelay, 0);
  onormal.max_hands = DefaultMaxHands;
  xwSETTIME(onormal.next_delay, DefaultRudedelay, 0);
  onormal.next = 0;
  
  xwSETTIME(lock_message_delay, DefaultLockmessagedelay, 0);
  lock_password = DefaultLockpassword;
  
  xwSETTIME(idle_select_delay, DefaultIdleselectdelay, 0);
  xwSETTIME(idle_gap_delay, DefaultIdlegapdelay, DefaultIdlegapdelayUsec);
  check_idle = 1;
  
  /* 15 seconds seems like a reasonable clock tick time, even though it'll
     redraw the same hands 4 times. */
  xwSETTIME(clock_tick, 15, 0);
  
  default_pictures();
  
  /* remove first argument = program name */
  parse_options(argc - 1, argv + 1);
  
  if (xwTIMELEQ0(type_delay)) type_delay = zero;
  if (xwTIMELEQ0(break_delay)) break_delay = zero;
  for (o = &onormal; o; o = o->next) {
    check_options(o);
    if (o->lock) lock_possible = 1;
  }
  
  if (strlen(lock_password) >= MaxPasswordSize)
    optionerror("%s argument too long", "password");
  
  display = XOpenDisplay(display_name);
  if (!display) error("cannot open display");
  x_socket = ConnectionNumber(display);
  screen_number = DefaultScreen(display);
  root_window = RootWindow(display, screen_number);
  choose_visual();
  display_width = DisplayWidth(display, screen_number);
  display_height = DisplayHeight(display, screen_number);
  font = XLoadQueryFont(display, "-*-helvetica-bold-r-*-*-*-180-*");
  
  ocurrent = &onormal;
  
  slideshow[Warning] = onormal.slideshow;
  slideshow[Resting] = parse_slideshow("resting", 0);
  slideshow[Ready] = parse_slideshow("ready", 0);
  if (lock_possible) slideshow[Locked] = parse_slideshow("locked", 0);
  
  hand = new_hand(NHCenter, NHCenter);
  load_needed_pictures(hand->w, lock_possible, force_mono);
  init_clock(hand->w);
  
  if (check_idle) {
    struct timeval now;
    double icd = 0.3 * (break_delay.tv_sec +
			(break_delay.tv_usec / (double)MicroPerSec));
    long integral_icd = (long)(floor(icd));
    
    idle_check_delay.tv_sec = integral_icd;
    idle_check_delay.tv_usec = (long)(MicroPerSec * (icd - integral_icd));
    
    xwGETTIME(now);
    XSetErrorHandler(x_error_handler);
    idle_create(root_window, &now);
  }
  
  while (1) {
    int val = 0;
    ocurrent = &onormal;
    wait_for_break();
    
    while (val != RestOK && val != RestCancelled && val != WarnCancelled) {
      
      val = warning(val == LockFailed || val == LockCancelled);
      
      if (val == WarnRest)
	val = rest();
      else if (val == WarnLock)
	val = lock();
      
    }
    
    if (val == RestOK)
      ready();
    
    unmap_all();
  }
  
  return 0;
}