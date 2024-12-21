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
#include <cuda.h>
#include <sstream>
#include <stdexcept>
#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include "FPGrowth.h"
#include "FPHeaderTable.h"
#include "FPRadixTree.h"
#include "FPTransMap.h"

#include <thrust/device_vector.h>
#include <thrust/extrema.h>
// include timer
#include "CycleTimer.h"


using namespace std;
using namespace cuda_fp_growth;

void load_transactions_from_file(const std::string& filename, Items& trans, Indices& indices, Sizes& sizes) {
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        throw std::runtime_error("Failed to open file for reading: " + filename);
    }

    std::string line;
    size_t current_index = 0; // 用來記錄當前交易的起始索引

    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        std::string item_str;
        size_type line_item_count = 0; // 記錄當前交易的項目數量

        // 記錄當前交易的起始索引
        indices.push_back(static_cast<index_type>(current_index));

        // 解析當前行的所有項目
        while (std::getline(iss, item_str, ',')) {
            // 將數字轉為 `Item` 並檢查範圍
            Item item = static_cast<Item>(std::stoul(item_str));
            if (item > 100) {
                throw std::runtime_error("Item value out of range (0-100): " + item_str);
            }
            trans.push_back(item);
            line_item_count++;
        }

        // 記錄當前交易的項目數量
        sizes.push_back(line_item_count);

        // 更新下一交易的起始索引
        current_index += line_item_count;
    }

    infile.close();
}

size_type pattern_count( const std::vector<cuda_uint>& buffer )
{
    index_type i = 0;
    size_type pattern_count = 0;
    while ( i < buffer.size() ) {
        ++pattern_count;
        i += ( buffer[i] / sizeof( cuda_uint ) );
    }
    return pattern_count;
}

bool pattern_exists( const std::vector<cuda_uint>& buffer, const std::vector<Item>& pattern, const size_type support, const cuda_real confidence = 0.0f )
{
    index_type i = 0;
    while ( i < buffer.size() ) {
        size_type length = buffer[ i ] / sizeof( cuda_uint );
        size_type offset = ( confidence > 0.0f ? 3 : 2 );
        bool exists = true;
        exists &= ( pattern.size() == length - offset );
        exists &= ( buffer[ i + 1 ] == support );
        exists &= ( std::equal( pattern.begin(), pattern.end(), buffer.begin() + i + offset ) );
        if ( confidence > 0.0f ) {
            const cuda_uint* ptr = &buffer[ i + 2 ];
            exists &= ( std::abs( *( reinterpret_cast<const cuda_real*>( ptr ) ) - confidence ) < 0.0001 );
        }
        if ( exists ) return true;

        i += ( buffer[i] / sizeof( cuda_uint ) );
    }
    return false;
}

int main(int argc, const char *argv[])
{
    string data_folderPath = argv[1];
    
    Items trans;      // 用來存放所有項目
    Indices indices;  // 每筆交易的起始索引
    Sizes sizes;      // 每筆交易的項目數量

    load_transactions_from_file(data_folderPath, trans, indices, sizes);

    double start_time = CycleTimer::currentSeconds();

    //step 1 :建立 FPTransMap 物件
    size_type min_support = 3;
    std::cout << "Testing FPTransMap with minimum support = 3 \n";
    FPTransMap fp_trans_map( trans.cbegin(), indices.cbegin(), sizes.cbegin(), indices.size(), min_support );

    // 取得頻繁項目
    const DItems& d_freq_items = fp_trans_map.frequent_items();
    const DSizes& d_freq_items_counts = fp_trans_map.items_frequency();
    const Items freq_items( d_freq_items.begin(), d_freq_items.end() );
    const Sizes freq_items_counts( d_freq_items_counts.begin(), d_freq_items_counts.end() );

    //step 2 :FPRadixTree
    FPRadixTree fp_radix_tree( fp_trans_map );
    const DInnerNodes& d_inner_nodes = fp_radix_tree.inner_nodes();
    const DLeafNodes& d_leaf_nodes = fp_radix_tree.leaf_nodes();
    InnerNodes inner_nodes(d_inner_nodes.cbegin(), d_inner_nodes.cend());
    LeafNodes leaf_nodes(d_leaf_nodes.cbegin(), d_leaf_nodes.cend());
    
    //step 3 :FPHeaderTable
    FPHeaderTable header_table( fp_trans_map, fp_radix_tree, min_support );

    double exec_time = CycleTimer::currentSeconds() - start_time;
    cout << "Test case - " << data_folderPath << " time : " << setprecision(4) << exec_time <<"s\n";

    //step 4 :FPGrowth
    FPGrowth fp( fp_trans_map, fp_radix_tree, header_table, min_support );
    
    std::vector<cuda_uint> buffer( 1024 );
    size_type buffer_size = sizeof( cuda_uint ) * buffer.size();

    fp.mine_frequent_patterns( &buffer[0], buffer_size );

    buffer.resize( buffer_size / sizeof( cuda_uint ) );
        
    // test結果
    if(data_folderPath == "./dataset/test.txt"){

        // 顯示載入的結果        
        std::cout << "Items (trans): ";
        for (const auto& item : trans) {
            std::cout << item << " ";
        }
        std::cout << "\n";

        std::cout << "Indices: ";
        for (const auto& index : indices) {
            std::cout << index << " ";
        }
        std::cout << "\n";

        std::cout << "Sizes: ";
        for (const auto& size : sizes) {
            std::cout << size << " ";
        }
        std::cout << "\n";

        // 驗證頻繁項目的正確性
        std::cout << "freq_items.size(): " << freq_items.size() << std::endl;
        std::cout << "freq_items_counts.size(): " << freq_items_counts.size() << std::endl;
        std::cout << "Frequent items and counts are correctly identified.\n";

        // 顯示頻繁項目及其支持數
        for (size_t i = 0; i < freq_items.size(); ++i) {
            std::cout << "Item: " << freq_items[i] << ", Count: " << freq_items_counts[i] << "\n";
        }

        // 驗證內部節點和葉節點的非空性（或其他必要條件）
        std::cout << "!inner_nodes.empty(): " << !inner_nodes.empty() << std::endl;
        std::cout << "!leaf_nodes.empty(): " << !leaf_nodes.empty() << std::endl;
        std::cout << "FPRadixTree inner nodes and leaf nodes are correctly constructed.\n";

        // 顯示內部節點資訊 
        std::cout << "Inner Nodes:\n";
        for (const auto& node : inner_nodes) {
            std::cout << "Parent: " << node.parent_idx
                      << ", Range: [" << node.range_start << ", " << node.range_end << "]"
                      << ", Left: " << (node.left_is_leaf ? "Leaf" : "Inner") << " (" << node.left_idx << ")"
                      << ", Right: " << (node.right_is_leaf ? "Leaf" : "Inner") << " (" << node.right_idx << ")"
                      << ", Prefix Length: " << node.prefix_length
                      << ", Transaction Count: " << node.trans_count << "\n";
        }

        // 顯示葉節點資訊   
        std::cout << "Leaf Nodes:\n";
        for (const auto& node : leaf_nodes) {
            std::cout << "Parent: " << node.parent_idx
                      << ", Transaction Count: " << node.trans_count << "\n";
        }

        // 測試 FPHeaderTable
        //cudaDeviceSynchronize();
        std::cout << "Header Table Size: " << header_table.size() << std::endl;
        std::cout << "header_table.ia_size: " << header_table.ia_size() << std::endl;

    }

    return EXIT_SUCCESS;
}
