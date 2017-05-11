
OBJS = lua_zlib.o
INSTALL ?= install

.phony : all clean

all : _zlib.so

_zlib.so : $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) -lz

clean :
	@rm -f _zlib.so *.o
