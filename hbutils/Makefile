ifeq ($(SYSROOT_DIR),)
    SYSROOT_DIR := $(abspath ../../../deploy)/rootfs
    export SYSROOT_DIR
endif

ifeq ($(PARAM_FILE),) 
    PARAM_FILE:=./Makefile.param
    include $(PARAM_FILE)
endif

ifeq (${BUILD_OUTPUT_PATH},)
export BUILD_OUTPUT_PATH=$(shell pwd)
endif

.PHONY: all install clean uninstall

all:
	${MAKE} BUILD_OUTPUT_PATH=${BUILD_OUTPUT_PATH}/hrut_cleanuserdata -C hrut_cleanuserdata
	${MAKE} BUILD_OUTPUT_PATH=${BUILD_OUTPUT_PATH}/utility -C utility
	${MAKE} BUILD_OUTPUT_PATH=${BUILD_OUTPUT_PATH}/bpu_utility -C bpu_utility
	${MAKE} BUILD_OUTPUT_PATH=${BUILD_OUTPUT_PATH}/hbm_utility -C hbm_utility

install:
	${MAKE} BUILD_OUTPUT_PATH=${BUILD_OUTPUT_PATH}/hrut_cleanuserdata -C hrut_cleanuserdata install
	${MAKE} BUILD_OUTPUT_PATH=${BUILD_OUTPUT_PATH}/utility -C utility install
	${MAKE} BUILD_OUTPUT_PATH=${BUILD_OUTPUT_PATH}/bpu_utility -C bpu_utility install
	${MAKE} BUILD_OUTPUT_PATH=${BUILD_OUTPUT_PATH}/hbm_utility -C hbm_utility install

clean:
	${MAKE} BUILD_OUTPUT_PATH=${BUILD_OUTPUT_PATH}/hrut_cleanuserdata -C hrut_cleanuserdata clean
	${MAKE} BUILD_OUTPUT_PATH=${BUILD_OUTPUT_PATH}/utility -C utility clean
	${MAKE} BUILD_OUTPUT_PATH=${BUILD_OUTPUT_PATH}/bpu_utility -C bpu_utility clean
	${MAKE} BUILD_OUTPUT_PATH=${BUILD_OUTPUT_PATH}/hbm_utility -C hbm_utility clean

uninstall:
	rm -fr ${BUILD_OUTPUT_PATH}/_install
