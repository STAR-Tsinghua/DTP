OS := $(shell uname)

SOURCE_DIR = ../src
AITRANS_DIR = ../src/aitrans
AITRANS_LIB_DIR = ../src/aitrans
BUILD_DIR = $(CURDIR)/build
LIB_DIR = $(BUILD_DIR)/release
INCLUDE_DIR = ../include

INCS = -I$(INCLUDE_DIR) -I../src/aitrans/libtorch/include
CFLAGS = -I. -Wall -Werror -pedantic -fsanitize=address -O1 #-g

ifeq ($(OS), Darwin)
CFLAGS += -framework Security
endif

LIBCRYPTO_DIR = $(dir $(shell find ${BUILD_DIR} -name libcrypto.a))
LIBSSL_DIR = $(dir $(shell find ${BUILD_DIR} -name libssl.a))

LDFLAGS = -L$(LIBCRYPTO_DIR) -L$(LIBSSL_DIR) -L$(LIB_DIR) -L../src/aitrans/libtorch/lib

LIBS = $(LIB_DIR)/libquiche.a -lev -ldl -pthread -lm
CPPFLAGS =  -lstdc++

all: server client

server: server.c $(INCLUDE_DIR)/quiche.h $(LIB_DIR)/libquiche.a dtp_config.h
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@ $(INCS) $(LIBS) $(CPPFLAGS)

client: client.c $(INCLUDE_DIR)/quiche.h $(LIB_DIR)/libquiche.a dtp_config.h
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@ $(INCS) $(LIBS) $(CPPFLAGS)

$(LIB_DIR)/libquiche.a: $(shell find $(SOURCE_DIR) -type f -name '*.rs')
	cd .. && cargo build --release --target-dir $(BUILD_DIR)

clean:
	@$(RM) -rf server client build/
