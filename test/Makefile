INCLUDE=-I/home/eunhyangkim/os2016/linux/include
GCCROOT=/home/eunhyangkim/Documents/opt/tizen-toolchain-4.9~git-i686_armv7l-tizen-linux-gnueabi-20151113/bin
CFLAGS=$(INCLUDE)
CC=$(GCCROOT)/armv7l-tizen-linux-gnueabi-gcc

all: test trial trial_5times 

test: test.c
	@echo [Arm-cc] $<...
	@$(CC) $(CFLAGS) $< -o $@

trial_5times: trial_5times.c
	@echo [Arm-cc] $<...
	@$(CC) $(CFLAGS) $< -o $@

trial: trial.c
	@echo [Arm-cc] $<...
	@$(CC) $(CFLAGS) $< -o $@

clean:
	@rm test trial
