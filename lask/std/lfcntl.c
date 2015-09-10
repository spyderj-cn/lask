
/*
 * Copyright (C) Spyderj
 */


#include "lstdimpl.h"
#include <fcntl.h>
#include <errno.h>

int fcntl_addfl(int fd, int val) 
{
	int err = 0;
	int fl = fcntl(fd, F_GETFL, 0);
	if (fl < 0) {
		err = errno;
	} else {
		fl |= val;
		err = fcntl(fd, F_SETFL, fl);
		if (err < 0) {
			err = errno;
		}
	}
	return err;
}

int fcntl_delfl(int fd, int val) 
{
	int err = 0;
	int fl = fcntl(fd, F_GETFL, 0);
	if (fl < 0) {
		err = errno;
	} else {
		fl &= ~val;
		err = fcntl(fd, F_SETFL, fl);
		if (err < 0) {
			err = errno;
		}
	}
	return err;
}

/*
** newfd, err = fcntl.dupfd(fd)
*/
static int lfcntl_dupfd(lua_State *L)
{
	int newfd = fcntl(luaL_checkint(L, 1), F_DUPFD, 0);
	int err = 0;
	
	if (newfd < 0)
		err = errno;
		
	lua_pushinteger(L, newfd);
	lua_pushinteger(L, err);
	return 2;
}

/*
** fl, err = fcntl.getfl(fd)
*/
static int lfcntl_getfl(lua_State *L) 
{
	int fd = luaL_checkint(L, 1);
	int fl = fcntl(fd, F_GETFL, 0);
	int err = 0;
	
	if (fl < 0) {
		fl = 0;
		err = errno;
	}
	lua_pushinteger(L, fl);
	lua_pushinteger(L, err);
	return 2;
}

/*
** err = fcntl.setfl(fd, val)
*/
static int lfcntl_setfl(lua_State *L) 
{
	int err = fcntl(luaL_checkint(L, 1), F_SETFL, luaL_checkint(L, 1));
	if (err < 0)
		err = errno;
	lua_pushinteger(L, err);
	return 1;
}

/*
** err = fcntl.addfl(fd, val)
*/
static int lfcntl_addfl(lua_State *L)
{
	lua_pushinteger(L, fcntl_addfl(luaL_checkint(L, 1), luaL_checkint(L, 2)));
	return 1;
}

/*
** err = fcntl.delfl(fd, val)
*/
static int lfcntl_delfl(lua_State *L)
{
	lua_pushinteger(L, fcntl_delfl(luaL_checkint(L, 1), luaL_checkint(L, 2)));
	return 1;
}

/*
** val, err = fcntl.getfd(fd)
*/
static int lfcntl_getfd(lua_State *L) 
{
	int val = fcntl(luaL_checkinteger(L, 1), F_GETFD, 0);
	int err = 0;
	
	if (val < 0) {
		val = 0;
		err = errno;
	}
	lua_pushinteger(L, val);
	lua_pushinteger(L, 0);
	return 2;
}

/*
** err = fcntl.setfd(fd, val)
*/
static int lfcntl_setfd(lua_State *L) 
{
	int err = fcntl(luaL_checkint(L, 1), F_SETFD, luaL_checkint(L, 2));
	if (err < 0)
		err = errno;
	lua_pushinteger(L, err);
	return 1;
}

/*
** boolean = fcntl.iscloexec(fd)
*/
static int lfcntl_iscloexec(lua_State *L)
{
	int val = fcntl(luaL_checkint(L, 1), F_GETFD, 0);
	if (val < 0)
		val = 0;
	lua_pushboolean(L, val & FD_CLOEXEC);
	return 1;
}

static const luaL_Reg funcs[] = {
	{"dupfd", lfcntl_dupfd},
	{"getfd", lfcntl_getfd},
	{"setfd", lfcntl_setfd},
	{"iscloexec", lfcntl_iscloexec},
	{"getfl", lfcntl_getfl},
	{"setfl", lfcntl_setfl},
	{"addfl", lfcntl_addfl},
	{"delfl", lfcntl_delfl},
	
	{NULL, NULL}
};

static const EnumReg enums[] = {
	LENUM(FD_CLOEXEC),
	LENUM_NULL
};

int l_openfcntl(lua_State *L)
{
	l_register_lib(L, "fcntl", funcs, enums);
	return 0;
}
