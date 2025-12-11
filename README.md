# Stealth Kernel Memory Access Module

一个完全隐蔽的内核内存访问模块

## 目录结构

```
kernel_mem_module/
├── src/                          # 内核模块源代码目录
│   ├── hook_impl.h               # Hook实现头文件
│   ├── hook_impl.c               # Hook实现源文件
│   ├── mem_hook.h                # 内存钩子头文件
│   ├── mem_hook.c                # 内存钩子源文件
│   └── main.c                    # 模块入口点
├── Makefile                      # 构建配置文件
├── userland/                     # 用户态工具目录
│   └── tester.c                  # 基础测试程序
├── examples/                     # 完整使用示例目录
│   └── demo.c                    # 高级演示程序（C++风格封装）
└── README.md                     # 本说明文档
```

## 核心特性

- ✅ **完全隐蔽**：不创建任何文件系统条目（proc/sysfs/debugfs）
- ✅ **零痕迹**：无日志输出，无 printk 调用
- ✅ **仅 root 可访问**：通过 CAP_SYS_ADMIN 能力检查
- ✅ **无限制内存访问**：支持任意虚拟/物理内存读写
- ✅ **适配 5.10 内核**：针对 Android 设备优化
- ✅ **Netlink 通信**：使用内核 Netlink Socket 实现隐蔽通道

## 文件详细说明

### src/hook_impl.h - Hook 实现头文件

**功能描述**：定义了内核函数 Hook 的核心数据结构和接口函数。

**主要数据结构**：
```c
typedef struct {
    void *original_func;   // 原始函数地址
    void *hook_func;       // 钩子函数地址  
    void *trampoline;      // 跳板代码地址
    size_t trampoline_size; // 跳板代码大小
} hook_t;
```

**导出函数**：
- `void *hook_mem_zalloc(void)` - 分配并清零 Hook 结构体内存
- `void hook_mem_free(void *hook_mem)` - 释放 Hook 结构体内存
- `void *find_symbol(const char *name)` - 通过符号名称查找内核函数地址
- `int hook_install(hook_t *hook, void *target, void *replacement)` - 安装 Hook
- `int hook_remove(hook_t *hook)` - 移除 Hook

**依赖头文件**：
- `<linux/types.h>` - 基础类型定义
- `<linux/kallsyms.h>` - 内核符号表访问
- `<linux/vmalloc.h>` - 虚拟内存分配
- `<linux/uaccess.h>` - 用户空间访问
- `<linux/string.h>` - 字符串操作

### src/hook_impl.c - Hook 实现源文件

**功能描述**：实现了完整的内核函数 Hook 机制，包括跳板代码生成和指令修改。

**关键实现细节**：

1. **内存分配**：
   - 使用 `vmalloc()` 分配连续虚拟内存
   - 自动初始化为零值

2. **符号查找**：
   - 主要使用 `kallsyms_lookup_name()` 
   - 提供备用查找机制

3. **Hook 安装流程**：
   ```c
   // 1. 保存原始函数地址
   // 2. 创建跳板代码（复制原始指令 + 跳转回原始函数）
   // 3. 修改目标函数开头为跳转到钩子函数
   // 4. 处理内存写保护（CR0.WP 位）
   ```

4. **x86_64 架构支持**：
   - 使用 5 字节 JMP rel32 指令
   - 正确计算相对偏移量
   - 处理内存权限切换

5. **Hook 移除**：
   - 恢复原始函数指令
   - 释放跳板内存
   - 保持内存状态一致性

### src/mem_hook.h - 内存钩子头文件

**功能描述**：定义了内存操作的核心接口和 Netlink 通信协议。

**常量定义**：
- `MEM_HOOK_NETLINK = 30` - 自定义 Netlink 协议号
- `MAX_PAYLOAD = 1024` - 最大单次操作数据量

**枚举类型**：
```c
typedef enum {
    MEM_READ,    // 虚拟内存读取
    MEM_WRITE,   // 虚拟内存写入  
    PHYS_READ,   // 物理内存读取
    PHYS_WRITE   // 物理内存写入
} mem_op_t;
```

**请求结构体**：
```c
typedef struct {
    mem_op_t op;           // 操作类型
    uint64_t addr;         // 目标地址（虚拟或物理）
    uint64_t len;          // 操作长度（≤1024）
    uint8_t data;    // 数据缓冲区
} mem_request_t;
```

**导出函数**：
- `int init_netlink(void)` - 初始化 Netlink 通信
- `void cleanup_netlink(void)` - 清理 Netlink 资源
- `void nl_receive(struct sk_buff *skb)` - Netlink 接收处理
- `int perform_memory_op(mem_request_t *req)` - 执行内存操作

