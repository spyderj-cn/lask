
/*
 * Copyright (C) Spyderj
 */


#include "lstdimpl.h"
#include <ctype.h>

static uint8 code_table[256] = {0};
static const char *xdigits = "0123456789abcdefABCDEF";

static inline int xvalue(int c)
{
	if (c <= '9' && c >= '0')
		return c - '0';
	else if (c <= 'F' && c >= 'A')
		return c - 'A' + 10;
	else
		return c - 'a' + 10;
}

/*
** str/buf = codec.urlencode(src, buf=nil)
*/
static int lcodec_urlencode(lua_State *L)
{
	const char *src = luaL_checkstring(L, 1);
	Buffer _buf;
	Buffer *buf = &_buf;
	char chbuf[36], *p = chbuf;
	uint8 ch;
	
	if (lua_gettop(L) > 1) 
		buf = buffer_lcheck(L, 2);
	else
		buffer_init(buf, 256);
	
	while ((ch = (uint8)*src) != 0) {
		uint8 code = code_table[ch];
		
		if (p - chbuf >= 32) {
			buffer_push(buf, (const uint8*)chbuf, p - chbuf);
			p = chbuf;
		}
		
		if (code != 0 && code != 1) {
			p[0] = '%';
			p[1] = xdigits[code / 16];
			p[2] = xdigits[code % 16];
			p += 3;
		} else {
			p[0] = (ch != ' ') ? (char)ch : '+';
			p++;
		}
		src++;
	}
	if (p > chbuf)
		buffer_push(buf, (const uint8*)chbuf, p - chbuf);
	
	if (buf == &_buf) {
		lua_pushlstring(L, (const char*)buf->data, buf->datasiz);
		buffer_finalize(buf);
	} else {
		lua_pushvalue(L, 2);
	}
	return 1;
}

static char* urldecode(char *str)
{
	if (str != NULL) {
		char *src = str, *dst = str;
		while (*src != 0) {
			if (src[0] == '%' && code_table[(uint8)src[1]] == 1 && code_table[(uint8)src[2]] == 1) {
				*dst = (char)(xvalue(src[1]) * 16 + xvalue(src[2]));
				src += 3;
			} else {
				if (*src == '+')
					*dst = ' ';
				else if (dst != src)
					*dst = *src;
				src++;
			} 
			dst++;
		}
		*dst = 0;
	}
	return str;
}

/*
** str/buf = codec.urldecode(src, buf=nil)
*/
static int lcodec_urldecode(lua_State *L)
{
	size_t inlen = 0;
	const char *in = luaL_checklstring(L, 1, &inlen);
	size_t outlen = 0;
	char out[inlen + 1];
	Buffer *buf = NULL;
	
	if (lua_gettop(L) > 1)
		buf = buffer_lcheck(L, 2);
	
	memcpy(out, in, inlen);
	out[inlen] = 0;
	urldecode(out);
	outlen = strlen(out);

	if (buf != NULL) {
		buffer_push(buf, (const uint8*)out, outlen);
		lua_pushvalue(L, 2);
	} else {
		lua_pushlstring(L, out, outlen);
	}
	return 1;
}

static bool isdigitstr(const char *p, const char *end)
{
	if (p == end)
		return false;
		
	while (p < end) {
		char ch = *p;
		if (ch == 0)
			return true;
		else if (!isdigit(ch))
			return false;
		p++;
	}
	return true;
}

