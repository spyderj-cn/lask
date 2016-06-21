
/*
 * Copyright (C) Spyderj
 */


#include "lstdimpl.h"
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

static lua_State *theL = NULL;

static void sighandler(int sig)
{
	lua_State *L = theL;
	int top = lua_gettop(L);
	lua_pushlightuserdata(L, sighandler);
	lua_gettable(L, LUA_REGISTRYINDEX);
	lua_pushinteger(L, sig);
	lua_gettable(L, -2);
	if (lua_isfunction(L, -1)) {
		lua_pushinteger(L, sig);
		lua_call(L, 1, LUA_MULTRET);
	}
	lua_settop(L, top);
}

static void on_sigchld(int sig)
{
	int stat;
	while (waitpid(-1, &stat, WNOHANG) > 0);
	unused(sig);
}

/*
** err = signal.signal(sig, function (sig) end)
*/
int lsignal_signal(lua_State *L)
{
	int sig = luaL_checkint(L, 1);
	
	if (lua_type(L, 2) == LUA_TNUMBER) {
		int value = (int)lua_tointeger(L, 2);
		if (value == (int)SIG_DFL)
			signal(sig, SIG_DFL);
		else if (value == (int)SIG_IGN)
			signal(sig, SIG_IGN);
		else
			luaL_error(L, "expecting function(or SIG_DFL/SIG_IGN) for argument #2");
	} else {
		if (!lua_isfunction(L, 2))
			luaL_error(L, "expecting function(or SIG_DFL/SIG_IGN) for argument #2");
	
		lua_pushlightuserdata(L, sighandler);
		lua_gettable(L, LUA_REGISTRYINDEX);
		if (lua_isnil(L, -1)) {
			lua_newtable(L);
			lua_pushlightuserdata(L, sighandler);
			lua_pushvalue(L, -2);
			lua_settable(L, LUA_REGISTRYINDEX);
		} 
		lua_pushinteger(L, sig);
		lua_pushvalue(L, 2);
		lua_settable(L, -3);
		signal(sig, sighandler);
	}
	return 0;
}

/*
** err = signal.kill(pid, sig)
*/
int lsignal_kill(lua_State *L)
{
	pid_t pid = (pid_t)luaL_checkint(L, 1);
	int sig = luaL_checkint(L, 2);
	int err = kill(pid, sig);
	lua_pushinteger(L, err == 0 ? err : errno);
	return 1;
}

/*
** signal.raise(sig)
*/
int lsignal_raise(lua_State *L)
{
	raise(luaL_checkint(L, 1));
	return 0;
}

/*
** signal.alarm(sec)
*/
int lsignal_alarm(lua_State *L)
{
	alarm((unsigned)luaL_checkint(L, 1));
	return 0;
}

static const EnumReg enums[] = {
	LENUM(SIGABRT),
	LENUM(SIGTERM),
	LENUM(SIGKILL),
	LENUM(SIGSTOP),
	LENUM(SIGINT),
	LENUM(SIGSEGV),
	LENUM(SIGILL),
	LENUM(SIGBUS),
	LENUM(SIGUSR1),
	LENUM(SIGUSR2),
	LENUM(SIGQUIT),
	LENUM(SIGHUP),
	LENUM(SIGPIPE),
	LENUM(SIGALRM),
	LENUM(SIGCHLD),
	LENUM(SIGCONT),
	LENUM(SIGTSTP),
	LENUM(SIGTTIN),
	LENUM(SIGTTOU),
	LENUM(SIGPROF),
	{"SIG_DFL", (int)SIG_DFL},
	{"SIG_IGN", (int)SIG_IGN},
	LENUM_NULL,
};

static const luaL_Reg funcs[] = {
	{"signal", lsignal_signal},
	{"raise", lsignal_raise},
	{"kill", lsignal_kill},
	{"alarm", lsignal_alarm},
	{NULL, NULL},
};

int l_opensignal(lua_State *L)
{
	theL = L;
	signal(SIGCHLD, on_sigchld);
	signal(SIGPIPE, SIG_IGN);
	l_register_lib(L, "signal", funcs, enums);
	return 0;
}
