#include <cassert>
#include <cstdlib>
#include <iostream>
#include <set>
#include <vector>
#include <fstream>
#include <string>
#include <filesystem>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iomanip> 

// include openmp
#include <omp.h>

// include timer
#include "CycleTimer.h"
#include "fptree.hpp"

using namespace std;
namespace fs = std::filesystem;


// define Transaction_Data for one transaction, contain transactions and itemSets
struct Transaction_Data {
    vector<Transaction> transactions;
    set<Item> ItemSets;
};


// return Transaction_Data
Transaction_Data test_read_dataset_from_path(const string& dataPath){

    // store transactions
    Transaction_Data transaction_Data;


    ifstream file(dataPath);
    string line;
    while (getline(file, line)) {

        // 使用", "分割字串
        size_t pos = 0;
        string token;
        vector<Item> ItemSet;

        while ((pos = line.find(", ")) != string::npos) {

            token = Item(line.substr(0, pos));
            ItemSet.push_back(token);
            transaction_Data.ItemSets.insert(token);
            

            // erase already handle part
            line.erase(0, pos + 2); // 移除已處理的部分
        }

        // handle last part
        ItemSet.push_back(line);
        transaction_Data.ItemSets.insert(line);

        // push one transaction
        transaction_Data.transactions.push_back(ItemSet);
    }
    file.close();


    //// for debugging
    // output
    // cout << "Vector內容：" << endl;

    // // iterate each vector
    // for (const auto& vec : transaction_Data.transactions) {
        
    //     // iterate each element
    //     for (const auto& item : vec) {
    //         cout << item << " ";  
    //     }
    //     cout << endl;  
    // }

    // cout << "Set內容：" << endl;
    // for (const auto& item : transaction_Data.ItemSets) {
    //     cout << item << endl;
    // }

    return transaction_Data;
}



void test_1(Transaction_Data transaction_Data)
{

    cout << "  !!!!!!!! In test 1 function !!!!!!!!\n";

    const Item a{ "a" };
    const Item b{ "b" };
    const Item c{ "c" };
    const Item d{ "d" };
    const Item e{ "e" };

    // const vector<Transaction> transactions{
    //     { a, b },
    //     { b, c, d },
    //     { a, c, d, e },
    //     { a, d, e },
    //     { a, b, c },
    //     { a, b, c, d },
    //     { a },
    //     { a, b, c },
    //     { a, b, d },
    //     { b, c, e }
    // };


    const uint64_t minimum_support_threshold = 2;

    const FPTree fptree{ transaction_Data.transactions, minimum_support_threshold };

    const set<Pattern> patterns = fptree_growth( fptree );

    assert( patterns.size() == 19 );
    assert( patterns.count( { { a }, 8 } ) );
    assert( patterns.count( { { b, a }, 5 } ) );
    assert( patterns.count( { { b }, 7 } ) );
    assert( patterns.count( { { c, b }, 5 } ) );
    assert( patterns.count( { { c, a, b }, 3 } ) );
    assert( patterns.count( { { c, a }, 4 } ) );
    assert( patterns.count( { { c }, 6 } ) );
    assert( patterns.count( { { d, a }, 4 } ) );
    assert( patterns.count( { { d, c, a }, 2 } ) );
    assert( patterns.count( { { d, c }, 3 } ) );
    assert( patterns.count( { { d, b, a }, 2 } ) );
    assert( patterns.count( { { d, b, c }, 2 } ) );
    assert( patterns.count( { { d, b }, 3 } ) );
    assert( patterns.count( { { d }, 5 } ) );
    assert( patterns.count( { { e, d }, 2 } ) );
    assert( patterns.count( { { e, c }, 2 } ) );
    assert( patterns.count( { { e, a, d }, 2 } ) );
    assert( patterns.count( { { e, a }, 2 } ) );
    assert( patterns.count( { { e }, 3 } ) );
}

void test_2(Transaction_Data transaction_Data)
{

    cout << "  !!!!!!!! In test 2 function !!!!!!!!\n";

    const Item a{ "a" };
    const Item b{ "b" };
    const Item c{ "c" };
    const Item d{ "d" };
    const Item e{ "e" };

    // const vector<Transaction> transactions{
    //     { a, b, d, e },
    //     { b, c, e },
    //     { a, b, d, e },
    //     { a, b, c, e },
    //     { a, b, c, d, e },
    //     { b, c, d },
    // };

    const uint64_t minimum_support_threshold = 3;

    const FPTree fptree{ transaction_Data.transactions, minimum_support_threshold };

    const set<Pattern> patterns = fptree_growth( fptree );

    assert( patterns.size() == 19 );
    assert( patterns.count( { { e, b }, 5 } ) );
    assert( patterns.count( { { e }, 5 } ) );
    assert( patterns.count( { { a, b, e }, 4 } ) );
    assert( patterns.count( { { a, b }, 4 } ) );
    assert( patterns.count( { { a, e }, 4 } ) );
    assert( patterns.count( { { a }, 4 } ) );
    assert( patterns.count( { { d, a, b }, 3 } ) );
    assert( patterns.count( { { d, a }, 3 } ) );
    assert( patterns.count( { { d, e, b, a }, 3 } ) );
    assert( patterns.count( { { d, e, b }, 3 } ) );
    assert( patterns.count( { { d, e, a }, 3 } ) );
    assert( patterns.count( { { d, e }, 3 } ) );
    assert( patterns.count( { { d, b }, 4 } ) );
    assert( patterns.count( { { d }, 4 } ) );
    assert( patterns.count( { { c, e, b }, 3 } ) );
    assert( patterns.count( { { c, e }, 3 } ) );
    assert( patterns.count( { { c, b }, 4 } ) );
    assert( patterns.count( { { c }, 4 } ) );
    assert( patterns.count( { { b }, 6 } ) );
}

