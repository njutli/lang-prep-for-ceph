// 练习1: 实现简单的内存块管理器
// 文件: exercises/memory_manager.cpp

#include <vector>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <ctime>

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
    std::vector<MemoryBlock> blocks;
    
public:
    // 分配新块
    size_t allocate(size_t size) {
        blocks.emplace_back(size);
        return blocks.size() - 1;
    }
    
    // 写入数据
    void write(size_t block_id, const void* data, size_t len, size_t offset = 0) {
        if (block_id >= blocks.size()) {
            throw std::out_of_range("Invalid block ID");
        }
        blocks[block_id].copy_from(data, len, offset);
    }
    
    // 读取数据
    void read(size_t block_id, void* out, size_t len, size_t offset = 0) {
        if (block_id >= blocks.size()) {
            throw std::out_of_range("Invalid block ID");
        }
        auto& block = blocks[block_id];
        if (offset + len > block.size()) {
            throw std::out_of_range("read: out of range");
        }
        std::memcpy(out, block.raw() + offset, len);
    }
    
    // 释放块
    void deallocate(size_t block_id) {
        if (block_id < blocks.size()) {
            blocks.erase(blocks.begin() + block_id);
        }
    }
    
    size_t block_count() const { return blocks.size(); }
    size_t total_size() const {
        size_t total = 0;
        for (const auto& b : blocks) {
            total += b.size();
        }
        return total;
    }
};

int main() {
    MemoryManager mgr;
    
    size_t id1 = mgr.allocate(1024);
    size_t id2 = mgr.allocate(512);
    
    const char* msg = "Hello World!";
    mgr.write(id1, msg, strlen(msg));
    
    char buf[20] = {0};
    mgr.read(id1, buf, 13, 0);
    
    std::cout << "Read: " << buf << "\n";
    std::cout << "Total blocks: " << mgr.block_count() << "\n";
    return 0;
}