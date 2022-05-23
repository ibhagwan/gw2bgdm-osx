#pragma once
#include <wchar.h>
#include "core/types.h"

// UDP max packet size + 1
#define UDP_MAX_PKT_SIZE 65536

// The closest number of players to accept.
#define MSG_CLOSEST_PLAYERS 9

// Name length restrictions.
#define MSG_NAME_LAST 19
#define MSG_NAME_SIZE 20

#define MSG_NAME_SIZE_UTF8 MSG_NAME_SIZE*4

// Cryptographic signature size.
#define MSG_SIG_SIZE 512

// Crypto key size
#define MSG_KEY_SIZE 1024

// Update piece size
#define MSG_UPDATE_SIZE 8192

// Update piece max size without fragmentation
// RFC 791: max UDP packet that ensure no fragmentation is 576
#define MSG_UPDATE_SIZE_NO_FRAG 548

// Maximum number of targets per client.
#define MSG_TARGETS 16

// The network information.
#define MSG_NETWORK_ADDR_AMZN	"54.144.142.153"	// Amazon
#define MSG_NETWORK_ADDR_NFO	"66.150.188.80"		// nfoserver.com
#ifdef _DEBUG
#define MSG_NETWORK_ADDR "127.0.0.1"
#define MSG_NETWORK_PORT 33322
#else
#define MSG_NETWORK_ADDR "0.0.0.0"  // MSG_NETWORK_ADDR_NFO
#define MSG_NETWORK_PORT 0          // 33322
#endif

// Client message types.
#define MSG_TYPE_CLIENT_DAMAGE 808
#define MSG_TYPE_CLIENT_UPDATE_BEGIN 809
#define MSG_TYPE_CLIENT_UPDATE_PIECE 810
// v2
#define MSG_TYPE_CLIENT_DAMAGE_V2 811
#define MSG_TYPE_CLIENT_UPDATE_BEGIN_V2 812
// wchar_t version
#define MSG_TYPE_CLIENT_DAMAGE_W 813
#define MSG_TYPE_CLIENT_UPDATE_BEGIN_W 814
#define MSG_TYPE_CLIENT_UPDATE_BEGIN_W_V2 815
// utf8 version
#define MSG_TYPE_CLIENT_DAMAGE_UTF8 816
#define MSG_TYPE_CLIENT_UPDATE_BEGIN_UTF8 817

// Server message types.
#define MSG_TYPE_SERVER_DAMAGE 808
#define MSG_TYPE_SERVER_UPDATE_BEGIN 809
#define MSG_TYPE_SERVER_UPDATE_PIECE 810
// v2
#define MSG_TYPE_SERVER_DAMAGE_V2 811
// wchar_t version
#define MSG_TYPE_SERVER_DAMAGE_W 813
// utf8 version
#define MSG_TYPE_SERVER_DAMAGE_UTF8 816

// Message rate limiting.
#define MSG_FREQ_CLIENT_PING 1000000
#define MSG_FREQ_CLIENT_DAMAGE 500000
#define MSG_FREQ_CLIENT_UPDATE_BEGIN 30000000
#define MSG_FREQ_CLIENT_UPDATE_PIECE 500000

// Client stats.
typedef struct ClientStats
{
    u16 pow;
    u16 pre;
    u16 tuf;
    u16 vit;
    u16 fer;
    u16 hlp;
    u16 cnd;
    u16 con;
    u16 exp;
} ClientStats;

// Client player data.
typedef struct ClientPlayer
{
    i8 name[MSG_NAME_SIZE];
} ClientPlayer;

typedef struct __ClientPlayerW
{
    wchar_t name[MSG_NAME_SIZE];
} ClientPlayerW;

typedef struct __ClientPlayerUTF8
{
    char name[MSG_NAME_SIZE_UTF8];
} ClientPlayerUTF8;

// Client target data.
typedef struct ClientTarget
{
    u32 time;
    u32 id;
    u32 tdmg;
    u32 invuln;
    u32 reserved1;
} ClientTarget;

// Server player data.
typedef struct ServerPlayer
{
    bool in_combat;
    f32 time;
    u32 damage;
    u32 profession;
    ClientStats stats;
    ClientTarget targets[MSG_TARGETS];
    i8 name[MSG_NAME_SIZE];
} ServerPlayer;

typedef struct ServerPlayerV2
{
    bool in_combat;
    f32 time;
    u32 damage;
    u32 damage_in;
    u32 heal_out;
    u32 profession;
    ClientStats stats;
    ClientTarget targets[MSG_TARGETS];
    i8 name[MSG_NAME_SIZE];
} ServerPlayerV2;

typedef struct __ServerPlayerW
{
    bool in_combat;
    f32 time;
    u32 damage;
    u32 damage_in;
    u32 heal_out;
    u32 profession;
    ClientStats stats;
    ClientTarget targets[MSG_TARGETS];
    wchar_t name[MSG_NAME_SIZE];
} ServerPlayerW;

typedef struct __ServerPlayerUTF8
{
    bool in_combat;
    f32 time;
    u32 damage;
    u32 damage_in;
    u32 heal_out;
    u32 profession;
    ClientStats stats;
    ClientTarget targets[MSG_TARGETS];
    char name[MSG_NAME_SIZE_UTF8];
} ServerPlayerUTF8;

