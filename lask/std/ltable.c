
/*
 * Copyright (C) spyder
 */


#include "lstdimpl.h"

int ltable_array(lua_State *L)
{
	int type = lua_type(L, 1);
	if (type == LUA_TNUMBER) {
		int narr = (int)luaL_checkinteger(L, 1);
		lua_createtable(L, narr, 0);
	} else if (type == LUA_TTABLE) {
		int len = lua_rawlen(L, 1);
		lua_createtable(L, len, 0);
		for (int i = 1; i <= len; i++) {
			lua_rawgeti(L, 1, i);
			lua_rawseti(L, -2, i);
		}
	} else {
		luaL_error(L, "expecting number or table for argument #1");
	}
	return 1;
}

static const luaL_Reg funcs[] = {
	{"array", ltable_array},
	{NULL, NULL}
};

int l_opentable(lua_State *L)
{
	l_register_lib(L, "table", funcs, NULL);
	return 0;
}
