USE_OPENSSL := no

BUILD := $(shell date -u +%Y%m%d-%H%M%S)
OS := $(shell uname -s)

PREFIX = $(DESTDIR)/opt/DigestPlay
TOOLKIT = ../..

DIRECTORIES = \
  $(TOOLKIT)/Common \
  $(TOOLKIT)/Rewind

ifeq ($(OS), Linux)
  FLAGS += -rdynamic
  KIND := $(shell grep -E "^6.0" /etc/debian_version > /dev/null ; echo $?)
ifneq ($(KIND), 0)
  LIBRARIES += rt
endif
ifeq ($(USE_OPENSSL), yes)
  FLAGS += -DUSE_OPENSSL
  DEPENDENCIES += openssl
endif
endif

OBJECTS = \
  RewindClient.o \
  DigestPlay.o

ifneq ($(USE_OPENSSL), yes)
  OBJECTS += sha256.o
endif

FLAGS += -g -fno-omit-frame-pointer -O3 -MMD $(foreach directory, $(DIRECTORIES), -I$(directory)) -DBUILD=\"$(BUILD)\"
LIBS += $(foreach library, $(LIBRARIES), -l$(library))

CC = gcc
CFLAGS += $(FLAGS) -std=gnu99

ifneq ($(strip $(DEPENDENCIES)),)
  FLAGS += $(shell pkg-config --cflags $(DEPENDENCIES))
  LIBS += $(shell pkg-config --libs $(DEPENDENCIES))
endif

all: build

build: $(PREREQUISITES) $(OBJECTS)
	$(CC) $(OBJECTS) $(FLAGS) $(LIBS) -o digestplay

install:
	install -D -d $(PREFIX)
	install -o root -g root digestplay $(PREFIX)

clean:
	rm -f $(PREREQUISITES) $(OBJECTS) digestplay
	rm -f *.d $(TOOLKIT)/*/*.d

version:
	echo "#define VERSION $(shell date -u +%Y%m%d)" > Version.h

debian-package:
	./UpdateLog.sh
ifdef ARCH
	dpkg-buildpackage -b -a$(ARCH) -tc
else
	dpkg-buildpackage -b -tc
endif

.PHONY: all build clean install
