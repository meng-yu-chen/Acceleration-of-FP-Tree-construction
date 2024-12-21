#include <algorithm>
#include <cassert>
#include <cstdint>
#include <utility>

#include <cuda_runtime.h>
#include <thrust/device_vector.h>
#include <thrust/host_vector.h>
#include <thrust/set_operations.h>
#include <set>
#include <vector>
#include <map>
#include <string>
#include <cassert>

#include "fptree.hpp"
#include "CycleTimer.h"

FPNode::FPNode(const Item& item, const std::shared_ptr<FPNode>& parent) :
    item( item ), frequency( 1 ), node_link( nullptr ), parent( parent ), children()
{
}

__global__ void calculate_frequencies(const int* d_flattened_transactions, 
                                      int num_transactions, 
                                      int* d_frequency, 
                                      int flattened_size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < flattened_size) {
        atomicAdd(&d_frequency[d_flattened_transactions[idx]], 1);
    }
}

__global__ void filter_items(const int* d_frequency, 
                             int* d_filtered_items, 
                             int num_items, 
                             int minimum_support_threshold) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < num_items) {
        d_filtered_items[idx] = (d_frequency[idx] >= minimum_support_threshold) ? 1 : 0;
    }
}

FPTree::FPTree(const std::vector<Transaction>& transactions, uint64_t minimum_support_threshold) :
    root( std::make_shared<FPNode>( Item{}, nullptr ) ), header_table(),
    minimum_support_threshold( minimum_support_threshold )
{
   //static int what = 0;
    // Flatten transactions and prepare offsets
    std::map<Item, int> item_to_index;
    std::vector<int> flattened_transactions;
    
    int current_index = 0;

    double startTime = CycleTimer::currentSeconds();

    for (const auto& transaction : transactions) {
        for (const auto& item : transaction) {
            if (item_to_index.find(item) == item_to_index.end()) {
                item_to_index[item] = current_index++;
            }
            flattened_transactions.push_back(item_to_index[item]);
        }
    }

    int num_items = item_to_index.size();
    int flattened_size = flattened_transactions.size();

    // Allocate CUDA memory
    int* d_flattened_transactions;
    int* d_frequency;
    int* d_filtered_items;
    cudaMalloc(&d_flattened_transactions, flattened_size * sizeof(int));
    cudaMalloc(&d_frequency, num_items * sizeof(int));
    cudaMalloc(&d_filtered_items, num_items * sizeof(int));
    cudaMemset(d_frequency, 0, num_items * sizeof(int));

    // Copy data to device
    cudaMemcpy(d_flattened_transactions, flattened_transactions.data(), flattened_size * sizeof(int), cudaMemcpyHostToDevice);

    // Launch frequency calculation kernel
    int blockSize = 256;
    int numBlocks = (flattened_size + blockSize - 1) / blockSize;
    calculate_frequencies<<<numBlocks, blockSize>>>(d_flattened_transactions, 
                                                    transactions.size(), 
                                                    d_frequency, 
                                                    flattened_size);
    cudaDeviceSynchronize();

    // Launch filtering kernel
    numBlocks = (num_items + blockSize - 1) / blockSize;
    filter_items<<<numBlocks, blockSize>>>(d_frequency, 
                                           d_filtered_items, 
                                           num_items, 
                                           minimum_support_threshold);
    cudaDeviceSynchronize();

    // Copy filtered results back to host
    std::vector<int> h_filtered_items(num_items);
    cudaMemcpy(h_filtered_items.data(), d_filtered_items, num_items * sizeof(int), cudaMemcpyDeviceToHost);

    // Create frequency_by_item map
    std::map<Item, uint64_t> frequency_by_item;
    for (const auto& [item, index] : item_to_index) {
        if (h_filtered_items[index]) {
            frequency_by_item[item] = h_filtered_items[index];
        }
    }

    // Clean up
    cudaFree(d_flattened_transactions);
    cudaFree(d_frequency);
    cudaFree(d_filtered_items);

    double endTime = CycleTimer::currentSeconds();
    
    //what++;
    //printf("cuda time %d = %lf s\n", what, endTime - startTime);


    // Order items by decreasing frequency (same as original code)
    struct frequency_comparator {
        bool operator()(const std::pair<Item, uint64_t>& lhs, const std::pair<Item, uint64_t>& rhs) const {
            return std::tie(lhs.second, lhs.first) > std::tie(rhs.second, rhs.first);
        }
    };

    std::set<std::pair<Item, uint64_t>, frequency_comparator> items_ordered_by_frequency(frequency_by_item.cbegin(), frequency_by_item.cend());

    // start tree construction

    // scan the transactions again
    for ( const Transaction& transaction : transactions ) {
        auto curr_fpnode = root;

        // select and sort the frequent items in transaction according to the order of items_ordered_by_frequency
        for ( const auto& pair : items_ordered_by_frequency ) {
            const Item& item = pair.first;

            // check if item is contained in the current transaction
            if ( std::find( transaction.cbegin(), transaction.cend(), item ) != transaction.cend() ) {
                // insert item in the tree

                // check if curr_fpnode has a child curr_fpnode_child such that curr_fpnode_child.item = item
                const auto it = std::find_if(
                    curr_fpnode->children.cbegin(), curr_fpnode->children.cend(),  [item](const std::shared_ptr<FPNode>& fpnode) {
                        return fpnode->item == item;
                } );
                if ( it == curr_fpnode->children.cend() ) {
                    // the child doesn't exist, create a new node
                    const auto curr_fpnode_new_child = std::make_shared<FPNode>( item, curr_fpnode );

                    // add the new node to the tree
                    curr_fpnode->children.push_back( curr_fpnode_new_child );

                    // update the node-link structure
                    if ( header_table.count( curr_fpnode_new_child->item ) ) {
                        auto prev_fpnode = header_table[curr_fpnode_new_child->item];
                        while ( prev_fpnode->node_link ) { prev_fpnode = prev_fpnode->node_link; }
                        prev_fpnode->node_link = curr_fpnode_new_child;
                    }
                    else {
                        header_table[curr_fpnode_new_child->item] = curr_fpnode_new_child;
                    }

                    // advance to the next node of the current transaction
                    curr_fpnode = curr_fpnode_new_child;
                }
                else {
                    // the child exist, increment its frequency
                    auto curr_fpnode_child = *it;
                    ++curr_fpnode_child->frequency;

                    // advance to the next node of the current transaction
                    curr_fpnode = curr_fpnode_child;
                }
            }
        }
    }
}



