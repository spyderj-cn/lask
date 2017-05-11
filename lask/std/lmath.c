
/*
 * Copyright (C) spyder
 */


#include "lstdimpl.h"
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

/*
** val1, val2, val3, ... valN = math.bitfields(val, bits1, bits2, ... )
*/
static int lmath_bitfields(lua_State *L)
{
	uint32 src = (uint32)luaL_checkinteger(L, 1);
	uint32 dst;
	uint32 bits_left = 32;
	int top = lua_gettop(L);
	int retc = 0;

	for (int i = 2; i <= top && bits_left > 0; i++) {
		uint32 bits = (uint32)luaL_checkinteger(L, i);
		if (bits > bits_left)
			bits = bits_left;

		if (bits < 32)
			dst = src & ((1 << bits) - 1);
		else
			dst = src;

		lua_pushinteger(L, (lua_Integer)dst);
		bits_left -= bits;
		src >>= bits;
		retc++;
	}

	if (bits_left > 0) {
		if (bits_left < 32)
			dst = src & ((1 << bits_left) - 1);
		else
			dst = src;

		lua_pushinteger(L, (lua_Integer)dst);
		retc++;
	}
	return retc;
}

/*
** TODO:
** str = math.randstr(character_num)
*/
static int lmath_randstr(lua_State *L)
{
	size_t len = (size_t)luaL_checkinteger(L, 1);
	char tmp[LINE_MAX];

	if (len > sizeof(tmp))
		len = sizeof(tmp);

	for (size_t i = 0; i < len; i++) {
		int value = rand() % 62;
		char ch;
		if (value < 10)
			ch = '0' + value;
		else if (value < 36)
			ch = 'a' + (value - 10);
		else
			ch = 'A' + (value - 36);
		tmp[i] = ch;
	}
	lua_pushlstring(L, tmp, len);
	return 1;
}

/*
** str = math.oct(intval)
*/
static int lmath_oct(lua_State *L)
{
	bool legal = true;
	unsigned int val = (unsigned int)luaL_checkinteger(L, 1);
	unsigned int ret = 0;
	uint8 digits[20];
	uint32 ndigits = 0;

	while (val > 0) {
		uint8 d = val % 10;
		if (d >= 8) {
			legal = false;
			break;
		}
		digits[ndigits++] = d;
		val /= 10;
	}
	if (legal) {
		for (int i = (int)ndigits - 1; i >= 0; i--) {
			ret *= 8;
			ret += digits[i];
		}
	}
	lua_pushinteger(L, (int)ret);
	return 1;
}

static const luaL_Reg funcs[] = {
	{"bitfields", lmath_bitfields},
	{"randstr", lmath_randstr},
	{"oct", lmath_oct},
	{NULL, NULL}
};

int l_openmath(lua_State *L)
{
	srand(time(NULL) + (time_t)getpid());
	l_register_lib(L, "math", funcs, NULL);
	return 0;
}
