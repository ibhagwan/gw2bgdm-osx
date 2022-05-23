//
//  config_osx.m
//  bgdm
//
//  Created by Bhagwan on 7/5/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Defines.h"
#import "core/file.h"
#import "meter/dps.h"
#import "meter/config.h"
#import "meter/game_data.h"
#import "meter/imgui_bgdm.h"
#import "osx.bund/imgui_osx.h"

static const char* ver_row_str[] = {
    "VER",
    "SIG",
    "PING",
    "FPS",
    "PROC",
    "SRV_TEXT",
    "SRV_TIME",
};

static const char* hp_row_str[] = {
    "PLAYER_HP",
    "TARGET_HP",
    "TARGET_DIST",
    "TARGET_BB",
    "PORT_DIST",
    "PORT_DIST_BG",
};

static const char* tdps_col_str[] = {
    "DPS",
    "DMG",
    "TTK",
    "GRAPH",
    "SPEC_ID",
};

const char* gdps_col_str[] = {
    "NAME",
    "CLS",
    "DPS",
    "PER",
    "DMGOUT",
    "DMGIN",
    "HPS",
    "HEAL",
    "TIME",
    "END",
    "GRAPH",
};


const char* buff_col_str[] = {
    "NAME",
    "CLS",
    "SUB",
    "DWN",
    "SCL",
    "SWD",
    "PRT",
    "QCK",
    "ALC",
    "FRY",
    "MGT",
    "VIG",
    "SWI",
    "STB",
    "RTL",
    "RES",
    "REG",
    "AEG",
    "GTL",
    "GLE",
    "STR",
    "DCP",
    "TAC",
    "WEA",
    "SPO",
    "FRO",
    "SUN",
    "STM",
    "STN",
    "PPD",
    "RNR",
    "RAP",
    "RRD",
    "ESM",
    "GSN",
    "NVA",
    "TIME",
};


static void config_parse_columns(Panel* panel, const char **cols, int startIdx, int endIdx)
{
    panel->cfg.col_str = cols;
    
    if (panel->cfg.str && strlen(panel->cfg.str) > 0) {
        
        i8* token = strtok (panel->cfg.str, "|");
        while (token != NULL)
        {
            for (i32 i = startIdx; i < endIdx; ++i) {
                if (strcasecmp(token, panel->cfg.col_str[i]) == 0)
                {
                    panel->cfg.col_visible[i] = 1;
                }
            }
            
            token = strtok (NULL, "|");
        }
    }
}

static inline void panelToggle(Panel* panel)
{
    if (panel->cfg.tabNo) {
        if (!panel->enabled) {
            panel->enabled = 1;
        }
        else if (++panel->mode >= panel->cfg.tabNo) {
            panel->enabled = 0;
            panel->mode = 0;
        }
        config_set_int(panel->section, "Mode", panel->mode);
        
    } else {
        panel->enabled ^= 1;
    }
    
    if (panel->enabled > 1)
        panel->enabled = 1;
    config_set_int(panel->section, "Enabled", panel->enabled);
}

void keybCallback_globalOnOff(int Key, bool Control, bool Alt, bool Shift)
{
    g_state.global_on_off ^= 1;
    config_set_int(CONFIG_SECTION_GLOBAL, "Enabled", g_state.global_on_off);
}

void keybCallback_inputCapOnOff(int Key, bool Control, bool Alt, bool Shift)
{
    ImGui_ToggleInput();
}

void keybCallback_uiMinPanels(int Key, bool Control, bool Alt, bool Shift)
{
    //CLogDebug(@"keybCallback_uiMinPanels");
    ImGui_ToggleMinPanels();
}

void keybCallback_screenshot(int Key, bool Control, bool Alt, bool Shift)
{
}

void keybCallback_targetLock(int Key, bool Control, bool Alt, bool Shift)
{
    dpsLockTarget();
}

void keybCallback_dpsReset(int Key, bool Control, bool Alt, bool Shift)
{
    dpsReset();
}

