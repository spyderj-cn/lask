
OBJS = lua_zlib.o
INSTALL ?= install

.phony : all clean

all : zlib.so

zlib.so : $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) -lz
	
clean : 
	@rm -f zlib.so *.o

