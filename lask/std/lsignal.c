
/*
 * Copyright (C) spyder
 */


#include "lstdimpl.h"
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/signalfd.h>

static void on_sigchld(int sig)
{
	int stat;
	while (waitpid(-1, &stat, WNOHANG) > 0);
	unused(sig);
}

/*
** err = signal.kill(pid, sig)
*/
static int lsignal_kill(lua_State *L)
{
	pid_t pid = (pid_t)luaL_checkinteger(L, 1);
	int sig = (int)luaL_checkinteger(L, 2);
	int err = kill(pid, sig);
	lua_pushinteger(L, err == 0 ? err : errno);
	return 1;
}

/*
** signal.raise(sig)
*/
static int lsignal_raise(lua_State *L)
{
	raise((int)luaL_checkinteger(L, 1));
	return 0;
}

/*
** signal.alarm(sec)
*/
static int lsignal_alarm(lua_State *L)
{
	alarm((unsigned)luaL_checkinteger(L, 1));
	return 0;
}

/*
** * These are stoken from lua-posix with small modifications.
*/

static lua_State *signalL;

static lua_Hook old_hook;
static int old_mask;
static int old_count;

#define SIGNAL_QUEUE_MAX 25
static volatile sig_atomic_t signal_pending, defer_signal;
static volatile sig_atomic_t signal_count = 0;
static volatile sig_atomic_t signals[SIGNAL_QUEUE_MAX];

static void sig_handle (lua_State *L, lua_Debug *ar)
{
	/* Block all signals until we have run the Lua signal handler */
	sigset_t mask, oldmask;
	sigfillset(&mask);
	sigprocmask(SIG_SETMASK, &mask, &oldmask);

	lua_sethook(L, old_hook, old_mask, old_count);

	lua_pushlightuserdata(L, &signalL);
	lua_rawget(L, LUA_REGISTRYINDEX);

	while (signal_count--)
	{
		sig_atomic_t signalno = signals[signal_count];
		lua_pushinteger(L, signalno);
		lua_gettable(L, -2);
		lua_pushinteger(L, signalno);
		if (lua_pcall(L, 1, 0, 0) != 0)
			fprintf(stderr,"error in signal handler %ld: %s\n", (long)signalno, lua_tostring(L,-1));
	}

	signal_count = 0;  /* reset global to initial state */

	/* Having run the Lua signal handler, restore original signal mask */
	sigprocmask(SIG_SETMASK, &oldmask, NULL);
	unused(ar);
}

static void sig_postpone (int i)
{
	if (defer_signal)
	{
		signal_pending = i;
		return;
	}
	if (signalL == NULL || signal_count == SIGNAL_QUEUE_MAX)
		return;
	defer_signal++;
	/* Queue signals */
	signals[signal_count] = i;
	signal_count ++;
	old_hook = lua_gethook(signalL);
	old_mask = lua_gethookmask(signalL);
	old_count = lua_gethookcount(signalL);
	lua_sethook(signalL, sig_handle, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
	defer_signal--;
	/* re-raise any pending signals */
	if (defer_signal == 0 && signal_pending != 0) {
		raise (signal_pending);
		signal_pending = 0;
	}
}

static int sig_handler_wrap (lua_State *L)
{
	int sig = luaL_checkinteger(L, lua_upvalueindex(1));
	void (*handler)(int) = lua_touserdata(L, lua_upvalueindex(2));
	handler(sig);
	return 0;
}

/*
** ret = signal.signal(sig, handler[, flags])
** ret could be func/'default'/'ignore'
** handler could be func/'default'/'ignore'
*/
static int lsignal_signal (lua_State *L)
{
	struct sigaction sa, oldsa;
	int sig = luaL_checkinteger(L, 1);
	int ret;
	void (*handler)(int) = sig_postpone;

	if (lua_type(L, 2) == LUA_TSTRING) {
		const char *val = luaL_checkstring(L, 2);
		if (strcmp(val, "ignore") == 0)
			handler = SIG_IGN;
		else if (strcmp(val, "default") == 0)
			handler = SIG_DFL;
		else
			handler = NULL;
	} else if (lua_type(L, 2) == LUA_TFUNCTION) {
		if (lua_tocfunction(L, 2) == sig_handler_wrap) {
			lua_getupvalue(L, 2, 1);
			handler = lua_touserdata(L, -1);
			lua_pop(L, 1);
		}
	} else {
		handler = NULL;
	}
		
	if (handler == NULL)
		luaL_error(L, "expected function/'ingore'/'default' for argument #2");
	
	/* Set up C signal handler, getting old handler */
	sa.sa_handler = handler;
	sa.sa_flags = luaL_optinteger(L, 3, 0);
	sigfillset(&sa.sa_mask);
	ret = sigaction(sig, &sa, &oldsa);
	if (ret == -1)
		return 0;

	/* Set Lua handler if necessary */
	if (handler == sig_postpone)
	{
		lua_pushlightuserdata(L, &signalL); /* We could use an upvalue, but we need this for sig_handle anyway. */
		lua_rawget(L, LUA_REGISTRYINDEX);
		lua_pushvalue(L, 1);
		lua_pushvalue(L, 2);
		lua_rawset(L, -3);
		lua_pop(L, 1);
	}

	/* Push old handler as result */
	if (oldsa.sa_handler == sig_postpone)
	{
		lua_pushlightuserdata(L, &signalL);
		lua_rawget(L, LUA_REGISTRYINDEX);
		lua_pushvalue(L, 1);
		lua_rawget(L, -2);
	} else if (oldsa.sa_handler == SIG_DFL)
		lua_pushstring(L, "default");
	else if (oldsa.sa_handler == SIG_IGN)
		lua_pushstring(L, "ignore");
	else
	{
		lua_pushinteger(L, sig);
		lua_pushlightuserdata(L, oldsa.sa_handler);
		lua_pushcclosure(L, sig_handler_wrap, 2);
	}
	return 1;
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
	LENUM(SIGVTALRM),
#ifdef SA_NOCLDSTOP
	LENUM(SA_NOCLDSTOP),
#endif
#ifdef SA_NOCLDWAIT
	LENUM(SA_NOCLDWAIT),
#endif
#ifdef SA_RESETHAND
	LENUM(SA_RESETHAND),
#endif
#ifdef SA_NODEFER
	LENUM(SA_NODEFER),
#endif

	LENUM_NULL,
};

static const luaL_Reg funcs[] = {
	{"raise", lsignal_raise},
	{"kill", lsignal_kill},
	{"alarm", lsignal_alarm},
	{"signal", lsignal_signal},
	{NULL, NULL},
};

int l_opensignal(lua_State *L)
{	
	signalL = L;
	
	lua_pushlightuserdata(L, &signalL);
	lua_newtable(L);
	lua_rawset(L, LUA_REGISTRYINDEX);
	
	signal(SIGCHLD, on_sigchld);
	signal(SIGPIPE, SIG_IGN);
	l_register_lib(L, "signal", funcs, enums);
	
	return 0;
}
