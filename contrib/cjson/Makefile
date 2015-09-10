.PHONY: all clean

objects=lua_cjson.o strbuf.o

all: cjson.so

cjson.so: $(objects) 
	$(CC) $(LDFLAGS) -o $@ $^

$(objects): %.o: %.c
	$(CC) -c $< $(CFLAGS) -DVERSION=\"1.0.3\" -o $@

clean:
	rm -f *.o *.so
