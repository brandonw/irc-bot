irc-bot: bot.o main.o plugins
	gcc -rdynamic `pkg-config --cflags --libs glib-2.0` \
		bot.o main.o -o irc-bot -ldl

.PHONY: debug
debug:
	gcc -g -O0 -rdynamic -ldl `pkg-config --cflags --libs glib-2.0` \
		bot.c main.c -o irc-bot
	gcc -g -O0 -fPIC -c join.c -o plugins/join.o
	ld -shared -soname join.so -o plugins/join.so \
		-lc plugins/join.o
	gcc -g -O0 -fPIC -c send_nick.c -o plugins/send_nick.o
	ld -shared -soname send_nick.so -o plugins/send_nick.so \
		-lc plugins/send_nick.o
	gcc -g -O0 -fPIC -c quit.c -o plugins/quit.o
	ld -shared -soname quit.so -o plugins/quit.so \
		-lc plugins/quit.o
	gcc -g -O0 -fPIC -c ping.c -o plugins/ping.o
	ld -shared -soname ping.so -o plugins/ping.so \
		-lc plugins/ping.o
	gcc -g -O0 -fPIC -c `pkg-config --cflags --libs glib-2.0` \
		karma.c -o plugins/karma.o
	ld -shared -soname karma.so -o plugins/karma.so \
		-lc plugins/karma.o
	rm plugins/*.o

bot.o: bot.c bot.h
	gcc -c bot.c

main.o: main.c bot.h
	gcc -c main.c

.PHONY: plugins
plugins: join.c send_nick.c quit.c ping.c bot.c karma.c bot.h
	gcc -fPIC -c join.c -o plugins/join.o
	ld -shared -soname join.so -o plugins/join.so \
		-lc plugins/join.o
	gcc -fPIC -c send_nick.c -o plugins/send_nick.o
	ld -shared -soname send_nick.so -o plugins/send_nick.so \
		-lc plugins/send_nick.o
	gcc -fPIC -c quit.c -o plugins/quit.o
	ld -shared -soname quit.so -o plugins/quit.so \
		-lc plugins/quit.o
	gcc -fPIC -c ping.c -o plugins/ping.o
	ld -shared -soname ping.so -o plugins/ping.so \
		-lc plugins/ping.o
	gcc -fPIC -c `pkg-config --cflags --libs glib-2.0` \
		karma.c -o plugins/karma.o
	ld -shared -soname karma.so -o plugins/karma.so \
		-lc plugins/karma.o
	rm plugins/*.o

.PHONY: clean
clean : 
	rm *.o irc-bot plugins/*.o plugins/*.so