**依赖头文件**：
- `<linux/module.h>` - 模块基础
- `<linux/kernel.h>` - 内核基础
- `<linux/netlink.h>` - Netlink 协议
- `<net/sock.h>` - Socket 支持
- `<linux/capability.h>` - 能力检查
- `<linux/uaccess.h>` - 用户空间访问
- `<linux/io.h>` - I/O 映射

### src/mem_hook.c - 内存钩子源文件

**功能描述**：实现了完整的内存操作功能和 Netlink 通信处理。

**核心功能实现**：

1. **内存操作分发**：
   ```c
   switch(req->op) {
       case MEM_READ: copy_to_user()
       case MEM_WRITE: copy_from_user()  
       case PHYS_READ: ioremap() + memcpy()
       case PHYS_WRITE: ioremap() + memcpy()
   }
   ```

2. **安全访问控制**：
   - 使用 `capable(CAP_SYS_ADMIN)` 严格限制访问权限
   - 仅允许 root 进程连接和操作

3. **Netlink 通信**：
   - 注册自定义 Netlink 协议 (30)
   - 实现双向通信（请求/响应）
   - 使用 `netlink_unicast()` 发送响应

4. **物理内存访问**：
   - 使用 `ioremap()` 映射物理地址到虚拟地址
   - 操作完成后调用 `iounmap()` 释放映射
   - 错误处理确保资源不泄漏

5. **错误处理**：
   - 返回标准 Linux 错误码（-EINVAL, -EFAULT, -ENOMEM）
   - 确保所有路径都有适当的错误返回

### src/main.c - 模块入口点

**功能描述**：内核模块的初始化和清理入口点。

**模块生命周期**：

1. **初始化 (`mem_hook_init`)**：
   - 分配全局 Hook 结构体
   - 初始化 Netlink 通信通道
   - 注册模块到内核

2. **清理 (`mem_hook_exit`)**：
   - 清理 Netlink 资源
   - 释放全局 Hook 结构体
   - 从内核卸载模块

**模块元数据**：
- `MODULE_LICENSE("GPL")` - GPL 许可证
- `MODULE_AUTHOR("Your Name")` - 作者信息
- `MODULE_DESCRIPTION("Stealth Memory Access Module")` - 模块描述

### userland/tester.c - 基础测试程序

**功能描述**：简单的 C 语言测试程序，验证基本功能。

**主要功能**：
- 创建 Netlink Socket 连接
- 发送内存读取请求
- 接收并解析响应
- 基础错误处理

**编译命令**：
```bash
aarch64-linux-android-gcc userland/tester.c -o userland/tester
```

**使用方法**：
```bash
./tester  # 需要 root 权限运行
```

### examples/demo.c - 高级演示程序

**功能描述**：完整的 C++ 风格封装，提供易用的 API 接口。

**类设计**：
```cpp
class MemoryAccessor {
private:
    int sock;                    // Netlink socket 描述符
    struct sockaddr_nl addr;     // Netlink 地址结构
    
public:
    bool connect();              // 连接到内核模块
    bool read_memory();          // 读取虚拟内存
    bool write_memory();         // 写入虚拟内存  
    bool read_physical_memory(); // 读取物理内存
    bool write_physical_memory(); // 写入物理内存
};
```

**高级特性**：
- 面向对象封装
- 自动资源管理（RAII）
- 详细的错误报告
- 完整的使用示例

**编译命令**：
```bash
aarch64-linux-android-gcc examples/demo.c -o examples/demo
```

## API 接口详细说明

### 内核模块 API

#### 内存操作命令 (`mem_op_t`)

| 命令 | 值 | 描述 | 地址类型 | 权限要求 |
|------|-----|------|----------|----------|
| `MEM_READ` | 0 | 读取虚拟内存 | 虚拟地址 | CAP_SYS_ADMIN |
| `MEM_WRITE` | 1 | 写入虚拟内存 | 虚拟地址 | CAP_SYS_ADMIN |
| `PHYS_READ` | 2 | 读取物理内存 | 物理地址 | CAP_SYS_ADMIN |
| `PHYS_WRITE` | 3 | 写入物理内存 | 物理地址 | CAP_SYS_ADMIN |

#### 请求结构体 (`mem_request_t`)

| 字段 | 类型 | 说明 | 限制 |
|------|------|------|------|
| `op` | `mem_op_t` | 操作类型 | 必须是有效枚举值 |
| `addr` | `uint64_t` | 目标内存地址 | 64位地址空间 |
| `len` | `uint64_t` | 操作数据长度 | ≤ 1024 字节 |
| `data` | `uint8_t` | 数据缓冲区 | 输入/输出缓冲区 |

