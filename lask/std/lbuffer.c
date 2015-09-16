
/*
 * Copyright (C) Spyderj
 */


#include "lstdimpl.h"
#include <unistd.h>

static size_t pagesize = 0;

#define BUFFER_MIN_SIZE             pagesize

#define WRITER_MAGIC 				0x79884312

typedef struct _Writer {
	uint32 magic;
	Buffer *buffer;
	uint32 offset;
	uint32 length;
	uint32 cursor;
}Writer;

#define WRITER_META 				"meta(writer)"
#define BUFFER_READERS 				"buffer_readers"
#define BUFFER_WRITERS				"buffer_writers"

/******************************************************************************
	buffer
******************************************************************************/

void buffer_init(Buffer *buf, size_t minsiz)
{
	if (minsiz == 0) {
		if (pagesize == 0)
			pagesize = sysconf(_SC_PAGESIZE);
		minsiz = pagesize;
	}
		
	buf->mem = buf->data = NULL;
	buf->minsiz = minsiz;
	buf->memsiz = 0;
	buf->datasiz = 0;
	buf->be = true;
	buf->rd = false;
	buf->wr = false;
}

uint8* buffer_grow(Buffer *buf, size_t growth)
{
	size_t lgap = buf->data - buf->mem;
	size_t rgap = buf->memsiz - buf->datasiz - lgap;
	uint8 *p;
	
	if (rgap < growth) {
		if ((lgap + rgap) >= growth) {
			memmove(buf->mem, buf->data, buf->datasiz);
			buf->data = buf->mem;
		} else {
			size_t reqsiz = growth + buf->datasiz;
			size_t newsiz = buf->memsiz * 2;
			
			if (newsiz == 0)
				newsiz = buf->minsiz;
				
			while (newsiz < reqsiz) 
				newsiz *= 2;
				
			if (lgap == 0) {
				p = REALLOC(buf->mem, newsiz);
			} else {
				p = (uint8*)MALLOC(newsiz);
				memcpy(p, buf->data, buf->datasiz);
				FREE(buf->mem);
			}
			buf->data = buf->mem = p;
			buf->memsiz = newsiz;
		}
	}
	p = buf->data + buf->datasiz;
	buf->datasiz += growth;
	return p;
}

size_t buffer_push(Buffer *buf, const void *mem, size_t memsiz)
{
	uint8 *p = buffer_grow(buf, memsiz);
	if (p != NULL) {
		memcpy(p, mem, memsiz);
		return memsiz;
	} else {
		return 0;
	}
}

static void buffer_adjust(Buffer *buf)
{
	size_t datasiz = buf->datasiz;
	if (datasiz == 0) {
		buf->data = buf->mem;
	} else {
		size_t lgap = buf->data - buf->mem;	
		size_t memsiz = buf->memsiz;
		size_t rgap = memsiz - lgap - datasiz;
		
		/* data area is relatively small */
		bool relsmall = (datasiz < (lgap / 4) && datasiz < (rgap / 4));
		
		/* data area is absolutely small */
		bool abssmall = (datasiz < memsiz / 100 || datasiz < 64);
		
		/* with a big hole at the head and the data area is close to the tail */
		bool close2end = (lgap * 4 > memsiz * 3 && datasiz > rgap);

		if (lgap != 0) {
			if (relsmall || abssmall || close2end) {
				memmove(buf->mem, buf->data, buf->datasiz);
				buf->data = buf->mem;
			}
		}
	}
}

void buffer_pop(Buffer *buf, size_t siz)
{
	if (siz > buf->datasiz)
		siz = buf->datasiz;
		
	buf->datasiz -= siz;
	buffer_adjust(buf);
}

void buffer_shift(Buffer *buf, size_t siz)
{
	if (siz > buf->datasiz)
		siz = buf->datasiz;
		
	buf->datasiz -= siz;
	buf->data += siz;
	buffer_adjust(buf);
}

void buffer_rewind(Buffer *buf)
{
	buf->data = buf->mem;
	buf->datasiz = 0;
}

void buffer_reset(Buffer *buf)
{
	if (buf->memsiz > buf->minsiz) {
		buf->mem = REALLOC(buf->mem, buf->minsiz);
		buf->memsiz = buf->minsiz;
	}
	buf->data = buf->mem;
	buf->datasiz = 0;
}

void buffer_finalize(Buffer *buf)
{
	if (buf->mem) 
		(void)FREE(buf->mem);
	
	buf->mem = buf->data = NULL;
	buf->memsiz = buf->datasiz = 0;
}

uint8* buffer_safegrow(Buffer *buffer, size_t growth, lua_State *L) 
{
    uint8 *p = buffer_grow(buffer, growth);
    if (p == NULL) {
        luaL_error(L, "buffer overflow (requiring %dB)", growth + buffer->datasiz);
    }
    return p;
}

Buffer* buffer_lcheck(lua_State *L, int idx)
{
	Buffer *buffer = lua_touserdata(L, idx);
	if (buffer == NULL || buffer->magic != BUFFER_MAGIC)
		luaL_error(L, "expecting buffer(userdata) for argument %d", idx);
	return buffer;
}

/******************************************************************************
	lua-buffer
******************************************************************************/

/*
** buf = buffer.new(minsiz=pagesize)
*/
static int lbuffer_new(lua_State *L) 
{
	Buffer *buffer;
	size_t minsiz = (size_t)luaL_optinteger(L, 1, 0);
	
	buffer = (Buffer*)lua_newuserdata(L, sizeof(Buffer));
    l_setmetatable(L, -1, BUFFER_META);
    buffer_init(buffer, minsiz);
	buffer->magic = BUFFER_MAGIC;
	
	return 1;
}

/*
** buffer:__gc
*/
static int lbuffer_gc(lua_State *L) 
{
	Buffer *buffer = buffer_lcheck(L, 1);
	
	if (buffer->rd || buffer->wr) {
		lua_pushlightuserdata(L, buffer);
		
		if (buffer->rd) {
			lua_getfield(L, LUA_REGISTRYINDEX, BUFFER_READERS);
			lua_pushvalue(L, -2);
			lua_pushnil(L);
			lua_rawset(L, -3);
			lua_pop(L, 1);
		}
		
		if (buffer->wr) {
			lua_getfield(L, LUA_REGISTRYINDEX, BUFFER_WRITERS);
			lua_pushvalue(L, -2);
			lua_pushnil(L);
			lua_rawset(L, -3);
			lua_pop(L, 1);
		}
		
		lua_pop(L, 1);
	}
	
	buffer_finalize(buffer);
	return 0;
}

/*
** buffer:__tostring
*/
static int lbuffer_tostring(lua_State *L) 
{
	Buffer *buffer = buffer_lcheck(L, 1);
	char buf[LINE_MAX];
	snprintf(buf, sizeof(buf), "buffer (0x%08x, memsiz=%u, datasiz=%u, %s-endian)",
			(unsigned int)buffer,
			(unsigned int)buffer->memsiz, 
			(unsigned int)buffer->datasiz, 
			buffer->be ? "big" : "little");
	lua_pushstring(L, buf);
	return 1;
}

/*
** __len
*/
static int lbuffer_len(lua_State *L) 
{
	Buffer *buffer = buffer_lcheck(L, 1);
	lua_pushinteger(L, buffer->datasiz);
	return 1;
}

/*
** str = buffer:str()
**
** convert the whole content of the buffer into a string.
**
** return an empty string in corresponding to an empty buffer.
*/
static int lbuffer_str(lua_State *L)
{
	Buffer *buffer = buffer_lcheck(L, 1);
	lua_pushlstring(L, (const char*)buffer->data, buffer->datasiz);
	return 1;
}

