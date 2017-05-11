
/*
 * Copyright (C) spyder
 */


#include "lstdimpl.h"
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>

typedef struct sockaddr SA;

typedef union {
	struct sockaddr any;
	struct sockaddr_in v4;
	struct sockaddr_in6 v6;
	struct sockaddr_un un;
}sockaddr_x;

#define MAX_ADDRSTRLEN				MAX(sizeof(((struct sockaddr_un*)0)->sun_path), INET6_ADDRSTRLEN)

#define SA_IS_IPV4(sa)				((sa).v4.sin_family == AF_INET)
#define SA_IS_IPV6(sa)				((sa).v4.sin_family == AF_INET6)
#define SA_SIZE(sa)					(SA_IS_IPV4(sa) ? sizeof((sa).v4) : (SA_IS_IPV6(sa) ? sizeof((sa).v6) : sizeof((sa).un)))

static int socket_socket(int domain, int type, int protocol, int *fd)
{
	*fd = socket(domain, type, protocol);
    return *fd >= 0 ? 0 : errno;
}

static bool sa_build(const char *addr, int port, sockaddr_x *sa)
{
	if (strchr(addr, '/') != NULL) {
		sa->un.sun_family = AF_UNIX;
		strncpy(sa->un.sun_path, addr, sizeof(sa->un.sun_path) - 1);
	} else if (strchr(addr, ':') != NULL) {
		sa->v6.sin6_family = AF_INET6;
		if (inet_pton(AF_INET6, addr, &sa->v6.sin6_addr) != 1)
			return false;
		sa->v6.sin6_port = htons((uint16)port);
		sa->v6.sin6_flowinfo = 0;
		sa->v6.sin6_scope_id = 0;
	} else {
		sa->v4.sin_family = AF_INET;
		if (inet_pton(AF_INET, addr, &sa->v4.sin_addr) != 1)
			return false;
		sa->v4.sin_port = htons((uint16)port);
	}
	return true;
}

static bool sa_parse(const sockaddr_x *sa, char *addr, int *port)
{
	int af = sa->v4.sin_family;

	if (af == AF_INET) {
		inet_ntop(af, &sa->v4.sin_addr, addr, INET6_ADDRSTRLEN);
		*port = (int)ntohs((uint16)sa->v4.sin_port);
	} else if (af == AF_UNIX) {
		strncpy(addr, sa->un.sun_path, MAX_ADDRSTRLEN - 1);
		*port = 0;
	} else {
		inet_ntop(af, &sa->v6.sin6_addr, addr, INET6_ADDRSTRLEN);
		*port = (int)ntohs((uint16)sa->v6.sin6_port);
	}

	return true;
}

int socket_shutdown(int fd, int how)
{
    shutdown(fd, how);
    return 0;
}

int socket_connect(int fd, const char *addr, int port)
{
    int err = 0;
    sockaddr_x sa;
    if (!sa_build(addr, port, &sa)) {
    	err = EFAULT;
    } else if (connect(fd, (SA*)&sa, SA_SIZE(sa)) < 0) {
    	err = errno;
    }
    return err;
}

int socket_bind(int fd, const char *addr, int port)
{
    int err = 0;
    sockaddr_x sa;
    if (!sa_build(addr, port, &sa)) {
    	err = EFAULT;
    } else {
		if (bind(fd, (SA*)&sa, SA_SIZE(sa)) < 0) {
			err = errno;
		}
    }
    return err;
}

int socket_listen(int fd, int backlog)
{
    int err = 0;
	if (backlog <= 0) {
		backlog = SOMAXCONN;
	}

    if (listen(fd, backlog) < 0) {
    	err = errno;
    }
    return err;
}

int socket_accept(int fd, int *peer, char *addr, int *port)
{
	sockaddr_x sa;
	socklen_t len = (socklen_t)sizeof(sa);
	*peer = accept(fd, (SA*)&sa, &len);
	if (*peer >= 0) {
		if (addr != NULL || port != NULL)
			sa_parse(&sa, addr, port);
		return 0;
	} else {
		return errno;
	}
}

int socket_recvfrom(int fd, uint8 *data, size_t count, int flags, char *addr, int *port, size_t *got)
{
    int nrecv;
    sockaddr_x sa;
    socklen_t len = (socklen_t)sizeof(sa);

    *got = 0;
    nrecv = recvfrom(fd, data, count, flags, (SA*)&sa, &len);
    if (nrecv >= 0) {
    	*got = nrecv;
    	sa_parse(&sa, addr, port);
    	return 0;
    } else {
    	return errno;
    }
}

