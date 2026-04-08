CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Werror -O2
TARGET = sql_processor

SRCS = \
	main.c \
	interfaces/cli.c \
	application/processor.c \
	domain/schema.c \
	domain/parser.c \
	infrastructure/file_io.c \
	infrastructure/storage.c \
	shared/text.c \
	shared/strvec.c \
	shared/error.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
