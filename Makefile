CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lcurl -lcjson -lncurses
TARGET = btc_fee_visualizer
SRC = btc_fee_visualizer.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(TARGET)

.PHONY: all clean install uninstall