int socket_sendto(int fd, const void *data, size_t count, int flags, const char *addr, int port, size_t *sent)
{
    ssize_t nsend;
    sockaddr_x sa;

    *sent = 0;
    if (!sa_build(addr, port, &sa)) {
		return EFAULT;
    } else {
		nsend = sendto(fd, data, (int)count, flags, (SA*)&sa, (socklen_t)SA_SIZE(sa));
		if (nsend >= 0) {
			*sent = nsend;
			return 0;
		} else {
			return errno;
		}
    }
}

int socket_getpeername(int fd, char *addr, int *port)
{
	sockaddr_x sa;
	socklen_t len = (socklen_t)sizeof(sa);
	if (getpeername(fd, (SA*)&sa, &len) == 0) {
		sa_parse(&sa, addr, port);
		return 0;
	} else {
		return errno;
	}
}

int socket_getsockname(int fd, char *addr, int *port)
{
	sockaddr_x sa;
	socklen_t len = (socklen_t)sizeof(sa);
	if (getsockname(fd, (SA*)&sa, &len) == 0) {
		sa_parse(&sa, addr, port);
		return 0;
	} else {
		return errno;
	}
}

/*
** fd, err = socket.socket(af, socktype, protocol=nil)
*/
static int lsocket_socket(lua_State *L)
{
	int fd, err;
	err = socket_socket(
		(int)luaL_checkinteger(L, 1),
		(int)luaL_checkinteger(L, 2),
		(int)luaL_optinteger(L, 3, 0), &fd);

	lua_pushinteger(L, err == 0 ? fd : -1);
	lua_pushinteger(L, err);
	return 2;
}

/*
** err = socket.connect(fd, addr, port)
*/
static int lsocket_connect(lua_State *L)
{
	int fd = (int)luaL_checkinteger(L, 1);
	const char *addr = luaL_checkstring(L, 2);
	int port = (int)luaL_optinteger(L, 3, 0);
	int err;
	do {
		err = socket_connect(fd, addr, port);
		if (err == 0 || err != EINTR)
			break;
	} while (1);
	lua_pushinteger(L, err);
	return 1;
}

/*
** err = socket.bind(fd, addr, port)
*/
static int lsocket_bind(lua_State *L)
{
	int err = socket_bind(
		(int)luaL_checkinteger(L, 1),
		luaL_checkstring(L, 2),
		(int)luaL_optinteger(L, 3, 0));
	lua_pushinteger(L, err);
	return 1;
}

/*
** err = socket.listen(fd, backlog=socket.SOMAXCONN)
*/
static int lsocket_listen(lua_State *L)
{
	int fd = (int)luaL_checkinteger(L, 1);
	int backlog = (int)luaL_optinteger(L, 1, SOMAXCONN);
	int err = socket_listen(fd, backlog);
	lua_pushinteger(L, err);
	return 1;
}

/*
** fd, addr, port, err = socket.accept(fd)
*/
static int lsocket_accept(lua_State *L)
{
	int fd = (int)luaL_checkinteger(L, 1);
	int peerfd;
	char addr[MAX_ADDRSTRLEN];
	int port;
	int err = socket_accept(fd, &peerfd, addr, &port);
	if (err == 0) {
		lua_pushinteger(L, peerfd);
		lua_pushstring(L, addr);
		lua_pushinteger(L, port);
	} else {
		lua_pushnil(L);
		lua_pushnil(L);
		lua_pushnil(L);
	}
	lua_pushinteger(L, err);
	return 4;
}