/*
** rd = buffer:reader(rd = nil, offset=0, length=all)
*/
static int lbuffer_reader(lua_State *L)
{
	int top = lua_gettop(L);
	if (top > 0) {
		Buffer *buffer = buffer_lcheck(L, 1);
		Reader *reader = NULL;
		size_t offset = 0;
		size_t length = buffer->datasiz;
		
		if (top >= 2 && lua_type(L, 2) == LUA_TUSERDATA) {
			reader = (Reader*)luaL_checkudata(L, 2, READER_META);
			lua_pushvalue(L, 2);
		} 
		
		if (reader == NULL) {
			lua_getfield(L, LUA_REGISTRYINDEX, BUFFER_READERS);
			lua_pushlightuserdata(L, buffer);
			lua_gettable(L, -2);
			
			if (lua_isnil(L, -1)) {
				lua_pop(L, 1);
				reader = lua_newuserdata(L, sizeof(Reader)); /* [readers, ud] */
				l_setmetatable(L, -1, READER_META);
				lua_pushlightuserdata(L, buffer);	/* [readers, rd, buf] */
				lua_pushvalue(L, -2);	/* [readers, rd, buf, rd] */
				lua_rawset(L, -4);
				buffer->rd = true;
			} else {
				reader = (Reader*)lua_touserdata(L, -1);
			}
		}
		
		if (top >= 3) {
			offset = (size_t)luaL_checkinteger(L, 3);
			if (offset >= buffer->datasiz)
				offset = 0;
		}
		
		if (top >= 4) {
			length = (size_t)luaL_checkinteger(L, 4);
		}
		
		if (offset + length > buffer->datasiz)
			length = buffer->datasiz - offset;
		
		reader_init(reader, buffer->data + offset, length);
	} else {
		Reader *reader = lua_newuserdata(L, sizeof(Reader)); /* [readers, ud] */
		l_setmetatable(L, -1, READER_META);
		reader_init(reader, NULL, 0);
	}
	return 1;
}

/*
** wr = buffer:writer(length, wr=nil)
*/
static int lbuffer_writer(lua_State *L)
{
	Buffer *buffer = buffer_lcheck(L, 1);
	Writer *wr = NULL;
	size_t length = (size_t)luaL_checkint(L, 2);
	int top = lua_gettop(L);
	
	if (top >= 3 && lua_type(L, 3) == LUA_TUSERDATA) {
		wr = (Writer*)luaL_checkudata(L, 3, WRITER_META);
		lua_pushvalue(L, 3);
	} 
	if (wr == NULL) {
		lua_getfield(L, LUA_REGISTRYINDEX, BUFFER_WRITERS);
		lua_pushlightuserdata(L, buffer);
		lua_gettable(L, -2);
		
		if (lua_isnil(L, -1)) {
			lua_pop(L, 1);
			wr = lua_newuserdata(L, sizeof(Writer));
			l_setmetatable(L, -1, WRITER_META);
			lua_pushlightuserdata(L, buffer);
			lua_pushvalue(L, -2);
			lua_rawset(L, -4);
			buffer->wr = true;
		} else {
			wr = (Writer*)lua_touserdata(L, -1);
		}
	}
	wr->magic = WRITER_MAGIC;
	wr->buffer = buffer;
	wr->offset = buffer->datasiz;
	wr->length = length;
	wr->cursor = 0;
	buffer_safegrow(buffer, length, L);
	return 1;
}

/*
** self = buffer:setbe(true/false)
** 
** 'be' here stands for Big-Endian
** set buffer's bytes-packing style to bit-endian(true) or little-endian(false)
** 
** it affects the behaviour of putw/putu/putlist methods
** 
** defaulted to big-endian(network byte order)
**
** return the buffer itself so that invoking can be chained up.
*/
static int lbuffer_setbe(lua_State *L)
{
	Buffer *buffer = buffer_lcheck(L, 1);
	buffer->be = true;
	
	if (lua_gettop(L) > 1) 
		buffer->be = (bool)lua_toboolean(L, 2);
		
	lua_pushvalue(L, 1);
	return 1;
}

/*
** opaque_value = buffer:beginlen()
*/
static int lbuffer_beginlen(lua_State *L)
{
	Buffer *buffer = buffer_lcheck(L, 1);
	buffer_grow(buffer, 4);
	lua_pushinteger(L, buffer->datasiz);
	return 1;
}

/*
** self = buffer:endlen(opaque_value)
*/
static int lbuffer_endlen(lua_State *L)
{
	Buffer *buffer = buffer_lcheck(L, 1);
	size_t oldlen = (size_t)luaL_checkint(L, 2);
	
	if (oldlen >= 4 && oldlen <= buffer->datasiz) {
		uint8 *p = buffer->data + oldlen - 4;
		if (buffer->be)
			uint32_to_bytes_be(buffer->datasiz - oldlen, p);
		else
			uint32_to_bytes_le(buffer->datasiz - oldlen, p);
	}
	
	lua_pushvalue(L, 1);
	return 1;
}

