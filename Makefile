# Compilador y banderas
CC = gcc
CFLAGS = -Wall -Wextra -O2 -g \
         -DGDK_VERSION_MAX_ALLOWED=GDK_VERSION_3_24 \
         -DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_68 \
         -DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_68 \
         `pkg-config --cflags gtk+-3.0` \
         `pkg-config --cflags libnotify` \
         `pkg-config --cflags cairo` \
         `pkg-config --cflags gmodule-2.0` \
         -I./include

LDFLAGS = -lcurl -lcjson -lm -lsqlite3 -lpthread \
          `pkg-config --libs gtk+-3.0` \
          `pkg-config --libs libnotify` \
          `pkg-config --libs cairo` \
          `pkg-config --libs gmodule-2.0`

# Nombres de los ejecutables
TARGET = btc_fee_visualizer
GUI_TARGET = btc_fee_gui

# Directorios
SRC_DIR = src
BUILD_DIR = build

# Archivos fuente
SRC = $(wildcard $(SRC_DIR)/*.c)
GUI_SRC = $(filter-out $(SRC_DIR)/btc_fee_visualizer.c, $(SRC))

# Archivos objeto
OBJ = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))
GUI_OBJ = $(filter-out $(BUILD_DIR)/btc_fee_visualizer.o, $(OBJ))

# Crear directorio de construcción si no existe
$(shell mkdir -p $(BUILD_DIR))

.PHONY: all clean gui cli

all: gui

# Objetivo para construir solo la interfaz gráfica
gui: $(GUI_TARGET)

# Objetivo para construir solo la versión de línea de comandos
cli: $(TARGET)

# Regla para el objetivo de línea de comandos
$(TARGET): $(BUILD_DIR)/btc_fee_visualizer.o
	$(CC) -o $@ $^ $(LDFLAGS)

# Regla para el objetivo con interfaz gráfica
$(GUI_TARGET): $(filter-out $(BUILD_DIR)/btc_fee_visualizer.o, $(OBJ))
	$(CC) -o $@ $^ $(LDFLAGS)

# Regla para compilar archivos fuente en objetos
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

# Limpiar archivos generados
clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(GUI_TARGET)

# Instalar las dependencias necesarias
setup:
	sudo apt-get update
	sudo apt-get install -y build-essential cmake libgtk-3-dev \
		libcurl4-openssl-dev libcjson-dev libnotify-dev

# Instalar la aplicación
install: gui
	install -d $(HOME)/.local/bin
	install -m 755 $(GUI_TARGET) $(HOME)/.local/bin/$(GUI_TARGET)
	@echo "Aplicación instalada en ~/.local/bin/$(GUI_TARGET)"

# Desinstalar la aplicación
uninstall:
	rm -f $(HOME)/.local/bin/$(GUI_TARGET)
	@echo "Aplicación desinstalada"
	rm -f $(DESTDIR)/usr/local/bin/$(TARGET)
	rm -f $(DESTDIR)/usr/local/bin/$(GUI_TARGET)
	rm -rf $(DATA_DIR)
	rm -rf $(CACHE_DIR)

.PHONY: all clean install uninstall

.PHONY: all clean install uninstall
