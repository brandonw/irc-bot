irc-bot: bot.o plugins
	gcc bot.o main.o -o irc-bot

bot.o: bot.c bot.h main.c
	gcc -c bot.c main.c

.PHONY: plugins
plugins: join.c send_nick.c quit.c ping.c bot.c karma.c bot.h
	gcc -c join.c send_nick.c quit.c ping.c
	gcc -c `pkg-config --cflags --libs glib-2.0` karma.c

.PHONY: clean
clean : 
	rm *.o irc-bot
