BIN_NAME = irc-bot

PLUG_SRC += join.c
PLUG_SRC += send_nick.c
PLUG_SRC += quit.c
PLUG_SRC += ping.c
PLUG_SRC += karma.c

# Specially compiled plugin source
SPE_PLUG_SRC += karma.c
# Default compiled plugin source
DEF_PLUG_SRC = $(filter-out $(SPE_PLUG_SRC),$(PLUG_SRC))

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
DEF_PLUGINS_O = $(addprefix $(PLUG_OBJ_DIR)/,\
			    $(patsubst %.c,%.o,$(DEF_PLUG_SRC)))
# Full plugin object file paths (special compilation)
SPE_PLUGINS_O = $(addprefix $(PLUG_OBJ_DIR)/,\
			    $(patsubst %.c,%.o,$(SPE_PLUG_SRC)))
# Full plugin shared object file paths
PLUGINS_SO = $(addprefix $(PLUG_SO_DIR)/,\
			 $(patsubst %.c,%.so,$(PLUG_SRC)))

PKG_GLIB = `pkg-config --cflags --libs glib-2.0`

# --------------------------------------------------------------------------

.PHONY: all
all: $(BIN_DIR)/$(BIN_NAME) irc-plugins

$(BIN_DIR)/$(BIN_NAME): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -rdynamic $(PKG_GLIB) $^ -o $@ -ldl
	
$(OBJS) $(PLUGINS_O): bot.h

.PHONY: irc-plugins
irc-plugins: $(PLUGINS_SO)

$(OBJS): $(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(DEF_PLUGINS_O): $(PLUG_OBJ_DIR)/%.o: %.c | $(PLUG_OBJ_DIR)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(PLUGINS_SO): $(PLUG_SO_DIR)/%.so: $(PLUG_OBJ_DIR)/%.o | $(PLUG_SO_DIR)
	ld -shared -soname $(@F) -o $@ -lc $<

# Compile special plugins here
# --------------------------------------------------------------------------
# Link karma.o with glib to ensure acces to HashTable functions
$(PLUG_OBJ_DIR)/karma.o: karma.c | $(PLUG_OBJ_DIR)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@ $(PKG_GLIB)
# --------------------------------------------------------------------------

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
	$(RM) -r $(BIN_DIR) $(OBJ_DIR)

.PHONY: debug
debug: CFLAGS += -g -O0
debug: all
