vpath %.c src
vpath %.h src

CFLAGS = -W -Wall
PKG_GLIB = `pkg-config --cflags --libs glib-2.0`
PD_DIR = plugins
SOURCES = bot.c main.c join.c send_nick.c quit.c ping.c karma.c

PLUGINS_O = $(PD_DIR)/join.o $(PD_DIR)/send_nick.o $(PD_DIR)/quit.o \
	    $(PD_DIR)/ping.o #$(PD_DIR)/karma.o
PLUGINS_SO = $(PD_DIR)/join.so $(PD_DIR)/send_nick.so $(PD_DIR)/quit.so \
	     $(PD_DIR)/ping.so $(PD_DIR)/karma.so

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

$(PLUGINS_O): $(PD_DIR)/%.o: %.c | $(PD_DIR)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(PD_DIR)/karma.o: karma.c | $(PD_DIR)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@ $(PKG_GLIB)

.PHONY: clean
clean : 
	-rm -rf *.o irc-bot $(PD_DIR)


#.PHONY: debug
#debug:
#	$(CC) -g -O0 -rdynamic -ldl `pkg-config --cflags --libs glib-2.0` \
#		bot.c main.c -o irc-bot
#	$(CC) -g -O0 -fPIC -c join.c -o plugins/join.o
#	ld -shared -soname join.so -o plugins/join.so \
#		-lc plugins/join.o
#	$(CC) -g -O0 -fPIC -c send_nick.c -o plugins/send_nick.o
#	ld -shared -soname send_nick.so -o plugins/send_nick.so \
#		-lc plugins/send_nick.o
#	$(CC) -g -O0 -fPIC -c quit.c -o plugins/quit.o
#	ld -shared -soname quit.so -o plugins/quit.so \
#		-lc plugins/quit.o
#	$(CC) -g -O0 -fPIC -c ping.c -o plugins/ping.o
#	ld -shared -soname ping.so -o plugins/ping.so \
#		-lc plugins/ping.o
#	$(CC) -g -O0 -fPIC -c `pkg-config --cflags --libs glib-2.0` \
#		karma.c -o plugins/karma.o
#	ld -shared -soname karma.so -o plugins/karma.so \
#		-lc plugins/karma.o
#	rm plugins/*.o