/*
** str/nil, addr/nil, port/nil, err = socket.recvfrom(fd)
**
** won't block even if the fd is working in blocking mode.
*/
static int lsocket_recvfrom(lua_State *L) {
	int fd = (int)luaL_checkinteger(L, 1);
	char addr[MAX_ADDRSTRLEN];
	int port;
	Buffer buf;
	size_t navaiable = 0;
	int err = 0;

	buffer_init(&buf, 0);
	err = os_getnread(fd, &navaiable);
	if (navaiable > 0) {
		uint8 *p = buffer_grow(&buf, (size_t)navaiable);
		size_t nread = 0;
		err = socket_recvfrom(fd, p, navaiable, 0, addr, &port, &nread);

		if (nread < navaiable)
			buffer_pop(&buf, navaiable - nread);
	}

	if (err != 0 || navaiable == 0) {
		lua_pushnil(L);
		lua_pushnil(L);
		lua_pushnil(L);
		lua_pushinteger(L, err);
	} else {
		lua_pushlstring(L, (const char*)buf.data, buf.datasiz);
		lua_pushstring(L, addr);
		lua_pushinteger(L, port);
		lua_pushinteger(L, 0);
	}
	buffer_finalize(&buf);

	return 4;
}

/*
** nread, addr/nil, port/nil, err = socket.recvfromb(fd, buffer)
**
** won't block even if the fd is working in blocking mode.
*/
static int lsocket_recvfromb(lua_State *L) {
	int fd = (int)luaL_checkinteger(L, 1);
	Buffer *buf = buffer_lcheck(L, 2);
	char addr[MAX_ADDRSTRLEN];
	int port;
	size_t navaiable = 0, nread = 0;
	int err = 0;

	err = os_getnread(fd, &navaiable);
	if (navaiable > 0) {
		uint8 *p = buffer_grow(buf, (size_t)navaiable);
		err = socket_recvfrom(fd, p, navaiable, 0, addr, &port, &nread);

		if (nread < navaiable)
			buffer_pop(buf, navaiable - nread);
	}

	if (err != 0 || navaiable == 0) {
		lua_pushinteger(L, 0);
		lua_pushnil(L);
		lua_pushnil(L);
		lua_pushinteger(L, err);
	} else {
		lua_pushinteger(L, nread);
		lua_pushstring(L, addr);
		lua_pushinteger(L, port);
		lua_pushinteger(L, 0);
	}

	return 4;
}

/*
** nwritten, err = socket.sendto(fd, addr, port, string)
*/
static int lsocket_sendto(lua_State *L)
{
	size_t nwritten = 0;
	size_t len;
	const char *str = luaL_checklstring(L, 4, &len);
	int err = socket_sendto((int)luaL_checkinteger(L, 1), str, len, 0, luaL_checkstring(L, 2), (int)luaL_checkinteger(L, 3), &nwritten);

	lua_pushinteger(L, nwritten);
	lua_pushinteger(L, err);
	return 2;
}

/*
** nwritten, err = socket.sendtob(fd, addr, port, buf, offset=0, len=#buf-offset)
*/
static int lsocket_sendtob(lua_State *L)
{
	Buffer *buffer = buffer_lcheck(L, 4);
	size_t offset = (size_t)(int)luaL_optinteger(L, 5, 0);
	size_t len = 0;
	size_t nwritten = 0;
	int err = 0;

	if (lua_gettop(L) >= 6)
		len = (size_t)(int)luaL_checkinteger(L, 6);
	else if (offset <= buffer->datasiz)
		len = buffer->datasiz - offset;

	if (offset <= buffer->datasiz && (offset + len) <= buffer->datasiz)
		err = socket_sendto((int)luaL_checkinteger(L, 1),
					buffer->data + offset, len, 0,
					luaL_checkstring(L, 2), (int)luaL_checkinteger(L, 3),
					&nwritten);

	lua_pushinteger(L, (int)nwritten);
	lua_pushinteger(L, err);

	return 2;
}

/*
** addr, port, err = socket.getpeername(fd)
*/
static int lsocket_getpeername(lua_State *L)
{
	int fd = luaL_checkinteger(L, 1);
	char addr[INET6_ADDRSTRLEN];
	int port = 0;
	int err = socket_getpeername(fd, addr, &port);
	if (err == 0) {
		lua_pushstring(L, addr);
		lua_pushinteger(L, port);
	} else {
		lua_pushnil(L);
		lua_pushnil(L);
	}
	lua_pushinteger(L, err);
	return 3;
}

/*
** addr, port, err = socket.getsockname(fd)
*/
static int lsocket_getsockname(lua_State *L)
{
	int fd = luaL_checkinteger(L, 1);
	char addr[INET6_ADDRSTRLEN];
	int port = 0;
	int err = socket_getsockname(fd, addr, &port);
	if (err == 0) {
		lua_pushstring(L, addr);
		lua_pushinteger(L, port);
	} else {
		lua_pushnil(L);
		lua_pushnil(L);
	}
	lua_pushinteger(L, err);
	return 3;
}

