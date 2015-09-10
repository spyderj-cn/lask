
/*
 * Copyright (C) Spyderj
 */

#ifndef LSTD_H
#define LSTD_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

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

#if STD_SELF
#else
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

#endif

