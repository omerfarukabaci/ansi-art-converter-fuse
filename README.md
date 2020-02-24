# ANSI-Art-Converter-FUSE
File system that filters and converts the text files which include ANSI art in source directory to PNG images in destination directory on the fly on Linux, written in C. System Programming course project.

Project is explained in p2.pdf file.

Add ansi art files in the source directory to see PNG versions in the destination.


For compilation and mounting:

	 Compile: gcc ansiart_to_png.c -o ansiart_to_png -Wall -ansi -W -std=c99 -g -ggdb -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -lfuse -lmagic -lansilove
  
	 Mount: ./ansiart_to_png -d <src_path> <dst_path>;


There are 2 bash scripts:

	 mount.sh: compile, create 2 directories called ansi(source) and png(destination), mount ansi to png and run.
  
	 unmount.sh: unmount png, delete ansi and png directories.