/*
** err = socket.setsocketopt(fd, optname, optvalue)
*/
static int lsocket_setsocketopt(lua_State *L)
{
	int fd = (int)luaL_checkinteger(L, 1);
	int optname = (int)luaL_checkinteger(L, 2);
	union {
		int b_val;
		int i_val;
		struct timeval tv_val;
		struct linger lg_val;
	}optval;
	socklen_t optval_len;
	int err = 0;
	switch (optname) {
		case SO_BROADCAST:
		case SO_DONTROUTE:
		case SO_KEEPALIVE:
		case SO_OOBINLINE:
		case SO_REUSEADDR:
		case SO_DEBUG: {
			optval.b_val = lua_toboolean(L, 3);
			optval_len = 4;
			break;
		}
		case SO_TYPE:
		case SO_ERROR:
		case SO_RCVBUF:
		case SO_SNDBUF:
		case SO_RCVLOWAT:
		case SO_SNDLOWAT: {
			optval.i_val = lua_tointeger(L, 3);
			optval_len = 4;
			break;
		}
		case SO_RCVTIMEO:
		case SO_SNDTIMEO: {
			lua_Number n = lua_tonumber(L, 3);
			optval.tv_val.tv_sec = (int)n;
			optval.tv_val.tv_usec = (n - optval.tv_val.tv_sec) * 1000000;
			optval_len = sizeof(optval.tv_val);
			break;
		}
		case SO_LINGER: {
			bool valid_arg = false;
			if (lua_type(L, 3) == LUA_TTABLE) {
				lua_rawgeti(L, 3, 1);
				lua_rawgeti(L, 3, 2);
				if (lua_type(L, -2) == LUA_TBOOLEAN && lua_type(L, -1) == LUA_TNUMBER)
					valid_arg = true;
			}
			if (valid_arg) {
				optval.lg_val.l_onoff = lua_toboolean(L, -2);
				optval.lg_val.l_linger = (uint32)lua_tointeger(L, -1);
				optval_len = sizeof(struct linger);
			} else {
				err = EINVAL;
			}
			break;
		}
		default: {
			err = EINVAL;
			break;
		}
	}
	if (err == 0) {
		err = setsockopt(fd, SOL_SOCKET, optname, (const void*)&optval, optval_len);
		if (err < 0) {
			err = errno;
		}
	}
	lua_pushinteger(L, err);
	return 1;
}

/*
** ..., err = socket.getsocketopt(fd, optname)
*/
static int lsocket_getsocketopt(lua_State *L)
{
	int fd = luaL_checkinteger(L, 1);
	int optname = luaL_checkinteger(L, 2);
	union {
		int b_val;
		int i_val;
		struct timeval tv_val;
		struct linger lg_val;
	}optval;
	socklen_t optval_len = sizeof(optval);
	int err = 0, ret = 0;
	switch (optname) {
		case SO_BROADCAST:
		case SO_DONTROUTE:
		case SO_KEEPALIVE:
		case SO_OOBINLINE:
		case SO_REUSEADDR:
		case SO_DEBUG: {
			err = getsockopt(fd, SOL_SOCKET, optname, (void*)&optval, &optval_len);
			if (err == 0) {
				lua_pushboolean(L, optval.b_val);
				ret = 1;
			}
			break;
		}
		case SO_TYPE:
		case SO_ERROR:
		case SO_RCVBUF:
		case SO_SNDBUF:
		case SO_RCVLOWAT:
		case SO_SNDLOWAT: {
			err = getsockopt(fd, SOL_SOCKET, optname, (void*)&optval, &optval_len);
			if (err == 0) {
				lua_pushinteger(L, optval.i_val);
				ret = 1;
			}
			break;
		}
		case SO_RCVTIMEO:
		case SO_SNDTIMEO: {
			err = getsockopt(fd, SOL_SOCKET, optname, (void*)&optval, &optval_len);
			if (err == 0) {
				lua_Number n = optval.tv_val.tv_sec;
				n += optval.tv_val.tv_usec / 1000000.0;
				lua_pushnumber(L, n);
				ret = 1;
			}
			break;
		}
		case SO_LINGER: {
			err = getsockopt(fd, SOL_SOCKET, optname, &optval, &optval_len);
			if (err == 0) {
				lua_createtable(L, 2, 0);
				lua_pushboolean(L, optval.lg_val.l_onoff);
				lua_rawseti(L, -2, 1);
				lua_pushinteger(L, optval.lg_val.l_linger);
				lua_rawseti(L, -2, 2);
				ret = 1;
			}
			break;
		}
		default: {
			err = EINVAL;
			break;
		}
	}
	if (err != 0) {
		lua_pushnil(L);
		lua_pushinteger(L, err < 0  ? errno : err);
		return 2;
	} else {
		lua_pushinteger(L, 0);
		return ret + 1;
	}
}

