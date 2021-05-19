OS := $(shell uname)

SOURCE_DIR = ../src
AITRANS_DIR = ../src/aitrans
AITRANS_LIB_DIR = ../src/aitrans/include
BUILD_DIR = $(CURDIR)/build
LIB_DIR = $(BUILD_DIR)/release
INCLUDE_DIR = ../include

INCS = -I$(INCLUDE_DIR)
CFLAGS = -I. -Wall -Werror -pedantic -fsanitize=address -O1 #-g

ifeq ($(OS), Darwin)
CFLAGS += -framework Security
endif

LIBCRYPTO_DIR = $(dir $(shell find ${BUILD_DIR} -name libcrypto.a))
LIBSSL_DIR = $(dir $(shell find ${BUILD_DIR} -name libssl.a))

LDFLAGS = -L$(LIBCRYPTO_DIR) -L$(LIBSSL_DIR) -L$(LIB_DIR)

LIBS = $(LIB_DIR)/libquiche.a -lev -ldl -pthread -lm -lsolution
CPPFLAGS =  -lstdc++

all: server

image_test: build
	cd ../docker && ./image_test.sh

build: pre_docker
	cd ../docker && ./build_image.sh

pre_docker: server
	cd ../docker && ./pre_docker.sh

server: server.c $(INCLUDE_DIR)/quiche.h $(LIB_DIR)/libquiche.a dtp_config.h $(LIB_DIR)/libsolution.so
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@ $(INCS) $(LIBS) $(CPPFLAGS)

$(LIB_DIR)/libsolution.so: $(AITRANS_DIR)/solution.cxx $(AITRANS_LIB_DIR)/solution.hxx
	cd $(AITRANS_DIR) && g++ -shared -fPIC solution.cxx -I include -o libsolution.so
	mv $(AITRANS_DIR)/libsolution.so $(LIB_DIR)/libsolution.so

$(LIB_DIR)/libquiche.a: $(shell find $(SOURCE_DIR) -type f -name '*.rs') $(AITRANS_DIR)/pre_solution.cxx $(AITRANS_LIB_DIR)/pre_solution.hxx
	cd .. && cargo build --release --target-dir $(BUILD_DIR)

clean:
	@$(RM) -rf client client_trace server http3-client http3-server build/
