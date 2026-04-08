CC = cc
CFLAGS = -Wall -Wextra -Werror -std=c11
TARGET = sql_processor

SRCS = main.c util.c sql_splitter.c parser.c executor.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
