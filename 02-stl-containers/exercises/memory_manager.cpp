// 练习1: 实现简单的内存块管理器
// 文件: exercises/memory_manager.cpp

#include <vector>
#include <optional>
#include <cstring>
#include <stdexcept>
#include <iostream>

class MemoryBlock {
private:
    std::vector<char> data;
    
public:
    MemoryBlock(size_t size) : data(size, 0) {}
    
    size_t size() const { return data.size(); }
    char* raw() { return data.data(); }
    const char* raw() const { return data.data(); }
    
    void copy_from(const void* src, size_t len, size_t offset = 0) {
        if (offset + len > data.size()) {
            throw std::out_of_range("copy_from: out of range");
        }
        std::memcpy(data.data() + offset, src, len);
    }
};

class MemoryManager {
private:
    std::vector<std::optional<MemoryBlock>> blocks;
    std::vector<size_t> free_list;
    
public:
    // 分配新块：优先复用空闲 ID
    size_t allocate(size_t size) {
        if (!free_list.empty()) {
            size_t id = free_list.back();
            free_list.pop_back();
            blocks[id].emplace(size);
            return id;
        }
        blocks.emplace_back(size);
        return blocks.size() - 1;
    }
    
    // 写入数据
    void write(size_t block_id, const void* data, size_t len, size_t offset = 0) {
        if (block_id >= blocks.size() || !blocks[block_id]) {
            throw std::out_of_range("Invalid block ID");
        }
        blocks[block_id]->copy_from(data, len, offset);
    }
    
    // 读取数据
    void read(size_t block_id, void* out, size_t len, size_t offset = 0) {
        if (block_id >= blocks.size() || !blocks[block_id]) {
            throw std::out_of_range("Invalid block ID");
        }
        auto& block = *blocks[block_id];
        if (offset + len > block.size()) {
            throw std::out_of_range("read: out of range");
        }
        std::memcpy(out, block.raw() + offset, len);
    }
    
    // 释放块：惰性删除，ID 保持稳定
    void deallocate(size_t block_id) {
        if (block_id < blocks.size() && blocks[block_id]) {
            blocks[block_id].reset();
            free_list.push_back(block_id);
        }
    }
    
    size_t block_count() const {
        size_t count = 0;
        for (const auto& b : blocks) {
            if (b) ++count;
        }
        return count;
    }
    
    size_t total_size() const {
        size_t total = 0;
        for (const auto& b : blocks) {
            if (b) total += b->size();
        }
        return total;
    }
};

int main() {
    MemoryManager mgr;
    
    size_t id1 = mgr.allocate(1024);
    size_t id2 = mgr.allocate(512);
    size_t id3 = mgr.allocate(256);
    
    const char* msg = "Hello World!";
    mgr.write(id1, msg, strlen(msg));
    
    char buf[20] = {0};
    mgr.read(id1, buf, 13, 0);
    
    std::cout << "Read: " << buf << "\n";
    std::cout << "Blocks before free: " << mgr.block_count() << "\n";
    std::cout << "Total size before free: " << mgr.total_size() << "\n";
    
    mgr.deallocate(id1);
    std::cout << "Blocks after free id1: " << mgr.block_count() << "\n";
    
    // id2 和 id3 仍然有效
    mgr.write(id2, "still valid", 11);
    mgr.read(id2, buf, 11);
    std::cout << "id2 after id1 freed: " << buf << "\n";
    
    // 复用 id1 的空闲 ID
    size_t id4 = mgr.allocate(64);
    std::cout << "New allocation reused id: " << id4 << " (was id1=" << id1 << ")\n";
    
    return 0;
}
