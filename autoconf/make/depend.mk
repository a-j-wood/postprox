#
# Dependencies.
#

src/library/fdline.d src/library/fdline.o: src/library/fdline.c src/include/config.h src/include/log.h src/include/library.h 
src/library/getopt.d src/library/getopt.o: src/library/getopt.c src/include/config.h 
src/library/net.d src/library/net.o: src/library/net.c src/include/config.h src/include/library.h 
src/library/writeretry.d src/library/writeretry.o: src/library/writeretry.c src/include/config.h src/include/log.h 
src/main/license.d src/main/license.o: src/main/license.c src/include/config.h 
src/main/help.d src/main/help.o: src/main/help.c src/include/config.h 
src/main/options.d src/main/options.o: src/main/options.c src/include/config.h src/include/options.h 
src/main/log.d src/main/log.o: src/main/log.c src/include/config.h src/include/log.h 
src/main/main.d src/main/main.o: src/main/main.c src/include/config.h src/include/options.h src/include/log.h 
src/main/version.d src/main/version.o: src/main/version.c src/include/config.h 
src/smtp/runfilter.d src/smtp/runfilter.o: src/smtp/runfilter.c src/include/config.h src/include/options.h src/include/log.h 
src/smtp/io.d src/smtp/io.o: src/smtp/io.c src/include/config.h src/include/options.h src/include/log.h src/include/library.h src/smtp/smtpi.h 
src/smtp/line.d src/smtp/line.o: src/smtp/line.c src/include/config.h src/include/options.h src/include/log.h src/include/library.h src/smtp/smtpi.h 
src/smtp/loop.d src/smtp/loop.o: src/smtp/loop.c src/include/config.h src/include/options.h src/include/log.h src/include/library.h src/smtp/smtpi.h 
src/smtp/main.d src/smtp/main.o: src/smtp/main.c src/include/config.h src/include/options.h src/include/log.h src/include/library.h src/smtp/smtpi.h 
src/test/emailinfo.d src/test/emailinfo.o: src/test/emailinfo.c src/include/config.h src/include/options.h src/include/library.h src/test/testi.h 
src/test/dataintegrity.d src/test/dataintegrity.o: src/test/dataintegrity.c src/include/config.h src/include/options.h src/include/library.h src/test/testi.h 
src/test/main.d src/test/main.o: src/test/main.c src/include/config.h src/include/options.h src/include/library.h src/include/log.h src/test/testi.h 
src/test/writeretry.d src/test/writeretry.o: src/test/writeretry.c src/include/config.h src/include/options.h src/include/library.h 
