#ifndef XWRITS_H
#define XWRITS_H

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#ifndef FD_SET
#include <sys/select.h>
#endif
#include "gif.h"
#include "gifx.h"

typedef struct Port Port;
typedef struct Options Options;
typedef struct Hand Hand;
typedef struct Picture Picture;
typedef struct Alarm Alarm;

#ifdef __cplusplus
#define EXTERNFUNCTION		extern "C"
#else
#define EXTERNFUNCTION		extern
#endif

#define xwNEW(typ)		(typ *)xmalloc(sizeof(typ))
#define xwNEWARR(typ,num)	(typ *)xmalloc(sizeof(typ) * (num))
#define xwREARRAY(var,typ,num)	var = (typ *)xrealloc(var, sizeof(typ) * (num))


/*****************************************************************************/
/*  Global X stuff							     */

struct Port {

  int port_number;		/* 0 <= port_number < nports */
  
  const char *display_name;	/* name of display */
  Display *display;		/* display pointer */
  int display_unique;		/* is this only master Port with display? */
  int x_socket;			/* socket of X connection */
  
  int screen_number;		/* screen number */
  Window root_window;		/* root window of screen */
  int left;			/* left edge of display (Xinerama) */
  int top;			/* top edge of display (Xinerama) */
  int width;			/* width of root window */
  int height;			/* height of root window */

  struct Port *master;		/* points to master Port, if this is a slave */
  int nslaves;			/* number of slave Ports */

  Drawable drawable;		/* drawable corresponding to visual */
  Visual *visual;		/* visual used for new windows */
  int depth;			/* depth of visual */
  Colormap colormap;		/* colormap on visual */
  unsigned long black;		/* black pixel value on colormap */
  unsigned long white;		/* white pixel value on colormap */
  
  XFontStruct *font;		/* font for lock messages */
  GC white_gc;			/* foreground white, font font */
  GC clock_fore_gc;		/* foreground black, thick rounded line */
  GC clock_hand_gc;		/* same as clock_fore_gc */
  
  Gif_XContext *gfx;		/* GIF X context */
  
  Atom wm_protocols_atom;	/* atoms for window manager communication */
  Atom wm_delete_window_atom;
  Atom wm_client_leader_atom;
  Atom mwm_hints_atom;
  Atom net_wm_ping_atom;
  Atom net_wm_desktop_atom;
  Atom net_wm_window_type_atom;
  Atom net_wm_window_type_utility_atom;
  Atom net_wm_pid_atom;
  Atom xwrits_window_atom;	/* atoms for communication with other xwrits */
  Atom xwrits_notify_peer_atom;
  Atom xwrits_break_atom;

  Hand *hands;			/* list of main hands */
  Hand *icon_hands;		/* list of icon hands */
  Hand *permanent_hand;		/* hand that will never be destroyed */
  
  int icon_width;		/* preferred icon width */
  int icon_height;		/* preferred icon height */

  Window last_mouse_root;	/* last root window the mouse was on */
  int last_mouse_x;		/* last X position of mouse */
  int last_mouse_y;		/* last Y position of mouse */

  Pixmap bars_pixmap;		/* bars background for lock screen */

  Window *peers;		/* list of peer windows */
  int npeers;
  int peers_capacity;
  
};

extern int nports;
extern Port **ports;

extern fd_set x_socket_set;
extern int max_x_socket;

Port *find_port(Display *, Window);

void mark_xwrits_window(Port *, Window);
Window check_xwrits_window(Port *, Window);

/* fake X event types */
#define Xw_DeleteWindow		(LASTEvent + 100)
#define Xw_TakeBreak		(LASTEvent + 101)
int default_x_processing(XEvent *);
int x_error_handler(Display *, XErrorEvent *);

void add_peer(Port *, Window);
void notify_peers_rest(void);


/*****************************************************************************/
/*  Options								     */

struct Options {
  
  struct timeval break_time;		/* length of break */
  struct timeval min_break_time;	/* minimum length of break (+quota) */
  struct timeval cancel_type_time;	/* typing OK for TIME after cancel */
  
  Gif_Stream *slideshow;		/* warn window animation  */
  Gif_Stream *icon_slideshow;		/* warn icon window animation */
  const char *slideshow_text;
  const char *icon_slideshow_text;
  const char *window_title;
  
  double flash_rate_ratio;		/* <1, flash fast; >1, flash slow */
  struct timeval multiply_delay;	/* time between window multiplies */
  struct timeval lock_bounce_delay;	/* time between lock bounces */
  
