#pragma once

#include <stdarg.h>
#include <stdio.h>
#include "sx/math-types.h"


#ifdef _MSC_VER
typedef unsigned __int64 ImU64;
#else
//typedef unsigned long long ImU64;
#endif

#if !defined(CIMGUI_DEFINE_ENUMS_AND_STRUCTS) && !defined(RIZZ__IMGUI)
typedef struct ImGuiTableColumnSettings ImGuiTableColumnSettings;
typedef struct ImGuiTableCellData ImGuiTableCellData;
typedef struct ImGuiViewportP ImGuiViewportP;
typedef struct ImGuiWindowDockStyle ImGuiWindowDockStyle;
typedef struct ImGuiPtrOrIndex ImGuiPtrOrIndex;
typedef struct ImGuiShrinkWidthItem ImGuiShrinkWidthItem;
typedef struct ImGuiDataTypeTempStorage ImGuiDataTypeTempStorage;
typedef struct ImVec2ih ImVec2ih;
typedef struct ImVec1 ImVec1;
typedef struct StbTexteditRow StbTexteditRow;
typedef struct STB_TexteditState STB_TexteditState;
typedef struct StbUndoState StbUndoState;
typedef struct StbUndoRecord StbUndoRecord;
typedef struct ImGuiWindowSettings ImGuiWindowSettings;
typedef struct ImGuiWindowTempData ImGuiWindowTempData;
typedef struct ImGuiWindow ImGuiWindow;
typedef struct ImGuiTableColumnsSettings ImGuiTableColumnsSettings;
typedef struct ImGuiTableSettings ImGuiTableSettings;
typedef struct ImGuiTableColumn ImGuiTableColumn;
typedef struct ImGuiTable ImGuiTable;
typedef struct ImGuiTabItem ImGuiTabItem;
typedef struct ImGuiTabBar ImGuiTabBar;
typedef struct ImGuiStyleMod ImGuiStyleMod;
typedef struct ImGuiStackSizes ImGuiStackSizes;
typedef struct ImGuiSettingsHandler ImGuiSettingsHandler;
typedef struct ImGuiPopupData ImGuiPopupData;
typedef struct ImGuiOldColumns ImGuiOldColumns;
typedef struct ImGuiOldColumnData ImGuiOldColumnData;
typedef struct ImGuiNextItemData ImGuiNextItemData;
typedef struct ImGuiNextWindowData ImGuiNextWindowData;
typedef struct ImGuiMetricsConfig ImGuiMetricsConfig;
typedef struct ImGuiNavMoveResult ImGuiNavMoveResult;
typedef struct ImGuiMenuColumns ImGuiMenuColumns;
typedef struct ImGuiLastItemDataBackup ImGuiLastItemDataBackup;
typedef struct ImGuiInputTextState ImGuiInputTextState;
typedef struct ImGuiGroupData ImGuiGroupData;
typedef struct ImGuiDockNodeSettings ImGuiDockNodeSettings;
typedef struct ImGuiDockNode ImGuiDockNode;
typedef struct ImGuiDockRequest ImGuiDockRequest;
typedef struct ImGuiDockContext ImGuiDockContext;
typedef struct ImGuiDataTypeInfo ImGuiDataTypeInfo;
typedef struct ImGuiContextHook ImGuiContextHook;
typedef struct ImGuiColorMod ImGuiColorMod;
typedef struct ImDrawDataBuilder ImDrawDataBuilder;
typedef struct ImRect ImRect;
typedef struct ImBitVector ImBitVector;
typedef struct ImFontAtlasCustomRect ImFontAtlasCustomRect;
typedef struct ImDrawCmdHeader ImDrawCmdHeader;
typedef struct ImGuiStoragePair ImGuiStoragePair;
typedef struct ImGuiTextRange ImGuiTextRange;
typedef struct ImGuiWindowClass ImGuiWindowClass;
typedef struct ImGuiViewport ImGuiViewport;
typedef struct ImGuiTextFilter ImGuiTextFilter;
typedef struct ImGuiTextBuffer ImGuiTextBuffer;
typedef struct ImGuiTableColumnSortSpecs ImGuiTableColumnSortSpecs;
typedef struct ImGuiTableSortSpecs ImGuiTableSortSpecs;
typedef struct ImGuiStyle ImGuiStyle;
typedef struct ImGuiStorage ImGuiStorage;
typedef struct ImGuiSizeCallbackData ImGuiSizeCallbackData;
typedef struct ImGuiPlatformMonitor ImGuiPlatformMonitor;
typedef struct ImGuiPlatformIO ImGuiPlatformIO;
typedef struct ImGuiPayload ImGuiPayload;
typedef struct ImGuiOnceUponAFrame ImGuiOnceUponAFrame;
typedef struct ImGuiListClipper ImGuiListClipper;
typedef struct ImGuiInputTextCallbackData ImGuiInputTextCallbackData;
typedef struct ImGuiIO ImGuiIO;
typedef struct ImGuiContext ImGuiContext;
typedef struct ImColor ImColor;
typedef struct ImFontGlyphRangesBuilder ImFontGlyphRangesBuilder;
typedef struct ImFontGlyph ImFontGlyph;
typedef struct ImFontConfig ImFontConfig;
typedef struct ImFontBuilderIO ImFontBuilderIO;
typedef struct ImFontAtlas ImFontAtlas;
typedef struct ImFont ImFont;
typedef struct ImDrawVert ImDrawVert;
typedef struct ImDrawListSplitter ImDrawListSplitter;
typedef struct ImDrawListSharedData ImDrawListSharedData;
typedef struct ImDrawList ImDrawList;
typedef struct ImDrawData ImDrawData;
typedef struct ImDrawCmd ImDrawCmd;
typedef struct ImDrawChannel ImDrawChannel;

typedef union sx_vec2 ImVec2;
typedef union sx_vec4 ImVec4;
struct ImDrawChannel;
struct ImDrawCmd;
struct ImDrawData;
struct ImDrawList;
struct ImDrawListSharedData;
struct ImDrawListSplitter;
struct ImDrawVert;
struct ImFont;
struct ImFontAtlas;
struct ImFontBuilderIO;
struct ImFontConfig;
struct ImFontGlyph;
struct ImFontGlyphRangesBuilder;
struct ImColor;
struct ImGuiContext;
struct ImGuiIO;
struct ImGuiInputTextCallbackData;
struct ImGuiListClipper;
struct ImGuiOnceUponAFrame;
struct ImGuiPayload;
struct ImGuiPlatformIO;
struct ImGuiPlatformMonitor;
struct ImGuiSizeCallbackData;
struct ImGuiStorage;
struct ImGuiStyle;
struct ImGuiTableSortSpecs;
struct ImGuiTableColumnSortSpecs;
struct ImGuiTextBuffer;
struct ImGuiTextFilter;
struct ImGuiViewport;
struct ImGuiWindowClass;
typedef int ImGuiCol;
typedef int ImGuiCond;
typedef int ImGuiDataType;
typedef int ImGuiDir;
typedef int ImGuiKey;
typedef int ImGuiNavInput;
typedef int ImGuiMouseButton;
typedef int ImGuiMouseCursor;
typedef int ImGuiSortDirection;
typedef int ImGuiStyleVar;
typedef int ImGuiTableBgTarget;
typedef int ImDrawFlags;
typedef int ImDrawListFlags;
typedef int ImFontAtlasFlags;
typedef int ImGuiBackendFlags;
typedef int ImGuiButtonFlags;
typedef int ImGuiColorEditFlags;
typedef int ImGuiConfigFlags;
typedef int ImGuiComboFlags;
typedef int ImGuiDockNodeFlags;
typedef int ImGuiDragDropFlags;
typedef int ImGuiFocusedFlags;
typedef int ImGuiHoveredFlags;
typedef int ImGuiInputTextFlags;
typedef int ImGuiKeyModFlags;
typedef int ImGuiPopupFlags;
typedef int ImGuiSelectableFlags;
typedef int ImGuiSliderFlags;
typedef int ImGuiTabBarFlags;
typedef int ImGuiTabItemFlags;
typedef int ImGuiTableFlags;
typedef int ImGuiTableColumnFlags;
typedef int ImGuiTableRowFlags;
typedef int ImGuiTreeNodeFlags;
typedef int ImGuiViewportFlags;
typedef int ImGuiWindowFlags;
typedef void* ImTextureID;
typedef unsigned int ImGuiID;
typedef int (*ImGuiInputTextCallback)(ImGuiInputTextCallbackData* data);
typedef void (*ImGuiSizeCallback)(ImGuiSizeCallbackData* data);
typedef void* (*ImGuiMemAllocFunc)(size_t sz, void* user_data);
typedef void (*ImGuiMemFreeFunc)(void* ptr, void* user_data);
typedef unsigned short ImWchar16;
typedef unsigned int ImWchar32;
typedef ImWchar16 ImWchar;
typedef signed char ImS8;
typedef unsigned char ImU8;
typedef signed short ImS16;
typedef unsigned short ImU16;
typedef signed int ImS32;
typedef unsigned int ImU32;
typedef int64_t ImS64;
typedef uint64_t ImU64;
typedef void (*ImDrawCallback)(const ImDrawList* parent_list, const ImDrawCmd* cmd);
typedef unsigned short ImDrawIdx;
struct ImBitVector;
struct ImRect;
struct ImDrawDataBuilder;
struct ImDrawListSharedData;
struct ImGuiColorMod;
struct ImGuiContext;
struct ImGuiContextHook;
struct ImGuiDataTypeInfo;
struct ImGuiDockContext;
struct ImGuiDockRequest;
struct ImGuiDockNode;
struct ImGuiDockNodeSettings;
struct ImGuiGroupData;
struct ImGuiInputTextState;
struct ImGuiLastItemDataBackup;
struct ImGuiMenuColumns;
struct ImGuiNavMoveResult;
struct ImGuiMetricsConfig;
struct ImGuiNextWindowData;
struct ImGuiNextItemData;
struct ImGuiOldColumnData;
struct ImGuiOldColumns;
struct ImGuiPopupData;
struct ImGuiSettingsHandler;
struct ImGuiStackSizes;
struct ImGuiStyleMod;
struct ImGuiTabBar;
struct ImGuiTabItem;
struct ImGuiTable;
struct ImGuiTableColumn;
struct ImGuiTableSettings;
struct ImGuiTableColumnsSettings;
struct ImGuiWindow;
struct ImGuiWindowTempData;
struct ImGuiWindowSettings;
typedef int ImGuiDataAuthority;
typedef int ImGuiLayoutType;
typedef int ImGuiItemFlags;
typedef int ImGuiItemStatusFlags;
typedef int ImGuiOldColumnFlags;
typedef int ImGuiNavHighlightFlags;
typedef int ImGuiNavDirSourceFlags;
typedef int ImGuiNavMoveFlags;
typedef int ImGuiNextItemDataFlags;
typedef int ImGuiNextWindowDataFlags;
typedef int ImGuiSeparatorFlags;
typedef int ImGuiTextFlags;
typedef int ImGuiTooltipFlags;
typedef void (*ImGuiErrorLogCallback)(void* user_data, const char* fmt, ...);
extern ImGuiContext* GImGui;
typedef FILE* ImFileHandle;
typedef int ImPoolIdx;
typedef void (*ImGuiContextHookCallback)(ImGuiContext* ctx, ImGuiContextHook* hook);
typedef ImS8 ImGuiTableColumnIdx;
typedef ImU8 ImGuiTableDrawChannelIdx;
typedef struct ImVector{int Size;int Capacity;void* Data;} ImVector;
typedef struct ImVector_ImGuiTableSettings {int Size;int Capacity;ImGuiTableSettings* Data;} ImVector_ImGuiTableSettings;
typedef struct ImChunkStream_ImGuiTableSettings {ImVector_ImGuiTableSettings Buf;} ImChunkStream_ImGuiTableSettings;
typedef struct ImVector_ImGuiWindowSettings {int Size;int Capacity;ImGuiWindowSettings* Data;} ImVector_ImGuiWindowSettings;
typedef struct ImChunkStream_ImGuiWindowSettings {ImVector_ImGuiWindowSettings Buf;} ImChunkStream_ImGuiWindowSettings;
typedef struct ImSpan_ImGuiTableCellData {ImGuiTableCellData* Data;ImGuiTableCellData* DataEnd;} ImSpan_ImGuiTableCellData;
typedef struct ImSpan_ImGuiTableColumn {ImGuiTableColumn* Data;ImGuiTableColumn* DataEnd;} ImSpan_ImGuiTableColumn;
typedef struct ImSpan_ImGuiTableColumnIdx {ImGuiTableColumnIdx* Data;ImGuiTableColumnIdx* DataEnd;} ImSpan_ImGuiTableColumnIdx;
typedef struct ImVector_ImDrawChannel {int Size;int Capacity;ImDrawChannel* Data;} ImVector_ImDrawChannel;
typedef struct ImVector_ImDrawCmd {int Size;int Capacity;ImDrawCmd* Data;} ImVector_ImDrawCmd;
typedef struct ImVector_ImDrawIdx {int Size;int Capacity;ImDrawIdx* Data;} ImVector_ImDrawIdx;
typedef struct ImVector_ImDrawListPtr {int Size;int Capacity;ImDrawList** Data;} ImVector_ImDrawListPtr;
typedef struct ImVector_ImDrawVert {int Size;int Capacity;ImDrawVert* Data;} ImVector_ImDrawVert;
typedef struct ImVector_ImFontPtr {int Size;int Capacity;ImFont** Data;} ImVector_ImFontPtr;
typedef struct ImVector_ImFontAtlasCustomRect {int Size;int Capacity;ImFontAtlasCustomRect* Data;} ImVector_ImFontAtlasCustomRect;
typedef struct ImVector_ImFontConfig {int Size;int Capacity;ImFontConfig* Data;} ImVector_ImFontConfig;
typedef struct ImVector_ImFontGlyph {int Size;int Capacity;ImFontGlyph* Data;} ImVector_ImFontGlyph;
typedef struct ImVector_ImGuiColorMod {int Size;int Capacity;ImGuiColorMod* Data;} ImVector_ImGuiColorMod;
typedef struct ImVector_ImGuiContextHook {int Size;int Capacity;ImGuiContextHook* Data;} ImVector_ImGuiContextHook;
typedef struct ImVector_ImGuiDockNodeSettings {int Size;int Capacity;ImGuiDockNodeSettings* Data;} ImVector_ImGuiDockNodeSettings;
typedef struct ImVector_ImGuiDockRequest {int Size;int Capacity;ImGuiDockRequest* Data;} ImVector_ImGuiDockRequest;
typedef struct ImVector_ImGuiGroupData {int Size;int Capacity;ImGuiGroupData* Data;} ImVector_ImGuiGroupData;
typedef struct ImVector_ImGuiID {int Size;int Capacity;ImGuiID* Data;} ImVector_ImGuiID;
typedef struct ImVector_ImGuiItemFlags {int Size;int Capacity;ImGuiItemFlags* Data;} ImVector_ImGuiItemFlags;
typedef struct ImVector_ImGuiOldColumnData {int Size;int Capacity;ImGuiOldColumnData* Data;} ImVector_ImGuiOldColumnData;
typedef struct ImVector_ImGuiOldColumns {int Size;int Capacity;ImGuiOldColumns* Data;} ImVector_ImGuiOldColumns;
typedef struct ImVector_ImGuiPlatformMonitor {int Size;int Capacity;ImGuiPlatformMonitor* Data;} ImVector_ImGuiPlatformMonitor;
typedef struct ImVector_ImGuiPopupData {int Size;int Capacity;ImGuiPopupData* Data;} ImVector_ImGuiPopupData;
typedef struct ImVector_ImGuiPtrOrIndex {int Size;int Capacity;ImGuiPtrOrIndex* Data;} ImVector_ImGuiPtrOrIndex;
typedef struct ImVector_ImGuiSettingsHandler {int Size;int Capacity;ImGuiSettingsHandler* Data;} ImVector_ImGuiSettingsHandler;
typedef struct ImVector_ImGuiShrinkWidthItem {int Size;int Capacity;ImGuiShrinkWidthItem* Data;} ImVector_ImGuiShrinkWidthItem;
typedef struct ImVector_ImGuiStoragePair {int Size;int Capacity;ImGuiStoragePair* Data;} ImVector_ImGuiStoragePair;
typedef struct ImVector_ImGuiStyleMod {int Size;int Capacity;ImGuiStyleMod* Data;} ImVector_ImGuiStyleMod;
typedef struct ImVector_ImGuiTabItem {int Size;int Capacity;ImGuiTabItem* Data;} ImVector_ImGuiTabItem;
typedef struct ImVector_ImGuiTableColumnSortSpecs {int Size;int Capacity;ImGuiTableColumnSortSpecs* Data;} ImVector_ImGuiTableColumnSortSpecs;
typedef struct ImVector_ImGuiTextRange {int Size;int Capacity;ImGuiTextRange* Data;} ImVector_ImGuiTextRange;
typedef struct ImVector_ImGuiViewportPtr {int Size;int Capacity;ImGuiViewport** Data;} ImVector_ImGuiViewportPtr;
typedef struct ImVector_ImGuiViewportPPtr {int Size;int Capacity;ImGuiViewportP** Data;} ImVector_ImGuiViewportPPtr;
typedef struct ImVector_ImGuiWindowPtr {int Size;int Capacity;ImGuiWindow** Data;} ImVector_ImGuiWindowPtr;
typedef struct ImVector_ImTextureID {int Size;int Capacity;ImTextureID* Data;} ImVector_ImTextureID;
typedef struct ImVector_ImU32 {int Size;int Capacity;ImU32* Data;} ImVector_ImU32;
typedef struct ImVector_ImVec2 {int Size;int Capacity;ImVec2* Data;} ImVector_ImVec2;
typedef struct ImVector_ImVec4 {int Size;int Capacity;ImVec4* Data;} ImVector_ImVec4;
typedef struct ImVector_ImWchar {int Size;int Capacity;ImWchar* Data;} ImVector_ImWchar;
typedef struct ImVector_char {int Size;int Capacity;char* Data;} ImVector_char;
typedef struct ImVector_const_charPtr {int Size;int Capacity;const char** Data;} ImVector_const_charPtr;
typedef struct ImVector_float {int Size;int Capacity;float* Data;} ImVector_float;
typedef struct ImVector_unsigned_char {int Size;int Capacity;unsigned char* Data;} ImVector_unsigned_char;

