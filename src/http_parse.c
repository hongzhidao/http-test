/*
 * Copyright (C) Valentin V. Bartenev
 * Copyright (C) Zhidao HONG
 */
#include "headers.h"

static int http_parse_status_line(http_parser *, char **, char *);
static int http_parse_field_name(http_parser *, char **, char *);
static int http_parse_field_value(http_parser *, char **, char *);
static int http_parse_field_end(http_parser *, char **, char *);
static char *http_parse_field_lookup_end(char *, char *);
static int http_parse_field_proc(http_parser *);
static int http_parse_field_connection(http_parser *);
static int http_parse_field_transfer_encoding(http_parser *);
static int http_parse_field_content_length(http_parser *);

#define HTTP_MAX_FIELD_NAME         0xFF
#define HTTP_MAX_FIELD_VALUE        0x7FFFFFFF
#define HTTP_FIELD_LVLHSH_SHIFT     5


void
http_parse_init(http_parser *pr)
{
    pr->field_hash = HTTP_FIELD_HASH_INIT;
    pr->content_length_n = -1;
}


int
http_parse_response(http_parser *pr, struct buf *b)
{
    int ret;

    if (pr->handler == NULL) {
        pr->handler = http_parse_status_line;
    }

    do {
        ret = pr->handler(pr, &b->pos, b->free);
    } while (ret == OK);

    return ret;
}


static int
http_parse_status_line(http_parser *pr, char **pos, char *end)
{
    int status;
    char *p;
    size_t length;

    p = *pos;
    length = end - p;

    if (length < 12) {
        return RETRY;
    }

    if (memcmp(p, "HTTP/1.", 7) != 0
        || (p[7] != '0' && p[7] != '1'))
    {
        return ERROR;
    }

    pr->keepalive = (p[7] == '1');

    status = parse_int(&p[9], 3);
    if (status < 0) {
        return ERROR;
    }

    p += 12;
    length -= 12;

    p = memchr(p, '\n', length);

    if (p == NULL) {
        return RETRY;
    }

    *pos = p + 1;

    pr->status = status;

    return http_parse_field_name(pr, pos, end);
}


static int
http_parse_field_name(http_parser *pr, char **pos, char *end)
{
    char *p, c;
    size_t len;
    uint32_t hash;

    static const char  normal[256] =
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    /*   \s ! " # $ % & ' ( ) * + ,        . /                 : ; < = > ?   */
        "\0\1\0\1\1\1\1\1\0\0\1\1\0" "-" "\1\0" "0123456789" "\0\0\0\0\0\0"

    /*    @                                 [ \ ] ^ _                        */
        "\0" "abcdefghijklmnopqrstuvwxyz" "\0\0\0\1\1"
    /*    `                                 { | } ~                          */
        "\1" "abcdefghijklmnopqrstuvwxyz" "\0\1\0\1\0"

        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

    p = *pos + pr->name_length;
    hash = pr->field_hash;

    while (end - p >= 8) {

#define field_name_test_char(ch)                                              \
                                                                              \
        c = normal[(u_char) ch];                                              \
                                                                              \
        if (c <= '\1') {                                                      \
            if (c == '\0') {                                                  \
                p = &(ch);                                                    \
                goto name_end;                                                \
            }                                                                 \
                                                                              \
            pr->skip_field = pr->discard_unsafe_fields;                       \
            c = ch;                                                           \
        }                                                                     \
                                                                              \
        hash = http_field_hash_char(hash, c);

/* enddef */

        field_name_test_char(p[0]);
        field_name_test_char(p[1]);
        field_name_test_char(p[2]);
        field_name_test_char(p[3]);

        field_name_test_char(p[4]);
        field_name_test_char(p[5]);
        field_name_test_char(p[6]);
        field_name_test_char(p[7]);

        p += 8;
    }

    while (p != end) {
        field_name_test_char(*p); p++;
    }

    len = p - *pos;

    if (len > HTTP_MAX_FIELD_NAME) {
        return HTTP_PARSE_TOO_LARGE_FIELD;
    }

    pr->field_hash = hash;
    pr->name_length = len;

    pr->handler = http_parse_field_name;

    return RETRY;

name_end:

    if (*p == ':') {
        if (p == *pos) {
            return HTTP_PARSE_INVALID;
        }

        len = p - *pos;

        if (len > HTTP_MAX_FIELD_NAME) {
            return HTTP_PARSE_TOO_LARGE_FIELD;
        }

        pr->field_hash = hash;
        pr->name = *pos;
        pr->name_length = len;

        *pos = p + 1;

        return http_parse_field_value(pr, pos, end);
    }

    if (p != *pos) {
        return HTTP_PARSE_INVALID;
    }

    return http_parse_field_end(pr, pos, end);
}


