// 练习3: RAII封装文件操作
// 文件: exercises/raii_file.cpp

#include <cstdio>
#include <stdexcept>
#include <string>

class File {
private:
    FILE* handle;
    
public:
    // 构造：获取资源
    File(const char* filename, const char* mode) {
        handle = fopen(filename, mode);
        if (!handle) {
            throw std::runtime_error("Cannot open file");
        }
    }
    
    // 析构：释放资源
    ~File() {
        if (handle) {
            fclose(handle);
        }
    }
    
    // 禁止拷贝
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    
    // 允许移动
    File(File&& other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }
    
    File& operator=(File&& other) noexcept {
        if (this != &other) {
            if (handle) fclose(handle);
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
    
    // 操作方法
    void write(const std::string& content) {
        fputs(content.c_str(), handle);
    }
    
    std::string read_line() {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), handle)) {
            return std::string(buffer);
        }
        return "";
    }
};

int main() {
    {
        File f("test.txt", "w");
        f.write("Hello RAII!\n");
    }  // 自动关闭
    
    {
        File f("test.txt", "r");
        std::string line = f.read_line();
        // 使用line...
    }  // 自动关闭
    
    // File f2 = f;  // 编译错误！禁止拷贝
    
    return 0;
}