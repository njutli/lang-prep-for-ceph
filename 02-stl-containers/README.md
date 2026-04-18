# 阶段二：STL容器与算法

## 1. 序列容器

### 1.1 std::vector - 动态数组

```cpp
#include <vector>

std::vector<int> vec;
vec.push_back(1);           // 尾部添加
vec.emplace_back(2);        // 原地构造（更高效）
vec.pop_back();             // 尾部删除

// 遍历
for (const auto& x : vec) { }
for (size_t i = 0; i < vec.size(); ++i) { }

// C++17结构化绑定
for (auto& [x, y] : pair_vec) { }

// Ceph: src/include/buffer.h — buffer::list 内部使用自定义单向链表
// buffers_t（不是 vector），管理数据块列表
```

### 1.2 std::string - 字符串

```cpp
#include <string>

std::string s = "Hello";
s += " World";              // 拼接
s.find("W");                // 查找
s.substr(0, 5);             // 子串

// C++17: std::string_view 非拥有字符串视图
#include <string_view>
std::string_view sv = "hello";  // 不拷贝，只是引用
```

### 1.3 std::list - 双向链表

```cpp
#include <list>

std::list<int> lst;
lst.push_front(1);          // 头部插入
lst.push_back(2);           // 尾部插入
lst.insert(it, 3);          // 迭代器位置插入

// list优势：任意位置插入/删除O(1)
// 注意：不支持随机访问
```

### 1.4 std::deque - 双端队列

```cpp
#include <deque>

std::deque<int> dq;
dq.push_front(1);           // 头部插入
dq.push_back(2);            // 尾部插入
dq[0];                      // 支持随机访问

// Ceph: 存储消息队列等场景
```

## 2. 关联容器

### 2.1 std::map / std::unordered_map

```cpp
#include <map>
#include <unordered_map>

// std::map - 红黑树，有序，O(log n)
std::map<std::string, int> orders;
orders["key"] = 42;
orders.insert({"key2", 100});
orders.find("key");             // 返回迭代器

// std::unordered_map - 哈希表，O(1)平均
std::unordered_map<int, std::string> hashmap;
hashmap[1] = "one";

// Ceph示例: src/osdc/Objecter.h — 使用 map 管理对象操作
// std::map<ceph_tid_t, Op*> ops;
// std::map<uint64_t, LingerOp*> linger_ops;
```

### 2.2 std::set / std::unordered_set

```cpp
#include <set>
#include <unordered_set>

// std::set - 有序集合，自动去重
std::set<int> s;
s.insert(1);
s.insert(2);
s.count(1);                      // 0或1

// std::unordered_set - 哈希集合
std::unordered_set<int> us;
```

## 3. 容器适配器

```cpp
#include <stack>
#include <queue>

// 栈
std::stack<int> st;
st.push(1);
st.top();           // 访问栈顶
st.pop();           // 弹出栈顶

// 队列
std::queue<int> q;
q.push(1);
q.front();
q.pop();

// 优先队列
std::priority_queue<int> pq;
pq.push(3);
pq.push(1);
pq.top();           // 最大元素
```

## 4. 算法库

```cpp
#include <algorithm>
#include <numeric>

std::vector<int> v = {3, 1, 4, 1, 5, 9};

// 排序
std::sort(v.begin(), v.end());

// 查找
auto it = std::find(v.begin(), v.end(), 4);
bool found = std::binary_search(v.begin(), v.end(), 4);

// 计数
int cnt = std::count(v.begin(), v.end(), 1);

// 遍历
std::for_each(v.begin(), v.end(), [](int x) {
    std::cout << x << " ";
});

// 拷贝/转换
std::transform(v.begin(), v.end(), v.begin(), 
    [](int x) { return x * 2; });

// 删除特定元素（erase-remove惯用法）
v.erase(std::remove(v.begin(), v.end(), 1), v.end());

// 累加
int sum = std::accumulate(v.begin(), v.end(), 0);
```

## 5. Lambda表达式

```cpp
// 基本语法
auto f1 = [](int x) { return x * 2; };
f1(5);  // 10

// 捕获外部变量
int y = 10;
auto f2 = [y](int x) { return x + y; };      // 值捕获
auto f3 = [&y](int x) { return x + y; };     // 引用捕获
auto f4 = [=](int x) { return x + y; };      // 全部值捕获
auto f5 = [&](int x) { return x + y; };      // 全部引用捕获

// Ceph大量使用lambda与STL算法配合
```

## 练习

参考 `exercises/` 目录：

1. 实现简单的内存块管理器（使用vector）
2. 实现一个简单的对象存储索引（使用map）

## Ceph源码阅读建议

```
src/include/buffer.h         # bufferlist/bufferptr实现（自定义单向链表）
src/osdc/Objecter.h           # map管理对象操作的典型用法
src/osd/OSDMap.h              # 大量使用map存储集群状态（mempool::osdmap::map）
```