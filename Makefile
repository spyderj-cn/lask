
.PHONY: all clean install

LIB_DIR=/usr/lib/lua/5.3
TOP_DIR=${CURDIR}

export CFLAGS+="-I${TOP_DIR}/include" -fPIC -std=c99
export LDFLAGS+=-llua -lrt -shared

all:
	make -C contrib/cjson all
	make -C lask/ssl all
	make -C lask/std all
	make -C lask/zlib all

install:
	install -m 644 contrib/cjson/cjson.so ${LIB_DIR}/cjson.so
	install -m 644 lask/ssl/ssl.so ${LIB_DIR}/ssl.so
	install -m 644 lask/zlib/_zlib.so ${LIB_DIR}/_zlib.so
	install -m 644 lask/zlib/zlib.lua ${LIB_DIR}/zlib.lua

	install -m 644 lask/std/_std.so ${LIB_DIR}/_std.so
	install -m 644 lask/luasrc/*.lua ${LIB_DIR}/
	install -d ${LIB_DIR}/tasklet
	install -d ${LIB_DIR}/tasklet/channel
	install -m 644 lask/luasrc/tasklet/*.lua ${LIB_DIR}/tasklet/
	install -m 644 lask/luasrc/tasklet/channel/*.lua ${LIB_DIR}/tasklet/channel/

clean:
	make -C contrib/cjson clean
	make -C lask/ssl clean
	make -C lask/std clean
	make -C lask/zlib clean