static int
http_parse_field_value(http_parser *pr, char **pos, char *end)
{
    char *p, *start, ch;
    size_t len;

    p = *pos;

    for ( ;; ) {
        if (p == end) {
            *pos = p;
            pr->handler = http_parse_field_value;
            return RETRY;
        }

        ch = *p;

        if (ch != ' ' && ch != '\t') {
            break;
        }

        p++;
    }

    start = p;

    p += pr->value_length;

    for ( ;; ) {
        p = http_parse_field_lookup_end(p, end);

        if (p == end) {
            *pos = start;

            len = p - start;
            if (len > HTTP_MAX_FIELD_VALUE) {
                return HTTP_PARSE_TOO_LARGE_FIELD;
            }

            pr->value_length = len;
            pr->handler = http_parse_field_value;

            return RETRY;
        }

        ch = *p;

        if (ch == '\r' || ch == '\n') {
            break;
        }

        if (ch != '\t') {
            return HTTP_PARSE_INVALID;
        }

        p++;
    }

    *pos = p;

    if (p != start) {

        while (p[-1] == ' ' || p[-1] == '\t') {
            p--;
        }
    }

    len = p - start;

    if (len > HTTP_MAX_FIELD_VALUE) {
        return HTTP_PARSE_TOO_LARGE_FIELD;
    }

    pr->value = start;
    pr->value_length = len;

    return http_parse_field_end(pr, pos, end);
}

 
static int
http_parse_field_end(http_parser *pr, char **pos, char *end)
{
    int ret;
    char *p;

    p = *pos;

    if (*p == '\r') {
        p++;

        if (p == end) {
            pr->handler = http_parse_field_end;
            return RETRY;
        }
    }

    if (*p == '\n') {
        *pos = p + 1;

        if (pr->name_length != 0) {
            if (pr->skip_field) {
                pr->skip_field = 0;

            } else {
                ret = http_parse_field_proc(pr);
                if (ret != OK) {
                    return ret;
                }
            }

            pr->field_hash = HTTP_FIELD_HASH_INIT;
            pr->name_length = 0;
            pr->value_length = 0;

            pr->handler = http_parse_field_name;

            return OK;
        }

        return DONE;
    }

    return HTTP_PARSE_INVALID;
}


static char *
http_parse_field_lookup_end(char *p, char *end)
{
    while (end - p >= 16) {

#define field_end_test_char(ch)                                               \
                                                                              \
        if ((u_char) (ch) < 0x20) {                                           \
            return &(ch);                                                     \
        }

/* enddef */

        field_end_test_char(p[0]);
        field_end_test_char(p[1]);
        field_end_test_char(p[2]);
        field_end_test_char(p[3]);

        field_end_test_char(p[4]);
        field_end_test_char(p[5]);
        field_end_test_char(p[6]);
        field_end_test_char(p[7]);

        field_end_test_char(p[8]);
        field_end_test_char(p[9]);
        field_end_test_char(p[10]);
        field_end_test_char(p[11]);

        field_end_test_char(p[12]);
        field_end_test_char(p[13]);
        field_end_test_char(p[14]);
        field_end_test_char(p[15]);

        p += 16;
    }

    while (end - p >= 4) {
        field_end_test_char(p[0]);
        field_end_test_char(p[1]);
        field_end_test_char(p[2]);
        field_end_test_char(p[3]);

        p += 4;
    }

    switch (end - p) {
    case 3:
        field_end_test_char(*p); p++;
        /* Fall through. */
    case 2:
        field_end_test_char(*p); p++;
        /* Fall through. */
    case 1:
        field_end_test_char(*p); p++;
        /* Fall through. */
    case 0:
        break;
    default:
        break;
    }

    return p;
}


static int
http_parse_field_proc(http_parser *pr)
{
    if (memcmp(pr->name, "Connection", pr->name_length) == 0) {
        return http_parse_field_connection(pr);
    }

    if (memcmp(pr->name, "Transfer-Encoding", pr->name_length) == 0) {
        return http_parse_field_transfer_encoding(pr);
    }

    if (memcmp(pr->name, "Content-Length", pr->name_length) == 0) {
        return http_parse_field_content_length(pr);
    }

    return OK;
}


static int
http_parse_field_connection(http_parser *pr)
{
    if (pr->value_length == 5
        && memcasecmp(pr->value, "close", 5) == 0)
    {
        pr->keepalive = 0;

    } else if (pr->value_length == 10
               && memcasecmp(pr->value, "keep-alive", 10) == 0)
    {
        pr->keepalive = 1;
    }

    return OK;
}


