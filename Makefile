CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -Iinclude
TARGET = main_app
OBJDIR = build

SRCS = \
	src/main/main.c \
	src/visual/visual.c \
	src/config/config.c \
	src/furniture/furniture.c \
	src/pipeline/pipeline.c \
	src/pipeline/pipeline_io.c \
	src/pipeline/pipeline_shared.c \
	src/pipeline/pipeline_roles.c

OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) -lGL -lGLU -lglut -lm

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)

.PHONY: all clean
