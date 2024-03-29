Project has left GitHub
-----------------------

It is now here: [https://codeberg.org/a-j-wood/postprox](https://codeberg.org/a-j-wood/postprox)

This project was briefly hosted on GitHub.  GitHub is a proprietary,
trade-secret system that is not Free and Open Source Software (FOSS).

Read about the [Give up GitHub](https://GiveUpGitHub.org) campaign from
[the Software Freedom Conservancy](https://sfconservancy.org) to understand
some of the reasons why GitHub is not a good place to host FOSS projects.

Any use of this project's code by GitHub Copilot, past or present, is done
without permission.  The project owner does not consent to GitHub's use of
this project's code in Copilot.

![Logo of the GiveUpGitHub campaign](https://sfconservancy.org/img/GiveUpGitHub.png)


Introduction
------------

This is the README for "postprox", a minimal Postfix SMTP proxy.  Postprox
is an SMTP proxy which copies an SMTP conversation between its input and
another SMTP server, but spools the DATA portion to a temporary file and
runs a specified program on it before passing it on to the output server -
or outputting an SMTP error code instead if the content filter says so.


Documentation
-------------

A manual page is included in this distribution.  If you prefer plain text,
then look at "doc/quickref.txt" for a text version.


Dependencies
------------

This package was written to be used as a before-queue or after-queue Postfix
content filter.  It reads and writes SMTP on stdin/out, so may or may not be
usable with other mailers.


Compilation
-----------

To compile the package, type "`sh ./configure`", which should generate a
Makefile for your system.  You may then type "`make`" to build everything. 
You may need GNU "`make`" for compilation to work properly.

See the file "doc/INSTALL" for more about the "configure" script.

Developers note that you can do "`./configure --enable-debugging`" to cause
debugging support to be built in.  Also note that doing "`make index`" will
generate an HTML code index (using "`ctags`" and "`cproto`"); this index lists
all files used, all functions defined, and all TODOs marked in the code.


Author
------

This package is copyright (C) 2006 Andrew Wood, and is being distributed
under the terms of the Artistic License.  For more details, see the file
"doc/COPYING".

You can contact me by using the contact form on my web page at
http://www.ivarch.com/.

The "postprox" home page is at:

  http://www.ivarch.com/programs/postprox.shtml

The latest version can always be found here.

Credit is also due to:

 * Graham Maltby - Patch for extended recipients logging, longer filter messages
 * Jeremy Fitzhardinge (http://www.goop.org/~jeremy/) - Detailed report and analysis of input/output sync problem

-----------------------------------------------------------------------------