static int
http_parse_field_transfer_encoding(http_parser *pr)
{
    if (pr->value_length == 7
        && memcasecmp(pr->value, "chunked", 5) == 0)
    {
        pr->chunked = 1;
    }

    return OK;
}


static int
http_parse_field_content_length(http_parser *pr)
{
    pr->content_length_n = parse_int(pr->value, pr->value_length);

    if (pr->content_length_n < 0) {
        return ERROR;
    }

    return OK;
}


#define HTTP_CHUNK_MIDDLE         0
#define HTTP_CHUNK_END_ON_BORDER  1
#define HTTP_CHUNK_END            2

#define size_is_sufficient(cs)                                            \
    (cs < ((__typeof__(cs)) 1 << (sizeof(cs) * 8 - 4)))

static int http_chunk_buffer(http_chunk_parser *, struct buf ***, struct buf *);


struct buf *
http_parse_chunk(http_chunk_parser *hcp, struct buf *b)
{
    int ret;
    u_char c, ch;
    struct buf *out, **tail;
    enum {
        sw_start = 0,
        sw_chunk_size,
        sw_chunk_size_linefeed,
        sw_chunk_end_newline,
        sw_chunk_end_linefeed,
        sw_chunk,
    } state;

    out = NULL;
    tail = &out;

    state = hcp->state;

    while (b->pos < b->free) {
        /*
         * The sw_chunk state is tested outside the switch
         * to preserve hcp->pos and to not touch memory.
         */
        if (state == sw_chunk) {
            ret = http_chunk_buffer(hcp, &tail, b);

            if (ret == HTTP_CHUNK_MIDDLE) {
                break;
            }

            if (ret == ERROR) {
                hcp->error = 1;
                return out;
            }

            state = sw_chunk_end_newline;

            if (ret == HTTP_CHUNK_END_ON_BORDER) {
                break;
            }

            /* ret == HTTP_CHUNK_END */
        }

        ch = (u_char) *b->pos++;

        switch (state) {

        case sw_start:
            state = sw_chunk_size;

            c = ch - '0';

            if (c <= 9) {
                hcp->chunk_size = c;
                continue;
            }

            c = (ch | 0x20) - 'a';

            if (c <= 5) {
                hcp->chunk_size = 0x0A + c;
                continue;
            }

            goto chunk_error;

        case sw_chunk_size:
            c = ch - '0';

            if (c > 9) {
                c = (ch | 0x20) - 'a';

                if (c <= 5) {
                    c += 0x0A;

                } else if (ch == '\r') {
                    state = sw_chunk_size_linefeed;
                    continue;

                } else {
                    goto chunk_error;
                }
            }

            if (size_is_sufficient(hcp->chunk_size)) {
                hcp->chunk_size = (hcp->chunk_size << 4) + c;
                continue;
            }

            goto chunk_error;

        case sw_chunk_size_linefeed:
            if (ch == '\n') {

                if (hcp->chunk_size != 0) {
                    state = sw_chunk;
                    continue;
                }

                hcp->last = 1;
                state = sw_chunk_end_newline;
                continue;
            }

            goto chunk_error;

        case sw_chunk_end_newline:
            if (ch == '\r') {
                state = sw_chunk_end_linefeed;
                continue;
            }

            goto chunk_error;

        case sw_chunk_end_linefeed:
            if (ch == '\n') {

                if (!hcp->last) {
                    state = sw_start;
                    continue;
                }

                return out;
            }

            goto chunk_error;

        case sw_chunk:
            /*
             * This state is processed before the switch.
             * It added here just to suppress a warning.
             */
            continue;
        }
    }

    hcp->state = state;

    return out;

chunk_error:

    hcp->chunk_error = 1;

    return out;
}


static int
http_chunk_buffer(http_chunk_parser *hcp, struct buf ***tail, struct buf *in)
{
    size_t size;
    struct buf *b;

    b = zcalloc(sizeof(*b));
    if (b == NULL) {
        return ERROR;
    }

    **tail = b;
    *tail = &b->next;

    b->pos = in->pos;
    b->start = in->pos;

    size = in->free - in->pos;

    if (hcp->chunk_size < size) {
        size = hcp->chunk_size;
    }

    in->pos += size;
    b->free = in->pos;

    hcp->chunk_size -= size;

    if (in->free > in->pos) {
        return HTTP_CHUNK_END;
    }

    if (hcp->chunk_size == 0) {
        return HTTP_CHUNK_END_ON_BORDER;
    }

    return HTTP_CHUNK_MIDDLE;
}
