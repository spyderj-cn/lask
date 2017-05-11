
/*
 * Copyright (C) spyder
 */


#include "lstdimpl.h"
#include <errno.h>
#include <sys/inotify.h>

/*
** fd, err = inotify.init(flags=nil)
*/
static int linotify_init(lua_State *L)
{
	int flags = luaL_optint(L, 1, 0);
	int fd;
	int err = 0;
	
	if (flags == 0)
		fd = inotify_init();
	else
		fd = inotify_init1(flags);
	
	if (fd < 0)
		err = errno;
	
	lua_pushinteger(L, fd);
	lua_pushinteger(L, err);
	return 2;
}

/*
** wd, err = inotify.add_watch(fd, path, mask)
*/
static int linotify_add_watch(lua_State *L)
{
	int fd = luaL_checkinteger(L, 1);
	const char *path = luaL_checkstring(L, 2);
	uint32_t mask = (uint32_t)luaL_checkinteger(L, 3);
	int wd = inotify_add_watch(fd, path, mask);
	int err = 0;
	
	if (wd < 0)
		err = errno;
		
	lua_pushinteger(L, wd);
	lua_pushinteger(L, err);
	return 2;
}

/*
** inotify.rm_watch(fd, wd)
*/
static int linotify_rm_watch(lua_State *L)
{
	int fd = luaL_checkinteger(L, 1);
	int wd = luaL_checkinteger(L, 2);
	inotify_rm_watch(fd, wd);
	return 0;
}

static const luaL_Reg funcs[] = {
	{"init", linotify_init},
	{"add_watch", linotify_add_watch},
	{"rm_watch", linotify_rm_watch},
	{NULL, NULL}
};

static const EnumReg enums[] = {
	/* flags to inotify.init */
	LENUM(IN_CLOEXEC),
	LENUM(IN_NONBLOCK),
	LENUM(IN_ONLYDIR),
	LENUM(IN_DONT_FOLLOW),
	LENUM(IN_MASK_ADD),
	LENUM(IN_ISDIR),
	LENUM(IN_ONESHOT),
	
	/* mask*/
	LENUM(IN_ACCESS),
	LENUM(IN_MODIFY),
	LENUM(IN_ATTRIB),
	LENUM(IN_CLOSE_WRITE),
	LENUM(IN_CLOSE_NOWRITE),
	LENUM(IN_CLOSE),
	LENUM(IN_OPEN),
	LENUM(IN_MOVED_FROM),
	LENUM(IN_MOVED_TO),
	LENUM(IN_MOVE),
	LENUM(IN_CREATE),
	LENUM(IN_DELETE),
	LENUM(IN_DELETE_SELF),
	LENUM(IN_MOVE_SELF),
	LENUM(IN_CLOSE),
	LENUM(IN_MOVE),
	LENUM(IN_ALL_EVENTS),
	
	/* events sent by kernel */
	LENUM(IN_UNMOUNT),
	LENUM(IN_Q_OVERFLOW),
	LENUM(IN_IGNORED),
	
	LENUM_NULL
};

int l_openinotify(lua_State *L)
{
	l_register_lib(L, "inotify", funcs, enums);
	return 0;
}
