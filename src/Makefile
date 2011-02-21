irc-bot: bot.o main.o plugins
	gcc -rdynamic bot.o main.o -o irc-bot -ldl

bot.o: bot.c bot.h
	gcc -c bot.c

main.o: main.c bot.h
	gcc -c main.c

.PHONY: plugins
plugins: join.c send_nick.c quit.c ping.c bot.c karma.c bot.h
	gcc -c join.c -o plugins/join.o
	gcc -c send_nick.c -o plugins/send_nick.o
	gcc -c quit.c -o plugins/quit.o
	gcc -c ping.c -o plugins/ping.o
	gcc -c `pkg-config --cflags --libs glib-2.0` karma.c -o plugins/karma.o

.PHONY: clean
clean : 
	rm *.o irc-bot plugins/*.o