/*
** result, err = socket.setipopt(fd, optname, optval)
*/
static int lsocket_setipopt(lua_State *L)
{
	int fd = (int)luaL_checkinteger(L, 1);
	int optname = (int)luaL_checkinteger(L, 2);
	union {
		int i_val;
		bool b_val;
	}optval;
	socklen_t optval_len = 0;
	int err = 0;
	switch (optname) {
#if (defined IP_TRANSPARENT && defined IP_FREEBIND)
		case IP_TRANSPARENT:
		case IP_FREEBIND: {
			optval.b_val = lua_toboolean(L, 3) ? 1 : 0;
			optval_len = 4;
			break;
		}
#endif
		default: {
			err = EINVAL;
			break;
		}
	}
	if (optval_len > 0) {
		err = setsockopt(fd, SOL_IP, optname, (const void*)&optval, optval_len);
		if (err < 0) {
			err = errno;
		}
	}
	lua_pushinteger(L, err);
	return 1;
}

/*
** result, err = socket.getipopt(fd, optname, optval)
*/
static int lsocket_getipopt(lua_State *L)
{
	int fd = luaL_checkinteger(L, 1);
	int optname = luaL_checkinteger(L, 2);
	union {
		int i_val;
		bool b_val;
	}optval;
	socklen_t optval_len = sizeof(optval);
	int err = 0, ret = 0;
	switch (optname) {
#if (defined IP_TRANSPARENT && defined IP_FREEBIND)
		case IP_TRANSPARENT:
		case IP_FREEBIND: {
			err = getsockopt(fd, SOL_IP, optname, (void*)&optval, &optval_len);
			if (err == 0) {
				lua_pushboolean(L, optval.b_val);
				ret = 1;
			}
			break;
		}
#endif
		default: {
			err = EINVAL;
			break;
		}
	}
	if (err != 0) {
		lua_pushnil(L);
		lua_pushinteger(L, err < 0 ? errno : EINVAL);
		return 2;
	} else {
		lua_pushinteger(L, 0);
		return ret + 1;
	}
}

/*
** result, err = socket.settcpopt(fd, optname, optval)
*/
static int lsocket_settcpopt(lua_State *L)
{
	int fd = (int)luaL_checkinteger(L, 1);
	int optname = (int)luaL_checkinteger(L, 2);
	union {
		int i_val;
		bool b_val;
	}optval;
	socklen_t optval_len = 0;
	int err = 0;
	switch (optname) {
	case TCP_KEEPCNT:
	case TCP_KEEPINTVL:
	case TCP_KEEPIDLE: {
			optval.i_val = lua_tointeger(L, 3);
			optval_len = 4;
			break;
		}
	case TCP_NODELAY: {
			optval.b_val = lua_toboolean(L, 3);
			optval_len = 4;
			break;
		}
	default: {
			err = EINVAL;
			break;
		}
	}
	if (optval_len > 0) {
		err = setsockopt(fd, SOL_TCP, optname, (const void*)&optval, optval_len);
		if (err < 0) {
			err = errno;
		}
	}
	lua_pushinteger(L, err);
	return 1;
}

