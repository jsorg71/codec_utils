
OBJS=stepper.o

CFLAGS=-O2 -Wall -I/opt/yami/include -I/opt/yami/include/libyami

LDFLAGS=-L/opt/yami/lib -Wl,-rpath=/opt/yami/lib

LIBS=-lX11 -lva -lva-glx -lyami -lGL
#LIBS=-lX11 -lva -lva-x11 -lyami

all: stepper

stepper: $(OBJS)
	$(CC) -o stepper $(OBJS) $(LDFLAGS) $(LIBS)

clean:
	rm -f stepper $(OBJS)

