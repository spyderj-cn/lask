
/*
 * Copyright (C) spyder
 */


#include "lstdimpl.h"

/******************************************************************************
	reader-api
******************************************************************************/

void reader_init(Reader *rd, const uint8 *mem, size_t memsiz)
{
	rd->magic = READER_MAGIC;
	rd->mem = rd->data = mem;
	rd->memsiz = rd->datasiz = memsiz;
	rd->be = true;
}

void reader_shift(Reader *rd, size_t siz)
{
	if (siz > rd->datasiz)
		siz = rd->datasiz;

	rd->datasiz -= siz;
	rd->data += siz;
}

const char* reader_getline(Reader *rd, size_t *len)
{
	char *p = (char*)rd->data;
	char *start = (char*)rd->data;
	char *end = start + rd->datasiz;
	size_t eaten;

	if (rd->datasiz == 0 || *start == 0)
		return NULL;

	while (*p != '\n' && p < end)
		p++;
	if (p == end)
		return NULL;

	eaten = p - start;
	if (p < end)
		eaten++;

	if (*p == '\n')
		*p = 0;
	if (p > start && p[-1] == '\r')
		p[-1] = 0;

	rd->data += eaten;
	rd->datasiz -= eaten;
	if (start[eaten - 1] == 0)
		eaten--;
	if (eaten > 0 && start[eaten - 1] == 0)
		eaten--;
	*len = eaten;
	return start;
}

Reader* reader_lcheck(lua_State *L, int idx)
{
	Reader *rd = lua_touserdata(L, idx);
	if (rd == NULL || rd->magic != READER_MAGIC)
		luaL_error(L, "expecting reader(userdata) for argument %d", idx);
	return rd;
}

/******************************************************************************
	lua interface
******************************************************************************/
/*
** __len
*/
static int lreader_len(lua_State *L)
{
	Reader *rd = reader_lcheck(L, 1);
	lua_pushinteger(L, rd->datasiz);
	return 1;
}

/*
** __tostring
*/
static int lreader_tostring(lua_State *L)
{
	Reader *rd = reader_lcheck(L, 1);
	char buf[LINE_MAX];
	snprintf(buf, sizeof(buf), "reader (%p, all=%u, left=%u, %s-endian)",
			rd, (unsigned int)rd->memsiz, (unsigned int)rd->datasiz, rd->be ? "big" : "little");
	lua_pushstring(L, buf);
	return 1;
}

/*
** str = reader:str()
*/
static int lreader_str(lua_State *L)
{
	Reader *rd = reader_lcheck(L, 1);
	lua_pushlstring(L, (const char*)rd->data, rd->datasiz);
	return 1;
}

/*
** reader:setbe(true/false)
*/
static int lreader_setbe(lua_State *L)
{
	Reader *rd = reader_lcheck(L, 1);
	rd->be = true;
	if (lua_gettop(L) > 1)
		rd->be = (bool)lua_toboolean(L, 2);
	return 0;
}

/*
** reader = reader:sub(offset=0, length=all)
*/
static int lreader_sub(lua_State *L)
{
	Reader *rd = reader_lcheck(L, 1);
	Reader *subrd;
	size_t offset = 0;
	size_t length = rd->datasiz;
	int top = lua_gettop(L);

	if (top >= 2) {
		offset = (size_t)luaL_checkinteger(L, 2);
		if (offset >= rd->datasiz)
			offset = 0;
	}

	if (top >= 3) {
		length = (size_t)luaL_checkinteger(L, 3);
	}

	if (offset + length > rd->datasiz)
		length = rd->datasiz - offset;

	subrd = (Reader*)lua_newuserdata(L, sizeof(Reader));
	reader_init(subrd, rd->data + offset, length);
	l_setmetatable(L, -1, READER_META);
	return 1;
}

