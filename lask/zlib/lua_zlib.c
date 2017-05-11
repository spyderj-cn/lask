
#include <ctype.h>
#include <lauxlib.h>
#include <lua.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "lstd.h"

#define DEF_MEM_LEVEL   		2
#define DEFLATE_CHUNK_SIZE      4096
#define DEFLATE_MAGIC 		    1491686609


typedef struct _Stream {
	unsigned int magic;
	z_stream zstrm;
}Stream;

/*
** zstream, err = zlib.deflate_init([wbits=16, level=-1, memlevel=2])
*/
static int l_deflate_init(lua_State *L)
{
	z_stream stream = {
		.zalloc = Z_NULL,
		.zfree = Z_NULL,
		.opaque = Z_NULL,
	};
	int wbits = (int)luaL_optinteger(L, 1, MAX_WBITS + 16);
	int level = (int)luaL_optinteger(L, 2, Z_DEFAULT_COMPRESSION);
	int memlevel = (int)luaL_optinteger(L, 3, DEF_MEM_LEVEL);
	int result = deflateInit2(&stream, level, Z_DEFLATED, wbits, memlevel, Z_DEFAULT_STRATEGY);
	if (result == Z_OK) {
		Stream *p = (Stream*)malloc(sizeof(z_stream));
		p->magic = DEFLATE_MAGIC;
		memcpy(&p->zstrm, &stream, sizeof(stream));
		lua_pushlightuserdata(L, p);
		lua_pushinteger(L, 0);
	} else {
		lua_pushnil(L);
		lua_pushinteger(L, result);
	}
	return 2;
}

/*
** err = zlib.deflate(zstream, buffer_or_reader, outbuf, flush)
**
** when buffer_or
*/
static int l_deflate(lua_State *L)
{
	Stream *strm;
	union {
		const Buffer *buf;
		const Reader *rd;
	}ptr;
	Buffer *outbuf;
	int flush;

	int result = Z_OK;
	const uint8_t *in;
	size_t in_all;
	uint8_t *out;
	size_t out_all;

	/* XXX: something goes wrong if size grow bigger after deflation, so we
	make the output buffer 20% bigger. */
	uint8_t mem[(int)(DEFLATE_CHUNK_SIZE * 1.2)];

	/******************* collect the arguments  *******************/

	strm = (Stream*)lua_touserdata(L, 1);
	if (strm == NULL || strm->magic != DEFLATE_MAGIC)
		luaL_error(L, "expected zstream for argument #1");

	ptr.buf = (const Buffer*)lua_touserdata(L, 2);
	if (ptr.buf != NULL) {
		if (ptr.buf->magic == BUFFER_MAGIC) {
			in = ptr.buf->data;
			in_all = ptr.buf->datasiz;
		} else if (ptr.rd->magic == READER_MAGIC) {
			in = ptr.rd->data;
			in_all = ptr.rd->datasiz;
		} else {
			ptr.buf = NULL;
		}
	}
	if (ptr.buf == NULL)
		luaL_error(L, "expected buffer/reader for argument #2");

	outbuf = (Buffer*)lua_touserdata(L, 3);
	if (outbuf == NULL || outbuf->magic != BUFFER_MAGIC)
		luaL_error(L, "argument #3 should be buffer if provided");

	flush = (int)luaL_optinteger(L, 4, 1);
	flush = (flush == 1 && in_all > 0) ? Z_SYNC_FLUSH : Z_FINISH;


	/******************* do the deflation *******************/

	while (in_all > 0 || flush == Z_FINISH) {
		size_t in_num = in_all > DEFLATE_CHUNK_SIZE ? DEFLATE_CHUNK_SIZE : in_all;
		size_t out_num;

		strm->zstrm.next_in = (uint8_t*)in;
		strm->zstrm.avail_in = in_num;
		strm->zstrm.next_out = mem;
		strm->zstrm.avail_out = sizeof(mem);

		result = deflate(&strm->zstrm, in_all > DEFLATE_CHUNK_SIZE ? Z_SYNC_FLUSH : flush);
		if (in_all > 0) {
			in_all -= (in_num - strm->zstrm.avail_in);
			in += (in_num - strm->zstrm.avail_in);
		}

		out_num = sizeof(mem) - strm->zstrm.avail_out;
		memcpy(buffer_grow(outbuf, out_num), mem, out_num);

		if (result != Z_OK)
			break;

		if (flush == Z_FINISH && in_all == 0)
			break;
	}

	if (flush == Z_FINISH && result == Z_STREAM_END)
		result = Z_OK;

	lua_pushinteger(L, result);
	return 1;
}

/*
** zlib.deflate_end(zstream)
*/
static int l_deflate_end(lua_State *L)
{
	Stream *p =(Stream*)lua_touserdata(L, 1);
	if (p == NULL || p->magic != DEFLATE_MAGIC)
		luaL_error(L, "expected zstream for argument #1");
	deflateEnd(&p->zstrm);
	free(p);
	return 0;
}

#define INFLATE_CHUNK_SIZE      4096
#define INFLATE_MAGIC 		    1491686610

