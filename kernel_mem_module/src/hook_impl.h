#ifndef HOOK_IMPL_H
#define HOOK_IMPL_H

#include <linux/types.h>
#include <linux/kallsyms.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/string.h>

// Hook结构体（与hook1.h完全一致）
typedef struct {
    void *original_func;   // 原始函数地址
    void *hook_func;       // 钩子函数地址
    void *trampoline;      // 跳板代码地址
    size_t trampoline_size; // 跳板代码大小
} hook_t;

// 内存分配函数
void *hook_mem_zalloc(void);
void hook_mem_free(void *hook_mem);

// 符号查找函数
void *find_symbol(const char *name);

// Hook操作函数
int hook_install(hook_t *hook, void *target, void *replacement);
int hook_remove(hook_t *hook);

#endif // HOOK_IMPL_H
