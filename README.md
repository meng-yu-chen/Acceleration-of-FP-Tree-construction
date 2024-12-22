### branch - jett 

* Use OpenMP to parallelize the FP-tree construction

* Parallelize the process by dividing it into two parts:        
    * Build frequency item set
    
    * Tree construction

#### How to run the code

* If you are using Docker, just follow the instructions below

    ```bash
    # Build the Docker image and create the container
    docker-compose up 

    # Navigate to the FP-growth directory
    cd FP-growth

    # Remove all compiled files
    make clean

    # Compile the program
    make

    # Run the parallel code on the dataset
    ./main -t {number_of_threads}
    ```

* If you are not using Docker, please ensure that your environment supports OpenMP and g++, then follow the instructions below

   * First: change the data file directory(data_folderPath) in main.cpp 

   * Then
       ```bash
       # Navigate to the FP-growth directory
       cd FP-growth
   
       # Remove all compiled files
       make clean
   
       # Compile the program
       make
   
       # Run the parallel code on the dataset
       ./main -t {number_of_threads}
       ```
