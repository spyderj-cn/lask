
/*
 * Copyright (C) spyder
 */


#include "lstdimpl.h"

/*
**  reader = string.reader(str)
*/
static int lstring_reader(lua_State *L)
{
	size_t len;
	const char *str = luaL_checklstring(L, 1, &len);
	Reader *reader = NULL;
	int top = lua_gettop(L);

	if (top >= 2 && lua_type(L, 2) == LUA_TUSERDATA) {
		reader = reader_lcheck(L, 2);
		lua_pushvalue(L, 2);
	}
	if (reader == NULL) {
		reader = lua_newuserdata(L, sizeof(Reader));
		l_setmetatable(L, -1, READER_META);
	}
	reader_init(reader, (const uint8*)str, len);
	return 1;
}

/*
** tab, num = string.tokenize(src, delim[, tab])
**
** a light-weight tokenizer (simply a wrapper for strtok)
*/
static int lstring_tokenize(lua_State *L)
{
	size_t len;
	const char *str = luaL_checklstring(L, 1, &len);
	const char *delim = luaL_checkstring(L, 2);
	char buf[len + 1], *tok = NULL;
	int i = 1;

	memcpy(buf, str, len);
	buf[len] = 0;
	tok = strtok(buf, delim);

	if (lua_type(L, 3) == LUA_TTABLE)
		lua_pushvalue(L, 3);
	else
		lua_newtable(L);

	while (tok != NULL) {
		lua_pushstring(L, tok);
		lua_rawseti(L, -2, i++);
		tok = strtok(NULL, delim);
	}
	lua_pushinteger(L, i - 1);
	return 2;
}

typedef struct eachtok_info {
	size_t src_len;
	size_t delim_len;
	char *src;
	char buf[0];
}eachtok_info_t;

static int l_eachtok_next(lua_State *L)
{
	eachtok_info_t *info = (eachtok_info_t*)lua_touserdata(L, 1);

	if (info == NULL) {
		luaL_error(L, "illegal usage of string.eachtok");
	} else {
		char *delim = info->buf;
		char *tok = strtok(info->src, delim);

		if (info->src != NULL)
			info->src = NULL;

		if (tok != NULL) {
			lua_pushstring(L, tok);
		} else {
			lua_pushnil(L);
			FREE(info);
		}
	}
	return 1;
}

/*
** for tok in str:eachtok(',') do ... end
*/
static int lstring_eachtok(lua_State *L)
{
	size_t src_len;
	const char *src = luaL_checklstring(L, 1, &src_len);
	size_t delim_len;
	const char *delim = luaL_checklstring(L, 2, &delim_len);
	eachtok_info_t *info = (eachtok_info_t*)MALLOC(sizeof(eachtok_info_t) + src_len + delim_len + 2);

	info->delim_len = delim_len;
	memcpy(info->buf, delim, delim_len);
	info->buf[delim_len] = 0;

	info->src_len = src_len;
	memcpy(info->buf + delim_len + 1, src, src_len);
	info->buf[src_len + delim_len + 1] = 0;

	info->src = info->buf + delim_len + 1;

	lua_pushcfunction(L, l_eachtok_next);
	lua_pushlightuserdata(L, info);
	return 2;
}

/*
** str = string.addslashes(src)
*/
static int lstring_addslashes(lua_State *L)
{
	char stkbuf[LINE_MAX];
	char *dst = NULL, *p = NULL;
	size_t srclen = 0;
	const char *src = luaL_checklstring(L, 1, &srclen);

	for (size_t i = 0; i < srclen; i++) {
		char ch = src[i];
		if (ch == '\\' || ch == '\'' || ch == '\"' || ch == '\0' || ch == '\r' || ch == '\n') {
			if (dst == NULL) {
				if (srclen * 2 > sizeof(stkbuf))
					dst = (char*)MALLOC(srclen * 2);
				else
					dst = stkbuf;

				if (i > 0)
					memcpy(dst, src, i);

				p = dst + i;
			}
			*p = '\\';
			if (ch == '\'' || ch == '\"' || ch == '\\')
				p[1] = ch;
			else if (ch == '\0')
				p[1] = '0';
			else if (ch == '\r')
				p[1] = 'r';
			else if (ch == '\n')
				p[1] = 'n';
			p += 2;
		} else if (p != NULL) {
			*p = ch;
			p++;
		}
	}

	if (dst != NULL) {
		lua_pushlstring(L, dst, p - dst);
		if (dst != stkbuf)
			FREE(dst);
	} else {
		lua_pushvalue(L, 1);
	}
	return 1;
}

static const luaL_Reg funcs[] = {
	{"reader", lstring_reader},
	{"tokenize", lstring_tokenize},
	{"eachtok", lstring_eachtok},
	{"addslashes", lstring_addslashes},
	{NULL, NULL}
};

int l_openstring(lua_State *L)
{
	lua_getglobal(L, "string");
	luaL_setfuncs(L, funcs, 0);

	lua_pushstring(L, "");
	lua_getmetatable(L, -1);

	lua_pop(L, 3);

	return 0;
}
