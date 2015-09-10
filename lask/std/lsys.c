
/*
 * Copyright (C) Spyderj
 */


#include "lstdimpl.h"
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <sys/utsname.h>

/*
** name, err = sys.gethostname()
*/
static int lsys_gethostname(lua_State *L)
{
	char name[HOST_NAME_MAX + 1];
	int err = gethostname(name, sizeof(name));
	if (err == 0) {
		lua_pushstring(L, name);
		lua_pushinteger(L, 0);
	} else {
		lua_pushnil(L);
		lua_pushinteger(L, errno);
	}
	return 2;
}

static void pwd_settable(lua_State *L, struct passwd *pwd)
{
    lua_createtable(L, 0, 7);
	lua_pushstring(L, "name");
	lua_pushstring(L, pwd->pw_name);
	lua_rawset(L, -3);
	lua_pushstring(L, "passwd");
	lua_pushstring(L, pwd->pw_passwd);
	lua_rawset(L, -3);
	lua_pushstring(L, "uid");
	lua_pushinteger(L, pwd->pw_uid);
	lua_rawset(L, -3);
	lua_pushstring(L, "gid");
	lua_pushinteger(L, pwd->pw_gid);
	lua_rawset(L, -3);
	lua_pushstring(L, "gecos");
	lua_pushstring(L, pwd->pw_gecos);
	lua_rawset(L, -3);
	lua_pushstring(L, "dir");
	lua_pushstring(L, pwd->pw_dir);
	lua_rawset(L, -3);
	lua_pushstring(L, "shell");
	lua_pushstring(L, pwd->pw_shell);
	lua_rawset(L, -3);
}

static int lsys_pwd_next(lua_State *L)
{
    struct passwd *pwd = getpwent();
    if (pwd != NULL) {
		pwd_settable(L, pwd);
		return 1;
	} else {
		endpwent();
		return 0;
	}
}

/*
** pwd, err = sys.getpwuid(uid)
*/
static int lsys_getpwuid(lua_State *L)
{
	uid_t uid = (uid_t)luaL_checkinteger(L, 1);
	struct passwd *pwd = getpwuid(uid);
	int err = 0;
	if (pwd != NULL) {
		pwd_settable(L, pwd);
	} else {
		err = errno;
		lua_pushnil(L);
	}
	lua_pushinteger(L, err);
	return 2;
}

/*
** pwdt, err = sys.getpwnam(name)
*/
static int lsys_getpwnam(lua_State *L)
{
 	const char* name = (const char*)luaL_checkstring(L, 1);
	struct passwd *pwd = getpwnam(name);
	int err = 0;
	if (pwd != NULL) {
		pwd_settable(L, pwd);
	} else {
		err = errno;
		lua_pushnil(L);
	}
	lua_pushinteger(L, err);
	return 2;
}

/*
** for pwd in eachpw() do ...end
*/
static int lsys_eachpw(lua_State *L)
{
	lua_pushcfunction(L, lsys_pwd_next);
	return 1;
}

static void grp_settable(lua_State *L, struct group *grp)
{
	lua_createtable(L, 0, 4);
	lua_pushstring(L, "name");
	lua_pushstring(L, grp->gr_name);
	lua_rawset(L, -3);
	lua_pushstring(L, "passwd");
	lua_pushstring(L, grp->gr_passwd);
	lua_rawset(L, -3);
	lua_pushstring(L, "gid");
	lua_pushinteger(L, grp->gr_gid);
	lua_rawset(L, -3);
	lua_pushstring(L, "mem");
	lua_pushstring(L, grp->gr_mem[0]);
	lua_rawset(L, -3);
}

static int lsys_grp_next(lua_State *L)
{
    struct group *grp = getgrent();
   	if (grp != NULL) {
		grp_settable(L, grp);
		return 1;
	} else {
		endgrent();
		return 0;
	}
}

/*
** grp, err = sys.getgrnam(name)
*/
static int lsys_getgrnam(lua_State *L)
{
  	const char* name = (const char*)luaL_checkstring(L, 1);
	struct group *grp = getgrnam(name);
	int err = 0;
	if (grp != NULL) {
		grp_settable(L, grp);
	} else {
		err = errno;
		lua_pushnil(L);
	}
	lua_pushinteger(L, err);
	return 2;
}

/*
** grpt, err = sys.getgrgid(gid)
*/
static int lsys_getgrgid(lua_State *L)
{
	gid_t gid = (uid_t)luaL_checkinteger(L, 1);
	struct group *grp = getgrgid(gid);
	int err = 0;
	if (grp != NULL) {
		grp_settable(L, grp);
	} else {
		err = errno;
		lua_pushnil(L);
	}
	lua_pushinteger(L, err);
	return 2;
}

/*
** for grp in eachgr() do ...end
*/
static int lsys_eachgr(lua_State *L)
{
	lua_pushcfunction(L, lsys_grp_next);	 
	return 1;
}

/*
** {sysname=, nodename=, release=, version=, machine=} = sys.uname()
*/
static int lsys_uname(lua_State *L)
{
	lua_pushlightuserdata(L, (void*)lsys_uname);
	lua_rawget(L, LUA_REGISTRYINDEX);
	
	if (lua_type(L, -1) != LUA_TTABLE) {
		struct utsname u;
		uname(&u);
		lua_createtable(L, 0, 5);
		lua_pushstring(L, "sysname");
		lua_pushstring(L, u.sysname);
		lua_rawset(L, -3);
		lua_pushstring(L, "nodename");
		lua_pushstring(L, u.nodename);
		lua_rawset(L, -3);
		lua_pushstring(L, "release");
		lua_pushstring(L, u.release);
		lua_rawset(L, -3);
		lua_pushstring(L, "version");
		lua_pushstring(L, u.version);
		lua_rawset(L, -3);
		lua_pushstring(L, "machine");
		lua_pushstring(L, u.machine);
		lua_rawset(L, -3);
		
		lua_pushlightuserdata(L, (void*)lsys_uname);
		lua_pushvalue(L, -2);
		lua_rawset(L, LUA_REGISTRYINDEX);
	}
	return 1;
}

static const luaL_Reg funcs[] = {
	{"gethostname", lsys_gethostname},
	{"getpwnam", lsys_getpwnam},
	{"getpwuid", lsys_getpwuid},
	{"eachpw", lsys_eachpw},
	{"getgrnam", lsys_getgrnam},
	{"getgrgid", lsys_getgrgid},
	{"eachgr", lsys_eachgr},
	{"uname", lsys_uname},
	{NULL, NULL},
};

int l_opensys(lua_State *L)
{
	l_register_lib(L, "sys", funcs, NULL);
	return 0;
}
