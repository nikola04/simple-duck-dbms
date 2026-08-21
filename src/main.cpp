/**
 * Copyright (c) 2026 Nikola Nedeljkovic
 * SPDX-License-Identifier: MIT
 */

#include "duck/buffer/pool_manager.hpp"
#include "duck/catalog/catalog.hpp"
#include "duck/storage/disk_manager.hpp"
#include "duck/table/table.hpp"
#include "duck/tuple/schema.hpp"
#include <cstddef>
#include <exception>
#include <iostream>
#include <print>
#include <string>

int main() {
    try {
        duck::DiskManager disk_manager{"test.db"};
        duck::BufferPoolManager pool{disk_manager, 5};

        duck::Catalog catalog{pool, disk_manager};
        for (auto table : catalog.all_tables()) {
            std::println("{}\n{}\n", table->name(), table->schema().to_string());
        }

        // duck::TableHeap heap{duck::TableHeap::create(pool)};
        // duck::TableHeap heap{0, pool};

        // heap.delete_tuple({0, 5});

        // std::array<std::byte, 1024> barr;
        // barr.fill(std::byte{0x1f});
        // std::span<std::byte> value{barr};
        // heap.insert_tuple(value);

        // duck::TableHeap::Scan scan = heap.scan();
        // while (auto entry = scan.next()) {
        //     std::println("RID: {}/{}, size: {}", entry->first.page_id, entry->first.slot_num, entry->second.size());
        // }

        // std::vector<duck::Column> columns{
        //     duck::Column{"id", duck::TypeId::UINT32}, duck::Column{"username", duck::TypeId::CHAR, 50},
        //     duck::Column{"password", duck::TypeId::VARCHAR, 128}, duck::Column{"email", duck::TypeId::VARCHAR, 256},
        //     duck::Column{"verified", duck::TypeId::BOOL}};
        // duck::Schema schema{columns};
        // catalog.create_table("users", schema);

        // duck::Table table{heap, schema};

        // auto serialized = schema.serialize();
        // for (auto b : serialized) {
        //     std::cout << std::hex << std::setw(2) << std::setfill('0') <<
        //     static_cast<int>(std::to_integer<uint8_t>(b))
        //               << " ";
        // }
        // std::cout << std::endl;
        // std::println("{}", duck::Schema{serialized}.to_string());

        // std::vector<duck::Value> values{
        //     duck::Value::of((uint32_t)4294967296), duck::Value::of((double)0.123456789),
        //     duck::Value::of(std::vector<std::byte>{std::byte{0xff}, std::byte{0x00}, std::byte{0xf0}})};
        // auto new_tuple{duck::Tuple{values, schema}};
        // auto rid{table.insert_tuple(new_tuple)};
        // std::println("Inserted tuple {}/{}", rid->page_id, rid->slot_num);

        // auto tuple{table.get_tuple({0, 0})};

        // std::println("{}", schema.to_string());
        // auto scan{table.scan()};
        // while (auto entry{scan.next()}) {
        //     if (entry.has_value()) {
        //         duck::Tuple tuple{entry.value().second};
        //         // duck::Value value{tuple.get(2)};
        //         std::println("{}", tuple.to_string());
        //     } else {
        //         std::println("Tuple not found");
        //     }
        // }

    } catch (std::exception& e) {
        std::cout << "Exception: " << e.what() << "\n";
    }
}