
OBJS = lbitlib.o lbuffer.o lerrno.o lfs.o lmath.o los.o lreader.o lpoll.o lprctl.o liface.o lmd5.o\
	lsignal.o lsocket.o lstat.o lstd.o lstring.o ltable.o ltime.o lsys.o lnetdb.o lcodec.o lfcntl.o
INSTALL ?= install

.phony : all clean

all : _std.so

_std.so : $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

clean :
	@rm -f _std.so *.o

$(OBJS):lstdimpl.h
