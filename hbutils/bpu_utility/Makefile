ifeq ($(PARAM_FILE),) 
    PARAM_FILE:=../Makefile.param
    include $(PARAM_FILE)
endif

ifeq (${BUILD_OUTPUT_PATH},)
export BUILD_OUTPUT_PATH=$(shell pwd)
endif

.PHONY: all all_32 clean

CFLAGS :=-Wall -fPIC -shared -O2 --sysroot=

SRCS = $(wildcard *.c)

OBJECT := ${BUILD_OUTPUT_PATH}/hrut_bpu

INCS += -I../
LIBS = -lpthread -lrt

all all_32: $(OBJECT)

$(OBJECT): $(SRCS)
	@mkdir -p $(abspath $(dir $@))
	${CC} ${CFLAGS} -o $@ $^ ${INCS} $(LIBS)

install:$(OBJECT)
	@install -d ${BUILD_OUTPUT_PATH}/../_install
	install $(OBJECT) ${BUILD_OUTPUT_PATH}/../_install

clean:
	rm -rf $(OBJECT)
