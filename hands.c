#include <config.h>
#include "xwrits.h"
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <assert.h>

Hand *hands;
Hand *icon_hands;

#define NEW_HAND_TRIES 6


/* creating a new hand */

static int icon_width;
static int icon_height;

static void
get_icon_size()
{
  XIconSize *ic;
  int nic;
  icon_width = ocurrent->icon_slideshow->screen_width;
  icon_height = ocurrent->icon_slideshow->screen_height;
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

#define xwMAX(i, j) ((i) > (j) ? (i) : (j))
#define xwMIN(i, j) ((i) < (j) ? (i) : (j))

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
	overw = xwMIN(x2, h->x + h->width) - xwMAX(x1, h->x);
	overh = xwMIN(y2, h->y + h->height) - xwMAX(y1, h->y);
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
  int width = ocurrent->slideshow->screen_width;
  int height = ocurrent->slideshow->screen_height;
  
  if (x == NEW_HAND_CENTER)
    x = (port.width - width) / 2;
  if (y == NEW_HAND_CENTER)
    y = (port.height - height) / 2;
  
  if (x == NEW_HAND_RANDOM || y == NEW_HAND_RANDOM) {
    int xs[NEW_HAND_TRIES], ys[NEW_HAND_TRIES], i;
    int xdist = port.width - width;
    int ydist = port.height - height;
    int xrand = x == NEW_HAND_RANDOM;
    int yrand = y == NEW_HAND_RANDOM;
    for (i = 0; i < NEW_HAND_TRIES; i++) {
      if (xrand) xs[i] = (rand() >> 4) % xdist;
      else xs[i] = x;
      if (yrand) ys[i] = (rand() >> 4) % ydist;
      else ys[i] = y;
    }
    get_best_position(xs, ys, NEW_HAND_TRIES, width, height, &x, &y);
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
    if (!ocurrent->never_iconify) {
      mwm_hints[1] |= (1L << 3); /* MWM_FUNC_MINIMIZE */
      mwm_hints[2] |= (1L << 5); /* MWM_DECOR_MINIMIZE */
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
  XSetWMProtocols(display, nh->w, &port.wm_delete_window_atom, 1);
  XChangeProperty(display, nh->w, port.mwm_hints_atom, port.mwm_hints_atom, 32,
		  PropModeReplace, (unsigned char *)mwm_hints, 4);
  
  XSelectInput(display, nh->w, ButtonPressMask | StructureNotifyMask
	       | KeyPressMask | VisibilityChangeMask | ExposureMask);
  
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

/* destroy a hand */

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


/* count active hands (mapped or iconified) */

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


/* translate a Window to a Hand */

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


/* draw a picture on a hand */

static void
ensure_picture(Gif_Stream *gfs, int n)
{
  Gif_Image *gfi = gfs->images[n];
  Picture *p, *last_p, *last_last_p;
  int i;
  
  for (i = 0; i < n; i++) {
    p = (Picture *)gfs->images[i]->user_data;
    if (!p->pix)		/* no picture cached for earlier image */
      ensure_picture(gfs, i);
  }
  
  p = (Picture *)gfi->user_data;
  last_p = (n > 0 ? (Picture *)gfs->images[n-1]->user_data : 0);
  last_last_p = (n > 1 ? (Picture *)gfs->images[n-2]->user_data : 0);
  
  /* reuse pixmaps from other streams */
  if (p->canonical && gfi->transparent < 0 && gfi->left == 0 && gfi->top == 0
      && gfi->width == gfs->screen_width && gfi->height == gfs->screen_height
      && ((Picture *)p->canonical->user_data)->pix) {
    p->pix = ((Picture *)p->canonical->user_data)->pix;
    return;
  } else if (p->canonical)
    p->canonical = 0;
  
  p->pix = Gif_XNextImage(port.gfx, (last_last_p ? last_last_p->pix : None),
			  (last_p ? last_p->pix : None),
			  gfs, n);
}

void
draw_slide(Hand *h)
{
  Gif_Stream *gfs;
  Gif_Image *gfi;
  Picture *p;

  assert(h && h->slideshow);
  gfs = h->slideshow;
  gfi = gfs->images[h->slide];
  p = (Picture *)gfi->user_data;
  
  if (!p->pix)
    ensure_picture(gfs, h->slide);
  
  XSetWindowBackgroundPixmap(display, h->w, p->pix);
  XClearWindow(display, h->w);
  
  if (h->clock)
    draw_clock(h, 0);
}
