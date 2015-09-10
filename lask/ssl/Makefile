
OBJS = ssl.o context.o

.phony : all

all : ssl.so

ssl.so : $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) -lssl -lcrypto
	
clean : 
	@rm -f ssl.so *.o

ssl.o: ssl.c consts.h common.h
context.o: context.c common.h
