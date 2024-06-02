/*
 * Copyright (C) Zhidao HONG
 */
#include "headers.h"

#define VERSION "0.1.0"

static void parse_args(int, char **);
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

    parse_args(argc, argv);

    threads = threads_create();
    if (threads == NULL) {
        return 1;
    }

    printf("Testing %d threads and %d connections\n@ %s for %ds\n",
           cfg.threads, cfg.connections, cfg.target, cfg.duration);

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
    printf("Usage: %s [-t value] [-c value] [-d value] [-v] [-h] url\n", prog);

    printf("Options:\n");
    printf(" -t value   Set the value of threads\n");
    printf(" -c value   Set the value of connections\n");
    printf(" -d value   Set the value of duration\n");
    printf(" -v         print the version information\n");
    printf(" -h         print this usage message\n");
    printf(" url        The required URL to test\n");

    exit(status);
}


static void
parse_args(int argc, char **argv)
{
    int opt, val;
    char *url;

    cfg.threads = 2;
    cfg.connections = 10;
    cfg.duration = 10;
    cfg.timeout = 2000000;

    while ((opt = getopt(argc, argv, "t:c:d:vh")) != -1) {
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

        case 'v':
            printf("Version %s Copyright (C) Zhidao HONG\n", VERSION);
            exit(0);
            return;

        case 'h':
            print_usage(argv[0], 0);
            return;

        default:
            print_usage(argv[0], 1);
            return;
        }
    }

    if (cfg.connections < cfg.threads) {
        printf("Invalid option: connections must be larger than threads\n");
        exit(1);
        return;
    }

    if (optind == argc) {
        printf("Invalid option: url is required\n");
        print_usage(argv[0], 1);
        return;
    }

    url = argv[optind];

    if (parse_url(&cfg.url, url)) {
        printf("Invalid option: url \"%s\" is invalid\n", url);
        exit(1);
        return;
    }

    cfg.target = url;

    char *host = cfg.url.host;
    char *service = cfg.url.port ? cfg.url.port : cfg.url.scheme;
    struct addrinfo *addr = addr_resolve(host, service);

    if (addr == NULL) {
        char *msg = strerror(errno);
        printf("connect(\"%s:%s\") failed: %s\n", host, service, msg);
        exit(1);
        return;
    }

    cfg.addr = addr;
    return;

fail:
    print_usage(argv[0], 1);
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
    int num;

    thr->engine = t->engine;

    thr->time = monotonic_time();
    thr->engine->timers.now = thr->time / 1000000;

    num = cfg.connections / cfg.threads;
    conns = zcalloc(sizeof(struct conn) * num);
    if (conns == NULL) {
        return NULL;
    }

    for (int i = 0; i < num; i++) {
        c = &conns[i];

        c->write = http_request_create();
        if (c->write == NULL) {
            return NULL;
        }

        c->read = buf_alloc(8192);
        if (c->read == NULL) {
            return NULL;
        }

        http_peer_connect(c);
    }

    event_engine_start(thr->engine);

    return NULL;
}
