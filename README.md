Xwrits
======

Xwrits was written when my wrists really hurt. They don’t any more—most of the
time.

Please see the man page (xwrits.1) for more information. The man page is on
the Web at <http://www.lcdf.org/xwrits/man.html>.

Building and installation
-------------------------

You need an ANSI C compiler, such as gcc, and X11 libraries and includes.

Just type `./configure`, then `make`. If checking out from source, start with
`./bootstrap.sh` (you will need an autotools installation).

If `./configure` can’t find your X includes and libraries, you need to tell it
where they are. You do this with the `--x-includes=DIRECTORY` and
`--x-libraries=DIRECTORY` options.

`./configure` accepts the usual options. See `INSTALL` for more details.

Copyright
---------

Source and pictures copyright 1994–2018 Eddie Kohler, ekohler@gmail.com and
http://lcdf.org/~kohler/

This package is distributed under the GNU General Public License, Version 2
(and only Version 2). The GNU General Public License is available via the Web
at <http://www.gnu.org/licenses/gpl2.html>. The GPL is designed to allow you
to alter and redistribute the package, as long as you do not deny that freedom
to others.

Bugs, suggestions, etc.
-----------------------

Please write me if you have trouble building or running xwrits, or if you have
any suggestions, or if you know some rude finger gestures not listed in
`GESTURES.md`.
