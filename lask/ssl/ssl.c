
/*
 * Copyright (C) spyder
 */

#include "common.h"

#define OBJSIZ					512
#define RBUFSIZ 				(OBJSIZ - sizeof(Object))
#define MALLOC					malloc
#define FREE					free

typedef struct _Object {
	SSL *ssl;

	/* how many bytes are left unread(fetched from SSL but not delivered to the
	 application) in rbuf */
	int rleft;

	/* offset of the first unread byte */
	int roffset;

	/* we can't use stack buffer because every time we call SSL_read, we
	   have to give it exactly the same arguments */
	char rbuf[0];
}Object;

static int ssl_key = 0;

static Object* getobj_safe(lua_State *L, int pop)
{
	Object *obj;
	int fd = (int)luaL_checkinteger(L, 1);
	lua_pushlightuserdata(L, &ssl_key);
	lua_gettable(L, LUA_REGISTRYINDEX);
	lua_pushinteger(L, fd);
	lua_gettable(L, -2);
	obj = lua_touserdata(L, -1);
	lua_pop(L, pop);
	return obj;
}

static Object* getobj(lua_State *L)
{
	Object *obj = getobj_safe(L, 2);
	if (obj == NULL)
		luaL_error(L, "invalid fd for argument #1");
	return obj;
}

#define getssl(L)			getobj(L)->ssl

/*
** ok(boolean) = SSL.attach(fd, ctx)
*/
static int l_attach(lua_State *L)
{
	SSL_CTX *ctx = l_getcontext(L, 2);
	Object *obj = getobj_safe(L, 1);
	SSL *ssl;

	if (obj != NULL) {
		if (obj->ssl) {
			SSL_free(obj->ssl);
			obj->ssl = NULL;
		}
	}

	ssl = SSL_new(ctx);
	if (ssl != NULL) {
		if (obj == NULL) {
			obj = (Object*)MALLOC(OBJSIZ);
			lua_pushvalue(L, 1);
			lua_pushlightuserdata(L, obj);
			lua_settable(L, -3);
		}
		SSL_set_fd(ssl, lua_tointeger(L, 1));
		obj->ssl = ssl;
		obj->rleft = 0;
		obj->roffset = 0;
	}
	lua_pop(L, 1);  /* pop fd table */
	lua_pushboolean(L, ssl != NULL);
	return 1;
}

/*
** SSL.detach(fd)
*/
static int l_detach(lua_State *L)
{
	Object *obj = getobj_safe(L, 1);
	if (obj != NULL) {
		if (obj->ssl != NULL)
			SSL_free(obj->ssl);
		FREE(obj);

		lua_pushvalue(L, 1);
		lua_pushnil(L);
		lua_settable(L, -3);
	}
	lua_pop(L, 1);
	return 0;
}

/*
** boolean = ssl.isattached(fd)
*/
static int l_isattached(lua_State *L)
{
	lua_pushboolean(L, getobj_safe(L, 2) != NULL);
	return 1;
}

/*
** ssl.set_connect_state(fd)
*/
static int l_set_connect_state(lua_State *L)
{
	SSL_set_connect_state(getssl(L));
	return 0;
}

/*
** ssl.set_accept_state(fd)
*/
static int l_set_accept_state(lua_State *L)
{
	SSL_set_accept_state(getssl(L));
	return 0;
}

/*
** ssl.set_mode(fd, mode)
*/
static int l_set_mode(lua_State *L)
{
	SSL_set_mode(getssl(L), (long)luaL_checkinteger(L, 2));
	return 0;
}

/*
** ssl.get_mode(fd, mode)
*/
static int l_get_mode(lua_State *L)
{
	lua_pushnumber(L, (lua_Number)SSL_get_mode(getssl(L)));
	return 1;
}

/*
** err = SSL.do_handshake(fd)
*/
static int l_do_handshake(lua_State *L)
{
	SSL *ssl = getssl(L);
	lua_pushinteger(L, SSL_get_error(ssl, SSL_do_handshake(ssl)));
	return 1;
}

/*
** ret = SSL.pending(fd)
*/
static int l_pending(lua_State *L)
{
	Object *obj = getobj(L);
	lua_pushinteger(L, obj->rleft + SSL_pending(obj->ssl));
	return 1;
}

/*
** nread, err = os.readb(fd, buffer, nbytes=-1)
** read n bytes or all available(if n is nil) from fd and push them into buffer
** will never block even if fd is not nonblocking when n is nil
**
** return number of bytes read plus the error code
** (nread, err):
** 	(>0, 0)  		succeed
**  (==0, 0)  		no data available (together with readable-event means fd is half-closed).
** 	(==0, non-0)  	error on fd
**
*/
static int l_readb(lua_State *L)
{
	Object *obj = getobj(L);
	Buffer *buf = lua_touserdata(L, 2);
	int nreq = luaL_optinteger(L, 3, -1);
	int nread = 0;
	int err = 0;

	if (buf == NULL || buf->magic != BUFFER_MAGIC) {
		luaL_error(L, "expecting userdata<buffer> for argument #1");
	}

	if (obj->rleft > 0) {
		int len = obj->rleft;
		if (nreq > 0 && len > nreq)
			len = nreq;

		buffer_push(buf, (const uint8_t*)obj->rbuf + obj->roffset, len);
		nread += len;
		obj->rleft -= len;
		if (obj->rleft > 0)
			obj->roffset += len;
	}

	if (nread < nreq || nreq < 0) {
		do {
			int ret = SSL_read(obj->ssl, obj->rbuf, RBUFSIZ);
			if (ret > 0) {
				int len = ret;
				if (nreq > 0 && len > (nreq - nread))
					len = (nreq - nread);

				buffer_push(buf, (const uint8_t*)obj->rbuf, (size_t)len);
				nread += len;
				if (len < ret) {
					obj->roffset = len;
					obj->rleft = ret - len;
				}
			} else {
				err = SSL_get_error(obj->ssl, ret);

				if (err == SSL_ERROR_WANT_READ && nread > 0)
					err = 0;

				break;
			}
		} while (nreq < 0 || nread < nreq);
	}

	lua_pushinteger(L, nread);
	lua_pushinteger(L, err);
	return 2;
}