/*
** zstream, err = zlib.inflate_init([wbits])
*/
static int l_inflate_init(lua_State *L)
{
	z_stream stream = {
		.zalloc = Z_NULL,
		.zfree = Z_NULL,
		.opaque = Z_NULL,
		.avail_in = 0,
		.next_in = Z_NULL,
	};
	int wbits = (int)luaL_optinteger(L, 1, MAX_WBITS + 16);
	int result = inflateInit2(&stream, wbits);
	if (result == Z_OK) {
		Stream *p = (Stream*)malloc(sizeof(z_stream));
		p->magic = INFLATE_MAGIC;
		memcpy(&p->zstrm, &stream, sizeof(stream));
		lua_pushlightuserdata(L, p);
		lua_pushinteger(L, 0);
	} else {
		lua_pushnil(L);
		lua_pushinteger(L, result);
	}
	return 2;
}


/*
** err = zlib.inflate(zstream, buffer_or_reader, outbuf[, flush=1])
*/
static int l_inflate(lua_State *L)
{
	Stream *strm;
	union {
		const Buffer *buf;
		const Reader *rd;
	}ptr;
	Buffer *outbuf;
	int flush;

	int wbits;
	int result = Z_OK;
	const uint8_t *in;
	uint8_t *out;
	size_t in_all;
	size_t in_done = 0;
	size_t offset, old_lgap, old_datasiz;

	/********************** collect the arguments  ******************/

	strm = (Stream*)lua_touserdata(L, 1);
	if (strm == NULL || strm->magic != INFLATE_MAGIC)
		luaL_error(L, "expected zstream for argument #1");

	ptr.buf = (const Buffer*)lua_touserdata(L, 2);
	if (ptr.buf != NULL) {
		if (ptr.buf->magic == BUFFER_MAGIC) {
			in = ptr.buf->data;
			in_all = ptr.buf->datasiz;
		} else if (ptr.rd->magic == READER_MAGIC) {
			in = ptr.rd->data;
			in_all = ptr.rd->datasiz;
		} else {
			ptr.buf = NULL;
		}
	}
	if (ptr.buf == NULL)
		luaL_error(L, "expected buffer/reader for argument #2");

	if (in_all == 0) {
		lua_pushinteger(L, 0);
		return 1;
	}

	outbuf = (Buffer*)lua_touserdata(L, 3);
	if (outbuf == NULL || outbuf->magic != BUFFER_MAGIC)
		luaL_error(L, "expected buffer for argument #3");

	flush = (int)luaL_optinteger(L, 4, 1);
	flush = flush == 1 ? Z_SYNC_FLUSH : Z_FINISH;

	/********************** do the inflation ******************/

	old_lgap = outbuf->data - outbuf->mem;
	old_datasiz = outbuf->datasiz;
	out = outbuf->data + outbuf->datasiz;
	offset = out - outbuf->mem;

	while (in_done < in_all) {
		size_t in_num = (in_all - in_done) > INFLATE_CHUNK_SIZE ? INFLATE_CHUNK_SIZE : (in_all - in_done);
		size_t out_num = outbuf->memsiz - offset;

		if (out_num < in_num * 4) {
			buffer_grow(outbuf, in_num * 4 - out_num);
			out = outbuf->mem + offset;
			out_num = outbuf->memsiz - offset;
		}

		strm->zstrm.next_in = (uint8_t*)in + in_done;
		strm->zstrm.avail_in = in_num;
		strm->zstrm.next_out = out;
		strm->zstrm.avail_out = out_num;

		result = inflate(&strm->zstrm, (in_all - in_done) > INFLATE_CHUNK_SIZE ? Z_NO_FLUSH : flush);
		if (result == Z_NEED_DICT || result == Z_MEM_ERROR || result == Z_DATA_ERROR)
			break;

		in_done += (in_num - strm->zstrm.avail_in);
		out += out_num - strm->zstrm.avail_out;
		offset = out - outbuf->mem;

		if (result == Z_STREAM_END)
			break;
	}

	if (in_all == in_done || result == Z_STREAM_END) {
		lua_pushinteger(L, 0);
		outbuf->datasiz = out - outbuf->mem - old_lgap;
	} else {
		outbuf->datasiz = old_datasiz;
		lua_pushinteger(L, result);
	}

	return 1;
}

/*
** zlib.inflate_end(zstream)
*/
static int l_inflate_end(lua_State *L)
{
	Stream *p =(Stream*)lua_touserdata(L, 1);
	if (p == NULL || p->magic != INFLATE_MAGIC)
		luaL_error(L, "expected zstream for argument #1");
	inflateEnd(&p->zstrm);
	free(p);
	return 0;
}

static const luaL_Reg funcs[] = {
	{"deflate_init", l_deflate_init},
	{"deflate", l_deflate},
	{"deflate_end", l_deflate_end},
	{"inflate_init", l_inflate_init},
	{"inflate", l_inflate},
	{"inflate_end", l_inflate_end},
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

int luaopen__zlib(lua_State *L)
{
	lua_newtable(L);
	luaL_setfuncs(L, funcs, 0);
	l_register_enums(L, enums);

	lua_pushstring(L, "MAX_WBITS");
	lua_pushinteger(L, MAX_WBITS);
	lua_settable(L, -3);

	buffer_initcfunc(L);

	return 1;
}
