/*
 * Copyright (C) Zhidao HONG
 */
#include "headers.h"

static int ssl_connected(struct conn *, char *host);
static ssize_t ssl_recv(struct conn *, void *, size_t);
static ssize_t ssl_send(struct conn *, void *, size_t);
static void ssl_close(struct conn *);

conn_io ssl_conn_io = {
    .connected = ssl_connected,
    .recv = ssl_recv,
    .send = ssl_send,
    .close = ssl_close,
};


SSL_CTX *
ssl_init()
{
    SSL_CTX *ctx;

    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_all_algorithms();

    ctx = SSL_CTX_new(SSLv23_client_method());

    if (ctx != NULL) {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
        SSL_CTX_set_verify_depth(ctx, 0);
        SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
        SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_CLIENT);
    }

    return ctx;
}


static int
ssl_connected(struct conn *c, char *host)
{
    int ret;

    SSL_set_fd(c->ssl, c->socket.fd);
    SSL_set_tlsext_host_name(c->ssl, host);

    ret = SSL_connect(c->ssl);

    if (ret != 1) {
        switch (SSL_get_error(c->ssl, ret)) {
        case SSL_ERROR_WANT_READ: return RETRY;
        case SSL_ERROR_WANT_WRITE: return RETRY;
        default: return ERROR;
        }
    }

    return OK;
}


static ssize_t
ssl_recv(struct conn *c, void *buf, size_t size)
{
    ssize_t n;

    n = SSL_read(c->ssl, buf, size);

    if (n <= 0) {
        switch (SSL_get_error(c->ssl, n)) {
        case SSL_ERROR_WANT_READ: return RETRY;
        case SSL_ERROR_WANT_WRITE: return RETRY;
        default: return ERROR;
        }
    }

    return n;
}


static ssize_t
ssl_send(struct conn *c, void *buf, size_t size)
{
    ssize_t n;

    n = SSL_write(c->ssl, buf, size);

    if (n <= 0) {
        switch (SSL_get_error(c->ssl, n)) {
        case SSL_ERROR_WANT_READ: return RETRY;
        case SSL_ERROR_WANT_WRITE: return RETRY;
        default: return ERROR;
        }
    }

    return n;
}


static void
ssl_close(struct conn *c)
{
    SSL_shutdown(c->ssl);
    SSL_clear(c->ssl);
}
