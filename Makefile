#
# This file is part of the stfuboot project.
#
# Copyright (C) 2012 Innovative Converged Devices(ICD)
# 
# Author(s):
# 	Andrey Smirnov <andrey.smirnov@convergeddevices.net>
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.


PREFIX	?= arm-none-eabi
CC 	:= $(PREFIX)-gcc

CFLAGS ?= -Os -g -Waddress -Warray-bounds -Wchar-subscripts -Wenum-compare 	\
          -Wimplicit-int -Wimplicit-function-declaration -Wcomment 		\
          -Wformat -Wmain -Wmissing-braces -Wnonnull -Wparentheses		\
	  -Wpointer-sign -Wreturn-type -Wsequence-point -Wsign-compare		\
	  -Wstrict-aliasing -Wstrict-overflow=1 -Wswitch -Wtrigraphs		\
	  -Wuninitialized -Wunknown-pragmas -Wunused-function -Wunused-label	\
	  -Wunused-value -Wvolatile-register-var -Wextra -fno-common -mthumb	\
	  -mcpu=cortex-m3 -msoft-float -MD

CFLAGS += -DSTM32F1 -Ilibopencm3/include

LDFLAGS ?= -T bootloader.ld -Wl,--start-group -lc -lgcc -Wl,--end-group 	\
	   -nostartfiles -Wl,--gc-sections -mthumb -mcpu=cortex-m3 -msoft-float

LIBS = -Llibopencm3/lib -lopencm3_stm32f1

# Be silent per default, but 'make V=1' will show all compiler calls.
ifneq ($(V),1)
Q := @
# Do not print "Entering directory ...".
MAKEFLAGS += --no-print-directory
endif

# common objects
OBJS += uart.o printf.o dfu.o main.o vectors.o

all: stfuboot.bin

stfuboot.bin: stfuboot.elf
	$(Q)$(PREFIX)-objcopy -Obinary $< $@

stfuboot.elf: $(OBJS)
	$(Q)$(CC) -o $@ $(LDFLAGS) $(OBJS) $(LIBS)

%.o: %.c
	@printf "  CC      $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(Q)rm -f *.o *.d ../*.o ../*.d

.PHONY: clean