/*
** url = codec.urlsplit(src_str)
** 
** url is {
	scheme=..., 
	user=..., 
	password=..., 
	host=..., 
	port=xxx, 
	path=..., 
	query=..., 
	fragment=...
}
*/
static int lcodec_urlsplit(lua_State *L)
{
	enum {
		SCHEME_IDX,
		USER_IDX,
		PASSWORD_IDX,
		HOST_IDX,
		PORT_IDX,
		PATH_IDX,
		QUERY_IDX,
		FRAGMENT_IDX,
	};
	const char *keys[] = {
		"scheme",
		"user",
		"password",
		"host",
		"port",
		"path",
		"query",
		"fragment",
	};
	
	char *parts[FRAGMENT_IDX + 1] = {0};
	size_t len;
	const char *_str = luaL_checklstring(L, 1, &len);
	char str[len + 1];
	char host[len + 1];
	int port;
	char *p;
	char *scheme_pos;
	char *colon_pos;
	char *slash_pos;
	char *at_pos;
	char root[] = "/";
	
	memcpy(str, _str, len);
	str[len] = 0;
	p = str;
	if (*p == '/' || *p == 0) {
		slash_pos = p;
		goto abspath;
	}
		
	/* scheme */
	scheme_pos = strstr(p, "://");
	if (scheme_pos != NULL) {
		parts[SCHEME_IDX] = p;
		*scheme_pos = 0;
		p = scheme_pos + 3;
	}
	slash_pos = strchr(p, '/');
	if (slash_pos == NULL)
		slash_pos = str + len;
	
	/* user:password */
	at_pos = strchr(p, '@');
	if (at_pos != NULL && at_pos < slash_pos) {
		*at_pos = 0;
		parts[USER_IDX] = p;
		colon_pos = strchr(p, ':');
		if (colon_pos != NULL) {
			*colon_pos = 0;
			parts[PASSWORD_IDX] = colon_pos + 1;
		}
		p = at_pos + 1;
	}
	
	/* port */
	colon_pos = strchr(p, ':');
	if (colon_pos != NULL && colon_pos < slash_pos) {
		if (!isdigitstr(colon_pos + 1, slash_pos))
			goto err;
			
		parts[PORT_IDX] = colon_pos + 1;
		port = atoi(colon_pos + 1);
		*colon_pos = 0;
		parts[HOST_IDX] = p;
		urldecode(p);
	} else {
		size_t hostlen = slash_pos - p;
		memcpy(host, p, hostlen + 1);
		host[hostlen] = 0;
		parts[HOST_IDX] = host;
		urldecode(host);
	}
	
abspath:
	p = slash_pos;
	if (*p == 0) {
		parts[PATH_IDX] = root;
	} else {
		char *query_pos = strchr(p, '?');
		char *frag_pos = NULL;
		
		if (query_pos != NULL) {
			parts[PATH_IDX] = p;
			*query_pos = 0;
			p = query_pos + 1;
			
			frag_pos = strchr(p, '#');
			if (frag_pos != NULL) {
				*frag_pos = 0;
				parts[QUERY_IDX] = p;
				parts[FRAGMENT_IDX] = frag_pos + 1;
			} else {
				parts[QUERY_IDX] = p;
			}
		} else {
			frag_pos = strchr(p, '#');
			if (frag_pos != NULL) {
				*frag_pos = 0;
				parts[PATH_IDX] = p;
				parts[FRAGMENT_IDX] = frag_pos + 1;
			} else {
				parts[PATH_IDX] = p;
			}
		}
	}

	lua_newtable(L);
	for (size_t i = 0; i < lengthof(parts); i++) {
		char *data = parts[i];
		if (data != NULL) {
			if (i == PORT_IDX) 
				lua_pushinteger(L, port);
			else 
				lua_pushstring(L, data);
			lua_setfield(L, -2, keys[i]);
		}
	}
	return 1;
	
err:
	return 0;
}

static const luaL_Reg funcs[] = {
	{"urlencode", lcodec_urlencode},
	{"urldecode", lcodec_urldecode},
	{"urlsplit", lcodec_urlsplit},
	{NULL, NULL},
};

int l_opencodec(lua_State *L)
{
	if (code_table['+'] == 0) {
		const char *reserved = ";/?:@&=+$,";
		
		for (const char *p = reserved; *p != 0; p++) {
			code_table[(uint8)*p] = (uint8)*p;
		}
		
		for (const char *p = xdigits; *p != 0; p++) {
			code_table[(uint8)(*p)] = 1;
		}
		
		for (uint32 i = 128; i < 256; i++) {
			code_table[i] = i;
		}
	}
	
	l_register_lib(L, "codec", funcs, NULL);
	
	return 0;
}
