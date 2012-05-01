BIN_NAME = irc-bot

PLUG_SRC += rmap.c
PLUG_SRC += urlget.c
# --------------------------------------------------------------------------

BIN_DIR = bin
OBJ_DIR = obj

vpath %.c src
vpath %.h src

CFLAGS += -W -Wall

# Bot source files
SRC += bot.c
SRC += main.c

# Plugin directory
PLUG_DIR = plugins

# Bot (not plugin) objects
OBJS = $(addprefix $(OBJ_DIR)/,\
		   $(patsubst %.c,%.o,$(SRC)))

PLUG_OBJ_DIR = $(OBJ_DIR)/$(PLUG_DIR)
PLUG_SO_DIR = $(BIN_DIR)/$(PLUG_DIR)

# Full plugin object file paths (default compilation)
PLUGINS_O = $(addprefix $(PLUG_OBJ_DIR)/,\
			    $(patsubst %.c,%.o,$(PLUG_SRC)))
# Full plugin shared object file paths
PLUGINS_SO = $(addprefix $(PLUG_SO_DIR)/,\
			 $(patsubst %.c,%.so,$(PLUG_SRC)))
# --------------------------------------------------------------------------

.PHONY: all
all: $(BIN_DIR)/$(BIN_NAME) irc-plugins cscope.out tags

$(BIN_DIR)/$(BIN_NAME): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -rdynamic $^ -o $@ -ldl -lcurl

$(OBJS) $(PLUGINS_O): bot.h

.PHONY: irc-plugins
irc-plugins: $(PLUGINS_SO)

$(OBJS): $(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(PLUGINS_O): $(PLUG_OBJ_DIR)/%.o: %.c | $(PLUG_OBJ_DIR)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(PLUGINS_SO): $(PLUG_SO_DIR)/%.so: $(PLUG_OBJ_DIR)/%.o | $(PLUG_SO_DIR)
	ld -shared -soname $(@F) -o $@ -lc $<

$(BIN_DIR):
	mkdir -p $(BIN_DIR)
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)
$(PLUG_OBJ_DIR):
	mkdir -p $(PLUG_OBJ_DIR)
$(PLUG_SO_DIR):
	mkdir -p $(PLUG_SO_DIR)

.PHONY: clean
clean :
	$(RM) -r $(BIN_DIR) $(OBJ_DIR) cscope.out tags

.PHONY: debug
debug: CFLAGS += -g -O0 -DDEBUG
debug: all

cscope.out: $(SRC) $(PLUG_SRC)
	cscope -R -b
tags: $(SRC) $(PLUG_SRC)
	ctags -R
