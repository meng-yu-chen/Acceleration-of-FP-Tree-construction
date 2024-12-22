#include <cassert>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <set>
#include <vector>
#include <time.h>
#include <sstream>
#include <string>
#include <algorithm>
#include <mpi.h>

#include "../include/fptree.hpp"

std::vector<Transaction> readTransactionsFromFile(const std::string& filename) {
    std::vector<Transaction> transactions;

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open the file: " << filename << "!!!" << std::endl;
        return transactions;
    }
    //std::cerr << "Open the file: " << filename << " successfully !!!" << std::endl;
    std::string line;
    while (std::getline(file, line)) {
        std::vector<std::string> transaction;
        std::istringstream line_stream(line);
        std::string item;

        // 處理每一行的每個數字
        while (std::getline(line_stream, item, ',')) {
            item.erase(remove(item.begin(), item.end(), ' '), item.end()); // remove space
            if (!item.empty()) { // 不插入空char
                transaction.push_back(item); // insert item
            }
        }

        if (!transaction.empty()) { // 確保不插入空的交易
            transactions.push_back(transaction);
        }
    }

    file.close();
    return transactions;
}


void Build_tree(const std::string& filename)
{
    // read transaction data
    std::vector<Transaction> transactions = readTransactionsFromFile(filename);

    const uint64_t minimum_support_threshold = 2;

    const FPTree fptree{ transactions, minimum_support_threshold };
}


int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    int world_rank, world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    std::cout << "Build low_50\n";
    Build_tree("../dataset/low_50.txt");
    std::cout << "Build low_100\n";
    Build_tree("../dataset/low_100.txt");
    std::cout << "Build high_50\n";
    Build_tree("../dataset/high_50.txt");
    std::cout << "Build high_100\n";
    Build_tree("../dataset/high_100.txt");
    std::cout << "Build Uniform_50\n";
    Build_tree("../dataset/Uniform50.txt");
    std::cout << "Build Uniform_100\n";
    Build_tree("../dataset/Uniform100.txt");
    std::cout << "All FP_tree finished !!!\n";
    MPI_Finalize();
    return 0;
}
