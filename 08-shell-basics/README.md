# 阶段六：Shell基础

> Shell是Ceph构建、测试、运维的基础工具，必须掌握到"实用级别"

## 为什么学Shell？

Ceph学习中会频繁用到：
- 构建代码
- 运行测试
- 分析日志
- 集群运维

## 1. 基础变量

```bash
# 变量
NAME="ceph"
VERSION=18

# 使用变量
echo "Project: $NAME, Version: $VERSION"
echo "Project: ${NAME}"  # 更明确的边界

# 命令替换
DATE=$(date +%Y%m%d)
FILES=$(ls *.cc)

# 特殊变量
$?    # 上一个命令返回值
$#    # 脚本参数个数
$@    # 所有参数
$$    # 当前进程PID
```

## 2. 条件判断

```bash
# if语句
if [ -f "/etc/ceph/ceph.conf" ]; then
    echo "Config exists"
elif [ -d "/etc/ceph" ]; then
    echo "Directory exists"
else
    echo "Not found"
fi

# 常用测试
[ -f file ]      # 文件存在
[ -d dir ]       # 目录存在
[ -r file ]      # 可读
[ -w file ]      # 可写
[ -x file ]      # 可执行
[ -z "$var" ]    # 变量为空
[ -n "$var" ]    # 变量非空
[ "$a" = "$b" ]  # 字符串相等
[ "$a" -eq "$b" ]  # 数字相等
```

## 3. 循环

```bash
# for循环
for i in 1 2 3; do
    echo $i
done

for file in *.cc; do
    echo "Processing $file"
done

for ((i=0; i<10; i++)); do
    echo $i
done

# while循环
count=0
while [ $count -lt 10 ]; do
    echo $count
    ((count++))
done

# 读取文件
while read line; do
    echo $line
done < file.txt
```

## 4. 函数

```bash
# 定义函数
log_info() {
    echo "[$(date)] INFO: $1"
}

log_error() {
    echo "[$(date)] ERROR: $1" >&2
}

# 调用
log_info "Starting OSD"
log_error "Failed to connect"

# 返回值
get_osd_count() {
    echo $(ceph osd ls | wc -l)
}
count=$(get_osd_count)
```

## 5. 管道与重定向

```bash
# 管道：将一个命令的输出作为另一个命令的输入
ceph osd tree | grep up
cat log | grep ERROR | wc -l

# 重定向
command > file        # 标准输出到文件（覆盖）
command >> file       # 标准输出到文件（追加）
command 2> file       # 错误输出到文件
command > file 2>&1   # 标准输出+错误输出到文件
command < file        # 从文件读输入

# tee：同时输出到屏幕和文件
ceph status | tee status.log
```

## 6. 常用命令组合

```bash
# 查找并处理
find src -name "*.cc" -exec grep "OSD" {} \;

# 文本处理
cat log | grep ERROR | awk '{print $3}' | sort | uniq -c

# 批量处理
for osd in $(ceph osd ls); do
    ceph osd down $osd
done

# 查看进程
ps aux | grep ceph-osd

# 网络连接
ss -tlnp | grep 6800

# 磁盘使用
df -h | grep ceph
```

## 7. Ceph常用命令

```bash
# 集群状态
ceph status
ceph health detail

# OSD操作
ceph osd ls
ceph osd tree
ceph osd df

# Pool操作
ceph osd pool ls
ceph osd pool get replicated size

# 日志查看
journalctl -u ceph-osd@0 -f

# 构建相关
cd build && ninja
ninja install
./bin/ceph status
```

## 8. 分析日志示例

```bash
# 统计错误类型
grep -E "ERROR|WARN" ceph.log | awk '{print $4}' | sort | uniq -c | sort -rn

# 找出所有延迟高的操作
grep "slow request" ceph.log | awk '{print $NF}' | sort -n | tail -20

# 提取时间范围内的日志
awk '/2024-01-01 10:00/,/2024-01-01 11:00/' ceph.log

# 查看OSD状态变化
grep "osd\." ceph.log | grep "state" | tail -100
```

## 9. 脚本示例：批量启动OSD

```bash
#!/bin/bash
# start_osds.sh

NUM_OSD=$1

if [ -z "$NUM_OSD" ]; then
    echo "Usage: $0 <num_osds>"
    exit 1
fi

for i in $(seq 0 $((NUM_OSD-1))); do
    echo "Starting OSD.$i..."
    ceph-osd -i $i --conf /etc/ceph/ceph.conf &
    
    if [ $? -eq 0 ]; then
        echo "OSD.$i started successfully"
    else
        echo "Failed to start OSD.$i" >&2
    fi
done

echo "All done. Checking status..."
ceph osd tree
```

## 10. 检查清单

完成本阶段后，你应该能：

- [ ] 写简单的Shell脚本
- [ ] 使用变量和条件判断
- [ ] 使用循环处理批量任务
- [ ] 使用管道和重定向
- [ ] 使用grep/awk/find处理文本
- [ ] 执行Ceph常用命令
- [ ] 分析Ceph日志

## 练习

1. 写一个脚本，批量获取所有OSD的状态
2. 写一个脚本，分析日志中的错误统计
3. 写一个脚本，自动备份配置文件

## Ceph相关脚本

```
src/vstart.sh              # 启动测试集群
src/stop.sh                # 停止集群
src/script/                # 各种脚本
qa/                        # 测试脚本
src/entrypoint/            # 容器入口脚本
```