CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -fopenmp 

main:
	$(CXX) $(CXXFLAGS) -o main -I include src/parallel_fptree.cpp src/main.cpp -ltbb

clean:
	rm -rf main
