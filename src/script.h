/*
 * Copyright (C) Zhidao HONG
 */
#ifndef SCRIPT_H
#define SCRIPT_H

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

lua_State *script_create(void);
int script_has_function(lua_State *, const char *);
struct buf *script_request(lua_State *);

#endif /* SCRIPT_H */
