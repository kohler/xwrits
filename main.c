/* -*- c-basic-offset: 2 -*- */
#include <config.h>
#include "xwrits.h"
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <assert.h>
#include <X11/Xatom.h>
#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

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

Gif_Stream *bars_slideshow;
static const char *bars_slideshow_text = 0;

int nports;
Port **ports;

fd_set x_socket_set;
int max_x_socket;
XErrorHandler old_x_error_handler;

int check_idle;
struct timeval idle_time;

int check_mouse;
int mouse_sensitivity;
struct timeval check_mouse_time;

int check_quota;
struct timeval quota_time;
struct timeval quota_allotment;

#define MAX_CHEATS_UNSET -97979797
int max_cheats;
static int allow_cheats;

static int run_once;

int verbose;

static int force_mono = 0;
static int multiscreen = 0;


static void
short_usage(void)
{
  fprintf(stderr, "Usage: xwrits [-display DISPLAY] [typetime=TIME] [breaktime=TIME] [OPTION]...\n\
Try 'xwrits --help' for more information.\n");
  exit(1);
}


static void
usage(void)
{
  printf("\
'Xwrits' reminds you to take wrist breaks, which should help you prevent or\n\
manage a repetitive stress injury. It runs on X.\n\
\n\
Usage: xwrits [-display DISPLAY] [typetime=TIME] [breaktime=TIME] [OPTION]...\n\
\n\
All options may be abbreviated to their unique prefixes. The three forms\n\
'OPTION', '--OPTION', and '+OPTION' are equivalent. Options listed as\n\
'+OPTION' can be on or off; they are normally off, and you can turn them off\n\
explicitly with '-OPTION'.\n\
\n\
General options:\n\
  --display DISPLAY   Monitor the X display DISPLAY. You can monitor more than\n\
                      one display by giving this option multiple times.\n\
  --multiscreen       Open every screen for each DISPLAY.\n\
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
  +once               Quit after the warning window is clicked on. Useful for\n\
                      setting alarms.\n\
\n");
  printf("\
Appearance:\n\
  after=TIME          Change behavior if warning window is ignored for TIME.\n\
                      Options following 'after' give new behavior.\n\
  +beep               Beep when the warning window appears.\n\
  +breakclock         Show how much time remains during the break.\n\
  +clock              Show how long you have ignored the warning window.\n\
  +finger             Be rude.\n\
  +finger=CULTURE     Be rude according to CULTURE. Choices: 'american',\n\
                      'korean' (synonyms 'japanese', 'russian'), 'german',\n\
                      or the name of any GIF file.\n\
  flashtime=RATE      Flash the warning window at RATE (default 2 sec).\n\
  +iconified          Warning windows appear as icons.\n\
  lock-picture=GIF-FILE, lp=GIF-FILE     Show GIF animation on lock window.\n\
  maxhands=NUM        Maximum number of warning windows is NUM (default 25).\n\
  +mono               Use monochrome pictures.\n\
  +multiply=PERIOD    Make a new warning window every PERIOD.\n\
  +noiconify          Don't let anyone iconify the warning window.\n\
  ready-picture=GIF-FILE, okp=GIF-FILE   Show GIF animation on the 'OK' window.\n\
  rest-picture=GIF-FILE, rp=GIF-FILE     Show GIF animation on resting window.\n\
  title=TITLE         Set xwrits window title to TITLE.\n\
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



/* ports and peers */

static Port *
add_port(const char *display_name, Display *display, int screen_number,
	 Port *master)
{
    Port *p = (Port *)xmalloc(sizeof(Port));
    ports = (Port **)xrealloc(ports, sizeof(Port *) * (nports + 1));
    if (!ports || !p)
	error("out of memory!");
    ports[nports] = p;
    memset(p, 0, sizeof(Port));
    p->display_name = display_name;
    p->display = display;
    p->screen_number = screen_number;
    p->master = (master ? master : p);
    p->nslaves = 0;
    if (master)
	master->nslaves++;
    p->port_number = nports;
    nports++;
    return ports[nports - 1];
}

Port *
find_port(Display *display, Window window)
{
    int screen_number, i;

    /* first, look using only 'display' */
    for (i = 0; i < nports; i++)
	if (ports[i]->display == display && ports[i]->display_unique)
	    return ports[i]->master;

    /* if display not unique (or not found), try 'screen_number' also */
    screen_number = 0;
    if (window) {
	XWindowAttributes attr;
	if (XGetWindowAttributes(display, window, &attr) != 0)
	    screen_number = XScreenNumberOfScreen(attr.screen);
    }
    for (i = 0; i < nports; i++)
	if (ports[i]->display == display && ports[i]->screen_number == screen_number)
	    return ports[i]->master;

    /* nothing found */
    return 0;
}

void
add_peer(Port *port, Window peer)
{
  XEvent e;
  int i;
  if (peer == port->permanent_hand->w)
    return;
  for (i = 0; i < port->npeers; i++)
    if (peer == port->peers[i])
      return;
  if (port->npeers == port->peers_capacity) {
    port->peers_capacity *= 2;
    xwREARRAY(port->peers, Window, port->peers_capacity);
  }
  port->peers[port->npeers++] = peer;
  /* send peering message to the peer */
  e.xclient.type = ClientMessage;
  e.xclient.window = peer;
  e.xclient.message_type = port->xwrits_notify_peer_atom;
  e.xclient.format = 32;
  e.xclient.data.l[0] = port->permanent_hand->w;
  XSendEvent(port->display, peer, True, 0, &e);
}

void
notify_peers_rest(void)
{
  int i, j;
  for (i = 0; i < nports; i++)
    for (j = 0; j < ports[i]->npeers; j++)
      if (check_xwrits_window(ports[i], ports[i]->peers[j])) {
	XEvent e;
	e.xclient.type = ClientMessage;
	e.xclient.window = ports[i]->peers[j];
	e.xclient.message_type = ports[i]->xwrits_break_atom;
	e.xclient.format = 32;
	e.xclient.data.l[0] = 1;
	XSendEvent(ports[i]->display, ports[i]->peers[j], True, 0, &e);
      } else {
	/* remove peer */
	ports[i]->peers[j] = ports[i]->peers[ ports[i]->npeers - 1 ];
	ports[i]->npeers--;
	j--;
      }
}

void
mark_xwrits_window(Port *port, Window w)
{
  uint32_t scrap = port->permanent_hand->w;
  assert(scrap);
  XChangeProperty(port->display, w, port->xwrits_window_atom,
		  XA_INTEGER, 32, PropModeReplace,
		  (unsigned char *)&scrap, 1);
  XChangeProperty(port->display, w, port->wm_client_leader_atom,
		  XA_WINDOW, 32, PropModeReplace,
		  (unsigned char *)&scrap, 1);
}

Window
check_xwrits_window(Port *port, Window w)
{
  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  union { unsigned char *uc; long *l; } prop;
  if (XGetWindowProperty(port->display, w, port->xwrits_window_atom,
			 0, 1, False, XA_INTEGER,
			 &actual_type, &actual_format, &nitems, &bytes_after,
			 &prop.uc) == Success) {
    Window xwrits_window = (actual_format ? *prop.l : None);
    XFree(prop.uc);
    return xwrits_window;
  } else
    return None;
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
      /* set XWRITS_WINDOW property on child of root */
      mark_xwrits_window(h->port, w);
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
  Port *port;
  
  switch (e->type) {
    
   case ConfigureNotify:
    port = find_port(display, e->xconfigure.window);
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
    port = find_port(display, e->xreparent.window);
    if ((h = window_to_hand(port, e->xreparent.window, 1)))
      find_root_child(h);
    break;
    
   case MapNotify:
    port = find_port(display, e->xmap.window);
    if ((h = window_to_hand(port, e->xmap.window, 1))) {
      draw_slide(h);
      h->mapped = 1;
    }
    break;
    
   case UnmapNotify:
    port = find_port(display, e->xunmap.window);
    if ((h = window_to_hand(port, e->xunmap.window, 1)))
      h->mapped = 0;
    break;
    
   case VisibilityNotify:
    port = find_port(display, e->xvisibility.window);
    if ((h = window_to_hand(port, e->xvisibility.window, 0))) {
      if (e->xvisibility.state == VisibilityUnobscured)
	h->obscured = 0;
      else
	h->obscured = 1;
    }
    break;
    
   case Expose:
    port = find_port(display, e->xexpose.window);
    h = window_to_hand(port, e->xexpose.window, 0);
    if (e->xexpose.count == 0 && h && h->clock)
      draw_clock(h, 0);
    break;
    
   case ClientMessage:
    /* change e->type depending on the message */
    port = find_port(display, e->xclient.window);
    h = window_to_hand(port, e->xclient.window, 0);
    if (h) {
      if (e->xclient.message_type == port->wm_protocols_atom
	  && (Atom)(e->xclient.data.l[0]) == port->wm_delete_window_atom) {
	/* window manager delete */
	e->type = Xw_DeleteWindow;
	destroy_hand(h);
      } else if (e->xclient.message_type == port->wm_protocols_atom
		 && (Atom)(e->xclient.data.l[0]) == port->net_wm_ping_atom) {
	/* window manager pinging us to make sure we're still alive */
	e->xclient.window = port->root_window;
	XSendEvent(port->display, port->root_window, True, 0, e);
      } else if (e->xclient.message_type == port->xwrits_break_atom
		 && e->xclient.data.l[0] != 0)
	e->type = Xw_TakeBreak;
      else if (e->xclient.message_type == port->xwrits_notify_peer_atom) {
	Window w = (Window)e->xclient.data.l[0];
	add_peer(port, w);
      }
    }
    break;
    
   /* for idle processing */
   case CreateNotify: {
     struct timeval now;
     xwGETTIME(now);
     port = find_port(display, e->xcreatewindow.window);
     watch_keystrokes(port, e->xcreatewindow.window, &now);
     break;
   }
   
   case DestroyNotify:
    /* We must unschedule any IdleSelect events for fear of selecting input on
       a destroyed window. There is a race condition here because X
       communication is asynchronous. */
    unschedule_data(A_IDLE_SELECT, (void *) e->xdestroywindow.window);
    if (verbose)
	fprintf(stderr, "Window 0x%x: destroyed\n", (unsigned)e->xdestroywindow.window);
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
    /* accept 'MINUTES:' */
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
  if (arg[0] == '-' && arg[1] == '-' && isalnum(arg[2]))
    arg += 2;

  if (*format == 't') {
      /* Toggle switch. -[option] means off; +[option] or [option] means on.
	 Arguments must be given with = syntax. */

      /* Allow --no-WHATEVER as well as +WHATEVER. */
      optparse_yesno = 1;
      if (arg[0] == '-' && arg[1] == 'n' && arg[2] == 'o' && arg[3] == '-')
	  arg++;
      while (arg[0]== 'n' && arg[1] == 'o' && arg[2] == '-' && isalnum(arg[3]))
	  optparse_yesno = !optparse_yesno, arg += 3;
      
      if (arg[0] == '-')
	  optparse_yesno = 0, arg++;
      else if (arg[0] == '+')
	  optparse_yesno = 1, arg++;
    
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
  /* 4.Sep.2002 - Prevent inappropriate identification of an option (could
     mistake --helpcdsainhfds for --help). */
  if (unique > 0 || (*arg && *arg != '=')) return 0;
  
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
  sprintf(is, "%s%s*%sicon", (lis ? o->icon_slideshow_text : ""),
	  (lis ? ";" : ""), built_in);
  
  if (!o->prev || o->prev->slideshow_text != o->slideshow_text)
    xfree((char *)o->slideshow_text);
  if (!o->prev || o->prev->icon_slideshow_text != o->icon_slideshow_text)
    xfree((char *)o->icon_slideshow_text);
  
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
      
    } else if (optparse(s, "bars-picture", 2, "ss", &bars_slideshow_text)
	       || optparse(s, "bp", 2, "ss", &bars_slideshow_text))
      ;
    else if (optparse(s, "breaktime", 1, "st", &o->break_time)) {
      if (o->prev && !breaktime_warn_context) {
	warning("meaning of 'breaktime' following 'after' has changed");
	message("(You can specify 'breaktime' multiple times, to lengthen a break");
	message("if you ignore xwrits for a while. In previous versions, there was");
	message("one global 'breaktime'.)");
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
    else if (optparse(s, "once", 2, "tI", &run_once)) {
	if (optparse_yesno && run_once < 0)
	    run_once = 1;
	else if (!optparse_yesno)
	    run_once = 0;
    } else if (optparse(s, "clock", 1, "t"))
      o->clock = optparse_yesno;
    
    else if (optparse(s, "display", 1, "ss", &arg)) {
      if (nports == 1 && ports[0]->display_name == 0)
	ports[0]->display_name = arg;
      else
	add_port(arg, 0, 0, 0);
    
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
    else if (optparse(s, "multiscreen", 6, "t"))
	multiscreen = optparse_yesno;
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

    else if (optparse(s, "title", 2, "ss", &o->window_title))
      ;
    else if (optparse(s, "typetime", 1, "st", &normal_type_time))
      ;
    else if (optparse(s, "top", 2, "t"))
      o->top = optparse_yesno;

    else if (optparse(s, "verbose", 4, "t")
	     || optparse(s, "V", 1, "t"))
	verbose = optparse_yesno;
    else if (optparse(s, "version", 1, "s")) {
      printf("LCDF Xwrits %s\n", VERSION);
      printf("\
Copyright (C) 1994-2006 Eddie Kohler\n\
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

void
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

  /* window title */
  onormal.window_title = "xwrits";
  
  /* clock tick time functions */
  /* 20 seconds seems like a reasonable clock tick time, even though it'll
     redraw the same hands 3 times. */
  xwSETTIME(clock_tick, 1, 0);
  
  /* next option set */
  xwSETTIME(onormal.next_delay, 15 * SEC_PER_MIN, 0);
  onormal.next = 0;
  onormal.prev = 0;

  /* how many times to run? */
  run_once = -1;
}


/* initialize port, for X stuff */

#if defined(__cplusplus) || defined(c_plusplus)
#define VISUAL_CLASS c_class
#else
#define VISUAL_CLASS class
#endif

static void
initialize_slave_port(Port *port)
{
    Port *m = port->master;
    assert(m->port_number < port->port_number);
    port->x_socket = m->x_socket;
    port->root_window = m->root_window;
    port->display_unique = 0;
    port->visual = m->visual;
    port->depth = m->depth;
    port->colormap = m->colormap;
    port->black = m->black;
    port->white = m->white;
    port->font = m->font;
    port->gfx = m->gfx;
    port->wm_protocols_atom = m->wm_protocols_atom;
    port->wm_delete_window_atom = m->wm_delete_window_atom;
    port->wm_client_leader_atom = m->wm_client_leader_atom;
    port->mwm_hints_atom = m->mwm_hints_atom;
    port->net_wm_ping_atom = m->net_wm_ping_atom;
    port->net_wm_desktop_atom = m->net_wm_desktop_atom;
    port->net_wm_window_type_atom = m->net_wm_window_type_atom;
    port->net_wm_window_type_utility_atom = m->net_wm_window_type_utility_atom;
    port->net_wm_pid_atom = m->net_wm_pid_atom;
    port->xwrits_window_atom = m->xwrits_window_atom;
    port->xwrits_notify_peer_atom = m->xwrits_notify_peer_atom;
    port->xwrits_break_atom = m->xwrits_break_atom;
    port->hands = port->icon_hands = 0;
    port->permanent_hand = m->permanent_hand;
    port->drawable = m->drawable;
    port->clock_fore_gc = m->clock_fore_gc;
    port->clock_hand_gc = m->clock_hand_gc;
    port->white_gc = m->white_gc;
    port->peers = 0;
    port->npeers = 0;
    port->peers_capacity = 0;
    port->icon_width = port->icon_height = 0;
    port->last_mouse_root = None;
    port->bars_pixmap = None;
}

static void
initialize_port(int portno)
{
  XVisualInfo visi_template;
  int nv, i;
  XVisualInfo *v;
  XVisualInfo *best_v = 0;
  VisualID default_visualid;
  Port *port = ports[portno];
  Display *display = port->display;
  int screen_number = port->screen_number;

  /* check that relevant fields have been initialized */
  assert(display && screen_number >= 0 && screen_number < ScreenCount(display) && port->port_number == portno);
  
  /* initialize slaves specially */
  if (port->master != port) {
      initialize_slave_port(port);
      return;
  }
  
  /* initialize Port fields */
  port->x_socket = ConnectionNumber(display);
  port->root_window = RootWindow(display, screen_number);
  port->display_unique = 1;
  for (i = 0; i < nports && port->display_unique; i++)
      if (i != portno && ports[i]->display == display && ports[i]->master == ports[i])
	  port->display_unique = 0;

  /* set X socket */
  FD_SET(port->x_socket, &x_socket_set);
  if (port->x_socket > max_x_socket)
      max_x_socket = port->x_socket;
  
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

  if (v)
      XFree(v);
  
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
  port->font = XLoadQueryFont(display, "-*-helvetica-bold-r-*-*-*-180-75-75-*-iso8859-1");
  if (!port->font)
      port->font = XLoadQueryFont(display, "fixed");
  
  /* set gfx */
  port->gfx = Gif_NewXContextFromVisual
    (display, screen_number, port->visual, port->depth, port->colormap);

  /* set atoms */
  port->wm_protocols_atom = XInternAtom(display, "WM_PROTOCOLS", False);
  port->wm_delete_window_atom = XInternAtom(display, "WM_DELETE_WINDOW", False);
  port->wm_client_leader_atom = XInternAtom(display, "WM_CLIENT_LEADER", False);
  port->mwm_hints_atom = XInternAtom(display, "_MOTIF_WM_HINTS", False);
  port->net_wm_ping_atom = XInternAtom(display, "_NET_WM_PING", False);
  port->net_wm_desktop_atom = XInternAtom(display, "_NET_WM_DESKTOP", False);
  port->net_wm_window_type_atom = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
  port->net_wm_window_type_utility_atom = XInternAtom(display, "_NET_WM_WINDOW_TYPE_UTILITY", False);
  port->net_wm_pid_atom = XInternAtom(display, "_NET_WM_PID", False);
  port->xwrits_window_atom = XInternAtom(display, "XWRITS_WINDOW", False);
  port->xwrits_notify_peer_atom = XInternAtom(display, "XWRITS_NOTIFY_PEER", False);
  port->xwrits_break_atom = XInternAtom(display, "XWRITS_BREAK", False);

  /* create first hand for this port, set drawable */
  port->hands = port->icon_hands = port->permanent_hand = 0;
  (void) new_hand(port, NEW_HAND_CENTER, NEW_HAND_CENTER);
  port->drawable = port->permanent_hand->w;

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

  /* xwrits peers */
  port->peers = xwNEWARR(Window, 4);
  port->npeers = 0;
  port->peers_capacity = 4;
  
  /* initialize other stuff */
  port->icon_width = port->icon_height = 0;
  port->last_mouse_root = None;
  port->bars_pixmap = None;
}


/* main! */

typedef enum {
    ST_NORMAL_WAIT, ST_FIRST_WARN, ST_WARN, ST_CANCEL_WAIT,
    ST_REST, ST_LOCK, ST_AWAKE
} XwritsState;

void
main_loop(void)
{
    XwritsState s = ST_NORMAL_WAIT;
    int tran;
    int was_lock = 0;

    while (1) {
	switch (s) {

	  case ST_NORMAL_WAIT:
	    ocurrent = &onormal;
	    tran = wait_for_break(&normal_type_time);
	    assert(tran == TRAN_WARN || tran == TRAN_REST);
	    if (tran == TRAN_WARN)
		s = ST_FIRST_WARN;
	    break;

	  case ST_FIRST_WARN:
	    xwGETTIME(first_warn_time);
	    was_lock = 0;
	    s = ST_WARN;
	    break;

	  case ST_WARN:
	    /* warn() will figure out current options by time */
	    tran = warn(was_lock, &onormal);
	    assert(tran == TRAN_CANCEL || tran == TRAN_REST || tran == TRAN_LOCK || tran == TRAN_AWAKE);
	    if (tran == TRAN_REST)
		s = ST_REST;
	    else if (tran == TRAN_CANCEL)
		s = ST_CANCEL_WAIT;
	    else if (tran == TRAN_LOCK)
		s = ST_LOCK;
	    else if (tran == TRAN_AWAKE)
		s = ST_AWAKE;
	    break;

	  case ST_CANCEL_WAIT:
	    tran = wait_for_break(&ocurrent->cancel_type_time);
	    assert(tran == TRAN_WARN || tran == TRAN_REST);
	    if (tran == TRAN_WARN)
		s = ST_WARN;
	    else
		s = ST_NORMAL_WAIT;
	    break;

	  case ST_REST:
	    if (run_once > 0 && run_once == 1)
		exit(0);
	    was_lock = 0;
	    tran = rest();
	    assert(tran == TRAN_AWAKE || tran == TRAN_CANCEL || tran == TRAN_FAIL);
	    if (tran == TRAN_AWAKE)
		s = ST_AWAKE;
	    else if (tran == TRAN_CANCEL)
		s = ST_CANCEL_WAIT;
	    else if (tran == TRAN_FAIL)
		s = ST_WARN;
	    break;

	  case ST_LOCK:
	    was_lock = 1;
	    tran = lock();
	    assert(tran == TRAN_AWAKE || tran == TRAN_FAIL);
	    if (tran == TRAN_AWAKE)
		s = ST_AWAKE;
	    else if (tran == TRAN_FAIL)
		s = ST_WARN;
	    break;

	  case ST_AWAKE:
	    if (run_once > 0 && --run_once == 0)
		exit(0);
	    ready();
	    unmap_all();
	    s = ST_NORMAL_WAIT;
	    break;
	    
	}
    }
}

int
main(int argc, char *argv[])
{
  Options *o;
  int i, j, orig_nports;
  int lock_possible = 0;
  struct timeval now;
  
  xwGETTIMEOFDAY(&genesis_time);
  init_scheduler();
  
  srand((getpid() + 1) * time(0));

  add_port(0, 0, 0, 0);
  
  /* parse options. remove first argument = program name */
  default_settings();  
  parse_options(argc - 1, argv + 1);

  /* At this point, all ports have 'display_name' valid and everything else
     invalid. Open displays, check multiscreen */
  orig_nports = nports;
  for (i = 0; i < orig_nports; i++) {
      Display *display = XOpenDisplay(ports[i]->display_name);
      if (!display)
	  error("can't open display '%s'", ports[i]->display_name);
      ports[i]->display = display;
      if (!multiscreen)
	  ports[i]->screen_number = DefaultScreen(display);
      else if (ScreenCount(display) > 0) {
	  ports[i]->screen_number = 0;
	  for (j = 1; j < ScreenCount(display); j++)
	      add_port(ports[i]->display_name, ports[i]->display, j, 0);
      }
  }

  /* Now check screen dimensions and Xinerama, if available. */
  orig_nports = nports;
  for (i = 0; i < orig_nports; i++) {
      Port *p = ports[i];
#ifdef HAVE_XINERAMA
      int j, nxsi;
      XineramaScreenInfo *xsi = 0;
      if (XineramaQueryExtension(p->display, &j, &nxsi))
	  xsi = XineramaQueryScreens(p->display, &nxsi);
      if (xsi) {
	  for (j = 0; j < nxsi; j++) {
	      Port *q = (j == 0 ? p : add_port(p->display_name, p->display, p->screen_number, p));
	      q->left = xsi[j].x_org;
	      q->top = xsi[j].y_org;
	      q->width = xsi[j].width;
	      q->height = xsi[j].height;
	  }
	  XFree(xsi);
      } else {
#endif
      p->width = DisplayWidth(p->display, p->screen_number);
      p->height = DisplayHeight(p->display, p->screen_number);
#ifdef HAVE_XINERAMA
      }
#endif
  }
  
  /* check global options */
  if (strlen(lock_password) >= MAX_PASSWORD_SIZE)
    error("password too long");
  
  /* default for idle_time and warn_idle_time is based on break_delay;
     otherwise, warn_idle_time == idle_time */
  if (check_idle && xwTIMELEQ0(idle_time))
    idle_time = onormal.break_time;
  
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
  ocurrent = &onormal;

  /* create ports */
  FD_ZERO(&x_socket_set);
  max_x_socket = 0;
  for (i = 0; i < nports; i++)
    initialize_port(i);

  /* initialize pictures using first hand */
  if (lock_possible) {
    locked_slideshow = parse_slideshow(locked_slideshow_text, 1, force_mono);
    /* set bars_slideshow to default only if locked_slideshow is default */
    if (!bars_slideshow_text)
      bars_slideshow_text = (strcmp(locked_slideshow_text, "&locked") == 0 ? "&bars" : 0);
    if (bars_slideshow_text) {
      bars_slideshow = parse_slideshow(bars_slideshow_text, 1, force_mono);
      for (i = 0; i < nports; i++)
        ports[i]->bars_pixmap = Gif_XImage(ports[i]->gfx, bars_slideshow, 0);
    }
  }
  
  /* watch keystrokes on all windows */
  xwGETTIME(now);
  old_x_error_handler = XSetErrorHandler(x_error_handler);
  for (i = 0; i < nports; i++)
      if (ports[i]->master == ports[i])
	  watch_keystrokes(ports[i], ports[i]->root_window, &now);
  
  /* start mouse checking */
  if (check_mouse) {
    Alarm *a = new_alarm(A_MOUSE);
    xwGETTIME(a->timer);
    schedule(a);
  }
  
  /* main loop */
  main_loop();
  
  return 0;
}
