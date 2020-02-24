{
	gcc ansiart_to_png.c -o ansiart_to_png -Wall -ansi -W -std=c99 -g -ggdb -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -lfuse -lmagic -lansilove;
	echo "COMPILED";
	mkdir ansiart;
	mkdir png;
	./ansiart_to_png -d ansiart png; # if you want to see version add -V
	echo "MOUNTED";
}
