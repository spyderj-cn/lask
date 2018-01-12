
/*
 * Copyright (C) www.go-cloud.cn
 */

#ifndef LSTDIMPL_H
#define LSTDIMPL_H

#define STD_SELF 		1
#include "lstd.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

#if LUA_VERSION_NUM == 501
#define lua_rawlen		lua_objlen
#endif

#define uint8 uint8_t
#define uint16 uint16_t
#define uint32 uint32_t
#define uint64 unsigned long long
#define uint uint32
#define int8  int8_t
#define int16 int16_t
#define int32 int32_t
#define int64 long long

#define unused(x)				(void)(x)
#define lengthof(arr)			(sizeof(arr) / sizeof(arr[0]))

#ifndef MIN
#define MIN(a, b)				(((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b)				(((a) > (b)) ? (a) : (b))
#endif

#define UINT16(x)					((uint16)(x))
#define UINT32(x)					((uint32)(x))

#define bytes_to_uint16_be(mem)		UINT16(mem[1]) | (UINT32(mem[0]) << 8)
#define bytes_to_uint32_be(mem)		UINT32(mem[3]) | (UINT32(mem[2]) << 8) | (UINT32(mem[1]) << 16) | (UINT32(mem[0]) << 24)
#define uint16_to_bytes_be(val, mem) do {\
	uint8 *_mem = (uint8*)(mem); \
	uint32 _val = (val); \
	_mem[1] = (uint8)((_val) & 0xff);\
	_mem[0] = (uint8)((_val >> 8) & 0xff); \
} while (0)
#define uint32_to_bytes_be(val, mem) do {\
	uint8 *_mem = (uint8*)(mem); \
	uint32 _val = (val); \
	_mem[3] = (uint8)((_val) & 0xff);\
	_mem[2] = (uint8)((_val >> 8) & 0xff); \
	_mem[1] = (uint8)((_val >> 16) & 0xff); \
	_mem[0] = (uint8)((_val >> 24) & 0xff); \
} while (0)

#define bytes_to_uint16_le(mem)		UINT16(mem[0]) | (UINT32(mem[1]) << 8)
#define bytes_to_uint32_le(mem)		UINT32(mem[0]) | (UINT32(mem[1]) << 8) | (UINT32(mem[2]) << 16) | (UINT32(mem[3]) << 24)
#define uint16_to_bytes_le(val, mem) do {\
	uint8 *_mem = (uint8*)(mem); \
	uint32 _val = (val); \
	_mem[0] = (uint8)((_val) & 0xff);\
	_mem[1] = (uint8)((_val >> 8) & 0xff); \
} while (0)
#define uint32_to_bytes_le(val, mem) do {\
	uint8 *_mem = (uint8*)(mem); \
	uint32 _val = (val); \
	_mem[0] = (uint8)((_val) & 0xff);\
	_mem[1] = (uint8)((_val >> 8) & 0xff); \
	_mem[2] = (uint8)((_val >> 16) & 0xff); \
	_mem[3] = (uint8)((_val >> 24) & 0xff); \
} while (0)

void* l_realloc(void *optr, size_t nsize);
#define		REALLOC		l_realloc
#define 	MALLOC(siz)	REALLOC(NULL, siz)
#define 	FREE(ptr)	REALLOC(ptr, 0)
#define 	NEW(type)	(type*)MALLOC(sizeof(type))
#define 	DELETE		FREE

#ifndef ESUCCEED
#define ESUCCEED				0
#endif

typedef enum {
	ST_DEV		= (1 << 1),
	ST_UID		= (1 << 2),
	ST_GID		= (1 << 3),
	ST_ATIME	= (1 << 4),
	ST_MTIME	= (1 << 5),
	ST_CTIME	= (1 << 6),
	ST_SIZE		= (1 << 9),
	ST_NLINK	= (1 << 10),
	ST_RDEV		= (1 << 11),
	ST_INO		= (1 << 12),
	ST_MODE		= (1 << 13),
	ST_ALL		= 0xffff,
}ST_FIELD;


