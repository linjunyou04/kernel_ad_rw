#include <sys/socket.h>
#include <linux/netlink.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// 与内核模块定义一致的结构体
typedef enum {
    MEM_READ = 0,
    MEM_WRITE = 1,
    PHYS_READ = 2,
    PHYS_WRITE = 3
} mem_op_t;

typedef struct {
    mem_op_t op;
    uint64_t addr;
    uint64_t len;
    uint8_t data[1024];
} mem_request_t;

// Netlink通信类
class MemoryAccessor {
private:
    int sock;
    struct sockaddr_nl addr;
    
public:
    MemoryAccessor() : sock(-1) {}
    
    ~MemoryAccessor() {
        if (sock >= 0) {
            close(sock);
        }
    }
    
    // 连接内核模块
    bool connect() {
        sock = socket(PF_NETLINK, SOCK_RAW, 30);
        if (sock < 0) {
            perror("创建socket失败");
            return false;
        }
        
        memset(&addr, 0, sizeof(addr));
        addr.nl_family = AF_NETLINK;
        addr.nl_pid = 0;  // 目标为内核
        addr.nl_groups = 0;
        
        if (::connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("连接失败，需要root权限");
            close(sock);
            sock = -1;
            return false;
        }
        
        printf("成功连接到内核模块\n");
        return true;
    }
    
    // 发送请求并接收响应
    int send_request(mem_request_t *req) {
        struct nlmsghdr hdr = {
            .nlmsg_len = NLMSG_LENGTH(sizeof(*req)),
            .nlmsg_type = 0,
            .nlmsg_flags = 0,
            .nlmsg_seq = 0,
            .nlmsg_pid = 0
        };
        
        // 发送请求头
        if (send(sock, &hdr, sizeof(hdr), 0) < 0) {
            perror("发送请求头失败");
            return -1;
        }
        
        // 发送请求数据
        if (send(sock, req, sizeof(*req), 0) < 0) {
            perror("发送请求数据失败");
            return -1;
        }
        
        // 接收响应
        char buffer[1024];
        ssize_t recv_len = recv(sock, buffer, sizeof(buffer), 0);
        if (recv_len < 0) {
            perror("接收响应失败");
            return -1;
        }
        
        return *(int *)buffer;
    }
    
    // 读取虚拟内存
    bool read_memory(uint64_t addr, void *buffer, size_t len) {
        if (len > 1024) {
            fprintf(stderr, "读取长度过大\n");
            return false;
        }
        
        mem_request_t req = {0};
        req.op = MEM_READ;
        req.addr = addr;
        req.len = len;
        
        int result = send_request(&req);
        if (result == 0) {
            memcpy(buffer, req.data, len);
            return true;
        }
        
        return false;
    }
    
    // 写入虚拟内存
    bool write_memory(uint64_t addr, const void *data, size_t len) {
        if (len > 1024) {
            fprintf(stderr, "写入长度过大\n");
            return false;
        }
        
        mem_request_t req = {0};
        req.op = MEM_WRITE;
        req.addr = addr;
        req.len = len;
        memcpy(req.data, data, len);
        
        return send_request(&req) == 0;
    }
    
    // 读取物理内存
    bool read_physical_memory(uint64_t phys_addr, void *buffer, size_t len) {
        if (len > 1024) {
            fprintf(stderr, "读取长度过大\n");
            return false;
        }
        
        mem_request_t req = {0};
        req.op = PHYS_READ;
        req.addr = phys_addr;
        req.len = len;
        
        int result = send_request(&req);
        if (result == 0) {
            memcpy(buffer, req.data, len);
            return true;
        }
        
        return false;
    }
    
    // 写入物理内存
    bool write_physical_memory(uint64_t phys_addr, const void *data, size_t len) {
        if (len > 1024) {
            fprintf(stderr, "写入长度过大\n");
            return false;
        }
        
        mem_request_t req = {0};
        req.op = PHYS_WRITE;
        req.addr = phys_addr;
        req.len = len;
        memcpy(req.data, data, len);
        
        return send_request(&req) == 0;
    }
};

// 使用示例
int main() {
    MemoryAccessor accessor;
    
    // 连接到内核模块
    if (!accessor.connect()) {
        fprintf(stderr, "无法连接到内核模块，请确保已加载模块且具有root权限\n");
        return 1;
    }
    
    // 示例1: 读取内存
    printf("\n=== 示例1: 读取内存 ===\n");
    uint64_t test_addr = 0xffff000000000000; // 测试地址
    char read_buffer[64] = {0};
    
    if (accessor.read_memory(test_addr, read_buffer, sizeof(read_buffer))) {
        printf("成功读取 %zu 字节:\n", sizeof(read_buffer));
        for (int i = 0; i < 16 && i < sizeof(read_buffer); i++) {
            printf("%02x ", (unsigned char)read_buffer[i]);
        }
        printf("...\n");
    } else {
        printf("读取内存失败\n");
    }
    
    // 示例2: 写入内存
    printf("\n=== 示例2: 写入内存 ===\n");
    char write_data[] = "Hello Kernel!";
    if (accessor.write_memory(test_addr, write_data, strlen(write_data))) {
        printf("成功写入 %zu 字节: %s\n", strlen(write_data), write_data);
    } else {
        printf("写入内存失败\n");
    }
    
    // 示例3: 读取物理内存
    printf("\n=== 示例3: 读取物理内存 ===\n");
    uint64_t phys_addr = 0x1000000; // 物理地址示例
    if (accessor.read_physical_memory(phys_addr, read_buffer, 16)) {
        printf("成功读取物理内存:\n");
        for (int i = 0; i < 16; i++) {
            printf("%02x ", (unsigned char)read_buffer[i]);
        }
        printf("\n");
    } else {
        printf("读取物理内存失败（可能需要更高权限）\n");
    }
    
    printf("\n演示完成\n");
    return 0;
}
