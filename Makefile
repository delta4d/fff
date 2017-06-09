SRC=fff.c
BIN=fff
DST=/usr/local/bin

all:
	gcc $(SRC) -std=c99 -Wall -pedantic -lncurses -o $(BIN)

install:
	install -d $(DST)
	install -p -m 0755 $(BIN) $(DST)

uninstall:
	rm $(DST)/$(BIN)

clean:
	rm $(BIN)