#### 返回值

| 返回值 | 含义 | 说明 |
|--------|------|------|
| `0` | 成功 | 操作完成，数据在 `data` 字段中 |
| `-EINVAL` | 无效参数 | 地址、长度或操作类型无效 |
| `-EFAULT` | 内存访问错误 | 虚拟地址不可访问 |
| `-ENOMEM` | 内存不足 | 物理内存映射失败 |
| `-EPERM` | 权限不足 | 调用者不是 root |

### 用户态 API (MemoryAccessor 类)

#### 构造函数/析构函数

- `MemoryAccessor()` - 默认构造函数
- `~MemoryAccessor()` - 自动关闭 socket 连接

#### 连接方法

- `bool connect()` 
  - 创建 Netlink Socket
  - 连接到内核模块 (协议号 30)
  - 返回连接成功状态

#### 内存操作方法

1. **虚拟内存读取**
   ```cpp
   bool read_memory(uint64_t addr, void *buffer, size_t len)
   ```
   - 参数：目标地址、输出缓冲区、读取长度
   - 返回：操作成功状态

2. **虚拟内存写入**
   ```cpp
   bool write_memory(uint64_t addr, const void *data, size_t len)
   ```
   - 参数：目标地址、输入数据、写入长度
   - 返回：操作成功状态

3. **物理内存读取**
   ```cpp
   bool read_physical_memory(uint64_t phys_addr, void *buffer, size_t len)
   ```
   - 参数：物理地址、输出缓冲区、读取长度
   - 返回：操作成功状态

4. **物理内存写入**
   ```cpp
   bool write_physical_memory(uint64_t phys_addr, const void *data, size_t len)
   ```
   - 参数：物理地址、输入数据、写入长度
   - 返回：操作成功状态

## 编译和部署指南

### 编译环境准备

**必需工具**：
- Android NDK (包含 aarch64-linux-android-gcc)
- Linux 内核源码 (Android 5.10)
- Make 构建工具

**环境变量设置**：
```bash
export CROSS_COMPILE=aarch64-linux-android-
export ARCH=arm64
export ANDROID_NDK_HOME=/path/to/android-ndk
```

### 编译步骤

#### 1. 编译内核模块
```bash
# 在 kernel_mem_module 目录下执行
make
```

**Makefile 详细说明**：
- `obj-m += mem_hook.o` - 定义模块目标
- `mem_hook-y := ...` - 指定模块源文件
- `KDIR := /lib/modules/$(shell uname -r)/build` - 内核构建目录
- 支持交叉编译 (`ARCH=arm64`, `CROSS_COMPILE`)

#### 2. 编译用户态程序
```bash
# 编译基础测试程序
aarch64-linux-android-gcc userland/tester.c -o userland/tester

# 编译高级演示程序  
aarch64-linux-android-gcc examples/demo.c -o examples/demo

# 或使用 Makefile 规则
make example
```

### 部署到 Android 设备

#### 1. 推送文件到设备
```bash
adb push mem_hook.ko /data/adb/
adb push userland/tester /data/adb/
adb push examples/demo /data/adb/
```

#### 2. 加载内核模块
```bash
# 进入设备 shell
adb shell

# 切换到 root
su

# 加载模块
insmod /data/adb/mem_hook.ko

# 验证模块加载
lsmod | grep mem_hook
```

#### 3. 运行测试程序
```bash
# 运行基础测试
/data/adb/userland/tester

# 运行高级演示
/data/adb/examples/demo
```

### 卸载模块
```bash
# 卸载内核模块
rmmod mem_hook

# 清理文件（可选）
rm /data/adb/mem_hook.ko
rm /data/adb/tester
rm /data/adb/demo
```

## 使用示例详解

### 示例 1: 基础内存读取

```cpp
MemoryAccessor accessor;
if (accessor.connect()) {
    uint64_t kernel_base = 0xffff000000000000;
    char buffer;
    
    if (accessor.read_memory(kernel_base, buffer, sizeof(buffer))) {
        printf("成功读取内核内存\n");
        // 处理读取的数据
    }
}
```

### 示例 2: 物理内存操作

```cpp
// 读取物理内存（如设备寄存器）
uint64_t device_register = 0x12345678;
uint32_t reg_value;
if (accessor.read_physical_memory(device_register, &reg_value, sizeof(reg_value))) {
    printf("设备寄存器值: 0x%08x\n", reg_value);
}

// 写入物理内存
uint32_t new_value = 0xdeadbeef;
accessor.write_physical_memory(device_register, &new_value, sizeof(new_value));
```

