#include "xwrits.h"
#include <stdlib.h>
#include <X11/keysym.h>


struct timeval lock_message_delay;
char *lock_password;

#define RedrawMessage ((char *)1L)

static Window cover;
static GC gc;
static char password[MaxPasswordSize];
static int passwordpos = -1;


static void
move_lock(int domove)
{
  static int lock_x, lock_y;
  if (domove) {
    XClearArea(display, cover, lock_x, lock_y, WindowWidth, WindowHeight,
	       False);
    lock_x = ((rand() >> 4) % (port.width / WindowWidth * 2 - 1))
      * (WindowWidth / 2);
    lock_y = (rand() >> 4) % (port.height - WindowHeight);
    lock_y = (lock_y & ~0x3) | 0x2;
  }
  if (lock_pixmap)
    XCopyArea(display, lock_pixmap, cover, gc, 0, 0,
	      WindowWidth, WindowHeight, lock_x, lock_y);
  XFlush(display);
}


static void
draw_message(char *message)
{
  static int msgx, msgy, msgw, msgh;
  static char *oldmessage;
  if (oldmessage && message != RedrawMessage)
    XClearArea(display, cover, msgx, msgy, msgw, msgh, False);
  if (message == RedrawMessage) message = oldmessage;
  if (message) {
    int length = strlen(message);
    msgw = XTextWidth(port.font, message, length);
    msgh = port.font->ascent + port.font->descent;
    msgx = (port.width - msgw) / 2;
    msgy = (port.height - msgh) / 2;
    XDrawString(display, cover, gc, msgx, msgy + port.font->ascent,
		message, length);
  }
  oldmessage = message;
}


static int
checkpassword(XKeyEvent *xkey)
{
  KeySym keysym;
  XComposeStatus compose;
  char c;
  int nchars = XLookupString(xkey, &c, 1, &keysym, &compose);
  
  if (keysym == XK_Return ||
      (nchars == 1 && (c == '\n' || c == '\r'))) {
    password[passwordpos] = 0;
    passwordpos = 0;
    if (strcmp(lock_password, password) == 0)
      return 1;
    else 
      draw_message("Incorrect password! Try again");
  } else if (keysym == XK_Delete || keysym == XK_BackSpace) {
    if (passwordpos > 0)
      passwordpos--;
  } else if (nchars == 1) {
    if (passwordpos < MaxPasswordSize - 1)
      password[passwordpos++] = c;
  }
  
  return 0;
}


static int
lockalarmloop(Alarm *a, struct timeval *now)
{
  switch (a->action) {
    
   case A_LOCK_BOUNCE:
    move_lock(1);
    draw_message(RedrawMessage);
    xwADDTIME(a->timer, a->timer, ocurrent->lock_bounce_delay);
    schedule(a);
    break;
    
   case A_LOCK_CLOCK:
    draw_clock(now);
    move_lock(0);
    draw_message(0);
    xwADDTIME(a->timer, a->timer, clock_tick);
    schedule(a);
    break;
    
   case A_LOCK_MESS_ERASE:
    draw_message(0);
    move_lock(0);
    passwordpos = -1;
    break;
    
  }
  
  return 0;
}


static int
lockxloop(XEvent *e)
{
  Alarm *a;
  
  switch (e->type) {
    
   case KeyPress:
    if (passwordpos == -1) {
      draw_message("Enter password to unlock screen");
      passwordpos = 0;
    }
    if (checkpassword(&e->xkey))
      return TRAN_FAIL;
    a = new_alarm(A_LOCK_MESS_ERASE);
    xwGETTIME(a->timer);
    xwADDTIME(a->timer, a->timer, lock_message_delay);
    schedule(a);
    break;
    
   case VisibilityNotify:
    if (e->xvisibility.state != VisibilityUnobscured) {
      XRaiseWindow(display, cover);
      move_lock(0);
      draw_message(RedrawMessage);
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
  
  XEvent event;
  int tran = TRAN_FAIL;
  
  blend_slideshow(slideshow[Locked]);
  
  {
    XSetWindowAttributes setattr;
    unsigned long cwmask = CWBackingStore | CWSaveUnder | CWOverrideRedirect
      | CWBorderPixel | CWColormap;
    if (!bars_pixmap) {
      setattr.background_pixel = port.black;
      cwmask |= CWBackPixel;
    } else {
      setattr.background_pixmap = bars_pixmap;
      cwmask |= CWBackPixmap;
    }
    setattr.backing_store = NotUseful;
    setattr.save_under = False;
    setattr.override_redirect = True;
    setattr.colormap = port.colormap;
    setattr.border_pixel = 0;
    cover = XCreateWindow
      (display, port.root_window,
       0, 0, port.width, port.height, 0,
       port.depth, InputOutput, port.visual, cwmask, &setattr);
    XSelectInput(display, cover, ButtonPressMask | ButtonReleaseMask |
		 KeyPressMask | VisibilityChangeMask | ExposureMask);
    XMapRaised(display, cover);
    XSync(display, False);
  }
  
  XWindowEvent(display, cover, ExposureMask, &event);
  if (XGrabKeyboard(display, cover, True, GrabModeAsync, GrabModeAsync,
		    CurrentTime) != GrabSuccess)
    goto no_keyboard_grab;
  
  if (!gc) {
    XGCValues gcv;
    gcv.font = port.font->fid;
    gcv.foreground = port.white;
    gc = XCreateGC(display, cover, GCFont | GCForeground, &gcv);
  }
  
  xwGETTIME(now);
  
  a = new_alarm(A_AWAKE);
  xwADDTIME(a->timer, break_delay, now);
  clock_zero_time = a->timer;
  schedule(a);
  
  a = new_alarm(A_LOCK_BOUNCE);
  xwADDTIME(a->timer, ocurrent->lock_bounce_delay, now);
  schedule(a);
  
  if (ocurrent->break_clock) {
    draw_clock(&now);
    a = new_alarm(A_LOCK_CLOCK);
    xwADDTIME(a->timer, now, clock_tick);
    schedule(a);
  }
  
  draw_message(0);
  move_lock(1);
  passwordpos = -1;
  
  tran = loopmaster(lockalarmloop, lockxloop);
  
  XUngrabKeyboard(display, CurrentTime);
  
 no_keyboard_grab:
  
  unschedule(A_LOCK_CLOCK | A_LOCK_BOUNCE | A_AWAKE | A_LOCK_MESS_ERASE);
  if (ocurrent->break_clock) erase_clock();
  XDestroyWindow(display, cover);
  XFlush(display);
  
  return tran;
}
