#include <config.h>
#include "xwrits.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "gifx.h"
#include "colorpic.c"
#include "monopic.c"

Pixmap bars_pixmap = None;
Pixmap lock_pixmap = None;

Picture *pictures = 0;
Slideshow *current_slideshow = 0;

static Gif_XContext *gfx;

static Gif_Record *large_color_records[] = {
  &clenchl_gif, &spreadl_gif, &fingerl_gif, &koreanl_gif,
  &restl_gif, &okl_gif, &lock_gif, &bars_gif
};
static Gif_Record *icon_color_records[] = {
  &clenchi_gif, &spreadi_gif, &fingeri_gif, 0,
  &resti_gif, &oki_gif, 0, 0
};
static Gif_Record *large_mono_records[] = {
  &clenchlm_gif, &spreadlm_gif, &fingerlm_gif, 0,
  &restlm_gif, &oklm_gif, &lockm_gif, &barsm_gif
};
static Gif_Record *icon_mono_records[] = {
  &clenchim_gif, &spreadim_gif, &fingerim_gif, 0,
  &restim_gif, &okim_gif, 0, 0
};

static Picture *
register_picture(char *name, int offset)
{
  Picture *p;
  
  p = xwNEW(Picture);
  
  p->name = name;
  p->next = pictures;
  p->used = 0;
  p->clock = 0;
  p->background = None;
  pictures = p;
  
  p->clock_x_off = 10;
  p->clock_y_off = 10;
  
  p->offset = offset;
  
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
  register_picture("clench", 0);
  register_picture("spread", 1);
  register_picture("finger", 2);
  register_picture("korean", 3);
  register_picture("resting", 4);
  register_picture("ready", 5);
  p = register_picture("locked", 6);
  p->clock_x_off = 65;
}


void
load_needed_pictures(Window window, int has_lock, int force_mono)
{
  Gif_Record **large_records, **icon_records;
  Picture *p;
  
  gfx = Gif_NewXContext(display, window);
  if (gfx->depth == 1 || force_mono) {
    large_records = large_mono_records;
    icon_records = icon_mono_records;
  } else {
    large_records = large_color_records;
    icon_records = icon_color_records;
  }
  
  for (p = pictures; p; p = p->next)
    if (p->used) {
      Gif_Record *large = large_records[p->offset];
      Gif_Record *icon = icon_records[p->offset];
      
      if (large) {
	Gif_Stream *gfs = Gif_ReadRecord(large);
	if (gfs) p->large = Gif_XImage(gfx, gfs, 0);
	Gif_DeleteStream(gfs);
      }
      
      if (icon) {
	Gif_Stream *gfs = Gif_ReadRecord(icon);
	if (gfs) p->icon = Gif_XImage(gfx, gfs, 0);
	Gif_DeleteStream(gfs);
      }
    }
  
  if (has_lock) {
    Gif_Stream *gfs = Gif_ReadRecord(large_records[5]);
    lock_pixmap = Gif_XImage(gfx, gfs, 0);
    Gif_DeleteStream(gfs);
    gfs = Gif_ReadRecord(large_records[6]);
    bars_pixmap = Gif_XImage(gfx, gfs, 0);
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
    delay.tv_sec = 30 * HOUR_PER_CYCLE * MIN_PER_HOUR * SEC_PER_MIN;
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
	a = grab_alarm_data(A_FLASH, h);
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
      if (!a) a = new_alarm_data(A_FLASH, h);
      xwADDTIME(a->timer, t, ss->delay[i]);
      schedule(a);
    } else {
      if (a) destroy_alarm(a);
    }
    
    set_picture(h, ss, i);
  }
}
