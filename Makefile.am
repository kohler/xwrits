## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = no-dependencies foreign check-news

bin_PROGRAMS = xwrits
noinst_PROGRAMS = giftoc
man_MANS = xwrits.1

INCLUDES = $(X_CFLAGS)

LDADD = @LIBOBJS@
xwrits_LDADD = $(LDADD) @X_LIBS@ @X_PRE_LIBS@ -lX11 @X_EXTRA_LIBS@ -lm

xwrits_SOURCES = fmalloc.c \
		gif.h giffunc.c gifread.c gifx.h gifx.c \
		xwrits.h clock.c lock.c main.c pictures.c rest.c \
		schedule.c warning.c

giftoc_SOURCES = giftoc.c

BUILT_SOURCES = colorpic.c monopic.c

xwrits_DEPENDENCIES = @LIBOBJS@
giftoc_DEPENDENCIES = @LIBOBJS@

giffunc.o: giffunc.c gif.h
gifread.o: gifread.c gif.h
giftoc.o: giftoc.c
gifx.o: gifx.c gifx.h gif.h

pictures.o: pictures.c colorpic.c monopic.c

COLOR_PIC = color/bars.gif color/lock.gif \
	color/clenchl.gif color/spreadl.gif color/fingerl.gif color/restl.gif \
	color/okl.gif color/clenchi.gif color/spreadi.gif color/fingeri.gif \
	color/resti.gif color/oki.gif color/koreanl.gif
MONO_PIC = mono/barsm.gif mono/lockm.gif \
	mono/clenchlm.gif mono/spreadlm.gif mono/fingerlm.gif mono/restlm.gif \
	mono/oklm.gif mono/clenchim.gif mono/spreadim.gif mono/fingerim.gif \
	mono/restim.gif mono/okim.gif

colorpic.c: $(COLOR_PIC) giftoc
	./giftoc -makename -dir $(srcdir) $(COLOR_PIC) > $@
monopic.c: $(MONO_PIC) giftoc
	./giftoc -makename -dir $(srcdir) $(MONO_PIC) > $@

EXTRA_DIST = xwrits.1 logo.gif rpm.spec
MOSTLYCLEANFILES = colorpic.c monopic.c

dist-hook:
	@mkdir $(distdir)/color $(distdir)/mono
	@d=`cd $(distdir)/color; pwd` && cd $(srcdir) && cp -p $(COLOR_PIC) $$d
	@d=`cd $(distdir)/mono; pwd` && cd $(srcdir) && cp -p $(MONO_PIC) $$d

srclinks:
	for i in fmalloc.c strerror.c ; do \
	ln -sf ~/src/edlib/$$i $$i ; done
	for i in gif.h giffunc.c gifread.c giftoc.c gifx.h gifx.c ; do \
	ln -sf ~/src/giflib/$$i $$i ; done

versionize:
	perl -pi -e "s/^\\.ds V.*/.ds V $(VERSION)/;" xwrits.1
	perl -pi -e "s/^Version: .*/Version: $(VERSION)/; s/$(PACKAGE)-[\w.]+\.tar\.gz/$(PACKAGE)-$(VERSION).tar.gz/;" rpm.spec

rpm: dist
	buildarch=`rpm --showrc | awk '/^build arch/ { print $$4; }'` ; \
	mkdir -p /tmp/rpmb/SOURCES /tmp/rpmb/RPMS/$$buildarch \
	/tmp/rpmb/BUILD ; \
	echo 'topdir: /tmp/rpmb' > /tmp/rpmb/rc ; \
	cp logo.gif $(PACKAGE)-$(VERSION).tar.gz /tmp/rpmb/SOURCES ; \
	rpm --rcfile /tmp/rpmb/rc -bb rpm.spec ; \
	cp /tmp/rpmb/RPMS/$$buildarch/*.rpm .
	rm -rf /tmp/rpmb

.PHONY: srclinks versionize rpm
