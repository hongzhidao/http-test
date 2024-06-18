BUILD = build
CFLAGS = -g -O0 -Wall -Wmissing-prototypes -Werror -D_GNU_SOURCE
CFLAGS += -MMD -MF $(BUILD)/$(@F).d
LDFLAGS = -g -Wl,-E
LIBS = -lm -lpthread -ldl -lssl -lcrypto

PROG = test
SRCS = utils.c rbtree.c epoll.c timer.c event_engine.c \
       hdr_histogram.c http_parse.c conn.c ssl.c http.c script.c status.c main.c
OBJS = $(patsubst %.c,$(BUILD)/%.o,$(SRCS))

LUA = lua-5.4.6
DEPS = $(BUILD)/lib/liblua.a
CFLAGS += -I$(BUILD)/include
LDFLAGS += -L$(BUILD)/lib
LIBS := -llua $(LIBS)

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -rf $(PROG) $(BUILD)/

$(BUILD):
	mkdir -p $@

$(OBJS): $(DEPS)

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

-include $(wildcard $(BUILD)/*.d)

$(BUILD)/lib/liblua.a: $(LUA)
	$(SHELL) -c "cd $< && make && make install INSTALL_TOP=$(abspath $(BUILD))"

.PHONY: all clean
vpath %.h src
vpath %.c src
