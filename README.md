# Acceleration-of-FP-Tree-construction (Open MPI)

This repository contains a C++11 implementation of the well-known FP-growth algorithm, published in the hope that it will be useful. I tested the code on three different samples and results were checked against [this other implementation](http://www.borgelt.net/fpgrowth.html) of the algorithm and use MPI method to construct FP-tree in parallel.

The files `fptree.hpp` and `fptree.cpp` contain the data structures and the algorithm, and `main.cpp` contains a few tests.

The file `MPI_tree.cpp` contains the FP-tree parallelized using the MPI method.

Compile the code using the appropriate options for C++11 (e.g. `-std=c++11` using g++).


### branch

* Use MPI to parallelize the FP-tree construction

* Parallelize the process by dividing it into two parts:        
    * Build frequency item set
    
    * Tree construction

#### How to run the code

* Please ensure that your environment supports MPI and g++, then follow the instructions below

   * First: change the data file directory(data_folderPath) in MPI_main.cpp 

   * Then
       ```bash
       # Navigate to the FP-growth directory
       cd src
   
       # Remove all compiled files
       make clean
   
       # Compile the program
       make
   
       # Run the parallel code on the dataset
      srun --mpi=pmix -n {number_of_processors} main
       ```


