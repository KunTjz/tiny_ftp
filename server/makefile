# makefile for root dir
# auther: richard
# time:   2013.10.6
include makefile.begin
export ROOTDIRS = $(shell pwd)
TARGET_DIRS =	util	\
				engine	\
				event	\
				log		\
				thread	\
				memory	\
				connection	\
				io	\
#				timer	\

EXE = tiny_ftp

FUNC = $(MAKE) -fPIC -o ./bin/$(EXE) ./obj/*.o -lpthread

include makefile.in

.PHONY:clean
clean:
	@-rm ./obj/*.o ./bin/$(EXE) -rvf

