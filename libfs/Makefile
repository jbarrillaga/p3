lib := libfs.a
objs := disk.o fs.o
CC := gcc
CFLAGS := -Wall -Wextra -Werror

all: $(lib)

disk.o: disk.c
	$(CC) $(CFLAGS) -c -o disk.o disk.c

fs.o: fs.c
	$(CC) $(CFLAGS) -c -o fs.o fs.c

$(lib): $(objs)
	ar -rs $(lib) $(objs)
	
clean:
	rm -f $(lib) $(objs)
