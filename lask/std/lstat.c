
/*
 * Copyright (C) spyder
 */


#include "lstdimpl.h"
#include <sys/stat.h>

static int lstat_isblk(lua_State *L) 
{
	mode_t mode = (mode_t)lua_tointeger(L, 1);
	lua_pushboolean(L, S_ISBLK(mode));
	return 1;
}

static int lstat_ischr(lua_State *L) 
{
	mode_t mode = (mode_t)lua_tointeger(L, 1);
	lua_pushboolean(L, S_ISCHR(mode));
	return 1;
}

static int lstat_isdir(lua_State *L) 
{
	mode_t mode = (mode_t)lua_tointeger(L, 1);
	lua_pushboolean(L, S_ISDIR(mode));
	return 1;
}

static int lstat_isfifo(lua_State *L) 
{
	mode_t mode = (mode_t)lua_tointeger(L, 1);
	lua_pushboolean(L, S_ISFIFO(mode));
	return 1;
}

static int lstat_isreg(lua_State *L) 
{
	mode_t mode = (mode_t)lua_tointeger(L, 1);
	lua_pushboolean(L, S_ISREG(mode));
	return 1;
}

static int lstat_islnk(lua_State *L) 
{
	mode_t mode = (mode_t)lua_tointeger(L, 1);
	lua_pushboolean(L, S_ISLNK(mode));
	return 1;
}

static int lstat_issock(lua_State *L) 
{
	mode_t mode = (mode_t)lua_tointeger(L, 1);
	lua_pushboolean(L, S_ISSOCK(mode));
	return 1;
}

static const luaL_Reg funcs[] = {
	{"isblk", lstat_isblk},
	{"ischr", lstat_ischr},
	{"isdir", lstat_isdir},
	{"isfifo", lstat_isfifo},
	{"isreg", lstat_isreg},
	{"islnk", lstat_islnk},
	{"issock", lstat_issock},
	{NULL, NULL},
};

static const EnumReg enums[] = {
	LENUM(S_IFBLK),
	LENUM(S_IFCHR),
	LENUM(S_IFREG),
	LENUM(S_IFLNK),
	LENUM(S_IFDIR),
	LENUM(S_IFSOCK),
	LENUM(S_IFIFO),
	
	LENUM(S_IRUSR),
	LENUM(S_IWUSR),
	LENUM(S_IXUSR),
	LENUM(S_IRWXU),
	LENUM(S_IRGRP),
	LENUM(S_IWGRP),
	LENUM(S_IXGRP),
	LENUM(S_IRWXG),
	LENUM(S_IROTH),
	LENUM(S_IWOTH),
	LENUM(S_IXOTH),
	LENUM(S_IRWXO),
	
	LENUM(S_ISUID),
	LENUM(S_ISGID),
	LENUM(S_ISVTX),

	LENUM_NULL
};

int l_openstat(lua_State *L) 
{
	l_register_lib(L, "stat", funcs, enums);
	return 0;
}