/*
** c1, c2, c3, ... cN = reader:getc(N = 1)
**
** read n bytes(unsigned 8-bit integer), each one is a number
*/
static int lreader_getc(lua_State *L)
{
	Reader *rd = reader_lcheck(L, 1);
	size_t num = (size_t)luaL_optinteger(L, 2, 1);

	if (num > rd->datasiz)
		num = rd->datasiz;

	for (size_t i = 0; i < num; i++) {
		lua_pushinteger(L, (lua_Integer)rd->data[i]);
	}
	reader_shift(rd, num);
	return (int)num;
}

/*
** w1, w2, w3, ... wN = reader:getw(N = 1)
**
** read n words(unsigned 16-bit integer), each one is a number
*/
static int lreader_getw(lua_State *L)
{
	Reader *rd = reader_lcheck(L, 1);
	size_t num = (size_t)luaL_optinteger(L, 2, 1);
	bool be = rd->be;
	const uint8 *p = rd->data;

	if (num * 2 > rd->datasiz)
		num = rd->datasiz / 2;

	for (size_t i = 0; i < num; i++) {
		lua_pushinteger(L, be ? (lua_Integer)bytes_to_uint16_be(p) : (lua_Integer)bytes_to_uint16_le(p));
		p += 2;
	}
	reader_shift(rd, 2 * num);
	return (int)num;
}

/*
** i1, i2, i3, ... iN = reader:getu(N = 1)
**
** read n integer(signed 32-bit integer), each one is a number
*/
static int lreader_getu(lua_State *L)
{
	Reader *rd = reader_lcheck(L, 1);
	size_t num = (size_t)luaL_optinteger(L, 2, 1);
	bool be = rd->be;
	const uint8 *p = rd->data;

	if (num * 4 > rd->datasiz)
		num = rd->datasiz / 4;

	for (size_t i = 0; i < num; i++) {
		lua_pushinteger(L, be ? (lua_Integer)(bytes_to_uint32_be(p)) : (lua_Integer)(bytes_to_uint32_le(p)));
		p += 4;
	}
	reader_shift(rd, 4 * num);
	return (int)num;
}