/*
** line1, ... = buffer:getline(num)
** 
** content will be shifted
*/
static int lbuffer_getline(lua_State *L)
{
	Buffer *buffer = buffer_lcheck(L, 1);
	size_t num = (size_t)luaL_optint(L, 2, 1);
	Reader rd;

	reader_init(&rd, buffer->data, buffer->datasiz);

	for (size_t i = 0; i < num ; i++) {
		size_t len;
		const char *str = reader_getline(&rd, &len);
		if (str != NULL)
			lua_pushlstring(L, str, len);
		else
			lua_pushnil(L);
	}	
	
	buffer_shift(buffer, (size_t)(rd.data - rd.mem));
	
	return (int)num;
}

/*
** str = buf:getlstr(len=nil)
**
** read a string with the sppecified length
*/
static int lbuffer_getlstr(lua_State *L)
{
	Buffer *buf = buffer_lcheck(L, 1);
	size_t length = (size_t)luaL_optint(L, 2, -1);
	
	if (length > buf->datasiz)
		length = buf->datasiz;
	
	if (length > 0) {
		lua_pushlstring(L, (const char*)buf->data, length);
		buf->data += length;
		buf->datasiz -= length;
	} else {
		lua_pushnil(L);
	}
		
	return 1;
}

/*
** b1, b2, ... cN = buffer:getb(N = 1)
**
** read n unsigned 8-bits integer 
*/
static int lbuffer_getb(lua_State *L) 
{
	Buffer *buf = buffer_lcheck(L, 1);
	size_t num = (size_t)luaL_optint(L, 2, 1);
	
	if (num > buf->datasiz)
		num = buf->datasiz;
	
	for (size_t i = 0; i < num; i++) {
		lua_pushinteger(L, buf->data[i]);
	}
	buffer_shift(buf, num);
	return (int)num;
}

/*
** w1, w2, ... wN = buffer:getw(N = 1)
** 
** read n unsigned 16-bit integer
*/
static int lbuffer_getw(lua_State *L)
{
	Buffer *buf = buffer_lcheck(L, 1);
	size_t num = (size_t)luaL_optint(L, 2, 1);
	bool be = buf->be;
	const uint8 *p = buf->data;
	
	if (num * 2 > buf->datasiz)
		num = buf->datasiz / 2;
	
	for (size_t i = 0; i < num; i++) {
		lua_pushinteger(L, be ? (int)bytes_to_uint16_be(p) : (int)bytes_to_uint16_le(p));
		p += 2;
	}
	buffer_shift(buf, 2 * num);
	return (int)num;
}

/*
** i1, i2, .. iN = reader:geti(N = 1)
**
** read n integer(signed 32-bit integer)
*/
static int lbuffer_geti(lua_State *L)
{
	Buffer *buf = buffer_lcheck(L, 1);
	size_t num = (size_t)luaL_optint(L, 2, 1);
	bool be = buf->be;
	const uint8 *p = buf->data;
	
	if (num * 4 > buf->datasiz)
		num = buf->datasiz / 4;
	
	for (size_t i = 0; i < num; i++) {
		lua_pushinteger(L, be ? (int)(bytes_to_uint32_be(p)) : (int)(bytes_to_uint32_le(p)));
		p += 4;
	}
	buffer_shift(buf, 4 * num);
	return (int)num;
}

/*
** u1, u2, .. uN = reader:getu(N = 1)
**
** read n unsigned 32-bits integers
*/
static int lbuffer_getu(lua_State *L)
{
	Buffer *buf = buffer_lcheck(L, 1);
	size_t num = (size_t)luaL_optint(L, 2, 1);
	bool be = buf->be;
	const uint8 *p = buf->data;
	
	if (num * 4 > buf->datasiz)
		num = buf->datasiz / 4;
	
	for (size_t i = 0; i < num; i++) {
		lua_pushnumber(L, be ? bytes_to_uint32_be(p) : bytes_to_uint32_le(p));
		p += 4;
	}
	buffer_shift(buf, 4 * num);
	return (int)num;
}

