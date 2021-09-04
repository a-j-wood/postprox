#
# Rules for all phony targets.
#

.PHONY: all help dep depend depclean make \
  check test memtest rats neat \
  clean distclean cvsclean \
  index manhtml indent indentclean \
  changelog doc dist release \
  install uninstall \
  rpmbuild srpm rpm deb

all: $(alltarg)

help:
	@echo 'This Makefile has the following utility targets:'
	@echo
	@echo '  all             build all binary targets'
	@echo '  doc             regenerate text version of man page'
	@echo '  install         install compiled package and manual'
	@echo '  uninstall       uninstall the package'
	@echo '  check / test    run standardised tests on the compiled binary'
	@echo
	@echo 'Developer targets:'
	@echo
	@echo '  make            rebuild the Makefile (after adding new files)'
	@echo '  dep / depend    rebuild .d (dependency) files'
	@echo '  clean           remove .o (object) and .c~ (backup) files'
	@echo '  depclean        remove .d (dependency) files'
	@echo '  indentclean     remove files left over from "make indent"'
	@echo '  distclean       remove everything not distributed'
	@echo '  cvsclean        remove everything not in CVS'
	@echo
	@echo '  index           generate an HTML index of source code'
	@echo '  manhtml         output HTML man page to stdout'
	@echo '  indent          reformat all source files with "indent"'
	@echo '  changelog       generate doc/changelog from CVS log info'
	@echo
	@echo '  memtest         run "make test" using valgrind to find faults'
	@echo '  rats            run "rats" to find potential faults'
	@echo '  neat            do indent indentclean all test memtest rats'
	@echo
	@echo '  dist            create a source tarball for distribution'
	@echo '  rpm             build a binary RPM (passes $$RPMFLAGS to RPM)'
	@echo '  srpm            build a source RPM (passes $$RPMFLAGS to RPM)'
	@echo '  deb             build a binary Debian package'
	@echo '  release         dist+rpm+srpm'
	@echo

make:
	echo > $(srcdir)/autoconf/make/filelist.mk
	echo > $(srcdir)/autoconf/make/modules.mk
	cd $(srcdir); \
	bash autoconf/scripts/makemake.sh \
	     autoconf/make/filelist.mk \
	     autoconf/make/modules.mk
	sh ./config.status
	
dep depend: $(alldep)
	echo '#' > $(srcdir)/autoconf/make/depend.mk
	echo '# Dependencies.' >> $(srcdir)/autoconf/make/depend.mk
	echo '#' >> $(srcdir)/autoconf/make/depend.mk
	echo >> $(srcdir)/autoconf/make/depend.mk
	cat $(alldep) >> $(srcdir)/autoconf/make/depend.mk
	sh ./config.status

clean:
	rm -f $(allobj)
	find . -type f -name "*.c~" -exec rm -f '{}' ';'
	rm -f memtest-out rats-out

depclean:
	rm -f $(alldep)

indentclean:
	cd $(srcdir) && for FILE in $(allsrc); do rm -fv ./$${FILE}~; done

distclean: clean depclean
	rm -f $(alltarg) src/include/config.h
	rm -rf $(package)-$(version).tar* $(package)-$(version) BUILD-DEB
	rm -f *.rpm *.deb
	rm -f *.html config.*
	rm -f trace gmon.out
	rm -f Makefile

cvsclean: distclean
	rm -f doc/lsm
	rm -f doc/$(package).spec
	rm -f doc/quickref.1
	rm -f doc/quickref.txt
	rm -f configure
	rm -f doc/changelog ChangeLog
	echo -n > $(srcdir)/autoconf/make/depend.mk
	echo -n > $(srcdir)/autoconf/make/filelist.mk
	echo -n > $(srcdir)/autoconf/make/modules.mk

doc: doc/quickref.txt

index:
	(cd $(srcdir); sh autoconf/scripts/index.sh $(srcdir)) > index.html

manhtml:
	@man2html ./doc/quickref.1 \
	| sed -e '1,/<BODY/d' -e '/<\/BODY/,$$d' \
	      -e 's|<A [^>]*>&nbsp;</A>||ig' \
	      -e 's|<A [^>]*>\([^<]*\)</A>|\1|ig' \
	      -e '/<H1/d' -e 's|\(</H[0-9]>\)|\1<P>|ig' \
	      -e 's/<DL COMPACT>/<DL>/ig' \
	      -e 's/&lt;[0-9A-Za-z_.-]\+@[0-9A-Za-z_.-]\+&gt;//g' \
	      -e 's|<I>\(http://.*\)</I>|<A HREF="\1">\1</A>|ig' \
	| sed -e '1,/<HR/d' -e '/<H2>Index/,/<HR/d' \

indent:
	cd $(srcdir) && indent -npro -kr -i8 -cd42 -c45 $(allsrc)

changelog:
	@which cvs2cl >/dev/null 2>&1 || ( \
	  echo '*** Please put cvs2cl in your PATH'; \
	  echo '*** Get it from http://www.red-bean.com/cvs2cl/'; \
	  exit 1; \
	)
	rm -f $(srcdir)/ChangeLog
	cd $(srcdir) && cvs2cl -S -P
	mv -f $(srcdir)/ChangeLog doc/changelog

