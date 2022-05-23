//
//  imgui_memory_editor.hpp
//  bgdm
//
//  Created by Bhagwan on 7/8/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#ifndef imgui_memory_editor_h
#define imgui_memory_editor_h

#include <stdint.h>
#include <string>
#include <vector>


struct MemoryEditor
{
    bool				IsOpen;
    bool				AllowEdits;
    int64_t				DataEditingAddr;
    bool				DataEditingTakeFocus;
    char				DataInput[32];
    char				AddrInput[32];
    char				OffsetInput[32];
    char				BaseAddrInput[32];
    
private:
    
    typedef struct TabData
    {
        int					mode;
        int					bytes_per_line;
        std::string         name;
        uintptr_t			base_addr;
        uintptr_t			edit_addr;
        unsigned char*		mem_data;
        int					mem_size;
    } TabData;
    
    std::string                         m_title;
    bool								m_is64;
    int									m_tabCount;
    int									m_tabSelected;
    std::vector<int>					m_tabOrder;
    std::vector<const char *>           m_tabNames;
    std::vector<TabData>				m_tabData;
    
public:
    
    MemoryEditor();
    MemoryEditor(const char *title, const char *tabTitle = NULL, int tabCount = 4, int selectedTab = 0,
                 unsigned char* mem_data = NULL, int mem_size = 0x1000, uintptr_t base_display_addr = 0);
    
    void Init(const char *title = NULL, const char *tabTitle = NULL, int tabCount = 4, int selectedTab = 0,
              unsigned char* mem_data = NULL, int mem_size = 0x1000, uintptr_t base_display_addr = 0);
    void InitTabs(const char *tabTitle, int tabCount, int tabSelected);
    unsigned char *GetBaseAddr(int selected_tab = -1);
    void SetBaseAddr(unsigned char* mem_data, int mem_size = 0x1000, uintptr_t base_display_addr = 0, int selected_tab = -1);
    void Open(unsigned char* mem_data, int mem_size = 0x1000, size_t base_display_addr = 0, int selected_tab = -1);
    void FindOffset(size_t offset);
    void FindStrOffset(const char *str, bool isAddr);
    void Draw();
};


#endif /* imgui_memory_editor_h */
