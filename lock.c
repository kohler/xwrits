#include <config.h>
#include "xwrits.h"
#include <stdlib.h>
#include <X11/keysym.h>

#define BARS_WIDTH	150
#define WINDOW_HEIGHT	300

struct timeval lock_message_delay;
char *lock_password;

#define REDRAW_MESSAGE ((char *)1L)

static Window *covers;
static Hand **lock_hands;

static char password[MaxPasswordSize];
static int password_pos = -1;


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
draw_message(char *message)
{
  static int msgx, msgy, msgw, msgh;
  static char *cur_message;
  int i, redraw = (message == REDRAW_MESSAGE);
  if (redraw) message = cur_message;
  for (i = 0; i < nports; i++)
    if (covers[i]) {
      if (cur_message && !redraw)
	XClearArea(ports[i].display, covers[i],
		   msgx, msgy, msgw, msgh, False);
      if (message) {
	int length = strlen(message);
	msgw = XTextWidth(ports[i].font, message, length);
	msgh = ports[i].font->ascent + ports[i].font->descent;
	msgx = (ports[i].width - msgw) / 2;
	msgy = (ports[i].height - msgh) / 2;
	XDrawString(ports[i].display, covers[i], ports[i].white_gc,
		    msgx, msgy + ports[i].font->ascent, message, length);
      }
    }
  cur_message = message;
}

static int
check_password(XKeyEvent *xkey)
{
  KeySym keysym;
  XComposeStatus compose;
  char c;
  int nchars = XLookupString(xkey, &c, 1, &keysym, &compose);
  
  if (keysym == XK_Return ||
      (nchars == 1 && (c == '\n' || c == '\r'))) {
    password[password_pos] = 0;
    password_pos = 0;
    if (strcmp(lock_password, password) == 0)
      return 1;
    else 
      draw_message("Incorrect password! Try again");
  } else if (keysym == XK_Delete || keysym == XK_BackSpace) {
    if (password_pos > 0)
      password_pos--;
  } else if (nchars == 1) {
    if (password_pos < MaxPasswordSize - 1)
      password[password_pos++] = c;
  }
  
  return 0;
}


static int
lock_alarm_loop(Alarm *a, struct timeval *now)
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
    password_pos = -1;
    break;
    
  }
  
  return 0;
}


static int
lock_x_loop(XEvent *e, struct timeval *now)
{
  Alarm *a;
  Port *port = find_port(e->xany.display);
  
  switch (e->type) {
    
   case KeyPress:
    if (password_pos == -1) {
      draw_message("Enter password to unlock screen");
      password_pos = 0;
    }
    if (check_password(&e->xkey))
      return TRAN_FAIL;
    a = grab_alarm(A_LOCK_MESS_ERASE);
    if (!a) a = new_alarm(A_LOCK_MESS_ERASE);
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
  struct timeval now;
  Alarm *a;
  int i, successful_grabs;
  
  XEvent event;
  int tran = TRAN_FAIL;

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
  xwGETTIME(now);

  a = new_alarm(A_AWAKE);
  xwADDTIME(a->timer, ocurrent->break_time, now);
  clock_zero_time = a->timer;
  schedule(a);
  
  a = new_alarm(A_LOCK_BOUNCE);
  xwADDTIME(a->timer, ocurrent->lock_bounce_delay, now);
  schedule(a);
  
  if (ocurrent->break_clock) {
    draw_all_clocks(&now);
    a = new_alarm(A_CLOCK);
    xwADDTIME(a->timer, now, clock_tick);
    schedule(a);
  }

  draw_message(0);
  password_pos = -1;
  
  tran = loopmaster(lock_alarm_loop, lock_x_loop);

 no_keyboard_grab:
  unschedule(A_CLOCK | A_LOCK_BOUNCE | A_AWAKE | A_LOCK_MESS_ERASE);
  for (i = 0; i < nports; i++)
    if (covers[i]) {
      XUngrabKeyboard(ports[i].display, CurrentTime);
      destroy_hand(lock_hands[i]);
      XDestroyWindow(ports[i].display, covers[i]);
      XFlush(ports[i].display);
    }
  return tran;
}
