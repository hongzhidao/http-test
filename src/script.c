/*
 * Copyright (C) Zhidao HONG
 */
#include "headers.h"

lua_State *
script_create()
{
    lua_State *L;
    http_field *field;

    L = luaL_newstate();
    if (L == NULL) {
        return NULL;
    }

    luaL_openlibs(L);

    lua_newtable(L);

    lua_pushstring(L, "GET");
    lua_setfield(L, -2, "method");

    lua_pushstring(L, cfg.path);
    lua_setfield(L, -2, "path");

    lua_newtable(L);
    for (field = cfg.headers; field != NULL; field = field->next) {
        lua_pushlstring(L, field->name, field->name_length);
        lua_pushlstring(L, field->value, field->value_length);
        lua_settable(L, -3);
    }
    lua_setfield(L, -2, "headers");

    lua_setglobal(L, "http");

    if (cfg.script && luaL_dofile(L, cfg.script) != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        printf("load script %s failed: %s\n", cfg.script, err);
        return NULL;
    }

    return L;
}


int
script_has_function(lua_State *L, const char *name)
{
    int ret;

    lua_getglobal(L, "http");
    lua_getfield(L, -1, name);
    ret = lua_isfunction(L, -1);
    lua_pop(L, 2);

    return ret;
}


struct buf *
script_request(lua_State *L)
{
    int chunked;
    size_t body_length;
    struct buf *b;
    const char *method, *path, *body, *te, *request;

    luaL_Buffer buffer;
    luaL_buffinit(L, &buffer);

    lua_pushvalue(L, -2);

    lua_getfield(L, -1, "method");
    method = lua_tostring(L, -1);
    if (method == NULL) {
        method = "GET";
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "path");
    path = lua_tostring(L, -1);
    if (path == NULL) {
        path = "/";
    }
    lua_pop(L, 1);

    luaL_addstring(&buffer, method);
    luaL_addstring(&buffer, " ");
    luaL_addstring(&buffer, path);
    luaL_addstring(&buffer, " HTTP/1.1\r\n");

    lua_getfield(L, -1, "body");
    body = lua_tostring(L, -1);
    body_length = body != NULL ? strlen(body) : 0;
    lua_pop(L, 1);

    lua_getfield(L, -1, "headers");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "headers");
    }

    lua_getfield(L, -1, "Host");
    if (lua_isnil(L, -1)) {
        luaL_addstring(&buffer, "Host: ");
        luaL_addstring(&buffer, cfg.host);
        luaL_addstring(&buffer, "\r\n");
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "Transfer-Encoding");
    te = lua_tostring(L, -1);
    if (te != NULL && strlen(te) == 7
        && strncmp(te, "chunked", 7) == 0)
    {
        chunked = 1;
    } else {
        chunked = 0;
    }
    lua_pop(L, 1);

    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        const char *name = lua_tostring(L, -2);
        const char *value = lua_tostring(L, -1);
        if (value == NULL) {
            return NULL;
        }

        luaL_addstring(&buffer, name);
        luaL_addstring(&buffer, ": ");
        luaL_addstring(&buffer, value);
        luaL_addstring(&buffer, "\r\n");

        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    if (body_length > 0 && !chunked) {
        char num[20];
        sprintf(num, "%zu", strlen(body));
        luaL_addstring(&buffer, "Content-Length: ");
        luaL_addstring(&buffer, num);
        luaL_addstring(&buffer, "\r\n");
    }

    luaL_addstring(&buffer, "\r\n");

    if (body_length > 0) {
        if (chunked) {
            char chunk[32];
            sprintf(chunk, "%zx\r\n", body_length);
            luaL_addstring(&buffer, chunk);
            luaL_addstring(&buffer, body);
            luaL_addstring(&buffer, "\r\n0\r\n\r\n");
        } else {
            luaL_addstring(&buffer, body);
        }
    }

    luaL_pushresult(&buffer);
    request = lua_tostring(L, -1);

    lua_pop(L, 2);

    b = buf_alloc(strlen(request));
    if (b == NULL) {
        return NULL;
    }

    b->free = cpymem(b->free, (char *) request, strlen(request));

    return b;
}
