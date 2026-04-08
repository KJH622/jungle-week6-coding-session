CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Werror -O2
TARGET = sql_processor
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean
