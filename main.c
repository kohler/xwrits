#include <config.h>
#include "xwrits.h"
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <assert.h>
#include "gifx.h"

#define DEFAULT_SLIDESHOW		"&clench;&spread"
#define DEFAULT_ICON_SLIDESHOW		"&clenchicon;&spreadicon"
#define FINGER_SLIDESHOW		"&clench;&spread;&finger"
#define FINGER_ICON_SLIDESHOW		"&clenchicon;&spreadicon;&fingericon"

#define SET_TIME(timeval, sec, usec) do { (timeval).tv_sec = (sec); (timeval).tv_usec = (usec); } while (0)

static Options onormal;
Options *ocurrent;

struct timeval genesis_time;
struct timeval type_delay;
struct timeval break_delay;
static struct timeval zero = {0, 0};
struct timeval first_warning_time;
struct timeval last_key_time;

Hand *hands;
Hand *icon_hands;

Gif_Stream *resting_slideshow, *resting_icon_slideshow;
static const char *resting_slideshow_text = "&resting";
static const char *resting_icon_slideshow_text = "&resting";

Gif_Stream *ready_slideshow, *ready_icon_slideshow;
static const char *ready_slideshow_text = "&ready";
static const char *ready_icon_slideshow_text = "&ready";

Display *display;
Port port;
static Gif_XContext *gfx;

static char *display_name = 0;

int check_idle;
struct timeval idle_time;

int check_mouse;
struct timeval check_mouse_time;
int last_mouse_x;
int last_mouse_y;
Window last_mouse_root;

static int icon_width;
static int icon_height;

static int force_mono = 0;


static void
short_usage(void)
{
  fprintf(stderr, "Usage: xwrits [-display DISPLAY] [typetime=TIME] [breaktime=TIME] [OPTION]...\n\
Try `xwrits --help' for more information.\n");
  exit(1);
}


static void
usage(void)
{
  printf("\
`Xwrits' reminds you to take wrist breaks, which should help you prevent or\n\
manage a repetitive stress injury. It runs on X.\n\
\n\
Usage: xwrits [-display DISPLAY] [typetime=TIME] [breaktime=TIME] [OPTION]...\n\
\n\
All options may be abbreviated to their unique prefixes. You can type\n\
`--OPTION', `OPTION', or `+OPTION': they act equivalently. Some options can be\n\
turned off with `-OPTION'; these are shown as `+/-OPTION', and only `+OPTION's\n\
effect is described.\n\
\n\
General options:\n\
  --display DISPLAY     Monitor the X display DISPLAY.\n\
  --help                Print this message and exit.\n\
  --version             Print version number and exit.\n\
  --mono                Use monochrome pictures.\n\
  +/-idle               Monitor your typing (on by default).\n\
  +/-mouse              Monitor your mouse movements (off by default).\n\
\n");
  printf("\
Break characteristics:\n\
  --typetime=DURATION   Type for DURATION, (also: `t=DURATION')\n\
  --breaktime=DURATION  then take a break for DURATION. (also: `b=DURATION')\n\
  +/-lock               Lock the keyboard during the break.\n\
  --password=PW         Set the password for unlocking the keyboard.\n\
\n\
Appearance options:\n\
  +/-finger             Be rude.\n\
  --flashtime=RATE      Flash the warning window at RATE.\n\
  --warning-picture=GIF-FILE, --wp=GIF-FILE   Use an arbitrary GIF animation\n\
                        as the warning window picture.\n\
  +/-beep               Beep when the warning window appears.\n\
  +/-clock              Show how long you have ignored the warning window.\n\
  +/-multiply=PERIOD    Make a new warning window every PERIOD,\n\
  --maxhands=NUM        up to a maximum of NUM windows (default: 25).\n\
  +/-iconified          Warning windows appear as icons.\n\
  +/-noiconify          Don't let anyone iconify the warning window.\n\
  +/-top                Keep the warning windows on top of the window stack.\n\
  --after=TIME          Change behavior if warning window is ignored for TIME;\n\
                        options following `--after' give new behavior.\n\
  +/-breakclock         Show how much time remains during the break.\n\
  --rest-picture=GIF-FILE, --rp=GIF-FILE   Use an arbitrary GIF animation\n\
                        as the resting window picture.\n\
  --ready-picture=GIF-FILE, --okp=GIF-FILE   Use an arbitrary GIF animation\n\
                        as the `OK' window picture.\n\
\n\
Report bugs to <eddietwo@lcs.mit.edu>.\n");
}


