/*
 * Copyright (C) Zhidao HONG
 */
#ifndef STATUS_H
#define STATUS_H

struct status {
    uint64_t bytes;
    hdr_histogram *latency;
    uint32_t connect_errors;
    uint32_t read_errors;
    uint32_t write_errors;
    uint32_t timeouts;
};

struct status *status_create(void);
void status_report(struct thread *, uint64_t time);

#endif /* STATUS_H */
