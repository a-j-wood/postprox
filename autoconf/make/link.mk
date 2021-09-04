#
# Targets.
#
#

commonobjs := src/smtp.o src/library.o
mainobjs := src/main.o $(commonobjs)
testobjs := src/test.o $(commonobjs)

$(package): $(mainobjs)
	$(CC) $(CFLAGS) -o $@ $(mainobjs) $(LIBS)

$(package)-static: $(mainobjs)
	$(CC) $(CFLAGS) -static -o $@ $(mainobjs) $(LIBS)

$(package)-test: $(testobjs)
	$(CC) $(CFLAGS) -o $@ $(testobjs) $(LIBS)

# EOF
