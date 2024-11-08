#include <algorithm>
#include <iostream>
#include <cassert>
#include <cstdint>
#include <utility>
#include <execution>
#include <vector>
using namespace std;

// include openmp
#include <omp.h>

#include "fptree.hpp"


FPNode::FPNode(const Item& item, const shared_ptr<FPNode>& parent) :
    item( item ), frequency( 1 ), node_link( nullptr ), parent( parent ), children()
{
}


// map step: use to merge local fp tree 
void merge_trees(shared_ptr<FPNode> global_root, shared_ptr<FPNode> local_root, map<Item, shared_ptr<FPNode>>& global_header_table, const map<Item, shared_ptr<FPNode>>& local_header_table)
{

    // iterate each node from local root
    for (const auto& local_child_node : local_root->children) {

        // find same node in global root
        const auto it = find_if( global_root->children.begin(), global_root->children.end(),
            [local_child_node](const shared_ptr<FPNode>& global_child_node) {
                return global_child_node->item == local_child_node->item;
            }
        );

        // successfully find the same node in global root children
        if(it != global_root->children.end()){

            // increase the frequency
            (*it)->frequency += local_child_node->frequency; 

            // merge node
            // auto global_current_node = (*it);
            // while (global_current_node->node_link)
            //     global_current_node = global_current_node->node_link;
            
            // global_current_node->node_link = local_child_node;

            // recrusive to merge next child node
            merge_trees(*it, local_child_node, global_header_table, local_header_table);

        // no same node in global root children
        }else{


            // copy local child node to global child node (set parent root)
            auto new_child_node = make_shared<FPNode>(local_child_node->item, global_root);
            new_child_node->frequency = local_child_node->frequency;
            global_root->children.push_back(new_child_node);

            // update global header table
            if (global_header_table.count(new_child_node->item)){
                
                auto current_node = global_header_table[new_child_node->item];
                
                // iterate until last item node
                while (current_node->node_link) 
                    current_node = current_node->node_link;
                

                current_node->node_link = new_child_node;

            }else{
                
                // directly update global header table
                global_header_table[new_child_node->item] = new_child_node;
            }

            merge_trees(new_child_node, local_child_node, global_header_table, local_header_table);
        }

    } 

}


