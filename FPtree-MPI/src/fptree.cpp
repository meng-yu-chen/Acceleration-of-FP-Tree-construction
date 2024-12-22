#include <algorithm>
#include <cassert>
#include <cstdint>
#include <utility>

#include "fptree.hpp"


FPNode::FPNode(const Item& item, const std::shared_ptr<FPNode>& parent) :
    item( item ), frequency( 1 ), node_link( nullptr ), parent( parent ), children()
{
}

FPTree::FPTree(const std::vector<Transaction>& transactions, uint64_t minimum_support_threshold) :
    root( std::make_shared<FPNode>( Item{}, nullptr ) ), header_table(),
    minimum_support_threshold( minimum_support_threshold )
{
    // scan the transactions counting the frequency of each item
    // 事務掃描
    // FP樹的建構首先需要掃描輸入的事務集，計算每個物品項目的出現頻率。
    // 這一步將每個物品項目對應到它的出現次數，儲存在frequency_by_item這個map中。
    std::map<Item, uint64_t> frequency_by_item;
    for ( const Transaction& transaction : transactions ) {
        for ( const Item& item : transaction ) {
            ++frequency_by_item[item];
        }
    }

    // keep only items which have a frequency greater or equal than the minimum support threshold
    // 頻次過濾
    // FP樹只保留那些滿足最小支持度閾值的物品項（即頻率大於或等於minimum_support_threshold）。
    for ( auto it = frequency_by_item.cbegin(); it != frequency_by_item.cend(); ) {
        const uint64_t item_frequency = (*it).second;
        // 頻率較低的物品會被過濾掉。
        if ( item_frequency < minimum_support_threshold ) { frequency_by_item.erase( it++ ); }
        else { ++it; }
    }

    // order items by decreasing frequency
    // 頻次排序
    // 將保留下來的物品項按頻率從高到低排序。確保了FP樹中的物品項目順序符合頻繁項集模式挖掘的要求。
    struct frequency_comparator
    {
        bool operator()(const std::pair<Item, uint64_t> &lhs, const std::pair<Item, uint64_t> &rhs) const
        {
            return std::tie(lhs.second, lhs.first) > std::tie(rhs.second, rhs.first);
        }
    };
    std::set<std::pair<Item, uint64_t>, frequency_comparator> items_ordered_by_frequency(frequency_by_item.cbegin(), frequency_by_item.cend());

    // start tree construction
    // FP樹的構建

    // scan the transactions again
    // 遍歷每個事務
    for ( const Transaction& transaction : transactions ) {
        auto curr_fpnode = root;

        // select and sort the frequent items in transaction according to the order of items_ordered_by_frequency
        // 依照前面排好的頻次順序將事務中的物品項目插入FP樹
        for ( const auto& pair : items_ordered_by_frequency ) {
            const Item& item = pair.first;

            // check if item is contained in the current transaction
            // 每個物品項插入時，檢查它是否已經在目前節點的子節點中
            if ( std::find( transaction.cbegin(), transaction.cend(), item ) != transaction.cend() ) {
                // insert item in the tree

                // check if curr_fpnode has a child curr_fpnode_child such that curr_fpnode_child.item = item
                const auto it = std::find_if(
                    curr_fpnode->children.cbegin(), curr_fpnode->children.cend(),  [item](const std::shared_ptr<FPNode>& fpnode) {
                        return fpnode->item == item;
                } );
                // 如果沒有，則建立新的子節點；
                if ( it == curr_fpnode->children.cend() ) {
                    // the child doesn't exist, create a new node
                    const auto curr_fpnode_new_child = std::make_shared<FPNode>( item, curr_fpnode );

                    // add the new node to the tree
                    curr_fpnode->children.push_back( curr_fpnode_new_child );

                    // update the node-link structure
                    // header_table 結構記錄了每個物品項目在FP樹中的起始節點。
                    // header_table 中的每個節點透過node_link連接到樹中相同物品項目的其他節點，形成鍊錶。
                    // 這種結構用於後續的模式成長（FP-Growth）步驟。
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
                    // 如果已經存在，則增加該節點的頻次。
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

// 檢查FP樹是否只包含一條單一路徑。
// 如果FP樹只包含一個分支（例如樹中的每個節點只有一個子節點），那麼可以透過列舉所有子集來產生頻繁模式。
// 這種情況就是FP-Growth的一個最佳化。
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

    // 單路徑狀況
    // 如果FP樹包含單一路徑，可以透過簡單地列舉這條路徑上的物品項目來產生頻繁模式，能夠快速處理。
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
        // 多路徑情況
        // 如果FP樹包含多個分支，FP-Growth的策略是為每個物品項目產生一個條件FP樹，然後遞歸地挖掘每個條件FP樹中的頻繁模式
        for ( const auto& pair : fptree.header_table ) {
            const Item& curr_item = pair.first;

            // build the conditional fptree relative to the current item

            // start by generating the conditional pattern base
            // 產生條件模式基：透過頭表中相同物品項目的節點產生條件模式基，這些條件模式基是與目前物品項目相關的事務前綴路徑。
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
            // 產生事務：將條件模式基底轉換為事務集。
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
            // 建構條件FP樹：利用這些事務集重新建構FP樹，並遞歸呼叫fptree_growth來挖掘該樹中的頻繁模式。
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
            // 組合模式：將條件FP樹中的模式與目前物品項組合，形成最終的頻繁模式。
            multi_path_patterns.insert( curr_item_patterns.cbegin(), curr_item_patterns.cend() );
        }

        return multi_path_patterns;
    }
}
