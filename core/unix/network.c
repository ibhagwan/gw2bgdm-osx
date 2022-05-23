#include "core/network.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static int g_sock = -1;
static struct sockaddr_in g_si;
static bool g_is_init;

int network_err()
{
    return errno;
}

bool network_create(i8 const* addr, u32 port)
{
    g_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_sock < 0)
    {
        return 0;
    }
    
    int val = 1;
    setsockopt(g_sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    
    if (addr) {
        
        ioctl(g_sock, FIONBIO, &val);
        g_si.sin_family = AF_INET;
        g_si.sin_port = htons(port);
        inet_pton(g_si.sin_family, addr, &g_si.sin_addr);
        
    } else {
    
        struct sockaddr_in si = { 0 };
        si.sin_family = AF_INET;
        si.sin_addr.s_addr = htonl(INADDR_ANY);
        si.sin_port = htons((u16)port);
    
        if (bind(g_sock, (struct sockaddr*)&si, sizeof(si)) < 0)
        {
            return 0;
        }
    }

    g_is_init = true;
    return true;
}

void network_destroy(void)
{
    if (g_sock >= 0)
    {
        close(g_sock);
    }
    g_is_init = false;
}

i32 network_recv(void* dst, u32 dst_bytes, u32* addr, u32* port)
{
    if (g_is_init == false)
    {
        return 0;
    }
    
    size_t len;
    if (addr && port) {
        struct sockaddr_in si = { 0 };
        socklen_t sil = sizeof(si);
        len = recvfrom(g_sock, dst, dst_bytes, 0, (struct sockaddr*)&si, &sil);
        if (addr) *addr = si.sin_addr.s_addr;
        if (port) *port = si.sin_port;
    }
    else {
        socklen_t sil = sizeof(g_si);
        len = recvfrom(g_sock, dst, dst_bytes, 0, (struct sockaddr*)&g_si, &sil);
    }
	
    return (i32)len;
}

i32 network_send(void const* src, u32 src_bytes, u32 addr, u32 port)
{
    if (g_is_init == false)
    {
        return 0;
    }
    
    size_t len;
    
    if (addr && port) {
        struct sockaddr_in si = { 0 };
        si.sin_family = AF_INET;
        si.sin_addr.s_addr = addr;
        si.sin_port = (u16)port;
        len = sendto(g_sock, src, src_bytes, 0, (struct sockaddr*)&si, sizeof(si));
    }
    else {
        len = sendto(g_sock, src, src_bytes, 0, (struct sockaddr*)&g_si, sizeof(g_si));
    }
    
    return (i32)len;
}

// Convert ip to string
i8 const* network_addr_to_str(u32 addr, i8* str_addr)
{
    if (!str_addr)
        return NULL;
    
    memset(str_addr, 0, INET_ADDRSTRLEN);
    
    struct sockaddr_in si;
    si.sin_family = AF_INET;
    memcpy(&si.sin_addr, &addr, sizeof(struct in_addr));
    return inet_ntop(AF_INET, &(si.sin_addr), str_addr, INET_ADDRSTRLEN);
    
    //wsprintfA(str_addr,
    //	"%d.%d.%d.%d",
    //	(addr & 0xFF),
    //	(addr >> 8) & 0xFF,
    //	(addr >> 16) & 0xFF,
    //	(addr >> 24) & 0xFF
    //);
}

