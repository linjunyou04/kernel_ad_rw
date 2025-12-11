#include <linux/module.h>
#include <linux/kernel.h>
#include "mem_hook.h"
#include "hook_impl.h"

// 全局hook变量
static hook_t *mem_hook = NULL;

// 模块初始化
static int __init mem_hook_init(void) {
    // 初始化hook系统
    mem_hook = hook_mem_zalloc();
    if (!mem_hook) {
        return -ENOMEM;
    }
    
    // 创建隐蔽通信通道
    if (init_netlink()) {
        hook_mem_free(mem_hook);
        return -1;
    }
    
    return 0;
}

// 模块退出
static void __exit mem_hook_exit(void) {
    cleanup_netlink();
    if (mem_hook) {
        hook_mem_free(mem_hook);
        mem_hook = NULL;
    }
}

module_init(mem_hook_init);
module_exit(mem_hook_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("weiwen");
MODULE_DESCRIPTION("---");