  unsigned beep: 1;			/* beep when bringing up a warn? */
  unsigned clock: 1;			/* show clock for time since warn? */
  unsigned appear_iconified: 1;		/* show warns as icons? */
  unsigned never_iconify: 1;		/* don't let warns be iconified? */
  unsigned top: 1;			/* keep warn windows on top? */
  unsigned multiply: 1;			/* multiply warn windows? */
  unsigned lock: 1;			/* lock screen? */
  unsigned break_clock: 1;		/* show clock time left in break? */
  int max_hands;			/* max number of hands (+multiply) */
  
  struct timeval next_delay;		/* delay till go to next options */
  Options *next;			/* next options */
  Options *prev;			/* previous options */
  
};

extern Options *ocurrent;

extern struct timeval type_delay;

#define MAX_PASSWORD_SIZE 256
extern struct timeval lock_message_delay;
extern char *lock_password;


/*****************************************************************************/
/*  Clocks								     */

extern struct timeval clock_zero_time;
extern struct timeval clock_tick;

void init_clock(Port *);
void draw_clock(Hand *, const struct timeval *);
void draw_all_clocks(const struct timeval *);
void erase_clock(Hand *);
void erase_all_clocks(void);


/*****************************************************************************/
/*  Alarms								     */

#define A_FLASH			0x0001
#define A_AWAKE			0x0002
#define A_CLOCK			0x0004
#define A_MULTIPLY		0x0008
#define A_NEXT_OPTIONS		0x0010
#define A_LOCK_BOUNCE		0x0020
#define A_LOCK_MESS_ERASE	0x0040
#define A_IDLE_SELECT		0x0080
#define A_IDLE_CHECK		0x0100
#define A_MOUSE			0x0200

struct Alarm {
  
  Alarm *next;
  Alarm *prev;
  struct timeval timer;
  int action;
  void *data1;
  void *data2;
  
  unsigned scheduled: 1;
  
};

typedef int (*Alarmloopfunc)(Alarm *, const struct timeval *);
typedef int (*Xloopfunc)(XEvent *, const struct timeval *);

#define new_alarm(i)	new_alarm_data((i), 0, 0)
Alarm *new_alarm_data(int, void *, void *);
#define grab_alarm(i)	grab_alarm_data((i), 0, 0)
Alarm *grab_alarm_data(int, void *, void *);
void destroy_alarm(Alarm *);

void init_scheduler(void);
void schedule(Alarm *);
#define unschedule(i)	unschedule_data((i), 0, 0)
void unschedule_data(int, void *, void *);

int loopmaster(Alarmloopfunc, Xloopfunc);


/*****************************************************************************/
/*  Hands								     */

#define MAX_HANDS	137

struct Hand {
  
  Hand *next;
  Hand *prev;
  Hand *icon;

  Port *port;
  Window w;
  int x;
  int y;
  int width;
  int height;
  
  Window root_child;
  
  Gif_Stream *slideshow;
  int slide;
  int loopcount;
  
  unsigned is_icon: 1;
  unsigned mapped: 1;
  unsigned configured: 1;
  unsigned obscured: 1;
  unsigned clock: 1;
  unsigned permanent: 1;
  unsigned toplevel: 1;
  
};

int active_hands(void);

#define NEW_HAND_CENTER	0x8000
#define NEW_HAND_RANDOM	0x7FFF
#define NEW_HAND_RANDOM_PORT ((Port *)0)
Hand *new_hand(Port *, int x, int y);
Hand *new_hand_subwindow(Port *, Window parent, int x, int y);
void destroy_hand(Hand *);
Hand *find_one_hand(Port *, int mapped);

Hand *window_to_hand(Port *, Window, int allow_icon);

void draw_slide(Hand *);


/*****************************************************************************/
/*  Slideshow								     */

extern Gif_Stream *current_slideshow;
extern Gif_Stream *resting_slideshow, *resting_icon_slideshow;
extern Gif_Stream *ready_slideshow, *ready_icon_slideshow;
extern Gif_Stream *locked_slideshow, *bars_slideshow;
#define DEFAULT_FLASH_DELAY_SEC 2

Gif_Stream *get_built_in_image(const char *);
Gif_Stream *parse_slideshow(const char *, double, int mono);
void set_slideshow(Hand *, Gif_Stream *, const struct timeval *);
void set_all_slideshows(Hand *, Gif_Stream *);


/*****************************************************************************/
/*  Pictures								     */

struct Picture {
  
  int clock_x_off;
  int clock_y_off;
  Gif_Image *canonical;
  Pixmap pix[1];
  
};

void default_pictures(void);
void load_needed_pictures(Window, int, int force_mono);


/*****************************************************************************/
/*  Idle processing							     */

extern struct timeval register_keystrokes_delay;
extern struct timeval register_keystrokes_gap;

extern struct timeval last_key_time;	/* time of last keystroke/equivalent */

extern int check_idle;			/* check for idle periods? */
extern struct timeval idle_time;	/* idle period of idle_time = break */
extern struct timeval warn_idle_time;	/* " during warning */

