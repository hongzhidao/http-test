/*
 * Copyright (C) Zhidao HONG
 */
#ifndef UTILS_H
#define UTILS_H

#define OK 0
#define ERROR -1
#define RETRY -2
#define DONE -3

struct url {
    char *scheme;
    char *host;
    char *port;
    char *path;
};

struct buf {
    char *start;
    char *free;
    char *pos;
    char *end;
    struct buf *next;
};

void *zmalloc(size_t);
void *zcalloc(size_t);
void *zrealloc(void *, size_t);
void zfree(void *);
int memcasecmp(const void *, const void *, size_t);
int parse_int(char *, size_t);
int parse_url(struct url *u, char *url);
struct addrinfo *addr_resolve(char *host, char *service);
struct buf *buf_alloc(size_t);
char *format_time(char *buf, uint64_t time);
char *format_byte(char *buf, size_t bytes);

#define max_int(val1, val2)                                                 \
    ((val1 < val2) ? (val2) : (val1))

#define min_int(val1, val2)                                                 \
    ((val1 > val2) ? (val2) : (val1))

#define pointer_to(p, offset)                                               \
    ((void *) ((char *) (p) + (offset)))

#define container_of(p, type, field)                                        \
    ((type *) ((uint8_t *) (p) - offsetof(type, field)))

#define countof(x)                                                          \
    (sizeof(x) / sizeof((x)[0]))

#define memzero(buf, n)                                                     \
    (void) memset(buf, 0, n)

#define lowcase(c)                                                          \
    (u_char) ((c >= 'A' && c <= 'Z') ? c | 0x20 : c)

static inline void *
cpymem(void *dst, void *src, size_t len)
{
    return ((char *) memcpy(dst, src, len)) + len;
}

/*
 * Since uint32_t values are stored just in 32 bits, they overflow
 * every 49 days.  This signed subtraction takes into account that overflow.
 * "msec_diff(m1, m2) < 0" means that m1 is lesser than m2.
 */
#define msec_diff(m1, m2)                                                     \
    ((int32_t) ((m1) - (m2)))

static inline uint64_t monotonic_time() {
    struct timespec  ts;

    (void) clock_gettime(CLOCK_MONOTONIC, &ts);

    return ts.tv_sec * 1000000000 + ts.tv_nsec;
}

#endif /* UTILS_H */
