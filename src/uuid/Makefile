# Makefile for libuuid

AR = ar rc
INCLUDEDIR = -I. -I../
CFLAGS ?= -O2
LIBRARY = libuuid

OBJS = clear.o compare.o copy.o gen_uuid.o isnull.o pack.o parse.o \
		unpack.o unparse.o uuid_time.o

.PHONY: all

all: $(LIBRARY).a

.c.o:
	@echo "    CC $<"
	@$(CC) $(INCLUDEDIR) $(CFLAGS) -c $< -o $@

$(LIBRARY).a: $(OBJS)
	@echo "    GEN_LIB $@"
	-@$(AR) $@ $^

.PHONY: clean

clean:
	-@rm *.o $(LIBRARY).a