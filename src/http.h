/*
 * Copyright (C) Zhidao HONG
 */
#ifndef HTTP_H
#define HTTP_H

typedef struct http_field {
    uint8_t name_length;
    uint32_t value_length;
    char *name;
    char *value;
    struct http_field *next;
} http_field;

int http_header_parse(http_field *field, char *header);
void http_peer_connect(struct conn *c);

#endif /* HTTP_H */
