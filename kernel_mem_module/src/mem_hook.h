#ifndef MEM_HOOK_H
#define MEM_HOOK_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netlink.h>
#include <net/sock.h>
#include <linux/capability.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include "hook_impl.h"

#define MEM_HOOK_NETLINK 30
#define MAX_PAYLOAD 1024

// 内存操作命令
typedef enum {
    MEM_READ,
    MEM_WRITE,
    PHYS_READ,
    PHYS_WRITE
} mem_op_t;

typedef struct {
    mem_op_t op;
    uint64_t addr;
    uint64_t len;
    uint8_t data[MAX_PAYLOAD];
} mem_request_t;

// Netlink相关函数
int init_netlink(void);
void cleanup_netlink(void);
void nl_receive(struct sk_buff *skb);

// 内存操作函数
int perform_memory_op(mem_request_t *req);

#endif // MEM_HOOK_H