FPTree::FPTree(const vector<Transaction>& transactions, uint64_t minimum_support_threshold) :
    root( make_shared<FPNode>( Item{}, nullptr ) ), header_table(),
    minimum_support_threshold( minimum_support_threshold )
{
    //// scan the transactions counting the frequency of each item
    
    // global frequency count
    map<Item, uint64_t> frequency_by_item;

    // get maximum number of threads
    int max_num_threads = omp_get_max_threads();

    // for each thread frequency count
    vector<map<Item, uint64_t>> frequency_by_item_per_thread(max_num_threads);


    //// Parallel item frequency counting using map-reduce
    int num_threads;
    
    // map step: let threads to parallel count frequency by each item
    # pragma omp parallel
    {

        // get total number of threads
        # pragma omp single
        num_threads = omp_get_num_threads();
        // cout << " In line 105 num_threads: " << num_threads << '\n';

        // get current thread id
        int thread_id = omp_get_thread_num();

        // get local counting vector
        map<Item, uint64_t>& local_frequency_by_item = frequency_by_item_per_thread[thread_id];

        # pragma omp for 
        for ( size_t i = 0; i < transactions.size(); i++ ) {
            const Transaction& transaction = transactions[i];

            for ( const Item& item : transaction ) {
                local_frequency_by_item[item]++;
            }
        }
    }

    // cout << " In line 124 num_threads: " << num_threads << '\n';

    // reduce step: accumulate local frequency count to global
    for (size_t i = 0; i < num_threads; i++) {
        const auto& local_frequency = frequency_by_item_per_thread[i];
        for (auto it = local_frequency.cbegin(); it != local_frequency.cend(); ++it) {
            frequency_by_item[it->first] += it->second;
        }
    }

    

    // keep only items which have a frequency greater or equal than the minimum support threshold
    for ( auto it = frequency_by_item.cbegin(); it != frequency_by_item.cend(); ) {
        
        const uint64_t item_frequency = (*it).second;
        if ( item_frequency < minimum_support_threshold ) 
            frequency_by_item.erase( it++ ); 

        else 
            it++;
    }

    //// for debug
    // for ( auto it = frequency_by_item.cbegin(); it != frequency_by_item.cend(); it++ ) {
        
    //     const uint64_t item_frequency = (*it).second;
    //     cout << "(*it).first: " << (*it).first << "\n(*it).second: " << (*it).second << '\n';
    // }


    //// order items by decreasing frequency
    vector<pair<Item, uint64_t>> items_vector(frequency_by_item.begin(), frequency_by_item.end());

    // use cpp execution to parallel sort
    sort(execution::par, items_vector.begin(), items_vector.end(),
        [](const pair<Item, uint64_t>& lhs, const pair<Item, uint64_t>& rhs) {
            
            // decrease by frequency, if freq same than following item order
            return tie(rhs.second, rhs.first) < tie(lhs.second, lhs.first);
        });

    struct frequency_comparator
    {
        bool operator()(const pair<Item, uint64_t> &lhs, const pair<Item, uint64_t> &rhs) const
        {
            return tie(lhs.second, lhs.first) > tie(rhs.second, rhs.first);
        }
    };

    // transfer to set
    set<pair<Item, uint64_t>, frequency_comparator> items_ordered_by_frequency(
        items_vector.begin(), items_vector.end());


    //// for debug
    // for ( auto it = frequency_by_item.cbegin(); it != frequency_by_item.cend(); it++ ) {
        
    //     const uint64_t item_frequency = (*it).second;
    //     cout << "(*it).first: " << (*it).first << "\n(*it).second: " << (*it).second << '\n';
    // }


    //// start tree construction, build hole fp tree and header table

    // declare local root and local header table for each threads
    vector<shared_ptr<FPNode>> local_roots(max_num_threads, nullptr);
    vector<map<Item, shared_ptr<FPNode>>> local_header_tables(max_num_threads);


    // initialize local root for each threads
    for (int idx = 0; idx < num_threads; idx++) {
        local_roots[idx] = make_shared<FPNode>(Item{}, nullptr);
    }


    // scan the transactions again
    # pragma omp parallel
    {

        // get total number of threads
        # pragma omp single
        num_threads = omp_get_num_threads();
        // int thread_nums = omp_get_num_threads();

        // get current thread id
        int thread_id = omp_get_thread_num();
        

        // get local root
        auto& local_root = local_roots[thread_id];

        // get local header table 
        auto& local_header_table= local_header_tables[thread_id];

        // reduce step: let each thread build local fp tree
        # pragma omp for nowait
        for ( size_t i = 0; i < transactions.size(); i++ ) {

            const Transaction& transaction = transactions[i];
            auto curr_fpnode = local_root;

            // select and sort the frequent items in transaction according to the order of items_ordered_by_frequency
            for ( const auto& pair : items_ordered_by_frequency ) {

                const Item& item = pair.first;

                // check if item is contained in the current transaction
                if ( find( transaction.cbegin(), transaction.cend(), item ) != transaction.cend() ) {
                    // insert item in the tree

                    // check if curr_fpnode has a child curr_fpnode_child such that curr_fpnode_child.item = item
                    const auto it = find_if(
                        curr_fpnode->children.cbegin(), curr_fpnode->children.cend(),  [item](const shared_ptr<FPNode>& fpnode) {
                            return fpnode->item == item;
                    } );

                    if ( it == curr_fpnode->children.cend() ) {
                        
                        // the child doesn't exist, create a new node
                        const auto curr_fpnode_new_child = make_shared<FPNode>( item, curr_fpnode );

                        // add the new node to the tree
                        curr_fpnode->children.push_back( curr_fpnode_new_child );

                        // update the local node-link structure
                        if ( local_header_table.count(item) ) {
                            
                            auto prev_fpnode = local_header_table[curr_fpnode_new_child->item];
                            while ( prev_fpnode->node_link ) { prev_fpnode = prev_fpnode->node_link; }
                            prev_fpnode->node_link = curr_fpnode_new_child;

                        }else{
                            local_header_table[curr_fpnode_new_child->item] = curr_fpnode_new_child;
                        }

                        // advance to the next node of the current transaction
                        curr_fpnode = curr_fpnode_new_child;

                    }
                    else {

                        // the child exist, increment its frequency
                        auto curr_fpnode_child = *it;
                        curr_fpnode_child->frequency++;

                        // advance to the next node of the current transaction
                        curr_fpnode = curr_fpnode_child;
                    }
                }

            }
        }
    }

    // reduce step: merge all local fp tree and header table to global 
    for (int tid = 0; tid < num_threads; tid++) {
        merge_trees(root, local_roots[tid], header_table, local_header_tables[tid]);
    }

    //// for debug
    // for (const auto& [item, node_ptr] : header_table) {
        
    //     cout << "Item: " << item << '\n';
    //     // cout << "Item: .first: " << item.first << "\nItem.second: " << item.second << '\n';
    //     cout << ", FPNode: ";
    //     cout << ", FPNode->frequency: " << node_ptr -> frequency << '\n';
    //     cout << ", FPNode->item: " << node_ptr -> item << '\n';
        
    //     // iterate each child

        
    //     cout << endl;
    // }


    /////////////////////////////////////////////////////////////////////////////////////////////////////



    // scan the transactions counting the frequency of each item
    // std::map<Item, uint64_t> frequency_by_item;
    // for ( const Transaction& transaction : transactions ) {
    //     for ( const Item& item : transaction ) {
    //         ++frequency_by_item[item];
    //     }
    // }

    // // keep only items which have a frequency greater or equal than the minimum support threshold
    // for ( auto it = frequency_by_item.cbegin(); it != frequency_by_item.cend(); ) {
    //     const uint64_t item_frequency = (*it).second;
    //     if ( item_frequency < minimum_support_threshold ) { frequency_by_item.erase( it++ ); }
    //     else { ++it; }
    // }

    // // order items by decreasing frequency
    // struct frequency_comparator
    // {
    //     bool operator()(const std::pair<Item, uint64_t> &lhs, const std::pair<Item, uint64_t> &rhs) const
    //     {
    //         return std::tie(lhs.second, lhs.first) > std::tie(rhs.second, rhs.first);
    //     }
    // };
    // std::set<std::pair<Item, uint64_t>, frequency_comparator> items_ordered_by_frequency(frequency_by_item.cbegin(), frequency_by_item.cend());

    // // start tree construction

    // // scan the transactions again
    // for ( const Transaction& transaction : transactions ) {
    //     auto curr_fpnode = root;

    //     // select and sort the frequent items in transaction according to the order of items_ordered_by_frequency
    //     for ( const auto& pair : items_ordered_by_frequency ) {
    //         const Item& item = pair.first;

    //         // check if item is contained in the current transaction
    //         if ( std::find( transaction.cbegin(), transaction.cend(), item ) != transaction.cend() ) {
    //             // insert item in the tree

    //             // check if curr_fpnode has a child curr_fpnode_child such that curr_fpnode_child.item = item
    //             const auto it = std::find_if(
    //                 curr_fpnode->children.cbegin(), curr_fpnode->children.cend(),  [item](const std::shared_ptr<FPNode>& fpnode) {
    //                     return fpnode->item == item;
    //             } );
    //             if ( it == curr_fpnode->children.cend() ) {
    //                 // the child doesn't exist, create a new node
    //                 const auto curr_fpnode_new_child = std::make_shared<FPNode>( item, curr_fpnode );

    //                 // add the new node to the tree
    //                 curr_fpnode->children.push_back( curr_fpnode_new_child );

    //                 // update the node-link structure
    //                 if ( header_table.count( curr_fpnode_new_child->item ) ) {
    //                     auto prev_fpnode = header_table[curr_fpnode_new_child->item];
    //                     while ( prev_fpnode->node_link ) { prev_fpnode = prev_fpnode->node_link; }
    //                     prev_fpnode->node_link = curr_fpnode_new_child;
    //                 }
    //                 else {
    //                     header_table[curr_fpnode_new_child->item] = curr_fpnode_new_child;
    //                 }

    //                 // advance to the next node of the current transaction
    //                 curr_fpnode = curr_fpnode_new_child;
    //             }
    //             else {
    //                 // the child exist, increment its frequency
    //                 auto curr_fpnode_child = *it;
    //                 ++curr_fpnode_child->frequency;

    //                 // advance to the next node of the current transaction
    //                 curr_fpnode = curr_fpnode_child;
    //             }
    //         }
    //     }
    // }
}

