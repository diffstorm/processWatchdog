#
# processWatchdog application makefile
# Copyright (c) 2023 Eray Ozturk <erayozturk1@gmail.com>
#
CC := gcc
STRIP := strip

TARGET_EXEC := processWatchdog

SRC_DIRS := src
DEPLOY_DIR := .

SRCS := $(shell find $(SRC_DIRS) -not -name 'main.c' -name '*.c')

INC_DIRS := $(shell find $(SRC_DIRS) -type d)

INC_FLAGS := $(addprefix -I,$(INC_DIRS))

WARNING_FLAGS := -pedantic -Wall -Wextra -Wno-missing-declarations -Wstrict-prototypes -Wpointer-arith -Wwrite-strings -Wbad-function-cast -Wformat-security -Wno-discarded-qualifiers -Wno-implicit-fallthrough -Wformat-nonliteral -Wmissing-format-attribute -Wno-unused-variable -Wno-unused-parameter -Wno-unused-function -Wno-ignored-qualifiers -Wno-strict-prototypes -Wno-bad-function-cast -Wno-pointer-sign
HARDENING_FLAGS := -fno-builtin -fvisibility=hidden -fstack-protector -fno-omit-frame-pointer -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-common -ffunction-sections -fdata-sections
PERFORMANCE_FLAGS := -fPIC -fPIE -ffast-math -fassociative-math -fno-signed-zeros -fno-trapping-math -fno-exceptions

#debug
#CFLAGS := $(INC_FLAGS) -g3 -o0 -ggdb $(WARNING_FLAGS)

#release
CFLAGS := $(INC_FLAGS) -g0 -o2  $(WARNING_FLAGS) $(HARDENING_FLAGS) $(PERFORMANCE_FLAGS)

LIBS := -lpthread -lm

all: $(TARGET_EXEC)

$(TARGET_EXEC):
	$(CC) $(SRCS) $(CFLAGS) $(EXTRA) -o $(DEPLOY_DIR)/$(TARGET_EXEC) $(SRC_DIRS)/main.c $(LIBS)
	$(STRIP) -R .comment -R *.note* -s -x -X -v $(DEPLOY_DIR)/$(TARGET_EXEC)

.PHONY: clean
clean:
	rm $(TARGET_EXEC)

.PHONY: install
install:
	cp $(TARGET_EXEC) ~/
	cp run.sh ~/
	chmod +x $(TARGET_EXEC) run.sh
