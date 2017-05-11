#!/bin/bash

V=5.3
R=$V.4

[ -d lua-$R ] || {
	[ -f lua-$R.tar.gz ] || wget http://www.lua.org/ftp/lua-$R.tar.gz
	tar xzf lua-$R.tar.gz
	cp -f luaconf.h lua-$R/src/
	cp -f Makefile.src lua-$R/src/Makefile
	cp -f Makefile lua-$R/Makefile
}

cd lua-$R && make $@
