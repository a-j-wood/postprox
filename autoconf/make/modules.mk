# Automatically generated module linking rules
#
# Creation time: Mon Mar  3 08:52:08 GMT 2008

src/main.o:  src/main/help.o src/main/license.o src/main/log.o src/main/main.o src/main/options.o src/main/version.o
	$(LD) $(LDFLAGS) -o $@  src/main/help.o src/main/license.o src/main/log.o src/main/main.o src/main/options.o src/main/version.o

src/test.o:  src/test/dataintegrity.o src/test/emailinfo.o src/test/main.o src/test/writeretry.o
	$(LD) $(LDFLAGS) -o $@  src/test/dataintegrity.o src/test/emailinfo.o src/test/main.o src/test/writeretry.o

src/library.o:  src/library/fdline.o src/library/getopt.o src/library/net.o src/library/writeretry.o
	$(LD) $(LDFLAGS) -o $@  src/library/fdline.o src/library/getopt.o src/library/net.o src/library/writeretry.o

src/smtp.o:  src/smtp/io.o src/smtp/line.o src/smtp/loop.o src/smtp/main.o src/smtp/runfilter.o
	$(LD) $(LDFLAGS) -o $@  src/smtp/io.o src/smtp/line.o src/smtp/loop.o src/smtp/main.o src/smtp/runfilter.o