bool FPTree::empty() const
{
    assert( root );
    return root->children.size() == 0;
}


bool contains_single_path(const shared_ptr<FPNode>& fpnode)
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

set<Pattern> fptree_growth(const FPTree& fptree)
{
    if ( fptree.empty() ) { return {}; }

    if ( contains_single_path( fptree ) ) {
        // collect all items and their frequencies along the single path
        vector<pair<Item, uint64_t>> path_items;
        auto curr_fpnode = fptree.root->children.front();
        while ( curr_fpnode ) {
            path_items.push_back({ curr_fpnode->item, curr_fpnode->frequency });
            if ( curr_fpnode->children.size() == 1 ) {
                curr_fpnode = curr_fpnode->children.front();
            } else {
                curr_fpnode = nullptr;
            }
        }

        // generate all possible combinations of the items in the path
        set<Pattern> single_path_patterns;
        size_t n = path_items.size();
        for (size_t i = 1; i < (1 << n); ++i) {
            Pattern pattern;
            uint64_t min_support = UINT64_MAX;
            for (size_t j = 0; j < n; ++j) {
                if (i & (1 << j)) {
                    pattern.first.insert(path_items[j].first);
                    min_support = min(min_support, path_items[j].second);
                }
            }
            pattern.second = min_support;
            single_path_patterns.insert(pattern);
        }

        return single_path_patterns;
    }
    else {
        // generate conditional fptrees for each different item in the fptree, then join the results

        set<Pattern> multi_path_patterns;

        // for each item in the fptree
        for ( const auto& pair : fptree.header_table ) {
            const Item& curr_item = pair.first;

            // build the conditional fptree relative to the current item

            // start by generating the conditional pattern base
            vector<TransformedPrefixPath> conditional_pattern_base;

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
            vector<Transaction> conditional_fptree_transactions;
            for ( const TransformedPrefixPath& transformed_prefix_path : conditional_pattern_base ) {
                const vector<Item>& transformed_prefix_path_items = transformed_prefix_path.first;
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
            set<Pattern> conditional_patterns = fptree_growth( conditional_fptree );

            // construct patterns relative to the current item using both the current item and the conditional patterns
            set<Pattern> curr_item_patterns;

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
