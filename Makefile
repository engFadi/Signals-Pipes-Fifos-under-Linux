CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude
TARGET = main_app
OBJDIR = build

SRCS = \
	main.c \
	src/config/config.c \
	src/furniture/furniture.c \
	src/pipeline/pipeline.c \
	src/pipeline/pipeline_io.c \
	src/pipeline/pipeline_shared.c \
	src/pipeline/pipeline_roles.c

OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)

.PHONY: all clean
