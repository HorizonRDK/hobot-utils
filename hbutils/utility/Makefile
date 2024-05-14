ifeq ($(PARAM_FILE),) 
    PARAM_FILE:=../Makefile.param
    include $(PARAM_FILE)
endif

ifeq (${BUILD_OUTPUT_PATH},)
export BUILD_OUTPUT_PATH=$(shell pwd)
endif

.PHONY: all all_32 clean install uninstall

SRCS_C = $(wildcard ./*.c)
OBJS_ORI = $(SRCS_C:.c=)
OBJS = $(patsubst ./%,${BUILD_OUTPUT_PATH}/%,$(OBJS_ORI))

INCS += -I../
INCS += -I/opt/gcc-arm-11.2-2022.02-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/usr/include
LIBS = -lpthread

all all_32: $(OBJS)

${BUILD_OUTPUT_PATH}/veeprom.o: ../veeprom.c
	@mkdir -p $(abspath $(dir $@))
	${CC} ${CFLAGS} -c $< ${INCS} ${LDFLAGS} -o $@

${BUILD_OUTPUT_PATH}/%.o: ./%.c
	@mkdir -p $(abspath $(dir $@))
	${CC} ${CFLAGS} -c $< ${INCS} ${LDFLAGS} -o $@

${BUILD_OUTPUT_PATH}/%: ${BUILD_OUTPUT_PATH}/%.o ${BUILD_OUTPUT_PATH}/veeprom.o
	@mkdir -p $(abspath $(dir $@))
	${CC} ${CFLAGS} -o $@ $^ ${INCS} ${LDFLAGS}  $(LIBS)

clean:
	rm -rf $(OBJS) ${BUILD_OUTPUT_PATH}/*.o

install:
	@install -d ${BUILD_OUTPUT_PATH}/../_install
	install $(OBJS) ${BUILD_OUTPUT_PATH}/../_install

uninstall:
	rm -fr ${BUILD_OUTPUT_PATH}/../_install