/*
** c, w, i, u, u = reader:getlist("cwiuu")
*/
static int lreader_getlist(lua_State *L)
{
	Reader *rd = reader_lcheck(L, 1);
	const char *types = luaL_checkstring(L, 2);
	const char *p = types;
	bool empty = false;
	bool be = rd->be;

	while (*p != 0 && !empty) {
		char type = *p;
		switch (type) {
		case 'c': case 'b': {
				if (rd->datasiz < 1)  {
					empty = true;
				} else {
					lua_pushinteger(L, (lua_Integer)rd->data[0]);
					reader_shift(rd, 1);
				}
				break;
			}
		case 'w': {
				if (rd->datasiz < 2)  {
					empty = true;
				} else {
					const uint8 *p = rd->data;
					lua_pushinteger(L, be ? (lua_Integer)(bytes_to_uint16_be(p)) : (lua_Integer)(bytes_to_uint16_le(p)));
					reader_shift(rd, 2);
				}
				break;
			}
		case 'i': case 'u': {
				if (rd->datasiz < 4)  {
					empty = true;
				} else {
					const uint8 *p = rd->data;
					lua_pushinteger(L, be ? (lua_Integer)(bytes_to_uint32_be(p)) : (lua_Integer)(bytes_to_uint32_le(p)));
					reader_shift(rd, 4);
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
** line1, ... = reader:getline(num)
**
** Note that EOL(\r\n, \n, \n\r) is removed.
*/
static int lreader_getline(lua_State *L)
{
	Reader *rd = reader_lcheck(L, 1);
	size_t num = (size_t)luaL_optinteger(L, 2, 1);

	for (size_t i = 0; i < num ; i++) {
		size_t len;
		const char *str = reader_getline(rd, &len);
		if (str != NULL)
			lua_pushlstring(L, str, len);
		else
			lua_pushnil(L);
	}
	return (int)num;
}

static int eachline_next(lua_State *L)
{
	Reader *rd = (Reader*)lua_touserdata(L, 1);
	size_t len;
	const char *str = reader_getline(rd, &len);
	if (str != NULL)
		lua_pushlstring(L, str, len);
	else
		lua_pushnil(L);
	return 1;
}

/*
** next_func, priv_userdata = reader:eachline()
*/
static int lreader_eachline(lua_State *L)
{
	Reader *rd = reader_lcheck(L, 1);
	lua_pushcfunction(L, eachline_next);
	lua_pushlightuserdata(L, rd);
	return 2;
}

/*
** str = reader:getlstr(len=nil)
**
** read a string with the sppecified length
*/
static int lreader_getlstr(lua_State *L)
{
	Reader *rd = reader_lcheck(L, 1);
	size_t length = (size_t)luaL_optinteger(L, 2, -1);

	if (length > rd->datasiz)
		length = rd->datasiz;

	if (length > 0) {
		lua_pushlstring(L, (const char*)rd->data, length);
		rd->data += length;
		rd->datasiz -= length;
	} else {
		lua_pushnil(L);
	}

	return 1;
}

/*
** reader:skip(bytes)
*/
static int lreader_skip(lua_State *L)
{
	Reader *rd = reader_lcheck(L, 1);
	size_t bytes = (size_t)luaL_checkinteger(L, 2);
	reader_shift(rd, bytes);
	return 0;
}

/*
** length = reader:shifted()
*/
static int lreader_shifted(lua_State *L)
{
	Reader *rd = reader_lcheck(L, 1);
	size_t length = rd->data - rd->mem;
	lua_pushinteger(L, length);
	return 1;
}

/*
** offset, tail = reader:find_formdata_content_boundary(boundary)
*/
static int lreader_find_formdata_content_boundary(lua_State *L)
{
	Reader *rd = reader_lcheck(L, 1);
	size_t boundary_len;
	const char *boundary = luaL_checklstring(L, 2, &boundary_len);
	const char *p = (const char*)rd->data;
	const char *end = p + rd->datasiz;
	int offset = -1, tail = 0;
	size_t len;

	if (boundary_len <= 2 || boundary[0] != '-' || boundary[1] != '-') {
		luaL_error(L, "not a valid http-boundary for argument #2");
	}

	while (p < end) {
		while (*p != '\r' && p < end)
			p++;

		if (p == end)
			break;

		len = (end - p);
		if (len == 1) { /* ended with \r */
			tail = 1;
			break;
		}

		if (p[1] != '\n') {
			p += 2;
		} else if (len >= boundary_len + 2) {
			if (p[2] == '-' && p[3] == '-' && strncmp(p + 4, boundary + 2, boundary_len - 2) == 0) {
				offset = (int)(p - (const char*)rd->data);
				break;
			} else {
				p += 2;
			}
		} else {
			if (strncmp(p + 2, boundary, len - 2) == 0) {
				tail = (int)len;
				break;
			} else {
				p += 2;
			}
		}
	}

	lua_pushinteger(L, offset);
	lua_pushinteger(L, tail);
	return 2;
}

static const luaL_Reg meta_funcs[] = {
	{"__len", lreader_len},
	{"__tostring", lreader_tostring},
	{NULL, NULL}
};

static const luaL_Reg funcs[] = {
	{"str", lreader_str},
	{"setbe", lreader_setbe},
	{"sub", lreader_sub},
	{"getc", lreader_getc},
	{"getw", lreader_getw},
	{"geti", lreader_getu},
	{"getu", lreader_getu},
	{"getlstr", lreader_getlstr},
	{"getlist", lreader_getlist},
	{"getline", lreader_getline},
	{"eachline", lreader_eachline},
	{"skip", lreader_skip},
	{"shifted", lreader_shifted},
	{"find_formdata_content_boundary", lreader_find_formdata_content_boundary},
	{NULL, NULL}
};

int l_openreader(lua_State *L)
{
	luaL_newlib(L, funcs);
	lua_pushvalue(L, -1);
	lua_setglobal(L, "reader");
	l_register_metatable(L, READER_META, meta_funcs);
	lua_pop(L, 1);
	return 0;
}
