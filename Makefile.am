## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = foreign check-news

bin_PROGRAMS = xwrits
noinst_PROGRAMS = giftoc
man_MANS = xwrits.1

CFLAGS = @CFLAGS@ @X_CFLAGS@
AM_CPPFLAGS = -I$(top_srcdir)/include

LDADD = @LIBOBJS@
xwrits_LDADD = $(LDADD) @X_LIBS@ @X_PRE_LIBS@ -lX11 @X_EXT_LIBS@ @X_EXTRA_LIBS@ -lm

xwrits_SOURCES = fmalloc.c \
		giffunc.c gifread.c gifx.c \
		xwrits.h clock.c hands.c lock.c main.c pictures.c rest.c \
		schedule.c warning.c

giftoc_SOURCES = giftoc.c

BUILT_SOURCES = colorpic.c monopic.c

xwrits_DEPENDENCIES = @LIBOBJS@
giftoc_DEPENDENCIES = @LIBOBJS@

giffunc.o gifread.o giftoc.o gifx.o: include/lcdfgif/gif.h config.h
pictures.o: pictures.c colorpic.c monopic.c
clock.o lock.o main.o pictures.o rest.o schedule.o warning.o: xwrits.h config.h

COLOR_PIC = color/bars.gif color/lock.gif \
	color/clenchl.gif color/spreadl.gif color/fingerl.gif color/restl.gif \
	color/okl.gif color/clenchi.gif color/spreadi.gif color/fingeri.gif \
	color/resti.gif color/oki.gif color/koreanl.gif color/koreani.gif \
	color/germanl.gif color/germani.gif
MONO_PIC = mono/barsm.gif mono/lockm.gif \
	mono/clenchlm.gif mono/spreadlm.gif mono/fingerlm.gif mono/restlm.gif \
	mono/oklm.gif mono/clenchim.gif mono/spreadim.gif mono/fingerim.gif \
	mono/restim.gif mono/okim.gif

colorpic.c: $(COLOR_PIC) giftoc
	./giftoc -makename -dir $(srcdir) $(COLOR_PIC) > $@
monopic.c: $(MONO_PIC) giftoc
	./giftoc -makename -dir $(srcdir) $(MONO_PIC) > $@

EXTRA_DIST = GESTURES xwrits.1 logo.gif xwrits.spec \
	include/lcdf/inttypes.h include/lcdfgif/gif.h include/lcdfgif/gifx.h
MOSTLYCLEANFILES = colorpic.c monopic.c

dist-hook:
	@mkdir $(distdir)/color $(distdir)/mono
	@d=`cd $(distdir)/color; pwd` && cd $(srcdir) && cp -p $(COLOR_PIC) $$d
	@d=`cd $(distdir)/mono; pwd` && cd $(srcdir) && cp -p $(MONO_PIC) $$d

srclinks:
	cd $(top_srcdir); for i in fmalloc.c strerror.c ; do \
	ln -sf ~/src/liblcdf/liblcdf/$$i $$i ; done
	cd $(top_srcdir); test -d include/lcdf || mkdir -p include/lcdf
	cd $(top_srcdir); test -d include/lcdfgif || mkdir -p include/lcdfgif
	cd $(top_srcdir); for i in inttypes.h; do \
	ln -sf ~/src/liblcdf/include/lcdf/$$i include/lcdf/$$i; done
	cd $(top_srcdir); for i in gif.h gifx.h; do \
	ln -sf ~/src/liblcdf/include/lcdfgif/$$i include/lcdfgif/$$i; done
	cd $(top_srcdir); for i in giffunc.c gifread.c giftoc.c gifx.c ; do \
	ln -sf ~/src/liblcdf/liblcdfgif/$$i $$i ; done

versionize:
	perl -pi -e "s/^\\.ds V.*/.ds V $(VERSION)/;" xwrits.1
	perl -pi -e "s/^Version: .*/Version: $(VERSION)/; s/$(PACKAGE)-[\w.]+\.tar\.gz/$(PACKAGE)-$(VERSION).tar.gz/;" xwrits.spec

rpm: dist
	buildarch=`rpm --showrc | awk '/^build arch/ { print $$4; }'` ; \
	mkdir -p /tmp/rpm-xw/SOURCES /tmp/rpm-xw/RPMS/$$buildarch \
	/tmp/rpm-xw/BUILD ; \
	echo 'include: /usr/lib/rpm/rpmrc' > /tmp/rpm-xw/rc ; \
	echo 'macrofiles: /usr/lib/rpm/macros:/tmp/rpm-xw/macros' >> /tmp/rpm-xw/rc ; \
	echo '%_topdir /tmp/rpm-xw' > /tmp/rpm-xw/macros ; \
	cp logo.gif $(PACKAGE)-$(VERSION).tar.gz /tmp/rpm-xw/SOURCES ; \
	rpmbuild --rcfile /tmp/rpm-xw/rc -bb xwrits.spec ; \
	cp /tmp/rpm-xw/RPMS/$$buildarch/*.rpm .
	rm -rf /tmp/rpm-xw

.PHONY: srclinks versionize rpm
