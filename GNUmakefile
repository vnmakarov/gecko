PREFIX=/usr/local
SRC_DIR=.
BUILD_DIR=.

YACC=yacc

ADDITIONAL_INCLUDE_PATH:=
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_S),Darwin)
    XCRUN := $(shell xcrun --show-sdk-path >/dev/null 2>&1 && echo yes || echo no)
    ifeq ($(XCRUN),yes)
        ADDITIONAL_INCLUDE_PATH := $(shell xcrun --show-sdk-path)/usr/include
    endif
endif

LDFLAGS =
OBJO=-o #trailing space is important
EXEO=-o #trailing space is important
ifeq ($(OS),Windows_NT)
  OBJSUFF=obj
  LIBSUFF=lib
  EXE=.exe
  ifeq ($(CC),cc)
    CC=cl
  endif
  ifeq ($(CC),gcc)
    CFLAGS += -g -std=gnu11 -Wno-abi -fsigned-char
    COPTFLAGS = -O2 -DNDEBUG
    CDEBFLAGS =
    CDEB2FLAGS = -Wall -Wextra -g3 -dwarf4 -fsanitize=address -fsanitize=undefined -fno-sanitize=alignment
    CFLAGS += $(COPTFLAGS)
    LDFLAGS=-Wl,--stack,8388608
    LD2FLAGS= $(LDFLAGS) -fsanitize=address -fsanitize=undefined -fno-sanitize=alignment
    GP_LIBS=-lm -lkernel32 -lpsapi
  else ifeq ($(CC),cl)
    COPTFLAGS = -O2 -DNDEBUG
    CDEBFLAGS = -Od -Z7
    CDEB2FLAGS = $(CDEBFLAGS)
    CFLAGS += -nologo $(COPTFLAGS)
    LDFLAGS= -nologo -F 8388608
    LD2FLAGS= $(LDFLAGS)
    GP_LIBS=
    OBJO=-Fo:
    EXEO=-Fe:
  endif

  CPPFLAGS = -I$(SRC_DIR)
  LDLIBS   = $(GP_LIBS)
  COMPILE = $(CC) $(CPPFLAGS) $(CFLAGS)
  ifeq ($(CC),gcc)
    COMPILE += -MMD -MP
  endif
  LINK = $(CC) $(LDFLAGS)
  COMPILE_AND_LINK = $(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)

else
  OBJSUFF=o
  LIBSUFF=a
  EXE=
  CC=gcc
  CFLAGS += -g -std=gnu11 -Wno-abi -fsigned-char
  ifneq ($(ADDITIONAL_INCLUDE_PATH),)
    CFLAGS += -DADDITIONAL_INCLUDE_PATH=\"$(ADDITIONAL_INCLUDE_PATH)\"
  endif

  GP_LIBS=-lm -ldl
  COPTFLAGS = -O3 -DNDEBUG
  CDEBFLAGS =
  CDEB2FLAGS = -Wall -Wextra -Wshadow -g3 -fsanitize=address -fsanitize=undefined -fno-sanitize=alignment
  LD2FLAGS =  -fsanitize=address -fsanitize=undefined  -fno-sanitize=alignment
  CFLAGS += $(COPTFLAGS)
  CPPFLAGS = -I$(SRC_DIR)
  LDLIBS   = $(GP_LIBS)

  COMPILE = $(CC) $(CPPFLAGS) -MMD -MP $(CFLAGS)
  LINK = $(CC) $(LDFLAGS)
  COMPILE_AND_LINK = $(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)
endif

Q=@

# Entries should be used for building and installation
.PHONY: all debug install uninstall clean test bench

all: $(BUILD_DIR)/libgecko.$(LIBSUFF)

debug: CFLAGS:=$(subst $(COPTFLAGS),$(CDEBFLAGS),$(CFLAGS))
debug: all

debug2: CFLAGS:=$(subst $(COPTFLAGS),$(CDEB2FLAGS),$(CFLAGS))
debug2: LDFLAGS:=$(LD2FLAGS)
debug2: all

install: $(BUILD_DIR)/libgecko.$(LIBSUFF) | $(PREFIX)/include $(PREFIX)/lib
	install -m a+r $(SRC_DIR)/gecko.h $(PREFIX)/include
	install -m a+r $(BUILD_DIR)/libgecko.$(LIBSUFF) $(PREFIX)/lib

$(PREFIX)/include $(PREFIX)/lib:
	   mkdir -p $@

uninstall: $(BUILD_DIR)/libgecko.$(LIBSUFF) $(EXECUTABLES) | $(PREFIX)/include $(PREFIX)/lib
	$(RM) $(PREFIX)/include/gecko.h
	$(RM) $(PREFIX)/lib/libgecko.$(LIBSUFF)
	-rmdir $(PREFIX)/include $(PREFIX)/lib
	-rmdir $(PREFIX)

clean: clean-gecko

test: gecko-test

# ------------------ LIBGECKO -----------------------
$(BUILD_DIR)/libgecko.$(LIBSUFF): $(BUILD_DIR)/gecko.$(OBJSUFF)
ifeq ($(OS),Windows_NT)
	lib -nologo $^ -OUT:$@
else
	$(AR) rcs $@ $^
endif

# ------------------ GECKO --------------------------
GP_SRC:=$(SRC_DIR)/gecko.c
GP_BUILD:=$(GP_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.$(OBJSUFF))

$(GP_BUILD): $(BUILD_DIR)/sgramm.c
$(BUILD_DIR)/sgramm.c: $(SRC_DIR)/sgramm.y
	$(YACC) $(SRC_DIR)/sgramm.y
	mv y.tab.c $@

.PHONY: clean-gecko
clean-gecko:
	$(RM) $(GP_BUILD) $(GP_BUILD:.$(OBJSUFF)=.d)

-include $(GP_BUILD:.$(OBJSUFF)=.d)

# ------------------ gecko tests --------------------------

.PHONY: gecko-test

test: CFLAGS:=$(subst $(COPTFLAGS),$(CDEB2FLAGS),$(CFLAGS))
test: $(BUILD_DIR)/sgramm.c
test: simple-test more-tests
simple-test:
	$(CC) $(CFLAGS) $(SRC_DIR)/gecko.c -DGP_TEST -o $(BUILD_DIR)/gp-test$(EXE)
	$(BUILD_DIR)/gp-test$(EXE) 1 2
	@echo simple test is OK

more-tests:
	$(SRC_DIR)/test/test.sh

bench: $(BUILD_DIR)/libgecko.$(LIBSUFF)
	$(SRC_DIR)/test/compare.sh

