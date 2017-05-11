
/*
 * Copyright (C) spyder
 */

#include "common.h"

#define META_NAME			"ssl.context"

typedef struct _Object {
	SSL_CTX *ctx;
}Object;

#define getobj(L, idx)		((Object*)luaL_checkudata(L, idx, META_NAME))
#define getctx(L)			(getobj(L, 1)->ctx)

/*
** ctx = ssl.context.new("sslv23/sslv3/tlsv1")
*/
static int l_new(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	const SSL_METHOD *method;
	SSL_CTX *ctx;
	
	if (strcasecmp(str, "sslv3") == 0)
		method = SSLv3_method();
	else if (strcasecmp(str, "sslv23") == 0)
		method = SSLv23_method();
	else if (strcasecmp(str, "tlsv1") == 0)
		method = TLSv1_method();
	else
		luaL_error(L, "invalid arugment #1, should be sslv3/sslv23/tlsv1");
		
	ctx = SSL_CTX_new(method);
	if (ctx != NULL) {
		Object *obj = (Object*)lua_newuserdata(L, sizeof(Object));
		obj->ctx = ctx;
		luaL_getmetatable(L, META_NAME);
		lua_setmetatable(L, -2);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

/*
** err = ctx:load_verify_locations(cafile=nil, capath=nil)
** succeed if err == 0
*/
static int l_load_verify_locations(lua_State *L)
{
	SSL_CTX *ctx = getctx(L);
	const char *cafile = luaL_optstring(L, 2, NULL);
	const char *capath = luaL_optstring(L, 2, NULL);
	int err = 0;
	
	if (SSL_CTX_load_verify_locations(ctx, cafile, capath) != 1) 
		err = ERR_get_error();
		
	lua_pushinteger(L, err);
	return 1;
}

/*
** err = ctx:use_certfile(filepath)
*/
static int l_use_certfile(lua_State *L)
{
	SSL_CTX *ctx = getctx(L);
	const char *filename = luaL_checkstring(L, 2);
	int err = 0;
	
	if (SSL_CTX_use_certificate_chain_file(ctx, filename) != 1)
		err = ERR_get_error();
		
	lua_pushinteger(L, err);
	return 1;
}

static int passwd_cb(char *buf, int size, int flag, void *udata)
{
	lua_State *L = (lua_State*)udata;
	switch (lua_type(L, 3)) {
	case LUA_TFUNCTION:
		lua_pushvalue(L, 3);
		lua_call(L, 0, 1);
		if (lua_type(L, -1) != LUA_TSTRING)
			return 0;
		/* fallthrough */
	case LUA_TSTRING:
		strncpy(buf, lua_tostring(L, -1), size);
		buf[size - 1] = 0;
		return (int)strlen(buf);
	}
	return 0;
}

/*
** boolean = ctx:use_keyfile(filepath, passwd)
*/
static int l_use_keyfile(lua_State *L)
{
	SSL_CTX *ctx = getctx(L);
	const char *filename = luaL_checkstring(L, 2);
	int err = 0;
	
	switch (lua_type(L, 3)) {
	case LUA_TSTRING:
	case LUA_TFUNCTION:
		SSL_CTX_set_default_passwd_cb(ctx, passwd_cb);
		SSL_CTX_set_default_passwd_cb_userdata(ctx, L);
		/* fall through */
	case LUA_TNIL:
		if (SSL_CTX_use_PrivateKey_file(ctx, filename, SSL_FILETYPE_PEM) != 1)
			err = ERR_get_error();
		SSL_CTX_set_default_passwd_cb(ctx, NULL);
		SSL_CTX_set_default_passwd_cb_userdata(ctx, NULL);
		break;
	default:
		luaL_error(L, "invalid argument #3 for password");
	}
	
	lua_pushinteger(L, err);
	return 1;
}

/*
** err = ctx:set_cipher_list(cipher)
*/
static int l_set_cipher_list(lua_State *L)
{
	SSL_CTX *ctx = getctx(L);
	const char *list = luaL_checkstring(L, 2);
	int err = 0;
	
	if (SSL_CTX_set_cipher_list(ctx, list) != 1)
		err = ERR_get_error();
		
	lua_pushinteger(L, err);
	return 1;
}

/*
** err = ctx:set_verify(flags)
*/
static int l_set_verify(lua_State *L)
{
	SSL_CTX_set_verify(getctx(L), (int)luaL_checkinteger(L, 2), NULL);	
	lua_pushinteger(L, 0);
	return 1;
}

/*
** err = ctx:set_verify_depth(depth)
*/
static int l_set_verify_depth(lua_State *L)
{
	SSL_CTX_set_verify_depth(getctx(L), (int)luaL_checkinteger(L, 2));
	lua_pushinteger(L, 0);
	return 1;
}

/*
** err = ctx:set_options(options)
*/
static int l_set_options(lua_State *L)
{
	SSL_CTX_set_options(getctx(L), (int)luaL_checkinteger(L, 2));	
	lua_pushinteger(L, 0);
	return 1;
}

static const luaL_Reg methods[] = {
	{"new", l_new},
	{"load_verify_locations", l_load_verify_locations},
	{"use_certfile", l_use_certfile},
	{"use_keyfile", l_use_keyfile},
	{"set_cipher_list", l_set_cipher_list},
	{"set_verify_depth", l_set_verify_depth},
	{"set_verify", l_set_verify},
	{"set_options", l_set_options},
	{NULL, NULL},
};

static int l_gc(lua_State *L)
{
	Object *obj = getobj(L, 1);
	if (obj->ctx) {
		SSL_CTX_free(obj->ctx);
		obj->ctx = NULL;
	}
	return 0;
}

static int l_tostring(lua_State *L)
{
	Object *obj = getobj(L, 1);
	lua_pushfstring(L, "ssl.context: %p (ctx=%p)", obj, obj->ctx);
	return 1;
}

static const luaL_Reg meta_funcs[] = {
	{"__gc", l_gc},
	{"__tostring", l_tostring},
	{NULL, NULL},
};

SSL_CTX* l_getcontext(lua_State *L, int idx)
{
	return getobj(L, idx)->ctx;
}

int l_opencontext(lua_State *L)
{
	lua_newtable(L);  		
	luaL_setfuncs(L, methods, 0);
	
	luaL_newmetatable(L, META_NAME);
	luaL_setfuncs(L, meta_funcs, 0);
	lua_pushstring(L, "__index");
	lua_pushvalue(L, -3);
	lua_settable(L, -3);
	lua_pop(L, 1);
	
	return 1;
}