typedef enum {
    ImGuiWindowFlags_None = 0,
    ImGuiWindowFlags_NoTitleBar = 1 << 0,
    ImGuiWindowFlags_NoResize = 1 << 1,
    ImGuiWindowFlags_NoMove = 1 << 2,
    ImGuiWindowFlags_NoScrollbar = 1 << 3,
    ImGuiWindowFlags_NoScrollWithMouse = 1 << 4,
    ImGuiWindowFlags_NoCollapse = 1 << 5,
    ImGuiWindowFlags_AlwaysAutoResize = 1 << 6,
    ImGuiWindowFlags_NoBackground = 1 << 7,
    ImGuiWindowFlags_NoSavedSettings = 1 << 8,
    ImGuiWindowFlags_NoMouseInputs = 1 << 9,
    ImGuiWindowFlags_MenuBar = 1 << 10,
    ImGuiWindowFlags_HorizontalScrollbar = 1 << 11,
    ImGuiWindowFlags_NoFocusOnAppearing = 1 << 12,
    ImGuiWindowFlags_NoBringToFrontOnFocus = 1 << 13,
    ImGuiWindowFlags_AlwaysVerticalScrollbar= 1 << 14,
    ImGuiWindowFlags_AlwaysHorizontalScrollbar=1<< 15,
    ImGuiWindowFlags_AlwaysUseWindowPadding = 1 << 16,
    ImGuiWindowFlags_NoNavInputs = 1 << 18,
    ImGuiWindowFlags_NoNavFocus = 1 << 19,
    ImGuiWindowFlags_UnsavedDocument = 1 << 20,
    ImGuiWindowFlags_NoDocking = 1 << 21,
    ImGuiWindowFlags_NoNav = ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus,
    ImGuiWindowFlags_NoDecoration = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse,
    ImGuiWindowFlags_NoInputs = ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus,
    ImGuiWindowFlags_NavFlattened = 1 << 23,
    ImGuiWindowFlags_ChildWindow = 1 << 24,
    ImGuiWindowFlags_Tooltip = 1 << 25,
    ImGuiWindowFlags_Popup = 1 << 26,
    ImGuiWindowFlags_Modal = 1 << 27,
    ImGuiWindowFlags_ChildMenu = 1 << 28,
    ImGuiWindowFlags_DockNodeHost = 1 << 29
}ImGuiWindowFlags_;
typedef enum {
    ImGuiInputTextFlags_None = 0,
    ImGuiInputTextFlags_CharsDecimal = 1 << 0,
    ImGuiInputTextFlags_CharsHexadecimal = 1 << 1,
    ImGuiInputTextFlags_CharsUppercase = 1 << 2,
    ImGuiInputTextFlags_CharsNoBlank = 1 << 3,
    ImGuiInputTextFlags_AutoSelectAll = 1 << 4,
    ImGuiInputTextFlags_EnterReturnsTrue = 1 << 5,
    ImGuiInputTextFlags_CallbackCompletion = 1 << 6,
    ImGuiInputTextFlags_CallbackHistory = 1 << 7,
    ImGuiInputTextFlags_CallbackAlways = 1 << 8,
    ImGuiInputTextFlags_CallbackCharFilter = 1 << 9,
    ImGuiInputTextFlags_AllowTabInput = 1 << 10,
    ImGuiInputTextFlags_CtrlEnterForNewLine = 1 << 11,
    ImGuiInputTextFlags_NoHorizontalScroll = 1 << 12,
    ImGuiInputTextFlags_AlwaysOverwrite = 1 << 13,
    ImGuiInputTextFlags_ReadOnly = 1 << 14,
    ImGuiInputTextFlags_Password = 1 << 15,
    ImGuiInputTextFlags_NoUndoRedo = 1 << 16,
    ImGuiInputTextFlags_CharsScientific = 1 << 17,
    ImGuiInputTextFlags_CallbackResize = 1 << 18,
    ImGuiInputTextFlags_CallbackEdit = 1 << 19,
    ImGuiInputTextFlags_Multiline = 1 << 20,
    ImGuiInputTextFlags_NoMarkEdited = 1 << 21
}ImGuiInputTextFlags_;
typedef enum {
    ImGuiTreeNodeFlags_None = 0,
    ImGuiTreeNodeFlags_Selected = 1 << 0,
    ImGuiTreeNodeFlags_Framed = 1 << 1,
    ImGuiTreeNodeFlags_AllowItemOverlap = 1 << 2,
    ImGuiTreeNodeFlags_NoTreePushOnOpen = 1 << 3,
    ImGuiTreeNodeFlags_NoAutoOpenOnLog = 1 << 4,
    ImGuiTreeNodeFlags_DefaultOpen = 1 << 5,
    ImGuiTreeNodeFlags_OpenOnDoubleClick = 1 << 6,
    ImGuiTreeNodeFlags_OpenOnArrow = 1 << 7,
    ImGuiTreeNodeFlags_Leaf = 1 << 8,
    ImGuiTreeNodeFlags_Bullet = 1 << 9,
    ImGuiTreeNodeFlags_FramePadding = 1 << 10,
    ImGuiTreeNodeFlags_SpanAvailWidth = 1 << 11,
    ImGuiTreeNodeFlags_SpanFullWidth = 1 << 12,
    ImGuiTreeNodeFlags_NavLeftJumpsBackHere = 1 << 13,
    ImGuiTreeNodeFlags_CollapsingHeader = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_NoAutoOpenOnLog
}ImGuiTreeNodeFlags_;
typedef enum {
    ImGuiPopupFlags_None = 0,
    ImGuiPopupFlags_MouseButtonLeft = 0,
    ImGuiPopupFlags_MouseButtonRight = 1,
    ImGuiPopupFlags_MouseButtonMiddle = 2,
    ImGuiPopupFlags_MouseButtonMask_ = 0x1F,
    ImGuiPopupFlags_MouseButtonDefault_ = 1,
    ImGuiPopupFlags_NoOpenOverExistingPopup = 1 << 5,
    ImGuiPopupFlags_NoOpenOverItems = 1 << 6,
    ImGuiPopupFlags_AnyPopupId = 1 << 7,
    ImGuiPopupFlags_AnyPopupLevel = 1 << 8,
    ImGuiPopupFlags_AnyPopup = ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel
}ImGuiPopupFlags_;
typedef enum {
    ImGuiSelectableFlags_None = 0,
    ImGuiSelectableFlags_DontClosePopups = 1 << 0,
    ImGuiSelectableFlags_SpanAllColumns = 1 << 1,
    ImGuiSelectableFlags_AllowDoubleClick = 1 << 2,
    ImGuiSelectableFlags_Disabled = 1 << 3,
    ImGuiSelectableFlags_AllowItemOverlap = 1 << 4
}ImGuiSelectableFlags_;
typedef enum {
    ImGuiComboFlags_None = 0,
    ImGuiComboFlags_PopupAlignLeft = 1 << 0,
    ImGuiComboFlags_HeightSmall = 1 << 1,
    ImGuiComboFlags_HeightRegular = 1 << 2,
    ImGuiComboFlags_HeightLarge = 1 << 3,
    ImGuiComboFlags_HeightLargest = 1 << 4,
    ImGuiComboFlags_NoArrowButton = 1 << 5,
    ImGuiComboFlags_NoPreview = 1 << 6,
    ImGuiComboFlags_HeightMask_ = ImGuiComboFlags_HeightSmall | ImGuiComboFlags_HeightRegular | ImGuiComboFlags_HeightLarge | ImGuiComboFlags_HeightLargest
}ImGuiComboFlags_;
typedef enum {
    ImGuiTabBarFlags_None = 0,
    ImGuiTabBarFlags_Reorderable = 1 << 0,
    ImGuiTabBarFlags_AutoSelectNewTabs = 1 << 1,
    ImGuiTabBarFlags_TabListPopupButton = 1 << 2,
    ImGuiTabBarFlags_NoCloseWithMiddleMouseButton = 1 << 3,
    ImGuiTabBarFlags_NoTabListScrollingButtons = 1 << 4,
    ImGuiTabBarFlags_NoTooltip = 1 << 5,
    ImGuiTabBarFlags_FittingPolicyResizeDown = 1 << 6,
    ImGuiTabBarFlags_FittingPolicyScroll = 1 << 7,
    ImGuiTabBarFlags_FittingPolicyMask_ = ImGuiTabBarFlags_FittingPolicyResizeDown | ImGuiTabBarFlags_FittingPolicyScroll,
    ImGuiTabBarFlags_FittingPolicyDefault_ = ImGuiTabBarFlags_FittingPolicyResizeDown
}ImGuiTabBarFlags_;
typedef enum {
    ImGuiTabItemFlags_None = 0,
    ImGuiTabItemFlags_UnsavedDocument = 1 << 0,
    ImGuiTabItemFlags_SetSelected = 1 << 1,
    ImGuiTabItemFlags_NoCloseWithMiddleMouseButton = 1 << 2,
    ImGuiTabItemFlags_NoPushId = 1 << 3,
    ImGuiTabItemFlags_NoTooltip = 1 << 4,
    ImGuiTabItemFlags_NoReorder = 1 << 5,
    ImGuiTabItemFlags_Leading = 1 << 6,
    ImGuiTabItemFlags_Trailing = 1 << 7
}ImGuiTabItemFlags_;
typedef enum {
    ImGuiTableFlags_None = 0,
    ImGuiTableFlags_Resizable = 1 << 0,
    ImGuiTableFlags_Reorderable = 1 << 1,
    ImGuiTableFlags_Hideable = 1 << 2,
    ImGuiTableFlags_Sortable = 1 << 3,
    ImGuiTableFlags_NoSavedSettings = 1 << 4,
    ImGuiTableFlags_ContextMenuInBody = 1 << 5,
    ImGuiTableFlags_RowBg = 1 << 6,
    ImGuiTableFlags_BordersInnerH = 1 << 7,
    ImGuiTableFlags_BordersOuterH = 1 << 8,
    ImGuiTableFlags_BordersInnerV = 1 << 9,
    ImGuiTableFlags_BordersOuterV = 1 << 10,
    ImGuiTableFlags_BordersH = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersOuterH,
    ImGuiTableFlags_BordersV = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuterV,
    ImGuiTableFlags_BordersInner = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersInnerH,
    ImGuiTableFlags_BordersOuter = ImGuiTableFlags_BordersOuterV | ImGuiTableFlags_BordersOuterH,
    ImGuiTableFlags_Borders = ImGuiTableFlags_BordersInner | ImGuiTableFlags_BordersOuter,
    ImGuiTableFlags_NoBordersInBody = 1 << 11,
    ImGuiTableFlags_NoBordersInBodyUntilResize = 1 << 12,
    ImGuiTableFlags_SizingFixedFit = 1 << 13,
    ImGuiTableFlags_SizingFixedSame = 2 << 13,
    ImGuiTableFlags_SizingStretchProp = 3 << 13,
    ImGuiTableFlags_SizingStretchSame = 4 << 13,
    ImGuiTableFlags_NoHostExtendX = 1 << 16,
    ImGuiTableFlags_NoHostExtendY = 1 << 17,
    ImGuiTableFlags_NoKeepColumnsVisible = 1 << 18,
    ImGuiTableFlags_PreciseWidths = 1 << 19,
    ImGuiTableFlags_NoClip = 1 << 20,
    ImGuiTableFlags_PadOuterX = 1 << 21,
    ImGuiTableFlags_NoPadOuterX = 1 << 22,
    ImGuiTableFlags_NoPadInnerX = 1 << 23,
    ImGuiTableFlags_ScrollX = 1 << 24,
    ImGuiTableFlags_ScrollY = 1 << 25,
    ImGuiTableFlags_SortMulti = 1 << 26,
    ImGuiTableFlags_SortTristate = 1 << 27,
    ImGuiTableFlags_SizingMask_ = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_SizingStretchSame
}ImGuiTableFlags_;
typedef enum {
    ImGuiTableColumnFlags_None = 0,
    ImGuiTableColumnFlags_DefaultHide = 1 << 0,
    ImGuiTableColumnFlags_DefaultSort = 1 << 1,
    ImGuiTableColumnFlags_WidthStretch = 1 << 2,
    ImGuiTableColumnFlags_WidthFixed = 1 << 3,
    ImGuiTableColumnFlags_NoResize = 1 << 4,
    ImGuiTableColumnFlags_NoReorder = 1 << 5,
    ImGuiTableColumnFlags_NoHide = 1 << 6,
    ImGuiTableColumnFlags_NoClip = 1 << 7,
    ImGuiTableColumnFlags_NoSort = 1 << 8,
    ImGuiTableColumnFlags_NoSortAscending = 1 << 9,
    ImGuiTableColumnFlags_NoSortDescending = 1 << 10,
    ImGuiTableColumnFlags_NoHeaderWidth = 1 << 11,
    ImGuiTableColumnFlags_PreferSortAscending = 1 << 12,
    ImGuiTableColumnFlags_PreferSortDescending = 1 << 13,
    ImGuiTableColumnFlags_IndentEnable = 1 << 14,
    ImGuiTableColumnFlags_IndentDisable = 1 << 15,
    ImGuiTableColumnFlags_IsEnabled = 1 << 20,
    ImGuiTableColumnFlags_IsVisible = 1 << 21,
    ImGuiTableColumnFlags_IsSorted = 1 << 22,
    ImGuiTableColumnFlags_IsHovered = 1 << 23,
    ImGuiTableColumnFlags_WidthMask_ = ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_WidthFixed,
    ImGuiTableColumnFlags_IndentMask_ = ImGuiTableColumnFlags_IndentEnable | ImGuiTableColumnFlags_IndentDisable,
    ImGuiTableColumnFlags_StatusMask_ = ImGuiTableColumnFlags_IsEnabled | ImGuiTableColumnFlags_IsVisible | ImGuiTableColumnFlags_IsSorted | ImGuiTableColumnFlags_IsHovered,
    ImGuiTableColumnFlags_NoDirectResize_ = 1 << 30
}ImGuiTableColumnFlags_;
typedef enum {
    ImGuiTableRowFlags_None = 0,
    ImGuiTableRowFlags_Headers = 1 << 0
}ImGuiTableRowFlags_;
typedef enum {
    ImGuiTableBgTarget_None = 0,
    ImGuiTableBgTarget_RowBg0 = 1,
    ImGuiTableBgTarget_RowBg1 = 2,
    ImGuiTableBgTarget_CellBg = 3
}ImGuiTableBgTarget_;
typedef enum {
    ImGuiFocusedFlags_None = 0,
    ImGuiFocusedFlags_ChildWindows = 1 << 0,
    ImGuiFocusedFlags_RootWindow = 1 << 1,
    ImGuiFocusedFlags_AnyWindow = 1 << 2,
    ImGuiFocusedFlags_RootAndChildWindows = ImGuiFocusedFlags_RootWindow | ImGuiFocusedFlags_ChildWindows
}ImGuiFocusedFlags_;
typedef enum {
    ImGuiHoveredFlags_None = 0,
    ImGuiHoveredFlags_ChildWindows = 1 << 0,
    ImGuiHoveredFlags_RootWindow = 1 << 1,
    ImGuiHoveredFlags_AnyWindow = 1 << 2,
    ImGuiHoveredFlags_AllowWhenBlockedByPopup = 1 << 3,
    ImGuiHoveredFlags_AllowWhenBlockedByActiveItem = 1 << 5,
    ImGuiHoveredFlags_AllowWhenOverlapped = 1 << 6,
    ImGuiHoveredFlags_AllowWhenDisabled = 1 << 7,
    ImGuiHoveredFlags_RectOnly = ImGuiHoveredFlags_AllowWhenBlockedByPopup | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_AllowWhenOverlapped,
    ImGuiHoveredFlags_RootAndChildWindows = ImGuiHoveredFlags_RootWindow | ImGuiHoveredFlags_ChildWindows
}ImGuiHoveredFlags_;
typedef enum {
    ImGuiDockNodeFlags_None = 0,
    ImGuiDockNodeFlags_KeepAliveOnly = 1 << 0,
    ImGuiDockNodeFlags_NoDockingInCentralNode = 1 << 2,
    ImGuiDockNodeFlags_PassthruCentralNode = 1 << 3,
    ImGuiDockNodeFlags_NoSplit = 1 << 4,
    ImGuiDockNodeFlags_NoResize = 1 << 5,
    ImGuiDockNodeFlags_AutoHideTabBar = 1 << 6
}ImGuiDockNodeFlags_;
typedef enum {
    ImGuiDragDropFlags_None = 0,
    ImGuiDragDropFlags_SourceNoPreviewTooltip = 1 << 0,
    ImGuiDragDropFlags_SourceNoDisableHover = 1 << 1,
    ImGuiDragDropFlags_SourceNoHoldToOpenOthers = 1 << 2,
    ImGuiDragDropFlags_SourceAllowNullID = 1 << 3,
    ImGuiDragDropFlags_SourceExtern = 1 << 4,
    ImGuiDragDropFlags_SourceAutoExpirePayload = 1 << 5,
    ImGuiDragDropFlags_AcceptBeforeDelivery = 1 << 10,
    ImGuiDragDropFlags_AcceptNoDrawDefaultRect = 1 << 11,
    ImGuiDragDropFlags_AcceptNoPreviewTooltip = 1 << 12,
    ImGuiDragDropFlags_AcceptPeekOnly = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect
}ImGuiDragDropFlags_;
typedef enum {
    ImGuiDataType_S8,
    ImGuiDataType_U8,
    ImGuiDataType_S16,
    ImGuiDataType_U16,
    ImGuiDataType_S32,
    ImGuiDataType_U32,
    ImGuiDataType_S64,
    ImGuiDataType_U64,
    ImGuiDataType_Float,
    ImGuiDataType_Double,
    ImGuiDataType_COUNT
}ImGuiDataType_;
typedef enum {
    ImGuiDir_None = -1,
    ImGuiDir_Left = 0,
    ImGuiDir_Right = 1,
    ImGuiDir_Up = 2,
    ImGuiDir_Down = 3,
    ImGuiDir_COUNT
}ImGuiDir_;
typedef enum {
    ImGuiSortDirection_None = 0,
    ImGuiSortDirection_Ascending = 1,
    ImGuiSortDirection_Descending = 2
}ImGuiSortDirection_;
typedef enum {
    ImGuiKey_Tab,
    ImGuiKey_LeftArrow,
    ImGuiKey_RightArrow,
    ImGuiKey_UpArrow,
    ImGuiKey_DownArrow,
    ImGuiKey_PageUp,
    ImGuiKey_PageDown,
    ImGuiKey_Home,
    ImGuiKey_End,
    ImGuiKey_Insert,
    ImGuiKey_Delete,
    ImGuiKey_Backspace,
    ImGuiKey_Space,
    ImGuiKey_Enter,
    ImGuiKey_Escape,
    ImGuiKey_KeyPadEnter,
    ImGuiKey_A,
    ImGuiKey_C,
    ImGuiKey_V,
    ImGuiKey_X,
    ImGuiKey_Y,
    ImGuiKey_Z,
    ImGuiKey_COUNT
}ImGuiKey_;
typedef enum {
    ImGuiKeyModFlags_None = 0,
    ImGuiKeyModFlags_Ctrl = 1 << 0,
    ImGuiKeyModFlags_Shift = 1 << 1,
    ImGuiKeyModFlags_Alt = 1 << 2,
    ImGuiKeyModFlags_Super = 1 << 3
}ImGuiKeyModFlags_;
typedef enum {
    ImGuiNavInput_Activate,
    ImGuiNavInput_Cancel,
    ImGuiNavInput_Input,
    ImGuiNavInput_Menu,
    ImGuiNavInput_DpadLeft,
    ImGuiNavInput_DpadRight,
    ImGuiNavInput_DpadUp,
    ImGuiNavInput_DpadDown,
    ImGuiNavInput_LStickLeft,
    ImGuiNavInput_LStickRight,
    ImGuiNavInput_LStickUp,
    ImGuiNavInput_LStickDown,
    ImGuiNavInput_FocusPrev,
    ImGuiNavInput_FocusNext,
    ImGuiNavInput_TweakSlow,
    ImGuiNavInput_TweakFast,
    ImGuiNavInput_KeyMenu_,
    ImGuiNavInput_KeyLeft_,
    ImGuiNavInput_KeyRight_,
    ImGuiNavInput_KeyUp_,
    ImGuiNavInput_KeyDown_,
    ImGuiNavInput_COUNT,
    ImGuiNavInput_InternalStart_ = ImGuiNavInput_KeyMenu_
}ImGuiNavInput_;
typedef enum {
    ImGuiConfigFlags_None = 0,
    ImGuiConfigFlags_NavEnableKeyboard = 1 << 0,
    ImGuiConfigFlags_NavEnableGamepad = 1 << 1,
    ImGuiConfigFlags_NavEnableSetMousePos = 1 << 2,
    ImGuiConfigFlags_NavNoCaptureKeyboard = 1 << 3,
    ImGuiConfigFlags_NoMouse = 1 << 4,
    ImGuiConfigFlags_NoMouseCursorChange = 1 << 5,
    ImGuiConfigFlags_DockingEnable = 1 << 6,
    ImGuiConfigFlags_ViewportsEnable = 1 << 10,
    ImGuiConfigFlags_DpiEnableScaleViewports= 1 << 14,
    ImGuiConfigFlags_DpiEnableScaleFonts = 1 << 15,
    ImGuiConfigFlags_IsSRGB = 1 << 20,
    ImGuiConfigFlags_IsTouchScreen = 1 << 21
}ImGuiConfigFlags_;
typedef enum {
    ImGuiBackendFlags_None = 0,
    ImGuiBackendFlags_HasGamepad = 1 << 0,
    ImGuiBackendFlags_HasMouseCursors = 1 << 1,
    ImGuiBackendFlags_HasSetMousePos = 1 << 2,
    ImGuiBackendFlags_RendererHasVtxOffset = 1 << 3,
    ImGuiBackendFlags_PlatformHasViewports = 1 << 10,
    ImGuiBackendFlags_HasMouseHoveredViewport=1 << 11,
    ImGuiBackendFlags_RendererHasViewports = 1 << 12
}ImGuiBackendFlags_;
typedef enum {
    ImGuiCol_Text,
    ImGuiCol_TextDisabled,
    ImGuiCol_WindowBg,
    ImGuiCol_ChildBg,
    ImGuiCol_PopupBg,
    ImGuiCol_Border,
    ImGuiCol_BorderShadow,
    ImGuiCol_FrameBg,
    ImGuiCol_FrameBgHovered,
    ImGuiCol_FrameBgActive,
    ImGuiCol_TitleBg,
    ImGuiCol_TitleBgActive,
    ImGuiCol_TitleBgCollapsed,
    ImGuiCol_MenuBarBg,
    ImGuiCol_ScrollbarBg,
    ImGuiCol_ScrollbarGrab,
    ImGuiCol_ScrollbarGrabHovered,
    ImGuiCol_ScrollbarGrabActive,
    ImGuiCol_CheckMark,
    ImGuiCol_SliderGrab,
    ImGuiCol_SliderGrabActive,
    ImGuiCol_Button,
    ImGuiCol_ButtonHovered,
    ImGuiCol_ButtonActive,
    ImGuiCol_Header,
    ImGuiCol_HeaderHovered,
    ImGuiCol_HeaderActive,
    ImGuiCol_Separator,
    ImGuiCol_SeparatorHovered,
    ImGuiCol_SeparatorActive,
    ImGuiCol_ResizeGrip,
    ImGuiCol_ResizeGripHovered,
    ImGuiCol_ResizeGripActive,
    ImGuiCol_Tab,
    ImGuiCol_TabHovered,
    ImGuiCol_TabActive,
    ImGuiCol_TabUnfocused,
    ImGuiCol_TabUnfocusedActive,
    ImGuiCol_DockingPreview,
    ImGuiCol_DockingEmptyBg,
    ImGuiCol_PlotLines,
    ImGuiCol_PlotLinesHovered,
    ImGuiCol_PlotHistogram,
    ImGuiCol_PlotHistogramHovered,
    ImGuiCol_TableHeaderBg,
    ImGuiCol_TableBorderStrong,
    ImGuiCol_TableBorderLight,
    ImGuiCol_TableRowBg,
    ImGuiCol_TableRowBgAlt,
    ImGuiCol_TextSelectedBg,
    ImGuiCol_DragDropTarget,
    ImGuiCol_NavHighlight,
    ImGuiCol_NavWindowingHighlight,
    ImGuiCol_NavWindowingDimBg,
    ImGuiCol_ModalWindowDimBg,
    ImGuiCol_COUNT
}ImGuiCol_;
typedef enum {
    ImGuiStyleVar_Alpha,
    ImGuiStyleVar_WindowPadding,
    ImGuiStyleVar_WindowRounding,
    ImGuiStyleVar_WindowBorderSize,
    ImGuiStyleVar_WindowMinSize,
    ImGuiStyleVar_WindowTitleAlign,
    ImGuiStyleVar_ChildRounding,
    ImGuiStyleVar_ChildBorderSize,
    ImGuiStyleVar_PopupRounding,
    ImGuiStyleVar_PopupBorderSize,
    ImGuiStyleVar_FramePadding,
    ImGuiStyleVar_FrameRounding,
    ImGuiStyleVar_FrameBorderSize,
    ImGuiStyleVar_ItemSpacing,
    ImGuiStyleVar_ItemInnerSpacing,
    ImGuiStyleVar_IndentSpacing,
    ImGuiStyleVar_CellPadding,
    ImGuiStyleVar_ScrollbarSize,
    ImGuiStyleVar_ScrollbarRounding,
    ImGuiStyleVar_GrabMinSize,
    ImGuiStyleVar_GrabRounding,
    ImGuiStyleVar_TabRounding,
    ImGuiStyleVar_ButtonTextAlign,
    ImGuiStyleVar_SelectableTextAlign,
    ImGuiStyleVar_COUNT
}ImGuiStyleVar_;
typedef enum {
    ImGuiButtonFlags_None = 0,
    ImGuiButtonFlags_MouseButtonLeft = 1 << 0,
    ImGuiButtonFlags_MouseButtonRight = 1 << 1,
    ImGuiButtonFlags_MouseButtonMiddle = 1 << 2,
    ImGuiButtonFlags_MouseButtonMask_ = ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle,
    ImGuiButtonFlags_MouseButtonDefault_ = ImGuiButtonFlags_MouseButtonLeft
}ImGuiButtonFlags_;
typedef enum {
    ImGuiColorEditFlags_None = 0,
    ImGuiColorEditFlags_NoAlpha = 1 << 1,
    ImGuiColorEditFlags_NoPicker = 1 << 2,
    ImGuiColorEditFlags_NoOptions = 1 << 3,
    ImGuiColorEditFlags_NoSmallPreview = 1 << 4,
    ImGuiColorEditFlags_NoInputs = 1 << 5,
    ImGuiColorEditFlags_NoTooltip = 1 << 6,
    ImGuiColorEditFlags_NoLabel = 1 << 7,
    ImGuiColorEditFlags_NoSidePreview = 1 << 8,
    ImGuiColorEditFlags_NoDragDrop = 1 << 9,
    ImGuiColorEditFlags_NoBorder = 1 << 10,
    ImGuiColorEditFlags_AlphaBar = 1 << 16,
    ImGuiColorEditFlags_AlphaPreview = 1 << 17,
    ImGuiColorEditFlags_AlphaPreviewHalf= 1 << 18,
    ImGuiColorEditFlags_HDR = 1 << 19,
    ImGuiColorEditFlags_DisplayRGB = 1 << 20,
    ImGuiColorEditFlags_DisplayHSV = 1 << 21,
    ImGuiColorEditFlags_DisplayHex = 1 << 22,
    ImGuiColorEditFlags_Uint8 = 1 << 23,
    ImGuiColorEditFlags_Float = 1 << 24,
    ImGuiColorEditFlags_PickerHueBar = 1 << 25,
    ImGuiColorEditFlags_PickerHueWheel = 1 << 26,
    ImGuiColorEditFlags_InputRGB = 1 << 27,
    ImGuiColorEditFlags_InputHSV = 1 << 28,
    ImGuiColorEditFlags__OptionsDefault = ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar,
    ImGuiColorEditFlags__DisplayMask = ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHSV | ImGuiColorEditFlags_DisplayHex,
    ImGuiColorEditFlags__DataTypeMask = ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_Float,
    ImGuiColorEditFlags__PickerMask = ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_PickerHueBar,
    ImGuiColorEditFlags__InputMask = ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_InputHSV
}ImGuiColorEditFlags_;
typedef enum {
    ImGuiSliderFlags_None = 0,
    ImGuiSliderFlags_AlwaysClamp = 1 << 4,
    ImGuiSliderFlags_Logarithmic = 1 << 5,
    ImGuiSliderFlags_NoRoundToFormat = 1 << 6,
    ImGuiSliderFlags_NoInput = 1 << 7,
    ImGuiSliderFlags_InvalidMask_ = 0x7000000F
}ImGuiSliderFlags_;
typedef enum {
    ImGuiMouseButton_Left = 0,
    ImGuiMouseButton_Right = 1,
    ImGuiMouseButton_Middle = 2,
    ImGuiMouseButton_COUNT = 5
}ImGuiMouseButton_;
typedef enum {
    ImGuiMouseCursor_None = -1,
    ImGuiMouseCursor_Arrow = 0,
    ImGuiMouseCursor_TextInput,
    ImGuiMouseCursor_ResizeAll,
    ImGuiMouseCursor_ResizeNS,
    ImGuiMouseCursor_ResizeEW,
    ImGuiMouseCursor_ResizeNESW,
    ImGuiMouseCursor_ResizeNWSE,
    ImGuiMouseCursor_Hand,
    ImGuiMouseCursor_NotAllowed,
    ImGuiMouseCursor_COUNT
}ImGuiMouseCursor_;
typedef enum {
    ImGuiCond_None = 0,
    ImGuiCond_Always = 1 << 0,
    ImGuiCond_Once = 1 << 1,
    ImGuiCond_FirstUseEver = 1 << 2,
    ImGuiCond_Appearing = 1 << 3
}ImGuiCond_;
struct ImGuiStyle
{
    float Alpha;
    ImVec2 WindowPadding;
    float WindowRounding;
    float WindowBorderSize;
    ImVec2 WindowMinSize;
    ImVec2 WindowTitleAlign;
    ImGuiDir WindowMenuButtonPosition;
    float ChildRounding;
    float ChildBorderSize;
    float PopupRounding;
    float PopupBorderSize;
    ImVec2 FramePadding;
    float FrameRounding;
    float FrameBorderSize;
    ImVec2 ItemSpacing;
    ImVec2 ItemInnerSpacing;
    ImVec2 CellPadding;
    ImVec2 TouchExtraPadding;
    float IndentSpacing;
    float ColumnsMinSpacing;
    float ScrollbarSize;
    float ScrollbarRounding;
    float GrabMinSize;
    float GrabRounding;
    float LogSliderDeadzone;
    float TabRounding;
    float TabBorderSize;
    float TabMinWidthForCloseButton;
    ImGuiDir ColorButtonPosition;
    ImVec2 ButtonTextAlign;
    ImVec2 SelectableTextAlign;
    ImVec2 DisplayWindowPadding;
    ImVec2 DisplaySafeAreaPadding;
    float MouseCursorScale;
    bool AntiAliasedLines;
    bool AntiAliasedLinesUseTex;
    bool AntiAliasedFill;
    float CurveTessellationTol;
    float CircleTessellationMaxError;
    ImVec4 Colors[ImGuiCol_COUNT];
};
struct ImGuiIO
{
    ImGuiConfigFlags ConfigFlags;
    ImGuiBackendFlags BackendFlags;
    ImVec2 DisplaySize;
    float DeltaTime;
    float IniSavingRate;
    const char* IniFilename;
    const char* LogFilename;
    float MouseDoubleClickTime;
    float MouseDoubleClickMaxDist;
    float MouseDragThreshold;
    int KeyMap[ImGuiKey_COUNT];
    float KeyRepeatDelay;
    float KeyRepeatRate;
    void* UserData;
    ImFontAtlas*Fonts;
    float FontGlobalScale;
    bool FontAllowUserScaling;
    ImFont* FontDefault;
    ImVec2 DisplayFramebufferScale;
    bool ConfigDockingNoSplit;
    bool ConfigDockingWithShift;
    bool ConfigDockingAlwaysTabBar;
    bool ConfigDockingTransparentPayload;
    bool ConfigViewportsNoAutoMerge;
    bool ConfigViewportsNoTaskBarIcon;
    bool ConfigViewportsNoDecoration;
    bool ConfigViewportsNoDefaultParent;
    bool MouseDrawCursor;
    bool ConfigMacOSXBehaviors;
    bool ConfigInputTextCursorBlink;
    bool ConfigDragClickToInputText;
    bool ConfigWindowsResizeFromEdges;
    bool ConfigWindowsMoveFromTitleBarOnly;
    float ConfigMemoryCompactTimer;
    const char* BackendPlatformName;
    const char* BackendRendererName;
    void* BackendPlatformUserData;
    void* BackendRendererUserData;
    void* BackendLanguageUserData;
    const char* (*GetClipboardTextFn)(void* user_data);
    void (*SetClipboardTextFn)(void* user_data, const char* text);
    void* ClipboardUserData;
    ImVec2 MousePos;
    bool MouseDown[5];
    float MouseWheel;
    float MouseWheelH;
    ImGuiID MouseHoveredViewport;
    bool KeyCtrl;
    bool KeyShift;
    bool KeyAlt;
    bool KeySuper;
    bool KeysDown[512];
    float NavInputs[ImGuiNavInput_COUNT];
    bool WantCaptureMouse;
    bool WantCaptureKeyboard;
    bool WantTextInput;
    bool WantSetMousePos;
    bool WantSaveIniSettings;
    bool NavActive;
    bool NavVisible;
    float Framerate;
    int MetricsRenderVertices;
    int MetricsRenderIndices;
    int MetricsRenderWindows;
    int MetricsActiveWindows;
    int MetricsActiveAllocations;
    ImVec2 MouseDelta;
    ImGuiKeyModFlags KeyMods;
    ImVec2 MousePosPrev;
    ImVec2 MouseClickedPos[5];
    double MouseClickedTime[5];
    bool MouseClicked[5];
    bool MouseDoubleClicked[5];
    bool MouseReleased[5];
    bool MouseDownOwned[5];
    bool MouseDownWasDoubleClick[5];
    float MouseDownDuration[5];
    float MouseDownDurationPrev[5];
    ImVec2 MouseDragMaxDistanceAbs[5];
    float MouseDragMaxDistanceSqr[5];
    float KeysDownDuration[512];
    float KeysDownDurationPrev[512];
    float NavInputsDownDuration[ImGuiNavInput_COUNT];
    float NavInputsDownDurationPrev[ImGuiNavInput_COUNT];
    float PenPressure;
    ImWchar16 InputQueueSurrogate;
    ImVector_ImWchar InputQueueCharacters;
};
struct ImGuiInputTextCallbackData
{
    ImGuiInputTextFlags EventFlag;
    ImGuiInputTextFlags Flags;
    void* UserData;
    ImWchar EventChar;
    ImGuiKey EventKey;
    char* Buf;
    int BufTextLen;
    int BufSize;
    bool BufDirty;
    int CursorPos;
    int SelectionStart;
    int SelectionEnd;
};
struct ImGuiSizeCallbackData
{
    void* UserData;
    ImVec2 Pos;
    ImVec2 CurrentSize;
    ImVec2 DesiredSize;
};
struct ImGuiWindowClass
{
    ImGuiID ClassId;
    ImGuiID ParentViewportId;
    ImGuiViewportFlags ViewportFlagsOverrideSet;
    ImGuiViewportFlags ViewportFlagsOverrideClear;
    ImGuiTabItemFlags TabItemFlagsOverrideSet;
    ImGuiDockNodeFlags DockNodeFlagsOverrideSet;
    ImGuiDockNodeFlags DockNodeFlagsOverrideClear;
    bool DockingAlwaysTabBar;
    bool DockingAllowUnclassed;
};
struct ImGuiPayload
{
    void* Data;
    int DataSize;
    ImGuiID SourceId;
    ImGuiID SourceParentId;
    int DataFrameCount;
    char DataType[32 + 1];
    bool Preview;
    bool Delivery;
};
struct ImGuiTableColumnSortSpecs
{
    ImGuiID ColumnUserID;
    ImS16 ColumnIndex;
    ImS16 SortOrder;
    ImGuiSortDirection SortDirection : 8;
};
struct ImGuiTableSortSpecs
{
    const ImGuiTableColumnSortSpecs* Specs;
    int SpecsCount;
    bool SpecsDirty;
};
struct ImGuiOnceUponAFrame
{
     int RefFrame;
};
struct ImGuiTextRange
{
        const char* b;
        const char* e;
};
struct ImGuiTextFilter
{
    char InputBuf[256];
    ImVector_ImGuiTextRange Filters;
    int CountGrep;
};
struct ImGuiTextBuffer
{
    ImVector_char Buf;
};
struct ImGuiStoragePair
{
        ImGuiID key;
        union { int val_i; float val_f; void* val_p; };
};
struct ImGuiStorage
{
    ImVector_ImGuiStoragePair Data;
};
typedef struct ImVector_ImGuiTabBar {int Size;int Capacity;ImGuiTabBar* Data;} ImVector_ImGuiTabBar;
typedef struct ImPool_ImGuiTabBar {ImVector_ImGuiTabBar Buf;ImGuiStorage Map;ImPoolIdx FreeIdx;} ImPool_ImGuiTabBar;
typedef struct ImVector_ImGuiTable {int Size;int Capacity;ImGuiTable* Data;} ImVector_ImGuiTable;
typedef struct ImPool_ImGuiTable {ImVector_ImGuiTable Buf;ImGuiStorage Map;ImPoolIdx FreeIdx;} ImPool_ImGuiTable;
struct ImGuiListClipper
{
    int DisplayStart;
    int DisplayEnd;
    int ItemsCount;
    int StepNo;
    int ItemsFrozen;
    float ItemsHeight;
    float StartPosY;
};
struct ImColor
{
    ImVec4 Value;
};
struct ImDrawCmd
{
    ImVec4 ClipRect;
    ImTextureID TextureId;
    unsigned int VtxOffset;
    unsigned int IdxOffset;
    unsigned int ElemCount;
    ImDrawCallback UserCallback;
    void* UserCallbackData;
};
struct ImDrawVert
{
    ImVec2 pos;
    ImVec2 uv;
    ImU32 col;
};
struct ImDrawCmdHeader
{
    ImVec4 ClipRect;
    ImTextureID TextureId;
    unsigned int VtxOffset;
};
struct ImDrawChannel
{
    ImVector_ImDrawCmd _CmdBuffer;
    ImVector_ImDrawIdx _IdxBuffer;
};
struct ImDrawListSplitter
{
    int _Current;
    int _Count;
    ImVector_ImDrawChannel _Channels;
};
typedef enum {
    ImDrawFlags_None = 0,
    ImDrawFlags_Closed = 1 << 0,
    ImDrawFlags_RoundCornersTopLeft = 1 << 4,
    ImDrawFlags_RoundCornersTopRight = 1 << 5,
    ImDrawFlags_RoundCornersBottomLeft = 1 << 6,
    ImDrawFlags_RoundCornersBottomRight = 1 << 7,
    ImDrawFlags_RoundCornersNone = 1 << 8,
    ImDrawFlags_RoundCornersTop = ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersTopRight,
    ImDrawFlags_RoundCornersBottom = ImDrawFlags_RoundCornersBottomLeft | ImDrawFlags_RoundCornersBottomRight,
    ImDrawFlags_RoundCornersLeft = ImDrawFlags_RoundCornersBottomLeft | ImDrawFlags_RoundCornersTopLeft,
    ImDrawFlags_RoundCornersRight = ImDrawFlags_RoundCornersBottomRight | ImDrawFlags_RoundCornersTopRight,
    ImDrawFlags_RoundCornersAll = ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersTopRight | ImDrawFlags_RoundCornersBottomLeft | ImDrawFlags_RoundCornersBottomRight,
    ImDrawFlags_RoundCornersDefault_ = ImDrawFlags_RoundCornersAll,
    ImDrawFlags_RoundCornersMask_ = ImDrawFlags_RoundCornersAll | ImDrawFlags_RoundCornersNone
}ImDrawFlags_;
typedef enum {
    ImDrawListFlags_None = 0,
    ImDrawListFlags_AntiAliasedLines = 1 << 0,
    ImDrawListFlags_AntiAliasedLinesUseTex = 1 << 1,
    ImDrawListFlags_AntiAliasedFill = 1 << 2,
    ImDrawListFlags_AllowVtxOffset = 1 << 3
}ImDrawListFlags_;
struct ImDrawList
{
    ImVector_ImDrawCmd CmdBuffer;
    ImVector_ImDrawIdx IdxBuffer;
    ImVector_ImDrawVert VtxBuffer;
    ImDrawListFlags Flags;
    unsigned int _VtxCurrentIdx;
    const ImDrawListSharedData* _Data;
    const char* _OwnerName;
    ImDrawVert* _VtxWritePtr;
    ImDrawIdx* _IdxWritePtr;
    ImVector_ImVec4 _ClipRectStack;
    ImVector_ImTextureID _TextureIdStack;
    ImVector_ImVec2 _Path;
    ImDrawCmdHeader _CmdHeader;
    ImDrawListSplitter _Splitter;
    float _FringeScale;
};
struct ImDrawData
{
    bool Valid;
    int CmdListsCount;
    int TotalIdxCount;
    int TotalVtxCount;
    ImDrawList** CmdLists;
    ImVec2 DisplayPos;
    ImVec2 DisplaySize;
    ImVec2 FramebufferScale;
    ImGuiViewport* OwnerViewport;
};
struct ImFontConfig
{
    void* FontData;
    int FontDataSize;
    bool FontDataOwnedByAtlas;
    int FontNo;
    float SizePixels;
    int OversampleH;
    int OversampleV;
    bool PixelSnapH;
    ImVec2 GlyphExtraSpacing;
    ImVec2 GlyphOffset;
    const ImWchar* GlyphRanges;
    float GlyphMinAdvanceX;
    float GlyphMaxAdvanceX;
    bool MergeMode;
    unsigned int FontBuilderFlags;
    float RasterizerMultiply;
    ImWchar EllipsisChar;
    char Name[40];
    ImFont* DstFont;
};
struct ImFontGlyph
{
    unsigned int Colored : 1;
    unsigned int Visible : 1;
    unsigned int Codepoint : 30;
    float AdvanceX;
    float X0, Y0, X1, Y1;
    float U0, V0, U1, V1;
};
struct ImFontGlyphRangesBuilder
{
    ImVector_ImU32 UsedChars;
};
struct ImFontAtlasCustomRect
{
    unsigned short Width, Height;
    unsigned short X, Y;
    unsigned int GlyphID;
    float GlyphAdvanceX;
    ImVec2 GlyphOffset;
    ImFont* Font;
};
typedef enum {
    ImFontAtlasFlags_None = 0,
    ImFontAtlasFlags_NoPowerOfTwoHeight = 1 << 0,
    ImFontAtlasFlags_NoMouseCursors = 1 << 1,
    ImFontAtlasFlags_NoBakedLines = 1 << 2
}ImFontAtlasFlags_;
struct ImFontAtlas
{
    ImFontAtlasFlags Flags;
    ImTextureID TexID;
    int TexDesiredWidth;
    int TexGlyphPadding;
    bool Locked;
    bool TexPixelsUseColors;
    unsigned char* TexPixelsAlpha8;
    unsigned int* TexPixelsRGBA32;
    int TexWidth;
    int TexHeight;
    ImVec2 TexUvScale;
    ImVec2 TexUvWhitePixel;
    ImVector_ImFontPtr Fonts;
    ImVector_ImFontAtlasCustomRect CustomRects;
    ImVector_ImFontConfig ConfigData;
    ImVec4 TexUvLines[(63) + 1];
    const ImFontBuilderIO* FontBuilderIO;
    unsigned int FontBuilderFlags;
    int PackIdMouseCursors;
    int PackIdLines;
};
struct ImFont
{
    ImVector_float IndexAdvanceX;
    float FallbackAdvanceX;
    float FontSize;
    ImVector_ImWchar IndexLookup;
    ImVector_ImFontGlyph Glyphs;
    const ImFontGlyph* FallbackGlyph;
    ImFontAtlas* ContainerAtlas;
    const ImFontConfig* ConfigData;
    short ConfigDataCount;
    ImWchar FallbackChar;
    ImWchar EllipsisChar;
    bool DirtyLookupTables;
    float Scale;
    float Ascent, Descent;
    int MetricsTotalSurface;
    ImU8 Used4kPagesMap[(0xFFFF +1)/4096/8];
};
typedef enum {
    ImGuiViewportFlags_None = 0,
    ImGuiViewportFlags_IsPlatformWindow = 1 << 0,
    ImGuiViewportFlags_IsPlatformMonitor = 1 << 1,
    ImGuiViewportFlags_OwnedByApp = 1 << 2,
    ImGuiViewportFlags_NoDecoration = 1 << 3,
    ImGuiViewportFlags_NoTaskBarIcon = 1 << 4,
    ImGuiViewportFlags_NoFocusOnAppearing = 1 << 5,
    ImGuiViewportFlags_NoFocusOnClick = 1 << 6,
    ImGuiViewportFlags_NoInputs = 1 << 7,
    ImGuiViewportFlags_NoRendererClear = 1 << 8,
    ImGuiViewportFlags_TopMost = 1 << 9,
    ImGuiViewportFlags_Minimized = 1 << 10,
    ImGuiViewportFlags_NoAutoMerge = 1 << 11,
    ImGuiViewportFlags_CanHostOtherWindows = 1 << 12
}ImGuiViewportFlags_;
struct ImGuiViewport
{
    ImGuiID ID;
    ImGuiViewportFlags Flags;
    ImVec2 Pos;
    ImVec2 Size;
    ImVec2 WorkPos;
    ImVec2 WorkSize;
    float DpiScale;
    ImGuiID ParentViewportId;
    ImDrawData* DrawData;
    void* RendererUserData;
    void* PlatformUserData;
    void* PlatformHandle;
    void* PlatformHandleRaw;
    bool PlatformRequestMove;
    bool PlatformRequestResize;
    bool PlatformRequestClose;
};
struct ImGuiPlatformIO
{
    void (*Platform_CreateWindow)(ImGuiViewport* vp);
    void (*Platform_DestroyWindow)(ImGuiViewport* vp);
    void (*Platform_ShowWindow)(ImGuiViewport* vp);
    void (*Platform_SetWindowPos)(ImGuiViewport* vp, ImVec2 pos);
    ImVec2 (*Platform_GetWindowPos)(ImGuiViewport* vp);
    void (*Platform_SetWindowSize)(ImGuiViewport* vp, ImVec2 size);
    ImVec2 (*Platform_GetWindowSize)(ImGuiViewport* vp);
    void (*Platform_SetWindowFocus)(ImGuiViewport* vp);
    bool (*Platform_GetWindowFocus)(ImGuiViewport* vp);
    bool (*Platform_GetWindowMinimized)(ImGuiViewport* vp);
    void (*Platform_SetWindowTitle)(ImGuiViewport* vp, const char* str);
    void (*Platform_SetWindowAlpha)(ImGuiViewport* vp, float alpha);
    void (*Platform_UpdateWindow)(ImGuiViewport* vp);
    void (*Platform_RenderWindow)(ImGuiViewport* vp, void* render_arg);
    void (*Platform_SwapBuffers)(ImGuiViewport* vp, void* render_arg);
    float (*Platform_GetWindowDpiScale)(ImGuiViewport* vp);
    void (*Platform_OnChangedViewport)(ImGuiViewport* vp);
    void (*Platform_SetImeInputPos)(ImGuiViewport* vp, ImVec2 pos);
    int (*Platform_CreateVkSurface)(ImGuiViewport* vp, ImU64 vk_inst, const void* vk_allocators, ImU64* out_vk_surface);
    void (*Renderer_CreateWindow)(ImGuiViewport* vp);
    void (*Renderer_DestroyWindow)(ImGuiViewport* vp);
    void (*Renderer_SetWindowSize)(ImGuiViewport* vp, ImVec2 size);
    void (*Renderer_RenderWindow)(ImGuiViewport* vp, void* render_arg);
    void (*Renderer_SwapBuffers)(ImGuiViewport* vp, void* render_arg);
    ImVector_ImGuiPlatformMonitor Monitors;
    ImVector_ImGuiViewportPtr Viewports;
};
struct ImGuiPlatformMonitor
{
    ImVec2 MainPos, MainSize;
    ImVec2 WorkPos, WorkSize;
    float DpiScale;
};
struct StbUndoRecord
{
   int where;
   int insert_length;
   int delete_length;
   int char_storage;
};
struct StbUndoState
{
   StbUndoRecord undo_rec [99];
   ImWchar undo_char[999];
   short undo_point, redo_point;
   int undo_char_point, redo_char_point;
};
struct STB_TexteditState
{
   int cursor;
   int select_start;
   int select_end;
   unsigned char insert_mode;
   int row_count_per_page;
   unsigned char cursor_at_end_of_line;
   unsigned char initialized;
   unsigned char has_preferred_x;
   unsigned char single_line;
   unsigned char padding1, padding2, padding3;
   float preferred_x;
   StbUndoState undostate;
};
struct StbTexteditRow
{
   float x0,x1;
   float baseline_y_delta;
   float ymin,ymax;
   int num_chars;
};
struct ImVec1
{
    float x;
};
struct ImVec2ih
{
    short x, y;
};
struct ImRect
{
    ImVec2 Min;
    ImVec2 Max;
};
struct ImBitVector
{
    ImVector_ImU32 Storage;
};
struct ImDrawListSharedData
{
    ImVec2 TexUvWhitePixel;
    ImFont* Font;
    float FontSize;
    float CurveTessellationTol;
    float CircleSegmentMaxError;
    ImVec4 ClipRectFullscreen;
    ImDrawListFlags InitialFlags;
    ImVec2 ArcFastVtx[48];
    float ArcFastRadiusCutoff;
    ImU8 CircleSegmentCounts[64];
    const ImVec4* TexUvLines;
};
struct ImDrawDataBuilder
{
    ImVector_ImDrawListPtr Layers[2];
};
typedef enum {
    ImGuiItemFlags_None = 0,
    ImGuiItemFlags_NoTabStop = 1 << 0,
    ImGuiItemFlags_ButtonRepeat = 1 << 1,
    ImGuiItemFlags_Disabled = 1 << 2,
    ImGuiItemFlags_NoNav = 1 << 3,
    ImGuiItemFlags_NoNavDefaultFocus = 1 << 4,
    ImGuiItemFlags_SelectableDontClosePopup = 1 << 5,
    ImGuiItemFlags_MixedValue = 1 << 6,
    ImGuiItemFlags_ReadOnly = 1 << 7,
    ImGuiItemFlags_Default_ = 0
}ImGuiItemFlags_;
typedef enum {
    ImGuiItemStatusFlags_None = 0,
    ImGuiItemStatusFlags_HoveredRect = 1 << 0,
    ImGuiItemStatusFlags_HasDisplayRect = 1 << 1,
    ImGuiItemStatusFlags_Edited = 1 << 2,
    ImGuiItemStatusFlags_ToggledSelection = 1 << 3,
    ImGuiItemStatusFlags_ToggledOpen = 1 << 4,
    ImGuiItemStatusFlags_HasDeactivated = 1 << 5,
    ImGuiItemStatusFlags_Deactivated = 1 << 6,
    ImGuiItemStatusFlags_HoveredWindow = 1 << 7
}ImGuiItemStatusFlags_;
typedef enum {
    ImGuiButtonFlags_PressedOnClick = 1 << 4,
    ImGuiButtonFlags_PressedOnClickRelease = 1 << 5,
    ImGuiButtonFlags_PressedOnClickReleaseAnywhere = 1 << 6,
    ImGuiButtonFlags_PressedOnRelease = 1 << 7,
    ImGuiButtonFlags_PressedOnDoubleClick = 1 << 8,
    ImGuiButtonFlags_PressedOnDragDropHold = 1 << 9,
    ImGuiButtonFlags_Repeat = 1 << 10,
    ImGuiButtonFlags_FlattenChildren = 1 << 11,
    ImGuiButtonFlags_AllowItemOverlap = 1 << 12,
    ImGuiButtonFlags_DontClosePopups = 1 << 13,
    ImGuiButtonFlags_Disabled = 1 << 14,
    ImGuiButtonFlags_AlignTextBaseLine = 1 << 15,
    ImGuiButtonFlags_NoKeyModifiers = 1 << 16,
    ImGuiButtonFlags_NoHoldingActiveId = 1 << 17,
    ImGuiButtonFlags_NoNavFocus = 1 << 18,
    ImGuiButtonFlags_NoHoveredOnFocus = 1 << 19,
    ImGuiButtonFlags_PressedOnMask_ = ImGuiButtonFlags_PressedOnClick | ImGuiButtonFlags_PressedOnClickRelease | ImGuiButtonFlags_PressedOnClickReleaseAnywhere | ImGuiButtonFlags_PressedOnRelease | ImGuiButtonFlags_PressedOnDoubleClick | ImGuiButtonFlags_PressedOnDragDropHold,
    ImGuiButtonFlags_PressedOnDefault_ = ImGuiButtonFlags_PressedOnClickRelease
}ImGuiButtonFlagsPrivate_;
typedef enum {
    ImGuiSliderFlags_Vertical = 1 << 20,
    ImGuiSliderFlags_ReadOnly = 1 << 21
}ImGuiSliderFlagsPrivate_;
typedef enum {
    ImGuiSelectableFlags_NoHoldingActiveID = 1 << 20,
    ImGuiSelectableFlags_SelectOnClick = 1 << 21,
    ImGuiSelectableFlags_SelectOnRelease = 1 << 22,
    ImGuiSelectableFlags_SpanAvailWidth = 1 << 23,
    ImGuiSelectableFlags_DrawHoveredWhenHeld = 1 << 24,
    ImGuiSelectableFlags_SetNavIdOnHover = 1 << 25,
    ImGuiSelectableFlags_NoPadWithHalfSpacing = 1 << 26
}ImGuiSelectableFlagsPrivate_;
typedef enum {
    ImGuiTreeNodeFlags_ClipLabelForTrailingButton = 1 << 20
}ImGuiTreeNodeFlagsPrivate_;
typedef enum {
    ImGuiSeparatorFlags_None = 0,
    ImGuiSeparatorFlags_Horizontal = 1 << 0,
    ImGuiSeparatorFlags_Vertical = 1 << 1,
    ImGuiSeparatorFlags_SpanAllColumns = 1 << 2
}ImGuiSeparatorFlags_;
typedef enum {
    ImGuiTextFlags_None = 0,
    ImGuiTextFlags_NoWidthForLargeClippedText = 1 << 0
}ImGuiTextFlags_;
typedef enum {
    ImGuiTooltipFlags_None = 0,
    ImGuiTooltipFlags_OverridePreviousTooltip = 1 << 0
}ImGuiTooltipFlags_;
typedef enum {
    ImGuiLayoutType_Horizontal = 0,
    ImGuiLayoutType_Vertical = 1
}ImGuiLayoutType_;
typedef enum {
    ImGuiLogType_None = 0,
    ImGuiLogType_TTY,
    ImGuiLogType_File,
    ImGuiLogType_Buffer,
    ImGuiLogType_Clipboard
}ImGuiLogType;
typedef enum {
    ImGuiAxis_None = -1,
    ImGuiAxis_X = 0,
    ImGuiAxis_Y = 1
}ImGuiAxis;
typedef enum {
    ImGuiPlotType_Lines,
    ImGuiPlotType_Histogram
}ImGuiPlotType;
typedef enum {
    ImGuiInputSource_None = 0,
    ImGuiInputSource_Mouse,
    ImGuiInputSource_Keyboard,
    ImGuiInputSource_Gamepad,
    ImGuiInputSource_Nav,
    ImGuiInputSource_COUNT
}ImGuiInputSource;
typedef enum {
    ImGuiInputReadMode_Down,
    ImGuiInputReadMode_Pressed,
    ImGuiInputReadMode_Released,
    ImGuiInputReadMode_Repeat,
    ImGuiInputReadMode_RepeatSlow,
    ImGuiInputReadMode_RepeatFast
}ImGuiInputReadMode;
typedef enum {
    ImGuiNavHighlightFlags_None = 0,
    ImGuiNavHighlightFlags_TypeDefault = 1 << 0,
    ImGuiNavHighlightFlags_TypeThin = 1 << 1,
    ImGuiNavHighlightFlags_AlwaysDraw = 1 << 2,
    ImGuiNavHighlightFlags_NoRounding = 1 << 3
}ImGuiNavHighlightFlags_;
typedef enum {
    ImGuiNavDirSourceFlags_None = 0,
    ImGuiNavDirSourceFlags_Keyboard = 1 << 0,
    ImGuiNavDirSourceFlags_PadDPad = 1 << 1,
    ImGuiNavDirSourceFlags_PadLStick = 1 << 2
}ImGuiNavDirSourceFlags_;
typedef enum {
    ImGuiNavMoveFlags_None = 0,
    ImGuiNavMoveFlags_LoopX = 1 << 0,
    ImGuiNavMoveFlags_LoopY = 1 << 1,
    ImGuiNavMoveFlags_WrapX = 1 << 2,
    ImGuiNavMoveFlags_WrapY = 1 << 3,
    ImGuiNavMoveFlags_AllowCurrentNavId = 1 << 4,
    ImGuiNavMoveFlags_AlsoScoreVisibleSet = 1 << 5,
    ImGuiNavMoveFlags_ScrollToEdge = 1 << 6
}ImGuiNavMoveFlags_;
typedef enum {
    ImGuiNavForward_None,
    ImGuiNavForward_ForwardQueued,
    ImGuiNavForward_ForwardActive
}ImGuiNavForward;
typedef enum {
    ImGuiNavLayer_Main = 0,
    ImGuiNavLayer_Menu = 1,
    ImGuiNavLayer_COUNT
}ImGuiNavLayer;
typedef enum {
    ImGuiPopupPositionPolicy_Default,
    ImGuiPopupPositionPolicy_ComboBox,
    ImGuiPopupPositionPolicy_Tooltip
}ImGuiPopupPositionPolicy;
struct ImGuiDataTypeTempStorage
{
    ImU8 Data[8];
};
struct ImGuiDataTypeInfo
{
    size_t Size;
    const char* Name;
    const char* PrintFmt;
    const char* ScanFmt;
};
typedef enum {
    ImGuiDataType_String = ImGuiDataType_COUNT + 1,
    ImGuiDataType_Pointer,
    ImGuiDataType_ID
}ImGuiDataTypePrivate_;
struct ImGuiColorMod
{
    ImGuiCol Col;
    ImVec4 BackupValue;
};
struct ImGuiStyleMod
{
    ImGuiStyleVar VarIdx;
    union { int BackupInt[2]; float BackupFloat[2]; };
};
struct ImGuiGroupData
{
    ImGuiID WindowID;
    ImVec2 BackupCursorPos;
    ImVec2 BackupCursorMaxPos;
    ImVec1 BackupIndent;
    ImVec1 BackupGroupOffset;
    ImVec2 BackupCurrLineSize;
    float BackupCurrLineTextBaseOffset;
    ImGuiID BackupActiveIdIsAlive;
    bool BackupActiveIdPreviousFrameIsAlive;
    bool BackupHoveredIdIsAlive;
    bool EmitItem;
};
struct ImGuiMenuColumns
{
    float Spacing;
    float Width, NextWidth;
    float Pos[3], NextWidths[3];
};
struct ImGuiInputTextState
{
    ImGuiID ID;
    int CurLenW, CurLenA;
    ImVector_ImWchar TextW;
    ImVector_char TextA;
    ImVector_char InitialTextA;
    bool TextAIsValid;
    int BufCapacityA;
    float ScrollX;
    STB_TexteditState Stb;
    float CursorAnim;
    bool CursorFollow;
    bool SelectedAllMouseLock;
    bool Edited;
    ImGuiInputTextFlags UserFlags;
    ImGuiInputTextCallback UserCallback;
    void* UserCallbackData;
};
struct ImGuiPopupData
{
    ImGuiID PopupId;
    ImGuiWindow* Window;
    ImGuiWindow* SourceWindow;
    int OpenFrameCount;
    ImGuiID OpenParentId;
    ImVec2 OpenPopupPos;
    ImVec2 OpenMousePos;
};
struct ImGuiNavMoveResult
{
    ImGuiWindow* Window;
    ImGuiID ID;
    ImGuiID FocusScopeId;
    float DistBox;
    float DistCenter;
    float DistAxial;
    ImRect RectRel;
};
typedef enum {
    ImGuiNextWindowDataFlags_None = 0,
    ImGuiNextWindowDataFlags_HasPos = 1 << 0,
    ImGuiNextWindowDataFlags_HasSize = 1 << 1,
    ImGuiNextWindowDataFlags_HasContentSize = 1 << 2,
    ImGuiNextWindowDataFlags_HasCollapsed = 1 << 3,
    ImGuiNextWindowDataFlags_HasSizeConstraint = 1 << 4,
    ImGuiNextWindowDataFlags_HasFocus = 1 << 5,
    ImGuiNextWindowDataFlags_HasBgAlpha = 1 << 6,
    ImGuiNextWindowDataFlags_HasScroll = 1 << 7,
    ImGuiNextWindowDataFlags_HasViewport = 1 << 8,
    ImGuiNextWindowDataFlags_HasDock = 1 << 9,
    ImGuiNextWindowDataFlags_HasWindowClass = 1 << 10
}ImGuiNextWindowDataFlags_;
struct ImGuiNextWindowData
{
    ImGuiNextWindowDataFlags Flags;
    ImGuiCond PosCond;
    ImGuiCond SizeCond;
    ImGuiCond CollapsedCond;
    ImGuiCond DockCond;
    ImVec2 PosVal;
    ImVec2 PosPivotVal;
    ImVec2 SizeVal;
    ImVec2 ContentSizeVal;
    ImVec2 ScrollVal;
    bool PosUndock;
    bool CollapsedVal;
    ImRect SizeConstraintRect;
    ImGuiSizeCallback SizeCallback;
    void* SizeCallbackUserData;
    float BgAlphaVal;
    ImGuiID ViewportId;
    ImGuiID DockId;
    ImGuiWindowClass WindowClass;
    ImVec2 MenuBarOffsetMinVal;
};
typedef enum {
    ImGuiNextItemDataFlags_None = 0,
    ImGuiNextItemDataFlags_HasWidth = 1 << 0,
    ImGuiNextItemDataFlags_HasOpen = 1 << 1
}ImGuiNextItemDataFlags_;
struct ImGuiNextItemData
{
    ImGuiNextItemDataFlags Flags;
    float Width;
    ImGuiID FocusScopeId;
    ImGuiCond OpenCond;
    bool OpenVal;
};
struct ImGuiShrinkWidthItem
{
    int Index;
    float Width;
};
struct ImGuiPtrOrIndex
{
    void* Ptr;
    int Index;
};
typedef enum {
    ImGuiOldColumnFlags_None = 0,
    ImGuiOldColumnFlags_NoBorder = 1 << 0,
    ImGuiOldColumnFlags_NoResize = 1 << 1,
    ImGuiOldColumnFlags_NoPreserveWidths = 1 << 2,
    ImGuiOldColumnFlags_NoForceWithinWindow = 1 << 3,
    ImGuiOldColumnFlags_GrowParentContentsSize = 1 << 4
}ImGuiOldColumnFlags_;
struct ImGuiOldColumnData
{
    float OffsetNorm;
    float OffsetNormBeforeResize;
    ImGuiOldColumnFlags Flags;
    ImRect ClipRect;
};
struct ImGuiOldColumns
{
    ImGuiID ID;
    ImGuiOldColumnFlags Flags;
    bool IsFirstFrame;
    bool IsBeingResized;
    int Current;
    int Count;
    float OffMinX, OffMaxX;
    float LineMinY, LineMaxY;
    float HostCursorPosY;
    float HostCursorMaxPosX;
    ImRect HostInitialClipRect;
    ImRect HostBackupClipRect;
    ImRect HostBackupParentWorkRect;
    ImVector_ImGuiOldColumnData Columns;
    ImDrawListSplitter Splitter;
};
typedef enum {
    ImGuiDockNodeFlags_DockSpace = 1 << 10,
    ImGuiDockNodeFlags_CentralNode = 1 << 11,
    ImGuiDockNodeFlags_NoTabBar = 1 << 12,
    ImGuiDockNodeFlags_HiddenTabBar = 1 << 13,
    ImGuiDockNodeFlags_NoWindowMenuButton = 1 << 14,
    ImGuiDockNodeFlags_NoCloseButton = 1 << 15,
    ImGuiDockNodeFlags_NoDocking = 1 << 16,
    ImGuiDockNodeFlags_NoDockingSplitMe = 1 << 17,
    ImGuiDockNodeFlags_NoDockingSplitOther = 1 << 18,
    ImGuiDockNodeFlags_NoDockingOverMe = 1 << 19,
    ImGuiDockNodeFlags_NoDockingOverOther = 1 << 20,
    ImGuiDockNodeFlags_NoResizeX = 1 << 21,
    ImGuiDockNodeFlags_NoResizeY = 1 << 22,
    ImGuiDockNodeFlags_SharedFlagsInheritMask_ = ~0,
    ImGuiDockNodeFlags_NoResizeFlagsMask_ = ImGuiDockNodeFlags_NoResize | ImGuiDockNodeFlags_NoResizeX | ImGuiDockNodeFlags_NoResizeY,
    ImGuiDockNodeFlags_LocalFlagsMask_ = ImGuiDockNodeFlags_NoSplit | ImGuiDockNodeFlags_NoResizeFlagsMask_ | ImGuiDockNodeFlags_AutoHideTabBar | ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_CentralNode | ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_HiddenTabBar | ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoCloseButton | ImGuiDockNodeFlags_NoDocking,
    ImGuiDockNodeFlags_LocalFlagsTransferMask_ = ImGuiDockNodeFlags_LocalFlagsMask_ & ~ImGuiDockNodeFlags_DockSpace,
    ImGuiDockNodeFlags_SavedFlagsMask_ = ImGuiDockNodeFlags_NoResizeFlagsMask_ | ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_CentralNode | ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_HiddenTabBar | ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoCloseButton | ImGuiDockNodeFlags_NoDocking
}ImGuiDockNodeFlagsPrivate_;
typedef enum {
    ImGuiDataAuthority_Auto,
    ImGuiDataAuthority_DockNode,
    ImGuiDataAuthority_Window
}ImGuiDataAuthority_;
typedef enum {
    ImGuiDockNodeState_Unknown,
    ImGuiDockNodeState_HostWindowHiddenBecauseSingleWindow,
    ImGuiDockNodeState_HostWindowHiddenBecauseWindowsAreResizing,
    ImGuiDockNodeState_HostWindowVisible
}ImGuiDockNodeState;
struct ImGuiDockNode
{
    ImGuiID ID;
    ImGuiDockNodeFlags SharedFlags;
    ImGuiDockNodeFlags LocalFlags;
    ImGuiDockNodeState State;
    ImGuiDockNode* ParentNode;
    ImGuiDockNode* ChildNodes[2];
    ImVector_ImGuiWindowPtr Windows;
    ImGuiTabBar* TabBar;
    ImVec2 Pos;
    ImVec2 Size;
    ImVec2 SizeRef;
    ImGuiAxis SplitAxis;
    ImGuiWindowClass WindowClass;
    ImGuiWindow* HostWindow;
    ImGuiWindow* VisibleWindow;
    ImGuiDockNode* CentralNode;
    ImGuiDockNode* OnlyNodeWithWindows;
    int LastFrameAlive;
    int LastFrameActive;
    int LastFrameFocused;
    ImGuiID LastFocusedNodeId;
    ImGuiID SelectedTabId;
    ImGuiID WantCloseTabId;
    ImGuiDataAuthority AuthorityForPos :3;
    ImGuiDataAuthority AuthorityForSize :3;
    ImGuiDataAuthority AuthorityForViewport :3;
    bool IsVisible :1;
    bool IsFocused :1;
    bool HasCloseButton :1;
    bool HasWindowMenuButton :1;
    bool WantCloseAll :1;
    bool WantLockSizeOnce :1;
    bool WantMouseMove :1;
    bool WantHiddenTabBarUpdate :1;
    bool WantHiddenTabBarToggle :1;
    bool MarkedForPosSizeWrite :1;
};
typedef enum {
    ImGuiWindowDockStyleCol_Text,
    ImGuiWindowDockStyleCol_Tab,
    ImGuiWindowDockStyleCol_TabHovered,
    ImGuiWindowDockStyleCol_TabActive,
    ImGuiWindowDockStyleCol_TabUnfocused,
    ImGuiWindowDockStyleCol_TabUnfocusedActive,
    ImGuiWindowDockStyleCol_COUNT
}ImGuiWindowDockStyleCol;
struct ImGuiWindowDockStyle
{
    ImU32 Colors[ImGuiWindowDockStyleCol_COUNT];
};
struct ImGuiDockContext
{
    ImGuiStorage Nodes;
    ImVector_ImGuiDockRequest Requests;
    ImVector_ImGuiDockNodeSettings NodesSettings;
    bool WantFullRebuild;
};
struct ImGuiViewportP
{
    ImGuiViewport _ImGuiViewport;
    int Idx;
    int LastFrameActive;
    int LastFrontMostStampCount;
    ImGuiID LastNameHash;
    ImVec2 LastPos;
    float Alpha;
    float LastAlpha;
    short PlatformMonitor;
    bool PlatformWindowCreated;
    ImGuiWindow* Window;
    int DrawListsLastFrame[2];
    ImDrawList* DrawLists[2];
    ImDrawData DrawDataP;
    ImDrawDataBuilder DrawDataBuilder;
    ImVec2 LastPlatformPos;
    ImVec2 LastPlatformSize;
    ImVec2 LastRendererSize;
    ImVec2 WorkOffsetMin;
    ImVec2 WorkOffsetMax;
    ImVec2 CurrWorkOffsetMin;
    ImVec2 CurrWorkOffsetMax;
};
struct ImGuiWindowSettings
{
    ImGuiID ID;
    ImVec2ih Pos;
    ImVec2ih Size;
    ImVec2ih ViewportPos;
    ImGuiID ViewportId;
    ImGuiID DockId;
    ImGuiID ClassId;
    short DockOrder;
    bool Collapsed;
    bool WantApply;
};
struct ImGuiSettingsHandler
{
    const char* TypeName;
    ImGuiID TypeHash;
    void (*ClearAllFn)(ImGuiContext* ctx, ImGuiSettingsHandler* handler);
    void (*ReadInitFn)(ImGuiContext* ctx, ImGuiSettingsHandler* handler);
    void* (*ReadOpenFn)(ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name);
    void (*ReadLineFn)(ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line);
    void (*ApplyAllFn)(ImGuiContext* ctx, ImGuiSettingsHandler* handler);
    void (*WriteAllFn)(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out_buf);
    void* UserData;
};
struct ImGuiMetricsConfig
{
    bool ShowWindowsRects;
    bool ShowWindowsBeginOrder;
    bool ShowTablesRects;
    bool ShowDrawCmdMesh;
    bool ShowDrawCmdBoundingBoxes;
    bool ShowDockingNodes;
    int ShowWindowsRectsType;
    int ShowTablesRectsType;
};
struct ImGuiStackSizes
{
    short SizeOfIDStack;
    short SizeOfColorStack;
    short SizeOfStyleVarStack;
    short SizeOfFontStack;
    short SizeOfFocusScopeStack;
    short SizeOfGroupStack;
    short SizeOfBeginPopupStack;
};
typedef enum { ImGuiContextHookType_NewFramePre, ImGuiContextHookType_NewFramePost, ImGuiContextHookType_EndFramePre, ImGuiContextHookType_EndFramePost, ImGuiContextHookType_RenderPre, ImGuiContextHookType_RenderPost, ImGuiContextHookType_Shutdown, ImGuiContextHookType_PendingRemoval_ }ImGuiContextHookType;
struct ImGuiContextHook
{
    ImGuiID HookId;
    ImGuiContextHookType Type;
    ImGuiID Owner;
    ImGuiContextHookCallback Callback;
    void* UserData;
};
struct ImGuiContext
{
    bool Initialized;
    bool FontAtlasOwnedByContext;
    ImGuiIO IO;
    ImGuiPlatformIO PlatformIO;
    ImGuiStyle Style;
    ImGuiConfigFlags ConfigFlagsCurrFrame;
    ImGuiConfigFlags ConfigFlagsLastFrame;
    ImFont* Font;
    float FontSize;
    float FontBaseSize;
    ImDrawListSharedData DrawListSharedData;
    double Time;
    int FrameCount;
    int FrameCountEnded;
    int FrameCountPlatformEnded;
    int FrameCountRendered;
    bool WithinFrameScope;
    bool WithinFrameScopeWithImplicitWindow;
    bool WithinEndChild;
    bool GcCompactAll;
    bool TestEngineHookItems;
    ImGuiID TestEngineHookIdInfo;
    void* TestEngine;
    ImVector_ImGuiWindowPtr Windows;
    ImVector_ImGuiWindowPtr WindowsFocusOrder;
    ImVector_ImGuiWindowPtr WindowsTempSortBuffer;
    ImVector_ImGuiWindowPtr CurrentWindowStack;
    ImGuiStorage WindowsById;
    int WindowsActiveCount;
    ImGuiWindow* CurrentWindow;
    ImGuiWindow* HoveredWindow;
    ImGuiWindow* HoveredWindowUnderMovingWindow;
    ImGuiDockNode* HoveredDockNode;
    ImGuiWindow* MovingWindow;
    ImGuiWindow* WheelingWindow;
    ImVec2 WheelingWindowRefMousePos;
    float WheelingWindowTimer;
    ImGuiID HoveredId;
    ImGuiID HoveredIdPreviousFrame;
    bool HoveredIdAllowOverlap;
    bool HoveredIdUsingMouseWheel;
    bool HoveredIdPreviousFrameUsingMouseWheel;
    bool HoveredIdDisabled;
    float HoveredIdTimer;
    float HoveredIdNotActiveTimer;
    ImGuiID ActiveId;
    ImGuiID ActiveIdIsAlive;
    float ActiveIdTimer;
    bool ActiveIdIsJustActivated;
    bool ActiveIdAllowOverlap;
    bool ActiveIdNoClearOnFocusLoss;
    bool ActiveIdHasBeenPressedBefore;
    bool ActiveIdHasBeenEditedBefore;
    bool ActiveIdHasBeenEditedThisFrame;
    bool ActiveIdUsingMouseWheel;
    ImU32 ActiveIdUsingNavDirMask;
    ImU32 ActiveIdUsingNavInputMask;
    ImU64 ActiveIdUsingKeyInputMask;
    ImVec2 ActiveIdClickOffset;
    ImGuiWindow* ActiveIdWindow;
    ImGuiInputSource ActiveIdSource;
    int ActiveIdMouseButton;
    ImGuiID ActiveIdPreviousFrame;
    bool ActiveIdPreviousFrameIsAlive;
    bool ActiveIdPreviousFrameHasBeenEditedBefore;
    ImGuiWindow* ActiveIdPreviousFrameWindow;
    ImGuiID LastActiveId;
    float LastActiveIdTimer;
    ImGuiNextWindowData NextWindowData;
    ImGuiNextItemData NextItemData;
    ImVector_ImGuiColorMod ColorStack;
    ImVector_ImGuiStyleMod StyleVarStack;
    ImVector_ImFontPtr FontStack;
    ImVector_ImGuiID FocusScopeStack;
    ImVector_ImGuiItemFlags ItemFlagsStack;
    ImVector_ImGuiGroupData GroupStack;
    ImVector_ImGuiPopupData OpenPopupStack;
    ImVector_ImGuiPopupData BeginPopupStack;
    ImVector_ImGuiViewportPPtr Viewports;
    float CurrentDpiScale;
    ImGuiViewportP* CurrentViewport;
    ImGuiViewportP* MouseViewport;
    ImGuiViewportP* MouseLastHoveredViewport;
    ImGuiID PlatformLastFocusedViewportId;
    ImGuiPlatformMonitor FallbackMonitor;
    int ViewportFrontMostStampCount;
    ImGuiWindow* NavWindow;
    ImGuiID NavId;
    ImGuiID NavFocusScopeId;
    ImGuiID NavActivateId;
    ImGuiID NavActivateDownId;
    ImGuiID NavActivatePressedId;
    ImGuiID NavInputId;
    ImGuiID NavJustTabbedId;
    ImGuiID NavJustMovedToId;
    ImGuiID NavJustMovedToFocusScopeId;
    ImGuiKeyModFlags NavJustMovedToKeyMods;
    ImGuiID NavNextActivateId;
    ImGuiInputSource NavInputSource;
    ImRect NavScoringRect;
    int NavScoringCount;
    ImGuiNavLayer NavLayer;
    int NavIdTabCounter;
    bool NavIdIsAlive;
    bool NavMousePosDirty;
    bool NavDisableHighlight;
    bool NavDisableMouseHover;
    bool NavAnyRequest;
    bool NavInitRequest;
    bool NavInitRequestFromMove;
    ImGuiID NavInitResultId;
    ImRect NavInitResultRectRel;
    bool NavMoveRequest;
    ImGuiNavMoveFlags NavMoveRequestFlags;
    ImGuiNavForward NavMoveRequestForward;
    ImGuiKeyModFlags NavMoveRequestKeyMods;
    ImGuiDir NavMoveDir, NavMoveDirLast;
    ImGuiDir NavMoveClipDir;
    ImGuiNavMoveResult NavMoveResultLocal;
    ImGuiNavMoveResult NavMoveResultLocalVisibleSet;
    ImGuiNavMoveResult NavMoveResultOther;
    ImGuiWindow* NavWrapRequestWindow;
    ImGuiNavMoveFlags NavWrapRequestFlags;
    ImGuiWindow* NavWindowingTarget;
    ImGuiWindow* NavWindowingTargetAnim;
    ImGuiWindow* NavWindowingListWindow;
    float NavWindowingTimer;
    float NavWindowingHighlightAlpha;
    bool NavWindowingToggleLayer;
    ImGuiWindow* TabFocusRequestCurrWindow;
    ImGuiWindow* TabFocusRequestNextWindow;
    int TabFocusRequestCurrCounterRegular;
    int TabFocusRequestCurrCounterTabStop;
    int TabFocusRequestNextCounterRegular;
    int TabFocusRequestNextCounterTabStop;
    bool TabFocusPressed;
    float DimBgRatio;
    ImGuiMouseCursor MouseCursor;
    bool DragDropActive;
    bool DragDropWithinSource;
    bool DragDropWithinTarget;
    ImGuiDragDropFlags DragDropSourceFlags;
    int DragDropSourceFrameCount;
    int DragDropMouseButton;
    ImGuiPayload DragDropPayload;
    ImRect DragDropTargetRect;
    ImGuiID DragDropTargetId;
    ImGuiDragDropFlags DragDropAcceptFlags;
    float DragDropAcceptIdCurrRectSurface;
    ImGuiID DragDropAcceptIdCurr;
    ImGuiID DragDropAcceptIdPrev;
    int DragDropAcceptFrameCount;
    ImGuiID DragDropHoldJustPressedId;
    ImVector_unsigned_char DragDropPayloadBufHeap;
    unsigned char DragDropPayloadBufLocal[16];
    ImGuiTable* CurrentTable;
    ImPool_ImGuiTable Tables;
    ImVector_ImGuiPtrOrIndex CurrentTableStack;
    ImVector_float TablesLastTimeActive;
    ImVector_ImDrawChannel DrawChannelsTempMergeBuffer;
    ImGuiTabBar* CurrentTabBar;
    ImPool_ImGuiTabBar TabBars;
    ImVector_ImGuiPtrOrIndex CurrentTabBarStack;
    ImVector_ImGuiShrinkWidthItem ShrinkWidthBuffer;
    ImVec2 LastValidMousePos;
    ImGuiInputTextState InputTextState;
    ImFont InputTextPasswordFont;
    ImGuiID TempInputId;
    ImGuiColorEditFlags ColorEditOptions;
    float ColorEditLastHue;
    float ColorEditLastSat;
    float ColorEditLastColor[3];
    ImVec4 ColorPickerRef;
    float SliderCurrentAccum;
    bool SliderCurrentAccumDirty;
    bool DragCurrentAccumDirty;
    float DragCurrentAccum;
    float DragSpeedDefaultRatio;
    float ScrollbarClickDeltaToGrabCenter;
    int TooltipOverrideCount;
    float TooltipSlowDelay;
    ImVector_char ClipboardHandlerData;
    ImVector_ImGuiID MenusIdSubmittedThisFrame;
    ImVec2 PlatformImePos;
    ImVec2 PlatformImeLastPos;
    ImGuiViewportP* PlatformImePosViewport;
    char PlatformLocaleDecimalPoint;
    ImGuiDockContext DockContext;
    bool SettingsLoaded;
    float SettingsDirtyTimer;
    ImGuiTextBuffer SettingsIniData;
    ImVector_ImGuiSettingsHandler SettingsHandlers;
    ImChunkStream_ImGuiWindowSettings SettingsWindows;
    ImChunkStream_ImGuiTableSettings SettingsTables;
    ImVector_ImGuiContextHook Hooks;
    ImGuiID HookIdNext;
    bool LogEnabled;
    ImGuiLogType LogType;
    ImFileHandle LogFile;
    ImGuiTextBuffer LogBuffer;
    const char* LogNextPrefix;
    const char* LogNextSuffix;
    float LogLinePosY;
    bool LogLineFirstItem;
    int LogDepthRef;
    int LogDepthToExpand;
    int LogDepthToExpandDefault;
    bool DebugItemPickerActive;
    ImGuiID DebugItemPickerBreakId;
    ImGuiMetricsConfig DebugMetricsConfig;
    float FramerateSecPerFrame[120];
    int FramerateSecPerFrameIdx;
    float FramerateSecPerFrameAccum;
    int WantCaptureMouseNextFrame;
    int WantCaptureKeyboardNextFrame;
    int WantTextInputNextFrame;
    char TempBuffer[1024 * 3 + 1];
};
struct ImGuiWindowTempData
{
    ImVec2 CursorPos;
    ImVec2 CursorPosPrevLine;
    ImVec2 CursorStartPos;
    ImVec2 CursorMaxPos;
    ImVec2 IdealMaxPos;
    ImVec2 CurrLineSize;
    ImVec2 PrevLineSize;
    float CurrLineTextBaseOffset;
    float PrevLineTextBaseOffset;
    ImVec1 Indent;
    ImVec1 ColumnsOffset;
    ImVec1 GroupOffset;
    ImGuiID LastItemId;
    ImGuiItemStatusFlags LastItemStatusFlags;
    ImRect LastItemRect;
    ImRect LastItemDisplayRect;
    ImGuiNavLayer NavLayerCurrent;
    int NavLayerActiveMask;
    int NavLayerActiveMaskNext;
    ImGuiID NavFocusScopeIdCurrent;
    bool NavHideHighlightOneFrame;
    bool NavHasScroll;
    bool MenuBarAppending;
    ImVec2 MenuBarOffset;
    ImGuiMenuColumns MenuColumns;
    int TreeDepth;
    ImU32 TreeJumpToParentOnPopMask;
    ImVector_ImGuiWindowPtr ChildWindows;
    ImGuiStorage* StateStorage;
    ImGuiOldColumns* CurrentColumns;
    int CurrentTableIdx;
    ImGuiLayoutType LayoutType;
    ImGuiLayoutType ParentLayoutType;
    int FocusCounterRegular;
    int FocusCounterTabStop;
    ImGuiItemFlags ItemFlags;
    float ItemWidth;
    float TextWrapPos;
    ImVector_float ItemWidthStack;
    ImVector_float TextWrapPosStack;
    ImGuiStackSizes StackSizesOnBegin;
};
struct ImGuiWindow
{
    char* Name;
    ImGuiID ID;
    ImGuiWindowFlags Flags, FlagsPreviousFrame;
    ImGuiWindowClass WindowClass;
    ImGuiViewportP* Viewport;
    ImGuiID ViewportId;
    ImVec2 ViewportPos;
    int ViewportAllowPlatformMonitorExtend;
    ImVec2 Pos;
    ImVec2 Size;
    ImVec2 SizeFull;
    ImVec2 ContentSize;
    ImVec2 ContentSizeIdeal;
    ImVec2 ContentSizeExplicit;
    ImVec2 WindowPadding;
    float WindowRounding;
    float WindowBorderSize;
    int NameBufLen;
    ImGuiID MoveId;
    ImGuiID ChildId;
    ImVec2 Scroll;
    ImVec2 ScrollMax;
    ImVec2 ScrollTarget;
    ImVec2 ScrollTargetCenterRatio;
    ImVec2 ScrollTargetEdgeSnapDist;
    ImVec2 ScrollbarSizes;
    bool ScrollbarX, ScrollbarY;
    bool ViewportOwned;
    bool Active;
    bool WasActive;
    bool WriteAccessed;
    bool Collapsed;
    bool WantCollapseToggle;
    bool SkipItems;
    bool Appearing;
    bool Hidden;
    bool IsFallbackWindow;
    bool HasCloseButton;
    signed char ResizeBorderHeld;
    short BeginCount;
    short BeginOrderWithinParent;
    short BeginOrderWithinContext;
    ImGuiID PopupId;
    ImS8 AutoFitFramesX, AutoFitFramesY;
    ImS8 AutoFitChildAxises;
    bool AutoFitOnlyGrows;
    ImGuiDir AutoPosLastDirection;
    ImS8 HiddenFramesCanSkipItems;
    ImS8 HiddenFramesCannotSkipItems;
    ImS8 HiddenFramesForRenderOnly;
    ImS8 DisableInputsFrames;
    ImGuiCond SetWindowPosAllowFlags : 8;
    ImGuiCond SetWindowSizeAllowFlags : 8;
    ImGuiCond SetWindowCollapsedAllowFlags : 8;
    ImGuiCond SetWindowDockAllowFlags : 8;
    ImVec2 SetWindowPosVal;
    ImVec2 SetWindowPosPivot;
    ImVector_ImGuiID IDStack;
    ImGuiWindowTempData DC;
    ImRect OuterRectClipped;
    ImRect InnerRect;
    ImRect InnerClipRect;
    ImRect WorkRect;
    ImRect ParentWorkRect;
    ImRect ClipRect;
    ImRect ContentRegionRect;
    ImVec2ih HitTestHoleSize;
    ImVec2ih HitTestHoleOffset;
    int LastFrameActive;
    int LastFrameJustFocused;
    float LastTimeActive;
    float ItemWidthDefault;
    ImGuiStorage StateStorage;
    ImVector_ImGuiOldColumns ColumnsStorage;
    float FontWindowScale;
    float FontDpiScale;
    int SettingsOffset;
    ImDrawList* DrawList;
    ImDrawList DrawListInst;
    ImGuiWindow* ParentWindow;
    ImGuiWindow* RootWindow;
    ImGuiWindow* RootWindowDockTree;
    ImGuiWindow* RootWindowForTitleBarHighlight;
    ImGuiWindow* RootWindowForNav;
    ImGuiWindow* NavLastChildNavWindow;
    ImGuiID NavLastIds[ImGuiNavLayer_COUNT];
    ImRect NavRectRel[ImGuiNavLayer_COUNT];
    int MemoryDrawListIdxCapacity;
    int MemoryDrawListVtxCapacity;
    bool MemoryCompacted;
    bool DockIsActive :1;
    bool DockTabIsVisible :1;
    bool DockTabWantClose :1;
    short DockOrder;
    ImGuiWindowDockStyle DockStyle;
    ImGuiDockNode* DockNode;
    ImGuiDockNode* DockNodeAsHost;
    ImGuiID DockId;
    ImGuiItemStatusFlags DockTabItemStatusFlags;
    ImRect DockTabItemRect;
};
struct ImGuiLastItemDataBackup
{
    ImGuiID LastItemId;
    ImGuiItemStatusFlags LastItemStatusFlags;
    ImRect LastItemRect;
    ImRect LastItemDisplayRect;
};
typedef enum {
    ImGuiTabBarFlags_DockNode = 1 << 20,
    ImGuiTabBarFlags_IsFocused = 1 << 21,
    ImGuiTabBarFlags_SaveSettings = 1 << 22
}ImGuiTabBarFlagsPrivate_;
typedef enum {
    ImGuiTabItemFlags_NoCloseButton = 1 << 20,
    ImGuiTabItemFlags_Button = 1 << 21,
    ImGuiTabItemFlags_Unsorted = 1 << 22,
    ImGuiTabItemFlags_Preview = 1 << 23
}ImGuiTabItemFlagsPrivate_;
struct ImGuiTabItem
{
    ImGuiID ID;
    ImGuiTabItemFlags Flags;
    ImGuiWindow* Window;
    int LastFrameVisible;
    int LastFrameSelected;
    float Offset;
    float Width;
    float ContentWidth;
    ImS16 NameOffset;
    ImS16 BeginOrder;
    ImS16 IndexDuringLayout;
    bool WantClose;
};
struct ImGuiTabBar
{
    ImVector_ImGuiTabItem Tabs;
    ImGuiTabBarFlags Flags;
    ImGuiID ID;
    ImGuiID SelectedTabId;
    ImGuiID NextSelectedTabId;
    ImGuiID VisibleTabId;
    int CurrFrameVisible;
    int PrevFrameVisible;
    ImRect BarRect;
    float CurrTabsContentsHeight;
    float PrevTabsContentsHeight;
    float WidthAllTabs;
    float WidthAllTabsIdeal;
    float ScrollingAnim;
    float ScrollingTarget;
    float ScrollingTargetDistToVisibility;
    float ScrollingSpeed;
    float ScrollingRectMinX;
    float ScrollingRectMaxX;
    ImGuiID ReorderRequestTabId;
    ImS8 ReorderRequestDir;
    ImS8 BeginCount;
    bool WantLayout;
    bool VisibleTabWasSubmitted;
    bool TabsAddedNew;
    ImS16 TabsActiveCount;
    ImS16 LastTabItemIdx;
    float ItemSpacingY;
    ImVec2 FramePadding;
    ImVec2 BackupCursorPos;
    ImGuiTextBuffer TabsNames;
};
struct ImGuiTableColumn
{
    ImGuiTableColumnFlags Flags;
    float WidthGiven;
    float MinX;
    float MaxX;
    float WidthRequest;
    float WidthAuto;
    float StretchWeight;
    float InitStretchWeightOrWidth;
    ImRect ClipRect;
    ImGuiID UserID;
    float WorkMinX;
    float WorkMaxX;
    float ItemWidth;
    float ContentMaxXFrozen;
    float ContentMaxXUnfrozen;
    float ContentMaxXHeadersUsed;
    float ContentMaxXHeadersIdeal;
    ImS16 NameOffset;
    ImGuiTableColumnIdx DisplayOrder;
    ImGuiTableColumnIdx IndexWithinEnabledSet;
    ImGuiTableColumnIdx PrevEnabledColumn;
    ImGuiTableColumnIdx NextEnabledColumn;
    ImGuiTableColumnIdx SortOrder;
    ImGuiTableDrawChannelIdx DrawChannelCurrent;
    ImGuiTableDrawChannelIdx DrawChannelFrozen;
    ImGuiTableDrawChannelIdx DrawChannelUnfrozen;
    bool IsEnabled;
    bool IsEnabledNextFrame;
    bool IsVisibleX;
    bool IsVisibleY;
    bool IsRequestOutput;
    bool IsSkipItems;
    bool IsPreserveWidthAuto;
    ImS8 NavLayerCurrent;
    ImU8 AutoFitQueue;
    ImU8 CannotSkipItemsQueue;
    ImU8 SortDirection : 2;
    ImU8 SortDirectionsAvailCount : 2;
    ImU8 SortDirectionsAvailMask : 4;
    ImU8 SortDirectionsAvailList;
};
struct ImGuiTableCellData
{
    ImU32 BgColor;
    ImGuiTableColumnIdx Column;
};
struct ImGuiTable
{
    ImGuiID ID;
    ImGuiTableFlags Flags;
    void* RawData;
    ImSpan_ImGuiTableColumn Columns;
    ImSpan_ImGuiTableColumnIdx DisplayOrderToIndex;
    ImSpan_ImGuiTableCellData RowCellData;
    ImU64 EnabledMaskByDisplayOrder;
    ImU64 EnabledMaskByIndex;
    ImU64 VisibleMaskByIndex;
    ImU64 RequestOutputMaskByIndex;
    ImGuiTableFlags SettingsLoadedFlags;
    int SettingsOffset;
    int LastFrameActive;
    int ColumnsCount;
    int CurrentRow;
    int CurrentColumn;
    ImS16 InstanceCurrent;
    ImS16 InstanceInteracted;
    float RowPosY1;
    float RowPosY2;
    float RowMinHeight;
    float RowTextBaseline;
    float RowIndentOffsetX;
    ImGuiTableRowFlags RowFlags : 16;
    ImGuiTableRowFlags LastRowFlags : 16;
    int RowBgColorCounter;
    ImU32 RowBgColor[2];
    ImU32 BorderColorStrong;
    ImU32 BorderColorLight;
    float BorderX1;
    float BorderX2;
    float HostIndentX;
    float MinColumnWidth;
    float OuterPaddingX;
    float CellPaddingX;
    float CellPaddingY;
    float CellSpacingX1;
    float CellSpacingX2;
    float LastOuterHeight;
    float LastFirstRowHeight;
    float InnerWidth;
    float ColumnsGivenWidth;
    float ColumnsAutoFitWidth;
    float ResizedColumnNextWidth;
    float ResizeLockMinContentsX2;
    float RefScale;
    ImRect OuterRect;
    ImRect InnerRect;
    ImRect WorkRect;
    ImRect InnerClipRect;
    ImRect BgClipRect;
    ImRect Bg0ClipRectForDrawCmd;
    ImRect Bg2ClipRectForDrawCmd;
    ImRect HostClipRect;
    ImRect HostBackupWorkRect;
    ImRect HostBackupParentWorkRect;
    ImRect HostBackupInnerClipRect;
    ImVec2 HostBackupPrevLineSize;
    ImVec2 HostBackupCurrLineSize;
    ImVec2 HostBackupCursorMaxPos;
    ImVec2 UserOuterSize;
    ImVec1 HostBackupColumnsOffset;
    float HostBackupItemWidth;
    int HostBackupItemWidthStackSize;
    ImGuiWindow* OuterWindow;
    ImGuiWindow* InnerWindow;
    ImGuiTextBuffer ColumnsNames;
    ImDrawListSplitter DrawSplitter;
    ImGuiTableColumnSortSpecs SortSpecsSingle;
    ImVector_ImGuiTableColumnSortSpecs SortSpecsMulti;
    ImGuiTableSortSpecs SortSpecs;
    ImGuiTableColumnIdx SortSpecsCount;
    ImGuiTableColumnIdx ColumnsEnabledCount;
    ImGuiTableColumnIdx ColumnsEnabledFixedCount;
    ImGuiTableColumnIdx DeclColumnsCount;
    ImGuiTableColumnIdx HoveredColumnBody;
    ImGuiTableColumnIdx HoveredColumnBorder;
    ImGuiTableColumnIdx AutoFitSingleColumn;
    ImGuiTableColumnIdx ResizedColumn;
    ImGuiTableColumnIdx LastResizedColumn;
    ImGuiTableColumnIdx HeldHeaderColumn;
    ImGuiTableColumnIdx ReorderColumn;
    ImGuiTableColumnIdx ReorderColumnDir;
    ImGuiTableColumnIdx LeftMostEnabledColumn;
    ImGuiTableColumnIdx RightMostEnabledColumn;
    ImGuiTableColumnIdx LeftMostStretchedColumn;
    ImGuiTableColumnIdx RightMostStretchedColumn;
    ImGuiTableColumnIdx ContextPopupColumn;
    ImGuiTableColumnIdx FreezeRowsRequest;
    ImGuiTableColumnIdx FreezeRowsCount;
    ImGuiTableColumnIdx FreezeColumnsRequest;
    ImGuiTableColumnIdx FreezeColumnsCount;
    ImGuiTableColumnIdx RowCellDataCurrent;
    ImGuiTableDrawChannelIdx DummyDrawChannel;
    ImGuiTableDrawChannelIdx Bg2DrawChannelCurrent;
    ImGuiTableDrawChannelIdx Bg2DrawChannelUnfrozen;
    bool IsLayoutLocked;
    bool IsInsideRow;
    bool IsInitializing;
    bool IsSortSpecsDirty;
    bool IsUsingHeaders;
    bool IsContextPopupOpen;
    bool IsSettingsRequestLoad;
    bool IsSettingsDirty;
    bool IsDefaultDisplayOrder;
    bool IsResetAllRequest;
    bool IsResetDisplayOrderRequest;
    bool IsUnfrozenRows;
    bool IsDefaultSizingPolicy;
    bool MemoryCompacted;
    bool HostSkipItems;
};
struct ImGuiTableColumnSettings
{
    float WidthOrWeight;
    ImGuiID UserID;
    ImGuiTableColumnIdx Index;
    ImGuiTableColumnIdx DisplayOrder;
    ImGuiTableColumnIdx SortOrder;
    ImU8 SortDirection : 2;
    ImU8 IsEnabled : 1;
    ImU8 IsStretch : 1;
};
struct ImGuiTableSettings
{
    ImGuiID ID;
    ImGuiTableFlags SaveFlags;
    float RefScale;
    ImGuiTableColumnIdx ColumnsCount;
    ImGuiTableColumnIdx ColumnsCountMax;
    bool WantApply;
};
struct ImFontBuilderIO
{
    bool (*FontBuilder_Build)(ImFontAtlas* atlas);
};