void
error(const char *format, ...)
{
  va_list val;
  va_start(val, format);
  fprintf(stderr, "xwrits: ");
  vfprintf(stderr, format, val);
  fprintf(stderr, "\n");
  va_end(val);
  exit(1);
}


static void
get_icon_size()
{
  XIconSize *ic;
  int nic;
  icon_width = onormal.icon_slideshow->screen_width;
  icon_height = onormal.icon_slideshow->screen_height;
  if (XGetIconSizes(display, port.root_window, &ic, &nic) == 0)
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
  unsigned int best_penalty = 0x8000U;
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
    if (penalty < best_penalty) {
      best_penalty = penalty;
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
  Hand *nh_icon = xwNEW(Hand);
  int width = onormal.slideshow->screen_width;
  int height = onormal.slideshow->screen_height;
  
  if (x == NHCenter)
    x = (port.width - width) / 2;
  if (y == NHCenter)
    y = (port.height - height) / 2;
  
  if (x == NHRandom || y == NHRandom) {
    int xs[NHTries], ys[NHTries], i;
    int xdist = port.width - width;
    int ydist = port.height - height;
    int xrand = x == NHRandom;
    int yrand = y == NHRandom;
    for (i = 0; i < NHTries; i++) {
      if (xrand) xs[i] = (rand() >> 4) % xdist;
      else xs[i] = x;
      if (yrand) ys[i] = (rand() >> 4) % ydist;
      else ys[i] = y;
    }
    get_best_position(xs, ys, NHTries, width, height, &x, &y);
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
    xsh->min_width = xsh->max_width = width;
    xsh->min_height = xsh->max_height = height;
    
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
    mwm_hints[1] = (1L << 2) | (1L << 5);
    /* functions = MWM_FUNC_MOVE | MWM_FUNC_CLOSE */
    mwm_hints[2] = (1L << 1) | (1L << 3) | (1L << 4);
    /* decorations = MWM_DECOR_BORDER | MWM_DECOR_TITLE | MWM_DECOR_MENU */
    mwm_hints[3] = ~(0L);

    /* Add MINIMIZE options only if the window might be iconifiable */
    {
      Options *o;
      int ever_iconifiable = 0;
      for (o = &onormal; o; o = o->next)
	if (!o->never_iconify)
	  ever_iconifiable = 1;
      if (ever_iconifiable) {
	mwm_hints[1] |= (1L << 3); /* MWM_FUNC_MINIMIZE */
	mwm_hints[2] |= (1L << 5); /* MWM_DECOR_MINIMIZE */
      }
    }
  }
  
  {
    XSetWindowAttributes setattr;
    unsigned long setattr_mask;
    setattr.colormap = port.colormap;
    setattr.backing_store = NotUseful;
    setattr.save_under = False;
    setattr.border_pixel = 0;
    setattr.background_pixel = 0;
    setattr_mask = CWColormap | CWBorderPixel | CWBackPixel | CWBackingStore
      | CWSaveUnder;
    
    nh->w = XCreateWindow
      (display, port.root_window,
       x, y, width, height, 0,
       port.depth, InputOutput, port.visual, setattr_mask, &setattr);
    
    xwmh->icon_window = nh_icon->w = XCreateWindow
      (display, port.root_window,
       x, y, icon_width, icon_height, 0,
       port.depth, InputOutput, port.visual, setattr_mask, &setattr);
  }
  
  XSelectInput(display, nh_icon->w, StructureNotifyMask);
  
  xsh->x = x;
  xsh->y = y;
  xwmh->initial_state = ocurrent->appear_iconified ? IconicState : NormalState;
  XSetWMProperties(display, nh->w, &window_name, &icon_name,
		   NULL, 0, xsh, xwmh, &classh);
  XSetWMProtocols(display, nh->w, &wm_delete_window_atom, 1);
  XChangeProperty(display, nh->w, mwm_hints_atom, mwm_hints_atom, 32,
		  PropModeReplace, (unsigned char *)mwm_hints, 4);
  
  XSelectInput(display, nh->w, ButtonPressMask | StructureNotifyMask
	       | (check_idle ? KeyPressMask : 0) | VisibilityChangeMask
	       | ExposureMask);
  
  nh->icon = nh_icon;
  nh->width = width;		/* will be reset correctly by */
  nh->height = height;		/* next ConfigureNotify */
  nh->root_child = nh->w;
  nh->is_icon = 0;
  nh->mapped = 0;
  nh->configured = 0;
  nh->slideshow = 0;
  nh->clock = 0;
  if (hands) hands->prev = nh;
  nh->next = hands;
  nh->prev = 0;
  hands = nh;
  
  nh_icon->icon = nh;
  nh_icon->root_child = nh->w;
  nh_icon->is_icon = 1;
  nh_icon->mapped = 0;
  nh_icon->configured = 0;
  nh_icon->slideshow = 0;
  nh_icon->clock = 0;
  if (icon_hands) icon_hands->prev = nh_icon;
  nh_icon->next = icon_hands;
  nh_icon->prev = 0;
  icon_hands = nh_icon;
  
  return nh;
}

void
destroy_hand(Hand *h)
{
  assert(!h->is_icon);
  unschedule_data(A_FLASH, h);
  if (h == hands && !h->next) {
    XEvent event;
    /* last remaining hand; don't destroy it, unmap it */
    XUnmapWindow(display, h->w);
    /* Synthetic UnmapNotify required by ICCCM to withdraw the window */
    event.type = UnmapNotify;
    event.xunmap.event = port.root_window;
    event.xunmap.window = h->w;
    event.xunmap.from_configure = False;
    XSendEvent(display, port.root_window, False,
	       SubstructureRedirectMask | SubstructureNotifyMask, &event);
    /* mark hand as unmapped now */
    h->mapped = h->icon->mapped = 0;
  } else {
    Hand *ih = h->icon;
    XDestroyWindow(display, ih->w);
    if (ih->prev) ih->prev->next = ih->next;
    else icon_hands = ih->next;
    if (ih->next) ih->next->prev = ih->prev;
    xfree(ih);
    XDestroyWindow(display, h->w);
    if (h->prev) h->prev->next = h->next;
    else hands = h->next;
    if (h->next) h->next->prev = h->prev;
    xfree(h);
  }
}

int
active_hands(void)
{
  Hand *h;
  int n = 0;
  for (h = hands; h; h = h->next)
    if (h->mapped || h->icon->mapped)
      n++;
  return n;
}


void
ensure_picture(Gif_Stream *gfs, int n)
{
  Picture *p, *last_p, *last_last_p;
  int i;
  
  for (i = 0; i < n; i++) {
    p = (Picture *)gfs->images[i]->user_data;
    if (!p->pix)		/* no picture cached for earlier image */
      ensure_picture(gfs, i);
  }
  
  p = (Picture *)gfs->images[n]->user_data;
  last_p = (n > 0 ? (Picture *)gfs->images[n-1]->user_data : 0);
  last_last_p = (n > 1 ? (Picture *)gfs->images[n-2]->user_data : 0);

  p->pix = Gif_XNextImage(gfx, (last_last_p ? last_last_p->pix : None),
			  (last_p ? last_p->pix : None),
			  gfs, n);
}

void
draw_slide(Hand *h)
{
  Gif_Stream *gfs = h->slideshow;
  Gif_Image *gfi = gfs->images[h->slide];
  Picture *p = (Picture *)gfi->user_data;
  
  if (!p->pix)
    ensure_picture(gfs, h->slide);
  
  XSetWindowBackgroundPixmap(display, h->w, p->pix);
  XClearWindow(display, h->w);
  
  if (h->clock)
    draw_clock(h, 0);
}


Hand *
window_to_hand(Window w, int allow_icons)
{
  Hand *h;
  for (h = hands; h; h = h->next)
    if (h->w == w)
      return h;
  if (allow_icons)
    for (h = icon_hands; h; h = h->next)
      if (h->w == w)
	return h;
  return 0;
}


static void
find_root_child(Hand *h)
{
  Window w = h->w;
  Window root, parent, *children;
  unsigned nchildren;
  h->root_child = None;
  while (XQueryTree(display, w, &root, &parent, &children, &nchildren)) {
    if (children) XFree(children);
    if (parent == root) {
      h->root_child = w;
      break;
    } else
      w = parent;
  }
}

int
default_x_processing(XEvent *e)
{
  Hand *h;
  
  switch (e->type) {
    
   case ConfigureNotify:
    h = window_to_hand(e->xconfigure.window, 1);
    if (!h) break;
    h->configured = 1;
    h->x = e->xconfigure.x;
    h->y = e->xconfigure.y;
    h->width = e->xconfigure.width;
    h->height = e->xconfigure.height;
    find_root_child(h);
    break;
    
   case ReparentNotify:
    if ((h = window_to_hand(e->xreparent.window, 1)))
      find_root_child(h);
    break;
    
   case MapNotify:
    if ((h = window_to_hand(e->xmap.window, 1))) {
      draw_slide(h);
      h->mapped = 1;
    }
    break;
    
   case UnmapNotify:
    if ((h = window_to_hand(e->xmap.window, 1)))
      h->mapped = 0;
    break;
    
   case VisibilityNotify:
    if ((h = window_to_hand(e->xvisibility.window, 0))) {
      if (e->xvisibility.state == VisibilityUnobscured)
	h->obscured = 0;
      else
	h->obscured = 1;
    }
    break;
    
   case Expose:
    h = window_to_hand(e->xexpose.window, 0);
    if (e->xexpose.count == 0 && h && h->clock)
      draw_clock(h, 0);
    break;
    
   case ClientMessage:
    /* Leave e->type == ClientMessage only if it was a DELETE_WINDOW. */
    if (e->xclient.message_type != wm_protocols_atom ||
	(Atom)(e->xclient.data.l[0]) != wm_delete_window_atom)
      e->type = 0;
    else {
      h = window_to_hand(e->xclient.window, 0);
      if (h) destroy_hand(h);
    }
    break;
    
    /* for idle processing */
   case CreateNotify:
    {
      struct timeval now;
      xwGETTIME(now);
      watch_keystrokes(e->xcreatewindow.window, &now);
      break;
    }
    
   case DestroyNotify:
    /* We must unschedule any IdleSelect events for fear of selecting input
       on a destroyed window. There may be a race condition here... */
    unschedule_data(A_IDLE_SELECT, (void *)e->xdestroywindow.window);
    break;
    
   case MappingNotify:
    /* Should listen to X mapping events! */
    XRefreshKeyboardMapping(&e->xmapping);
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
  iv->tv_sec = min * SEC_PER_MIN + integral_sec;
  iv->tv_usec = (int)(MICRO_PER_SEC * (sec - integral_sec));
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
    if (!*format) error("too many arguments to %s", option);
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
      error("incorrect time format in %s argument", option);
    break;
    
   case 's': /* string */
    charptr = va_arg(val, char **);
    *charptr = arg;
    break;
    
   case 'i': /* integer */
    intptr = va_arg(val, int *);
    *intptr = (int)strtol(arg, &arg, 10);
    if (*arg)
      error("argument to %s must be integer", option);
    break;
    
  }
  
 doneargs:
  ARGSHIFT;
  while (*format)
    switch (*format++) {
     case 't':
     case 's':
      error("%s not given enough arguments", option);
    }
  va_end(val);
  return 1;
}


static void
parse_options(int pargc, char **pargv)
{
  char *s;
  Options *o = &onormal;
  Options *p;
  struct timeval flash_delay;
  
  argc = pargc;
  argv = pargv;
  
  while (argc > 0) {
    
    s = argv[0];
    
    if (optparse(s, "after", 1, "sT", &o->next_delay)) {
      p = xwNEW(Options);
      *p = *o;
      o->next = p;
      p->prev = o;
      o = p;
      
    } else if (optparse(s, "breaktime", 1, "st", &break_delay))
      ;
    else if (optparse(s, "beep", 2, "t"))
      o->beep = optparse_yesno;
    else if (optparse(s, "breakclock", 6, "t"))
      o->break_clock = optparse_yesno;
    else if (optparse(s, "bc", 2, "t"))
      o->break_clock = optparse_yesno;
    
    else if (optparse(s, "clock", 1, "t"))
      o->clock = optparse_yesno;
    
    else if (optparse(s, "display", 1, "ss", &display_name))
      ;
    
    else if (optparse(s, "finger", 1, "t")
	     || optparse(s, "flipoff", 1, "t")) {
      o->slideshow_text =
	(optparse_yesno ? FINGER_SLIDESHOW : DEFAULT_SLIDESHOW);
      o->icon_slideshow_text =
	(optparse_yesno ? FINGER_ICON_SLIDESHOW : DEFAULT_ICON_SLIDESHOW);
    } else if (optparse(s, "flashtime", 3, "st", &flash_delay)) {
      double f = flash_delay.tv_sec*MICRO_PER_SEC + flash_delay.tv_usec;
      o->flash_rate_ratio = f / (DEFAULT_FLASH_DELAY_SEC*MICRO_PER_SEC);
    
    } else if (optparse(s, "help", 1, "s")) {
      usage();
      exit(0);
      
    } else if (optparse(s, "iconified", 2, "t"))
      o->appear_iconified = optparse_yesno;
    else if (optparse(s, "idle", 1, "tT", &idle_time))
      check_idle = optparse_yesno;
    
    else if (optparse(s, "lock", 1, "tT", &o->lock_bounce_delay))
      o->lock = optparse_yesno;
    
    else if (optparse(s, "mono", 3, "t"))
      force_mono = optparse_yesno;
    else if (optparse(s, "mouse", 3, "tT", &check_mouse_time))
      check_mouse = optparse_yesno;
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
      error("the nooffscreen option is no longer supported");

    else if (optparse(s, "okp", 1, "ss", &ready_slideshow_text))
      ;
    
    else if (optparse(s, "password", 1, "ss", &lock_password))
      ;

    else if (optparse(s, "rest-picture", 3, "ss", &resting_slideshow_text)
	     || optparse(s, "rp", 2, "ss", &resting_slideshow_text))
      ;
    else if (optparse(s, "ready-picture", 3, "ss", &ready_slideshow_text))
      ;
    
    else if (optparse(s, "typetime", 1, "st", &type_delay))
      ;
    else if (optparse(s, "top", 2, "t"))
      o->top = optparse_yesno;
    
    else if (optparse(s, "version", 1, "s")) {
      printf("LCDF Xwrits %s\n", VERSION);
      printf("\
Copyright (C) 1994-9 Eddie Kohler\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      
    } else if (optparse(s, "warning-picture", 1, "ss", &o->slideshow_text))
      ;
    else if (optparse(s, "wp", 2, "ss", &o->slideshow_text))
      ;
    
    else
      short_usage();
  }
}


static void
check_options(Options *o)
{
  Options *prev = o->prev;
  if (xwTIMELEQ0(o->multiply_delay)) o->multiply_delay = zero;
  
  if (o->max_hands < 1) o->max_hands = 1;
  if (o->max_hands > MaxHands) o->max_hands = MaxHands;
  
  while (xwTIMELEQ0(o->next_delay)) {
    Options *p = o->next;
    /* If the next set of options is supposed to appear before this one,
       replace this one with the next set. Iterate. */
    *o = *p;
    o->prev = prev;
    if (o->next) o->next->prev = o;
    xfree(p);
  }
  
  /* create the slideshows */
  if (prev && strcmp(o->slideshow_text, prev->slideshow_text) == 0
      && o->flash_rate_ratio == prev->flash_rate_ratio) {
    o->slideshow = prev->slideshow;
    o->slideshow->refcount++;
  } else
    o->slideshow = parse_slideshow(o->slideshow_text,
				   o->flash_rate_ratio, force_mono);
  
  if (prev && strcmp(o->icon_slideshow_text, prev->icon_slideshow_text) == 0
      && o->flash_rate_ratio == prev->flash_rate_ratio) {
    o->icon_slideshow = prev->icon_slideshow;
    o->icon_slideshow->refcount++;
  } else
    o->icon_slideshow = parse_slideshow(o->icon_slideshow_text,
					o->flash_rate_ratio, force_mono);
}


#if defined(__cplusplus) || defined(c_plusplus)
#define VISUAL_CLASS c_class
#else
#define VISUAL_CLASS class
#endif

static void
initialize_port(Display *display, int screen_number)
{
  XVisualInfo visi_template;
  int nv, i;
  XVisualInfo *v;
  XVisualInfo *best_v = 0;
  VisualID default_visualid;
  
  /* initialize Port fields */
  port.display = display;
  port.x_socket = ConnectionNumber(display);
  port.screen_number = screen_number;
  port.root_window = RootWindow(display, screen_number);
  port.width = DisplayWidth(display, screen_number);
  port.height = DisplayHeight(display, screen_number);
  
  /* choose the Visual */
  default_visualid = DefaultVisual(display, screen_number)->visualid;
  visi_template.screen = screen_number;
  v = XGetVisualInfo(display, VisualScreenMask, &visi_template, &nv);
  
  for (i = 0; i < nv && !best_v; i++)
    if (v[i].visualid == default_visualid)
      best_v = &v[i];
  
  if (!best_v) {
    port.visual = DefaultVisual(display, screen_number);
    port.depth = DefaultDepth(display, screen_number);
    port.colormap = DefaultColormap(display, screen_number);
  } else {
  
    /* Which visual to choose? This isn't exactly a simple decision, since
       we want to avoid colormap flashing while choosing a nice visual. So
       here's the algorithm: Prefer the default visual, or take a TrueColor
       visual with strictly greater depth. */
    for (i = 0; i < nv; i++)
      if (v[i].depth > best_v->depth && v[i].VISUAL_CLASS == TrueColor)
	best_v = &v[i];
    
    port.visual = best_v->visual;
    port.depth = best_v->depth;
    if (best_v->visualid != default_visualid)
      port.colormap = XCreateColormap(display, port.root_window,
				      port.visual, AllocNone);
    else
      port.colormap = DefaultColormap(display, screen_number);
    
  }
  
  if (v) XFree(v);
  
  /* set up black_pixel and white_pixel */
  {
    XColor color;
    color.red = color.green = color.blue = 0;
    XAllocColor(display, port.colormap, &color);
    port.black = color.pixel;
    color.red = color.green = color.blue = 0xFFFF;
    XAllocColor(display, port.colormap, &color);
    port.white = color.pixel;
  }
  
  /* choose the font */
  port.font = XLoadQueryFont(display, "-*-helvetica-bold-r-*-*-*-180-*");
  
  /* don't set port.drawable until after we create the first hand */
}


static void
default_settings(void)
{
  /* Global default settings */
  SET_TIME(type_delay, 55 * SEC_PER_MIN, 0);
  SET_TIME(break_delay, 5 * SEC_PER_MIN, 0);
  onormal.flash_rate_ratio = 1;
  
  SET_TIME(onormal.multiply_delay, 2, 300000);
  onormal.max_hands = 25;
  
  /* Locking settings */
  SET_TIME(onormal.lock_bounce_delay, 4, 0);
  SET_TIME(lock_message_delay, 10, 0);
  lock_password = "quit";
  
  /* Keystroke registration functions */
  check_idle = 1;
  SET_TIME(idle_time, 0, 0);
  SET_TIME(register_keystrokes_delay, 1, 0);
  
  /* Mouse tracking functions */
  check_mouse = 0;
  SET_TIME(check_mouse_time, 3, 0);
  last_mouse_x = last_mouse_y = 0;
  
  /* Slideshows */
  onormal.slideshow_text = DEFAULT_SLIDESHOW;
  onormal.icon_slideshow_text = DEFAULT_ICON_SLIDESHOW;
  
  /* Clock tick time functions */
  /* 20 seconds seems like a reasonable clock tick time, even though it'll
     redraw the same hands 3 times. */
  SET_TIME(clock_tick, 20, 0);
  
  /* Next option set */
  SET_TIME(onormal.next_delay, 15 * SEC_PER_MIN, 0);
  onormal.next = 0;
  onormal.prev = 0;
}


int
main(int argc, char *argv[])
{
  Hand *hand;
  Options *o;
  int lock_possible = 0;
  
  xwGETTIMEOFDAY(&genesis_time);
  
  srand((getpid() + 1) * time(0));
  
  default_settings();
  
  /* remove first argument = program name */
  parse_options(argc - 1, argv + 1);
  
  if (xwTIMELEQ0(type_delay)) type_delay = zero;
  if (xwTIMELEQ0(break_delay)) break_delay = zero;
  for (o = &onormal; o; o = o->next) {
    check_options(o);
    if (o->lock) lock_possible = 1;
  }
  
  if (strlen(lock_password) >= MaxPasswordSize)
    error("password too long");
  
  display = XOpenDisplay(display_name);
  if (!display) error("cannot open display");
  initialize_port(display, DefaultScreen(display));
  
  ocurrent = &onormal;
  
  resting_slideshow =
    parse_slideshow(resting_slideshow_text, 1, force_mono);
  resting_icon_slideshow =
    parse_slideshow(resting_icon_slideshow_text, 1, force_mono);
  ready_slideshow =
    parse_slideshow(ready_slideshow_text, 1, force_mono);
  ready_icon_slideshow =
    parse_slideshow(ready_icon_slideshow_text, 1, force_mono);
  
  hand = new_hand(NHCenter, NHCenter);
  gfx = Gif_NewXContext(display, hand->w);
  port.drawable = hand->w;
  init_clock(hand->w);
  if (lock_possible) {
    Gif_Image *gfi = get_built_in_image("&locked", force_mono);
    lock_pixmap = Gif_XImage(gfx, 0, gfi);
    Gif_DeleteImage(gfi);
    gfi = get_built_in_image("&bars", force_mono);
    bars_pixmap = Gif_XImage(gfx, 0, gfi);
    Gif_DeleteImage(gfi);
  }
  
  if (check_idle && xwTIMELEQ0(idle_time)) {
    double time = 0.3 * (break_delay.tv_sec +
			(break_delay.tv_usec / (double)MICRO_PER_SEC));
    long integral_time = (long)(floor(time));
    idle_time.tv_sec = integral_time;
    idle_time.tv_usec = (long)(MICRO_PER_SEC * (time - integral_time));
  }
  
  if (check_idle) {
    struct timeval now;
    xwGETTIME(now);
    XSetErrorHandler(x_error_handler);
    watch_keystrokes(port.root_window, &now);
  }
  
  while (1) {
    int tran = 0;
    int was_lock = 0;
    ocurrent = &onormal;
    wait_for_break();
    
    xwGETTIME(first_warning_time);
    while (tran != TRAN_AWAKE && tran != TRAN_CANCEL) {
      
      tran = warning(was_lock);
      
      if (tran == TRAN_REST)
	tran = rest();
      else if (tran == TRAN_LOCK) {
	was_lock = 1;
	tran = lock();
      }
      
    }
    
    if (tran == TRAN_AWAKE)
      ready();
    
    unmap_all();
  }
  
  return 0;
}
