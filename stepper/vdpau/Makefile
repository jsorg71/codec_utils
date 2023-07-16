
OBJS=stepper.o

CFLAGS=-O2 -Wall

LDFLAGS=

LIBS=-lX11 -lvdpau

all: stepper

stepper: $(OBJS)
	$(CC) -o stepper $(OBJS) $(LDFLAGS) $(LIBS)

clean:
	rm -f stepper $(OBJS)