### 示例 3: 批量内存操作

```cpp
// 读取大块内存（分多次操作）
const size_t total_size = 4096;
const size_t chunk_size = 1024;
char *large_buffer = malloc(total_size);

for (size_t offset = 0; offset < total_size; offset += chunk_size) {
    size_t current_size = (offset + chunk_size <= total_size) ? chunk_size : (total_size - offset);
    if (!accessor.read_memory(target_addr + offset, large_buffer + offset, current_size)) {
        fprintf(stderr, "读取失败 at offset %zu\n", offset);
        break;
    }
}
```

## 安全性和隐蔽性分析

### 隐蔽性保障措施

| 检测维度 | 传统方案 | 本方案 | 优势 |
|----------|----------|--------|------|
| 文件系统 | /proc/modules, /sys/module | 无任何文件 | 完全隐藏 |
| 进程列表 | insmod/rmmod 进程 | 无相关进程 | 无法追踪 |
| 网络连接 | TCP/UDP socket | Netlink 协议 30 | 难以识别 |
| 日志输出 | dmesg 输出 | 无 printk 调用 | 无痕迹 |
| 内存特征 | 可识别的字符串 | 最小化字符串 | 难以扫描 |

### 安全访问控制

1. **能力检查**：
   ```c
   if (!capable(CAP_SYS_ADMIN)) {
       return; // 拒绝非 root 访问
   }
   ```

2. **协议隔离**：
   - 使用自定义 Netlink 协议号 (30)
   - 避免与标准协议冲突
   - 增加检测难度

3. **错误处理**：
   - 不泄露内部实现细节
   - 统一的错误返回格式
   - 防止信息泄露

### 潜在风险和缓解

| 风险 | 缓解措施 |
|------|----------|
| 系统崩溃 | 严格的地址有效性检查 |
| 安全漏洞 | 仅限 root 访问，无提权功能 |
| 检测发现 | 完全隐蔽设计，无文件系统暴露 |
| 资源泄漏 | 完善的资源管理和清理 |

## 故障排除

### 常见问题和解决方案

#### 1. 模块加载失败
**症状**：`insmod: failed to load module`
**原因**：内核版本不匹配或符号未导出
**解决方案**：
- 确认内核版本为 5.10
- 检查 `kallsyms_lookup_name` 是否可用
- 使用正确的交叉编译工具链

#### 2. 连接被拒绝
**症状**：`Connect failed: Permission denied`
**原因**：非 root 用户尝试连接
**解决方案**：
- 确保使用 `su` 切换到 root
- 验证设备已 root

#### 3. 内存操作失败
**症状**：`Memory read failed: -14 (EFAULT)`
**原因**：目标地址无效或不可访问
**解决方案**：
- 验证地址在有效范围内
- 检查内存页是否已分配
- 使用有效的内核地址

#### 4. 物理内存访问失败
**症状**：`Memory read failed: -12 (ENOMEM)`
**原因**：物理地址映射失败
**解决方案**：
- 确认物理地址有效
- 检查硬件内存布局
- 避免访问保留区域

### 调试技巧

1. **验证模块加载**：
   ```bash
   lsmod | grep mem_hook
   cat /proc/modules | grep mem_hook
   ```

2. **检查 Netlink 协议**：
   ```bash
   ss -nl | grep 30
   ```

3. **监控系统日志**：
   ```bash
   dmesg | tail -20
   ```

4. **验证权限**：
   ```bash
   id  # 确认 UID=0
   capsh --print  # 确认 CAP_SYS_ADMIN
   ```

## 免责声明

⚠️ **重要安全警告**

本软件提供无限制的内核内存访问能力，具有极高风险：

- **可能导致系统崩溃**：错误的内存操作会立即导致内核 panic
- **可能造成数据损坏**：写入关键内存区域会破坏系统稳定性  
- **安全漏洞风险**：不当使用可能被恶意利用
- **法律合规风险**：仅限在您拥有完全权限的设备上使用

**使用前提**：
- 仅用于合法的研究和学习目的
- 仅在您完全控制的测试设备上使用
- 充分了解内核内存布局和风险
- 遵守所有适用的法律和道德规范

**作者声明**：
本软件按"原样"提供，不提供任何形式的担保。作者不对因使用本软件造成的任何直接、间接、偶然、特殊或后果性损害负责。

---

*文档版本：2.0*  
*最后更新：2025年12月11日*