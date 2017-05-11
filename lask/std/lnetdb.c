
/*
 * Copyright (C) spyder
 */


#include "lstdimpl.h"
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

/*
** [offical-name, [alias-name1, ... , alias-nameN], [addr1, ... , addrN]]/nil, err = sys.gethostbyname(name)
*/
static int lnetdb_gethostbyname(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);
	struct hostent *h = gethostbyname(name);

	if (h != NULL) {
		int num;
		char addr_str[60];

		lua_createtable(L, 3, 0);
		lua_pushstring(L, h->h_name);
		lua_rawseti(L, -2, 1);

		num = 0;
		for(char **pptr = h->h_aliases; *pptr != NULL; pptr++)
			num++;
		lua_createtable(L, num, 0);
		num = 1;
		for(char **pptr = h->h_aliases; *pptr != NULL; pptr++) {
			lua_pushstring(L, *pptr);
			lua_rawseti(L, -2, num);
			num++;
		}
		lua_rawseti(L, -2, 2);

		num = 0;
		for (char **pptr = h->h_addr_list; *pptr != NULL; pptr++)
			num++;
		lua_createtable(L, num, 0);
		num = 1;
		for (char **pptr = h->h_addr_list; *pptr != NULL; pptr++) {
			inet_ntop(h->h_addrtype, *pptr, addr_str, sizeof(addr_str));
			lua_pushstring(L, addr_str);
			lua_rawseti(L, -2, num);
			num++;
		}
		lua_rawseti(L, -2, 3);
        lua_pushinteger(L, 0);
	} else {
		lua_pushnil(L);
		lua_pushinteger(L, h_errno);
	}
	return 2;
}

/*
** {"xxx.xxx.xxx.xxx"/"xx:xx:xx:xx:xx:xx", ...}/nil, err = netdb.getaddrbyname(name)
*/
static int lnetdb_getaddrbyname(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);
	struct hostent *h = gethostbyname(name);

	if (h != NULL) {
		int num = 0;

		for (char **pptr = h->h_addr_list; *pptr != NULL; pptr++)
			num++;

		if (num > 0) {
			char addr_str[60];
			lua_createtable(L, num, 0);
			num = 1;
			for (char **pptr = h->h_addr_list; *pptr != NULL; pptr++) {
				inet_ntop(h->h_addrtype, *pptr, addr_str, sizeof(addr_str));
				lua_pushstring(L, addr_str);
				lua_rawseti(L, -2, num);
				num++;
			}
		} else {
			lua_pushnil(L);
		}
		lua_pushinteger(L, 0);
	} else {
		lua_pushnil(L);
		lua_pushinteger(L, h_errno);
	}
	return 2;
}

/*
** str = netdb.strerror(err)
*/
static int lnetdb_strerror(lua_State *L)
{
	int err = (int)luaL_checkinteger(L, 1);
	lua_pushstring(L, gai_strerror(err));
	return 1;
}


static const luaL_Reg funcs[] = {
	{"gethostbyname", lnetdb_gethostbyname},
	{"getaddrbyname", lnetdb_getaddrbyname},
	{"strerror", lnetdb_strerror},
	{NULL, NULL},
};

static const EnumReg enums[] = {
	LENUM(HOST_NOT_FOUND),
	LENUM(TRY_AGAIN),
	LENUM(NO_RECOVERY),
	LENUM(NO_RECOVERY),
	LENUM(EAI_AGAIN),
	LENUM(EAI_BADFLAGS),
	LENUM(EAI_FAIL),
	LENUM(EAI_FAMILY),
	LENUM(EAI_MEMORY),
	LENUM(EAI_NONAME),
	LENUM(EAI_OVERFLOW),
	LENUM(EAI_SERVICE),
	LENUM(EAI_SOCKTYPE),
	LENUM(EAI_SYSTEM),
	LENUM_NULL,
};

int l_opennetdb(lua_State *L)
{
	l_register_lib(L, "netdb", funcs, enums);
	return 0;
}
