
/*
 * Copyright (C) Spyderj
 */

#include "lstdimpl.h"
#include <signal.h>
#include <time.h>

size_t mem_total = 0;

#define SIZE_T 					sizeof(size_t)
#define cast(type, p)			((type*)(p))
#define lshift(p)				(cast(size_t, p) - 1)
#define rshift(p)				(cast(size_t, p) + 1)

void* l_realloc(void *optr, size_t nsize)
{
	void *nptr = NULL;
	
	if (nsize > 0) {
		nptr = malloc(nsize + SIZE_T);
		
		if (nptr == NULL)
			abort();
			
		*(cast(size_t, nptr)) = nsize;
		nptr = rshift(nptr);
		mem_total += nsize;
	}
	
	if (optr != NULL) {
		size_t *psize = lshift(optr);
		size_t osize = *psize;
		
		mem_total -= osize;
		
		if (nsize > 0)
			memcpy(nptr, optr, MIN(osize, nsize));
			
		free(psize);
	}
	
	return nptr;
}

void l_register_enums(lua_State *L, const EnumReg *regs) 
{
	while (regs->name != NULL) {
		lua_pushstring(L, regs->name);
		lua_pushinteger(L, regs->val);
		lua_settable(L, -3);
		regs++;
	}
}

void l_register_metatable_full(lua_State *L, const char *name, const luaL_Reg *funcs, bool index_self)
{
	luaL_newmetatable(L, name);		/* [meta] */
	luaL_register(L, NULL, funcs);
    lua_pushstring(L, "__index");	/* [meta, "__index"] */
    lua_pushvalue(L, index_self ? -2 : -3); /* [meta, "__index", index_t] */
    lua_rawset(L, -3);
    lua_pop(L, 1);
}

void l_register_lib(lua_State *L, const char *name, const luaL_Reg *funcs, const EnumReg *enums)
{
	bool extend = true; /* true if the package exists and we are extending it */
	
	lua_getglobal(L, name);
	if (lua_isnil(L, -1)) {
		extend = false;
		lua_pop(L, 1);
		lua_newtable(L);
	}
	
	if (funcs != NULL)
		luaL_register(L, NULL, funcs);
	if (enums != NULL)
		l_register_enums(L, enums);
	
	if (extend)
		lua_pop(L, 1);
	else
		lua_setglobal(L, name);
}

static int l_stdmem(lua_State *L)
{
	lua_pushinteger(L, (int)mem_total);
	return 1;
}

static int l_tonumber2(lua_State *L)
{
	const void *p = L;
	
	if (lua_gettop(L) > 0)
		p = lua_topointer(L, 1);
		
	lua_pushinteger(L, (int)(((unsigned int)p) >> 2));
	return 1;
}

int luaopen__std(lua_State *L) 
{
	l_openstring(L);
	l_opentable(L);
	l_openmath(L);
	l_openbitlib(L);
	l_openerrno(L);
	l_openos(L);
	l_openfs(L);
	l_openreader(L);
	l_openbuffer(L);
	l_openstat(L);
	l_opentime(L);
	l_opensocket(L);
	l_opensignal(L);
	l_opensys(L);
	l_opennetdb(L);
	l_opencodec(L);
#if 0
	l_opensem(L);
	l_openinotify(L);
#endif
	l_openfcntl(L);
	l_openpoll(L);
	l_openprctl(L);
	l_openrbtree(L);
	l_openiface(L);
	
	lua_pushcfunction(L, l_tonumber2);
	lua_setglobal(L, "tonumber2");
	
	lua_pushcfunction(L, l_stdmem);
	lua_setglobal(L, "stdmem");
	
	return 0;
}
