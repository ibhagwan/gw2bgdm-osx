#pragma once
#if defined(_WIN32) || defined (_WIN64)
#include <windows.h>
#endif
#include "core/message.h"
#include "core/types.h"
#include "core/input.h"
#include "meter/game.h"
#include "meter/config.h"
#include "meter/logging.h"


// Max sortable columns in a grid panel
#define MAX_COL 64

// Max no. of tab in a panel
#define MAX_TABS 6

// Max no. of panels
#define MAX_PANELS 20


#define COL_ASC_STR	">"
#define COL_DESC_STR "<"


enum VersionRows {
	VER_ROW_VER,
	VER_ROW_SIG,
	VER_ROW_PING,
	VER_ROW_FPS,
	VER_ROW_PROC,
	VER_ROW_SRV_TEXT,
	VER_ROW_SRV_TIME,
	VER_ROW_END
};

enum HPRows {
	HP_ROW_PLAYER_HP,
	HP_ROW_TARGET_HP,
	HP_ROW_TARGET_DIST,
	HP_ROW_TARGET_BB,
	HP_ROW_PORT_DIST,
	HP_ROW_PORT_DIST_BG,
	HP_ROW_END
};

enum TargetDpsColumns {
	TDPS_COL_DPS,
	TDPS_COL_DMG,
	TDPS_COL_TTK,
	TDPS_COL_GRAPH,
	TDPS_COL_SPECID,
	TDPS_COL_END
};

enum GroupDpsColumns {
	GDPS_COL_NAME,
	GDPS_COL_CLS,
	GDPS_COL_DPS,
	GDPS_COL_PER,
	GDPS_COL_DMGOUT,
	GDPS_COL_DMGIN,
	GDPS_COL_HPS,
	GDPS_COL_HEAL,
	GDPS_COL_TIME,
	GDPS_COL_END,
	GDPS_COL_GRAPH,
	GDPS_COL_END2
};

enum BuffUptimeColumns {
	BUFF_COL_NAME,
	BUFF_COL_CLS,
	BUFF_COL_SUB,
	BUFF_COL_DOWN,
	BUFF_COL_SCLR,
	BUFF_COL_SEAW,
	BUFF_COL_PROT,
	BUFF_COL_QUIK,
	BUFF_COL_ALAC,
	BUFF_COL_FURY,
	BUFF_COL_MIGHT,
	BUFF_COL_VIGOR,
	BUFF_COL_SWIFT,
	BUFF_COL_STAB,
	BUFF_COL_RETAL,
	BUFF_COL_RESIST,
	BUFF_COL_REGEN,
	BUFF_COL_AEGIS,
	BUFF_COL_GOTL,
	BUFF_COL_GLEM,
	BUFF_COL_BANS,
	BUFF_COL_BAND,
	BUFF_COL_BANT,
	BUFF_COL_EA,
	BUFF_COL_SPOTTER,
	BUFF_COL_FROST,
	BUFF_COL_SUN,
	BUFF_COL_STORM,
	BUFF_COL_STONE,
	BUFF_COL_ENGPP,
	BUFF_COL_REVNR,
	BUFF_COL_REVAP,
	BUFF_COL_REVRD,
	BUFF_COL_ELESM,
	BUFF_COL_GRDSN,
	BUFF_COL_NECVA,
	BUFF_COL_TIME,
	BUFF_COL_END
};

typedef struct {
	i8 *str;
	bool is_expanded;
	bool col_visible[MAX_COL];
	const char** col_str;
	i32 maxCol;
	i32 sortCol;
	i32 sortColLast;
	i32 asc;
    u32 tabNo;
	bool useLocalizedText;
	bool useProfColoring;
	bool lineNumbering;
} PanelConfig;

typedef struct Panel {
	i8 section[64];
	KeyBind bind;
	u32	enabled;
	bool init;
	bool tinyFont;
	bool autoResize;
	int mode;
	int tmp_fl;
	int tmp_fr;
	float fAlpha;
	PanelConfig cfg;
} Panel;

typedef struct State
{
	// Key bindings
	KeyBind bind_screenshot;
	KeyBind bind_on_off;
	KeyBind bind_input;
	KeyBind bind_minPanels;
	KeyBind bind_dps_lock;
	KeyBind bind_dps_reset;

	// Panels
	Panel panel_hp;
	Panel panel_float;
	Panel panel_compass;
	Panel panel_dps_self;
	Panel panel_dps_group;
	Panel panel_buff_uptime;
	Panel panel_gear_self;
	Panel panel_gear_target;
	Panel panel_skills;
	Panel panel_debug;
	Panel panel_version;
	Panel *panel_arr[MAX_PANELS];

	// config
	bool is_gw2china;
	bool global_on_off;
	bool global_cap_input;
	bool show_metrics;
	bool show_metrics_bgdm;
	bool show_server;
	bool autoUpdate_disable;
	bool netDps_disable;
    bool hpCommas_disable;
	bool minPanels_enable;
	bool priortizeSquad;
	bool hideNonSquad;
	bool hideNonParty;
	bool profColor_disable;
	bool use_localized_text;
	bool use_seaweed_icon;
	bool use_downed_enemy_icon;
	i32 icon_pack_no;
	i32 target_retention_time;
	i32 ooc_grace_period;
    i32 max_players;
	char lang_dir[1024];
	char lang_file[1024];

	// server
	i8* network_addr;
	u16 network_port;

} State;


#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined (_WIN64)
// Our module handle
extern HANDLE g_hInstance;

// game window, acquired on load
extern HWND g_hWnd;
#endif

// global mod state
extern State g_state;

// Mumble data
extern LinkedMem* g_mum_mem;
extern u32 g_buildId;

// Controlled player
//extern Player g_player;

extern const char* gdps_col_str[];
extern const char* buff_col_str[];

// Array forward-def
struct Array;
struct Dict;
struct BuffStacks;

// Mumble Link
void mumble_link_create(void);
void mumble_link_destroy(void);

// Read the .ini configuration options
void config_get_state(void);

// Creates the DPS meter.
void dps_create(void);

// Destroys the DPS meter.
void dps_destroy(void);

// Handles damage messages from the server.
void dps_handle_msg_damage_utf8(MsgServerDamageUTF8* msg);

// Updates the DPS meter.
void dps_update(i64 now);

// Update the camera data
bool dps_update_cam_data();

// Draw version info
void dps_version(f32 time);

// get the client ctx
u64 dps_get_client_context();

//  get the agent view ctx
u64 dps_get_av_context();

// Reset the DPS targets
void dps_targets_reset();

// Updte targets SpeciesDef
void dps_targets_get_species();

// Get a DPS target
DPSTarget* dps_targets_get(u32 i);

// Insert a DPS target
u32 dps_targets_insert(u64 target, u64 cptr, struct Array* ca);

// Get last known target
struct DPSTargetEx* dps_target_get_lastknown(i64 now);

// Get the latest timestap of dps_update
i64 dps_get_now();

#ifdef __cplusplus
}
#endif
