irc-bot: bot.o
	gcc `pkg-config --cflags --libs glib-2.0` bot.o -o irc-bot

bot.o: bot.c
	gcc -c `pkg-config --cflags --libs glib-2.0` bot.c -o bot.o


