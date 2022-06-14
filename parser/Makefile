
OBJS=bits.o sps.o utils.o

CFLAGS=-O2 -Wall

LDFLAGS=

LIBS=

all: parser patch_sps_bit_res_flag

parser: $(OBJS) parser.o
	$(CC) -o parser parser.o $(OBJS) $(LDFLAGS) $(LIBS)

patch_sps_bit_res_flag: $(OBJS) patch_sps_bit_res_flag.o
	$(CC) -o patch_sps_bit_res_flag patch_sps_bit_res_flag.o $(OBJS) $(LDFLAGS) $(LIBS)

clean:
	rm -f parser patch_sps_bit_res_flag $(OBJS) parser.o patch_sps_bit_res_flag.o

