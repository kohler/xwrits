#include <config.h>
#include "xwrits.h"
#include <stdlib.h>
#include <X11/keysym.h>
#include <assert.h>

#define BARS_WIDTH		150
#define WINDOW_HEIGHT		300

struct timeval lock_message_delay;
char *lock_password;

#define REDRAW_MESSAGE		((char *)1L)
#define MAX_MESSAGE_SIZE	(256 + MAX_PASSWORD_SIZE)

static Window *covers;
static Hand **lock_hands;

static char password[MAX_PASSWORD_SIZE];
static int password_pos;


static void
lock_hand_position(Port *port, int *x, int *y)
{
  /* x_width_bars = max x | x*150 + screen_width <= port_width */
  int x_width_bars = (port->width - locked_slideshow->screen_width) / BARS_WIDTH;
  *x = ((rand() >> 4) % (x_width_bars + 1)) * BARS_WIDTH;
  *y = (((rand() >> 4) % (port->height - locked_slideshow->screen_height))
	& ~0x3) | 0x2;
}

static void
move_locks(void)
{
  int i, x, y;
  struct timeval now;
  xwGETTIME(now);
  for (i = 0; i < nports; i++)
    if (covers[i]) {
      lock_hand_position(&ports[i], &x, &y);
      XClearWindow(ports[i].display, lock_hands[i]->w);
      XMoveWindow(ports[i].display, lock_hands[i]->w, x, y);
      if (ocurrent->break_clock) draw_clock(lock_hands[i], &now);
      XFlush(ports[i].display);
    }
}

static void
find_message_boundaries(Port *port, const char *message,
			int *x, int *y, int *w, int *h)
{
  int length = strlen(message);
  *w = XTextWidth(port->font, message, length);
  *h = port->font->ascent + port->font->descent;
  *x = (port->width - *w) / 2;
  *y = (port->height - *h) / 2;
}

static void
draw_message(const char *message)
{
  static char cur_message[MAX_MESSAGE_SIZE];
  int i, x, y, w, h;
  int redraw = (message == REDRAW_MESSAGE);
  int length;
  
  if (redraw)
    message = (cur_message[0] ? cur_message : 0);
  
  length = (message ? strlen(message) : 0);
  assert(length < MAX_MESSAGE_SIZE);
  
  for (i = 0; i < nports; i++)
    if (covers[i]) {
      if (cur_message[0] && !redraw) {
	find_message_boundaries(&ports[i], cur_message, &x, &y, &w, &h);
	XClearArea(ports[i].display, covers[i], x, y, w, h, False);
	XClearArea(ports[i].display, lock_hands[i]->w,
		   x - lock_hands[i]->x, y - lock_hands[i]->y, w, h, False);
      }
      if (message) {
	find_message_boundaries(&ports[i], message, &x, &y, &w, &h);
	XDrawString(ports[i].display, covers[i], ports[i].white_gc,
		    x, y + ports[i].font->ascent, message, length);
      }
    }

  if (!redraw) {
    if (message)
      strcpy(cur_message, message);
    else
      cur_message[0] = 0;
  }
}

static int
check_password(XKeyEvent *xkey)
{
  KeySym keysym;
  XComposeStatus compose;
  char c;
  int nchars = XLookupString(xkey, &c, 1, &keysym, &compose);
  int incorrect_message = 0;
  
  if (keysym == XK_Return ||
      (nchars == 1 && (c == '\n' || c == '\r'))) {
    password[password_pos] = 0;
    password_pos = 0;
    if (strcmp(lock_password, password) == 0)
      return 1;
    else
      incorrect_message = 1;
  } else if (keysym == XK_Delete || keysym == XK_BackSpace) {
    if (password_pos > 0)
      password_pos--;
  } else if (nchars == 1) {
    if (password_pos < MAX_PASSWORD_SIZE - 1)
      password[password_pos++] = c;
  }

  if (incorrect_message)
    draw_message("Incorrect password! Try again");
  else {
    char message[MAX_MESSAGE_SIZE];
    strcpy(message, "Enter password to unlock screen");
    if (password_pos > 0) {
      int i, pos = strlen(message);
      message[pos++] = ':';
      message[pos++] = ' ';
      for (i = 0; i < password_pos; i++)
	message[pos++] = '*';
      message[pos++] = 0;
    }
    draw_message(message);
  }
  
  return 0;
}


static int
lock_alarm_loop(Alarm *a, const struct timeval *now)
{
  switch (a->action) {
    
   case A_LOCK_BOUNCE:
    move_locks();
    draw_message(REDRAW_MESSAGE);
    xwADDTIME(a->timer, a->timer, ocurrent->lock_bounce_delay);
    schedule(a);
    break;
    
   case A_LOCK_MESS_ERASE:
    draw_message(0);
    password_pos = 0;
    break;
    
  }
  
  return 0;
}


