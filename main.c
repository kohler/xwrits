#include <config.h>
#include "xwrits.h"
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <assert.h>

static Options onormal;
Options *ocurrent;

struct timeval genesis_time;
static struct timeval zero = {0, 0};
struct timeval first_warn_time;
struct timeval last_key_time;
static struct timeval normal_type_time;

Gif_Stream *resting_slideshow, *resting_icon_slideshow;
static const char *resting_slideshow_text = "&resting";
static const char *resting_icon_slideshow_text = "&restingicon";

Gif_Stream *ready_slideshow, *ready_icon_slideshow;
static const char *ready_slideshow_text = "&ready";
static const char *ready_icon_slideshow_text = "&readyicon";

Gif_Stream *locked_slideshow;
static const char *locked_slideshow_text = "&locked";

int nports;
Port *ports;

fd_set x_socket_set;
int max_x_socket;

int check_idle;
struct timeval idle_time;
struct timeval warn_idle_time;

int check_mouse;
int mouse_sensitivity;
struct timeval check_mouse_time;

int check_quota;
struct timeval quota_time;
struct timeval quota_allotment;

#define MAX_CHEATS_UNSET -97979797
int max_cheats;
static int allow_cheats;

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
All options may be abbreviated to their unique prefixes. The three forms\n\
`OPTION', `--OPTION', and `+OPTION' are equivalent. Options listed as\n\
`+OPTION' can be on or off; they are normally off, and you can turn them off\n\
explicitly with `-OPTION'.\n\
\n\
General options:\n\
  --display DISPLAY   Monitor the X display DISPLAY. You can monitor more than\n\
                      one display by giving this option multiple times.\n\
  --help              Print this message and exit.\n\
  --version           Print version number and exit.\n\
\n");
  printf("\
Break characteristics:\n\
  typetime=TIME, t=TIME     Allow typing for TIME (default 55 minutes).\n\
  breaktime=TIME, b=TIME    Breaks last for TIME (default 5 minutes).\n\
  +lock               Lock the keyboard during the break.\n\
  password=TEXT       Set the password for unlocking the keyboard.\n\
  +mouse              Monitor your mouse movements.\n\
  +idle[=TIME]        Leaving the keyboard idle for TIME is the same as taking\n\
                      a break. On by default. Default TIME is breaktime.\n\
  +quota[=TIME]       Leaving the keyboard idle for more than TIME reduces next\n\
                      break length by the idle time. Default TIME is 1 minute.\n\
  minbreaktime=TIME   Minimum break length is TIME (see +quota).\n\
  +cheat[=NUM]        Allow NUM keystrokes before cancelling a break.\n\
  canceltime=TIME, ct=TIME  Allow typing for TIME after a break is cancelled\n\
                      (default 10 minutes).\n\
\n");
  printf("\
Appearance:\n\
  after=TIME          Change behavior if warning window is ignored for TIME.\n\
                      Options following `after' give new behavior.\n\
  +beep               Beep when the warning window appears.\n\
  +breakclock         Show how much time remains during the break.\n\
  +clock              Show how long you have ignored the warning window.\n\
  +finger             Be rude.\n\
  +finger=CULTURE     Be rude according to CULTURE. Choices: `american',\n\
                      `korean' (synonyms `japanese', `russian'), `german'.\n\
  flashtime=RATE      Flash the warning window at RATE (default 2 sec).\n\
  +iconified          Warning windows appear as icons.\n\
  lock-picture=GIF-FILE, lp=GIF-FILE     Show GIF animation on lock window.\n\
  maxhands=NUM        Maximum number of warning windows is NUM (default 25).\n\
  +mono               Use monochrome pictures.\n\
  +multiply=PERIOD    Make a new warning window every PERIOD.\n\
  +noiconify          Don't let anyone iconify the warning window.\n\
  ready-picture=GIF-FILE, okp=GIF-FILE   Show GIF animation on the `OK' window.\n\
  rest-picture=GIF-FILE, rp=GIF-FILE     Show GIF animation on resting window.\n\
  +top                Keep the warning windows on top of the window stack.\n\
  warning-picture=GIF-FILE, wp=GIF-FILE  Show GIF animation on the warning\n\
                      window.\n\
\n\
Report bugs and suggestions to <eddietwo@lcs.mit.edu>.\n");
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