/*
** nwritten, err = ssl.writeb(fd, buffer/reader, offset=0, length=all)
**
** return number of bytes written, plus the error code
**
*/
static int l_writeb(lua_State *L)
{
	SSL *ssl = getssl(L);
	union {
		const Buffer *buffer;
		const Reader *reader;
	}ptr;
	const uint8_t *data = NULL;
	size_t datasiz = 0;
	size_t offset = (size_t)luaL_optinteger(L, 3, 0);
	size_t length = 0;
	int nwritten = 0, err = 0;

	ptr.buffer = (const Buffer*)lua_touserdata(L, 2);
	if (ptr.buffer->magic == BUFFER_MAGIC) {
		data = ptr.buffer->data;
		datasiz = ptr.buffer->datasiz;
	} else if (ptr.reader->magic == READER_MAGIC) {
		data = ptr.reader->data;
		datasiz = ptr.reader->datasiz;
	} else {
		luaL_error(L, "expecting string or userdata<buffer> or userdata<reader> for argument 2");
	}

	length = datasiz;
	if (lua_gettop(L) >= 4)
		length = (size_t)luaL_checkinteger(L, 4);

	if (offset >= datasiz)
		length = 0;
	else if ((offset + length) > datasiz)
		length = (datasiz - offset);

	if (length > 0) {
		int ret = SSL_write(ssl, data + offset, (int)length);
		if (ret > 0) {
			nwritten = ret;
		} else {
			err = SSL_get_error(ssl, ret);
		}
	}

	lua_pushinteger(L, nwritten);
	lua_pushinteger(L, err);
	return 2;
}

/*
** ret, err = ssl.shutdown(fd)
*/
static int l_shutdown(lua_State *L)
{
	SSL *ssl = getssl(L);
	int ret = SSL_shutdown(ssl);
	int err = 0;

	if (ret < 0)
		err = SSL_get_error(ssl, ret);

	lua_pushinteger(L, ret);
	lua_pushinteger(L, err);
	return 2;
}

/*
** str = ssl.get_error()
*/
static int l_get_error(lua_State *L)
{
	lua_pushinteger(L, ERR_get_error());
	return 1;
}

/*
** str = ssl.error_string(err=ssl.get_error())
*/
static int l_error_string(lua_State *L)
{
	int err = luaL_optinteger(L, 1, -1);
	char buf[1024];

	if (err < 0)
		err = ERR_get_error();

	ERR_error_string_n((unsigned long)err, buf, sizeof(buf));
	lua_pushstring(L, buf);
	return 1;
}

/*
** reason_str = ssl.reason_error_string(err=ssl.get_error())
*/
static int l_reason_error_string(lua_State *L)
{
	int err = luaL_optinteger(L, 1, -1);
	if (err < 0)
		err = ERR_get_error();
	lua_pushstring(L, ERR_reason_error_string((unsigned long)err));
	return 1;
}

static const luaL_Reg funcs[] = {
	{"attach", l_attach},
	{"detach", l_detach},
	{"isattached", l_isattached},

	{"set_connect_state", l_set_connect_state},
	{"set_accept_state", l_set_accept_state},
	{"get_mode", l_get_mode},
	{"set_mode", l_set_mode},

	{"do_handshake", l_do_handshake},
	{"pending", l_pending},
	{"readb", l_readb},
	{"writeb", l_writeb},
	{"shutdown", l_shutdown},

	{"get_error", l_get_error},
	{"error_string", l_error_string},
	{"reason_error_string", l_reason_error_string},

	{NULL, NULL}
};

#include "consts.h"

static void l_register_consts(lua_State *L)
{
	const ConstReg *p = consts;
	while (p->name != NULL) {
		lua_pushstring(L, p->name);
		lua_pushinteger(L, p->value);
		lua_settable(L, -3);
		p++;
	}
}

BufferCFunc *buf_cfunc = NULL;

int luaopen_ssl(lua_State *L)
{
	if (!SSL_library_init()) {
		luaL_error(L, "SSL_library_init() failed");
	}

	buffer_initcfunc(L);

	SSL_load_error_strings();

	/* create the fd table */
	lua_pushlightuserdata(L, &ssl_key);
	lua_newtable(L);
	lua_settable(L, LUA_REGISTRYINDEX);

	/* ssl { */
	lua_newtable(L);
		/* ssl.context { */
		lua_pushstring(L, "context");
		l_opencontext(L);
		lua_settable(L, -3);
		/* } */

		l_register_consts(L);
		luaL_setfuncs(L, funcs, 0);
	/* } */

	return 1;
}
