/**
 * Copyright (c) 2026 Nikola Nedeljkovic
 * SPDX-License-Identifier: MIT
 */

#include "duck/buffer/pool_manager.hpp"
#include "duck/storage/disk_manager.hpp"
#include "duck/table/table.hpp"
#include "duck/table/table_heap.hpp"
#include "duck/tuple/schema.hpp"
#include "duck/tuple/tuple.hpp"
#include <cstddef>
#include <exception>
#include <iomanip>
#include <iostream>
#include <print>
#include <string>
#include <vector>

int main() {
    try {
        duck::DiskManager disk_manager{"test.db"};
        duck::BufferPoolManager pool{disk_manager, 5};

        // duck::TableHeap heap{duck::TableHeap::create(pool)};
        duck::TableHeap heap{0, pool};

        // heap.delete_tuple({0, 5});

        // std::array<std::byte, 1024> barr;
        // barr.fill(std::byte{0x1f});
        // std::span<std::byte> value{barr};
        // heap.insert_tuple(value);

        // duck::TableHeap::Scan scan = heap.scan();
        // while (auto entry = scan.next()) {
        //     std::println("RID: {}/{}, size: {}", entry->first.page_id, entry->first.slot_num, entry->second.size());
        // }

        std::vector<duck::Column> columns{duck::Column{"ID", duck::TypeId::INT32},
                                          duck::Column{"Float", duck::TypeId::FLOAT},
                                          duck::Column{"string", duck::TypeId::VARCHAR, 50}};
        duck::Schema schema{columns};
        duck::Table table{heap, schema};

        auto serialized = schema.serialize();
        // for (auto b : serialized) {
        //     std::cout << std::hex << std::setw(2) << std::setfill('0') <<
        //     static_cast<int>(std::to_integer<uint8_t>(b))
        //               << " ";
        // }
        // std::cout << std::endl;
        std::println("{}", duck::Schema{serialized}.to_string());

        // std::vector<duck::Value> values{duck::Value::of(234), duck::Value::of((float)0.41),
        //                                 duck::Value::of(std::string{"Ovo je vec neki drugi string"})};
        // auto new_tuple{duck::Tuple{values, schema}};
        // auto rid{table.insert_tuple(new_tuple)};
        // std::println("Inserted tuple {}/{}", rid->page_id, rid->slot_num);

        // auto tuple{table.get_tuple({0, 0})};

        std::println("{}", schema.to_string());
        auto scan{table.scan()};
        while (auto entry{scan.next()}) {
            if (entry.has_value()) {
                duck::Tuple tuple{entry.value().second};
                // duck::Value value{tuple.get(2)};
                std::println("{}", tuple.to_string());
            } else {
                std::println("Tuple not found");
            }
        }

    } catch (std::exception& e) {
        std::cout << "Exception: " << e.what() << "\n";
    }
}