bool FPTree::empty() const
{
    assert( root );
    return root->children.size() == 0;
}


bool contains_single_path(const std::shared_ptr<FPNode>& fpnode)
{
    assert( fpnode );
    if ( fpnode->children.size() == 0 ) { return true; }
    if ( fpnode->children.size() > 1 ) { return false; }
    return contains_single_path( fpnode->children.front() );
}
bool contains_single_path(const FPTree& fptree)
{
    return fptree.empty() || contains_single_path( fptree.root );
}



std::set<Pattern> fptree_growth(const FPTree& fptree)
{
    if ( fptree.empty() ) { return {}; }

    if ( contains_single_path( fptree ) ) {
        // generate all possible combinations of the items in the tree

        std::set<Pattern> single_path_patterns;

        // for each node in the tree
        assert( fptree.root->children.size() == 1 );
        auto curr_fpnode = fptree.root->children.front();
        while ( curr_fpnode ) {
            const Item& curr_fpnode_item = curr_fpnode->item;
            const uint64_t curr_fpnode_frequency = curr_fpnode->frequency;

            // add a pattern formed only by the item of the current node
            Pattern new_pattern{ { curr_fpnode_item }, curr_fpnode_frequency };
            single_path_patterns.insert( new_pattern );

            // create a new pattern by adding the item of the current node to each pattern generated until now
            for ( const Pattern& pattern : single_path_patterns ) {
                Pattern new_pattern{ pattern };
                new_pattern.first.insert( curr_fpnode_item );
                assert( curr_fpnode_frequency <= pattern.second );
                new_pattern.second = curr_fpnode_frequency;

                single_path_patterns.insert( new_pattern );
            }

            // advance to the next node until the end of the tree
            assert( curr_fpnode->children.size() <= 1 );
            if ( curr_fpnode->children.size() == 1 ) { curr_fpnode = curr_fpnode->children.front(); }
            else { curr_fpnode = nullptr; }
        }

        return single_path_patterns;
    }
    else {
        // generate conditional fptrees for each different item in the fptree, then join the results

        std::set<Pattern> multi_path_patterns;

        // for each item in the fptree
        for ( const auto& pair : fptree.header_table ) {
            const Item& curr_item = pair.first;

            // build the conditional fptree relative to the current item

            // start by generating the conditional pattern base
            std::vector<TransformedPrefixPath> conditional_pattern_base;

            // for each path in the header_table (relative to the current item)
            auto path_starting_fpnode = pair.second;
            while ( path_starting_fpnode ) {
                // construct the transformed prefix path

                // each item in th transformed prefix path has the same frequency (the frequency of path_starting_fpnode)
                const uint64_t path_starting_fpnode_frequency = path_starting_fpnode->frequency;

                auto curr_path_fpnode = path_starting_fpnode->parent.lock();
                // check if curr_path_fpnode is already the root of the fptree
                if ( curr_path_fpnode->parent.lock() ) {
                    // the path has at least one node (excluding the starting node and the root)
                    TransformedPrefixPath transformed_prefix_path{ {}, path_starting_fpnode_frequency };

                    while ( curr_path_fpnode->parent.lock() ) {
                        assert( curr_path_fpnode->frequency >= path_starting_fpnode_frequency );
                        transformed_prefix_path.first.push_back( curr_path_fpnode->item );

                        // advance to the next node in the path
                        curr_path_fpnode = curr_path_fpnode->parent.lock();
                    }

                    conditional_pattern_base.push_back( transformed_prefix_path );
                }

                // advance to the next path
                path_starting_fpnode = path_starting_fpnode->node_link;
            }

            // generate the transactions that represent the conditional pattern base
            std::vector<Transaction> conditional_fptree_transactions;
            for ( const TransformedPrefixPath& transformed_prefix_path : conditional_pattern_base ) {
                const std::vector<Item>& transformed_prefix_path_items = transformed_prefix_path.first;
                const uint64_t transformed_prefix_path_items_frequency = transformed_prefix_path.second;

                Transaction transaction = transformed_prefix_path_items;

                // add the same transaction transformed_prefix_path_items_frequency times
                for ( auto i = 0; i < transformed_prefix_path_items_frequency; ++i ) {
                    conditional_fptree_transactions.push_back( transaction );
                }
            }

            // build the conditional fptree relative to the current item with the transactions just generated
            const FPTree conditional_fptree( conditional_fptree_transactions, fptree.minimum_support_threshold );
            // call recursively fptree_growth on the conditional fptree (empty fptree: no patterns)
            std::set<Pattern> conditional_patterns = fptree_growth( conditional_fptree );

            // construct patterns relative to the current item using both the current item and the conditional patterns
            std::set<Pattern> curr_item_patterns;

            // the first pattern is made only by the current item
            // compute the frequency of this pattern by summing the frequency of the nodes which have the same item (follow the node links)
            uint64_t curr_item_frequency = 0;
            auto fpnode = pair.second;
            while ( fpnode ) {
                curr_item_frequency += fpnode->frequency;
                fpnode = fpnode->node_link;
            }
            // add the pattern as a result
            Pattern pattern{ { curr_item }, curr_item_frequency };
            curr_item_patterns.insert( pattern );

            // the next patterns are generated by adding the current item to each conditional pattern
            for ( const Pattern& pattern : conditional_patterns ) {
                Pattern new_pattern{ pattern };
                new_pattern.first.insert( curr_item );
                assert( curr_item_frequency >= pattern.second );
                new_pattern.second = pattern.second;

                curr_item_patterns.insert( { new_pattern } );
            }

            // join the patterns generated by the current item with all the other items of the fptree
            multi_path_patterns.insert( curr_item_patterns.cbegin(), curr_item_patterns.cend() );
        }

        return multi_path_patterns;
    }
}


