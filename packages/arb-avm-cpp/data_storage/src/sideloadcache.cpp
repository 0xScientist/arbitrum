/*
 * Copyright 2021, Offchain Labs, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <data_storage/sideloadcache.hpp>

size_t SideloadCache::size() {
    std::shared_lock lock(mutex);

    return cache.size();
}

void SideloadCache::add(std::unique_ptr<Machine> machine) {
    std::unique_lock lock(mutex);

    auto block_number = machine->machine_state.output.l2_block_number;
    auto timestamp = machine->machine_state.output.last_inbox_timestamp;

    reorgNoLock(block_number);
    deleteExpiredNoLock();

    if (timestamp <= expiredTimestamp()) {
        // Don't save expired machine to cache
        return;
    }

    // Add new entry
    cache[block_number].timestamp = timestamp;
    cache[block_number].machine = std::move(machine);
}

std::unique_ptr<Machine> SideloadCache::get(uint256_t block_number) {
    std::shared_lock lock(mutex);

    auto it = cache.find(block_number);
    if (it == cache.end()) {
        return nullptr;
    }

    return std::make_unique<Machine>(*it->second.machine);
}

void SideloadCache::reorg(uint256_t next_block_number) {
    std::unique_lock lock(mutex);

    reorgNoLock(next_block_number);
}

void SideloadCache::reorgNoLock(uint256_t next_block_number) {
    if (next_block_number <= cache.begin()->first) {
        // Remove everything
        cache.clear();
    }

    auto it = cache.lower_bound(next_block_number);
    cache.erase(it, cache.end());
}

void SideloadCache::deleteExpiredNoLock() {
    auto expired = expiredTimestamp();
    for (auto it = cache.cbegin();
         it != cache.cend() && it->second.timestamp <= expired;) {
        it = cache.erase(it);
    }
}

uint256_t SideloadCache::expiredTimestamp() const {
    return std::time(nullptr) - expiration_seconds;
}