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

typedef struct Port Port;
typedef struct Options Options;
typedef struct Hand Hand;
typedef struct Picture Picture;
typedef struct Slideshow Slideshow;
typedef struct Alarm Alarm;

#ifdef __cplusplus
#define EXTERNFUNCTION		extern "C"
#else
#define EXTERNFUNCTION		extern
#endif

#define xwNEW(typ)		(typ *)malloc(sizeof(typ))
#define xwNEWARR(typ,num)	(typ *)malloc(sizeof(typ) * (num))
#define xwREARRAY(var,typ,num)	var = (typ *)realloc(var, sizeof(typ) * (num))


/*****************************************************************************/
/*  Global X stuff							     */

struct Port {
  
  Display *display;
  int x_socket;
  
  int screen_number;
  Window root_window;
  int width;
  int height;
  
  int wm_delta_x;
  int wm_delta_y;
  
  Visual *visual;
  int depth;
  Colormap colormap;
  unsigned long black;
  unsigned long white;
  
  XFontStruct *font;
  
};

extern Display *display;
extern Port port;

int default_x_processing(XEvent *);
int x_error_handler(Display *, XErrorEvent *);


/*****************************************************************************/
/*  Options								     */

struct Options {
  
  struct timeval multiply_delay;
  struct timeval lock_bounce_delay;
  struct timeval top_delay;
    
  unsigned never_iconify: 1;
  unsigned top: 1;
  unsigned clock: 1;
  unsigned break_clock: 1;
  unsigned multiply: 1;
  unsigned beep: 1;
  unsigned appear_iconified: 1;
  unsigned lock: 1;
  int max_hands;

  Slideshow *slideshow;
  
  struct timeval next_delay;
  Options *next;
  
};

extern Options *ocurrent;

extern struct timeval type_delay;
extern struct timeval break_delay;

#define MaxPasswordSize 256
extern struct timeval lock_message_delay;
extern char *lock_password;


/*****************************************************************************/
/*  States and current state						     */

#define Warning		0
#define Resting		1
#define Ready		2
#define Locked		3
#define MaxState	4


/*****************************************************************************/
/*  Clocks								     */

extern struct timeval clock_zero_time;
extern struct timeval clock_tick;

void init_clock(Drawable);
void draw_clock(struct timeval *);
void erase_clock(void);


/*****************************************************************************/
/*  Alarms								     */

#define Flash		0x001
#define Raise		0x002
#define Return		0x004
#define Clock		0x008
#define Multiply	0x010
#define NextOptions	0x020
#define LockBounce	0x040
#define LockClock	0x080
#define LockMessErase	0x100
#define IdleSelect	0x200
#define IdleCheck	0x400

struct Alarm {
  
  Alarm *next;
  struct timeval timer;
  int action;
  void *data;
  
  unsigned scheduled: 1;
  
};

typedef int (*Alarmloopfunc)(Alarm *, struct timeval *);
typedef int (*Xloopfunc)(XEvent *);

extern Alarm *alarms;

#define new_alarm(i)	new_alarm_data((i), 0)
Alarm *new_alarm_data(int, void *);
#define grab_alarm(i)	grab_alarm_data((i), 0)
Alarm *grab_alarm_data(int, void *);
void destroy_alarm(Alarm *);

void schedule(Alarm *);
#define unschedule(i)	unschedule_data((i), 0)
void unschedule_data(int, void *);

int loopmaster(Alarmloopfunc, Xloopfunc);


/*****************************************************************************/
/*  Hands								     */

#define WindowWidth	300
#define WindowHeight	300
#define IconWidth	50
#define IconHeight	50

#define MaxHands	137

struct Hand {
  
  Hand *next;
  Hand *prev;
  
  Window w;
  int x;
  int y;
  int width;
  int height;
  
  Window iconw;
  
  Slideshow *slideshow;
  int slide;
  
  unsigned mapped: 1;
  unsigned configured: 1;
  unsigned iconified: 1;
  unsigned obscured: 1;
  
};


extern Hand *hands;
extern int active_hands;

#define NHCenter	0x8000
#define NHRandom	0x7FFF
Hand *new_hand(int x, int y);
void destroy_hand(Hand *);

Hand *window_to_hand(Window);
Hand *icon_window_to_hand(Window);

void set_picture(Hand *, Slideshow *, int);
void refresh_hands(void);


/*****************************************************************************/
/*  Slideshow								     */

struct Slideshow {
  
  int nslides;
  Picture **picture;
  struct timeval *delay;
  
};


extern Slideshow *current_slideshow;
extern Slideshow *slideshow[MaxState];

Slideshow *parse_slideshow(char *, struct timeval *);
void blend_slideshow(Slideshow *);


/*****************************************************************************/
/*  Pictures								     */

struct Picture {
  
  char *name;
  Picture *next;
  
  int offset;
  
  Pixmap large;
  Pixmap icon;
  Pixmap background;
  
  int clock_x_off;
  int clock_y_off;

  unsigned used: 1;
  unsigned clock: 1;
  
};

extern Picture *pictures;
extern Pixmap bars_pixmap;
extern Pixmap lock_pixmap;

void default_pictures(void);
void load_needed_pictures(Window, int, int force_mono);


/*****************************************************************************/
/*  Idle processing							     */

<<<<<<< xwrits.h
extern struct timeval register_keystrokes_delay;
extern struct timeval register_keystrokes_gap;
extern struct timeval idle_time;
=======
extern struct timeval idle_select_delay;
extern struct timeval idle_gap_delay;
extern struct timeval idle_check_delay;
>>>>>>> 1.7
extern int check_idle;

void watch_keystrokes(Window, struct timeval *);
void register_keystrokes(Window);


/*****************************************************************************/
/*  The high-level procedures						     */

void error(char *);

void wait_for_break(void);

#define WarnRest	1
#define WarnLock	2
#define WarnCancelled	3
int warning(int lockfailed);

#define RestOK		4
#define RestCancelled	5
#define RestFailed	6
int rest(void);

#define LockOK		RestOK
#define LockCancelled	7
#define LockFailed	8
int lock(void);

void ready(void);
void unmap_all(void);


/*****************************************************************************/
/*  Time functions							     */

#define MICRO_PER_SEC 1000000
#define SEC_PER_MIN 60
#define MIN_PER_HOUR 60
#define HOUR_PER_CYCLE 12

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


#ifdef X_GETTIMEOFDAY
#define xwGETTIMEOFDAY(a) X_GETTIMEOFDAY(a)
#elif SYSV_GETTIMEOFDAY
#define xwGETTIMEOFDAY(a) gettimeofday((a))
#else
#ifdef NO_GETTIMEOFDAY_PROTO
EXTERNFUNCTION int gettimeofday(struct timeval *, struct timezone *);
#endif
#define xwGETTIMEOFDAY(a) gettimeofday((a), 0)
#endif

#define xwGETTIME(a) do { xwGETTIMEOFDAY(&(a)); xwSUBTIME((a), (a), genesis_time); } while (0)
extern struct timeval genesis_time;

#endif
