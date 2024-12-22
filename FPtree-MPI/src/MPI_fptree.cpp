#include <algorithm>
#include <cassert>
#include <cstdint>
#include <utility>
#include <iostream>
#include <sstream>
#include <mpi.h>
#include <time.h>
#include <vector>
#include <cstring>
using namespace std;

#include "../include/fptree.hpp"

FPNode::FPNode(const Item& item, const std::shared_ptr<FPNode>& parent) :
    item( item ), frequency( 1 ), node_link( nullptr ), parent( parent ), children()
{
}

FPTree::FPTree(const std::vector<Transaction>& transactions, uint64_t minimum_support_threshold) :
    root( std::make_shared<FPNode>( Item{}, nullptr ) ), header_table(), // init tree root with nullptr
    minimum_support_threshold( minimum_support_threshold )               // init minimum threshold
{
    int world_rank, world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    //std::cout << "world size " << " = " << world_size << '\n';
    // MPI_Status status;

    // 將交易資料平均分給各處理器
    int total_transactions = transactions.size();
    //std::cout << "rank " << world_rank << ", total_transactions: " << total_transactions << '\n';
    int local_transaction_count = total_transactions / world_size;
    //std::cout << "rank " << world_rank << ", local_transaction_count: " << local_transaction_count << '\n';
    int extra_transactions = total_transactions % world_size;
    //std::cout << "rank " << world_rank << ", extra_transactions: " << extra_transactions << '\n';
    // 計算每個處理器的交易範圍
    int start_idx = world_rank * local_transaction_count + std::min(world_rank, extra_transactions);
    //std::cout << "rank " << world_rank << ", start_idx: " << start_idx << '\n';
    int end_idx = start_idx + local_transaction_count + (world_rank < extra_transactions);
    //std::cout << "rank " << world_rank << ", end_idx: " << end_idx << '\n';

    double item_frequency_start_time = MPI_Wtime();
    // local data
    std::vector<Transaction> local_transactions(transactions.begin() + start_idx, transactions.begin() + end_idx);
    /**/
    // Calculate local frequency
    std::map<Item, uint64_t> local_frequency_by_item;
    for (const Transaction& transaction : local_transactions) {
        for (const Item& item : transaction) {
            ++local_frequency_by_item[item];
        }
    }
    
    /**/
    // Send local frequency
    std::map<Item, uint64_t> global_frequency_by_item;
    if (world_rank > 0){
        
        //std::cout << "------------------------------------------------------" << std::endl;
        //std::cout << "Here is rank: " << world_rank << std::endl;
        ///std::cout << "Counting the frequency of each item:" << std::endl;
        //for ( const Transaction& transaction : local_transactions ) {
            //for ( const Item& item : transaction ) {
                // 計數每個 item 在 transaction 中的出現次數
                //std::cout << "item: " << item << " frequency: "  << local_frequency_by_item[item] << ", ";
            //}
            //std::cout << std::endl;
        //}
        
        for (const auto& pair : local_frequency_by_item) {
            const std::string& item = pair.first;
            uint64_t frequency = pair.second;

            // MPI_Send item 和 frequency
            MPI_Send(item.c_str(), item.size() + 1, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
            MPI_Send(&frequency, 1, MPI_UINT64_T, 0, 0, MPI_COMM_WORLD);
        }
        // 傳送空字串表示結束
        char end_signal = '\0';
        MPI_Send(&end_signal, 1, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
        
    }
    // Master Receive frequency
    else if (world_rank == 0){
        //std::cout << "Here is MASTER Rank" << std::endl;
        global_frequency_by_item = local_frequency_by_item; // 先加上 Master data

        for (int source = 1; source < world_size; ++source) {
            while (true) {
                char buffer[256];
                uint64_t frequency;

                // 接收 item
                MPI_Recv(buffer, 256, MPI_CHAR, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                std::string item(buffer);

                // 如果接收到結束標誌，跳出迴圈
                if (item.empty()) break;

                // 接收 frequency
                MPI_Recv(&frequency, 1, MPI_UINT64_T, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                global_frequency_by_item[item] += frequency;
            }
        }
        //for (const auto& pair : global_frequency_by_item) {
            //std::cout << "item: " << pair.first << ", frequency: " << pair.second << '\n';
        //}
    }
    MPI_Barrier(MPI_COMM_WORLD);
    /**/
    if (world_rank == 0){
        double item_frequency_end_time = MPI_Wtime();
        std::cout << "Build frequency item set costs " << (item_frequency_end_time - item_frequency_start_time)*1000 << " ms\n";
        std::cout << "Build frequency item set passed!" << std::endl;
    }

    /**/
    // keep only items which have a frequency greater or equal than the minimum support threshold
    // Delete freq. < threshold 的 item
    for ( auto it = global_frequency_by_item.cbegin(); it != global_frequency_by_item.cend(); ) {
        const uint64_t item_frequency = (*it).second;
        // std::cout << "First: " << (*it).first << " , item_frequency: " << item_frequency << std::endl;
        if ( item_frequency < minimum_support_threshold ) { 
            global_frequency_by_item.erase( it++ );
        }
        else {
            ++it;
        }
    }
    
    // Broadcast global frequency to all processors
    if (world_rank == 0) {
        // Serialize global_frequency_by_item into two arrays
        std::vector<std::string> items;
        std::vector<uint64_t> frequencies;
        for (const auto& pair : global_frequency_by_item) {
            items.push_back(pair.first);
            frequencies.push_back(pair.second);
        }

        // Broadcast size
        int size = items.size();
        MPI_Bcast(&size, 1, MPI_INT, 0, MPI_COMM_WORLD);

        // Broadcast each item size and data
        for (int i = 0; i < size; ++i) {
            int item_length = items[i].size();
            MPI_Bcast(&item_length, 1, MPI_INT, 0, MPI_COMM_WORLD);
            MPI_Bcast(const_cast<char*>(items[i].c_str()), item_length + 1, MPI_CHAR, 0, MPI_COMM_WORLD);
        }

        // Broadcast frequency
        MPI_Bcast(frequencies.data(), size, MPI_UINT64_T, 0, MPI_COMM_WORLD);
    } else {
        // Receive Broadcast data
        int size;
        MPI_Bcast(&size, 1, MPI_INT, 0, MPI_COMM_WORLD);

        std::vector<std::string> items(size);
        std::vector<uint64_t> frequencies(size);

        for (int i = 0; i < size; ++i) {
            int item_length;
            MPI_Bcast(&item_length, 1, MPI_INT, 0, MPI_COMM_WORLD);

            char* buffer = new char[item_length + 1];
            MPI_Bcast(buffer, item_length + 1, MPI_CHAR, 0, MPI_COMM_WORLD);
            items[i] = buffer;
            delete[] buffer;
        }

        MPI_Bcast(frequencies.data(), size, MPI_UINT64_T, 0, MPI_COMM_WORLD);

        // Deserialize the received broadcast data back into a map
        global_frequency_by_item.clear();
        for (int i = 0; i < size; ++i) {
            global_frequency_by_item[items[i]] = frequencies[i];
        }
    }
    MPI_Barrier(MPI_COMM_WORLD);

    // order items by decreasing frequency
    // 根據 item freq. 作 decreasing
    /**/
    struct frequency_comparator
    {
        bool operator()(const std::pair<Item, uint64_t> &lhs, const std::pair<Item, uint64_t> &rhs) const
        {
            return std::tie(lhs.second, lhs.first) > std::tie(rhs.second, rhs.first);
        }
    };
    std::set<std::pair<Item, uint64_t>, frequency_comparator> items_ordered_by_frequency(global_frequency_by_item.cbegin(), global_frequency_by_item.cend());

    double tree_start_time = MPI_Wtime();

    // declare local root and local header table for each threads
    vector<shared_ptr<FPNode>> local_roots(world_size, nullptr);
    vector<map<Item, shared_ptr<FPNode>>> local_header_tables(world_size);


    // initialize local root for each threads
    for (int idx = 0; idx < world_size; idx++) {
        local_roots[idx] = make_shared<FPNode>(Item{}, nullptr);
    }

    // get local root
    auto& local_root = local_roots[world_rank];

    // get local header table 
    auto& local_header_table= local_header_tables[world_rank];

    // construct a local FP-tree for each processors
    for ( size_t i = 0; i < transactions.size(); i++ ) {

        const Transaction& transaction = transactions[i];
        // 從 root start
        auto curr_fpnode = local_root;

        // select and sort the frequent items in transaction according to the order of items_ordered_by_frequency
        for ( const auto& pair : items_ordered_by_frequency ) {
            const Item& item = pair.first;

            // check if item is contained in the current transaction
            // 檢查目前交易中是否包含此 item
            if ( std::find( transaction.cbegin(), transaction.cend(), item ) != transaction.cend() ) {
                // insert item in the tree
                // 如果包含該 item，insert 到 FP-tree

                // check if curr_fpnode has a child curr_fpnode_child such that curr_fpnode_child.item = item
                // 檢查目前節點是否已經有該 item 的子節點
                const auto it = std::find_if(
                    curr_fpnode->children.cbegin(), curr_fpnode->children.cend(),  [item](const std::shared_ptr<FPNode>& fpnode) {
                        return fpnode->item == item;
                } );
                if ( it == curr_fpnode->children.cend() ) {
                    // the child doesn't exist, create a new node
                    // 子節點不存在，create 新節點
                    const auto curr_fpnode_new_child = std::make_shared<FPNode>( item, curr_fpnode );

                    // add the new node to the tree
                    // 將 new 節點添加到當前節點的子節點列表中
                    curr_fpnode->children.push_back( curr_fpnode_new_child );

                    // update the node-link structure
                    // 更新 Node-Link 結構
                    if ( local_header_table.count( curr_fpnode_new_child->item ) ) {
                        auto prev_fpnode = local_header_table[curr_fpnode_new_child->item];
                        while ( prev_fpnode->node_link ) { 
                            // 遍歷到該項目最後一個節點
                            prev_fpnode = prev_fpnode->node_link;
                        }
                        // 建立 link
                        prev_fpnode->node_link = curr_fpnode_new_child;
                    }
                    else {
                        // 添加 header link
                        local_header_table[curr_fpnode_new_child->item] = curr_fpnode_new_child;
                    }

                    // advance to the next node of the current transaction
                    // 移動到新建的子節點
                    curr_fpnode = curr_fpnode_new_child;
                }
                else {
                    // the child exist, increment its frequency
                    // 子節點已存在，增加 frequency
                    auto curr_fpnode_child = *it;
                    ++curr_fpnode_child->frequency;

                    // advance to the next node of the current transaction
                    // 移動到已存在的子節點
                    curr_fpnode = curr_fpnode_child;
                }
            }
        }
    }
    /**/
    // merge header_table
    if (world_rank == 0) {
        // Master processor receives the header_table of all processors
        for (int source = 1; source < world_size; ++source) {
            uint64_t num_items;
            MPI_Recv(&num_items, 1, MPI_UINT64_T, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            for (uint64_t i = 0; i < num_items; ++i) {
                uint64_t item_length;
                MPI_Recv(&item_length, 1, MPI_UINT64_T, source, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                std::vector<char> buffer(item_length);
                MPI_Recv(buffer.data(), item_length, MPI_CHAR, source, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                uint64_t frequency;
                MPI_Recv(&frequency, 1, MPI_UINT64_T, source, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                std::string item(buffer.data());
                if (header_table.count(item)) {
                    auto node = header_table[item];
                    while (node->node_link) node = node->node_link;
                    node->node_link = std::make_shared<FPNode>(item, nullptr);
                    node->node_link->frequency = frequency;
                } else {
                    auto new_node = std::make_shared<FPNode>(item, nullptr);
                    new_node->frequency = frequency;
                    header_table[item] = new_node;
                }
            }
        }
    } else {
        // Each processor sends header_table
        uint64_t num_items = header_table.size();
        MPI_Send(&num_items, 1, MPI_UINT64_T, 0, 0, MPI_COMM_WORLD);

        for (const auto& pair : header_table) {
            const std::string& item = pair.first;
            uint64_t frequency = pair.second->frequency;

            uint64_t item_length = item.size() + 1;  // 包含結尾 '\0'
            MPI_Send(&item_length, 1, MPI_UINT64_T, 0, 1, MPI_COMM_WORLD);
            MPI_Send(item.c_str(), item_length, MPI_CHAR, 0, 2, MPI_COMM_WORLD);
            MPI_Send(&frequency, 1, MPI_UINT64_T, 0, 3, MPI_COMM_WORLD);
        }
    }
    MPI_Barrier(MPI_COMM_WORLD);
    /**/
    if (world_rank == 0){
        double tree_end_time = MPI_Wtime();
        std::cout << "Build FP-tree costs " << (tree_end_time - tree_start_time)*1000 << " ms\n";
        std::cout << "Build FP-tree passed!" << std::endl;
        std::cout << "------------------------------------------------------" << std::endl;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // start tree construction
    // scan the transactions again
    // 構建 FP-tree
    /*
    for ( const Transaction& transaction : transactions ) {
        // 從 root start
        auto curr_fpnode = root;

        // select and sort the frequent items in transaction according to the order of items_ordered_by_frequency
        for ( const auto& pair : items_ordered_by_frequency ) {
            const Item& item = pair.first;

            // check if item is contained in the current transaction
            // 檢查目前交易中是否包含此 item
            if ( std::find( transaction.cbegin(), transaction.cend(), item ) != transaction.cend() ) {
                // insert item in the tree
                // 如果包含該 item，insert 到 FP-tree

                // check if curr_fpnode has a child curr_fpnode_child such that curr_fpnode_child.item = item
                // 檢查目前節點是否已經有該 item 的子節點
                const auto it = std::find_if(
                    curr_fpnode->children.cbegin(), curr_fpnode->children.cend(),  [item](const std::shared_ptr<FPNode>& fpnode) {
                        return fpnode->item == item;
                } );
                if ( it == curr_fpnode->children.cend() ) {
                    // the child doesn't exist, create a new node
                    // 子節點不存在，create 新節點
                    const auto curr_fpnode_new_child = std::make_shared<FPNode>( item, curr_fpnode );

                    // add the new node to the tree
                    // 將 new 節點添加到當前節點的子節點列表中
                    curr_fpnode->children.push_back( curr_fpnode_new_child );

                    // update the node-link structure
                    // 更新 Node-Link 結構
                    if ( header_table.count( curr_fpnode_new_child->item ) ) {
                        auto prev_fpnode = header_table[curr_fpnode_new_child->item];
                        while ( prev_fpnode->node_link ) { 
                            // 遍歷到該項目最後一個節點
                            prev_fpnode = prev_fpnode->node_link;
                        }
                        // 建立 link
                        prev_fpnode->node_link = curr_fpnode_new_child;
                    }
                    else {
                        // 添加 header link
                        header_table[curr_fpnode_new_child->item] = curr_fpnode_new_child;
                    }

                    // advance to the next node of the current transaction
                    // 移動到新建的子節點
                    curr_fpnode = curr_fpnode_new_child;
                }
                else {
                    // the child exist, increment its frequency
                    // 子節點已存在，增加 frequency
                    auto curr_fpnode_child = *it;
                    ++curr_fpnode_child->frequency;

                    // advance to the next node of the current transaction
                    // 移動到已存在的子節點
                    curr_fpnode = curr_fpnode_child;
                }
            }
        }
    }
    */
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
                for ( uint64_t i = 0; i < transformed_prefix_path_items_frequency; ++i ) {
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