extern int check_mouse;			/* pay attention to mouse movement? */
extern struct timeval check_mouse_time;	/* next time to check mouse pos */
extern int mouse_sensitivity;		/* movement > sensitivity = keypress */

extern int check_quota;			/* use quota system? */
extern struct timeval quota_time;	/* if idle more than quota_time,
					   count idle time towards break */
extern struct timeval quota_allotment;	/* counted towards break */

extern int max_cheats;			/* allow this many cheat events before
					   cancelling break */

extern int verbose;			/* be verbose */

void watch_keystrokes(Port *, Window, const struct timeval *);
void register_keystrokes(Port *, Window);


/*****************************************************************************/
/*  The high-level procedures						     */

void error(const char *, ...);
void warning(const char *, ...);
void message(const char *, ...);

#define TRAN_WARN	1
#define TRAN_CANCEL	2
#define TRAN_FAIL	3
#define TRAN_REST	4
#define TRAN_LOCK	5
#define TRAN_AWAKE	6

extern struct timeval first_warn_time;

int wait_for_break(const struct timeval *type_time);
int warn(int was_lock, Options *first_options);
void calculate_break_time(struct timeval *break_over_time, const struct timeval *now);
int rest(void);
int lock(void);

void ready(void);
void unmap_all(void);


/*****************************************************************************/
/*  Time functions							     */

#define MICRO_PER_SEC 1000000
#define SEC_PER_MIN 60
#define MIN_PER_HOUR 60
#define HOUR_PER_CYCLE 12

#define xwSETTIME(timeval, sec, usec) do { \
	(timeval).tv_sec = (sec); (timeval).tv_usec = (usec); \
	} while (0)

#define xwADDTIME(result, a, b) do { \
	(result).tv_sec = (a).tv_sec + (b).tv_sec; \
	if (((result).tv_usec = (a).tv_usec+(b).tv_usec) >= MICRO_PER_SEC) { \
		(result).tv_sec++; \
		(result).tv_usec -= MICRO_PER_SEC; \
	} } while (0)

#define xwSUBTIME(result, a, b) do { \
	(result).tv_sec = (a).tv_sec - (b).tv_sec; \
	if (((result).tv_usec = (a).tv_usec - (b).tv_usec) < 0) { \
		(result).tv_sec--; \
		(result).tv_usec += MICRO_PER_SEC; \
	} } while (0)

#define xwSETMINTIME(a, b) do { \
	if ((b).tv_sec < (a).tv_sec || \
	    ((b).tv_sec == (a).tv_sec && (b).tv_usec < (a).tv_usec)) \
		(a) = (b); \
	} while (0)

#define xwTIMEGEQ(a, b) ((a).tv_sec > (b).tv_sec || \
	((a).tv_sec == (b).tv_sec && (a).tv_usec >= (b).tv_usec))

#define xwTIMEGT(a, b) ((a).tv_sec > (b).tv_sec || \
	((a).tv_sec == (b).tv_sec && (a).tv_usec > (b).tv_usec))

#define xwTIMELEQ0(a) ((a).tv_sec < 0 || ((a).tv_sec == 0 && (a).tv_usec <= 0))
#define xwTIMELT0(a)  ((a).tv_sec < 0 || ((a).tv_sec == 0 && (a).tv_usec < 0))


#ifdef X_GETTIMEOFDAY
# define xwGETTIMEOFDAY(a) X_GETTIMEOFDAY(a)
#elif GETTIMEOFDAY_PROTO == 0
EXTERNFUNCTION int gettimeofday(struct timeval *, struct timezone *);
# define xwGETTIMEOFDAY(a) gettimeofday((a), 0)
#elif GETTIMEOFDAY_PROTO == 1
# define xwGETTIMEOFDAY(a) gettimeofday((a))
#else
# define xwGETTIMEOFDAY(a) gettimeofday((a), 0)
#endif

#define xwGETTIME(a) do { xwGETTIMEOFDAY(&(a)); xwSUBTIME((a), (a), genesis_time); } while (0)
extern struct timeval genesis_time;

#define xwADDDELAY(result, a, d) do { \
	(result).tv_sec = (a).tv_sec + ((d)/100); \
	if (((result).tv_usec = (a).tv_usec + ((d)%100)*10000) >= MICRO_PER_SEC) { \
		(result).tv_sec++; \
		(result).tv_usec -= MICRO_PER_SEC; \
	} } while (0)

#define xwSUBDELAY(result, a, d) do { \
	(result).tv_sec = (a).tv_sec - ((d)/100); \
	if (((result).tv_usec = (a).tv_usec - ((d)%100)*10000) < 0) { \
		(result).tv_sec--; \
		(result).tv_usec += MICRO_PER_SEC; \
	} } while (0)

#endif
