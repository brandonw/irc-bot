vpath %.c src
vpath %.h src

CFLAGS = -W -Wall
PKG_GLIB = `pkg-config --cflags --libs glib-2.0`
PD_DIR = plugins
SOURCES = bot.c main.c join.c send_nick.c quit.c ping.c karma.c

PLUGINS_O = $(PD_DIR)/join.o $(PD_DIR)/send_nick.o $(PD_DIR)/quit.o \
	    $(PD_DIR)/ping.o $(PD_DIR)/karma.o
PLUGINS_O_STANDARD = $(PLUGINS_O)
PLUGINS_O_STANDARD := $(subst $(PD_DIR)/karma.o,,$(PLUGINS_O_STANDARD))
PLUGINS_SO = $(PLUGINS_O:%.o=%.so)

.PHONY: all
all: irc-bot irc-plugins

irc-bot: bot.o main.o
	$(CC) $(CFLAGS) -rdynamic $(PKG_GLIB) $^ -o $@ -ldl
	
bot.o main.o $(PLUGINS_O): bot.h

.PHONY: irc-plugins
irc-plugins: $(PLUGINS_SO)

$(PD_DIR):
	mkdir $(PD_DIR)

$(PLUGINS_SO): %.so: %.o
	ld -shared -soname $(@F) -o $@ -lc $<

$(PLUGINS_O_STANDARD): $(PD_DIR)/%.o: %.c | $(PD_DIR)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

# Link karma.o with glib to ensure acces to HashTable functions
$(PD_DIR)/karma.o: karma.c | $(PD_DIR)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@ $(PKG_GLIB)

.PHONY: clean
clean : 
	-rm -rf *.o irc-bot $(PD_DIR)

.PHONY: debug
debug: CFLAGS += -g -O0
debug: all