dist: doc
	test -d $(srcdir)/CVS && $(MAKE) changelog || :
	rm -rf $(package)-$(version)
	mkdir $(package)-$(version)
	cp -dprf Makefile $(distfiles) $(package)-$(version)
	cd $(package)-$(version); $(MAKE) distclean
	-cp -dpf doc/changelog      $(package)-$(version)/doc/
	cp -dpf doc/lsm             $(package)-$(version)/doc/
	cp -dpf doc/$(package).spec $(package)-$(version)/doc/
	cp -dpf doc/quickref.txt    $(package)-$(version)/doc/
	chmod 644 `find $(package)-$(version) -type f -print`
	chmod 755 `find $(package)-$(version) -type d -print`
	chmod 755 `find $(package)-$(version)/autoconf/scripts`
	chmod 755 $(package)-$(version)/configure
	chmod 755 $(package)-$(version)/debian/rules
	rm -rf DUMMY `find $(package)-$(version) -type d -name CVS`
	tar cf $(package)-$(version).tar $(package)-$(version)
	rm -rf $(package)-$(version)
	-cat $(package)-$(version).tar \
	 | bzip2 > $(package)-$(version).tar.bz2 \
	 || rm -f $(package)-$(version).tar.bz2
	$(DO_GZIP) $(package)-$(version).tar

check test: $(package)-test
	./$(package)-test

memtest: $(package)-test
	@which valgrind >/dev/null 2>/dev/null || (\
	 echo The memory leak / access test requires valgrind to be installed.; \
	 echo See http://valgrind.kde.org/ for details.; \
	 exit 1; \
	)
	valgrind --tool=memcheck --trace-children=yes --track-fds=yes --leak-check=yes --show-reachable=yes --db-attach=no ./$(package)-test 2>memtest-out
	@FAIL=1; \
	 grep '^==[0-9]\+== ERROR SUMMARY: ' memtest-out \
	 | grep -vq 'ERROR SUMMARY: 0' || FAIL=0; \
	 test $$FAIL -eq 0 || (\
	 echo Errors were found with valgrind. Check memtest-out for details.; \
	 exit 1; \
	)
	@echo No errors were found by valgrind.

rats:
	@which rats >/dev/null 2>/dev/null || (\
	 echo The Rough Auditing Tool for Security test requires rats to be installed.; \
	 echo See http://www.securesoftware.com/resources/download_rats.html for details.; \
	 exit 1; \
	)
	@rats --noheader --nofooter --quiet $(srcdir)/src >rats-out 2>rats-out
	@cat rats-out
	@FAIL=1; \
	 test -s rats-out || FAIL=0; \
	 test $$FAIL -eq 0 || (\
	 echo Potential faults were found with rats. See above for details.; \
	 rm -f rats-out; \
	 exit 1; \
	)
	@rm -f rats-out
	@echo No potential errors were found by rats.

neat: indent indentclean all test memtest rats

install: all doc
	$(srcdir)/autoconf/scripts/mkinstalldirs \
	  "$(DESTDIR)/$(sbindir)"
	$(srcdir)/autoconf/scripts/mkinstalldirs \
	  "$(DESTDIR)/$(mandir)/man1"
	$(INSTALL) -m 755 $(package) \
	  "$(DESTDIR)/$(sbindir)/$(package)"
	$(INSTALL) -m 644 doc/quickref.1 \
	  "$(DESTDIR)/$(mandir)/man1/$(package).1"
	$(DO_GZIP) "$(DESTDIR)/$(mandir)/man1/$(package).1"      || :

uninstall:
	$(UNINSTALL) "$(DESTDIR)/$(sbindir)/$(package)"
	$(UNINSTALL) "$(DESTDIR)/$(mandir)/man1/$(package).1"
	$(UNINSTALL) "$(DESTDIR)/$(mandir)/man1/$(package).1.gz"

rpmbuild:
	echo macrofiles: `rpm --showrc \
	  | grep ^macrofiles \
	  | cut -d : -f 2- \
	  | sed 's,^[^/]*/,/,'`:`pwd`/rpmmacros > rpmrc
	echo %_topdir `pwd`/rpm > rpmmacros
	rm -rf rpm
	mkdir rpm
	mkdir rpm/SPECS rpm/BUILD rpm/SOURCES rpm/RPMS rpm/SRPMS
	-cat /usr/lib/rpm/rpmrc /etc/rpmrc $$HOME/.rpmrc \
	 | grep -hsv ^macrofiles \
	 >> rpmrc

srpm:
	-test -e $(package)-$(version).tar.gz || $(MAKE) dist
	-test -e rpmrc || $(MAKE) rpmbuild
	rpmbuild $(RPMFLAGS) --rcfile=rpmrc -ts $(package)-$(version).tar.bz2
	mv rpm/SRPMS/*$(package)-*.rpm .
	rm -rf rpm rpmmacros rpmrc

rpm:
	-test -e $(package)-$(version).tar.gz || $(MAKE) dist
	-test -e rpmrc || $(MAKE) rpmbuild
	rpmbuild $(RPMFLAGS) --rcfile=rpmrc -tb $(package)-$(version).tar.bz2
	rpmbuild $(RPMFLAGS) --rcfile=rpmrc -tb --with static $(package)-$(version).tar.bz2
	mv rpm/RPMS/*/$(package)-*.rpm .
	rm -rf rpm rpmmacros rpmrc

deb: dist
	rm -rf BUILD-DEB
	mkdir BUILD-DEB
	cd BUILD-DEB && tar xzf ../$(package)-$(version).tar.gz
	cd BUILD-DEB && cd $(package)-$(version) && dpkg-buildpackage -rfakeroot
	mv BUILD-DEB/*.deb .
	rm -rf BUILD-DEB

release: dist rpm srpm

