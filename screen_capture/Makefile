
OBJS=x11_x264_capture.o

CFLAGS=-O2 -Wall -Wextra

LIBS=-lX11

LDFLAGS=

x11_x264_capture: $(OBJS)
	$(CC) -o x11_x264_capture $(LDFLAGS) $(LIBS) $(OBJS)

clean:
	rm -r -f x11_x264_capture $(OBJS)