/*
** c, w, i = reader:getlist("cwi")
*/
static int lbuffer_getlist(lua_State *L)
{
	Buffer *buf = buffer_lcheck(L, 1);
	const char *types = luaL_checkstring(L, 2);
	const char *p = types;
	bool empty = false;
	bool be = buf->be;
	
	while (*p != 0 && !empty) {
		char type = *p;
		switch (type) {
		case 'c': {
				if (buf->datasiz < 1)  {
					empty = true;
				} else {
					lua_pushinteger(L, (int)(uint32)buf->data[0]);
					buffer_shift(buf, 1);
				}
				break;
			}
		case 'w': {
				if (buf->datasiz < 2)  {
					empty = true;
				} else {
					const uint8 *p = buf->data;
					lua_pushinteger(L, be ? (int)(bytes_to_uint16_be(p)) : (int)(bytes_to_uint16_le(p)));
					buffer_shift(buf, 2);
				}
				break;
			}
		case 'i': {
				if (buf->datasiz < 4)  {
					empty = true;
				} else {
					const uint8 *p = buf->data;
					lua_pushinteger(L, be ? (int)(bytes_to_uint32_be(p)) : (int)(bytes_to_uint32_le(p)));
					buffer_shift(buf, 4);
				}
				break;
			}
		case 'u': {
				if (buf->datasiz < 4) {
					empty = true;
				} else {
					const uint8 *p = buf->data;
					lua_pushnumber(L, (lua_Number)(be ? bytes_to_uint32_be(p) : bytes_to_uint32_le(p)));
				}
				break;
			}
		default:
			break;
		}
		if (!empty)
			p++;
	}
	return (int)(p - types);
}

/*
** self = buffer:fill(value, length)
*/
static int lbuffer_fill(lua_State *L)
{
	Buffer *buffer = buffer_lcheck(L, 1);
	size_t length = (size_t)luaL_checkint(L, 3);	
	memset(buffer_grow(buffer, length), (uint8)luaL_checkint(L, 2), length);
	lua_pushvalue(L, 1);
	return 1;
}


/*
** self = buffer:putb(b1, b2, ...)
**
** push a serial of bytes(uint8) into the buffer
*/
static int lbuffer_putb(lua_State *L)
{
	Buffer *buffer = buffer_lcheck(L, 1);
	int top = lua_gettop(L);
	uint8 *p = buffer_safegrow(buffer, (size_t)(top - 1), L);
	
	for (int i = 2; i <= top; i++) {
		uint32 val = (uint32)luaL_checkint(L, i);
		*p = (uint8)val;
		p++;
	}
	
	lua_pushvalue(L, 1);
	return 1;
}

/*
** self = buffer:putw(w1, w2, ... wN)
**
** push a serial of words(uint16) into the buffer
*/
static int lbuffer_putw(lua_State *L)
{
	Buffer *buffer = buffer_lcheck(L, 1);
	int top = lua_gettop(L);
	uint8 *p = buffer_safegrow(buffer, (size_t)(top - 1) * 2, L);
	bool be = buffer->be;
	
	for (int i = 2; i <= top; i++) {
		uint32 val = (uint32)luaL_checkint(L, i);
		if (be) {
			uint16_to_bytes_be(val, p);
		} else {
			uint16_to_bytes_le(val, p);
		}
		p += 2;
	}
	
	lua_pushvalue(L, 1);
	return 1;
}

/*
** self = buffer_puti(i1, i2, ... iN)
**
** push a serial of integers into the buffer
*/
static int lbuffer_puti(lua_State *L)
{
	Buffer *buffer = buffer_lcheck(L, 1);
	int top = lua_gettop(L);
	uint8 *p = buffer_safegrow(buffer, (size_t)(top - 1) * 4, L);
	bool be = buffer->be;
	
	for (int i = 2; i <= top; i++) {
		uint32 val = (uint32)luaL_checkint(L, i);
		if (be) {
			uint32_to_bytes_be(val, p);
		} else {
			uint32_to_bytes_le(val, p);
		}
		p += 4;
	}
	
	lua_pushvalue(L, 1);
	return 1;
}

/*
** self = buffer_putu(u1, u2, ... uN)
**
** push a serial of unsigned integers into the buffer
*/
static int lbuffer_putu(lua_State *L)
{
	Buffer *buffer = buffer_lcheck(L, 1);
	int top = lua_gettop(L);
	uint8 *p = buffer_safegrow(buffer, (size_t)(top - 1) * 4, L);
	bool be = buffer->be;
	
	for (int i = 2; i <= top; i++) {
		uint32 val = (uint32)luaL_checknumber(L, i);
		if (be) {
			uint32_to_bytes_be(val, p);
		} else {
			uint32_to_bytes_le(val, p);
		}
		p += 4;
	}
	
	lua_pushvalue(L, 1);
	return 1;
}

