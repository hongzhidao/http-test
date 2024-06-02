BUILD = build
CFLAGS = -g -O0 -Wall -Wmissing-prototypes -Werror -D_GNU_SOURCE
CFLAGS += -MMD -MF $(BUILD)/$(@F).d
LDFLAGS = -g -Wl,-E
LIBS = -lm -lpthread -ldl

PROG = test
SRCS = utils.c rbtree.c epoll.c timer.c event_engine.c \
       hdr_histogram.c http_parse.c conn.c http.c status.c main.c
OBJS = $(patsubst %.c,$(BUILD)/%.o,$(SRCS))

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -rf $(PROG) $(BUILD)/

$(BUILD):
	mkdir -p $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

-include $(wildcard $(BUILD)/*.d)

.PHONY: all clean
vpath %.h src
vpath %.c src
