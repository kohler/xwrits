#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "xwrits.h"

#include "gifx.h"

#include "large/gifs.c"
#include "icon/gifs.c"
#include "other/gifs.c"

Pixmap lockpix;
Pixmap barspix;
unsigned long lockpixbackground;

Picture *pictures = 0;
Slideshow *current_slideshow = 0;

static Drawable drawable;
static Gif_XContext *gfx;


static Picture *
register_picture(char *name, Gif_Record *large, Gif_Record *icon)
{
  Picture *p;
  
  p = xwNEW(Picture);
  
  p->name = name;
  p->next = pictures;
  p->used = 0;
  p->clock = 0;
  pictures = p;
  
  p->clock_x_off = 10;
  p->clock_y_off = 10;
  
  p->large_record = large;
  p->icon_record = icon;
  
  return p;
}


static Picture *
find_picture(char *name)
{
  Picture *p = pictures;
  while (p && strcmp(p->name, name) != 0) p = p->next;
  return p;
}


void
default_pictures(void)
{
  Picture *p;
  register_picture("clench", &clenchl_gif, &clenchi_gif);
  register_picture("spread", &spreadl_gif, &spreadi_gif);
  register_picture("finger", &fingerl_gif, &fingeri_gif);
  register_picture("ready", &okl_gif, &oki_gif);
  register_picture("resting", &restingl_gif, &restingi_gif);
  p = register_picture("locked", &lock_gif, 0);
  p->clock_x_off = 65;
}


void
load_needed_pictures(Window window, int haslock)
{
  Picture *p;
  drawable = window;
  gfx = Gif_NewXContext(display, window);
  
  for (p = pictures; p; p = p->next)
    if (p->used) {
      
      if (p->large_record) {
	Gif_Stream *gfs = Gif_ReadRecord(p->large_record);
	if (gfs) {
	  Gif_Color *gfc;
	  p->large = Gif_XImage(gfx, gfs, 0);
	  gfc = Gif_GetBackground(gfs, 0);
	  if (gfc) p->background = gfc->pixel;
	  Gif_DeleteStream(gfs);
	}
      }
      
      if (p->icon_record) {
	Gif_Stream *gfs = Gif_ReadRecord(p->icon_record);
	if (gfs) {
	  p->icon = Gif_XImage(gfx, gfs, 0);
	  Gif_DeleteStream(gfs);
	}
      }
      
    }
  
  if (haslock) {
    Gif_Stream *gfs = Gif_ReadRecord(&bars_gif);
    barspix = Gif_XImage(gfx, gfs, 0);
    Gif_DeleteStream(gfs);
  }
}


Slideshow *
parse_slideshow(char *slideshowtext, struct timeval *delayinput)
{
  char buf[BUFSIZ];
  char *s;
  Slideshow *ss;
  int size;
  Picture *picture;
  struct timeval delay;
  
  if (strlen(slideshowtext) >= BUFSIZ) return 0;
  strcpy(buf, slideshowtext);
  s = buf;
  
  ss = xwNEW(Slideshow);
  ss->nslides = 0;
  ss->picture = xwNEWARR(Picture *, 4);
  ss->delay = xwNEWARR(struct timeval, 4);
  size = 4;
  
  if (delayinput) delay = *delayinput;
  if (!delayinput || xwTIMELEQ0(delay)) {
    /* Simulate never flashing by flashing with a VERY long period --
       30 days. */
    delay.tv_sec = 30 * HrPerCycle * MinPerHr * SecPerMin;
    delay.tv_usec = 0;
  }
  
  while (*s) {
    char *n, save;
    
    while (isspace(*s)) s++;
    n = s;
    while (!isspace(*s) && *s != ';' && *s) s++;
    save = *s;
    *s = 0;
    
    picture = find_picture(n);
    
    if (picture) {
      if (ss->nslides >= size) {
	size *= 2;
	xwREARRAY(ss->picture, Picture *, size);
	xwREARRAY(ss->delay, struct timeval, size);
      }
      ss->picture[ss->nslides] = picture;
      ss->delay[ss->nslides] = delay;
      ss->nslides++;
      
      picture->used = 1;
    }
    
    *s = save;
    if (*s) s++;
  }
  
  return ss;
}


void
blend_slideshow(Slideshow *ss)
{
  Hand *h;
  Picture *p;
  struct timeval t, now;
  
  current_slideshow = ss;
  
  xwGETTIME(now);
  for (h = hands; h; h = h->next) {
    int i = 0;
    Alarm *a = 0;
    t = now;
    
    if (h->slideshow) {
      p = h->slideshow->picture[h->slide];
      if (h->slideshow->nslides > 1) {
	a = grab_alarm_data(Flash, h);
	if (a)
	  xwSUBTIME(t, a->timer, h->slideshow->delay[h->slide]);
      }
      /* Leave i with a picture that matches the old one, or 0 if no
	 picture matches the old one. */
      for (i = ss->nslides - 1; i > 0; i--)
	if (ss->picture[i] == p)
	  break;
    }
    
    if (ss->nslides > 1) {
      if (!a) a = new_alarm_data(Flash, h);
      xwADDTIME(a->timer, t, ss->delay[i]);
      schedule(a);
    } else {
      if (a) destroy_alarm(a);
    }
    
    set_picture(h, ss, i);
  }
}
