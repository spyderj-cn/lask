
/*
 * Copyright (C) Spyderj
 */


#include "lstdimpl.h"
#include <errno.h>
#include <sys/prctl.h>

/*
** prctl.setname(process_name)
*/
static int lprctl_setname(lua_State *L)
{
	int err = prctl(PR_SET_NAME, luaL_checkstring(L, 1), NULL, NULL);
	lua_pushinteger(L, err == 0 ? 0 : errno);
	return 1;
}

static const luaL_Reg funcs[] = {
	{"setname", lprctl_setname},
	{NULL, NULL}
};

int l_openprctl(lua_State *L) 
{
	l_register_lib(L, "prctl", funcs, NULL);
	return 0;
}