void keybCallback_panelDebug(int Key, bool Control, bool Alt, bool Shift)
{
#if (defined _DEBUG) || (defined DEBUG)
    ImGui_ToggleDebug();
#endif
}

void keybCallback_panelMain(int Key, bool Control, bool Alt, bool Shift)
{
    panelToggle(&g_state.panel_version);
}

void keybCallback_panelHp(int Key, bool Control, bool Alt, bool Shift)
{
    panelToggle(&g_state.panel_hp);
}

void keybCallback_panelCompass(int Key, bool Control, bool Alt, bool Shift)
{
    panelToggle(&g_state.panel_compass);
}

void keybCallback_panelDpsTarget(int Key, bool Control, bool Alt, bool Shift)
{
    panelToggle(&g_state.panel_dps_self);
}

void keybCallback_panelDpsGroup(int Key, bool Control, bool Alt, bool Shift)
{
    panelToggle(&g_state.panel_dps_group);
}

void keybCallback_panelBuffUptime(int Key, bool Control, bool Alt, bool Shift)
{
    panelToggle(&g_state.panel_buff_uptime);
}

void keybCallback_panelSkillBreakdown(int Key, bool Control, bool Alt, bool Shift)
{
    panelToggle(&g_state.panel_skills);
}

void keybCallback_panelFloat(int Key, bool Control, bool Alt, bool Shift)
{
    panelToggle(&g_state.panel_float);
}

void keybCallback_panelGearSelf(int Key, bool Control, bool Alt, bool Shift)
{
    panelToggle(&g_state.panel_gear_self);
}

void keybCallback_panelGearTarget(int Key, bool Control, bool Alt, bool Shift)
{
    panelToggle(&g_state.panel_gear_target);
}


static void config_get_panel(Panel* panel, i8 const* section, i8 const* keybind, i8 const* keybind_default, keybCallback keybind_cb, i32 enabled)
{
    strncpy(panel->section, section, sizeof(panel->section));
    panel->enabled = config_get_int(section, "Enabled", enabled);
    panel->tinyFont = config_get_int(section, "TinyFont", 1);
    panel->autoResize = config_get_int(section, "AutoResize", 1);
    panel->mode = config_get_int(section, "Mode", 0);
    panel->fAlpha = config_get_float(section, "Alpha", 0.4f);
    panel->cfg.useLocalizedText = config_get_int(section, "LocalizedText", 0);
    panel->bind = config_get_bind(CONFIG_SECTION_BIND, keybind, keybind_default, keybind_cb);
}

void config_fini()
{
    clearKeys();
}