void test_3(Transaction_Data transaction_Data)
{
    const Item a{ "a" };
    const Item b{ "b" };
    const Item c{ "c" };
    const Item d{ "d" };
    const Item e{ "e" };
    const Item f{ "f" };
    const Item g{ "g" };
    const Item h{ "h" };
    const Item i{ "i" };
    const Item j{ "j" };
    const Item k{ "k" };
    const Item l{ "l" };
    const Item m{ "m" };
    const Item n{ "n" };
    const Item o{ "o" };
    const Item p{ "p" };
    const Item s{ "s" };

    // const vector<Transaction> transactions{
    //     { f, a, c, d, g, i, m, p },
    //     { a, b, c, f, l, m, o },
    //     { b, f, h, j, o },
    //     { b, c, k, s, p },
    //     { a, f, c, e, l, p, m, n }
    // };

    const uint64_t minimum_support_threshold = 3;

    const FPTree fptree{ transaction_Data.transactions, minimum_support_threshold };

    const set<Pattern> patterns = fptree_growth( fptree );

    cout << "patterns.size(): " << patterns.size() << '\n';
    

    assert( patterns.size() == 18 );
    assert( patterns.count( { { f }, 4 } ) );
    assert( patterns.count( { { c, f }, 3 } ) );
    assert( patterns.count( { { c }, 4 } ) );
    assert( patterns.count( { { b }, 3 } ) );
    assert( patterns.count( { { p, c }, 3 } ) );
    assert( patterns.count( { { p }, 3 } ) );
    assert( patterns.count( { { m, f, c }, 3 } ) );
    assert( patterns.count( { { m, f }, 3 } ) );
    assert( patterns.count( { { m, c }, 3 } ) );
    assert( patterns.count( { { m }, 3 } ) );
    assert( patterns.count( { { a, f, c, m }, 3 } ) );
    assert( patterns.count( { { a, f, c }, 3 } ) );
    assert( patterns.count( { { a, f, m }, 3 } ) );
    assert( patterns.count( { { a, f }, 3 } ) );
    assert( patterns.count( { { a, c, m }, 3 } ) );
    assert( patterns.count( { { a, c }, 3 } ) );
    assert( patterns.count( { { a, m }, 3 } ) );
    assert( patterns.count( { { a }, 3 } ) );
}

int main(int argc, char *argv[])
{

    // default one thread
    int thread_count = 1;
    int opt;

    // get number of thread from cmd
    if ((opt = getopt(argc, argv, "t:")) != -1) {

        switch (opt) {

            case 't':
                thread_count = atoi(optarg);  // 將參數轉換成整數並儲存
                break;

            default:
                fprintf(stderr, "Usage: %s -t <number>\n", argv[0]);
                exit(EXIT_FAILURE);

        }

    }


    printf("----------------------------------------------------------\n");
    printf("Max system threads = %d\n", omp_get_max_threads());
    if (thread_count > 0)
    {
        thread_count = min(thread_count, omp_get_max_threads());
    
    } else if (thread_count <= -1){

        // run all threads
        thread_count = omp_get_max_threads();
        
    }
    printf("Running with %d threads\n", thread_count);
    printf("----------------------------------------------------------\n");


    //Set thread count
    omp_set_num_threads(thread_count);


    /////////////////////////////////////////////////////////////////////////////////

    const string data_folderPath = "/workspace/FP-growth/dataset";


    for (auto& entry : fs::directory_iterator(data_folderPath)) {
        
        if (entry.path().extension() == ".txt") {
            // cout << "entry.path(): " << entry.path() << "\n";
            Transaction_Data transaction_Data = test_read_dataset_from_path(entry.path());

            if (entry.path() == "/workspace/FP-growth/dataset/test_data1.txt") {

                //Run implementations
                double start_time = CycleTimer::currentSeconds();
                test_1(transaction_Data);
                double exec_time = CycleTimer::currentSeconds() - start_time;
                cout << "Test case1 - " << thread_count << " thread: " << fixed << setprecision(4) << exec_time <<"s\n";
            
            } else if (entry.path() == "/workspace/FP-growth/dataset/test_data2.txt") {

                 //Run implementations
                double start_time = CycleTimer::currentSeconds();
                test_2(transaction_Data);
                double exec_time = CycleTimer::currentSeconds() - start_time;
                cout << "Test case2 - " << thread_count << " thread: " << fixed << setprecision(4) << exec_time <<"s\n";
                // cout << "skip test2." << endl;

            } else if (entry.path() == "/workspace/FP-growth/dataset/test_data3.txt")
                // test_3(transaction_Data);
                cout << "skip test3." << endl;
            
            else
                cout << "Unknown file name, no matching function." << endl;
            
        }
        //break;
    }
    
    // test_1();
    // test_2();
    // test_3();
    cout << "All tests passed!" << endl;

    return EXIT_SUCCESS;
}