void
warning(const char *format, ...)
{
  va_list val;
  va_start(val, format);
  fprintf(stderr, "xwrits: warning: ");
  vfprintf(stderr, format, val);
  fprintf(stderr, "\n");
  va_end(val);
}

void
message(const char *format, ...)
{
  va_list val;
  va_start(val, format);
  fprintf(stderr, "xwrits: ");
  vfprintf(stderr, format, val);
  fprintf(stderr, "\n");
  va_end(val);
}



/* default X processing */

static void
find_root_child(Hand *h)
{
  Display *display = h->port->display;
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
  Display *display = e->xany.display;
  Port *port = find_port(display);
  
  switch (e->type) {
    
   case ConfigureNotify:
    h = window_to_hand(port, e->xconfigure.window, 1);
    if (!h) break;
    h->configured = 1;
    h->x = e->xconfigure.x;
    h->y = e->xconfigure.y;
    h->width = e->xconfigure.width;
    h->height = e->xconfigure.height;
    find_root_child(h);
    break;
    
   case ReparentNotify:
    if ((h = window_to_hand(port, e->xreparent.window, 1)))
      find_root_child(h);
    break;
    
   case MapNotify:
    if ((h = window_to_hand(port, e->xmap.window, 1))) {
      draw_slide(h);
      h->mapped = 1;
    }
    break;
    
   case UnmapNotify:
    if ((h = window_to_hand(port, e->xmap.window, 1)))
      h->mapped = 0;
    break;
    
   case VisibilityNotify:
    if ((h = window_to_hand(port, e->xvisibility.window, 0))) {
      if (e->xvisibility.state == VisibilityUnobscured)
	h->obscured = 0;
      else
	h->obscured = 1;
    }
    break;
    
   case Expose:
    h = window_to_hand(port, e->xexpose.window, 0);
    if (e->xexpose.count == 0 && h && h->clock)
      draw_clock(h, 0);
    break;
    
   case ClientMessage:
    /* Leave e->type == ClientMessage only if it was a DELETE_WINDOW. */
    if (e->xclient.message_type != port->wm_protocols_atom ||
	(Atom)(e->xclient.data.l[0]) != port->wm_delete_window_atom)
      e->type = 0;
    else {
      h = window_to_hand(port, e->xclient.window, 0);
      if (h) destroy_hand(h);
    }
    break;
    
   /* for idle processing */
   case CreateNotify: {
     struct timeval now;
     xwGETTIME(now);
     watch_keystrokes(display, e->xcreatewindow.window, &now);
     break;
   }
   
   case DestroyNotify:
    /* We must unschedule any IdleSelect events for fear of selecting input
       on a destroyed window. There is a race condition here because of the
       asynchronicity of X communication. */
    unschedule_data(A_IDLE_SELECT, (void *)e->xdestroywindow.display,
		    (void *)e->xdestroywindow.window);
    break;
    
   case MappingNotify:
    /* Should listen to X mapping events! */
    XRefreshKeyboardMapping(&e->xmapping);
    break;
    
  }
  
  return 0;
}


/* option parsing */

static int
strtointerval(char *s, char **stores, struct timeval *iv)
{
  double sec = 0;
  long integral_sec;
  int ok = 0;
  
  /* read minutes */
  if (isdigit(*s)) {
    sec = strtol(s, &s, 10) * SEC_PER_MIN;
    ok = 1;
  }
  if (*s == '.') {
    sec += strtod(s, &s) * SEC_PER_MIN;
    ok = 1;
  }
  
  /* read seconds */
  if (*s == ':' && (isdigit(s[1]) || s[1] == '.')) {
    sec += strtod(s+1, &s);
    ok = 1;
  } else if (*s == ':' && s[1] == 0)
    /* accept `MINUTES:' */
    s++;

  /* return */
  if (stores) *stores = s;
  if (!ok || *s != 0) return 0;
  integral_sec = (long)(floor(sec));
  iv->tv_sec = integral_sec;
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
    
   case 't': /* time */
   case 'T': /* optional time */
    timeptr = va_arg(val, struct timeval *);
    if (!strtointerval(arg, &arg, timeptr))
      error("incorrect time format in %s argument", option);
    break;
    
   case 's': /* string */
   case 'S': /* optional string */
    charptr = va_arg(val, char **);
    *charptr = arg;
    break;
    
   case 'i': /* integer */
   case 'I': /* optional integer */
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
     case 'i':
      error("%s not given enough arguments", option);
      break;
    }
  va_end(val);
  return 1;
}

