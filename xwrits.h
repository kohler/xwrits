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

extern Display *display;
extern int screennumber;
extern Window rootwindow;
extern int xsocket;
extern int display_width, display_height;
extern int wmdeltax, wmdeltay;
extern XFontStruct *font;

int defaultxprocessing(XEvent *);
int xerrorhandler(Display *, XErrorEvent *);


/*****************************************************************************/
/*  Options								     */

struct Options {
  
  struct timeval multiplydelay;
  struct timeval lockbouncedelay;
  struct timeval topdelay;
  
  unsigned nevericonify: 1;
  unsigned top: 1;
  unsigned clock: 1;
  unsigned breakclock: 1;
  unsigned multiply: 1;
  unsigned beep: 1;
  unsigned appeariconified: 1;
  unsigned lock: 1;
  int maxhands;
  
  Slideshow *slideshow;
  
  struct timeval nextdelay;
  Options *next;
  
};

extern Options *ocurrent;

extern struct timeval typedelay;
extern struct timeval breakdelay;
extern struct timeval idlecheckdelay;

#define MaxPasswordSize 256
extern struct timeval lockmessagedelay;
extern char *lockpassword;


/*****************************************************************************/
/*  States and current state						     */

#define Warning		0
#define Resting		1
#define Ready		2
#define Locked		3
#define MaxState	4


/*****************************************************************************/
/*  Clocks								     */

extern struct timeval clockzerotime;
extern struct timeval clocktick;

void initclock(Drawable);
void drawclock(struct timeval *);
void eraseclock(void);


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

#define newalarm(i)	newalarmdata((i), 0)
Alarm *newalarmdata(int, void *);
#define grabalarm(i)	grabalarmdata((i), 0)
Alarm *grabalarmdata(int, void *);
void destroyalarm(Alarm *);

void schedule(Alarm *);
#define unschedule(i)	unscheduledata((i), 0)
void unscheduledata(int, void *);

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
extern int activehands;

#define NHCenter	-0x8000
#define NHRandom	-0x7FFF
Hand *newhand(int x, int y);
void destroyhand(Hand *);

Hand *windowtohand(Window);
Hand *iconwindowtohand(Window);

void setpicture(Hand *, Slideshow *, int);
void refreshhands(void);


/*****************************************************************************/
/*  Slideshow								     */

struct Slideshow {
  
  int nslides;
  Picture **picture;
  struct timeval *delay;
  
};


extern Slideshow *currentslideshow;
extern Slideshow *slideshow[MaxState];

Slideshow *parseslideshow(char *, struct timeval *);
void blendslideshow(Slideshow *);


/*****************************************************************************/
/*  Pictures								     */

struct Picture {
  
  char *name;
  Picture *next;
  
  Gif_Record *largerecord;
  Gif_Record *iconrecord;
  
  Pixmap large;
  Pixmap icon;
  
  int clockxoff;
  int clockyoff;
  unsigned long background;

  unsigned used: 1;
  unsigned clock: 1;
  
};

extern Picture *pictures;
extern Pixmap barspix;

void defaultpictures(void);
void loadneededpictures(Window, int);


/*****************************************************************************/
/*  Idle processing							     */

extern struct timeval idleselectdelay;
extern struct timeval idlegapdelay;
extern int checkidle;

void idlecreate(Window, struct timeval *);
void idleselect(Window);


/*****************************************************************************/
/*  The high-level procedures						     */

void error(char *);

void waitforbreak(void);

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
void unmapall(void);


/*****************************************************************************/
/*  Time functions							     */

#define MicroPerSec 1000000
#define SecPerMin 60
#define MinPerHr 60
#define HrPerCycle 12

#define xwADDTIME(result, a, b) do { \
	(result).tv_sec = (a).tv_sec + (b).tv_sec; \
	if (((result).tv_usec = (a).tv_usec + (b).tv_usec) >= MicroPerSec) { \
		(result).tv_sec++; \
		(result).tv_usec -= MicroPerSec; \
	} } while (0)

#define xwSUBTIME(result, a, b) do { \
	(result).tv_sec = (a).tv_sec - (b).tv_sec; \
	if (((result).tv_usec = (a).tv_usec - (b).tv_usec) < 0) { \
		(result).tv_sec--; \
		(result).tv_usec += MicroPerSec; \
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
#define xwGETTIMEOFDAY(t) X_GETTIMEOFDAY(t)
#elif SYSV_GETTIMEOFDAY
#define xwGETTIMEOFDAY(a) gettimeofday((a))
#else
#ifdef NO_GETTIMEOFDAY_PROTO
EXTERNFUNCTION int gettimeofday(struct timeval *, struct timezone *);
#endif
#define xwGETTIMEOFDAY(a) gettimeofday((a), 0)
#endif

#define xwGETTIME(a) do { xwGETTIMEOFDAY(&(a)); xwSUBTIME((a), (a), genesistime); } while (0)
extern struct timeval genesistime;

#endif
