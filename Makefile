CFLAGS = -Wall -Wextra -pedantic #-ansi
LDFLAGS = 

i: *.[ch]; $(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS) -O3
d: *.[ch]; $(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS) -ggdb
