ifeq ($(PARAM_FILE),) 
    PARAM_FILE:=../Makefile.param
    include $(PARAM_FILE)
endif

ifeq (${BUILD_OUTPUT_PATH},)
export BUILD_OUTPUT_PATH=$(shell pwd)
endif

.PHONY: all clean install

SRCS = $(wildcard *.c)

OBJECTS := ${BUILD_OUTPUT_PATH}/hrut_cleanuserdata

LIBS = -lcjson
INCS += -I${SYSROOT_DIR}/usr/include/cjson
INCS += -I/opt/gcc-arm-11.2-2022.02-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/usr/include

all all_32: $(OBJECTS)

$(OBJECTS): $(SRCS)
	@mkdir -p $(abspath $(dir $@))
	$(CC) $(CFLAGS) $^ $(INCS) $(LDFLAGS) $(LIBS) -o $@

install:$(OBJECTS)
	@install -d ${BUILD_OUTPUT_PATH}/../_install
	install $(OBJECTS) ${BUILD_OUTPUT_PATH}/../_install

clean:
	rm -rf $(OBJECTS)
