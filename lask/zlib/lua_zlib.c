
/*
 * Copyright (C) Spyderj
 */

#include <ctype.h>
#include <lauxlib.h>
#include <lua.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "lstd.h"

#define DEF_MEM_LEVEL 2  

/*
** err = zlib.compress(buf, wbits=MAX_WBITS+16)
*/
static int l_compress(lua_State *L)
{
	z_stream stream;
	int wbits;
	int result = Z_OK;
	Buffer *buf;
	uint8_t *in;
	size_t in_all;
	uint8_t *deflated_ptr;
	size_t deflated_all = 0;
	uint8_t mem[1024];
	
	buf = (Buffer*)lua_touserdata(L, 1);
	if (buf == NULL || buf->magic != BUFFER_MAGIC)
		luaL_error(L, "expected buffer for argument #1");
		
	wbits = luaL_optint(L, 2, MAX_WBITS + 16);
	in = buf->data;
	in_all = buf->datasiz;
	deflated_ptr = buf->data;	
	if (in_all == 0) {
		lua_pushinteger(L, 0);
		return 1;
	}
	
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	result = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, wbits, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY);
	if (result != Z_OK) {
		lua_pushinteger(L, result);
		return 1;
	}
	
	while (in_all > 0 || result == Z_OK) {
		size_t in_num = in_all > 1024 ? 1024 : in_all;
		
		stream.next_in = in;
		stream.avail_in = in_num;
		stream.next_out = mem;
		stream.avail_out = sizeof(mem);
		
		result = deflate(&stream, in_all > 1024 ? Z_SYNC_FLUSH : Z_FINISH);
		if (result != Z_OK && result != Z_STREAM_END) {
			break;
		} else {
			size_t out_num = sizeof(mem) - stream.avail_out;
			deflated_all += out_num;
			memcpy(deflated_ptr, mem, out_num);
			deflated_ptr += out_num;
		}
		
		if (in_all > 0) {
			in_all -= (in_num - stream.avail_in);
			in += (in_num - stream.avail_in);
		}
	}
	
	deflateEnd(&stream);
	if (in_all == 0) {
		buffer_pop(buf, buf->datasiz - deflated_all);
		lua_pushinteger(L, 0);
	} else {
		lua_pushinteger(L, result);
	}
	return 1;
}

/*
** err = zlib.uncompress(buf, wbits=MAX_WBITS+16)
*/
static int l_uncompress(lua_State *L)
{
	z_stream stream;
	int wbits;
	int result = Z_OK;
	Buffer *buf;
	uint8_t *in;
	size_t in_all;
	size_t in_done = 0;
	uint8_t mem[4096];
	
	buf = (Buffer*)lua_touserdata(L, 1);
	if (buf == NULL || buf->magic != BUFFER_MAGIC)
		luaL_error(L, "expected buffer for argument #1");

	wbits = luaL_optint(L, 2, MAX_WBITS + 16);
	in_all = buf->datasiz;
	if (in_all == 0) {
		lua_pushinteger(L, 0);
		return 1;
	}
	
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.next_in = Z_NULL;
	stream.avail_in = 0;
	result = inflateInit2(&stream, wbits);
	if (result != Z_OK) {
		lua_pushinteger(L, result);
		return 1;
	}
	
	in = (uint8_t*)malloc(in_all);
	memcpy(in, buf->data, in_all);
	buffer_rewind(buf);
	
	while (in_done < in_all || result == Z_OK) {
		size_t in_num = (in_all - in_done) > 1024 ? 1024 : (in_all - in_done);
		
		stream.next_in = in + in_done;
		stream.avail_in = in_num;
		stream.next_out = mem;
		stream.avail_out = sizeof(mem);
		
		result = inflate(&stream, (in_all - in_done) > 1024 ? Z_NO_FLUSH : Z_FINISH);
		if (result != Z_OK && result != Z_STREAM_END) {
			break;
		} else {
			buffer_push(buf, mem, sizeof(mem) - stream.avail_out);
		}
		
		in_done += (in_num - stream.avail_in);
	}
	
	inflateEnd(&stream);
	if (in_all == in_done) {
		lua_pushinteger(L, 0);
	} else {
		buffer_reset(buf);
		buffer_push(buf, in, in_all);
		lua_pushinteger(L, result);
	}
	free(in);
	
	return 1;
}

static const luaL_Reg funcs[] = {
	{"compress", l_compress},
	{"uncompress", l_uncompress},
	{NULL, NULL}
};

typedef struct {
	const char *name;
	int value;
}EnumReg;
#define ENUM(x)		{#x, Z_##x}

static const EnumReg enums[] = {
	ENUM(NEED_DICT),
	ENUM(STREAM_ERROR),
	ENUM(DATA_ERROR),
	ENUM(MEM_ERROR),
	ENUM(BUF_ERROR),
	ENUM(VERSION_ERROR),
	{NULL, 0},
};

static void l_register_enums(lua_State *L, const EnumReg *enums) 
{
	while (enums->name != NULL) {
		lua_pushstring(L, enums->name);
		lua_pushinteger(L, enums->value);
		lua_settable(L, -3);
		enums++;
	}
}

BufferCFunc *buf_cfunc = NULL;

int luaopen_zlib(lua_State *L)
{
	lua_newtable(L);
	luaL_register(L, NULL, funcs);
	l_register_enums(L, enums);
	
	lua_pushstring(L, "MAX_WBITS");
	lua_pushinteger(L, MAX_WBITS);
	lua_settable(L, -3);
	
	buffer_initcfunc(L);
	
	return 1;
}

