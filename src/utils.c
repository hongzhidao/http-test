/*
 * Copyright (C) Zhidao HONG
 */
#include "headers.h"

void *
zmalloc(size_t size)
{
    return malloc(size);
}


void *
zcalloc(size_t size)
{
    void *p;

    p = malloc(size);
    if (p != NULL) {
        memzero(p, size);
    }

    return p;
}


void *
zrealloc(void *p, size_t size)
{
    return realloc(p, size);
}


void
zfree(void *p)
{
    free(p);
}


int
memcasecmp(const void *p1, const void *p2, size_t length)
{
    int n;
    u_char c1, c2;
    const u_char  *s1, *s2;

    s1 = p1;
    s2 = p2;

    while (length-- != 0) {
        c1 = *s1++;
        c2 = *s2++;

        c1 = lowcase(c1);
        c2 = lowcase(c2);

        n = c1 - c2;

        if (n != 0) {
            return n;
        }
    }

    return 0;
}


int
parse_int(char *str, size_t len)
{
    char *end;
    char buf[65];

    if (len > 64) {
        return -1;
    }

    memcpy(buf, str, len);
    buf[len] = '\0';

    long val = strtol(buf, &end, 10);

    if (errno == ERANGE) {
        return -1;
    }

    if (end == str) {
        return -1;
    }

    if (*end != '\0') {
        return -1;
    }

    return val;
}


static inline char *
strlchr(char *p, char *last, char c)
{
    while (p < last) {

        if (*p == c) {
            return p;
        }

        p++;
    }

    return NULL;
}


int
parse_url(struct url *u, char *url)
{
    char *p, *start, *end;
    char *path, *host, *port;
    size_t path_len, host_len, port_len;

    start = url;
    end = start + strlen(url);

    if (strncmp(start, "http://", 7) == 0) {
        u->scheme = "http";
        start += 7;

    } else if (strncmp(start, "https://", 8) == 0) {
        u->scheme = "https";
        start += 8;

    } else {
        return -1;
    }

    p = strlchr(start, end, '?');

    path = NULL;
    path_len = 0;

    if (p != NULL) {
        path = p;
        path_len = end - path;

        end = p;
    }

    p = strlchr(start, end, '/');

    if (p != NULL) {
        path = p;
        path_len += end - p;

        end = p;
    }

    if (path != NULL) {
        u->path = zmalloc(path_len + 1);
        memcpy(u->path, path, path_len);

    } else {
        u->path = "/";
    }

    p = strlchr(start, end, ':');

    if (p != NULL) {
        port = p + 1;
        port_len = end - port;

        if (port_len == 0) {
            return -1;
        }

        u->port = zmalloc(port_len + 1);
        memcpy(u->port, port, port_len);

        end = p;
    }

    host = start;
    host_len = end - host;

    if (host_len == 0) {
        return -1;
    }

    u->host = zmalloc(host_len + 1);
    memcpy(u->host, host, host_len);

    return 0;
}


static int
addr_connect(struct addrinfo *addr)
{
    int fd, ret = 0;

    fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

    if (fd != -1) {
        ret = connect(fd, addr->ai_addr, addr->ai_addrlen) == 0;
    }
    close(fd);
    return ret;
}


struct addrinfo *
addr_resolve(char *host, char *service)
{
    int ret;
    struct addrinfo *addrs, *ai, *addr = NULL;
    struct addrinfo hints = {
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM
    };

    ret = getaddrinfo(host, service, &hints, &addrs);
    if (ret != 0) {
        const char *msg = gai_strerror(ret);
        fprintf(stderr, "unable to resolve %s:%s %s\n", host, service, msg);
        return NULL;
    }

    for (ai = addrs; ai != NULL; ai = ai->ai_next) {
        if (addr_connect(ai)) {
            addr = zmalloc(sizeof(struct addrinfo));
            if (addr == NULL) {
                break;
            }

            *addr = *ai;
            addr->ai_addr = zmalloc(ai->ai_addrlen);
            memcpy(addr->ai_addr, ai->ai_addr, ai->ai_addrlen);
            break;
        }
    }

    freeaddrinfo(addrs);

    return addr;
}


struct buf *
buf_alloc(size_t size)
{
    struct buf *b;

    b = zcalloc(sizeof(struct buf) + size);

    if (size > 0) {
        b->start = pointer_to(b, sizeof(struct buf));
        b->free = b->start;
        b->pos = b->start;
        b->end = b->start + size;
    }

    return b;
}


char *
format_time(char *buf, uint64_t time)
{
    char *scale;
    double size;

    if (time > 60 * 1000000 - 1) {
        size = (double) time / (60 * 1000000);
        scale = "m";

    } else if (time > 1000000 - 1) {
        size = (double) time / 1000000;
        scale = "s";

    } else if (time > 1000 - 1) {
        size = (double) time / 1000;
        scale = "ms";

    } else {
        size = time;
        scale = "us";
    }

    sprintf(buf, "%.2f%s", size, scale);
    return buf;
}


char *
format_byte(char *buf, size_t bytes)
{
    char scale;
    double size;

    if (bytes > 1024 * 1024 * 1024 - 1) {
        size = (double) bytes / (1024 * 1024 * 1024);
        scale = 'G';

    } else if (bytes > 1024 * 1024 - 1) {
        size = (double) bytes / (1024 * 1024);
        scale = 'M';

    } else if (bytes > 1024 - 1) {
        size = (double) bytes / 1024;
        scale = 'K';

    } else {
        size = bytes;
        scale = 'B';
    }

    sprintf(buf, "%.2f%c", size, scale);
    return buf;
}
