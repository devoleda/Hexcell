# Makefile for libsysdb

LIBRARY = libsysdb
TEST = testdb
TEST2 = resolv_test

AR = ar -rcs
INCLUDEDIR = -I.. -I. -I../shared -I../../include
CFLAGS ?= -g
LIBRARY_OBJ = sysdb.o sqlite3.o
TEST_OBJ = test.o
TEST_LINK_LIB = libuuid.a

.PHONY: all
all: $(LIBRARY).a
$(LIBRARY).a: $(LIBRARY_OBJ)
	@echo "    GEN_LIB $@"
	@$(AR) $@ $^

.c.o:
	@echo "    CC $@"
	@$(CC) $(CFLAGS) $(INCLUDEDIR) -c $< -o $@

.PHONY: test
test: $(TEST) $(TEST2)
$(TEST): $(TEST_OBJ) $(LIBRARY).a message.o utils.o $(TEST_LINK_LIB)
	@echo "    LD $@"
	@$(CC) $^ -lpthread -lncurses -ldl -o $@

message.o: ../shared/message.c
	@echo "    CC $@"
	@$(CC) $(CFLAGS) $(INCLUDEDIR) -c $< -o $@

utils.o: ../shared/utils.c
	@echo "    CC $@"
	@$(CC) $(CFLAGS) $(INCLUDEDIR) -c $< -o $@

libuuid.a:
	@cd ../libuuid && $(MAKE)
	@cp ../libuuid/libuuid.a .

$(TEST2): resolv_test.o utils.o message.o list.o $(LIBRARY).a libuuid.a
	@echo "    LD $@"
	@$(CC) $^ -pthread -lncurses -ldl -o $@

list.o: ../shared/list.c
	@echo "    CC $@"
	@$(CC) $(CFLAGS) $(INCLUDEDIR) -c $< -o $@

.PHONY: clean test-clean
clean:
	@-rm *.o $(LIBRARY).a

test-clean:
	@-rm *.o $(TEST) *.a $(TEST2)
	@-cd ../libuuid && $(MAKE) clean