static int
lock_x_loop(XEvent *e, const struct timeval *now)
{
  Alarm *a;
  Port *port = find_port(e->xany.display);
  
  switch (e->type) {
    
   case KeyPress:
    if (check_password(&e->xkey))
      return TRAN_FAIL;
    a = grab_alarm(A_LOCK_MESS_ERASE);
    if (!a)
      a = new_alarm(A_LOCK_MESS_ERASE);
    xwGETTIME(a->timer);
    xwADDTIME(a->timer, a->timer, lock_message_delay);
    schedule(a);
    break;
    
   case VisibilityNotify:
    if (e->xvisibility.state != VisibilityUnobscured) {
      XRaiseWindow(port->display, covers[port->port_number]);
      draw_message(REDRAW_MESSAGE);
    }
    break;
    
  }
  
  return 0;
}


int
lock(void)
{
  struct timeval now, break_over_time;
  Alarm *a;
  int i, successful_grabs;
  
  XEvent event;
  int tran = TRAN_FAIL;

  /* clear slideshows */
  /* Do this first so later set_slideshows start from scratch. */
  for (i = 0; i < nports; i++) {
    set_all_slideshows(ports[i].hands, 0);
    set_all_slideshows(ports[i].icon_hands, 0);
  }

  /* calculate break time */
  xwGETTIME(now);
  calculate_break_time(&break_over_time, &now);

  /* if break already over, return */
  if (xwTIMEGEQ(now, break_over_time))
    return TRAN_AWAKE;

  /* create covers */
  if (!covers) {
    covers = (Window *)xmalloc(sizeof(Window) * nports);
    lock_hands = (Hand **)xmalloc(sizeof(Hand *) * nports);
  }
  
  for (i = 0; i < nports; i++) {
    XSetWindowAttributes setattr;
    unsigned long cwmask = CWBackingStore | CWSaveUnder | CWOverrideRedirect
      | CWBorderPixel | CWColormap;
    if (!ports[i].bars_pixmap) {
      setattr.background_pixel = ports[i].black;
      cwmask |= CWBackPixel;
    } else {
      setattr.background_pixmap = ports[i].bars_pixmap;
      cwmask |= CWBackPixmap;
    }
    setattr.backing_store = NotUseful;
    setattr.save_under = False;
    setattr.override_redirect = True;
    setattr.colormap = ports[i].colormap;
    setattr.border_pixel = 0;
    covers[i] = XCreateWindow
      (ports[i].display, ports[i].root_window,
       0, 0, ports[i].width, ports[i].height, 0,
       ports[i].depth, InputOutput, ports[i].visual, cwmask, &setattr);
    XSelectInput(ports[i].display, covers[i],
		 ButtonPressMask | ButtonReleaseMask | KeyPressMask
		 | VisibilityChangeMask | ExposureMask);
    XMapRaised(ports[i].display, covers[i]);
    XSync(ports[i].display, False);
  }

  /* grab keyboard */
  successful_grabs = 0;
  for (i = 0; i < nports; i++) {
    XWindowEvent(ports[i].display, covers[i], ExposureMask, &event);
    if (XGrabKeyboard(ports[i].display, covers[i], True,
		      GrabModeAsync, GrabModeAsync, CurrentTime)
	== GrabSuccess) {
      int x, y;
      lock_hand_position(&ports[i], &x, &y);
      lock_hands[i] = new_hand_subwindow(&ports[i], covers[i], x, y);
      set_slideshow(lock_hands[i], locked_slideshow, 0);
      XMapRaised(ports[i].display, lock_hands[i]->w);
      successful_grabs++;
    } else {
      XDestroyWindow(ports[i].display, covers[i]);
      covers[i] = None;
      lock_hands[i] = 0;
    }
  }
  if (!successful_grabs)
    goto no_keyboard_grab;

  /* set up clocks */
  a = new_alarm(A_AWAKE);
  a->timer = break_over_time;
  schedule(a);
  
  a = new_alarm(A_LOCK_BOUNCE);
  xwADDTIME(a->timer, ocurrent->lock_bounce_delay, now);
  schedule(a);
  
  if (ocurrent->break_clock) {
    clock_zero_time = break_over_time;
    draw_all_clocks(&now);
    a = new_alarm(A_CLOCK);
    xwADDTIME(a->timer, now, clock_tick);
    schedule(a);
  }

  draw_message(0);
  password_pos = 0;
  
  tran = loopmaster(lock_alarm_loop, lock_x_loop);

 no_keyboard_grab:
  unschedule(A_FLASH | A_CLOCK | A_LOCK_BOUNCE | A_AWAKE | A_LOCK_MESS_ERASE);
  erase_all_clocks();
  for (i = 0; i < nports; i++)
    if (covers[i]) {
      XUngrabKeyboard(ports[i].display, CurrentTime);
      destroy_hand(lock_hands[i]);
      XDestroyWindow(ports[i].display, covers[i]);
      XFlush(ports[i].display);
    }
  return tran;
}
