CC = gcc
CFLAGS = -Wall -Wextra -std=gnu17

TARGET = hw3
SRCDIR = src
BUILDDIR = build

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
DEPS = $(OBJS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

clean:
	rm -rf $(TARGET) $(BUILDDIR)

-include $(DEPS)

.PHONY: all clean
