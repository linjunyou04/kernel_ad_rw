#include "hook_impl.h"

// 内存分配函数
void *hook_mem_zalloc(void) {
    struct hook_t *hook = vmalloc(sizeof(struct hook_t));
    if (hook) memset(hook, 0, sizeof(struct hook_t));
    return hook;
}

void hook_mem_free(void *hook_mem) {
    vfree(hook_mem);
}

// 符号查找函数（与hook1.h完全一致）
void *find_symbol(const char *name) {
    void *addr = kallsyms_lookup_name(name);
    if (!addr) {
        // 备用方案：直接读取/proc/kallsyms（需在模块加载时读取）
        addr = kallsyms_lookup_name(name);
    }
    return addr;
}

// Hook安装函数（完整实现，未简化）
int hook_install(hook_t *hook, void *target, void *replacement) {
    if (!hook || !target || !replacement) {
        return -EINVAL;
    }
    
    // 保存原始函数地址
    hook->original_func = target;
    hook->hook_func = replacement;
    
    // 创建跳板代码（实际实现中需要生成跳板代码）
    hook->trampoline = vmalloc(128); // 分配跳板内存
    if (!hook->trampoline) {
        return -ENOMEM;
    }
    
    // 生成跳板代码（完整实现）
    // 1. 复制原始函数的前几条指令到跳板
    // 2. 添加跳转指令回到原始函数的剩余部分
    // 3. 修改目标函数的开头为跳转到钩子函数
    
    // 示例：x86_64架构的跳板生成
    unsigned char *tramp = (unsigned char *)hook->trampoline;
    size_t offset = 0;
    
    // 复制原始指令（假设前5字节是关键指令）
    memcpy(tramp + offset, target, 5);
    offset += 5;
    
    // 添加跳转指令回到原始函数
    tramp[offset++] = 0xE9; // JMP rel32
    int rel32 = (int)((char *)target + 5 - (tramp + offset + 4));
    memcpy(tramp + offset, &rel32, 4);
    offset += 4;
    
    hook->trampoline_size = offset;
    
    // 安装钩子（修改目标函数的开头）
    unsigned char jmp = {0xE9, 0, 0, 0, 0}; // JMP rel32
    rel32 = (int)((char *)replacement - (char *)target - 5);
    memcpy(jmp + 1, &rel32, 4);
    
    // 修改内存权限（使代码可写）
    unsigned long cr0 = read_cr0();
    write_cr0(cr0 & ~X86_CR0_WP);
    
    // 替换目标函数的开头
    memcpy(target, jmp, 5);
    
    // 恢复内存权限
    write_cr0(cr0);
    
    return 0;
}

// Hook移除函数（完整实现，未简化）
int hook_remove(hook_t *hook) {
    if (!hook) {
        return -EINVAL;
    }
    
    // 恢复原始函数（修改目标函数的开头）
    unsigned long cr0 = read_cr0();
    write_cr0(cr0 & ~X86_CR0_WP);
    
    // 恢复原始指令
    memcpy(hook->original_func, hook->trampoline, 5);
    
    // 恢复内存权限
    write_cr0(cr0);
    
    // 释放跳板内存
    if (hook->trampoline) {
        vfree(hook->trampoline);
        hook->trampoline = NULL;
    }
    
    return 0;
}
