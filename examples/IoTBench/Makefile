#cc = gcc
#prom = main
#deph = benchmark.h
#obj = main.o list_search_sort.o matrix.o conv.o
#source = main.c list_search_sort.c matrix.c conv.c

CC = gcc -static
ACC = aarch64-linux-gnu-gcc -static
RCC = riscv64-linux-gnu-gcc -static
OUTFLAG = -o


CORE_FILES  = list_search_sort conv main matrix 
ORIG_SRCS = $(addsuffix .c, $(CORE_FILES))
OBJS = $(addsuffix .o,$(CORE_FILES))
OUTFILE_X86 = iotbench-x86-$(SID)
OUTFILE_RISCV =  iotbench-RISCV-$(SID)
OUTFILE_ARM = iotbench-ARM-$(SID)

HEADERS = benchmark.h

.PHONY: riscv 
riscv: 
	$(RCC) $(OUTFLAG) $(OUTFILE_RISCV) $(ORIG_SRCS)
#%.o:%.c $(HEADERS)
#	$(RCC) -c $< $(OUTFLAG) $@


.PHONY: arm 
arm: 
	$(ACC) $(OUTFLAG) $(OUTFILE_ARM) $(ORIG_SRCS)
#%.o:%.c $(HEADERS)
#	$(ACC) -c $< $(OUTFLAG) $@

.PHONY: x86 
x86: 
	$(CC) $(OUTFLAG) $(OUTFILE_X86) $(ORIG_SRCS)

#$(prom):$(obj)
#	$(cc) -o $(prom) $(obj)

#%.o:%.c $(deps)
#	gcc -c $< -o $@

.PHONY: clean
clean:
	rm -f $(OUTFILE_ARM) $(OUTFILE_RISCV) $(OUTFILE_X86)
#clean:
#	rm -rf $(obj) $(prom)