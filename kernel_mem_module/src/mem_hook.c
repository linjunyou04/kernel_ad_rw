#include "mem_hook.h"

static struct sock *nl_sk = NULL;

// 核心内存操作函数
int perform_memory_op(mem_request_t *req) {
    if (req->len > MAX_PAYLOAD) {
        return -EINVAL;
    }
    
    switch(req->op) {
        case MEM_READ:
            if (copy_to_user((void __user *)req->data, (void *)req->addr, req->len)) {
                return -EFAULT;
            }
            break;
        case MEM_WRITE:
            if (copy_from_user((void *)req->addr, (void __user *)req->data, req->len)) {
                return -EFAULT;
            }
            break;
        case PHYS_READ: {
            void *virt = ioremap(req->addr, req->len);
            if (!virt) {
                return -ENOMEM;
            }
            memcpy(req->data, virt, req->len);
            iounmap(virt);
            break;
        }
        case PHYS_WRITE: {
            void *virt = ioremap(req->addr, req->len);
            if (!virt) {
                return -ENOMEM;
            }
            memcpy(virt, req->data, req->len);
            iounmap(virt);
            break;
        }
    }
    return 0;
}

// Netlink接收处理
void nl_receive(struct sk_buff *skb) {
    struct nlmsghdr *nlh;
    mem_request_t *req;
    struct sk_buff *reply_skb;
    int ret;
    
    nlh = nlmsg_hdr(skb);
    req = (mem_request_t *)NLMSG_DATA(nlh);
    
    // 仅允许root操作
    if (!capable(CAP_SYS_ADMIN)) {
        return;
    }
    
    ret = perform_memory_op(req);
    
    // 构造回复
    reply_skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
    if (!reply_skb) {
        return;
    }
    
    nlh = nlmsg_put(reply_skb, 0, 0, NLMSG_DONE, sizeof(int), 0);
    *(int *)nlmsg_data(nlh) = ret;
    
    netlink_unicast(nl_sk, reply_skb, NETLINK_CB(skb).portid, MSG_DONTWAIT);
}

// 初始化netlink
int init_netlink(void) {
    struct netlink_kernel_cfg cfg = {
        .input = nl_receive,
    };
    nl_sk = netlink_kernel_create(&init_net, MEM_HOOK_NETLINK, &cfg);
    return nl_sk ? 0 : -ENOMEM;
}

// 清理netlink
void cleanup_netlink(void) {
    if (nl_sk) {
        netlink_kernel_release(nl_sk);
        nl_sk = NULL;
    }
}