/*
** self = buffer:putlist("cwi", c, w, i)
*/
static int lbuffer_putlist(lua_State *L)
{
	Buffer *buffer = buffer_lcheck(L, 1);
	const char *types = luaL_checkstring(L, 2);
	bool be = buffer->be;
	int idx = 3;
	
	while (*types != 0) {
		char type = *types++;
		uint32 value = (uint32)luaL_checknumber(L, idx++);
		switch (type) {
		case 'c': {
				uint8 *p = buffer_safegrow(buffer, 1, L);
				*p = (uint8)value;
				break;
			}
		case 'w': {
				uint8 *p = buffer_safegrow(buffer, 2, L);
				if (be) {
					uint16_to_bytes_be(value, p);
				} else {
					uint16_to_bytes_le(value, p);
				}
				break;
			}
		case 'i': 
		case 'u': {
				uint8 *p = buffer_safegrow(buffer, 4, L);
				if (be) {
					uint32_to_bytes_be(value, p);
				} else {
					uint32_to_bytes_le(value, p);
				}
				break;
			}
		default:
			break;
		}
	}
	
	lua_pushvalue(L, 1);
	return 1;
}

/*
** self = buffer:putstr(str1, str2, ..., strN)
*/
static int lbuffer_putstr(lua_State *L)
{
	Buffer *buffer = buffer_lcheck(L, 1);
	int top = lua_gettop(L);
	
	for (int i = 2; i <= top; i++) {
		size_t len;
		const char *str = luaL_checklstring(L, i, &len);
		buffer_push(buffer, str, len);
	}
	
	lua_pushvalue(L, 1);
	return 1;
}

/*
** self = buffer:putreader(rd, offset=0, length=all)
*/
static int lbuffer_putreader(lua_State *L) 
{
	Buffer *buffer = buffer_lcheck(L, 1);
	Reader *reader = reader_lcheck(L, 2);
	size_t offset = (size_t)luaL_optint(L, 3, 0);
	size_t length = reader->datasiz;
	
	if (lua_gettop(L) >= 4) 
		length = (size_t)luaL_checkint(L, 4);
	
	if (offset >= reader->datasiz)
		length = 0;
	else if ((offset + length) > reader->datasiz)
		length = (reader->datasiz - offset);
		
	if (length > 0)
		buffer_push(buffer, reader->data + offset, length);
		
	lua_pushvalue(L, 1);
	return 1;
}

/*
** self = buffer:pop(nbytes)
**
** discard some bytes from the tail
*/
static int lbuffer_pop(lua_State *L) 
{
	Buffer *buffer = buffer_lcheck(L, 1);
	size_t nbytes = (size_t)luaL_checkint(L, 2);
	
	buffer_pop(buffer, nbytes);
	
	lua_pushvalue(L, 1);
	return 1;
}

/*
** self = buffer:shift(nbytes/reader)
**
** discard nbytes/shifted-bytes-of-reader from the head
*/
static int lbuffer_shift(lua_State *L) 
{
	Buffer *buffer = buffer_lcheck(L, 1);
	int type = lua_type(L, 2);
	
	if (type == LUA_TNUMBER) {
		buffer_shift(buffer, (size_t)luaL_checkint(L, 2));
	} else {
		Reader *rd = reader_lcheck(L, 2);
		buffer_shift(buffer, (size_t)(rd->data - rd->mem));
	}
	
	lua_pushvalue(L, 1);
	return 1;
}

/*
** self = buffer:rewind()
**
** discard all the data in the buffer
**
** allocated memory stays the same
*/
static int lbuffer_rewind(lua_State *L) 
{
	Buffer *buffer = buffer_lcheck(L, 1);
	
	buffer_rewind(buffer);
	
	lua_pushvalue(L, 1);
	return 1;
}

