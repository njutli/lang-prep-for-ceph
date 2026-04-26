# 阶段九：Python基础（辅助工具）

## 🛑 初学者必读：Python 要学到什么程度？

**结论：初步学习 Ceph 核心源码时，几乎不需要掌握 Python。**

Ceph 的核心逻辑（OSD、MON、MDS、CRUSH）全部由 **C++** 编写。Python 在 Ceph 中仅用于管理工具、测试脚本和 Dashboard 模块。

| 学习目标 | 所需 Python 程度 | 推荐 |
| :--- | :--- | :--- |
| **只读 C++ 核心源码** | **入门**：能看懂变量、循环、`print` 即可 | ✅ 够用 |
| **看懂构建/测试脚本** | **基础**：理解 `subprocess`、`with` 语句 | ✅ 够用 |
| **研究 Mgr/Dashboard** | **进阶**：装饰器、`asyncio`、`rados` 绑定 | ⏳ 按需学习 |

> **版本建议**：直接学习 **Python 3.6+**（Ceph 早已彻底废弃 Python 2）。
> **建议**：不要为了读 Ceph 源码去系统学习 Python。遇到看不懂的脚本，查一下语法即可。

---

Python 主要用于 Ceph 的管理工具、测试脚本和 mgr 模块。

## 1. Python基础语法

```python
# 变量和类型
name = "ceph"           # 字符串
version = 18.0          # 浮点数
count = 42              # 整数

# 列表（类似vector）
hosts = ["mon", "osd", "mds"]
hosts.append("rgw")
print(hosts[0])  # "mon"

# 字典（类似map）
config = {"mon_host": "192.168.1.1", "port": 6789}
print(config["mon_host"])

# 条件和循环
for host in hosts:
    if host == "osd":
        print("Storage node")
```

## 2. 函数和类

```python
# 函数
def get_cluster_status(cluster_name="ceph"):
    return f"Cluster {cluster_name} is healthy"

# 类
class OSD:
    def __init__(self, osd_id):
        self.osd_id = osd_id
        self.status = "up"
    
    def get_info(self):
        return {"id": self.osd_id, "status": self.status}
    
    def __str__(self):
        return f"OSD.{self.osd_id}"

# 使用
osd = OSD(1)
print(osd)  # "OSD.1"
```

## 3. 异常处理

```python
try:
    result = perform_operation()
except FileNotFoundError as e:
    print(f"File not found: {e}")
except Exception as e:
    print(f"Error: {e}")
finally:
    cleanup()
```

## 4. 文件操作

```python
# 读写文件
with open("ceph.conf", "r") as f:
    content = f.read()

with open("output.txt", "w") as f:
    f.write("Hello")
```

## 5. 模块和包

```python
# 导入标准库
import os
import json
import subprocess

# 执行命令
result = subprocess.run(["ceph", "status"], capture_output=True, text=True)
print(result.stdout)

# JSON处理
data = json.loads('{"key": "value"}')
json_str = json.dumps(data)
```

## 6. 列表推导式

```python
# 简洁的列表操作
numbers = [1, 2, 3, 4, 5]
squares = [x**2 for x in numbers]          # [1, 4, 9, 16, 25]
even = [x for x in numbers if x % 2 == 0]  # [2, 4]

# 字典推导式
squares_dict = {x: x**2 for x in numbers}  # {1: 1, 2: 4, ...}
```

## 7. 装饰器

```python
# 装饰器：修改函数行为
def log_call(func):
    def wrapper(*args, **kwargs):
        print(f"Calling {func.__name__}")
        result = func(*args, **kwargs)
        print(f"Finished {func.__name__}")
        return result
    return wrapper

@log_call
def get_osd_status(osd_id):
    return "up"

# 相当于 get_osd_status = log_call(get_osd_status)
```

## 8. 上下文管理器

```python
# 类似RAII，自动管理资源
class DatabaseConnection:
    def __enter__(self):
        print("Connecting...")
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        print("Closing connection...")
    
    def query(self, sql):
        return []

# 使用
with DatabaseConnection() as db:
    result = db.query("SELECT * FROM osds")

# 退出with块自动调用__exit__
```

## 9. 异步Python（asyncio）

```python
import asyncio

async def fetch_osd_status(osd_id):
    await asyncio.sleep(1)  # 模拟异步IO
    return {"osd_id": osd_id, "status": "up"}

async def main():
    tasks = [
        fetch_osd_status(1),
        fetch_osd_status(2),
        fetch_osd_status(3)
    ]
    results = await asyncio.gather(*tasks)
    print(results)

asyncio.run(main())
```

## 10. Ceph Python代码位置

```
src/pybind/                  # Python绑定代码
src/pybind/mgr/              # MGR模块（Python实现）
    ├── dashboard/           # Web管理界面
    ├── orchestrator/        # 集群编排
    └── progress/            # 进度管理

src/cephadm/                 # 集群部署工具
src/ceph-volume/             # 卷管理

qa/                          # 测试脚本
```

## 11. 阅读Ceph Python代码建议

### mgr/dashboard示例

```python
# src/pybind/mgr/dashboard/controllers/osd.py (简化)
class OSDController(BaseController):
    @Endpoint('GET')
    def list(self):
        osds = self.get_osd_map()
        return {'osds': [self._format_osd(osd) for osd in osds]}
    
    @Endpoint('POST')
    def create(self, osd_spec):
        # 创建新的OSD
        pass
```

### 常见模式

```python
# 连接RADOS集群
import rados

cluster = rados.Rados(conffile='/etc/ceph/ceph.conf')
cluster.connect()

# 获取Pool列表
pools = cluster.list_pools()

# 操作对象
ioctx = cluster.open_ioctx('mypool')
ioctx.write('object_name', b'data', offset=0)
data = ioctx.read('object_name', length=100, offset=0)
```

## 12. 学习资源

- Python官方教程: https://docs.python.org/zh-cn/3/tutorial/
- Ceph Python模块: `src/pybind/mgr/`
- 重点关注：`dashboard/controllers/` 目录

## 学习建议

1. **Python是可选的**：如果只关注C++核心代码，可以跳过
2. **学习重点**：
   - 理解mgr模块如何与C++核心交互
   - 阅读dashboard代码了解Web API实现
3. **优先级低**：Ceph核心逻辑是C++，Python主要是管理和界面