/* note: memory leak on slideshow texts */
static void
slideshow_text_append_built_in(Options *o, char *built_in)
{
  int lbi = strlen(built_in);
  int ls = (o->slideshow_text ? strlen(o->slideshow_text) : 0);
  int lis = (o->icon_slideshow_text ? strlen(o->icon_slideshow_text) : 0);
  char *s = xwNEWARR(char, ls + lbi + 3);
  char *is = xwNEWARR(char, lis + lbi + 7);
  
  sprintf(s, "%s%s&%s", (ls ? o->slideshow_text : ""),
	  (ls ? ";" : ""), built_in);
  sprintf(is, "%s%s&%sicon", (lis ? o->icon_slideshow_text : ""),
	  (lis ? ";" : ""), built_in);
  
  if (!o->prev || o->prev->slideshow_text != o->slideshow_text)
    xfree(o->slideshow_text);
  if (!o->prev || o->prev->icon_slideshow_text != o->icon_slideshow_text)
    xfree(o->icon_slideshow_text);
  
  o->slideshow_text = s;
  o->icon_slideshow_text = is;
}

static void
parse_options(int pargc, char **pargv)
{
  char *s;
  char *arg;
  Options *o = &onormal;
  Options *p;
  struct timeval flash_delay;
  int breaktime_warn_context = 0;
  
  argc = pargc;
  argv = pargv;

  while (argc > 0) {
    
    s = argv[0];
    arg = 0;
    
    if (optparse(s, "after", 1, "sT", &o->next_delay)) {
      p = xwNEW(Options);
      *p = *o;
      o->next = p;
      p->prev = o;
      o = p;
      
    } else if (optparse(s, "breaktime", 1, "st", &o->break_time)) {
      if (o->prev && !breaktime_warn_context) {
	warning("meaning of `breaktime' following `after' has changed");
	message("(You can specify `breaktime' multiple times, to lengthen a break");
	message("if you ignore xwrits for a while. In previous versions, there was");
	message("one global `breaktime'.)");
	breaktime_warn_context = 1;
      } else
	breaktime_warn_context = 1;
    } else if (optparse(s, "beep", 2, "t"))
      o->beep = optparse_yesno;
    else if (optparse(s, "breakclock", 6, "t"))
      o->break_clock = optparse_yesno;
    else if (optparse(s, "bc", 2, "t"))
      o->break_clock = optparse_yesno;
    
    else if (optparse(s, "canceltime", 2, "sT", &o->cancel_type_time)
	     || optparse(s, "ct", 2, "sT", &o->cancel_type_time))
      ;
    else if (optparse(s, "cheat", 2, "tI", &max_cheats))
      allow_cheats = optparse_yesno;
    else if (optparse(s, "clock", 1, "t"))
      o->clock = optparse_yesno;
    
    else if (optparse(s, "display", 1, "ss", &arg)) {
      if (nports == 1 && ports[0].display_name == 0)
	ports[0].display_name = arg;
      else {
	nports++;
	ports = (Port *)xrealloc(ports, sizeof(Port) * nports);
	ports[nports-1].display_name = arg;
      }
    
    } else if (optparse(s, "finger", 1, "tS", &arg)
	      || optparse(s, "flipoff", 1, "tS", &arg)) {
      if (!optparse_yesno) {
	o->slideshow_text = o->icon_slideshow_text = 0;
	slideshow_text_append_built_in(o, "clench");
	slideshow_text_append_built_in(o, "spread");
      } else if (arg)
	slideshow_text_append_built_in(o, arg);
      else
	slideshow_text_append_built_in(o, "american");
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
    else if (optparse(s, "lock-picture", 5, "ss", &locked_slideshow_text)
	     || optparse(s, "lp", 2, "ss", &locked_slideshow_text))
      ;

    else if (optparse(s, "minbreaktime", 2, "st", &o->min_break_time))
      ;
    else if (optparse(s, "mono", 3, "t"))
      force_mono = optparse_yesno;
    else if (optparse(s, "mouse", 3, "tI", &mouse_sensitivity))
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
    
    else if (optparse(s, "quota", 1, "tT", &quota_time))
      check_quota = optparse_yesno;
    
    else if (optparse(s, "rest-picture", 3, "ss", &resting_slideshow_text)
	     || optparse(s, "rp", 2, "ss", &resting_slideshow_text))
      ;
    else if (optparse(s, "ready-picture", 3, "ss", &ready_slideshow_text))
      ;
    
    else if (optparse(s, "typetime", 1, "st", &normal_type_time))
      ;
    else if (optparse(s, "top", 2, "t"))
      o->top = optparse_yesno;
    
    else if (optparse(s, "version", 1, "s")) {
      printf("LCDF Xwrits %s\n", VERSION);
      printf("\
Copyright (C) 1994-2001 Eddie Kohler\n\
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


/* option checking */

static void
set_fraction_time(struct timeval *result, struct timeval in, double fraction)
{
  double d = fraction * (in.tv_sec + (in.tv_usec / (double)MICRO_PER_SEC));
  long integral_d = (long)(floor(d));
  result->tv_sec = integral_d;
  result->tv_usec = (long)(MICRO_PER_SEC * (d - integral_d));
}

static void
check_options(Options *o)
{
  Options *prev = o->prev;

  /* check multiply_delay */
  if (xwTIMELEQ0(o->multiply_delay))
    o->multiply_delay = zero;

  /* check max_hands */
  if (o->max_hands < 1)
    o->max_hands = 1;
  else if (o->max_hands > MAX_HANDS)
    o->max_hands = MAX_HANDS;
  
  /* check min_break_time */
  if (xwTIMELT0(o->min_break_time)) {
    set_fraction_time(&o->min_break_time, o->break_time, 0.5);
    if (xwTIMEGEQ(quota_time, o->min_break_time))
      o->min_break_time = quota_time;
  }

  /* check cancel_type_time */
  if (xwTIMELT0(o->cancel_type_time)) {
    xwSETTIME(o->cancel_type_time, 10 * SEC_PER_MIN, 0);
    if (xwTIMEGT(o->cancel_type_time, normal_type_time))
      o->cancel_type_time = normal_type_time;
  }
  
  /* If the next set of options is supposed to appear before this one, replace
     this one with the next set. Iterate. */
  while (xwTIMELEQ0(o->next_delay)) {
    Options *p = o->next;
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


/* default option settings */

static void
default_settings(void)
{
  /* time settings */
  xwSETTIME(normal_type_time, 55 * SEC_PER_MIN, 0);
  xwSETTIME(onormal.break_time, 5 * SEC_PER_MIN, 0);
  xwSETTIME(onormal.cancel_type_time, -1, 0);
  xwSETTIME(onormal.min_break_time, -1, 0);
  onormal.flash_rate_ratio = 1;

  /* multiply settings */
  xwSETTIME(onormal.multiply_delay, 2, 300000);
  onormal.max_hands = 25;
  
  /* locking settings */
  xwSETTIME(onormal.lock_bounce_delay, 4, 0);
  xwSETTIME(lock_message_delay, 10, 0);
  lock_password = "quit";
  
  /* keystroke registration functions */
  check_idle = 1;
  xwSETTIME(idle_time, 0, 0);
  xwSETTIME(register_keystrokes_delay, 1, 0);
  
  /* mouse tracking functions */
  check_mouse = 0;
  xwSETTIME(check_mouse_time, 3, 0);
  mouse_sensitivity = 15;

  /* quota settings */
  check_quota = 0;
  xwSETTIME(quota_time, 60, 0);
  
  /* cheating */
  max_cheats = MAX_CHEATS_UNSET;
  allow_cheats = 0;
  
  /* slideshows */
  onormal.slideshow_text = onormal.icon_slideshow_text = 0;
  slideshow_text_append_built_in(&onormal, "clench");
  slideshow_text_append_built_in(&onormal, "spread");
  
  /* clock tick time functions */
  /* 20 seconds seems like a reasonable clock tick time, even though it'll
     redraw the same hands 3 times. */
  xwSETTIME(clock_tick, 1, 0);
  
  /* next option set */
  xwSETTIME(onormal.next_delay, 15 * SEC_PER_MIN, 0);
  onormal.next = 0;
  onormal.prev = 0;
}


/* initialize port, for X stuff */

#if defined(__cplusplus) || defined(c_plusplus)
#define VISUAL_CLASS c_class
#else
#define VISUAL_CLASS class
#endif

static void
initialize_port(Port *port, Display *display, int screen_number)
{
  XVisualInfo visi_template;
  int nv, i;
  XVisualInfo *v;
  XVisualInfo *best_v = 0;
  VisualID default_visualid;
  
  /* initialize Port fields */
  port->display = display;
  port->x_socket = ConnectionNumber(display);
  port->screen_number = screen_number;
  port->root_window = RootWindow(display, screen_number);
  port->width = DisplayWidth(display, screen_number);
  port->height = DisplayHeight(display, screen_number);
  
  /* choose the Visual */
  default_visualid = DefaultVisual(display, screen_number)->visualid;
  visi_template.screen = screen_number;
  v = XGetVisualInfo(display, VisualScreenMask, &visi_template, &nv);
  
  for (i = 0; i < nv && !best_v; i++)
    if (v[i].visualid == default_visualid)
      best_v = &v[i];
  
  if (!best_v) {
    port->visual = DefaultVisual(display, screen_number);
    port->depth = DefaultDepth(display, screen_number);
    port->colormap = DefaultColormap(display, screen_number);
  } else {
  
    /* Which visual to choose? This isn't exactly a simple decision, since
       we want to avoid colormap flashing while choosing a nice visual. So
       here's the algorithm: Prefer the default visual, or take a TrueColor
       visual with strictly greater depth. */
    for (i = 0; i < nv; i++)
      if (v[i].depth > best_v->depth && v[i].VISUAL_CLASS == TrueColor)
	best_v = &v[i];
    
    port->visual = best_v->visual;
    port->depth = best_v->depth;
    if (best_v->visualid != default_visualid)
      port->colormap = XCreateColormap(display, port->root_window,
				       port->visual, AllocNone);
    else
      port->colormap = DefaultColormap(display, screen_number);
    
  }
  
  if (v) XFree(v);
  
  /* set up black_pixel and white_pixel */
  {
    XColor color;
    color.red = color.green = color.blue = 0;
    XAllocColor(display, port->colormap, &color);
    port->black = color.pixel;
    color.red = color.green = color.blue = 0xFFFF;
    XAllocColor(display, port->colormap, &color);
    port->white = color.pixel;
  }
  
  /* choose the font */
  port->font = XLoadQueryFont(display, "-*-helvetica-bold-r-*-*-*-180-*");
  
  /* set gfx */
  port->gfx = Gif_NewXContextFromVisual
    (display, port->screen_number, port->visual, port->depth, port->colormap);

  /* set atoms */
  port->wm_delete_window_atom = XInternAtom(display, "WM_DELETE_WINDOW", False);
  port->wm_protocols_atom = XInternAtom(display, "WM_PROTOCOLS", False);
  port->mwm_hints_atom = XInternAtom(display, "_MOTIF_WM_HINTS", False);

  /* create first hand for this port, set drawable */
  {
    Hand *hand = new_hand(port, NEW_HAND_CENTER, NEW_HAND_CENTER);
    port->drawable = hand->w;
    hand->permanent = 1;
  }

  /* initialize GCs */
  {
    XGCValues gcv;
    
    gcv.foreground = port->black;
    gcv.line_width = 3;
    gcv.cap_style = CapRound;
    port->clock_fore_gc = XCreateGC
      (port->display, port->drawable,
       GCForeground | GCLineWidth | GCCapStyle, &gcv);

    gcv.line_width = 1;
    port->clock_hand_gc = XCreateGC
      (port->display, port->drawable,
       GCForeground | GCLineWidth | GCCapStyle, &gcv);
    
    gcv.foreground = port->white;
    gcv.font = port->font->fid;
    gcv.subwindow_mode = IncludeInferiors;
    port->white_gc = XCreateGC
      (port->display, port->drawable,
       GCForeground | GCFont | GCSubwindowMode, &gcv);
  }
  
  /* initialize other stuff */
  port->icon_width = port->icon_height = 0;
  port->last_mouse_root = None;
  port->bars_pixmap = None;
}

Port *
find_port(Display *display)
{
  int i;
  for (i = 0; i < nports; i++)
    if (ports[i].display == display)
      return &ports[i];
  return 0;
}


/* main! */

int
main(int argc, char *argv[])
{
  Options *o;
  int i;
  int lock_possible = 0;
  struct timeval now;
  struct timeval type_time;
  
  xwGETTIMEOFDAY(&genesis_time);
  init_scheduler();
  
  srand((getpid() + 1) * time(0));
  
  nports = 1;
  ports = (Port *)xmalloc(sizeof(Port));
  
  /* parse options. remove first argument = program name */
  default_settings();  
  parse_options(argc - 1, argv + 1);
  
  /* check global options */
  if (strlen(lock_password) >= MAX_PASSWORD_SIZE)
    error("password too long");
  
  /* default for idle_time and warn_idle_time is based on break_delay;
     otherwise, warn_idle_time == idle_time */
  if (check_idle && xwTIMELEQ0(idle_time)) {
    idle_time = onormal.break_time;
    set_fraction_time(&warn_idle_time, idle_time, 0.3);
  } else
    warn_idle_time = idle_time;
  
  /* check quota */
  if (check_quota && check_idle && xwTIMEGEQ(quota_time, onormal.break_time)) {
    warning("quota time is longer than break length");
    warning("(With +quota=TIME, unexpected breaks longer than TIME reduce");
    warning("the length of the main break, so a TIME longer than the main");
    warning("break is probably a mistake.)");
  }
  
  /* fix cheats */
  if (!allow_cheats)
    max_cheats = 0;
  else if (max_cheats == MAX_CHEATS_UNSET)
    max_cheats = 5;
  
  /* check local options */
  for (o = &onormal; o; o = o->next) {
    check_options(o);
    if (o->lock) lock_possible = 1;
  }

  /* load resting and ready slideshows */
  resting_slideshow = parse_slideshow(resting_slideshow_text, 1, force_mono);
  resting_icon_slideshow = parse_slideshow(resting_icon_slideshow_text, 1, force_mono);
  ready_slideshow = parse_slideshow(ready_slideshow_text, 1, force_mono);
  ready_icon_slideshow = parse_slideshow(ready_icon_slideshow_text, 1, force_mono);
  locked_slideshow = parse_slideshow(locked_slideshow_text, 1, force_mono);
  ocurrent = &onormal;

  /* set up displays */
  FD_ZERO(&x_socket_set);
  max_x_socket = 0;
  for (i = 0; i < nports; i++) {
    Display *display = XOpenDisplay(ports[i].display_name);
    if (!display) error("can't open display `%s'", ports[i].display_name);
    initialize_port(&ports[i], display, DefaultScreen(display));
    ports[i].port_number = i;
    FD_SET(ports[i].x_socket, &x_socket_set);
    if (ports[i].x_socket > max_x_socket)
      max_x_socket = ports[i].x_socket;
  }

  /* initialize pictures using first hand */
  if (lock_possible) {
    Gif_Stream *bgfs = get_built_in_image(force_mono?"barsmono":"bars");
    if (!bgfs && force_mono) bgfs = get_built_in_image("bars");
    for (i = 0; i < nports; i++)
      ports[i].bars_pixmap = Gif_XImage(ports[i].gfx, bgfs, 0);
  }
  
  /* watch keystrokes on all windows */
  xwGETTIME(now);
  XSetErrorHandler(x_error_handler);
  for (i = 0; i < nports; i++)
    watch_keystrokes(ports[i].display, ports[i].root_window, &now);
  
  /* start mouse checking */
  if (check_mouse) {
    Alarm *a = new_alarm(A_MOUSE);
    xwGETTIME(a->timer);
    schedule(a);
  }
  
  /* main loop */
  ocurrent = &onormal;		/* start with normal options */
  type_time = normal_type_time;
  
  while (1) {
    int tran, was_lock;
    
    /* wait for break */
    tran = wait_for_break(&type_time);
    if (tran == TRAN_REST) {
      type_time = normal_type_time; /* reset to normal type time */
      continue;
    }
    
    /* warn */
    xwGETTIME(first_warn_time);
    tran = was_lock = 0;
    while (tran != TRAN_AWAKE && tran != TRAN_CANCEL) {
      /* warn() will figure out current options by time */
      tran = warn(was_lock, &onormal);
      if (tran == TRAN_REST) {
	was_lock = 0;
	tran = rest();
      } else if (tran == TRAN_LOCK) {
	was_lock = 1;
	tran = lock();
      }
    }
    
    /* done with break? */
    if (tran == TRAN_AWAKE) {
      ready();
      type_time = normal_type_time; /* reset to normal type time */
    } else			/* tran == TRAN_CANCEL */
      type_time = ocurrent->cancel_type_time;
    
    unmap_all();
  }
  
  return 0;
}