/*
** self = buffer:reset()
**
** discard all the data in the buffer
**
** allocated memory will shrink to the original minimum size 
*/
static int lbuffer_reset(lua_State *L) 
{
	Buffer *buffer = buffer_lcheck(L, 1);
	
	buffer_reset(buffer);
	
	lua_pushvalue(L, 1);
	return 1;
}

static const luaL_Reg buffer_meta_methods[] = {
	{"__tostring", lbuffer_tostring},
    {"__gc", lbuffer_gc},
    {"__len", lbuffer_len},
	{NULL, NULL}
};

static const luaL_Reg buffer_methods[] =  {
	{"new", lbuffer_new},
	{"str", lbuffer_str},
	{"setbe", lbuffer_setbe},
	{"reader", lbuffer_reader},
	{"writer", lbuffer_writer},
	{"beginlen", lbuffer_beginlen},
	{"endlen", lbuffer_endlen},
	{"getlstr", lbuffer_getlstr},
	{"getline", lbuffer_getline},
	{"getc", lbuffer_getb},
	{"getb", lbuffer_getb},
	{"getw", lbuffer_getw},
	{"geti", lbuffer_geti},
	{"getu", lbuffer_getu},
	{"getlist", lbuffer_getlist},
	{"fill", lbuffer_fill},
    {"putc", lbuffer_putb},
	{"putb", lbuffer_putb},
    {"putw", lbuffer_putw},
	{"puti", lbuffer_puti},
	{"putu", lbuffer_putu}, 
	{"putlist", lbuffer_putlist},
    {"putstr", lbuffer_putstr},
	{"putreader", lbuffer_putreader},
    {"pop", lbuffer_pop},
	{"shift", lbuffer_shift},
	{"rewind", lbuffer_rewind},
	{"reset", lbuffer_reset},
    {NULL, NULL}
};

/*
** self = writer:putc(c1, c2, ... cN)
*/
static int lwriter_putc(lua_State *L)
{
	Writer *wr = (Writer*)luaL_checkudata(L, 1, WRITER_META);
	int argc = (size_t)lua_gettop(L) - 1;
	int left;
	uint8 *p;
	
	left = (int)(wr->length - wr->cursor);
	if (argc > left)
		argc = left;
	
	p = wr->buffer->data + wr->cursor + wr->offset;
	for (int i = 0; i < argc; i++) {
		uint32 val = (uint32)luaL_checkint(L, i + 2);
		*p = (uint8)val;
		p++;
	}
	wr->cursor += (size_t)argc;
	return 0;
}

/*
** self = writer:putw(w1, w2, ... wN)
*/
static int lwriter_putw(lua_State *L)
{
	Writer *wr = (Writer*)luaL_checkudata(L, 1, WRITER_META);
	int argc = (size_t)lua_gettop(L) - 1;
	int left;
	uint8 *p;
	bool be;
	
	left = (int)(wr->length - wr->cursor);
	if (argc > left / 2)
		argc = left / 2;
	
	p = wr->buffer->data + wr->cursor + wr->offset;
	be = wr->buffer->be;
	for (int i = 0; i < argc; i++) {
		uint32 val = (uint32)luaL_checkint(L, i);
		if (be) {
			uint16_to_bytes_be(val, p);
		} else {
			uint16_to_bytes_le(val, p);
		}
		p += 2;
	}
	wr->cursor += (size_t)argc * 2;
	return 0;
}
/*
** self = writer:puti(i1, i2, ... iN)
*/
static int lwriter_puti(lua_State *L)
{
	Writer *wr = (Writer*)luaL_checkudata(L, 1, WRITER_META);
	int argc = (size_t)lua_gettop(L) - 1;
	int left;
	uint8 *p;
	bool be;
	
	left = (int)(wr->length - wr->cursor);
	if (argc > left / 4)
		argc = left / 4;
	
	p = wr->buffer->data + wr->cursor + wr->offset;
	be = wr->buffer->be;
	for (int i = 0; i < argc; i++) {
		uint32 val = (uint32)luaL_checkint(L, i);
		if (be) {
			uint32_to_bytes_be(val, p);
		} else {
			uint32_to_bytes_le(val, p);
		}
		p += 4;
	}
	wr->cursor += (size_t)argc * 2;
	return 0;
}

