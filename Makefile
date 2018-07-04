CXX ?= clang++
CC ?= clang
DEBUG ?= 0
PROFILE ?= 0

rwildcard = $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

SRCS = $(call rwildcard, src/, *.cpp *.c)
OBJS = $(filter %.o,$(SRCS:.cpp=.o) $(SRCS:.c=.o))
DEPS = $(filter %.d,$(SRCS:.cpp=.d) $(SRCS:.c=.d))

# Release builds /w optimize for size
CFLAGS_RELEASE = \
	-D_NDEBUG \
	-Os \
	-fomit-frame-pointer

# Profile builds /w slightly less aggressive optimization and /w debug symbols
CFLAGS_PROFILE = \
	-D_NDEBUG \
	-g3 \
	-O2 \
	-pg \
	-no-pie \
	-fno-inline-functions \
	-fno-inline-functions-called-once \
	-fno-optimize-sibling-calls

LDFLAGS_PROFILE = \
	-pg \
	-no-pie

# Debug builds enable trap and stack protector
CFLAGS_DEBUG = \
	-g3 \
	-O0 \
	-ftrapv \
	-fstack-protector-all \
	-fno-exceptions

LDFLAGS_DEBUG =

CFLAGS_COMMON = \
	-fstrict-aliasing \
	-Wall \
	-Wextra \
	-Wpointer-arith \
	-Wunreachable-code \
	-Wwrite-strings \
	-Winit-self \
	-Wno-format-truncation \
	-Wno-implicit-fallthrough \
	-Wno-cast-function-type \
	-Wno-unused-parameter \
	-MMD

CFLAGS_ONLY = \
	-Wno-discarded-qualifiers \

CXXFLAGS_COMMON = \
	-Wno-class-memaccess \
	-std=c++17

LDFLAGS_COMMON = \
	-ldl \
	-lpthread

CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_RELEASE)
LDFLAGS = $(LDFLAGS_COMMON) $(LDFLAGS_RELEASE)
STRIP = strip
BIN = kaizen-release

ifeq ($(PROFILE),1)
CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_PROFILE)
LDFLAGS = $(LDFLAGS_COMMON) $(LDFLAGS_PROFILE)
STRIP true
BIN = kaizen-profile
endif

ifeq ($(DEBUG), 1)
CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_DEBUG)
LDFLAGS = $(LDFLAGS_COMMON) $(LDFLAGS_DEBUG)
STRIP = true
BIN = kaizen-debug
endif

CXXFLAGS = $(CFLAGS) $(CXXFLAGS_COMMON)

all: $(BIN)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_ONLY) -c -o $@ $<

$(BIN): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@
	$(STRIP) $(BIN)

clean:
	rm -rf $(OBJS) $(DEPS) $(BIN)

.PHONY: clean

-include $(DEPS)
