#include <config.h>
#include "xwrits.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "colorpic.c"
#include "monopic.c"

Pixmap bars_pixmap = None;
Pixmap lock_pixmap = None;

Gif_Stream *current_slideshow = 0;

#define NPICTURES 14

static const char *built_in_picture_names[NPICTURES] = {
  "&clench", "&spread", "&finger", "&korean",
  "&resting", "&ready", "&locked", "&bars",
  "&clenchicon", "&spreadicon", "&fingericon", "&koreanicon",
  "&restingicon", "&readyicon"
};

static Gif_Record *color_records[NPICTURES] = {
  &clenchl_gif, &spreadl_gif, &fingerl_gif, &koreanl_gif,
  &restl_gif, &okl_gif, &lock_gif, &bars_gif,
  &clenchi_gif, &spreadi_gif, &fingeri_gif, 0,
  &resti_gif, &oki_gif
};
static Gif_Record *mono_records[NPICTURES] = {
  &clenchlm_gif, &spreadlm_gif, &fingerlm_gif, 0,
  &restlm_gif, &oklm_gif, &lockm_gif, &barsm_gif,
  &clenchim_gif, &spreadim_gif, &fingerim_gif, 0,
  &restim_gif, &okim_gif
};

static Gif_Stream *color_streams[NPICTURES];
static Gif_Stream *mono_streams[NPICTURES];


static void
free_picture(void *v)
{
  Picture *p = (Picture *)v;
  if (p->pix)
    XFreePixmap(display, p->pix);
  xfree(p);
}

static void
add_picture(Gif_Image *gfi, int clock_x_off, int clock_y_off)
{
  Picture *p = (Picture *)xmalloc(sizeof(Picture));
  p->pix = None;
  p->clock_x_off = clock_x_off;
  p->clock_y_off = clock_y_off;
  
  gfi->user_data = p;
  gfi->free_user_data = free_picture;
}


Gif_Image *
get_built_in_image(const char *name, int mono)
{
  Gif_Stream **store;
  Gif_Record **records;
  Gif_Stream *gfs;
  int i;
  
  for (i = 0; i < NPICTURES; i++)
    if (strcmp(built_in_picture_names[i], name) == 0)
      goto found;
  return 0;
  
 found:
  store = (mono ? mono_streams : color_streams);
  if (store[i])
    return store[i]->images[0];
  
  records = (mono ? mono_records : color_records);
  if (!records[i])
    return 0;
  
  gfs = Gif_ReadRecord(records[i]);
  if (!gfs)
    return 0;
  
  add_picture(gfs->images[0], (strcmp(name, "&locked") == 0 ? 65 : 10), 10);
  if (!gfs->images[0]->local) {
    gfs->images[0]->local = gfs->global;
    gfs->global->refcount++;
  }
  
  store[i] = gfs;
  return gfs->images[0];
}


#define MIN_DELAY 4

