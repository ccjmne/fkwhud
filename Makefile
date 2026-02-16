CC = gcc

SRC = fkwhud.c
TARGET = fkwhud

PKG_CFLAGS = $(shell pkg-config --cflags gtk4 gtk4-layer-shell-0)
PKG_LIBS   = $(shell pkg-config --libs   gtk4 gtk4-layer-shell-0)

CFLAGS  = $(PKG_CFLAGS) -Os -ffunction-sections -fdata-sections -flto
LDFLAGS = $(PKG_LIBS)   -Wl,--as-needed -Wl,--gc-sections -flto

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

release: $(TARGET)
	strip --strip-unneeded $(TARGET)

clean:
	rm -f $(TARGET)
