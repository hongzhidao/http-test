/*
 * Copyright (C) Zhidao HONG
 */
#include "headers.h"

static void print_request(struct status *, uint64_t);
static void print_latency(hdr_histogram *);
static void print_errors(struct status *);


struct status *
status_create(void)
{
    struct status *status;

    status = zcalloc(sizeof(struct status));
    if (status == NULL) {
        return NULL;
    }

    hdr_init(1, cfg.timeout, 3, &status->latency);

    return status;
}


void status_report(struct thread *threads, uint64_t time)
{
    int i;
    struct thread *t;
    struct status *status, *stats;

    status = status_create();
    if (status == NULL) {
        return;
    }

    hdr_init(1, cfg.timeout, 3, &status->latency);

    for (i = 0; i < cfg.threads; i++) {
        t = &threads[i];
        stats = t->engine->status;

        status->bytes += stats->bytes;
        hdr_add(status->latency, stats->latency);

        status->connect_errors += stats->connect_errors;
        status->read_errors += stats->read_errors;
        status->write_errors += stats->write_errors;
        status->timeouts += stats->timeouts;
    }

    print_request(status, time);
    print_latency(status->latency);
    print_errors(status);
}


static void print_request(struct status *status, uint64_t time) {
    char buf1[20], buf2[20];
    hdr_histogram *latency;

    latency = status->latency;

    uint64_t requests, bytes;
    requests = latency->total_count;
    bytes = status->bytes;

    format_byte(buf1, bytes);
    format_time(buf2, time);

    printf("\n%lu requests and %s bytes in %s\n", requests, buf1, buf2);
    printf("  Requests/sec  %lu\n", requests / cfg.duration);
    printf("  Transfer/sec  %s\n", format_byte(buf1, bytes / cfg.duration));
}


static double
stdev_percent(hdr_histogram *hdr, double lower, double upper)
{
    int sum;
    struct hdr_iter iter;

    sum = 0;
    hdr_iter_init(&iter, hdr);

    while (hdr_iter_next(&iter)) {
        if (iter.count_at_index == 0) {
            continue;
        }

        if (iter.value_from_index >= lower && iter.value_from_index <= upper) {
            sum += iter.count_at_index;
        }
    }

    return (double) sum / hdr->total_count;
}


static void print_latency(hdr_histogram *hdr) {
    int i;
    char buf[20];
    double mean, stdev, max;
    double lower, upper, percent;
    int percents[] = {50, 75, 90, 99};

    mean = hdr_mean(hdr);
    stdev = hdr_stddev(hdr);
    max = hdr_max(hdr);

    lower = mean - stdev;
    upper = mean + stdev;
    percent = stdev_percent(hdr, lower, upper);

    printf("\nLatency:\n");
    printf("  Mean      %s\n", format_time(buf, mean));
    printf("  Stdev     %s\n", format_time(buf, stdev));
    printf("  Max       %s\n", format_time(buf, max));
    printf("  +/-Stdev  %.2f%%\n", percent * 100);

    printf("\nLatency Distribution:\n");

    for (i = 0; i < countof(percents); i++) {
        int percent = percents[i];
        format_time(buf, hdr_value_at_percentile(hdr, percent));
        printf("  %d%%  %s\n", percent, buf);
    }
}


static void print_errors(struct status *status) {
    uint32_t errors = status->connect_errors
                      + status->read_errors
                      + status->write_errors
                      + status->timeouts;

    if (errors > 0) {
        double percent = (double) errors / status->latency->total_count;

        printf("\nErrors:\n");
        printf("  Connect  %u\n", status->connect_errors);
        printf("  Read     %u\n", status->read_errors);
        printf("  Write    %u\n", status->write_errors);
        printf("  Timeout  %u\n", status->timeouts);
        printf("  Percent  %.2f\n", percent * 100);
    }
}
