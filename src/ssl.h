/*
 * Copyright (C) Zhidao HONG
 */
#ifndef SSL_H
#define SSL_H

#include <openssl/err.h>
#include <openssl/ssl.h>

SSL_CTX *ssl_init(void);

extern conn_io ssl_conn_io;

#endif /* SSL_H */
