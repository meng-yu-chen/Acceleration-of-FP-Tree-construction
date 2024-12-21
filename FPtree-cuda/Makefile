NVCC = nvcc
CXXFLAGS=-I./common -Iobjs/ -O3 -std=c++17 -Wall -g -fPIC -lm

APP_NAME=tree
COMMONDIR=./include

CUDA_LINK_FLAGS =  -rdc=true -gencode=arch=compute_61,code=sm_61 -Xcompiler '-fPIC' 
CUDA_COMPILE_FLAGS = --device-c -gencode=arch=compute_61,code=sm_61 -Xcompiler '-fPIC' -g -O3

all: main.o FPTransMap.o FPRadixTree.o FPHeaderTable.o FPGrowth.o include
	$(NVCC) ${CUDA_LINK_FLAGS} -o tree FPTransMap.o FPRadixTree.o FPHeaderTable.o FPGrowth.o main.o

main.o: src/main.cu include
	$(NVCC) ${CUDA_COMPILE_FLAGS} -c src/main.cu -I./include

FPGrowth.o: src/FPGrowth.cu include 
	$(NVCC) ${CUDA_COMPILE_FLAGS} -c src/FPGrowth.cu -I./include

FPHeaderTable.o: src/FPHeaderTable.cu include 
	$(NVCC) ${CUDA_COMPILE_FLAGS} -c src/FPHeaderTable.cu -I./include

FPRadixTree.o: src/FPRadixTree.cu include 
	$(NVCC) ${CUDA_COMPILE_FLAGS} -c src/FPRadixTree.cu -I./include

FPTransMap.o: src/FPTransMap.cu include 
	$(NVCC) ${CUDA_COMPILE_FLAGS} -c src/FPTransMap.cu -I./include

run:
	srun ./tree

clean:
	$(RM) *.o *.s tree