bool config_init(i8 const* default_ini)
{
    // First check if we have an INI file
    // if not use the embedded INI
#if TARGET_OS_WIN
    if (!file_existsW(config_get_ini_file()))
    {
        LPVOID pIniFile = NULL;
        DWORD dwBytes = 0;
        if (GetEmbeddedResource(IDR_RT_TEXT_INIFILE, &pIniFile, &dwBytes)) {
            wchar_t dir[1024] = { 0 };
            StringCchPrintfW(dir, ARRAYSIZE(dir), L"%s\\bgdm", config_get_my_documents());
            CreateDirectoryW(dir, 0);
            file_writeW(config_get_ini_file(), pIniFile, dwBytes);
        }
    }
#elif __APPLE__
    if (default_ini) {
        file_copy(default_ini, config_get_ini_fileA(), false);
    }
#endif
    
    // Read the configuration state.
    // Global on/off
    g_state.bind_on_off = config_get_bind(CONFIG_SECTION_BIND, "GlobalOnOff", "F7", keybCallback_globalOnOff);
    g_state.bind_input = config_get_bind(CONFIG_SECTION_BIND, "GlobalInput", "Ctrl + Shift + F7", keybCallback_inputCapOnOff);
    g_state.bind_minPanels = config_get_bind(CONFIG_SECTION_BIND, "GlobalMinPanels", "Ctrl + Shift + F8", keybCallback_uiMinPanels);
    g_state.bind_screenshot = config_get_bind(CONFIG_SECTION_BIND, "Screenshot", "Ctrl + Shift + F10", keybCallback_screenshot);
    g_state.global_on_off = 1;
    
    // For some stuid reason CTRL+SHIFT+LBUTTON doesn't work
    // whenever CTRL+SHIFT are pressed LBUTTON is sent as RBUTTON
    // so I had to use ALT+SHIFT
    g_state.bind_dps_lock = config_get_bind(CONFIG_SECTION_BIND, "DPSLock", "F8", keybCallback_targetLock);
    g_state.bind_dps_reset = config_get_bind(CONFIG_SECTION_BIND, "DPSReset", "F9", keybCallback_dpsReset);
    
    // Panel configuration
    config_get_panel(&g_state.panel_debug, CONFIG_SECTION_DBG, "Debug", "F12", keybCallback_panelDebug, 0);
    config_get_panel(&g_state.panel_version, CONFIG_SECTION_OPTIONS, "OptionsToggle", "Ctrl + Shift + 0", keybCallback_panelMain, 1);
    config_get_panel(&g_state.panel_hp, CONFIG_SECTION_HP, "HPToggle", "Ctrl + Shift + 1", keybCallback_panelHp, 1);
    config_get_panel(&g_state.panel_compass, CONFIG_SECTION_CONMPASS, "CompassToggle", "Ctrl + Shift + 2", keybCallback_panelCompass, 1);
    config_get_panel(&g_state.panel_dps_self, CONFIG_SECTION_DPS_TARGET, "DPSToggleTarget", "Ctrl + Shift + 3", keybCallback_panelDpsTarget, 1);
    config_get_panel(&g_state.panel_dps_group, CONFIG_SECTION_DPS_GROUP, "DPSToggleGroup", "Ctrl + Shift + 4", keybCallback_panelDpsGroup, 1);
    config_get_panel(&g_state.panel_buff_uptime, CONFIG_SECTION_BUFF_UPTIME, "BuffUptimeToggle", "Ctrl + Shift + 5", keybCallback_panelBuffUptime, 0);
    config_get_panel(&g_state.panel_skills, CONFIG_SECTION_SKILLS, "SkillBreakdownToggle", "Ctrl + Shift + 6", keybCallback_panelSkillBreakdown, 0);
    
#if !(defined BGDM_TOS_COMPLIANT)
    config_get_panel(&g_state.panel_float, CONFIG_SECTION_FLOAT, "FloatBarsToggle", "Ctrl + Shift + 7", keybCallback_panelFloat, 0);
    config_get_panel(&g_state.panel_gear_self, CONFIG_SECTION_GEAR_SELF, "GearToggleSelf", "Ctrl + Shift + 8", keybCallback_panelGearSelf, 0);
    config_get_panel(&g_state.panel_gear_target, CONFIG_SECTION_GEAR_TARGET, "GearToggleTarget", "Ctrl + Shift + 9", keybCallback_panelGearTarget, 1);
#endif
    
    // Options panel
    g_state.show_metrics = config_get_int(CONFIG_SECTION_OPTIONS, "ShowMetricsWindow", 0);
    g_state.show_server = config_get_int(CONFIG_SECTION_OPTIONS, "ShowServerStatus", 0);
    
    // Target stats config
    g_state.panel_dps_self.cfg.str = config_get_str(CONFIG_SECTION_DPS_TARGET, "Columns", "DPS|DMG|TTK");
    g_state.panel_dps_self.cfg.maxCol = TDPS_COL_END;
    g_state.panel_dps_self.cfg.col_visible[TDPS_COL_DPS] = 1;
    
    // Sort configuration
    g_state.panel_dps_group.cfg.lineNumbering = config_get_int(CONFIG_SECTION_DPS_GROUP, "LineNumbering", 1);
    g_state.panel_dps_group.cfg.useProfColoring = config_get_int(CONFIG_SECTION_DPS_GROUP, "ProfessionColoring", 0);
    g_state.panel_dps_group.cfg.maxCol = GDPS_COL_END2;
    g_state.panel_dps_group.cfg.sortColLast = GDPS_COL_END;
    g_state.panel_dps_group.cfg.col_visible[GDPS_COL_NAME] = 1;
    g_state.panel_dps_group.cfg.sortCol = config_get_int(CONFIG_SECTION_DPS_GROUP, "Sort", 1);
    g_state.panel_dps_group.cfg.asc = config_get_int(CONFIG_SECTION_DPS_GROUP, "SortAsc", 0);
    g_state.panel_dps_group.cfg.str = config_get_str(CONFIG_SECTION_DPS_GROUP,
                                                     "Columns", "NAME|CLS|DPS|PER|DMGOUT|DMGIN|HPS|HEAL|TIME");
    
    // Skills panel
    g_state.panel_skills.cfg.lineNumbering = config_get_int(CONFIG_SECTION_SKILLS, "LineNumbering", 1);
    
    g_state.icon_pack_no = config_get_int(CONFIG_SECTION_BUFF_UPTIME, "IconPackNo", 0);
    g_state.use_seaweed_icon = config_get_int(CONFIG_SECTION_BUFF_UPTIME, "UseSeaweedSaladIcon", 0);
    g_state.use_downed_enemy_icon = config_get_int(CONFIG_SECTION_BUFF_UPTIME, "UseDownedEnemyIcon", 0);
    g_state.panel_buff_uptime.cfg.lineNumbering = config_get_int(CONFIG_SECTION_BUFF_UPTIME, "LineNumbering", 1);
    g_state.panel_buff_uptime.cfg.useProfColoring = config_get_int(CONFIG_SECTION_BUFF_UPTIME, "ProfessionColoring", 0);
    g_state.panel_buff_uptime.cfg.maxCol = BUFF_COL_END;
    g_state.panel_buff_uptime.cfg.sortColLast = BUFF_COL_END;
    g_state.panel_buff_uptime.cfg.col_visible[BUFF_COL_NAME] = 1;
    g_state.panel_buff_uptime.cfg.sortCol = config_get_int(CONFIG_SECTION_BUFF_UPTIME, "Sort", 0);
    g_state.panel_buff_uptime.cfg.asc = config_get_int(CONFIG_SECTION_BUFF_UPTIME, "SortAsc", 0);
    g_state.panel_buff_uptime.cfg.str = config_get_str(CONFIG_SECTION_BUFF_UPTIME,
                                                       "Columns", "NAME|CLS|SUB|DWN|SCL|SWD|PRT|QCK|ALC|FRY|MGT|AEG|GTL|GLE|TIME");
    
    // Row config for version & hp panel
    g_state.hpCommas_disable = config_get_int(CONFIG_SECTION_HP, "CommaSeparatorsDisable", 0);
    g_state.panel_hp.cfg.str = config_get_str(CONFIG_SECTION_HP, "Columns", "PLAYER_HP|TARGET_HP|TARGET_DIST|TARGET_BB");
    g_state.panel_hp.cfg.maxCol = HP_ROW_END;
    
#if !(defined BGDM_TOS_COMPLIANT)
    // Float HP bars
    ImGui_FloatBarConfigLoad();
#endif
    
    // Load styling config
    ImGui_StyleConfigLoad();
    
    // Global
    g_state.global_on_off = config_get_int(CONFIG_SECTION_GLOBAL, "Enabled", 1);
    g_state.global_cap_input = config_get_int(CONFIG_SECTION_GLOBAL, "CaptureInput", 1);
    g_state.minPanels_enable = config_get_int(CONFIG_SECTION_GLOBAL, "MinimalPanels", 0);
    g_state.priortizeSquad = config_get_int(CONFIG_SECTION_GLOBAL, "PrioritizeSquadMembers", 0);
    g_state.hideNonSquad = config_get_int(CONFIG_SECTION_GLOBAL, "HideNonSquadMembers", 0);
    g_state.hideNonParty = config_get_int(CONFIG_SECTION_GLOBAL, "HideNonPartyMembers", 0);
    g_state.ooc_grace_period = config_get_int(CONFIG_SECTION_GLOBAL, "OOCGracePeriod", 5);
    g_state.target_retention_time = config_get_int(CONFIG_SECTION_GLOBAL, "TargetRetention", -1);
    g_state.max_players = config_get_int(CONFIG_SECTION_GLOBAL, "MaxPlayers", 9);
    if (g_state.max_players < 0) g_state.max_players = 0;
    if (g_state.max_players > SQUAD_SIZE_MAX) g_state.max_players = SQUAD_SIZE_MAX;
    
    // Language
    char *lang_dir = config_get_str(CONFIG_SECTION_LANG, "Directory", "bgdm/lang");
    char *lang_file = config_get_str(CONFIG_SECTION_LANG, "Default", "");
    snprintf(g_state.lang_dir, ARRAYSIZE(g_state.lang_dir), "%s/%s", config_get_my_documentsA(), lang_dir);
    if (lang_file && strlen(lang_file) > 0)
        snprintf(g_state.lang_file, ARRAYSIZE(g_state.lang_file), "%s", lang_file);
    if (lang_file) free(lang_file);
    if (lang_dir) free(lang_dir);
    
    // Server addr:port
    g_state.network_addr = config_get_str(CONFIG_SECTION_SRV,  "srv_addr", MSG_NETWORK_ADDR);
    g_state.network_port = (u16)config_get_int(CONFIG_SECTION_SRV, "srv_port", MSG_NETWORK_PORT);
    g_state.autoUpdate_disable = config_get_int(CONFIG_SECTION_SRV, "AutoUpdateDisable", 0);
    g_state.netDps_disable = config_get_int(CONFIG_SECTION_SRV, "NetworkDpsDisable", 0);
    
    // Override the old server
    if (g_state.network_addr && strcmp(g_state.network_addr, MSG_NETWORK_ADDR_AMZN) == 0) {
        free(g_state.network_addr);
        g_state.network_addr = strdup(MSG_NETWORK_ADDR_NFO);
        config_set_str(CONFIG_SECTION_SRV, "srv_addr", g_state.network_addr);
    }
    
    // adjust to seconds
    if (g_state.target_retention_time >= 0)
        g_state.target_retention_time = g_state.target_retention_time * 1000000;
    else
        g_state.target_retention_time = INT_MAX;
    
    
    config_parse_columns(&g_state.panel_dps_group, gdps_col_str, GDPS_COL_CLS, GDPS_COL_END2);
    config_parse_columns(&g_state.panel_dps_self, tdps_col_str, TDPS_COL_DMG, TDPS_COL_END);
    config_parse_columns(&g_state.panel_buff_uptime, buff_col_str, BUFF_COL_CLS, BUFF_COL_END);
    config_parse_columns(&g_state.panel_version, ver_row_str, VER_ROW_VER, VER_ROW_END);
    config_parse_columns(&g_state.panel_hp, hp_row_str, HP_ROW_PLAYER_HP, HP_ROW_END);
    
    g_state.panel_skills.cfg.tabNo = 2;
    g_state.panel_dps_group.cfg.tabNo = 2;
    g_state.panel_gear_self.cfg.tabNo = 3;
    g_state.panel_gear_target.cfg.tabNo = 3;
    
    
    // Add panels with close button to the array
    g_state.panel_arr[0] = &g_state.panel_gear_self;
    g_state.panel_arr[1] = &g_state.panel_gear_target;
    g_state.panel_arr[2] = &g_state.panel_dps_self;
    g_state.panel_arr[3] = &g_state.panel_dps_group;
    g_state.panel_arr[4] = &g_state.panel_buff_uptime;
    g_state.panel_arr[5] = &g_state.panel_version;
    g_state.panel_arr[6] = &g_state.panel_hp;
    g_state.panel_arr[7] = &g_state.panel_compass;
    g_state.panel_arr[8] = &g_state.panel_float;
    g_state.panel_arr[9] = &g_state.panel_skills;
    
    
    return true;
}