/*
** self = writer:putlist("cwi", c, w, i)
*/
static int lwriter_putlist(lua_State *L)
{
	Writer *wr = (Writer*)luaL_checkudata(L, 1, WRITER_META);
	const char *types = luaL_checkstring(L, 2);
	bool be = wr->buffer->be;
	int idx = 3;
	int left = (int)(wr->length - wr->cursor);
	bool success = true;
	uint8 *p = wr->buffer->data + wr->offset + wr->cursor;
	
	while (*types != 0 && success) {
		char type = *types++;
		uint32 value = (uint32)luaL_checkint(L, idx++);
		switch (type) {
		case 'c': {
				if (left > 0)  {
					*p = (uint8)value;
					wr->cursor++;
					p++;
				} else {
					success = false;
				}
				break;
			}
		case 'w': {
				if (left >= 2) {
					if (be) {
						uint16_to_bytes_be(value, p);
					} else {
						uint16_to_bytes_le(value, p);
					}
					p += 2;
					wr->cursor += 2;
				} else {
					success = false;
				}
				break;
			}
		case 'i': 
		case 'u': {
				if (left >= 4) {
					if (be) {
						uint32_to_bytes_be(value, p);
					} else {
						uint32_to_bytes_le(value, p);
					}
					p += 4;
					wr->cursor += 4;
				} else {
					success = false;
				}
				break;
			}
		default:
			break;
		}
		left = (int)(wr->length - wr->cursor);
	}
	return 0;
}

/*
** self = writer:putstr(str1, str2, ..., strN)
*/
static int lwriter_putstr(lua_State *L)
{
	Writer *wr = (Writer*)luaL_checkudata(L, 1, WRITER_META);
	int top = lua_gettop(L);
	
	for (int i = 2; i <= top; i++) {
		size_t len;
		const char *str = luaL_checklstring(L, i, &len);
		size_t left = (size_t)(wr->length - wr->cursor);
		
		if (left >= len) {
			uint8 *p = wr->buffer->data + wr->cursor + wr->offset;
			memcpy(p, str, len);
			wr->cursor += len;
		} else {
			break;
		}
	}
	return 0;
}

static int lwriter_tostring(lua_State *L)
{
	Writer *wr = (Writer*)luaL_checkudata(L, 1, WRITER_META);
	char buf[LINE_MAX];
	snprintf(buf, sizeof(buf), "writer (0x%08x, offset=%d, length=%d, cursor=%d)", 
				(unsigned int)wr, (int)wr->offset, (int)wr->length, (int)wr->cursor);
	lua_pushstring(L, buf);
	return 1;
}

static const luaL_Reg writer_methods[] = {
	{"__tostring", lwriter_tostring},
	{"putc", lwriter_putc},
	{"putw", lwriter_putw},
	{"puti", lwriter_puti},
	{"putu", lwriter_puti},
	{"putlist", lwriter_putlist},
	{"putstr", lwriter_putstr},
	{NULL, NULL},
};

int l_openbuffer(lua_State *L) 
{
	BufferCFunc *cfunc = NULL;
	
	lua_newtable(L);			/* [buffer] */
	lua_pushvalue(L, -1);		/* [buffer, buffer] */
	luaL_register(L, NULL, buffer_methods);
	lua_pushstring(L, "_cfuncs");	/* [buffer, buffer,  "_cfuncs"] */
	cfunc = lua_newuserdata(L, sizeof(*cfunc));	/* [buffer, buffer,  "_cfuncs", cfuncs] */
	cfunc->init = buffer_init;
	cfunc->grow = buffer_grow;
	cfunc->push = buffer_push;
	cfunc->rewind = buffer_rewind;
	cfunc->shift = buffer_shift;
	cfunc->pop = buffer_pop;
	cfunc->reset = buffer_reset;
	cfunc->finalize = buffer_finalize;
	lua_settable(L, -3);	/* [buffer, buffer] */
	lua_setglobal(L, "buffer");	/* [buffer] */
	l_register_metatable(L, BUFFER_META, buffer_meta_methods);
	lua_pop(L, 1);
	
	l_register_metatable2(L, WRITER_META, writer_methods);
	
	lua_newtable(L);
	lua_setfield(L, LUA_REGISTRYINDEX, BUFFER_READERS);
	lua_newtable(L);
	lua_setfield(L, LUA_REGISTRYINDEX, BUFFER_WRITERS);
	
	return 0;
}
