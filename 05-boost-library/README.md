# 阶段五：Boost库（按需学习）

Ceph大量使用Boost库，以下是核心组件：

## 1. Boost.Container

```cpp
#include <boost/container/vector.hpp>
#include <boost/container/small_vector.hpp>

// small_vector: 小容量时使用栈内存，超出才用堆
// Ceph: src/osd/osd_types.h 使用 small_vector

boost::container::small_vector<int, 10> vec;  // 前10个元素在栈上
vec.push_back(1);
vec.push_back(2);
// 超过10个才分配堆内存
```

## 2. Boost.Intrusive（侵入式容器）

```cpp
#include <boost/intrusive/list.hpp>

// 侵入式容器：元素本身包含链表节点
// 优势：无额外内存分配，更适合内存敏感场景

struct Object : public boost::intrusive::list_base_hook<> {
    int id;
    std::string name;
};

boost::intrusive::list<Object> obj_list;

// Ceph: src/include/RefCountedObj.h 使用 intrusive_ptr
// intrusive_ptr：引用计数嵌入对象内部，避免shared_ptr的额外开销
```

## 3. Boost.SmartPtr

```cpp
#include <boost/smart_ptr/intrusive_ptr.hpp>

// intrusive_ptr 比shared_ptr更高效（引用计数在对象内部）

class RefCounted {
public:
    void add_ref() { ++ref_count_; }
    void release() { if (--ref_count_ == 0) delete this; }
private:
    int ref_count_ = 0;
};

// 全局函数（intrinsic_ptr需要）
void intrusive_ptr_add_ref(RefCounted* p) { p->add_ref(); }
void intrusive_ptr_release(RefCounted* p) { p->release(); }

boost::intrusive_ptr<RefCounted> ptr(new RefCounted());

// Ceph: src/include/RefCountedObj.h 是类似实现
```

## 4. Boost.Asio（异步IO）

```cpp
#include <boost/asio.hpp>

// TCP服务器示例
using boost::asio::ip::tcp;

void session(tcp::socket sock) {
    try {
        char data[1024];
        while (true) {
            size_t length = sock.read_some(boost::asio::buffer(data));
            boost::asio::write(sock, boost::asio::buffer(data, length));
        }
    } catch (std::exception& e) {
        // 连接关闭
    }
}

// Ceph: 旧版Messenger使用Asio
// 新版Crimson使用Seastar（更高性能）
```

## 5. Boost.Format

```cpp
#include <boost/format.hpp>

// 类型安全的格式化
std::string s = (boost::format("Hello %s, id=%d") % "World" % 42).str();

// 比printf安全，比iostream格式化能力强

// Ceph: src/common/fmt_common.h 使用fmt库（更现代的替代）
```

## 6. Boost.Optional

```cpp
#include <boost/optional.hpp>

// C++17前使用boost::optional
boost::optional<int> find_value(const std::string& key) {
    if (key == "valid") {
        return 42;
    }
    return boost::none;  // 空值
}

// C++17后使用std::optional
```

## 7. Boost.Variant

```cpp
#include <boost/variant.hpp>

// 类型安全的union
using Value = boost::variant<int, std::string, double>;

Value v1 = 42;
Value v2 = std::string("hello");
Value v3 = 3.14;

// 访问使用visitor
struct Visitor : boost::static_visitor<void> {
    void operator()(int i) const { std::cout << "int: " << i << "\n"; }
    void operator()(const std::string& s) const { std::cout << "string: " << s << "\n"; }
    void operator()(double d) const { std::cout << "double: " << d << "\n"; }
};

boost::apply_visitor(Visitor(), v1);

// C++17后使用std::variant
// Ceph配置系统使用std::variant (src/common/config.h)
```

## 8. Boost.ProgramOptions

```cpp
#include <boost/program_options.hpp>

namespace po = boost::program_options;

po::options_description desc("Options");
desc.add_options()
    ("help,h", "Show help")
    ("port,p", po::value<int>()->default_value(8080), "Port number")
    ("config,c", po::value<std::string>(), "Config file");

po::variables_map vm;
po::store(po::parse_command_line(argc, argv, desc), vm);
po::notify(vm);

if (vm.count("help")) {
    std::cout << desc << "\n";
}

// Ceph使用自定义的配置系统，但概念类似
```

## Ceph中Boost使用位置

```
src/common/config.h               # boost::variant/std::variant
src/osd/osd_types.h               # boost::container::small_vector
src/include/RefCountedObj.h       # intrusive_ptr模式
src/msg/async/AsyncMessenger.cc   # boost::asio (经典版)
```

## 安装Boost

```bash
# Ubuntu/Debian
sudo apt install libboost-all-dev

# CentOS/RHEL  
sudo yum install boost-devel
```

## 学习建议

1. 按需学习：遇到不懂的Boost组件再查
2. 优先学习现代C++等价物：
   - `boost::optional` → `std::optional`
   - `boost::variant` → `std::variant`
   - `boost::shared_ptr` → `std::shared_ptr`
3. Ceph特有的组件：
   - `boost::container::small_vector`（无STL等价）
   - `boost::intrusive_ptr`（比shared_ptr高效）