Summary: Reminds you take wrist breaks

Name: xwrits
Version: 2.20
Release: 1
Source: http://www.lcdf.org/xwrits/xwrits-2.20.tar.gz

Icon: logo.gif
URL: http://www.lcdf.org/xwrits/

Group: X11/Utilities
Vendor: Little Cambridgeport Design Factory
Packager: Eddie Kohler <eddietwo@lcs.mit.edu>
Copyright: GPL

BuildRoot: /tmp/xwrits-build

%description
Xwrits reminds you to take wrist breaks, which
should help you prevent or manage a repetitive
stress injury. It pops up an X window when you
should rest; you click on that window, then take a
break.

Xwrits's graphics are brightly colored pictures of
a wrist and the attached hand. The wrist clenches
and stretches ``as if in pain'' when you should
rest, slumps relaxed during the break, and points
forward valiantly when the break is over. It is
trapped behind bars while the keyboard is locked.
Other gestures are included.

Extensive command line options let you control how
often xwrits appears. It can escalate its behavior
over time -- by putting up more flashing windows
or actually locking you out of the keyboard, for
example -- which makes it harder to cheat.

%prep
%setup

%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=$RPM_BUILD_ROOT/usr/X11R6
make

%install
make install

%clean
rm -rf $RPM_BUILD_ROOT

%post

%files
%attr(-,root,root) %doc NEWS README
%attr(0755,root,root) /usr/X11R6/bin/xwrits
%attr(0644,root,root) /usr/X11R6/man/man1/xwrits.1