// Client damage request message.
typedef struct MsgClientDamage
{
    u32 msg_type;
    u32 selected_id;
    u32 shard_id;
    bool in_combat;
    f32 time;
    u32 damage;
    u32 profession;
    ClientStats stats;
    ClientTarget targets[MSG_TARGETS];
    ClientPlayer closest[MSG_CLOSEST_PLAYERS];
    i8 name[MSG_NAME_SIZE];
} MsgClientDamage;

typedef struct MsgClientDamageV2
{
    u32 msg_type;
    u32 selected_id;
    u32 shard_id;
    bool in_combat;
    f32 time;
    u32 damage;
    u32 profession;
    u32 damage_in;
    u32 heal_out;
    ClientStats stats;
    ClientTarget targets[MSG_TARGETS];
    ClientPlayer closest[MSG_CLOSEST_PLAYERS];
    i8 name[MSG_NAME_SIZE];
} MsgClientDamageV2;

typedef struct __MsgClientDamageW
{
    u32 msg_type;
    u32 selected_id;
    u32 shard_id;
    bool in_combat;
    f32 time;
    u32 damage;
    u32 profession;
    u32 damage_in;
    u32 heal_out;
    ClientStats stats;
    ClientTarget targets[MSG_TARGETS];
    ClientPlayerW closest[MSG_CLOSEST_PLAYERS];
    wchar_t name[MSG_NAME_SIZE];
} MsgClientDamageW;

typedef struct __MsgClientDamageUTF8
{
    u32 msg_type;
    u32 selected_id;
    u32 shard_id;
    bool in_combat;
    f32 time;
    u32 damage;
    u32 profession;
    u32 damage_in;
    u32 heal_out;
    ClientStats stats;
    ClientTarget targets[MSG_TARGETS];
    ClientPlayerUTF8 closest[MSG_CLOSEST_PLAYERS];
    char name[MSG_NAME_SIZE_UTF8];
} MsgClientDamageUTF8;

// Client update request message.
typedef struct MsgClientUpdateBegin
{
    u32 msg_type;
    u32 data_version;
    u64 sequence;
} MsgClientUpdateBegin;

typedef struct MsgClientUpdateBeginV2
{
    u32 msg_type;
    u32 data_version;
    u64 sequence;
    i8 charName[MSG_NAME_SIZE * 2];
    i8 acctName[MSG_NAME_SIZE * 2];
} MsgClientUpdateBeginV2;

typedef struct __MsgClientUpdateBeginW
{
    u32 msg_type;
    u32 data_version;
    u64 sequence;
    wchar_t charName[MSG_NAME_SIZE * 2];
    wchar_t acctName[MSG_NAME_SIZE * 2];
} MsgClientUpdateBeginW;

typedef struct __MsgClientUpdateBeginW2
{
    u32 msg_type;
    u32 data_version;
    u64 sequence;
    wchar_t charName[MSG_NAME_SIZE * 2];
    wchar_t acctName[MSG_NAME_SIZE * 2];
    char version[32];
} MsgClientUpdateBeginW2;

typedef struct __MsgClientUpdateBeginUTF8
{
    u32 msg_type;
    u32 data_version;
    u64 sequence;
    char charName[MSG_NAME_SIZE_UTF8 * 2];
    char acctName[MSG_NAME_SIZE_UTF8 * 2];
    char version[32];
} MsgClientUpdateBeginUTF8;

// Client update request message.
typedef struct MsgClientUpdatePiece
{
    u32 msg_type;
    u32 data_version;
    u64 sequence;
    u32 data_size;
    u32 data_offset;
} MsgClientUpdatePiece;

// Server damage response message.
typedef struct MsgServerDamage
{
    u32 msg_type;
    u32 selected_id;
    ServerPlayer closest[MSG_CLOSEST_PLAYERS];
} MsgServerDamage;

typedef struct MsgServerDamageV2
{
    u32 msg_type;
    u32 selected_id;
    ServerPlayerV2 closest[MSG_CLOSEST_PLAYERS];
} MsgServerDamageV2;

typedef struct __MsgServerDamageW
{
    u32 msg_type;
    u32 selected_id;
    ServerPlayerW closest[MSG_CLOSEST_PLAYERS];
} MsgServerDamageW;

typedef struct __MsgServerDamageUTF8
{
    u32 msg_type;
    u32 selected_id;
    ServerPlayerUTF8 closest[MSG_CLOSEST_PLAYERS];
} MsgServerDamageUTF8;

// Server update response message.
typedef struct MsgServerUpdateBegin
{
    u32 msg_type;
    u32 data_version;
    u64 sequence;
    u32 data_size;
    u32 pubkey_len;
    u8 sig[MSG_SIG_SIZE];
    u8 pubkey_data[MSG_KEY_SIZE];
} MsgServerUpdateBegin;

// Server update piece response.
typedef struct MsgServerUpdatePiece
{
    u32 msg_type;
    u32 data_version;
    u64 sequence;
    u32 data_size;
    u32 data_offset;
    u8 data_buffer[MSG_UPDATE_SIZE];
} MsgServerUpdatePiece;


typedef struct MsgServerUpdatePieceNoFrag
{
    u32 msg_type;
    u32 data_version;
    u64 sequence;
    u32 data_size;
    u32 data_offset;
    u8 data_buffer[MSG_UPDATE_SIZE_NO_FRAG];
} MsgServerUpdatePieceNoFrag;

