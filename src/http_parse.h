/*
 * Copyright (C) Valentin V. Bartenev
 * Copyright (C) Zhidao HONG
 */
#ifndef HTTP_PARSE_H
#define HTTP_PARSE_H

typedef struct http_parser {
    int (*handler)(struct http_parser *, char **, char *);

    int status;
    char *name;
    char *value;
    uint32_t name_length;
    uint32_t value_length;
    uint32_t field_hash;
    off_t content_length_n;
    uint8_t keepalive;
    uint8_t chunked;
    uint8_t skip_field;
    uint8_t discard_unsafe_fields;
} http_parser;

typedef struct {
    uint64_t chunk_size;
    uint8_t state;
    uint8_t last;
    uint8_t chunk_error;
    uint8_t error;
} http_chunk_parser;

enum {
    HTTP_PARSE_INVALID = 1,
    HTTP_PARSE_UNSUPPORTED_VERSION,
    HTTP_PARSE_TOO_LARGE_FIELD,
};

void http_parse_init(http_parser *);
int http_parse_response(http_parser *, struct buf *);
struct buf *http_parse_chunk(http_chunk_parser *, struct buf *);

#define HTTP_FIELD_HASH_INIT        159406U
#define http_field_hash_char(h, c)  (((h) << 4) + (h) + (c))
#define http_field_hash_end(h)      (((h) >> 16) ^ (h))

#endif /* HTTP_PARSE_H */
