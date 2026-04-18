// 练习2: 实现简单的对象存储索引
// 文件: exercises/object_store.cpp

#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <optional>
#include <ctime>
#include <cstdint>
#include <algorithm>

struct ObjectInfo {
    std::string oid;            // 对象ID
    uint64_t size;              // 大小
    uint64_t modification_time;  // 修改时间
    std::string locator;         // 定位器
    
    void print() const {
        std::cout << "Object: " << oid 
                  << ", size=" << size
                  << ", mtime=" << modification_time << "\n";
    }
};

class ObjectIndex {
private:
    std::map<std::string, ObjectInfo> objects;      // oid -> ObjectInfo
                                                    // oid 到 ObjectInfo的映射
    std::map<std::string, std::vector<std::string>> locator_index;  // locator -> [oids]
                                                                    // locator 到 oid 列表的映射
    
public:
    // 创建对象
    void create(const std::string& oid, uint64_t size, const std::string& locator = "") {
        ObjectInfo info;
        info.oid = oid;
        info.size = size;
        info.modification_time = std::time(nullptr);
        info.locator = locator.empty() ? oid : locator;
        
        objects[oid] = info;
        
        // 更新locator索引
        locator_index[info.locator].push_back(oid);
    }
    
    // 删除对象
    bool remove(const std::string& oid) {
        auto it = objects.find(oid);
        if (it == objects.end()) return false;
        
        // 从locator索引中删除
        auto& vec = locator_index[it->second.locator];
        vec.erase(std::remove(vec.begin(), vec.end(), oid), vec.end());
        
        objects.erase(it);
        return true;
    }
    
    // 获取对象信息
    std::optional<ObjectInfo> get(const std::string& oid) const {
        auto it = objects.find(oid);
        if (it == objects.end()) return std::nullopt;
        return it->second;
    }
    
    // 按locator列出对象
    std::vector<ObjectInfo> list_by_locator(const std::string& locator) const {
        std::vector<ObjectInfo> result;
        auto it = locator_index.find(locator);
        if (it != locator_index.end()) {
            for (const auto& oid : it->second) {
                auto obj = get(oid);
                if (obj) result.push_back(*obj);
            }
        }
        return result;
    }
    
    // 列出所有对象
    std::vector<ObjectInfo> list_all() const {
        std::vector<ObjectInfo> result;
        for (const auto& [oid, info] : objects) {
            result.push_back(info);
        }
        return result;
    }
    
    size_t count() const { return objects.size(); }
};

int main() {
    ObjectIndex index;
    
    // 创建对象
    index.create("obj1", 1024, "pool1");
    index.create("obj2", 2048, "pool1");
    index.create("obj3", 512, "pool2");
    
    // 查询
    auto obj = index.get("obj1");
    if (obj) {
        obj->print();
    }
    
    // 按locator列出
    std::cout << "\nObjects in pool1:\n";
    for (const auto& o : index.list_by_locator("pool1")) {
        o.print();
    }
    
    // 删除
    index.remove("obj2");
    std::cout << "\nAfter deletion, count: " << index.count() << "\n";
    
    return 0;
}