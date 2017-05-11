
/*
 * Copyright (C) spyder
 */

#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <lua.h>
#include <lauxlib.h>

#include "lstd.h"

SSL_CTX*	l_getcontext(lua_State *L, int idx);
int 		l_opencontext(lua_State *L);

#endif
