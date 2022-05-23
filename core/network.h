#pragma once
#include "core/types.h"

// Returns the last network error WSAGetLastError
int network_err();

// Creates the networking interface.
bool network_create(i8 const* addr, u32 port);

// Destroys the networking interface.
void network_destroy(void);

// Receives a packet from the network. Retunrns the length of the packet receieved.
i32 network_recv(void* dst, u32 dst_bytes, u32* addr, u32* port);

// Sends a packet.
i32 network_send(void const * src, u32 src_bytes, u32 addr, u32 port);

// Convert ip to string
i8 const* network_addr_to_str(u32 addr, i8* str_addr);
