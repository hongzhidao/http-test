/*
 * Copyright (C) Zhidao HONG
 */
#include "headers.h"

#define VERSION "0.2.0"

static int config_init(int, char **);
struct thread *threads_create(void);
static void *thread_start(void *);

struct config cfg;
__thread struct thread thread_ctx;

static volatile sig_atomic_t stop = 0;

static void sigint_handler(int sig)
{
    stop = 1;
}


int main(int argc, char **argv)
{
    int i;
    uint64_t start, used;
    struct thread *t, *threads;

    if (config_init(argc, argv)) {
        return 1;
    }

    threads = threads_create();
    if (threads == NULL) {
        return 1;
    }

    printf("Testing %d threads and %d connections\n@ %s for %ds\n",
           cfg.threads, cfg.connections, cfg.url, cfg.duration);

    start = monotonic_time() / 1000;
    signal(SIGINT, sigint_handler);
    sleep(cfg.duration);

    for (i = 0; i < cfg.threads; i++) {
        t = &threads[i];
        pthread_cancel(t->handle);
    }

    used = monotonic_time() / 1000 - start;

    status_report(threads, used);

    return 0;
}


static void
print_usage(char *prog, int status)
{
    printf("Usage: %s [-t value] [-c value] [-d value] [-H header] [-s file]"
           " [-v] [-h] url\n", prog);

    printf("Options:\n");
    printf(" -t value   Set the value of threads\n");
    printf(" -c value   Set the value of connections\n");
    printf(" -d value   Set the value of duration\n");
    printf(" -H header  Set the request header\n");
    printf(" -s file    Set the script file\n");
    printf(" -v         print the version information\n");
    printf(" -h         print this usage message\n");
    printf(" url        The required URL to test\n");

    exit(status);
}


static int
parse_args(int argc, char **argv)
{
    int opt, val;
    http_field *field, **last_field;

    last_field = &cfg.headers;

    while ((opt = getopt(argc, argv, "t:c:d:H:s:vh")) != -1) {
        switch (opt) {
        case 't':
            val = parse_int(optarg, strlen(optarg));
            if (val <= 0) {
                printf("Invalid thread %d\n", val);
                goto fail;
            }
            cfg.threads = val;
            break;

        case 'c':
            val = parse_int(optarg, strlen(optarg));
            if (val <= 0) {
                printf("Invalid connections %d\n", val);
                goto fail;
            }
            cfg.connections = val;
            break;

        case 'd':
            val = parse_int(optarg, strlen(optarg));
            if (val <= 0) {
                printf("Invalid duration %d\n", val);
                goto fail;
            }
            cfg.duration = val;
            break;

        case 'H':
            field = zcalloc(sizeof(http_field));
            if (field == NULL) {
                return ERROR;
            }

            if (http_header_parse(field, optarg)) {
                printf("Invalid header %s\n", optarg);
                goto fail;
            }

            *last_field = field;
            last_field = &field->next;
            break;

        case 's':
            cfg.script = optarg;
            break;

        case 'v':
            printf("Version %s Copyright (C) Zhidao HONG\n", VERSION);
            return DONE;

        case 'h':
            print_usage(argv[0], 0);
            return DONE;

        default:
            print_usage(argv[0], 1);
            return ERROR;
        }
    }

    if (optind == argc) {
        printf("Invalid option: url is required\n");
        goto fail;
    }

    if (cfg.connections < cfg.threads) {
        printf("Invalid option: connections must be larger than threads\n");
        return ERROR;
    }

    cfg.url = argv[optind];

    return OK;

fail:
    print_usage(argv[0], 1);
    return ERROR;
}


static int
config_init(int argc, char **argv)
{
    char *service;
    struct url u;
    struct addrinfo *addr;

    cfg.threads = 2;
    cfg.connections = 10;
    cfg.duration = 10;
    cfg.timeout = 2000000;

    switch (parse_args(argc, argv)) {
    case DONE:
        exit(0);

    case OK:
        break;

    case ERROR:
        return -1;
    }

    if (parse_url(&u, cfg.url)) {
        printf("Invalid option: url \"%s\" is invalid\n", cfg.url);
        return -1;
    }

    service = u.port ? u.port : u.scheme;
    addr = addr_resolve(u.host, service);

    if (addr == NULL) {
        char *msg = strerror(errno);
        printf("connect(\"%s:%s\") failed: %s\n", u.host, service, msg);
        return -1;
    }

    cfg.host = u.host;
    cfg.path = u.path;
    cfg.addr = addr;

    return 0;
}


struct thread *
threads_create()
{
    int i;
    struct thread *t, *threads;

    threads = zcalloc(sizeof(struct thread) * cfg.threads);
    if (threads == NULL) {
        return NULL;
    }

    for (i = 0; i < cfg.threads; i++) {
        t = &threads[i];

        t->engine = event_engine_create(128);
        if (t->engine == NULL) {
            return NULL;
        }

        t->lua = script_create();
        if (t->lua == NULL) {
            return NULL;
        }

        if (pthread_create(&t->handle, NULL, thread_start, t)) {
            return NULL;
        }
    }
    
    return threads;
}


static void *
thread_start(void *data)
{
    struct thread *t = data;
    struct thread *thr = cur_thread();
    struct conn *conns, *c;
    int i, num;

    thr->engine = t->engine;
    thr->lua = t->lua;

    thr->time = monotonic_time();
    thr->engine->timers.now = thr->time / 1000000;

    num = cfg.connections / cfg.threads;
    conns = zcalloc(sizeof(struct conn) * num);
    if (conns == NULL) {
        return NULL;
    }

    thr->has_request = script_has_function(thr->lua, "request");

    for (i = 0; i < num; i++) {
        c = &conns[i];

        c->read = buf_alloc(8192);
        if (c->read == NULL) {
            return NULL;
        }

        if (!thr->has_request) {
            lua_getglobal(thr->lua, "http");
            c->write = script_request(thr->lua);
            if (c->write == NULL) {
                return NULL;
            }
        }

        http_peer_connect(c);
    }

    event_engine_start(thr->engine);

    return NULL;
}