#endif // !defined(CIMGUI_DEFINE_ENUMS_AND_STRUCTS) && !defined(RIZZ__IMGUI)

typedef struct rizz_api_imgui
{
    ImGuiContext*  (*CreateContext)(ImFontAtlas *shared_font_atlas);
    void           (*DestroyContext)(ImGuiContext *ctx);
    ImGuiContext*  (*GetCurrentContext)(void);
    void           (*SetCurrentContext)(ImGuiContext *ctx);
    ImGuiIO*       (*GetIO)(void);
    ImGuiStyle*    (*GetStyle)(void);
    void           (*NewFrame)(void);
    void           (*EndFrame)(void);
    void           (*Render)(void);
    ImDrawData*    (*GetDrawData)(void);
    void           (*ShowMetricsWindow)(bool *p_open);
    const char*    (*GetVersion)(void);
    void           (*StyleColorsDark)(ImGuiStyle *dst);
    void           (*StyleColorsLight)(ImGuiStyle *dst);
    void           (*StyleColorsClassic)(ImGuiStyle *dst);
    bool           (*Begin)(const char *name, bool *p_open, ImGuiWindowFlags flags);
    void           (*End)(void);
    bool           (*BeginChild_Str)(const char *str_id, const ImVec2 size, bool border, ImGuiWindowFlags flags);
    bool           (*BeginChild_ID)(ImGuiID id, const ImVec2 size, bool border, ImGuiWindowFlags flags);
    void           (*EndChild)(void);
    bool           (*IsWindowAppearing)(void);
    bool           (*IsWindowCollapsed)(void);
    bool           (*IsWindowFocused)(ImGuiFocusedFlags flags);
    bool           (*IsWindowHovered)(ImGuiHoveredFlags flags);
    ImDrawList*    (*GetWindowDrawList)(void);
    float          (*GetWindowDpiScale)(void);
    void           (*GetWindowPos)(ImVec2 *pOut);
    void           (*GetWindowSize)(ImVec2 *pOut);
    float          (*GetWindowWidth)(void);
    float          (*GetWindowHeight)(void);
    ImGuiViewport*  (*GetWindowViewport)(void);
    void           (*SetNextWindowPos)(const ImVec2 pos, ImGuiCond cond, const ImVec2 pivot);
    void           (*SetNextWindowSize)(const ImVec2 size, ImGuiCond cond);
    void           (*SetNextWindowSizeConstraints)(const ImVec2 size_min, const ImVec2 size_max, ImGuiSizeCallback custom_callback, void *custom_callback_data);
    void           (*SetNextWindowContentSize)(const ImVec2 size);
    void           (*SetNextWindowCollapsed)(bool collapsed, ImGuiCond cond);
    void           (*SetNextWindowFocus)(void);
    void           (*SetNextWindowBgAlpha)(float alpha);
    void           (*SetNextWindowViewport)(ImGuiID viewport_id);
    void           (*SetWindowPos_Vec2)(const ImVec2 pos, ImGuiCond cond);
    void           (*SetWindowSize_Vec2)(const ImVec2 size, ImGuiCond cond);
    void           (*SetWindowCollapsed_Bool)(bool collapsed, ImGuiCond cond);
    void           (*SetWindowFocus_Nil)(void);
    void           (*SetWindowFontScale)(float scale);
    void           (*SetWindowPos_Str)(const char *name, const ImVec2 pos, ImGuiCond cond);
    void           (*SetWindowSize_Str)(const char *name, const ImVec2 size, ImGuiCond cond);
    void           (*SetWindowCollapsed_Str)(const char *name, bool collapsed, ImGuiCond cond);
    void           (*SetWindowFocus_Str)(const char *name);
    void           (*GetContentRegionAvail)(ImVec2 *pOut);
    void           (*GetContentRegionMax)(ImVec2 *pOut);
    void           (*GetWindowContentRegionMin)(ImVec2 *pOut);
    void           (*GetWindowContentRegionMax)(ImVec2 *pOut);
    float          (*GetWindowContentRegionWidth)(void);
    float          (*GetScrollX)(void);
    float          (*GetScrollY)(void);
    void           (*SetScrollX_Float)(float scroll_x);
    void           (*SetScrollY_Float)(float scroll_y);
    float          (*GetScrollMaxX)(void);
    float          (*GetScrollMaxY)(void);
    void           (*SetScrollHereX)(float center_x_ratio);
    void           (*SetScrollHereY)(float center_y_ratio);
    void           (*SetScrollFromPosX_Float)(float local_x, float center_x_ratio);
    void           (*SetScrollFromPosY_Float)(float local_y, float center_y_ratio);
    void           (*PushFont)(ImFont *font);
    void           (*PopFont)(void);
    void           (*PushStyleColor_U32)(ImGuiCol idx, ImU32 col);
    void           (*PushStyleColor_Vec4)(ImGuiCol idx, const ImVec4 col);
    void           (*PopStyleColor)(int count);
    void           (*PushStyleVar_Float)(ImGuiStyleVar idx, float val);
    void           (*PushStyleVar_Vec2)(ImGuiStyleVar idx, const ImVec2 val);
    void           (*PopStyleVar)(int count);
    void           (*PushAllowKeyboardFocus)(bool allow_keyboard_focus);
    void           (*PopAllowKeyboardFocus)(void);
    void           (*PushButtonRepeat)(bool repeat);
    void           (*PopButtonRepeat)(void);
    void           (*PushItemWidth)(float item_width);
    void           (*PopItemWidth)(void);
    void           (*SetNextItemWidth)(float item_width);
    float          (*CalcItemWidth)(void);
    void           (*PushTextWrapPos)(float wrap_local_pos_x);
    void           (*PopTextWrapPos)(void);
    ImFont*        (*GetFont)(void);
    float          (*GetFontSize)(void);
    void           (*GetFontTexUvWhitePixel)(ImVec2 *pOut);
    ImU32          (*GetColorU32_Col)(ImGuiCol idx, float alpha_mul);
    ImU32          (*GetColorU32_Vec4)(const ImVec4 col);
    ImU32          (*GetColorU32_U32)(ImU32 col);
    const ImVec4*  (*GetStyleColorVec4)(ImGuiCol idx);
    void           (*Separator)(void);
    void           (*SameLine)(float offset_from_start_x, float spacing);
    void           (*NewLine)(void);
    void           (*Spacing)(void);
    void           (*Dummy)(const ImVec2 size);
    void           (*Indent)(float indent_w);
    void           (*Unindent)(float indent_w);
    void           (*BeginGroup)(void);
    void           (*EndGroup)(void);
    void           (*GetCursorPos)(ImVec2 *pOut);
    float          (*GetCursorPosX)(void);
    float          (*GetCursorPosY)(void);
    void           (*SetCursorPos)(const ImVec2 local_pos);
    void           (*SetCursorPosX)(float local_x);
    void           (*SetCursorPosY)(float local_y);
    void           (*GetCursorStartPos)(ImVec2 *pOut);
    void           (*GetCursorScreenPos)(ImVec2 *pOut);
    void           (*SetCursorScreenPos)(const ImVec2 pos);
    void           (*AlignTextToFramePadding)(void);
    float          (*GetTextLineHeight)(void);
    float          (*GetTextLineHeightWithSpacing)(void);
    float          (*GetFrameHeight)(void);
    float          (*GetFrameHeightWithSpacing)(void);
    void           (*PushID_Str)(const char *str_id);
    void           (*PushID_StrStr)(const char *str_id_begin, const char *str_id_end);
    void           (*PushID_Ptr)(const void *ptr_id);
    void           (*PushID_Int)(int int_id);
    void           (*PopID)(void);
    ImGuiID        (*GetID_Str)(const char *str_id);
    ImGuiID        (*GetID_StrStr)(const char *str_id_begin, const char *str_id_end);
    ImGuiID        (*GetID_Ptr)(const void *ptr_id);
    void           (*TextUnformatted)(const char *text, const char *text_end);
    void           (*Text)(const char *fmt, ...);
    void           (*TextV)(const char *fmt, va_list args);
    void           (*TextColored)(const ImVec4 col, const char *fmt, ...);
    void           (*TextColoredV)(const ImVec4 col, const char *fmt, va_list args);
    void           (*TextDisabled)(const char *fmt, ...);
    void           (*TextDisabledV)(const char *fmt, va_list args);
    void           (*TextWrapped)(const char *fmt, ...);
    void           (*TextWrappedV)(const char *fmt, va_list args);
    void           (*LabelText)(const char *label, const char *fmt, ...);
    void           (*LabelTextV)(const char *label, const char *fmt, va_list args);
    void           (*BulletText)(const char *fmt, ...);
    void           (*BulletTextV)(const char *fmt, va_list args);
    bool           (*Button)(const char *label, const ImVec2 size);
    bool           (*SmallButton)(const char *label);
    bool           (*InvisibleButton)(const char *str_id, const ImVec2 size, ImGuiButtonFlags flags);
    bool           (*ArrowButton)(const char *str_id, ImGuiDir dir);
    void           (*Image)(ImTextureID user_texture_id, const ImVec2 size, const ImVec2 uv0, const ImVec2 uv1, const ImVec4 tint_col, const ImVec4 border_col);
    bool           (*ImageButton)(ImTextureID user_texture_id, const ImVec2 size, const ImVec2 uv0, const ImVec2 uv1, int frame_padding, const ImVec4 bg_col, const ImVec4 tint_col);
    bool           (*Checkbox)(const char *label, bool *v);
    bool           (*CheckboxFlags_IntPtr)(const char *label, int *flags, int flags_value);
    bool           (*CheckboxFlags_UintPtr)(const char *label, unsigned int *flags, unsigned int flags_value);
    bool           (*RadioButton_Bool)(const char *label, bool active);
    bool           (*RadioButton_IntPtr)(const char *label, int *v, int v_button);
    void           (*ProgressBar)(float fraction, const ImVec2 size_arg, const char *overlay);
    void           (*Bullet)(void);
    bool           (*BeginCombo)(const char *label, const char *preview_value, ImGuiComboFlags flags);
    void           (*EndCombo)(void);
    bool           (*Combo_Str_arr)(const char *label, int *current_item, const char * const items[], int items_count, int popup_max_height_in_items);
    bool           (*Combo_Str)(const char *label, int *current_item, const char *items_separated_by_zeros, int popup_max_height_in_items);
    bool           (*Combo_FnBoolPtr)(const char *label, int *current_item, bool (*items_getter)(void *data, int idx, const char **out_text), void *data, int items_count, int popup_max_height_in_items);
    bool           (*DragFloat)(const char *label, float *v, float v_speed, float v_min, float v_max, const char *format, ImGuiSliderFlags flags);
    bool           (*DragFloat2)(const char *label, float v[2], float v_speed, float v_min, float v_max, const char *format, ImGuiSliderFlags flags);
    bool           (*DragFloat3)(const char *label, float v[3], float v_speed, float v_min, float v_max, const char *format, ImGuiSliderFlags flags);
    bool           (*DragFloat4)(const char *label, float v[4], float v_speed, float v_min, float v_max, const char *format, ImGuiSliderFlags flags);
    bool           (*DragFloatRange2)(const char *label, float *v_current_min, float *v_current_max, float v_speed, float v_min, float v_max, const char *format, const char *format_max, ImGuiSliderFlags flags);
    bool           (*DragInt)(const char *label, int *v, float v_speed, int v_min, int v_max, const char *format, ImGuiSliderFlags flags);
    bool           (*DragInt2)(const char *label, int v[2], float v_speed, int v_min, int v_max, const char *format, ImGuiSliderFlags flags);
    bool           (*DragInt3)(const char *label, int v[3], float v_speed, int v_min, int v_max, const char *format, ImGuiSliderFlags flags);
    bool           (*DragInt4)(const char *label, int v[4], float v_speed, int v_min, int v_max, const char *format, ImGuiSliderFlags flags);
    bool           (*DragIntRange2)(const char *label, int *v_current_min, int *v_current_max, float v_speed, int v_min, int v_max, const char *format, const char *format_max, ImGuiSliderFlags flags);
    bool           (*DragScalar)(const char *label, ImGuiDataType data_type, void *p_data, float v_speed, const void *p_min, const void *p_max, const char *format, ImGuiSliderFlags flags);
    bool           (*DragScalarN)(const char *label, ImGuiDataType data_type, void *p_data, int components, float v_speed, const void *p_min, const void *p_max, const char *format, ImGuiSliderFlags flags);
    bool           (*SliderFloat)(const char *label, float *v, float v_min, float v_max, const char *format, ImGuiSliderFlags flags);
    bool           (*SliderFloat2)(const char *label, float v[2], float v_min, float v_max, const char *format, ImGuiSliderFlags flags);
    bool           (*SliderFloat3)(const char *label, float v[3], float v_min, float v_max, const char *format, ImGuiSliderFlags flags);
    bool           (*SliderFloat4)(const char *label, float v[4], float v_min, float v_max, const char *format, ImGuiSliderFlags flags);
    bool           (*SliderAngle)(const char *label, float *v_rad, float v_degrees_min, float v_degrees_max, const char *format, ImGuiSliderFlags flags);
    bool           (*SliderInt)(const char *label, int *v, int v_min, int v_max, const char *format, ImGuiSliderFlags flags);
    bool           (*SliderInt2)(const char *label, int v[2], int v_min, int v_max, const char *format, ImGuiSliderFlags flags);
    bool           (*SliderInt3)(const char *label, int v[3], int v_min, int v_max, const char *format, ImGuiSliderFlags flags);
    bool           (*SliderInt4)(const char *label, int v[4], int v_min, int v_max, const char *format, ImGuiSliderFlags flags);
    bool           (*SliderScalar)(const char *label, ImGuiDataType data_type, void *p_data, const void *p_min, const void *p_max, const char *format, ImGuiSliderFlags flags);
    bool           (*SliderScalarN)(const char *label, ImGuiDataType data_type, void *p_data, int components, const void *p_min, const void *p_max, const char *format, ImGuiSliderFlags flags);
    bool           (*VSliderFloat)(const char *label, const ImVec2 size, float *v, float v_min, float v_max, const char *format, ImGuiSliderFlags flags);
    bool           (*VSliderInt)(const char *label, const ImVec2 size, int *v, int v_min, int v_max, const char *format, ImGuiSliderFlags flags);
    bool           (*VSliderScalar)(const char *label, const ImVec2 size, ImGuiDataType data_type, void *p_data, const void *p_min, const void *p_max, const char *format, ImGuiSliderFlags flags);
    bool           (*InputText)(const char *label, char *buf, size_t buf_size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void *user_data);
    bool           (*InputTextMultiline)(const char *label, char *buf, size_t buf_size, const ImVec2 size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void *user_data);
    bool           (*InputTextWithHint)(const char *label, const char *hint, char *buf, size_t buf_size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void *user_data);
    bool           (*InputFloat)(const char *label, float *v, float step, float step_fast, const char *format, ImGuiInputTextFlags flags);
    bool           (*InputFloat2)(const char *label, float v[2], const char *format, ImGuiInputTextFlags flags);
    bool           (*InputFloat3)(const char *label, float v[3], const char *format, ImGuiInputTextFlags flags);
    bool           (*InputFloat4)(const char *label, float v[4], const char *format, ImGuiInputTextFlags flags);
    bool           (*InputInt)(const char *label, int *v, int step, int step_fast, ImGuiInputTextFlags flags);
    bool           (*InputInt2)(const char *label, int v[2], ImGuiInputTextFlags flags);
    bool           (*InputInt3)(const char *label, int v[3], ImGuiInputTextFlags flags);
    bool           (*InputInt4)(const char *label, int v[4], ImGuiInputTextFlags flags);
    bool           (*InputDouble)(const char *label, double *v, double step, double step_fast, const char *format, ImGuiInputTextFlags flags);
    bool           (*InputScalar)(const char *label, ImGuiDataType data_type, void *p_data, const void *p_step, const void *p_step_fast, const char *format, ImGuiInputTextFlags flags);
    bool           (*InputScalarN)(const char *label, ImGuiDataType data_type, void *p_data, int components, const void *p_step, const void *p_step_fast, const char *format, ImGuiInputTextFlags flags);
    bool           (*ColorEdit3)(const char *label, float col[3], ImGuiColorEditFlags flags);
    bool           (*ColorEdit4)(const char *label, float col[4], ImGuiColorEditFlags flags);
    bool           (*ColorPicker3)(const char *label, float col[3], ImGuiColorEditFlags flags);
    bool           (*ColorPicker4)(const char *label, float col[4], ImGuiColorEditFlags flags, const float *ref_col);
    bool           (*ColorButton)(const char *desc_id, const ImVec4 col, ImGuiColorEditFlags flags, ImVec2 size);
    void           (*SetColorEditOptions)(ImGuiColorEditFlags flags);
    bool           (*TreeNode_Str)(const char *label);
    bool           (*TreeNode_StrStr)(const char *str_id, const char *fmt, ...);
    bool           (*TreeNode_Ptr)(const void *ptr_id, const char *fmt, ...);
    bool           (*TreeNodeV_Str)(const char *str_id, const char *fmt, va_list args);
    bool           (*TreeNodeV_Ptr)(const void *ptr_id, const char *fmt, va_list args);
    bool           (*TreeNodeEx_Str)(const char *label, ImGuiTreeNodeFlags flags);
    bool           (*TreeNodeEx_StrStr)(const char *str_id, ImGuiTreeNodeFlags flags, const char *fmt, ...);
    bool           (*TreeNodeEx_Ptr)(const void *ptr_id, ImGuiTreeNodeFlags flags, const char *fmt, ...);
    bool           (*TreeNodeExV_Str)(const char *str_id, ImGuiTreeNodeFlags flags, const char *fmt, va_list args);
    bool           (*TreeNodeExV_Ptr)(const void *ptr_id, ImGuiTreeNodeFlags flags, const char *fmt, va_list args);
    void           (*TreePush_Str)(const char *str_id);
    void           (*TreePush_Ptr)(const void *ptr_id);
    void           (*TreePop)(void);
    float          (*GetTreeNodeToLabelSpacing)(void);
    bool           (*CollapsingHeader_TreeNodeFlags)(const char *label, ImGuiTreeNodeFlags flags);
    bool           (*CollapsingHeader_BoolPtr)(const char *label, bool *p_visible, ImGuiTreeNodeFlags flags);
    void           (*SetNextItemOpen)(bool is_open, ImGuiCond cond);
    bool           (*Selectable_Bool)(const char *label, bool selected, ImGuiSelectableFlags flags, const ImVec2 size);
    bool           (*Selectable_BoolPtr)(const char *label, bool *p_selected, ImGuiSelectableFlags flags, const ImVec2 size);
    bool           (*BeginListBox)(const char *label, const ImVec2 size);
    void           (*EndListBox)(void);
    bool           (*ListBox_Str_arr)(const char *label, int *current_item, const char * const items[], int items_count, int height_in_items);
    bool           (*ListBox_FnBoolPtr)(const char *label, int *current_item, bool (*items_getter)(void *data, int idx, const char **out_text), void *data, int items_count, int height_in_items);
    void           (*PlotLines_FloatPtr)(const char *label, const float *values, int values_count, int values_offset, const char *overlay_text, float scale_min, float scale_max, ImVec2 graph_size, int stride);
    void           (*PlotLines_FnFloatPtr)(const char *label, float (*values_getter)(void *data, int idx), void *data, int values_count, int values_offset, const char *overlay_text, float scale_min, float scale_max, ImVec2 graph_size);
    void           (*PlotHistogram_FloatPtr)(const char *label, const float *values, int values_count, int values_offset, const char *overlay_text, float scale_min, float scale_max, ImVec2 graph_size, int stride);
    void           (*PlotHistogram_FnFloatPtr)(const char *label, float (*values_getter)(void *data, int idx), void *data, int values_count, int values_offset, const char *overlay_text, float scale_min, float scale_max, ImVec2 graph_size);
    void           (*Value_Bool)(const char *prefix, bool b);
    void           (*Value_Int)(const char *prefix, int v);
    void           (*Value_Uint)(const char *prefix, unsigned int v);
    void           (*Value_Float)(const char *prefix, float v, const char *float_format);
    bool           (*BeginMenuBar)(void);
    void           (*EndMenuBar)(void);
    bool           (*BeginMainMenuBar)(void);
    void           (*EndMainMenuBar)(void);
    bool           (*BeginMenu)(const char *label, bool enabled);
    void           (*EndMenu)(void);
    bool           (*MenuItem_Bool)(const char *label, const char *shortcut, bool selected, bool enabled);
    bool           (*MenuItem_BoolPtr)(const char *label, const char *shortcut, bool *p_selected, bool enabled);
    void           (*BeginTooltip)(void);
    void           (*EndTooltip)(void);
    void           (*SetTooltip)(const char *fmt, ...);
    void           (*SetTooltipV)(const char *fmt, va_list args);
    bool           (*BeginPopup)(const char *str_id, ImGuiWindowFlags flags);
    bool           (*BeginPopupModal)(const char *name, bool *p_open, ImGuiWindowFlags flags);
    void           (*EndPopup)(void);
    void           (*OpenPopup)(const char *str_id, ImGuiPopupFlags popup_flags);
    void           (*OpenPopupOnItemClick)(const char *str_id, ImGuiPopupFlags popup_flags);
    void           (*CloseCurrentPopup)(void);
    bool           (*BeginPopupContextItem)(const char *str_id, ImGuiPopupFlags popup_flags);
    bool           (*BeginPopupContextWindow)(const char *str_id, ImGuiPopupFlags popup_flags);
    bool           (*BeginPopupContextVoid)(const char *str_id, ImGuiPopupFlags popup_flags);
    bool           (*IsPopupOpen_Str)(const char *str_id, ImGuiPopupFlags flags);
    bool           (*BeginTable)(const char *str_id, int column, ImGuiTableFlags flags, const ImVec2 outer_size, float inner_width);
    void           (*EndTable)(void);
    void           (*TableNextRow)(ImGuiTableRowFlags row_flags, float min_row_height);
    bool           (*TableNextColumn)(void);
    bool           (*TableSetColumnIndex)(int column_n);
    void           (*TableSetupColumn)(const char *label, ImGuiTableColumnFlags flags, float init_width_or_weight, ImGuiID user_id);
    void           (*TableSetupScrollFreeze)(int cols, int rows);
    void           (*TableHeadersRow)(void);
    void           (*TableHeader)(const char *label);
    ImGuiTableSortSpecs*  (*TableGetSortSpecs)(void);
    int            (*TableGetColumnCount)(void);
    int            (*TableGetColumnIndex)(void);
    int            (*TableGetRowIndex)(void);
    const char*    (*TableGetColumnName_Int)(int column_n);
    ImGuiTableColumnFlags  (*TableGetColumnFlags)(int column_n);
    void           (*TableSetBgColor)(ImGuiTableBgTarget target, ImU32 color, int column_n);
    void           (*Columns)(int count, const char *id, bool border);
    void           (*NextColumn)(void);
    int            (*GetColumnIndex)(void);
    float          (*GetColumnWidth)(int column_index);
    void           (*SetColumnWidth)(int column_index, float width);
    float          (*GetColumnOffset)(int column_index);
    void           (*SetColumnOffset)(int column_index, float offset_x);
    int            (*GetColumnsCount)(void);
    bool           (*BeginTabBar)(const char *str_id, ImGuiTabBarFlags flags);
    void           (*EndTabBar)(void);
    bool           (*BeginTabItem)(const char *label, bool *p_open, ImGuiTabItemFlags flags);
    void           (*EndTabItem)(void);
    bool           (*TabItemButton)(const char *label, ImGuiTabItemFlags flags);
    void           (*SetTabItemClosed)(const char *tab_or_docked_window_label);
    void           (*DockSpace)(ImGuiID id, const ImVec2 size, ImGuiDockNodeFlags flags, const ImGuiWindowClass *window_class);
    ImGuiID        (*DockSpaceOverViewport)(const ImGuiViewport *viewport, ImGuiDockNodeFlags flags, const ImGuiWindowClass *window_class);
    void           (*SetNextWindowDockID)(ImGuiID dock_id, ImGuiCond cond);
    void           (*SetNextWindowClass)(const ImGuiWindowClass *window_class);
    ImGuiID        (*GetWindowDockID)(void);
    bool           (*IsWindowDocked)(void);
    void           (*LogToTTY)(int auto_open_depth);
    void           (*LogToFile)(int auto_open_depth, const char *filename);
    void           (*LogToClipboard)(int auto_open_depth);
    void           (*LogFinish)(void);
    void           (*LogButtons)(void);
    void           (*LogTextV)(const char *fmt, va_list args);
    bool           (*BeginDragDropSource)(ImGuiDragDropFlags flags);
    bool           (*SetDragDropPayload)(const char *type, const void *data, size_t sz, ImGuiCond cond);
    void           (*EndDragDropSource)(void);
    bool           (*BeginDragDropTarget)(void);
    const ImGuiPayload*  (*AcceptDragDropPayload)(const char *type, ImGuiDragDropFlags flags);
    void           (*EndDragDropTarget)(void);
    const ImGuiPayload*  (*GetDragDropPayload)(void);
    void           (*PushClipRect)(const ImVec2 clip_rect_min, const ImVec2 clip_rect_max, bool intersect_with_current_clip_rect);
    void           (*PopClipRect)(void);
    void           (*SetItemDefaultFocus)(void);
    void           (*SetKeyboardFocusHere)(int offset);
    bool           (*IsItemHovered)(ImGuiHoveredFlags flags);
    bool           (*IsItemActive)(void);
    bool           (*IsItemFocused)(void);
    bool           (*IsItemClicked)(ImGuiMouseButton mouse_button);
    bool           (*IsItemVisible)(void);
    bool           (*IsItemEdited)(void);
    bool           (*IsItemActivated)(void);
    bool           (*IsItemDeactivated)(void);
    bool           (*IsItemDeactivatedAfterEdit)(void);
    bool           (*IsItemToggledOpen)(void);
    bool           (*IsAnyItemHovered)(void);
    bool           (*IsAnyItemActive)(void);
    bool           (*IsAnyItemFocused)(void);
    void           (*GetItemRectMin)(ImVec2 *pOut);
    void           (*GetItemRectMax)(ImVec2 *pOut);
    void           (*GetItemRectSize)(ImVec2 *pOut);
    void           (*SetItemAllowOverlap)(void);
    ImGuiViewport*  (*GetMainViewport)(void);
    bool           (*IsRectVisible_Nil)(const ImVec2 size);
    bool           (*IsRectVisible_Vec2)(const ImVec2 rect_min, const ImVec2 rect_max);
    double         (*GetTime)(void);
    int            (*GetFrameCount)(void);
    ImDrawList*    (*GetBackgroundDrawList_Nil)(void);
    ImDrawList*    (*GetForegroundDrawList_Nil)(void);
    ImDrawList*    (*GetBackgroundDrawList_ViewportPtr)(ImGuiViewport *viewport);
    ImDrawList*    (*GetForegroundDrawList_ViewportPtr)(ImGuiViewport *viewport);
    ImDrawListSharedData*  (*GetDrawListSharedData)(void);
    const char*    (*GetStyleColorName)(ImGuiCol idx);
    void           (*SetStateStorage)(ImGuiStorage *storage);
    ImGuiStorage*  (*GetStateStorage)(void);
    void           (*CalcListClipping)(int items_count, float items_height, int *out_items_display_start, int *out_items_display_end);
    bool           (*BeginChildFrame)(ImGuiID id, const ImVec2 size, ImGuiWindowFlags flags);
    void           (*EndChildFrame)(void);
    void           (*CalcTextSize)(ImVec2 *pOut, const char *text, const char *text_end, bool hide_text_after_double_hash, float wrap_width);
    void           (*ColorConvertU32ToFloat4)(ImVec4 *pOut, ImU32 in);
    ImU32          (*ColorConvertFloat4ToU32)(const ImVec4 in);
    void           (*ColorConvertRGBtoHSV)(float r, float g, float b, float *out_h, float *out_s, float *out_v);
    void           (*ColorConvertHSVtoRGB)(float h, float s, float v, float *out_r, float *out_g, float *out_b);
    int            (*GetKeyIndex)(ImGuiKey imgui_key);
    bool           (*IsKeyDown)(int user_key_index);
    bool           (*IsKeyPressed)(int user_key_index, bool repeat);
    bool           (*IsKeyReleased)(int user_key_index);
    int            (*GetKeyPressedAmount)(int key_index, float repeat_delay, float rate);
    void           (*CaptureKeyboardFromApp)(bool want_capture_keyboard_value);
    bool           (*IsMouseDown)(ImGuiMouseButton button);
    bool           (*IsMouseClicked)(ImGuiMouseButton button, bool repeat);
    bool           (*IsMouseReleased)(ImGuiMouseButton button);
    bool           (*IsMouseDoubleClicked)(ImGuiMouseButton button);
    bool           (*IsMouseHoveringRect)(const ImVec2 r_min, const ImVec2 r_max, bool clip);
    bool           (*IsMousePosValid)(const ImVec2 *mouse_pos);
    bool           (*IsAnyMouseDown)(void);
    void           (*GetMousePos)(ImVec2 *pOut);
    void           (*GetMousePosOnOpeningCurrentPopup)(ImVec2 *pOut);
    bool           (*IsMouseDragging)(ImGuiMouseButton button, float lock_threshold);
    void           (*GetMouseDragDelta)(ImVec2 *pOut, ImGuiMouseButton button, float lock_threshold);
    void           (*ResetMouseDragDelta)(ImGuiMouseButton button);
    ImGuiMouseCursor  (*GetMouseCursor)(void);
    void           (*SetMouseCursor)(ImGuiMouseCursor cursor_type);
    void           (*CaptureMouseFromApp)(bool want_capture_mouse_value);
    const char*    (*GetClipboardText)(void);
    void           (*SetClipboardText)(const char *text);
    void           (*LoadIniSettingsFromDisk)(const char *ini_filename);
    void           (*LoadIniSettingsFromMemory)(const char *ini_data, size_t ini_size);
    void           (*SaveIniSettingsToDisk)(const char *ini_filename);
    const char*    (*SaveIniSettingsToMemory)(size_t *out_ini_size);
    bool           (*DebugCheckVersionAndDataLayout)(const char *version_str, size_t sz_io, size_t sz_style, size_t sz_vec2, size_t sz_vec4, size_t sz_drawvert, size_t sz_drawidx);
    void           (*SetAllocatorFunctions)(ImGuiMemAllocFunc alloc_func, ImGuiMemFreeFunc free_func, void *user_data);
    void           (*GetAllocatorFunctions)(ImGuiMemAllocFunc *p_alloc_func, ImGuiMemFreeFunc *p_free_func, void **p_user_data);
    void*          (*MemAlloc)(size_t size);
    void           (*MemFree)(void *ptr);
    ImGuiPlatformIO*  (*GetPlatformIO)(void);
    void           (*UpdatePlatformWindows)(void);
    void           (*RenderPlatformWindowsDefault)(void *platform_render_arg, void *renderer_render_arg);
    void           (*DestroyPlatformWindows)(void);
    ImGuiViewport*  (*FindViewportByID)(ImGuiID id);
    ImGuiViewport*  (*FindViewportByPlatformHandle)(void *platform_handle);
    ImGuiID        (*ImHashData)(const void *data, size_t data_size, ImU32 seed);
    ImGuiID        (*ImHashStr)(const char *data, size_t data_size, ImU32 seed);
    ImU32          (*ImAlphaBlendColors)(ImU32 col_a, ImU32 col_b);
    bool           (*ImIsPowerOfTwo_Int)(int v);
    bool           (*ImIsPowerOfTwo_U64)(ImU64 v);
    int            (*ImUpperPowerOfTwo)(int v);
    int            (*ImStricmp)(const char *str1, const char *str2);
    int            (*ImStrnicmp)(const char *str1, const char *str2, size_t count);
    void           (*ImStrncpy)(char *dst, const char *src, size_t count);
    char*          (*ImStrdup)(const char *str);
    char*          (*ImStrdupcpy)(char *dst, size_t *p_dst_size, const char *str);
    const char*    (*ImStrchrRange)(const char *str_begin, const char *str_end, char c);
    int            (*ImStrlenW)(const ImWchar *str);
    const char*    (*ImStreolRange)(const char *str, const char *str_end);
    const ImWchar*  (*ImStrbolW)(const ImWchar *buf_mid_line, const ImWchar *buf_begin);
    const char*    (*ImStristr)(const char *haystack, const char *haystack_end, const char *needle, const char *needle_end);
    void           (*ImStrTrimBlanks)(char *str);
    const char*    (*ImStrSkipBlank)(const char *str);
    int            (*ImFormatString)(char *buf, size_t buf_size, const char *fmt, ...);
    int            (*ImFormatStringV)(char *buf, size_t buf_size, const char *fmt, va_list args);
    const char*    (*ImParseFormatFindStart)(const char *format);
    const char*    (*ImParseFormatFindEnd)(const char *format);
    const char*    (*ImParseFormatTrimDecorations)(const char *format, char *buf, size_t buf_size);
    int            (*ImParseFormatPrecision)(const char *format, int default_value);
    bool           (*ImCharIsBlankA)(char c);
    bool           (*ImCharIsBlankW)(unsigned int c);
    int            (*ImTextStrToUtf8)(char *buf, int buf_size, const ImWchar *in_text, const ImWchar *in_text_end);
    int            (*ImTextCharFromUtf8)(unsigned int *out_char, const char *in_text, const char *in_text_end);
    int            (*ImTextStrFromUtf8)(ImWchar *buf, int buf_size, const char *in_text, const char *in_text_end, const char **in_remaining);
    int            (*ImTextCountCharsFromUtf8)(const char *in_text, const char *in_text_end);
    int            (*ImTextCountUtf8BytesFromChar)(const char *in_text, const char *in_text_end);
    int            (*ImTextCountUtf8BytesFromStr)(const ImWchar *in_text, const ImWchar *in_text_end);
    ImFileHandle   (*ImFileOpen)(const char *filename, const char *mode);
    bool           (*ImFileClose)(ImFileHandle file);
    ImU64          (*ImFileGetSize)(ImFileHandle file);
    ImU64          (*ImFileRead)(void *data, ImU64 size, ImU64 count, ImFileHandle file);
    ImU64          (*ImFileWrite)(const void *data, ImU64 size, ImU64 count, ImFileHandle file);
    void*          (*ImFileLoadToMemory)(const char *filename, const char *mode, size_t *out_file_size, int padding_bytes);
    float          (*ImPow_Float)(float x, float y);
    double         (*ImPow_double)(double x, double y);
    float          (*ImLog_Float)(float x);
    double         (*ImLog_double)(double x);
    float          (*ImAbs_Float)(float x);
    double         (*ImAbs_double)(double x);
    float          (*ImSign_Float)(float x);
    double         (*ImSign_double)(double x);
    void           (*ImMin)(ImVec2 *pOut, const ImVec2 lhs, const ImVec2 rhs);
    void           (*ImMax)(ImVec2 *pOut, const ImVec2 lhs, const ImVec2 rhs);
    void           (*ImClamp)(ImVec2 *pOut, const ImVec2 v, const ImVec2 mn, ImVec2 mx);
    void           (*ImLerp_Vec2Float)(ImVec2 *pOut, const ImVec2 a, const ImVec2 b, float t);
    void           (*ImLerp_Vec2Vec2)(ImVec2 *pOut, const ImVec2 a, const ImVec2 b, const ImVec2 t);
    void           (*ImLerp_Vec4)(ImVec4 *pOut, const ImVec4 a, const ImVec4 b, float t);
    float          (*ImSaturate)(float f);
    float          (*ImLengthSqr_Vec2)(const ImVec2 lhs);
    float          (*ImLengthSqr_Vec4)(const ImVec4 lhs);
    float          (*ImInvLength)(const ImVec2 lhs, float fail_value);
    float          (*ImFloor_Float)(float f);
    void           (*ImFloor_Vec2)(ImVec2 *pOut, const ImVec2 v);
    int            (*ImModPositive)(int a, int b);
    float          (*ImDot)(const ImVec2 a, const ImVec2 b);
    void           (*ImRotate)(ImVec2 *pOut, const ImVec2 v, float cos_a, float sin_a);
    float          (*ImLinearSweep)(float current, float target, float speed);
    void           (*ImMul)(ImVec2 *pOut, const ImVec2 lhs, const ImVec2 rhs);
    void           (*ImBezierCubicCalc)(ImVec2 *pOut, const ImVec2 p1, const ImVec2 p2, const ImVec2 p3, const ImVec2 p4, float t);
    void           (*ImBezierCubicClosestPoint)(ImVec2 *pOut, const ImVec2 p1, const ImVec2 p2, const ImVec2 p3, const ImVec2 p4, const ImVec2 p, int num_segments);
    void           (*ImBezierCubicClosestPointCasteljau)(ImVec2 *pOut, const ImVec2 p1, const ImVec2 p2, const ImVec2 p3, const ImVec2 p4, const ImVec2 p, float tess_tol);
    void           (*ImBezierQuadraticCalc)(ImVec2 *pOut, const ImVec2 p1, const ImVec2 p2, const ImVec2 p3, float t);
    void           (*ImLineClosestPoint)(ImVec2 *pOut, const ImVec2 a, const ImVec2 b, const ImVec2 p);
    bool           (*ImTriangleContainsPoint)(const ImVec2 a, const ImVec2 b, const ImVec2 c, const ImVec2 p);
    void           (*ImTriangleClosestPoint)(ImVec2 *pOut, const ImVec2 a, const ImVec2 b, const ImVec2 c, const ImVec2 p);
    void           (*ImTriangleBarycentricCoords)(const ImVec2 a, const ImVec2 b, const ImVec2 c, const ImVec2 p, float *out_u, float *out_v, float *out_w);
    float          (*ImTriangleArea)(const ImVec2 a, const ImVec2 b, const ImVec2 c);
    ImGuiDir       (*ImGetDirQuadrantFromDelta)(float dx, float dy);
    bool           (*ImBitArrayTestBit)(const ImU32 *arr, int n);
    void           (*ImBitArrayClearBit)(ImU32 *arr, int n);
    void           (*ImBitArraySetBit)(ImU32 *arr, int n);
    void           (*ImBitArraySetBitRange)(ImU32 *arr, int n, int n2);
    ImGuiWindow*   (*GetCurrentWindowRead)(void);
    ImGuiWindow*   (*GetCurrentWindow)(void);
    ImGuiWindow*   (*FindWindowByID)(ImGuiID id);
    ImGuiWindow*   (*FindWindowByName)(const char *name);
    void           (*UpdateWindowParentAndRootLinks)(ImGuiWindow *window, ImGuiWindowFlags flags, ImGuiWindow *parent_window);
    void           (*CalcWindowNextAutoFitSize)(ImVec2 *pOut, ImGuiWindow *window);
    bool           (*IsWindowChildOf)(ImGuiWindow *window, ImGuiWindow *potential_parent);
    bool           (*IsWindowAbove)(ImGuiWindow *potential_above, ImGuiWindow *potential_below);
    bool           (*IsWindowNavFocusable)(ImGuiWindow *window);
    void           (*GetWindowAllowedExtentRect)(ImRect *pOut, ImGuiWindow *window);
    void           (*SetWindowPos_WindowPtr)(ImGuiWindow *window, const ImVec2 pos, ImGuiCond cond);
    void           (*SetWindowSize_WindowPtr)(ImGuiWindow *window, const ImVec2 size, ImGuiCond cond);
    void           (*SetWindowCollapsed_WindowPtr)(ImGuiWindow *window, bool collapsed, ImGuiCond cond);
    void           (*SetWindowHitTestHole)(ImGuiWindow *window, const ImVec2 pos, const ImVec2 size);
    void           (*FocusWindow)(ImGuiWindow *window);
    void           (*FocusTopMostWindowUnderOne)(ImGuiWindow *under_this_window, ImGuiWindow *ignore_window);
    void           (*BringWindowToFocusFront)(ImGuiWindow *window);
    void           (*BringWindowToDisplayFront)(ImGuiWindow *window);
    void           (*BringWindowToDisplayBack)(ImGuiWindow *window);
    void           (*SetCurrentFont)(ImFont *font);
    ImFont*        (*GetDefaultFont)(void);
    ImDrawList*    (*GetForegroundDrawList_WindowPtr)(ImGuiWindow *window);
    void           (*Initialize)(ImGuiContext *context);
    void           (*Shutdown)(ImGuiContext *context);
    void           (*UpdateHoveredWindowAndCaptureFlags)(void);
    void           (*StartMouseMovingWindow)(ImGuiWindow *window);
    void           (*StartMouseMovingWindowOrNode)(ImGuiWindow *window, ImGuiDockNode *node, bool undock_floating_node);
    void           (*UpdateMouseMovingWindowNewFrame)(void);
    void           (*UpdateMouseMovingWindowEndFrame)(void);
    ImGuiID        (*AddContextHook)(ImGuiContext *context, const ImGuiContextHook *hook);
    void           (*RemoveContextHook)(ImGuiContext *context, ImGuiID hook_to_remove);
    void           (*CallContextHooks)(ImGuiContext *context, ImGuiContextHookType type);
    void           (*TranslateWindowsInViewport)(ImGuiViewportP *viewport, const ImVec2 old_pos, const ImVec2 new_pos);
    void           (*ScaleWindowsInViewport)(ImGuiViewportP *viewport, float scale);
    void           (*DestroyPlatformWindow)(ImGuiViewportP *viewport);
    const ImGuiPlatformMonitor*  (*GetViewportPlatformMonitor)(ImGuiViewport *viewport);
    void           (*MarkIniSettingsDirty_Nil)(void);
    void           (*MarkIniSettingsDirty_WindowPtr)(ImGuiWindow *window);
    void           (*ClearIniSettings)(void);
    ImGuiWindowSettings*  (*CreateNewWindowSettings)(const char *name);
    ImGuiWindowSettings*  (*FindWindowSettings)(ImGuiID id);
    ImGuiWindowSettings*  (*FindOrCreateWindowSettings)(const char *name);
    ImGuiSettingsHandler*  (*FindSettingsHandler)(const char *type_name);
    void           (*SetNextWindowScroll)(const ImVec2 scroll);
    void           (*SetScrollX_WindowPtr)(ImGuiWindow *window, float scroll_x);
    void           (*SetScrollY_WindowPtr)(ImGuiWindow *window, float scroll_y);
    void           (*SetScrollFromPosX_WindowPtr)(ImGuiWindow *window, float local_x, float center_x_ratio);
    void           (*SetScrollFromPosY_WindowPtr)(ImGuiWindow *window, float local_y, float center_y_ratio);
    void           (*ScrollToBringRectIntoView)(ImVec2 *pOut, ImGuiWindow *window, const ImRect item_rect);
    ImGuiID        (*GetItemID)(void);
    ImGuiItemStatusFlags  (*GetItemStatusFlags)(void);
    ImGuiID        (*GetActiveID)(void);
    ImGuiID        (*GetFocusID)(void);
    ImGuiItemFlags  (*GetItemsFlags)(void);
    void           (*SetActiveID)(ImGuiID id, ImGuiWindow *window);
    void           (*SetFocusID)(ImGuiID id, ImGuiWindow *window);
    void           (*ClearActiveID)(void);
    ImGuiID        (*GetHoveredID)(void);
    void           (*SetHoveredID)(ImGuiID id);
    void           (*KeepAliveID)(ImGuiID id);
    void           (*MarkItemEdited)(ImGuiID id);
    void           (*PushOverrideID)(ImGuiID id);
    ImGuiID        (*GetIDWithSeed)(const char *str_id_begin, const char *str_id_end, ImGuiID seed);
    void           (*ItemSize_Vec2)(const ImVec2 size, float text_baseline_y);
    void           (*ItemSize_Rect)(const ImRect bb, float text_baseline_y);
    bool           (*ItemAdd)(const ImRect bb, ImGuiID id, const ImRect *nav_bb);
    bool           (*ItemHoverable)(const ImRect bb, ImGuiID id);
    bool           (*IsClippedEx)(const ImRect bb, ImGuiID id, bool clip_even_when_logged);
    void           (*SetLastItemData)(ImGuiWindow *window, ImGuiID item_id, ImGuiItemStatusFlags status_flags, const ImRect item_rect);
    bool           (*FocusableItemRegister)(ImGuiWindow *window, ImGuiID id);
    void           (*FocusableItemUnregister)(ImGuiWindow *window);
    void           (*CalcItemSize)(ImVec2 *pOut, ImVec2 size, float default_w, float default_h);
    float          (*CalcWrapWidthForPos)(const ImVec2 pos, float wrap_pos_x);
    void           (*PushMultiItemsWidths)(int components, float width_full);
    void           (*PushItemFlag)(ImGuiItemFlags option, bool enabled);
    void           (*PopItemFlag)(void);
    bool           (*IsItemToggledSelection)(void);
    void           (*GetContentRegionMaxAbs)(ImVec2 *pOut);
    void           (*ShrinkWidths)(ImGuiShrinkWidthItem *items, int count, float width_excess);
    void           (*LogBegin)(ImGuiLogType type, int auto_open_depth);
    void           (*LogToBuffer)(int auto_open_depth);
    void           (*LogRenderedText)(const ImVec2 *ref_pos, const char *text, const char *text_end);
    void           (*LogSetNextTextDecoration)(const char *prefix, const char *suffix);
    bool           (*BeginChildEx)(const char *name, ImGuiID id, const ImVec2 size_arg, bool border, ImGuiWindowFlags flags);
    void           (*OpenPopupEx)(ImGuiID id, ImGuiPopupFlags popup_flags);
    void           (*ClosePopupToLevel)(int remaining, bool restore_focus_to_window_under_popup);
    void           (*ClosePopupsOverWindow)(ImGuiWindow *ref_window, bool restore_focus_to_window_under_popup);
    bool           (*IsPopupOpen_ID)(ImGuiID id, ImGuiPopupFlags popup_flags);
    bool           (*BeginPopupEx)(ImGuiID id, ImGuiWindowFlags extra_flags);
    void           (*BeginTooltipEx)(ImGuiWindowFlags extra_flags, ImGuiTooltipFlags tooltip_flags);
    ImGuiWindow*   (*GetTopMostPopupModal)(void);
    void           (*FindBestWindowPosForPopup)(ImVec2 *pOut, ImGuiWindow *window);
    void           (*FindBestWindowPosForPopupEx)(ImVec2 *pOut, const ImVec2 ref_pos, const ImVec2 size, ImGuiDir *last_dir, const ImRect r_outer, const ImRect r_avoid, ImGuiPopupPositionPolicy policy);
    void           (*NavInitWindow)(ImGuiWindow *window, bool force_reinit);
    bool           (*NavMoveRequestButNoResultYet)(void);
    void           (*NavMoveRequestCancel)(void);
    void           (*NavMoveRequestForward)(ImGuiDir move_dir, ImGuiDir clip_dir, const ImRect bb_rel, ImGuiNavMoveFlags move_flags);
    void           (*NavMoveRequestTryWrapping)(ImGuiWindow *window, ImGuiNavMoveFlags move_flags);
    float          (*GetNavInputAmount)(ImGuiNavInput n, ImGuiInputReadMode mode);
    void           (*GetNavInputAmount2d)(ImVec2 *pOut, ImGuiNavDirSourceFlags dir_sources, ImGuiInputReadMode mode, float slow_factor, float fast_factor);
    int            (*CalcTypematicRepeatAmount)(float t0, float t1, float repeat_delay, float repeat_rate);
    void           (*ActivateItem)(ImGuiID id);
    void           (*SetNavID)(ImGuiID id, int nav_layer, ImGuiID focus_scope_id, const ImRect rect_rel);
    void           (*PushFocusScope)(ImGuiID id);
    void           (*PopFocusScope)(void);
    ImGuiID        (*GetFocusedFocusScope)(void);
    ImGuiID        (*GetFocusScope)(void);
    void           (*SetItemUsingMouseWheel)(void);
    bool           (*IsActiveIdUsingNavDir)(ImGuiDir dir);
    bool           (*IsActiveIdUsingNavInput)(ImGuiNavInput input);
    bool           (*IsActiveIdUsingKey)(ImGuiKey key);
    bool           (*IsMouseDragPastThreshold)(ImGuiMouseButton button, float lock_threshold);
    bool           (*IsKeyPressedMap)(ImGuiKey key, bool repeat);
    bool           (*IsNavInputDown)(ImGuiNavInput n);
    bool           (*IsNavInputTest)(ImGuiNavInput n, ImGuiInputReadMode rm);
    ImGuiKeyModFlags  (*GetMergedKeyModFlags)(void);
    void           (*DockContextInitialize)(ImGuiContext *ctx);
    void           (*DockContextShutdown)(ImGuiContext *ctx);
    void           (*DockContextClearNodes)(ImGuiContext *ctx, ImGuiID root_id, bool clear_settings_refs);
    void           (*DockContextRebuildNodes)(ImGuiContext *ctx);
    void           (*DockContextNewFrameUpdateUndocking)(ImGuiContext *ctx);
    void           (*DockContextNewFrameUpdateDocking)(ImGuiContext *ctx);
    ImGuiID        (*DockContextGenNodeID)(ImGuiContext *ctx);
    void           (*DockContextQueueDock)(ImGuiContext *ctx, ImGuiWindow *target, ImGuiDockNode *target_node, ImGuiWindow *payload, ImGuiDir split_dir, float split_ratio, bool split_outer);
    void           (*DockContextQueueUndockWindow)(ImGuiContext *ctx, ImGuiWindow *window);
    void           (*DockContextQueueUndockNode)(ImGuiContext *ctx, ImGuiDockNode *node);
    bool           (*DockContextCalcDropPosForDocking)(ImGuiWindow *target, ImGuiDockNode *target_node, ImGuiWindow *payload, ImGuiDir split_dir, bool split_outer, ImVec2 *out_pos);
    bool           (*DockNodeBeginAmendTabBar)(ImGuiDockNode *node);
    void           (*DockNodeEndAmendTabBar)(void);
    ImGuiDockNode*  (*DockNodeGetRootNode)(ImGuiDockNode *node);
    int            (*DockNodeGetDepth)(const ImGuiDockNode *node);
    ImGuiDockNode*  (*GetWindowDockNode)(void);
    bool           (*GetWindowAlwaysWantOwnTabBar)(ImGuiWindow *window);
    void           (*BeginDocked)(ImGuiWindow *window, bool *p_open);
    void           (*BeginDockableDragDropSource)(ImGuiWindow *window);
    void           (*BeginDockableDragDropTarget)(ImGuiWindow *window);
    void           (*SetWindowDock)(ImGuiWindow *window, ImGuiID dock_id, ImGuiCond cond);
    void           (*DockBuilderDockWindow)(const char *window_name, ImGuiID node_id);
    ImGuiDockNode*  (*DockBuilderGetNode)(ImGuiID node_id);
    ImGuiDockNode*  (*DockBuilderGetCentralNode)(ImGuiID node_id);
    ImGuiID        (*DockBuilderAddNode)(ImGuiID node_id, ImGuiDockNodeFlags flags);
    void           (*DockBuilderRemoveNode)(ImGuiID node_id);
    void           (*DockBuilderRemoveNodeDockedWindows)(ImGuiID node_id, bool clear_settings_refs);
    void           (*DockBuilderRemoveNodeChildNodes)(ImGuiID node_id);
    void           (*DockBuilderSetNodePos)(ImGuiID node_id, ImVec2 pos);
    void           (*DockBuilderSetNodeSize)(ImGuiID node_id, ImVec2 size);
    ImGuiID        (*DockBuilderSplitNode)(ImGuiID node_id, ImGuiDir split_dir, float size_ratio_for_node_at_dir, ImGuiID *out_id_at_dir, ImGuiID *out_id_at_opposite_dir);
    void           (*DockBuilderCopyDockSpace)(ImGuiID src_dockspace_id, ImGuiID dst_dockspace_id, ImVector_const_charPtr *in_window_remap_pairs);
    void           (*DockBuilderCopyNode)(ImGuiID src_node_id, ImGuiID dst_node_id, ImVector_ImGuiID *out_node_remap_pairs);
    void           (*DockBuilderCopyWindowSettings)(const char *src_name, const char *dst_name);
    void           (*DockBuilderFinish)(ImGuiID node_id);
    bool           (*BeginDragDropTargetCustom)(const ImRect bb, ImGuiID id);
    void           (*ClearDragDrop)(void);
    bool           (*IsDragDropPayloadBeingAccepted)(void);
    void           (*SetWindowClipRectBeforeSetChannel)(ImGuiWindow *window, const ImRect clip_rect);
    void           (*BeginColumns)(const char *str_id, int count, ImGuiOldColumnFlags flags);
    void           (*EndColumns)(void);
    void           (*PushColumnClipRect)(int column_index);
    void           (*PushColumnsBackground)(void);
    void           (*PopColumnsBackground)(void);
    ImGuiID        (*GetColumnsID)(const char *str_id, int count);
    ImGuiOldColumns*  (*FindOrCreateColumns)(ImGuiWindow *window, ImGuiID id);
    float          (*GetColumnOffsetFromNorm)(const ImGuiOldColumns *columns, float offset_norm);
    float          (*GetColumnNormFromOffset)(const ImGuiOldColumns *columns, float offset);
    void           (*TableOpenContextMenu)(int column_n);
    void           (*TableSetColumnEnabled)(int column_n, bool enabled);
    void           (*TableSetColumnWidth)(int column_n, float width);
    void           (*TableSetColumnSortDirection)(int column_n, ImGuiSortDirection sort_direction, bool append_to_sort_specs);
    int            (*TableGetHoveredColumn)(void);
    float          (*TableGetHeaderRowHeight)(void);
    void           (*TablePushBackgroundChannel)(void);
    void           (*TablePopBackgroundChannel)(void);
    ImGuiTable*    (*GetCurrentTable)(void);
    ImGuiTable*    (*TableFindByID)(ImGuiID id);
    bool           (*BeginTableEx)(const char *name, ImGuiID id, int columns_count, ImGuiTableFlags flags, const ImVec2 outer_size, float inner_width);
    void           (*TableBeginInitMemory)(ImGuiTable *table, int columns_count);
    void           (*TableBeginApplyRequests)(ImGuiTable *table);
    void           (*TableSetupDrawChannels)(ImGuiTable *table);
    void           (*TableUpdateLayout)(ImGuiTable *table);
    void           (*TableUpdateBorders)(ImGuiTable *table);
    void           (*TableUpdateColumnsWeightFromWidth)(ImGuiTable *table);
    void           (*TableDrawBorders)(ImGuiTable *table);
    void           (*TableDrawContextMenu)(ImGuiTable *table);
    void           (*TableMergeDrawChannels)(ImGuiTable *table);
    void           (*TableSortSpecsSanitize)(ImGuiTable *table);
    void           (*TableSortSpecsBuild)(ImGuiTable *table);
    ImGuiSortDirection  (*TableGetColumnNextSortDirection)(ImGuiTableColumn *column);
    void           (*TableFixColumnSortDirection)(ImGuiTable *table, ImGuiTableColumn *column);
    float          (*TableGetColumnWidthAuto)(ImGuiTable *table, ImGuiTableColumn *column);
    void           (*TableBeginRow)(ImGuiTable *table);
    void           (*TableEndRow)(ImGuiTable *table);
    void           (*TableBeginCell)(ImGuiTable *table, int column_n);
    void           (*TableEndCell)(ImGuiTable *table);
    void           (*TableGetCellBgRect)(ImRect *pOut, const ImGuiTable *table, int column_n);
    const char*    (*TableGetColumnName_TablePtr)(const ImGuiTable *table, int column_n);
    ImGuiID        (*TableGetColumnResizeID)(const ImGuiTable *table, int column_n, int instance_no);
    float          (*TableGetMaxColumnWidth)(const ImGuiTable *table, int column_n);
    void           (*TableSetColumnWidthAutoSingle)(ImGuiTable *table, int column_n);
    void           (*TableSetColumnWidthAutoAll)(ImGuiTable *table);
    void           (*TableRemove)(ImGuiTable *table);
    void           (*TableGcCompactTransientBuffers)(ImGuiTable *table);
    void           (*TableGcCompactSettings)(void);
    void           (*TableLoadSettings)(ImGuiTable *table);
    void           (*TableSaveSettings)(ImGuiTable *table);
    void           (*TableResetSettings)(ImGuiTable *table);
    ImGuiTableSettings*  (*TableGetBoundSettings)(ImGuiTable *table);
    void           (*TableSettingsInstallHandler)(ImGuiContext *context);
    ImGuiTableSettings*  (*TableSettingsCreate)(ImGuiID id, int columns_count);
    ImGuiTableSettings*  (*TableSettingsFindByID)(ImGuiID id);
    bool           (*BeginTabBarEx)(ImGuiTabBar *tab_bar, const ImRect bb, ImGuiTabBarFlags flags, ImGuiDockNode *dock_node);
    ImGuiTabItem*  (*TabBarFindTabByID)(ImGuiTabBar *tab_bar, ImGuiID tab_id);
    ImGuiTabItem*  (*TabBarFindMostRecentlySelectedTabForActiveWindow)(ImGuiTabBar *tab_bar);
    void           (*TabBarAddTab)(ImGuiTabBar *tab_bar, ImGuiTabItemFlags tab_flags, ImGuiWindow *window);
    void           (*TabBarRemoveTab)(ImGuiTabBar *tab_bar, ImGuiID tab_id);
    void           (*TabBarCloseTab)(ImGuiTabBar *tab_bar, ImGuiTabItem *tab);
    void           (*TabBarQueueReorder)(ImGuiTabBar *tab_bar, const ImGuiTabItem *tab, int dir);
    bool           (*TabBarProcessReorder)(ImGuiTabBar *tab_bar);
    bool           (*TabItemEx)(ImGuiTabBar *tab_bar, const char *label, bool *p_open, ImGuiTabItemFlags flags, ImGuiWindow *docked_window);
    void           (*TabItemCalcSize)(ImVec2 *pOut, const char *label, bool has_close_button);
    void           (*TabItemBackground)(ImDrawList *draw_list, const ImRect bb, ImGuiTabItemFlags flags, ImU32 col);
    void           (*TabItemLabelAndCloseButton)(ImDrawList *draw_list, const ImRect bb, ImGuiTabItemFlags flags, ImVec2 frame_padding, const char *label, ImGuiID tab_id, ImGuiID close_button_id, bool is_contents_visible, bool *out_just_closed, bool *out_text_clipped);
    void           (*RenderText)(ImVec2 pos, const char *text, const char *text_end, bool hide_text_after_hash);
    void           (*RenderTextWrapped)(ImVec2 pos, const char *text, const char *text_end, float wrap_width);
    void           (*RenderTextClipped)(const ImVec2 pos_min, const ImVec2 pos_max, const char *text, const char *text_end, const ImVec2 *text_size_if_known, const ImVec2 align, const ImRect *clip_rect);
    void           (*RenderTextClippedEx)(ImDrawList *draw_list, const ImVec2 pos_min, const ImVec2 pos_max, const char *text, const char *text_end, const ImVec2 *text_size_if_known, const ImVec2 align, const ImRect *clip_rect);
    void           (*RenderTextEllipsis)(ImDrawList *draw_list, const ImVec2 pos_min, const ImVec2 pos_max, float clip_max_x, float ellipsis_max_x, const char *text, const char *text_end, const ImVec2 *text_size_if_known);
    void           (*RenderFrame)(ImVec2 p_min, ImVec2 p_max, ImU32 fill_col, bool border, float rounding);
    void           (*RenderFrameBorder)(ImVec2 p_min, ImVec2 p_max, float rounding);
    void           (*RenderColorRectWithAlphaCheckerboard)(ImDrawList *draw_list, ImVec2 p_min, ImVec2 p_max, ImU32 fill_col, float grid_step, ImVec2 grid_off, float rounding, ImDrawFlags flags);
    void           (*RenderNavHighlight)(const ImRect bb, ImGuiID id, ImGuiNavHighlightFlags flags);
    const char*    (*FindRenderedTextEnd)(const char *text, const char *text_end);
    void           (*RenderArrow)(ImDrawList *draw_list, ImVec2 pos, ImU32 col, ImGuiDir dir, float scale);
    void           (*RenderBullet)(ImDrawList *draw_list, ImVec2 pos, ImU32 col);
    void           (*RenderCheckMark)(ImDrawList *draw_list, ImVec2 pos, ImU32 col, float sz);
    void           (*RenderMouseCursor)(ImDrawList *draw_list, ImVec2 pos, float scale, ImGuiMouseCursor mouse_cursor, ImU32 col_fill, ImU32 col_border, ImU32 col_shadow);
    void           (*RenderArrowPointingAt)(ImDrawList *draw_list, ImVec2 pos, ImVec2 half_sz, ImGuiDir direction, ImU32 col);
    void           (*RenderArrowDockMenu)(ImDrawList *draw_list, ImVec2 p_min, float sz, ImU32 col);
    void           (*RenderRectFilledRangeH)(ImDrawList *draw_list, const ImRect rect, ImU32 col, float x_start_norm, float x_end_norm, float rounding);
    void           (*RenderRectFilledWithHole)(ImDrawList *draw_list, ImRect outer, ImRect inner, ImU32 col, float rounding);
    void           (*TextEx)(const char *text, const char *text_end, ImGuiTextFlags flags);
    bool           (*ButtonEx)(const char *label, const ImVec2 size_arg, ImGuiButtonFlags flags);
    bool           (*CloseButton)(ImGuiID id, const ImVec2 pos);
    bool           (*CollapseButton)(ImGuiID id, const ImVec2 pos, ImGuiDockNode *dock_node);
    bool           (*ArrowButtonEx)(const char *str_id, ImGuiDir dir, ImVec2 size_arg, ImGuiButtonFlags flags);
    void           (*Scrollbar)(ImGuiAxis axis);
    bool           (*ScrollbarEx)(const ImRect bb, ImGuiID id, ImGuiAxis axis, float *p_scroll_v, float avail_v, float contents_v, ImDrawFlags flags);
    bool           (*ImageButtonEx)(ImGuiID id, ImTextureID texture_id, const ImVec2 size, const ImVec2 uv0, const ImVec2 uv1, const ImVec2 padding, const ImVec4 bg_col, const ImVec4 tint_col);
    void           (*GetWindowScrollbarRect)(ImRect *pOut, ImGuiWindow *window, ImGuiAxis axis);
    ImGuiID        (*GetWindowScrollbarID)(ImGuiWindow *window, ImGuiAxis axis);
    ImGuiID        (*GetWindowResizeID)(ImGuiWindow *window, int n);
    void           (*SeparatorEx)(ImGuiSeparatorFlags flags);
    bool           (*CheckboxFlags_S64Ptr)(const char *label, ImS64 *flags, ImS64 flags_value);
    bool           (*CheckboxFlags_U64Ptr)(const char *label, ImU64 *flags, ImU64 flags_value);
    bool           (*ButtonBehavior)(const ImRect bb, ImGuiID id, bool *out_hovered, bool *out_held, ImGuiButtonFlags flags);
    bool           (*DragBehavior)(ImGuiID id, ImGuiDataType data_type, void *p_v, float v_speed, const void *p_min, const void *p_max, const char *format, ImGuiSliderFlags flags);
    bool           (*SliderBehavior)(const ImRect bb, ImGuiID id, ImGuiDataType data_type, void *p_v, const void *p_min, const void *p_max, const char *format, ImGuiSliderFlags flags, ImRect *out_grab_bb);
    bool           (*SplitterBehavior)(const ImRect bb, ImGuiID id, ImGuiAxis axis, float *size1, float *size2, float min_size1, float min_size2, float hover_extend, float hover_visibility_delay);
    bool           (*TreeNodeBehavior)(ImGuiID id, ImGuiTreeNodeFlags flags, const char *label, const char *label_end);
    bool           (*TreeNodeBehaviorIsOpen)(ImGuiID id, ImGuiTreeNodeFlags flags);
    void           (*TreePushOverrideID)(ImGuiID id);
    const ImGuiDataTypeInfo*  (*DataTypeGetInfo)(ImGuiDataType data_type);
    int            (*DataTypeFormatString)(char *buf, int buf_size, ImGuiDataType data_type, const void *p_data, const char *format);
    void           (*DataTypeApplyOp)(ImGuiDataType data_type, int op, void *output, const void *arg_1, const void *arg_2);
    bool           (*DataTypeApplyOpFromText)(const char *buf, const char *initial_value_buf, ImGuiDataType data_type, void *p_data, const char *format);
    int            (*DataTypeCompare)(ImGuiDataType data_type, const void *arg_1, const void *arg_2);
    bool           (*DataTypeClamp)(ImGuiDataType data_type, void *p_data, const void *p_min, const void *p_max);
    bool           (*InputTextEx)(const char *label, const char *hint, char *buf, int buf_size, const ImVec2 size_arg, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void *user_data);
    bool           (*TempInputText)(const ImRect bb, ImGuiID id, const char *label, char *buf, int buf_size, ImGuiInputTextFlags flags);
    bool           (*TempInputScalar)(const ImRect bb, ImGuiID id, const char *label, ImGuiDataType data_type, void *p_data, const char *format, const void *p_clamp_min, const void *p_clamp_max);
    bool           (*TempInputIsActive)(ImGuiID id);
    ImGuiInputTextState*  (*GetInputTextState)(ImGuiID id);
    void           (*ColorTooltip)(const char *text, const float *col, ImGuiColorEditFlags flags);
    void           (*ColorEditOptionsPopup)(const float *col, ImGuiColorEditFlags flags);
    void           (*ColorPickerOptionsPopup)(const float *ref_col, ImGuiColorEditFlags flags);
    int            (*PlotEx)(ImGuiPlotType plot_type, const char *label, float (*values_getter)(void *data, int idx), void *data, int values_count, int values_offset, const char *overlay_text, float scale_min, float scale_max, ImVec2 frame_size);
    void           (*ShadeVertsLinearColorGradientKeepAlpha)(ImDrawList *draw_list, int vert_start_idx, int vert_end_idx, ImVec2 gradient_p0, ImVec2 gradient_p1, ImU32 col0, ImU32 col1);
    void           (*ShadeVertsLinearUV)(ImDrawList *draw_list, int vert_start_idx, int vert_end_idx, const ImVec2 a, const ImVec2 b, const ImVec2 uv_a, const ImVec2 uv_b, bool clamp);
    void           (*GcCompactTransientMiscBuffers)(void);
    void           (*GcCompactTransientWindowBuffers)(ImGuiWindow *window);
    void           (*GcAwakeTransientWindowBuffers)(ImGuiWindow *window);
    void           (*ErrorCheckEndFrameRecover)(ImGuiErrorLogCallback log_callback, void *user_data);
    void           (*DebugDrawItemRect)(ImU32 col);
    void           (*DebugStartItemPicker)(void);
    void           (*DebugNodeColumns)(ImGuiOldColumns *columns);
    void           (*DebugNodeDockNode)(ImGuiDockNode *node, const char *label);
    void           (*DebugNodeDrawList)(ImGuiWindow *window, ImGuiViewportP *viewport, const ImDrawList *draw_list, const char *label);
    void           (*DebugNodeDrawCmdShowMeshAndBoundingBox)(ImDrawList *out_draw_list, const ImDrawList *draw_list, const ImDrawCmd *draw_cmd, bool show_mesh, bool show_aabb);
    void           (*DebugNodeStorage)(ImGuiStorage *storage, const char *label);
    void           (*DebugNodeTabBar)(ImGuiTabBar *tab_bar, const char *label);
    void           (*DebugNodeTable)(ImGuiTable *table);
    void           (*DebugNodeTableSettings)(ImGuiTableSettings *settings);
    void           (*DebugNodeWindow)(ImGuiWindow *window, const char *label);
    void           (*DebugNodeWindowSettings)(ImGuiWindowSettings *settings);
    void           (*DebugNodeWindowsList)(ImVector_ImGuiWindowPtr *windows, const char *label);
    void           (*DebugNodeViewport)(ImGuiViewportP *viewport);
    void           (*DebugRenderViewportThumbnail)(ImDrawList *draw_list, ImGuiViewportP *viewport, const ImRect bb);
    const ImFontBuilderIO*  (*ImFontAtlasGetBuilderForStbTruetype)(void);
    void           (*ImFontAtlasBuildInit)(ImFontAtlas *atlas);
    void           (*ImFontAtlasBuildSetupFont)(ImFontAtlas *atlas, ImFont *font, ImFontConfig *font_config, float ascent, float descent);
    void           (*ImFontAtlasBuildPackCustomRects)(ImFontAtlas *atlas, void *stbrp_context_opaque);
    void           (*ImFontAtlasBuildFinish)(ImFontAtlas *atlas);
    void           (*ImFontAtlasBuildRender8bppRectFromString)(ImFontAtlas *atlas, int x, int y, int w, int h, const char *in_str, char in_marker_char, unsigned char in_marker_pixel_value);
    void           (*ImFontAtlasBuildRender32bppRectFromString)(ImFontAtlas *atlas, int x, int y, int w, int h, const char *in_str, char in_marker_char, unsigned int in_marker_pixel_value);
    void           (*ImFontAtlasBuildMultiplyCalcLookupTable)(unsigned char out_table[256], float in_multiply_factor);
    void           (*ImFontAtlasBuildMultiplyRectAlpha8)(const unsigned char table[256], unsigned char *pixels, int x, int y, int w, int h, int stride);
    void           (*LogText)(const char *fmt, ...);
    float          (*GET_FLT_MAX)();
    float          (*GET_FLT_MIN)();
    // ImGuiIO
    void           (*ImGuiIO_AddInputCharacter)(ImGuiIO *self, unsigned int c);
    void           (*ImGuiIO_AddInputCharacterUTF16)(ImGuiIO *self, ImWchar16 c);
    void           (*ImGuiIO_AddInputCharactersUTF8)(ImGuiIO *self, const char *str);
    void           (*ImGuiIO_ClearInputCharacters)(ImGuiIO *self);
    ImGuiIO*       (*ImGuiIO_ImGuiIO)(void);
    void           (*ImGuiIO_destroy)(ImGuiIO *self);
    // ImDrawList
    ImDrawList*    (*ImDrawList_ImDrawList)(const ImDrawListSharedData *shared_data);
    void           (*ImDrawList_destroy)(ImDrawList *self);
    void           (*ImDrawList_PushClipRect)(ImDrawList *self, ImVec2 clip_rect_min, ImVec2 clip_rect_max, bool intersect_with_current_clip_rect);
    void           (*ImDrawList_PushClipRectFullScreen)(ImDrawList *self);
    void           (*ImDrawList_PopClipRect)(ImDrawList *self);
    void           (*ImDrawList_PushTextureID)(ImDrawList *self, ImTextureID texture_id);
    void           (*ImDrawList_PopTextureID)(ImDrawList *self);
    void           (*ImDrawList_GetClipRectMin)(ImVec2 *pOut, ImDrawList *self);
    void           (*ImDrawList_GetClipRectMax)(ImVec2 *pOut, ImDrawList *self);
    void           (*ImDrawList_AddLine)(ImDrawList *self, const ImVec2 p1, const ImVec2 p2, ImU32 col, float thickness);
    void           (*ImDrawList_AddRect)(ImDrawList *self, const ImVec2 p_min, const ImVec2 p_max, ImU32 col, float rounding, ImDrawFlags flags, float thickness);
    void           (*ImDrawList_AddRectFilled)(ImDrawList *self, const ImVec2 p_min, const ImVec2 p_max, ImU32 col, float rounding, ImDrawFlags flags);
    void           (*ImDrawList_AddRectFilledMultiColor)(ImDrawList *self, const ImVec2 p_min, const ImVec2 p_max, ImU32 col_upr_left, ImU32 col_upr_right, ImU32 col_bot_right, ImU32 col_bot_left);
    void           (*ImDrawList_AddQuad)(ImDrawList *self, const ImVec2 p1, const ImVec2 p2, const ImVec2 p3, const ImVec2 p4, ImU32 col, float thickness);
    void           (*ImDrawList_AddQuadFilled)(ImDrawList *self, const ImVec2 p1, const ImVec2 p2, const ImVec2 p3, const ImVec2 p4, ImU32 col);
    void           (*ImDrawList_AddTriangle)(ImDrawList *self, const ImVec2 p1, const ImVec2 p2, const ImVec2 p3, ImU32 col, float thickness);
    void           (*ImDrawList_AddTriangleFilled)(ImDrawList *self, const ImVec2 p1, const ImVec2 p2, const ImVec2 p3, ImU32 col);
    void           (*ImDrawList_AddCircle)(ImDrawList *self, const ImVec2 center, float radius, ImU32 col, int num_segments, float thickness);
    void           (*ImDrawList_AddCircleFilled)(ImDrawList *self, const ImVec2 center, float radius, ImU32 col, int num_segments);
    void           (*ImDrawList_AddNgon)(ImDrawList *self, const ImVec2 center, float radius, ImU32 col, int num_segments, float thickness);
    void           (*ImDrawList_AddNgonFilled)(ImDrawList *self, const ImVec2 center, float radius, ImU32 col, int num_segments);
    void           (*ImDrawList_AddText_Vec2)(ImDrawList *self, const ImVec2 pos, ImU32 col, const char *text_begin, const char *text_end);
    void           (*ImDrawList_AddText_FontPtr)(ImDrawList *self, const ImFont *font, float font_size, const ImVec2 pos, ImU32 col, const char *text_begin, const char *text_end, float wrap_width, const ImVec4 *cpu_fine_clip_rect);
    void           (*ImDrawList_AddPolyline)(ImDrawList *self, const ImVec2 *points, int num_points, ImU32 col, ImDrawFlags flags, float thickness);
    void           (*ImDrawList_AddConvexPolyFilled)(ImDrawList *self, const ImVec2 *points, int num_points, ImU32 col);
    void           (*ImDrawList_AddBezierCubic)(ImDrawList *self, const ImVec2 p1, const ImVec2 p2, const ImVec2 p3, const ImVec2 p4, ImU32 col, float thickness, int num_segments);
    void           (*ImDrawList_AddBezierQuadratic)(ImDrawList *self, const ImVec2 p1, const ImVec2 p2, const ImVec2 p3, ImU32 col, float thickness, int num_segments);
    void           (*ImDrawList_AddImage)(ImDrawList *self, ImTextureID user_texture_id, const ImVec2 p_min, const ImVec2 p_max, const ImVec2 uv_min, const ImVec2 uv_max, ImU32 col);
    void           (*ImDrawList_AddImageQuad)(ImDrawList *self, ImTextureID user_texture_id, const ImVec2 p1, const ImVec2 p2, const ImVec2 p3, const ImVec2 p4, const ImVec2 uv1, const ImVec2 uv2, const ImVec2 uv3, const ImVec2 uv4, ImU32 col);
    void           (*ImDrawList_AddImageRounded)(ImDrawList *self, ImTextureID user_texture_id, const ImVec2 p_min, const ImVec2 p_max, const ImVec2 uv_min, const ImVec2 uv_max, ImU32 col, float rounding, ImDrawFlags flags);
    void           (*ImDrawList_PathClear)(ImDrawList *self);
    void           (*ImDrawList_PathLineTo)(ImDrawList *self, const ImVec2 pos);
    void           (*ImDrawList_PathLineToMergeDuplicate)(ImDrawList *self, const ImVec2 pos);
    void           (*ImDrawList_PathFillConvex)(ImDrawList *self, ImU32 col);
    void           (*ImDrawList_PathStroke)(ImDrawList *self, ImU32 col, ImDrawFlags flags, float thickness);
    void           (*ImDrawList_PathArcTo)(ImDrawList *self, const ImVec2 center, float radius, float a_min, float a_max, int num_segments);
    void           (*ImDrawList_PathArcToFast)(ImDrawList *self, const ImVec2 center, float radius, int a_min_of_12, int a_max_of_12);
    void           (*ImDrawList_PathBezierCubicCurveTo)(ImDrawList *self, const ImVec2 p2, const ImVec2 p3, const ImVec2 p4, int num_segments);
    void           (*ImDrawList_PathBezierQuadraticCurveTo)(ImDrawList *self, const ImVec2 p2, const ImVec2 p3, int num_segments);
    void           (*ImDrawList_PathRect)(ImDrawList *self, const ImVec2 rect_min, const ImVec2 rect_max, float rounding, ImDrawFlags flags);
    void           (*ImDrawList_AddCallback)(ImDrawList *self, ImDrawCallback callback, void *callback_data);
    void           (*ImDrawList_AddDrawCmd)(ImDrawList *self);
    ImDrawList*    (*ImDrawList_CloneOutput)(ImDrawList *self);
    void           (*ImDrawList_ChannelsSplit)(ImDrawList *self, int count);
    void           (*ImDrawList_ChannelsMerge)(ImDrawList *self);
    void           (*ImDrawList_ChannelsSetCurrent)(ImDrawList *self, int n);
    void           (*ImDrawList_PrimReserve)(ImDrawList *self, int idx_count, int vtx_count);
    void           (*ImDrawList_PrimUnreserve)(ImDrawList *self, int idx_count, int vtx_count);
    void           (*ImDrawList_PrimRect)(ImDrawList *self, const ImVec2 a, const ImVec2 b, ImU32 col);
    void           (*ImDrawList_PrimRectUV)(ImDrawList *self, const ImVec2 a, const ImVec2 b, const ImVec2 uv_a, const ImVec2 uv_b, ImU32 col);
    void           (*ImDrawList_PrimQuadUV)(ImDrawList *self, const ImVec2 a, const ImVec2 b, const ImVec2 c, const ImVec2 d, const ImVec2 uv_a, const ImVec2 uv_b, const ImVec2 uv_c, const ImVec2 uv_d, ImU32 col);
    void           (*ImDrawList_PrimWriteVtx)(ImDrawList *self, const ImVec2 pos, const ImVec2 uv, ImU32 col);
    void           (*ImDrawList_PrimWriteIdx)(ImDrawList *self, ImDrawIdx idx);
    void           (*ImDrawList_PrimVtx)(ImDrawList *self, const ImVec2 pos, const ImVec2 uv, ImU32 col);
    void           (*ImDrawList__ResetForNewFrame)(ImDrawList *self);
    void           (*ImDrawList__ClearFreeMemory)(ImDrawList *self);
    void           (*ImDrawList__PopUnusedDrawCmd)(ImDrawList *self);
    void           (*ImDrawList__OnChangedClipRect)(ImDrawList *self);
    void           (*ImDrawList__OnChangedTextureID)(ImDrawList *self);
    void           (*ImDrawList__OnChangedVtxOffset)(ImDrawList *self);
    int            (*ImDrawList__CalcCircleAutoSegmentCount)(ImDrawList *self, float radius);
    void           (*ImDrawList__PathArcToFastEx)(ImDrawList *self, const ImVec2 center, float radius, int a_min_sample, int a_max_sample, int a_step);
    void           (*ImDrawList__PathArcToN)(ImDrawList *self, const ImVec2 center, float radius, float a_min, float a_max, int num_segments);
    // ImDrawList
    ImFont*        (*ImFont_ImFont)(void);
    void           (*ImFont_destroy)(ImFont *self);
    const ImFontGlyph*  (*ImFont_FindGlyph)(ImFont *self, ImWchar c);
    const ImFontGlyph*  (*ImFont_FindGlyphNoFallback)(ImFont *self, ImWchar c);
    float          (*ImFont_GetCharAdvance)(ImFont *self, ImWchar c);
    bool           (*ImFont_IsLoaded)(ImFont *self);
    const char*    (*ImFont_GetDebugName)(ImFont *self);
    void           (*ImFont_CalcTextSizeA)(ImVec2 *pOut, ImFont *self, float size, float max_width, float wrap_width, const char *text_begin, const char *text_end, const char **remaining);
    const char*    (*ImFont_CalcWordWrapPositionA)(ImFont *self, float scale, const char *text, const char *text_end, float wrap_width);
    void           (*ImFont_RenderChar)(ImFont *self, ImDrawList *draw_list, float size, ImVec2 pos, ImU32 col, ImWchar c);
    void           (*ImFont_RenderText)(ImFont *self, ImDrawList *draw_list, float size, ImVec2 pos, ImU32 col, const ImVec4 clip_rect, const char *text_begin, const char *text_end, float wrap_width, bool cpu_fine_clip);
    void           (*ImFont_BuildLookupTable)(ImFont *self);
    void           (*ImFont_ClearOutputData)(ImFont *self);
    void           (*ImFont_GrowIndex)(ImFont *self, int new_size);
    void           (*ImFont_AddGlyph)(ImFont *self, const ImFontConfig *src_cfg, ImWchar c, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, float advance_x);
    void           (*ImFont_AddRemapChar)(ImFont *self, ImWchar dst, ImWchar src, bool overwrite_dst);
    void           (*ImFont_SetGlyphVisible)(ImFont *self, ImWchar c, bool visible);
    void           (*ImFont_SetFallbackChar)(ImFont *self, ImWchar c);
    bool           (*ImFont_IsGlyphRangeUnused)(ImFont *self, unsigned int c_begin, unsigned int c_last);
    // ImFontAtlas
    ImFontAtlas*   (*ImFontAtlas_ImFontAtlas)(void);
    void           (*ImFontAtlas_destroy)(ImFontAtlas *self);
    ImFont*        (*ImFontAtlas_AddFont)(ImFontAtlas *self, const ImFontConfig *font_cfg);
    ImFont*        (*ImFontAtlas_AddFontDefault)(ImFontAtlas *self, const ImFontConfig *font_cfg);
    ImFont*        (*ImFontAtlas_AddFontFromFileTTF)(ImFontAtlas *self, const char *filename, float size_pixels, const ImFontConfig *font_cfg, const ImWchar *glyph_ranges);
    ImFont*        (*ImFontAtlas_AddFontFromMemoryTTF)(ImFontAtlas *self, void *font_data, int font_size, float size_pixels, const ImFontConfig *font_cfg, const ImWchar *glyph_ranges);
    ImFont*        (*ImFontAtlas_AddFontFromMemoryCompressedTTF)(ImFontAtlas *self, const void *compressed_font_data, int compressed_font_size, float size_pixels, const ImFontConfig *font_cfg, const ImWchar *glyph_ranges);
    ImFont*        (*ImFontAtlas_AddFontFromMemoryCompressedBase85TTF)(ImFontAtlas *self, const char *compressed_font_data_base85, float size_pixels, const ImFontConfig *font_cfg, const ImWchar *glyph_ranges);
    void           (*ImFontAtlas_ClearInputData)(ImFontAtlas *self);
    void           (*ImFontAtlas_ClearTexData)(ImFontAtlas *self);
    void           (*ImFontAtlas_ClearFonts)(ImFontAtlas *self);
    void           (*ImFontAtlas_Clear)(ImFontAtlas *self);
    bool           (*ImFontAtlas_Build)(ImFontAtlas *self);
    void           (*ImFontAtlas_GetTexDataAsAlpha8)(ImFontAtlas *self, unsigned char **out_pixels, int *out_width, int *out_height, int *out_bytes_per_pixel);
    void           (*ImFontAtlas_GetTexDataAsRGBA32)(ImFontAtlas *self, unsigned char **out_pixels, int *out_width, int *out_height, int *out_bytes_per_pixel);
    bool           (*ImFontAtlas_IsBuilt)(ImFontAtlas *self);
    void           (*ImFontAtlas_SetTexID)(ImFontAtlas *self, ImTextureID id);
    const ImWchar*  (*ImFontAtlas_GetGlyphRangesDefault)(ImFontAtlas *self);
    const ImWchar*  (*ImFontAtlas_GetGlyphRangesKorean)(ImFontAtlas *self);
    const ImWchar*  (*ImFontAtlas_GetGlyphRangesJapanese)(ImFontAtlas *self);
    const ImWchar*  (*ImFontAtlas_GetGlyphRangesChineseFull)(ImFontAtlas *self);
    const ImWchar*  (*ImFontAtlas_GetGlyphRangesChineseSimplifiedCommon)(ImFontAtlas *self);
    const ImWchar*  (*ImFontAtlas_GetGlyphRangesCyrillic)(ImFontAtlas *self);
    const ImWchar*  (*ImFontAtlas_GetGlyphRangesThai)(ImFontAtlas *self);
    const ImWchar*  (*ImFontAtlas_GetGlyphRangesVietnamese)(ImFontAtlas *self);
    int            (*ImFontAtlas_AddCustomRectRegular)(ImFontAtlas *self, int width, int height);
    int            (*ImFontAtlas_AddCustomRectFontGlyph)(ImFontAtlas *self, ImFont *font, ImWchar id, int width, int height, float advance_x, const ImVec2 offset);
    ImFontAtlasCustomRect*  (*ImFontAtlas_GetCustomRectByIndex)(ImFontAtlas *self, int index);
    void           (*ImFontAtlas_CalcCustomRectUV)(ImFontAtlas *self, const ImFontAtlasCustomRect *rect, ImVec2 *out_uv_min, ImVec2 *out_uv_max);
    bool           (*ImFontAtlas_GetMouseCursorTexData)(ImFontAtlas *self, ImGuiMouseCursor cursor, ImVec2 *out_offset, ImVec2 *out_size, ImVec2 out_uv_border[2], ImVec2 out_uv_fill[2]);
    // ImGuiPayload
    ImGuiPayload*  (*ImGuiPayload_ImGuiPayload)(void);
    void           (*ImGuiPayload_destroy)(ImGuiPayload *self);
    void           (*ImGuiPayload_Clear)(ImGuiPayload *self);
    bool           (*ImGuiPayload_IsDataType)(ImGuiPayload *self, const char *type);
    bool           (*ImGuiPayload_IsPreview)(ImGuiPayload *self);
    bool           (*ImGuiPayload_IsDelivery)(ImGuiPayload *self);
    // ImGuiListClipper
    ImGuiListClipper*  (*ImGuiListClipper_ImGuiListClipper)(void);
    void           (*ImGuiListClipper_destroy)(ImGuiListClipper *self);
    void           (*ImGuiListClipper_Begin)(ImGuiListClipper *self, int items_count, float items_height);
    void           (*ImGuiListClipper_End)(ImGuiListClipper *self);
    bool           (*ImGuiListClipper_Step)(ImGuiListClipper *self);
    // ImGuiTextFilter
    ImGuiTextFilter*  (*ImGuiTextFilter_ImGuiTextFilter)(const char *default_filter);
    void           (*ImGuiTextFilter_destroy)(ImGuiTextFilter *self);
    bool           (*ImGuiTextFilter_Draw)(ImGuiTextFilter *self, const char *label, float width);
    bool           (*ImGuiTextFilter_PassFilter)(ImGuiTextFilter *self, const char *text, const char *text_end);
    void           (*ImGuiTextFilter_Build)(ImGuiTextFilter *self);
    void           (*ImGuiTextFilter_Clear)(ImGuiTextFilter *self);
    bool           (*ImGuiTextFilter_IsActive)(ImGuiTextFilter *self);
    // ImGuiTextBuffer
    ImGuiTextBuffer*  (*ImGuiTextBuffer_ImGuiTextBuffer)(void);
    void           (*ImGuiTextBuffer_destroy)(ImGuiTextBuffer *self);
    const char*    (*ImGuiTextBuffer_begin)(ImGuiTextBuffer *self);
    const char*    (*ImGuiTextBuffer_end)(ImGuiTextBuffer *self);
    int            (*ImGuiTextBuffer_size)(ImGuiTextBuffer *self);
    bool           (*ImGuiTextBuffer_empty)(ImGuiTextBuffer *self);
    void           (*ImGuiTextBuffer_clear)(ImGuiTextBuffer *self);
    void           (*ImGuiTextBuffer_reserve)(ImGuiTextBuffer *self, int capacity);
    const char*    (*ImGuiTextBuffer_c_str)(ImGuiTextBuffer *self);
    void           (*ImGuiTextBuffer_append)(ImGuiTextBuffer *self, const char *str, const char *str_end);
    void           (*ImGuiTextBuffer_appendfv)(ImGuiTextBuffer *self, const char *fmt, va_list args);
    void           (*ImGuiTextBuffer_appendf)(struct ImGuiTextBuffer *buffer, const char *fmt, ...);
} rizz_api_imgui;


