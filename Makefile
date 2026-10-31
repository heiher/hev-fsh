# Makefile for hev-fsh

PROJECT=hev-fsh

CROSS_PREFIX :=
PP=$(CROSS_PREFIX)cpp
CC=$(CROSS_PREFIX)gcc
STRIP=$(CROSS_PREFIX)strip
CCFLAGS=-O3 -pipe -Wall -Werror $(CFLAGS) \
		-I$(SRCDIR)/misc \
		-I$(SRCDIR)/sock/include \
		-I$(THIRDPARTDIR)/hev-task-system/include
LDFLAGS=-L$(THIRDPARTDIR)/hev-task-system/bin -lhev-task-system \
		-lutil -pthread

CCFLAGS+=$(shell $(CC) -Werror -fno-stack-clash-protection -E -x c /dev/null \
           >/dev/null 2>&1 && echo -fno-stack-clash-protection)

SRCDIR=src
BINDIR=bin
BUILDDIR=build
THIRDPARTDIR=third-part

TARGET=$(BINDIR)/hev-fsh
THIRDPARTS=$(THIRDPARTDIR)/hev-task-system
EXEEXT=

# Native Windows: clang targeting MSVC, built with the very same rules.
# Run make from a Visual Studio developer prompt (x64, x86 or arm64) with
# a POSIX sh.exe on PATH, e.g. Git for Windows:
#   make ARCH=x64
ifeq ($(OS),Windows_NT)
ifeq ($(MSYSTEM),)
WIN_NATIVE=1
ARCH?=x64
CROSS_TARGET_x64=x86_64
CROSS_TARGET_x86=i686
CROSS_TARGET_arm64=aarch64
CC=clang --target=$(CROSS_TARGET_$(ARCH))-pc-windows-msvc
PP=$(CC)
STRIP=true
EXEEXT=.exe
TARGET=$(BINDIR)/hev-fsh$(EXEEXT)
CCFLAGS+=-DEXPORT_SYMBOL= -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS \
         -D_WINSOCK_DEPRECATED_NO_WARNINGS -D_WIN32_WINNT=0x0A00 \
         -DNTDDI_VERSION=0x0A000006 -Wno-microsoft-enum-forward-reference
LDFLAGS=-L$(THIRDPARTDIR)/hev-task-system/bin -lhev-task-system \
        -lkernel32 -lws2_32 -lbcrypt
ifneq ($(ARCH),arm64)
# Coroutine switching is incompatible with the CET shadow stack.
LDFLAGS+=-Wl,/CETCOMPAT:NO
endif
endif
endif

-include build.mk
CCFLAGS+=$(VERSION_CFLAGS)
ifneq ($(WIN_NATIVE),1)
TPFLAGS=ENABLE_STACK_OVERFLOW_DETECTOR=1
endif
CCSRCS=$(filter %.c,$(SRCFILES))
ASSRCS=$(filter %.S,$(SRCFILES))
LDOBJS=$(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(CCSRCS)) \
	   $(patsubst $(SRCDIR)/%.S,$(BUILDDIR)/%.o,$(ASSRCS))
DEPEND=$(LDOBJS:.o=.dep)

BUILDMSG="\e[1;31mBUILD\e[0m %s\n"
LINKMSG="\e[1;34mLINK\e[0m  \e[1;32m%s\e[0m\n"
STRIPMSG="\e[1;34mSTRIP\e[0m \e[1;32m%s\e[0m\n"
CLEANMSG="\e[1;34mCLEAN\e[0m %s\n"

ENABLE_DEBUG :=
ifeq ($(ENABLE_DEBUG),1)
	CCFLAGS+=-g -O0 -DENABLE_DEBUG
	STRIP=true
endif

ENABLE_STATIC :=
ifeq ($(ENABLE_STATIC),1)
	CCFLAGS+=-static
endif

V :=
ECHO_PREFIX := @
ifeq ($(V),1)
	undefine ECHO_PREFIX
endif

.PHONY: all clean tp-build tp-clean

all : $(TARGET)

tp-build : $(THIRDPARTS)
	@$(foreach dir,$^,$(MAKE) --no-print-directory -C $(dir) $(TPFLAGS);)

tp-clean : $(THIRDPARTS)
	@$(foreach dir,$^,$(MAKE) --no-print-directory -C $(dir) clean;)

clean : tp-clean
	$(ECHO_PREFIX) $(RM) -rf $(BINDIR) $(BUILDDIR)
	@printf $(CLEANMSG) $(PROJECT)

$(TARGET) : $(LDOBJS) tp-build
	$(ECHO_PREFIX) mkdir -p $(dir $@)
	$(ECHO_PREFIX) $(CC) $(CCFLAGS) -o $@ $(LDOBJS) $(LDFLAGS)
	@printf $(LINKMSG) $@
	$(ECHO_PREFIX) $(STRIP) $@
	@printf $(STRIPMSG) $@

$(BUILDDIR)/%.dep : $(SRCDIR)/%.c
	$(ECHO_PREFIX) mkdir -p $(dir $@)
	$(ECHO_PREFIX) $(PP) $(CCFLAGS) -MM -MT$(@:.dep=.o) -MF$@ $< 2> /dev/null

$(BUILDDIR)/%.o : $(SRCDIR)/%.c
	$(ECHO_PREFIX) mkdir -p $(dir $@)
	$(ECHO_PREFIX) $(CC) $(CCFLAGS) -c -o $@ $<
	@printf $(BUILDMSG) $<

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPEND)
endif