Gif_Stream *
parse_slideshow(const char *slideshowtext, double flash_rate_ratio, int mono)
{
  char buf[BUFSIZ];
  char *s;
  Gif_Stream *gfs;
  Gif_Image *gfi;
  u_int16_t delay;
  int i;
  double d;
  
  if (strlen(slideshowtext) >= BUFSIZ) return 0;
  strcpy(buf, slideshowtext);
  s = buf;
  
  gfs = Gif_NewStream();
  gfs->loopcount = 0;

  d = flash_rate_ratio * DEFAULT_FLASH_DELAY_SEC * 100;
  if (d < 0 || d > 0xFFFF)
    delay = 0xFFFF;
  else
    delay = (d < MIN_DELAY ? MIN_DELAY : (u_int16_t)d);
  
  while (*s) {
    char *n, save;
    
    while (isspace(*s)) s++;
    n = s;
    while (!isspace(*s) && *s != ';' && *s) s++;
    save = *s;
    *s = 0;
    
    gfi = get_built_in_image(n, mono);
    if (gfi) {
      gfi->delay = delay;
      Gif_AddImage(gfs, gfi);
      
    } else {
      /* load file from disk */
      FILE *f = fopen(n, "rb");
      Gif_Stream *read = Gif_FullReadFile(f, GIF_READ_COMPRESSED, 0, 0);
      if (!f)
	error("%s: %s", n, strerror(errno));
      else if (!read || (read->nimages == 0 && read->errors > 0))
	error("%s: not a GIF", n);
      else {
	/* adapt delays */
	if (read->nimages == 1)
	  read->images[0]->delay = delay;
	else
	  for (i = 0; i < read->nimages; i++) {
	    gfi = read->images[i];
	    d = gfi->delay * flash_rate_ratio;
	    if (d < 0 || d >= 0xFFFF)
	      gfi->delay = 0xFFFF;
	    else
	      gfi->delay = (d < MIN_DELAY ? MIN_DELAY : (u_int16_t)d);
	  }
	/* account for background and loopcount */
	if (gfs->nimages == 0) {
	  if (read->global) {
	    gfs->global = read->global;
	    read->global->refcount++;
	    gfs->background = read->background;
	  }
	  gfs->loopcount = read->loopcount;
	}
	/* adapt screen size */
	if (read->screen_width > gfs->screen_width)
	  gfs->screen_width = read->screen_width;
	if (read->screen_height > gfs->screen_height)
	  gfs->screen_height = read->screen_height;
	/* add images to gfs */
	for (i = 0; i < read->nimages; i++) {
	  Gif_Image *gfi = read->images[i];
	  if (!gfi->local && read->global) {
	    gfi->local = read->global;
	    read->global->refcount++;
	  }
	  Gif_AddImage(gfs, gfi);
	  add_picture(gfi, 10, 10);
	  if (gfi->delay == 0)
	    gfi->delay = 1;
	}
      }
      if (read) Gif_DeleteStream(read);
      if (f) fclose(f);
    }
    
    *s = save;
    if (*s) s++;
  }

  /* make sure screen_width and screen_height aren't 0 */
  if (gfs->screen_width == 0 || gfs->screen_height == 0 || gfs->nimages == 1) {
    gfs->screen_width = gfs->screen_height = 0;
    for (i = 0; i < gfs->nimages; i++) {
      gfi = gfs->images[i];
      if (gfi->width > gfs->screen_width)
	gfs->screen_width = gfi->width;
      if (gfi->height > gfs->screen_height)
	gfs->screen_height = gfi->height;
    }
  }
  
  return gfs;
}


void
set_slideshow(Hand *h, Gif_Stream *gfs, struct timeval *now)
{
  int i = 0;
  Alarm *a;
  struct timeval t;

  if (h->slideshow == gfs)
    return;
  
  if (now)
    t = *now;
  else
    xwGETTIME(t);
  
  a = grab_alarm_data(A_FLASH, h);
  
  if (h->slideshow) {
    Gif_Image *cur_im = h->slideshow->images[h->slide];
    if (a) xwSUBDELAY(t, a->timer, cur_im->delay);
    /* Leave i with a picture that matches the old one, or 0 if no
       picture matches the old one. */
    for (i = gfs->nimages - 1; i > 0; i--)
      if (gfs->images[i] == cur_im)
	break;
  }
  
  if (gfs->nimages > 1) {
    if (!a) a = new_alarm_data(A_FLASH, h);
    xwADDDELAY(a->timer, t, gfs->images[i]->delay);
    schedule(a);
  } else {
    if (a) destroy_alarm(a);
  }
  
  h->slideshow = gfs;
  
  if ((gfs->screen_width != h->width || gfs->screen_height != h->height)
      && !h->is_icon) {
    XWindowChanges wmch;
    XSizeHints *xsh = XAllocSizeHints();
    xsh->flags = USPosition | PMinSize | PMaxSize;
    xsh->min_width = xsh->max_width = gfs->screen_width;
    xsh->min_height = xsh->max_height = gfs->screen_height;
    wmch.width = gfs->screen_width;
    wmch.height = gfs->screen_height;
    XSetWMNormalHints(display, h->w, xsh);
    XReconfigureWMWindow(display, h->w, port.screen_number,
			 CWWidth | CWHeight, &wmch);
    XFree(xsh);
  }

  h->loopcount = 0;
  h->slide = i;
  draw_slide(h);
}

void
set_all_slideshows(Hand *hands, Gif_Stream *gfs)
{
  Hand *h;
  struct timeval now;
  xwGETTIME(now);
  for (h = hands; h; h = h->next)
    set_slideshow(h, gfs, &now);
  current_slideshow = gfs;
}
