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
  static int lockx, locky;
  if (domove) {
    XClearArea(display, cover, lockx, locky, WindowWidth, WindowHeight, False);
    lockx = ((rand() >> 4) % (display_width / WindowWidth * 2 - 1))
      * (WindowWidth / 2);
    locky = (rand() >> 4) % (display_height - WindowHeight);
  }
  if (barspix) /* FIXME */
    XCopyArea(display, barspix, cover, gc, 0, 0,
	      WindowWidth, WindowHeight, lockx, locky);
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
    msgw = XTextWidth(font, message, length);
    msgh = font->ascent + font->descent;
    msgx = (display_width - msgw) / 2;
    msgy = (display_height - msgh) / 2;
    XDrawString(display, cover, gc, msgx, msgy + font->ascent,
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
    
   case LockBounce:
    move_lock(1);
    draw_message(RedrawMessage);
    xwADDTIME(a->timer, a->timer, ocurrent->lock_bounce_delay);
    schedule(a);
    break;
    
   case LockClock:
    draw_clock(now);
    move_lock(0);
    draw_message(0);
    xwADDTIME(a->timer, a->timer, clock_tick);
    schedule(a);
    break;
    
   case LockMessErase:
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
      return LockCancelled;
    a = new_alarm(LockMessErase);
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
  int val = LockFailed;
  
  blend_slideshow(slideshow[Locked]);
  
  {
    XSetWindowAttributes setattr;
    unsigned long cwmask = CWBackingStore | CWSaveUnder | CWOverrideRedirect;
    if (!barspix) {
      setattr.background_pixel = black_pixel;
      cwmask |= CWBackPixel;
    } else {
      setattr.background_pixmap = barspix;
      cwmask |= CWBackPixmap;
    }
    setattr.backing_store = NotUseful;
    setattr.save_under = False;
    setattr.override_redirect = True;
    cover = XCreateWindow(display, root_window,
			  0, 0, display_width, display_height,
			  0, CopyFromParent, InputOutput, CopyFromParent,
			  cwmask, &setattr);
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
    gcv.font = font->fid;
    gcv.foreground = white_pixel;
    gc = XCreateGC(display, cover, GCFont | GCForeground, &gcv);
  }
  
  xwGETTIME(now);
  
  a = new_alarm(Return);
  xwADDTIME(a->timer, break_delay, now);
  clock_zero_time = a->timer;
  schedule(a);
  
  a = new_alarm(LockBounce);
  xwADDTIME(a->timer, ocurrent->lock_bounce_delay, now);
  schedule(a);
  
  if (ocurrent->break_clock) {
    draw_clock(&now);
    a = new_alarm(LockClock);
    xwADDTIME(a->timer, now, clock_tick);
    schedule(a);
  }
  
  draw_message(0);
  move_lock(1);
  passwordpos = -1;
  
  val = loopmaster(lockalarmloop, lockxloop);
  if (val == Return) val = LockOK;
  
  XUngrabKeyboard(display, CurrentTime);
  
 no_keyboard_grab:

  unschedule(LockClock | LockBounce | Return | LockMessErase);
  if (ocurrent->break_clock) erase_clock();
  XDestroyWindow(display, cover);
  XFlush(display);
  
  return val;
}
