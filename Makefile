LDFLAGS = -ldl -lpthread

OBJ_DIR ?= ./obj
BIN_DIR ?= ./bin
SRC_DIRS ?= ./server

TARGET ?= bgdm.server
OUTPUT = $(TARGET:%=$(BIN_DIR)/%)

SRCS := $(filter-out core/input.c, $(wildcard core/*.c)) $(filter-out core/unix/mem.c core/unix/segv.c, $(wildcard core/unix/*.c)) $(wildcard server/*.c)
SRCS += sqlite3/sqlite3.c
#OBJS := $(addprefix $(OBJ_DIR)/,$(notdir $(SRCS:.c=.o)))
OBJS := $(SRCS:%.c=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := ./ $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

#CXXFLAGS ?= $(INC_FLAGS) -MMD -MP
CXXFLAGS ?= -D_XOPEN_SOURCE=500 -g -std=c11 -Wall -I./ -I./uthash/src
LDFLAGS := -ldl -lm -lpthread

OS := $(shell uname)
ifeq ($(OS),Darwin)
CXXFLAGS += -D_DARWIN_C_SOURCE
endif

build: $(OUTPUT)

clean:
	$(RM) -r $(OBJ_DIR)
	@#$(RM) -r $(BIN_DIR)
	$(RM) $(OUTPUT)
	@# ^^^ I don't recommend suppressing the echoing of the command using @

# http://www.gnu.org/software/make/manual/make.html#Phony-Targets
.PHONY: build clean all

$(OUTPUT): $(OBJS)
	@echo Linking $(BUILD_DIR)/$(TARGET)...
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) -o $@ $^
	@# ^^^ http://www.gnu.org/software/make/manual/make.html#Automatic-Variables
	@echo "Build successful!"

#$(OBJS): | $(OBJ_DIR) 

# http://www.gnu.org/software/make/manual/make.html#Static-Pattern
$(OBJS): $(OBJ_DIR)/%.o : %.c
	@echo $<
	@mkdir -p $(@D)
	@# ^^^ http://www.gnu.org/software/make/manual/make.html#index-_0024_0028_0040D_0029
	$(CC) $(CFLAGS) $(CXXFLAGS) -c -o $@ $<
	@# ^^^ Use $(CFLAGS), not $(LDFLAGS), when compiling.

$(OBJ_DIR):
	@mkdir -p $@

#-include $(DEPS)
