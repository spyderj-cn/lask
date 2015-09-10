
/*
 * Copyright (C) Spyderj
 */


/*
** Three ways to watch file descriptor events:
** 1. the 'waitfd' function, which can only watch one file descriptor's event
** 2. the 'select' function, which is suitable to watch a couple of file descriptors.
** 3. the most powerfull 'create/add/mod/del/wait/destroy' function group, can watch thousands or more.
*/

#include "lstdimpl.h"
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/select.h>

/*
** poll_fd, err = poll.create()
*/
static int lpoll_create(lua_State *L) 
{
	int poll_fd = epoll_create1(EPOLL_CLOEXEC);
	lua_pushinteger(L, poll_fd);
	lua_pushinteger(L, errno);
	return 2;
}

/*
** err = poll.add(poll_fd, fd, events)
*/
static int lpoll_add(lua_State *L) 
{
	int poll_fd = luaL_checkinteger(L, 1);
	struct epoll_event evt;
	
	evt.data.fd = luaL_checkinteger(L, 2);
	evt.events = (unsigned int)luaL_checkint(L, 3);
	if (epoll_ctl(poll_fd, EPOLL_CTL_ADD, evt.data.fd, &evt) == 0) {
		lua_pushinteger(L, 0);
	} else {
		lua_pushinteger(L, errno);
	}
	return 1;
}

/*
** err = poll.mod(poll_fd, fd, events)
*/
static int lpoll_mod(lua_State *L) 
{
	int poll_fd = luaL_checkinteger(L, 1);
	struct epoll_event evt;
	
	evt.data.fd = luaL_checkinteger(L, 2);
	evt.events = (unsigned int)luaL_checkint(L, 3);
	if (epoll_ctl(poll_fd, EPOLL_CTL_MOD, evt.data.fd, &evt) == 0) {
		lua_pushinteger(L, 0);
	} else {
		lua_pushinteger(L, errno);
	}
	return 1;
}

/*
** err = poll.del(poll_fd, fd)
*/
static int lpoll_del(lua_State *L) 
{
	int poll_fd = luaL_checkinteger(L, 1);
	int fd = luaL_checkinteger(L, 2);
	
	if (epoll_ctl(poll_fd, EPOLL_CTL_DEL, fd, NULL) == 0) {
		lua_pushinteger(L, 0);
	} else {
		lua_pushinteger(L, errno);
	}
	return 1;
}

/*
** 	{fdi = eventsi, ... }/nil, err = poll.wait(poll_fd, timeo=-1)
*/
static int lpoll_wait(lua_State *L) 
{
	int poll_fd = luaL_checkinteger(L, 1);
	struct epoll_event events[256];
	int timeout = -1;
	
	if (lua_gettop(L) >= 2)
		timeout = (int)(lua_tonumber(L, 2) * 1000);
	
	int n = epoll_wait(poll_fd, events, lengthof(events), timeout);
	if (n <= 0) {
		lua_pushnil(L);
		lua_pushinteger(L, n ? errno : 0);
	} else {
		int i = 0;
		lua_createtable(L, 0, n);
		for (i = 0; i < n; i++) {
			if (events[i].events & (EPOLLERR | EPOLLHUP))
				events[i].events |= EPOLLIN;
				
			lua_pushinteger(L, events[i].data.fd);
			lua_pushinteger(L, events[i].events);
			lua_settable(L, -3);
		}
		lua_pushinteger(L, 0);
	}
	return 2;
}

/*
** n_walked, err = poll.walk(timeo, function (fd, revents) ... end)
*/
static int lpoll_walk(lua_State *L) 
{
	int poll_fd = luaL_checkinteger(L, 1);
	struct epoll_event events[256];
	int timeout = -1;
	
	if (lua_gettop(L) >= 2) 
		timeout = (int)(lua_tonumber(L, 2) * 1000);
	
	if (lua_type(L, 3) != LUA_TFUNCTION) {
		return luaL_error(L, "expecting type function for argument #3");
	} else {
		int n = epoll_wait(poll_fd, events, lengthof(events), timeout);
		int i;
		for (i = 0; i < n; i++) {
			if (events[i].events & (EPOLLERR | EPOLLHUP))
				events[i].events |= EPOLLIN;
				
			lua_pushvalue(L, 3);
			lua_pushinteger(L, events[i].data.fd);
			lua_pushinteger(L, events[i].events);
			lua_pcall(L, 2, 1, -3);
			if (!lua_toboolean(L, -1)) {
				i++;
				lua_pop(L, 1);
				break;
			}
			lua_pop(L, 1);
		}
		lua_pushinteger(L, i);
		lua_pushinteger(L, n >= 0 ? 0 : errno);
	}
	return 2;
}

/*
** poll.destroy(poll_fd)
**
** since we are using epoll, this is exactly equal to os.close(poll_fd)
*/
static int lpoll_destroy(lua_State *L)
{
	close(luaL_checkinteger(L, 1));
	return 0;
}

