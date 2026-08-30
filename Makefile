CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude -ffast-math
PKG_SDL := sdl2
PKG_DRM := libdrm

SRC_COMMON := src/tiling.cpp src/window.cpp src/renderer.cpp src/renderer_ttf.cpp src/wallpaper.cpp src/main.cpp src/terminal.cpp src/compositor/display_server.cpp
LIB_CLIENT := lib/viewsun_client.cpp
EXAMPLES := examples/custom_app examples/custom_term examples/custom_browser examples/custom_files
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
CLIENT_OBJ := $(BUILD)/viewsun_client.o
EXAMPLE_OBJS := $(BUILD)/custom_app.o $(BUILD)/custom_term.o

all: $(BIN) $(EXAMPLES) libviewsun-client

$(BIN): $(OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(shell pkg-config --cflags wayland-server) -c $< -o $@

$(BUILD)/viewsun_client.o: lib/viewsun_client.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

libviewsun-client: $(CLIENT_OBJ)
	@mkdir -p build
	ar rcs build/libviewsun-client.a $(CLIENT_OBJ)

$(BUILD)/custom_app.o: examples/custom_app.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

examples/custom_app: $(BUILD)/custom_app.o $(CLIENT_OBJ)
	@mkdir -p examples
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD)/custom_term.o: examples/custom_term.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

examples/custom_term: $(BUILD)/custom_term.o $(CLIENT_OBJ)
	@mkdir -p examples
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD)/custom_browser.o: examples/custom_browser.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

examples/custom_browser: $(BUILD)/custom_browser.o $(CLIENT_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD)/custom_files.o: examples/custom_files.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

examples/custom_files: $(BUILD)/custom_files.o $(CLIENT_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -rf $(BUILD)
	rm -f examples/custom_app examples/custom_term

install: all
	install -Dm755 $(BIN) $(DESTDIR)$(BINDIR)/viewsun
	install -Dm644 man/viewsun.1 $(DESTDIR)$(MANDIR)/man1/viewsun.1
	install -Dm644 LICENSE $(DESTDIR)$(DATADIR)/LICENSE 2>/dev/null || true
	install -Dm644 README.md $(DESTDIR)$(DATADIR)/README.md
	install -Dm644 sessions/viewsun.desktop $(DESTDIR)/usr/share/xsessions/viewsun.desktop
	install -Dm644 sessions/viewsun.desktop $(DESTDIR)/usr/share/wayland-sessions/viewsun.desktop
	install -Dm755 sessions/viewsun-login.sh $(DESTDIR)$(DATADIR)/viewsun-login.sh
	@echo "viewsun installed to $(DESTDIR)$(BINDIR)/viewsun"
	@echo "Usage: viewsun -w /path/to/wallpaper.png"
	@echo "Login sessions installed - select Viewsun at display manager"

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/viewsun
	rm -f $(DESTDIR)$(MANDIR)/man1/viewsun.1
	rm -f $(DESTDIR)/usr/share/xsessions/viewsun.desktop
	rm -f $(DESTDIR)/usr/share/wayland-sessions/viewsun.desktop
	rm -f $(DESTDIR)$(DATADIR)/viewsun-login.sh

# arch package
pkg: all
	@echo "See pkg/PKGBUILD"

test: all
	./$(BIN) --help

.PHONY: all clean install uninstall pkg test