void 		buffer_init(Buffer *buf, size_t minsiz);
uint8* 		buffer_grow(Buffer *buf, size_t growth);
size_t 		buffer_push(Buffer *buf, const void *mem, size_t memsiz);
void 		buffer_rewind(Buffer *buf);
void 		buffer_shift(Buffer *buf, size_t siz);
void 		buffer_pop(Buffer *buf, size_t siz);
void 		buffer_reset(Buffer *buf);
void 		buffer_finalize(Buffer *buf);
Buffer* 	buffer_lcheck(lua_State *L, int idx);
uint8* 		buffer_safegrow(Buffer *buffer, size_t growth, lua_State *L);

void 		reader_init(Reader *rd, const uint8 *mem, size_t memsiz);
void 		reader_shift(Reader *rd, size_t siz);
Reader* 	reader_lcheck(lua_State *L, int idx);
const char* reader_getline(Reader *rd, size_t *len);

int  		fcntl_addfl(int fd, int flags);
int 		fcntl_delfl(int fd, int flags);

int 		os_getnread(int fd, size_t *nread);
int 		os_implread(int fd, Buffer *buffer, ssize_t req, size_t *done,
				ssize_t (*read_cb)(int, void*, size_t, void*), void* ud);
int 		os_read(int fd, Buffer *buffer, ssize_t bytes_req, size_t *bytes_done);
int 		os_implwrite(int fd, const uint8 *mem, size_t bytes_req, size_t *bytes_done,
				ssize_t (*write_cb)(int, const void*, size_t, void*), void* ud);
int 		os_write(int fd, const uint8 *mem, size_t bytes_req, size_t *bytes_done);


typedef struct _EnumReg {
	const char *name;
	int val;
} EnumReg;
#define LENUM(x)					{#x, (int)(x)}
#define LENUM_NULL					{NULL, 0}

#define 	l_setmetatable(L, idx, name)       do {\
	luaL_getmetatable(L, name); \
	lua_setmetatable(L, idx > 0 ? idx : (idx - 1)); } while (0)

/* these reigsteration functions won't leave the created table on the stack */
void		l_register_enums(lua_State *L, const EnumReg *regs);
void 		l_register_metatable_full(lua_State *L, const char *name, const luaL_Reg *funcs, bool index_self);
#define 	l_register_metatable(L, name, funcs) 	l_register_metatable_full(L, name, funcs, false)
#define 	l_register_metatable2(L, name, funcs) 	l_register_metatable_full(L, name, funcs, true)
void 		l_register_lib(lua_State *L, const char *name, const luaL_Reg *funcs, const EnumReg *enums);


int 		l_opentable(lua_State *L);
int 		l_openstring(lua_State *L);
int 		l_openmath(lua_State *L);
int 		l_openbitlib(lua_State *L);
int			l_openerrno(lua_State *L);
int			l_openbuffer(lua_State *L);
int 		l_openreader(lua_State *L);
int 		l_openio(lua_State *L);
int 		l_openfs(lua_State *L);
int			l_openstat(lua_State *L);
int			l_openos(lua_State *L);
int 		l_opentime(lua_State *L);
int			l_opensocket(lua_State *L);
int 		l_opensignal(lua_State *L);
int 		l_opensys(lua_State *L);
int 		l_opennetdb(lua_State *L);
int 		l_opencodec(lua_State *L);
int 		l_opensem(lua_State *L);
int 		l_openinotify(lua_State *L);
int 		l_openfcntl(lua_State *L);
int 		l_openpoll(lua_State *L);
int 		l_openprctl(lua_State *L);
int 		l_openrbtree(lua_State *L);
int 		l_openiface(lua_State *L);
int 		l_openmd5(lua_State *L);

#if LUA_VERSION_NUM == 501
void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup);
#endif

#endif