/*
** {fdi, fdj ...}/nil, {...}/nil, {...}/nil, err = poll.select({fd1, fd2 ...}/nil, {...}/nil, {..}/nil, sec=-1)
*/
static int lpoll_select(lua_State *L) 
{
	fd_set fdset[3];
	int fdarray[FD_SETSIZE * 3], fdend[4], fdtotal = 0;
	int result;
	int maxfd = -1;
	int i;
	struct timeval tv_struct, *tv = NULL;
	lua_Number n = -1;
	
	fdend[0] = 0;
	fdend[1] = 0;
	fdend[2] = 0;
	fdend[3] = 0;
	
	if (lua_gettop(L) > 3) {
		n = lua_tonumber(L, 4);
		if (n >= 0) {
			tv = &tv_struct;
			tv->tv_sec = (time_t)n;
			tv->tv_usec = (suseconds_t)((n - tv->tv_sec) * 1000000);
		}
	}
	
	for (i = 0; i < 3; i++) {
		FD_ZERO(&fdset[i]);
		if (lua_type(L, i + 1) == LUA_TTABLE) {
			size_t len = lua_objlen(L, i + 1);
			for (size_t j = 0; j < len; j++) {
				lua_rawgeti(L, i + 1, (int)j + 1);
				if (lua_type(L, -1) == LUA_TNUMBER) {
					int fd = (int)lua_tointeger(L, -1);
					fdarray[fdtotal++] = fd;
					FD_SET(fd, &fdset[i]);
					if (fd > maxfd)
						maxfd = fd;
				}
				lua_pop(L, 1);
			}
		}
		fdend[i + 1] = fdtotal;
	}
	result = select(maxfd + 1, &fdset[0], &fdset[1], &fdset[2], tv);
	if (result > 0) {
		int j = 0;
		for (i = 1; i <= 3; i++) {
			if (fdend[i] > fdend[i - 1]){
				int count = 0;
				for (; j < fdend[i]; j++) {
					if (FD_ISSET(fdarray[j], &fdset[i - 1])) {
						if (count == 0) 
							lua_newtable(L);
						
						count++;
						lua_pushinteger(L, fdarray[j]);
						lua_rawseti(L, -2, count);
					}
				}
			} else {
				lua_pushnil(L);
			}
		}
		lua_pushinteger(L, 0);
	} else {
		lua_pushnil(L);
		lua_pushnil(L);
		lua_pushnil(L);
		lua_pushinteger(L, result ? errno : 0);
	}
	return 4;
}

/*
** DEPRECATED
** boolean = poll.waitrfd(fd, sec=-1)
**
** wait read-event on a single file descriptor
*/
static int lpoll_waitrfd(lua_State *L)
{
	struct pollfd pfd = {
		.fd = luaL_checkint(L, 1),
		.events = POLLIN
	};
	int timeout = -1;
	
	if (lua_gettop(L) > 1)
		timeout = (int)(lua_tonumber(L, 2) * 1000);
	
	lua_pushboolean(L, poll(&pfd, 1, timeout) > 0);
	
	return 1;
}

/*
** revents = poll.waitfd(fd, events, sec=-1)
** 
** events/revents could be 'rw'/'r'/'w'
**
** defaulted to 'r' if events is nil
*/
static int lpoll_waitfd(lua_State *L)
{
	struct pollfd pfd = {
		.fd = luaL_checkint(L, 1),
		.events = 0
	};
	int timeout = -1;
	const char *str = luaL_optstring(L, 2, "r");
	
	if (strchr(str, 'r') != NULL)
		pfd.events |= POLLIN;
	if (strchr(str, 'w') != NULL)
		pfd.events |= POLLOUT;
		
	if (lua_gettop(L) >= 3)
		timeout = (int)(lua_tonumber(L, 3) * 1000);
	
	if (poll(&pfd, 1, timeout) > 0) {
		char ret[4] = {0};
		short revents = pfd.revents;
		
		if (revents & POLLHUP)
			revents |= POLLIN;
			
		if (revents & POLLERR) {
			revents |= POLLIN;
			revents |= POLLOUT;
		}
		
		if (revents & POLLIN)
			ret[0] = 'r';
		if (revents & POLLOUT)
			ret[ret[0] ? 1 : 0] = 'w';
			
		lua_pushstring(L, ret);
	} else {
		lua_pushnil(L);
	}
	
	return 1;
}

static const luaL_Reg funcs[] = {
	{"create", lpoll_create},
	{"add", lpoll_add},
	{"del", lpoll_del},
	{"mod", lpoll_mod},
	{"wait", lpoll_wait},
	{"walk", lpoll_walk},
	{"destroy", lpoll_destroy},
	{"select", lpoll_select},
	{"waitfd", lpoll_waitfd},
	{"waitrfd", lpoll_waitrfd}, /* DEPRECATED */
	{NULL, NULL}
};

static const EnumReg enums[] = {
	{"IN", EPOLLIN},
	{"OUT", EPOLLOUT},
	{"ERR", EPOLLERR},
	{"HUP", EPOLLHUP},
	{"PRI", EPOLLPRI},
	{"ET", EPOLLET},
	LENUM_NULL
};

int l_openpoll(lua_State *L) 
{
	l_register_lib(L, "poll", funcs, enums);
	return 0;
}
