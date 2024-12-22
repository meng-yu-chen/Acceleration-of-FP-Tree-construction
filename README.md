
### branch - cuda

* Use CUDA to parallelize the FP-tree construction

* Parallelize the process by dividing it into four parts:        
    * FPTransMap
    * FPRadixTree
    * FPHeaderTable
    * FPGrowth

#### How to run the code

 ```bash
       # Navigate to the FP-growth directory
       cd FPtree-cuda
   
       # Remove all compiled files
       make clean
   
       # Compile the program
       make

      # Run the parallel code on simple dataset
       ./tree ./dataset/test.txt

       # Run the parallel code on the dataset
       ./tree {dataset.txt}       
```
dataset:
   igh_100.txt
   low_100.txt
   uniform100.txt
   low_50.txt
   high_50.txt
   uniform50.txt