OS := $(shell uname)

ROOT_DIR = ../..
INCLUDE_DIR = $(ROOT_DIR)/include
BUILD_DIR = $(CURDIR)/build
LIB_DIR = $(BUILD_DIR)/debug
SOLUTION_DIR = ./solutions

INCS = -I$(INCLUDE_DIR)
CFLAGS = -I. -Wall -Werror -pedantic -fsanitize=address -g

ifeq ($(OS), Darwin)
CFLAGS += -framework Security
endif

LIBCRYPTO_DIR = $(dir $(shell find ${BUILD_DIR} -name libcrypto.a))
LIBSSL_DIR = $(dir $(shell find ${BUILD_DIR} -name libssl.a))

LDFLAGS = -L$(LIBCRYPTO_DIR) -L$(LIBSSL_DIR) -L$(LIB_DIR)

LIBS = $(LIB_DIR)/libquiche.a -lev -ldl -pthread -lm -lstdc++

all: client server

client: client.c $(INCLUDE_DIR)/quiche.h $(LIB_DIR)/libquiche.a $(LIB_DIR)/libsolution.so
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@ $(INCS) $(LIBS) $(LIB_DIR)/libsolution.so

server: server.c $(INCLUDE_DIR)/quiche.h $(LIB_DIR)/libquiche.a $(LIB_DIR)/libsolution.so
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@ $(INCS) $(LIBS) $(LIB_DIR)/libsolution.so

$(LIB_DIR)/libquiche.a: $(shell find $(SOURCE_DIR) -type f -name '*.rs')
	cd $(ROOT_DIR) && cargo build --target-dir $(BUILD_DIR) --features "boringssl-vendored interface fec"

$(LIB_DIR)/libsolution.so: $(SOLUTION_DIR)/solution.cxx $(SOLUTION_DIR)/include/solution.hxx
	cd $(SOLUTION_DIR) && g++ -shared -fPIC solution.cxx -I include -o libsolution.so
	mv $(SOLUTION_DIR)/libsolution.so $(LIB_DIR)/libsolution.so

clean:
	@$(RM) -rf client server build/