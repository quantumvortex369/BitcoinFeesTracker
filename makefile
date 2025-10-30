CC = gcc
CFLAGS = -Wall -Wextra -O2 -g \
         -DGDK_VERSION_MAX_ALLOWED=GDK_VERSION_3_24 \
         -DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_68 \
         -DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_68 \
         `pkg-config --cflags gtk+-3.0` \
         `pkg-config --cflags libnotify` \
         `pkg-config --cflags cairo` \
         `pkg-config --cflags gmodule-2.0` \
         -I./

LDFLAGS = -lcurl -lcjson -lm -lsqlite3 -lpthread \
          `pkg-config --libs gtk+-3.0` \
          `pkg-config --libs libnotify` \
          `pkg-config --libs cairo` \
          `pkg-config --libs gmodule-2.0`
TARGET = btc_fee_visualizer
GUI_TARGET = btc_fee_gui
SRC = btc_fee_visualizer.c
GUI_SRC = btc_fee_gui.c chart_utils.c ui_utils.c
DATA_DIR = $(HOME)/.local/share/btc-fee-tracker
CACHE_DIR = $(HOME)/.cache/btc-fee-tracker

all: $(TARGET) $(GUI_TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ -lcurl -lcjson -lncurses

$(GUI_TARGET): $(GUI_SRC) chart_utils.h ui_utils.h
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(GUI_TARGET)

install: $(TARGET) $(GUI_TARGET)
	install -d $(DESTDIR)/usr/local/bin
	install -d $(DATA_DIR)
	install -d $(CACHE_DIR)
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/$(TARGET)
	install -m 755 $(GUI_TARGET) $(DESTDIR)/usr/local/bin/$(GUI_TARGET)
	cp -n config.json $(DATA_DIR)/ 2>/dev/null || true

uninstall:
	rm -f $(DESTDIR)/usr/local/bin/$(TARGET)
	rm -f $(DESTDIR)/usr/local/bin/$(GUI_TARGET)
	rm -rf $(DATA_DIR)
	rm -rf $(CACHE_DIR)

.PHONY: all clean install uninstall

.PHONY: all clean install uninstall