/*
** result, err = socket.gettcpopt(fd, optname)
*/
static int lsocket_gettcpopt(lua_State *L)
{
	int fd = luaL_checkinteger(L, 1);
	int optname = luaL_checkinteger(L, 2);
	union {
		int i_val;
		bool b_val;
	}optval;
	socklen_t optval_len = sizeof(optval);
	int err = 0, ret = 0;
	switch (optname) {
	case TCP_KEEPIDLE:
	case TCP_KEEPCNT:
	case TCP_KEEPINTVL: {
			err = getsockopt(fd, SOL_TCP, optname, (void*)&optval, &optval_len);
			if (err == 0) {
				lua_pushinteger(L, optval.i_val);
				ret = 1;
			}
			break;
		}
	case TCP_NODELAY: {
			err = getsockopt(fd, SOL_TCP, optname, (void*)&optval, &optval_len);
			if (err == 0) {
				lua_pushboolean(L, optval.b_val);
				ret = 1;
			}
			break;
		}
	default: {
			err = EINVAL;
			break;
		}
	}
	if (err < 0 || err == EINVAL) {
		lua_pushnil(L);
		lua_pushinteger(L, err < 0 ? errno : EINVAL);
		return 2;
	} else {
		lua_pushinteger(L, 0);
		return ret + 1;
	}
}

/*
** err = sock.shutdown(fd, shut=sock.SHUT_RDWR)
*/
static int lsocket_shutdown(lua_State *L)
{
	int fd = (int)luaL_checkinteger(L, 1);
	int how = (int)luaL_optinteger(L, 2, SHUT_RDWR);
	int err = socket_shutdown(fd, how);
	lua_pushinteger(L, err);
	return 1;
}

static const luaL_Reg funcs[] = {
	{"socket", lsocket_socket},
	{"connect", lsocket_connect},
	{"bind", lsocket_bind},
	{"listen", lsocket_listen},
	{"accept", lsocket_accept},
	{"recvfrom", lsocket_recvfrom},
	{"recvfromb", lsocket_recvfromb},
	{"sendto", lsocket_sendto},
	{"sendtob", lsocket_sendtob},
	{"setsocketopt", lsocket_setsocketopt},
	{"getsocketopt", lsocket_getsocketopt},
	{"setipopt", lsocket_setipopt},
	{"getipopt", lsocket_getipopt},
	{"settcpopt", lsocket_settcpopt},
	{"gettcpopt", lsocket_gettcpopt},
	{"getpeername", lsocket_getpeername},
	{"getsockname", lsocket_getsockname},
	{"shutdown", lsocket_shutdown},
	{NULL, NULL}
};

static const EnumReg enums[] = {
	/* SOCKET */
	LENUM(AF_INET),
	LENUM(AF_INET6),
	LENUM(AF_UNIX),
	LENUM(AF_PACKET),
	LENUM(AF_UNSPEC),
	LENUM(SOCK_STREAM),
	LENUM(SOCK_DGRAM),
	LENUM(SOCK_RAW),
	LENUM(SOCK_SEQPACKET),
	LENUM(SOL_SOCKET),
	LENUM(SOL_IP),
	LENUM(SOL_TCP),
	LENUM(SO_ACCEPTCONN),
	LENUM(SO_BROADCAST),
	LENUM(SO_DEBUG),
	LENUM(SO_DONTROUTE),
	LENUM(SO_ERROR),
	LENUM(SO_KEEPALIVE),
	LENUM(SO_LINGER),
	LENUM(SO_OOBINLINE),
	LENUM(SO_RCVBUF),
	LENUM(SO_RCVLOWAT),
	LENUM(SO_RCVTIMEO),
	LENUM(SO_REUSEADDR),
	LENUM(SO_SNDBUF),
	LENUM(SO_SNDLOWAT),
	LENUM(SO_SNDLOWAT),
	LENUM(TCP_NODELAY),
	LENUM(TCP_KEEPIDLE),
	LENUM(TCP_KEEPCNT),
	LENUM(TCP_KEEPINTVL),
#ifdef IP_FREEBIND
	LENUM(IP_FREEBIND),
#endif
#ifdef IP_TRANSPARENT
	LENUM(IP_TRANSPARENT),
#endif
	LENUM(IPPROTO_IP),
	LENUM(IPPROTO_IPV6),
	LENUM(IPPROTO_ICMP),
	LENUM(IPPROTO_UDP),
	LENUM(IPPROTO_TCP),
	LENUM(IPPROTO_RAW),
	LENUM(SO_TYPE),
	LENUM(SOMAXCONN),
	LENUM(MSG_OOB),
	LENUM(MSG_PEEK),
	LENUM(MSG_DONTROUTE),
	LENUM(SHUT_RD),
	LENUM(SHUT_WR),
	LENUM(SHUT_RDWR),
	LENUM_NULL
};

int l_opensocket(lua_State *L)
{
	l_register_lib(L, "socket", funcs, enums);
	return 0;
}
