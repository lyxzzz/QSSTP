.PHONY: all shell

all:shell

shell:
	@cd utils && make -f Makefile.real
	@cd sender && make -f Makefile.real
	@cd receiver && make -f Makefile.real
	@find . -name "*.log"|xargs rm -f
	@find . -name "core.*"|xargs rm -f

clean:
	@cd sender && make -f Makefile.real clean
	@cd receiver && make -f Makefile.real clean
	@cd utils && make -f Makefile.real clean

