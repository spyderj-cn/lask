
/*
 * Copyright (C) spyder
 */

#ifndef LSTD_H
#define LSTD_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <lua.h>
#include <lauxlib.h>

#define BUFFER_META				"meta(buffer)"
#define BUFFER_MAGIC 			1437549501
typedef struct _Buffer {
	uint32_t magic;
	uint8_t *mem;
	size_t memsiz;
	uint8_t *data;
	size_t datasiz;
	size_t minsiz;
	bool be;
	bool rd;
	bool wr;
}Buffer;

typedef struct _BufferCFunc {
	void (*init)(Buffer *buf, size_t minsiz);
	uint8_t* (*grow)(Buffer *buf, size_t growth);
	size_t (*push)(Buffer *buf, const void *mem, size_t memsiz);
	void (*rewind)(Buffer *buf);
	void (*shift)(Buffer *buf, size_t siz);
	void (*pop)(Buffer *buf, size_t siz);
	void (*reset)(Buffer *buf);
	void (*finalize)(Buffer *buf);
}BufferCFunc;

#define READER_META				"meta(reader)"
#define READER_MAGIC 			1437549502
typedef struct _Reader {
	uint32_t magic;
	const uint8_t *mem;
	const uint8_t *data;
	size_t memsiz;
	size_t datasiz;
	bool be;
}Reader;

#if !STD_SELF
extern BufferCFunc *buf_cfunc;
#define	buffer_initcfunc(L) do { \
	lua_getglobal(L, "buffer"); \
	if (lua_isnil(L, -1)) { luaL_error(L, "need _G.buffer._cfuncs, please require 'std' first "); } \
	lua_getfield(L, -1, "_cfuncs"); \
	buf_cfunc = (BufferCFunc*)lua_touserdata(L, -1); \
	lua_pop(L, 2); \
} while (0)

#define buffer_init			(buf_cfunc->init)
#define buffer_grow			(buf_cfunc->grow)
#define buffer_push			(buf_cfunc->push)
#define buffer_rewind		(buf_cfunc->rewind)
#define buffer_shift		(buf_cfunc->shift)
#define buffer_pop			(buf_cfunc->pop)
#define buffer_reset		(buf_cfunc->reset)
#define buffer_finalize		(buf_cfunc->finalize)
#endif

#if LUA_VERSION_NUM == 501
#if !STD_SELF 
static void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
  luaL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    int i;
    for (i = 0; i < nup; i++)  /* copy upvalues to the top */
      lua_pushvalue(L, -nup);
    lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    lua_setfield(L, -(nup + 2), l->name);
  }
  lua_pop(L, nup);  /* remove upvalues */
}
#endif

#define luaL_newlib(L, l) (lua_newtable(L), luaL_setfuncs(L,l,0))
#endif

#endif

