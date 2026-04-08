CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Werror -O2
CPPFLAGS =
LDFLAGS =
TARGET = sql_processor

READLINE_AVAILABLE := $(shell printf 'int main(void){return 0;}\n' | $(CC) -include readline/readline.h -include readline/history.h -x c - -lreadline -o /tmp/sql_processor_readline_test >/dev/null 2>&1 && echo 1 || echo 0)

ifeq ($(READLINE_AVAILABLE),1)
CPPFLAGS += -DUSE_READLINE
LDFLAGS += -lreadline
endif

SRCS = \
	main.c \
	interfaces/cli.c \
	interfaces/interactive.c \
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
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
