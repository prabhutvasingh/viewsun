CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude -ffast-math
PKG_SDL := sdl2
PKG_DRM := libdrm

SRC_COMMON := src/tiling.cpp src/window.cpp src/renderer.cpp src/wallpaper.cpp src/main.cpp
SRC_SDL := src/backend_sdl.cpp
SRC_DRM := src/backend_drm.cpp

# auto backend default: build both and link
BACKEND ?= auto

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man
DATADIR ?= $(PREFIX)/share/viewsun

BUILD := build
BIN := $(BUILD)/viewsun

ifeq ($(BACKEND),sdl)
  PKGS := $(PKG_SDL)
  SRC := $(SRC_COMMON) $(SRC_SDL)
  CXXFLAGS += -DBACKEND_SDL_ONLY
else ifeq ($(BACKEND),drm)
  PKGS := $(PKG_DRM)
  SRC := $(SRC_COMMON) $(SRC_DRM)
  CXXFLAGS += -DBACKEND_DRM_ONLY
else
  PKGS := $(PKG_SDL) $(PKG_DRM)
  SRC := $(SRC_COMMON) $(SRC_SDL) $(SRC_DRM)
endif

PKG_CFLAGS := $(shell pkg-config --cflags $(PKGS) 2>/dev/null)
PKG_LIBS   := $(shell pkg-config --libs $(PKGS) 2>/dev/null)

CXXFLAGS += $(PKG_CFLAGS)
LDLIBS   += $(PKG_LIBS)

OBJ := $(SRC:src/%.cpp=$(BUILD)/%.o)

all: $(BIN)

$(BIN): $(OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

install: all
	install -Dm755 $(BIN) $(DESTDIR)$(BINDIR)/viewsun
	install -Dm644 man/viewsun.1 $(DESTDIR)$(MANDIR)/man1/viewsun.1
	install -Dm644 LICENSE $(DESTDIR)$(DATADIR)/LICENSE 2>/dev/null || true
	install -Dm644 README.md $(DESTDIR)$(DATADIR)/README.md
	@echo "viewsun installed to $(DESTDIR)$(BINDIR)/viewsun"
	@echo "Usage: viewsun -w /path/to/wallpaper.png"

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/viewsun
	rm -f $(DESTDIR)$(MANDIR)/man1/viewsun.1

# arch package
pkg: all
	@echo "See pkg/PKGBUILD"

test: all
	./$(BIN) --help

.PHONY: all clean install uninstall pkg test
