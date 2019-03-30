#pragma once

#include <stdarg.h>
#include <sx/math.h>

typedef struct CustomRect                 CustomRect;
typedef struct Pair                       Pair;
typedef struct TextRange                  TextRange;
typedef struct ImGuiTextFilter            ImGuiTextFilter;
typedef struct ImGuiTextBuffer            ImGuiTextBuffer;
typedef struct ImGuiStyle                 ImGuiStyle;
typedef struct ImGuiStorage               ImGuiStorage;
typedef struct ImGuiSizeCallbackData      ImGuiSizeCallbackData;
typedef struct ImGuiPayload               ImGuiPayload;
typedef struct ImGuiOnceUponAFrame        ImGuiOnceUponAFrame;
typedef struct ImGuiListClipper           ImGuiListClipper;
typedef struct ImGuiInputTextCallbackData ImGuiInputTextCallbackData;
typedef struct ImGuiIO                    ImGuiIO;
typedef struct ImGuiContext               ImGuiContext;
typedef struct ImColor                    ImColor;
typedef struct ImFontGlyphRangesBuilder   ImFontGlyphRangesBuilder;
typedef struct ImFontGlyph                ImFontGlyph;
typedef struct ImFontConfig               ImFontConfig;
typedef struct ImFontAtlas                ImFontAtlas;
typedef struct ImFont                     ImFont;
typedef struct ImDrawVert                 ImDrawVert;
typedef struct ImDrawListSharedData       ImDrawListSharedData;
typedef struct ImDrawList                 ImDrawList;
typedef struct ImDrawData                 ImDrawData;
typedef struct ImDrawCmd                  ImDrawCmd;
typedef struct ImDrawChannel              ImDrawChannel;
typedef union sx_vec2 ImVec2;
typedef union sx_vec4 ImVec4;

struct ImDrawChannel;
struct ImDrawCmd;
struct ImDrawData;
struct ImDrawList;
struct ImDrawListSharedData;
struct ImDrawVert;
struct ImFont;
struct ImFontAtlas;
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
struct ImGuiSizeCallbackData;
struct ImGuiStorage;
struct ImGuiStyle;
struct ImGuiTextBuffer;
struct ImGuiTextFilter;
typedef void*          ImTextureID;
typedef unsigned int   ImGuiID;
typedef unsigned short ImWchar;
typedef int            ImGuiCol;
typedef int            ImGuiCond;
typedef int            ImGuiDataType;
typedef int            ImGuiDir;
typedef int            ImGuiKey;
typedef int            ImGuiNavInput;
typedef int            ImGuiMouseCursor;
typedef int            ImGuiStyleVar;
typedef int            ImDrawCornerFlags;
typedef int            ImDrawListFlags;
typedef int            ImFontAtlasFlags;
typedef int            ImGuiBackendFlags;
typedef int            ImGuiColorEditFlags;
typedef int            ImGuiColumnsFlags;
typedef int            ImGuiConfigFlags;
typedef int            ImGuiComboFlags;
typedef int            ImGuiDragDropFlags;
typedef int            ImGuiFocusedFlags;
typedef int            ImGuiHoveredFlags;
typedef int            ImGuiInputTextFlags;
typedef int            ImGuiSelectableFlags;
typedef int            ImGuiTabBarFlags;
typedef int            ImGuiTabItemFlags;
typedef int            ImGuiTreeNodeFlags;
typedef int            ImGuiWindowFlags;
typedef int (*ImGuiInputTextCallback)(ImGuiInputTextCallbackData* data);
typedef void (*ImGuiSizeCallback)(ImGuiSizeCallbackData* data);
typedef signed char    ImS8;
typedef unsigned char  ImU8;
typedef signed short   ImS16;
typedef unsigned short ImU16;
typedef signed int     ImS32;
typedef unsigned int   ImU32;
typedef int64_t        ImS64;
typedef uint64_t       ImU64;
typedef void (*ImDrawCallback)(const ImDrawList* parent_list, const ImDrawCmd* cmd);
typedef unsigned short ImDrawIdx;
typedef struct ImVector {
    int   Size;
    int   Capacity;
    void* Data;
} ImVector;
typedef struct ImVector_float {
    int    Size;
    int    Capacity;
    float* Data;
} ImVector_float;
typedef struct ImVector_ImWchar {
    int      Size;
    int      Capacity;
    ImWchar* Data;
} ImVector_ImWchar;
typedef struct ImVector_ImFontConfig {
    int           Size;
    int           Capacity;
    ImFontConfig* Data;
} ImVector_ImFontConfig;
typedef struct ImVector_ImFontGlyph {
    int          Size;
    int          Capacity;
    ImFontGlyph* Data;
} ImVector_ImFontGlyph;
typedef struct ImVector_TextRange {
    int        Size;
    int        Capacity;
    TextRange* Data;
} ImVector_TextRange;
typedef struct ImVector_CustomRect {
    int         Size;
    int         Capacity;
    CustomRect* Data;
} ImVector_CustomRect;
typedef struct ImVector_ImDrawChannel {
    int            Size;
    int            Capacity;
    ImDrawChannel* Data;
} ImVector_ImDrawChannel;
typedef struct ImVector_char {
    int   Size;
    int   Capacity;
    char* Data;
} ImVector_char;
typedef struct ImVector_ImTextureID {
    int          Size;
    int          Capacity;
    ImTextureID* Data;
} ImVector_ImTextureID;
typedef struct ImVector_ImDrawVert {
    int         Size;
    int         Capacity;
    ImDrawVert* Data;
} ImVector_ImDrawVert;
typedef struct ImVector_int {
    int  Size;
    int  Capacity;
    int* Data;
} ImVector_int;
typedef struct ImVector_Pair {
    int   Size;
    int   Capacity;
    Pair* Data;
} ImVector_Pair;
typedef struct ImVector_ImFontPtr {
    int      Size;
    int      Capacity;
    ImFont** Data;
} ImVector_ImFontPtr;
typedef struct ImVector_ImVec4 {
    int     Size;
    int     Capacity;
    ImVec4* Data;
} ImVector_ImVec4;
typedef struct ImVector_ImDrawCmd {
    int        Size;
    int        Capacity;
    ImDrawCmd* Data;
} ImVector_ImDrawCmd;
typedef struct ImVector_ImDrawIdx {
    int        Size;
    int        Capacity;
    ImDrawIdx* Data;
} ImVector_ImDrawIdx;
typedef struct ImVector_ImVec2 {
    int     Size;
    int     Capacity;
    ImVec2* Data;
} ImVector_ImVec2;

enum ImGuiWindowFlags_ {
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
    ImGuiWindowFlags_AlwaysVerticalScrollbar = 1 << 14,
    ImGuiWindowFlags_AlwaysHorizontalScrollbar = 1 << 15,
    ImGuiWindowFlags_AlwaysUseWindowPadding = 1 << 16,
    ImGuiWindowFlags_NoNavInputs = 1 << 18,
    ImGuiWindowFlags_NoNavFocus = 1 << 19,
    ImGuiWindowFlags_UnsavedDocument = 1 << 20,
    ImGuiWindowFlags_NoNav = ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus,
    ImGuiWindowFlags_NoDecoration = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse,
    ImGuiWindowFlags_NoInputs =
        ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus,
    ImGuiWindowFlags_NavFlattened = 1 << 23,
    ImGuiWindowFlags_ChildWindow = 1 << 24,
    ImGuiWindowFlags_Tooltip = 1 << 25,
    ImGuiWindowFlags_Popup = 1 << 26,
    ImGuiWindowFlags_Modal = 1 << 27,
    ImGuiWindowFlags_ChildMenu = 1 << 28
};
enum ImGuiInputTextFlags_ {
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
    ImGuiInputTextFlags_AlwaysInsertMode = 1 << 13,
    ImGuiInputTextFlags_ReadOnly = 1 << 14,
    ImGuiInputTextFlags_Password = 1 << 15,
    ImGuiInputTextFlags_NoUndoRedo = 1 << 16,
    ImGuiInputTextFlags_CharsScientific = 1 << 17,
    ImGuiInputTextFlags_CallbackResize = 1 << 18,
    ImGuiInputTextFlags_Multiline = 1 << 20
};
enum ImGuiTreeNodeFlags_ {
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
    ImGuiTreeNodeFlags_NavLeftJumpsBackHere = 1 << 13,
    ImGuiTreeNodeFlags_CollapsingHeader = ImGuiTreeNodeFlags_Framed |
                                          ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                          ImGuiTreeNodeFlags_NoAutoOpenOnLog
};
enum ImGuiSelectableFlags_ {
    ImGuiSelectableFlags_None = 0,
    ImGuiSelectableFlags_DontClosePopups = 1 << 0,
    ImGuiSelectableFlags_SpanAllColumns = 1 << 1,
    ImGuiSelectableFlags_AllowDoubleClick = 1 << 2,
    ImGuiSelectableFlags_Disabled = 1 << 3
};
enum ImGuiComboFlags_ {
    ImGuiComboFlags_None = 0,
    ImGuiComboFlags_PopupAlignLeft = 1 << 0,
    ImGuiComboFlags_HeightSmall = 1 << 1,
    ImGuiComboFlags_HeightRegular = 1 << 2,
    ImGuiComboFlags_HeightLarge = 1 << 3,
    ImGuiComboFlags_HeightLargest = 1 << 4,
    ImGuiComboFlags_NoArrowButton = 1 << 5,
    ImGuiComboFlags_NoPreview = 1 << 6,
    ImGuiComboFlags_HeightMask_ = ImGuiComboFlags_HeightSmall | ImGuiComboFlags_HeightRegular |
                                  ImGuiComboFlags_HeightLarge | ImGuiComboFlags_HeightLargest
};
enum ImGuiTabBarFlags_ {
    ImGuiTabBarFlags_None = 0,
    ImGuiTabBarFlags_Reorderable = 1 << 0,
    ImGuiTabBarFlags_AutoSelectNewTabs = 1 << 1,
    ImGuiTabBarFlags_TabListPopupButton = 1 << 2,
    ImGuiTabBarFlags_NoCloseWithMiddleMouseButton = 1 << 3,
    ImGuiTabBarFlags_NoTabListScrollingButtons = 1 << 4,
    ImGuiTabBarFlags_NoTooltip = 1 << 5,
    ImGuiTabBarFlags_FittingPolicyResizeDown = 1 << 6,
    ImGuiTabBarFlags_FittingPolicyScroll = 1 << 7,
    ImGuiTabBarFlags_FittingPolicyMask_ =
        ImGuiTabBarFlags_FittingPolicyResizeDown | ImGuiTabBarFlags_FittingPolicyScroll,
    ImGuiTabBarFlags_FittingPolicyDefault_ = ImGuiTabBarFlags_FittingPolicyResizeDown
};
enum ImGuiTabItemFlags_ {
    ImGuiTabItemFlags_None = 0,
    ImGuiTabItemFlags_UnsavedDocument = 1 << 0,
    ImGuiTabItemFlags_SetSelected = 1 << 1,
    ImGuiTabItemFlags_NoCloseWithMiddleMouseButton = 1 << 2,
    ImGuiTabItemFlags_NoPushId = 1 << 3
};
enum ImGuiFocusedFlags_ {
    ImGuiFocusedFlags_None = 0,
    ImGuiFocusedFlags_ChildWindows = 1 << 0,
    ImGuiFocusedFlags_RootWindow = 1 << 1,
    ImGuiFocusedFlags_AnyWindow = 1 << 2,
    ImGuiFocusedFlags_RootAndChildWindows =
        ImGuiFocusedFlags_RootWindow | ImGuiFocusedFlags_ChildWindows
};
enum ImGuiHoveredFlags_ {
    ImGuiHoveredFlags_None = 0,
    ImGuiHoveredFlags_ChildWindows = 1 << 0,
    ImGuiHoveredFlags_RootWindow = 1 << 1,
    ImGuiHoveredFlags_AnyWindow = 1 << 2,
    ImGuiHoveredFlags_AllowWhenBlockedByPopup = 1 << 3,
    ImGuiHoveredFlags_AllowWhenBlockedByActiveItem = 1 << 5,
    ImGuiHoveredFlags_AllowWhenOverlapped = 1 << 6,
    ImGuiHoveredFlags_AllowWhenDisabled = 1 << 7,
    ImGuiHoveredFlags_RectOnly = ImGuiHoveredFlags_AllowWhenBlockedByPopup |
                                 ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
                                 ImGuiHoveredFlags_AllowWhenOverlapped,
    ImGuiHoveredFlags_RootAndChildWindows =
        ImGuiHoveredFlags_RootWindow | ImGuiHoveredFlags_ChildWindows
};
enum ImGuiDragDropFlags_ {
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
    ImGuiDragDropFlags_AcceptPeekOnly =
        ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect
};
enum ImGuiDataType_ {
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
};
enum ImGuiDir_ {
    ImGuiDir_None = -1,
    ImGuiDir_Left = 0,
    ImGuiDir_Right = 1,
    ImGuiDir_Up = 2,
    ImGuiDir_Down = 3,
    ImGuiDir_COUNT
};
enum ImGuiKey_ {
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
    ImGuiKey_A,
    ImGuiKey_C,
    ImGuiKey_V,
    ImGuiKey_X,
    ImGuiKey_Y,
    ImGuiKey_Z,
    ImGuiKey_COUNT
};
enum ImGuiNavInput_ {
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
    ImGuiNavInput_KeyTab_,
    ImGuiNavInput_KeyLeft_,
    ImGuiNavInput_KeyRight_,
    ImGuiNavInput_KeyUp_,
    ImGuiNavInput_KeyDown_,
    ImGuiNavInput_COUNT,
    ImGuiNavInput_InternalStart_ = ImGuiNavInput_KeyMenu_
};
enum ImGuiConfigFlags_ {
    ImGuiConfigFlags_None = 0,
    ImGuiConfigFlags_NavEnableKeyboard = 1 << 0,
    ImGuiConfigFlags_NavEnableGamepad = 1 << 1,
    ImGuiConfigFlags_NavEnableSetMousePos = 1 << 2,
    ImGuiConfigFlags_NavNoCaptureKeyboard = 1 << 3,
    ImGuiConfigFlags_NoMouse = 1 << 4,
    ImGuiConfigFlags_NoMouseCursorChange = 1 << 5,
    ImGuiConfigFlags_IsSRGB = 1 << 20,
    ImGuiConfigFlags_IsTouchScreen = 1 << 21
};
enum ImGuiBackendFlags_ {
    ImGuiBackendFlags_None = 0,
    ImGuiBackendFlags_HasGamepad = 1 << 0,
    ImGuiBackendFlags_HasMouseCursors = 1 << 1,
    ImGuiBackendFlags_HasSetMousePos = 1 << 2
};
enum ImGuiCol_ {
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
    ImGuiCol_PlotLines,
    ImGuiCol_PlotLinesHovered,
    ImGuiCol_PlotHistogram,
    ImGuiCol_PlotHistogramHovered,
    ImGuiCol_TextSelectedBg,
    ImGuiCol_DragDropTarget,
    ImGuiCol_NavHighlight,
    ImGuiCol_NavWindowingHighlight,
    ImGuiCol_NavWindowingDimBg,
    ImGuiCol_ModalWindowDimBg,
    ImGuiCol_COUNT
};
enum ImGuiStyleVar_ {
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
    ImGuiStyleVar_ScrollbarSize,
    ImGuiStyleVar_ScrollbarRounding,
    ImGuiStyleVar_GrabMinSize,
    ImGuiStyleVar_GrabRounding,
    ImGuiStyleVar_TabRounding,
    ImGuiStyleVar_ButtonTextAlign,
    ImGuiStyleVar_SelectableTextAlign,
    ImGuiStyleVar_COUNT
};
enum ImGuiColorEditFlags_ {
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
    ImGuiColorEditFlags_AlphaBar = 1 << 16,
    ImGuiColorEditFlags_AlphaPreview = 1 << 17,
    ImGuiColorEditFlags_AlphaPreviewHalf = 1 << 18,
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
    ImGuiColorEditFlags__OptionsDefault =
        ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB |
        ImGuiColorEditFlags_PickerHueBar,
    ImGuiColorEditFlags__DisplayMask = ImGuiColorEditFlags_DisplayRGB |
                                       ImGuiColorEditFlags_DisplayHSV |
                                       ImGuiColorEditFlags_DisplayHex,
    ImGuiColorEditFlags__DataTypeMask = ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_Float,
    ImGuiColorEditFlags__PickerMask =
        ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_PickerHueBar,
    ImGuiColorEditFlags__InputMask = ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_InputHSV
};
enum ImGuiMouseCursor_ {
    ImGuiMouseCursor_None = -1,
    ImGuiMouseCursor_Arrow = 0,
    ImGuiMouseCursor_TextInput,
    ImGuiMouseCursor_ResizeAll,
    ImGuiMouseCursor_ResizeNS,
    ImGuiMouseCursor_ResizeEW,
    ImGuiMouseCursor_ResizeNESW,
    ImGuiMouseCursor_ResizeNWSE,
    ImGuiMouseCursor_Hand,
    ImGuiMouseCursor_COUNT
};
enum ImGuiCond_ {
    ImGuiCond_Always = 1 << 0,
    ImGuiCond_Once = 1 << 1,
    ImGuiCond_FirstUseEver = 1 << 2,
    ImGuiCond_Appearing = 1 << 3
};
struct ImGuiStyle {
    float  Alpha;
    ImVec2 WindowPadding;
    float  WindowRounding;
    float  WindowBorderSize;
    ImVec2 WindowMinSize;
    ImVec2 WindowTitleAlign;
    float  ChildRounding;
    float  ChildBorderSize;
    float  PopupRounding;
    float  PopupBorderSize;
    ImVec2 FramePadding;
    float  FrameRounding;
    float  FrameBorderSize;
    ImVec2 ItemSpacing;
    ImVec2 ItemInnerSpacing;
    ImVec2 TouchExtraPadding;
    float  IndentSpacing;
    float  ColumnsMinSpacing;
    float  ScrollbarSize;
    float  ScrollbarRounding;
    float  GrabMinSize;
    float  GrabRounding;
    float  TabRounding;
    float  TabBorderSize;
    ImVec2 ButtonTextAlign;
    ImVec2 SelectableTextAlign;
    ImVec2 DisplayWindowPadding;
    ImVec2 DisplaySafeAreaPadding;
    float  MouseCursorScale;
    bool   AntiAliasedLines;
    bool   AntiAliasedFill;
    float  CurveTessellationTol;
    ImVec4 Colors[ImGuiCol_COUNT];
};
struct ImGuiIO {
    ImGuiConfigFlags  ConfigFlags;
    ImGuiBackendFlags BackendFlags;
    ImVec2            DisplaySize;
    float             DeltaTime;
    float             IniSavingRate;
    const char*       IniFilename;
    const char*       LogFilename;
    float             MouseDoubleClickTime;
    float             MouseDoubleClickMaxDist;
    float             MouseDragThreshold;
    int               KeyMap[ImGuiKey_COUNT];
    float             KeyRepeatDelay;
    float             KeyRepeatRate;
    void*             UserData;
    ImFontAtlas*      Fonts;
    float             FontGlobalScale;
    bool              FontAllowUserScaling;
    ImFont*           FontDefault;
    ImVec2            DisplayFramebufferScale;
    bool              MouseDrawCursor;
    bool              ConfigMacOSXBehaviors;
    bool              ConfigInputTextCursorBlink;
    bool              ConfigWindowsResizeFromEdges;
    bool              ConfigWindowsMoveFromTitleBarOnly;
    const char*       BackendPlatformName;
    const char*       BackendRendererName;
    void*             BackendPlatformUserData;
    void*             BackendRendererUserData;
    void*             BackendLanguageUserData;
    const char* (*GetClipboardTextFn)(void* user_data);
    void (*SetClipboardTextFn)(void* user_data, const char* text);
    void* ClipboardUserData;
    void (*ImeSetInputScreenPosFn)(int x, int y);
    void*            ImeWindowHandle;
    void*            RenderDrawListsFnUnused;
    ImVec2           MousePos;
    bool             MouseDown[5];
    float            MouseWheel;
    float            MouseWheelH;
    bool             KeyCtrl;
    bool             KeyShift;
    bool             KeyAlt;
    bool             KeySuper;
    bool             KeysDown[512];
    float            NavInputs[ImGuiNavInput_COUNT];
    bool             WantCaptureMouse;
    bool             WantCaptureKeyboard;
    bool             WantTextInput;
    bool             WantSetMousePos;
    bool             WantSaveIniSettings;
    bool             NavActive;
    bool             NavVisible;
    float            Framerate;
    int              MetricsRenderVertices;
    int              MetricsRenderIndices;
    int              MetricsRenderWindows;
    int              MetricsActiveWindows;
    int              MetricsActiveAllocations;
    ImVec2           MouseDelta;
    ImVec2           MousePosPrev;
    ImVec2           MouseClickedPos[5];
    double           MouseClickedTime[5];
    bool             MouseClicked[5];
    bool             MouseDoubleClicked[5];
    bool             MouseReleased[5];
    bool             MouseDownOwned[5];
    float            MouseDownDuration[5];
    float            MouseDownDurationPrev[5];
    ImVec2           MouseDragMaxDistanceAbs[5];
    float            MouseDragMaxDistanceSqr[5];
    float            KeysDownDuration[512];
    float            KeysDownDurationPrev[512];
    float            NavInputsDownDuration[ImGuiNavInput_COUNT];
    float            NavInputsDownDurationPrev[ImGuiNavInput_COUNT];
    ImVector_ImWchar InputQueueCharacters;
};
struct ImGuiInputTextCallbackData {
    ImGuiInputTextFlags EventFlag;
    ImGuiInputTextFlags Flags;
    void*               UserData;
    ImWchar             EventChar;
    ImGuiKey            EventKey;
    char*               Buf;
    int                 BufTextLen;
    int                 BufSize;
    bool                BufDirty;
    int                 CursorPos;
    int                 SelectionStart;
    int                 SelectionEnd;
};
struct ImGuiSizeCallbackData {
    void*  UserData;
    ImVec2 Pos;
    ImVec2 CurrentSize;
    ImVec2 DesiredSize;
};
struct ImGuiPayload {
    void*   Data;
    int     DataSize;
    ImGuiID SourceId;
    ImGuiID SourceParentId;
    int     DataFrameCount;
    char    DataType[32 + 1];
    bool    Preview;
    bool    Delivery;
};
struct ImGuiOnceUponAFrame {
    int RefFrame;
};
struct ImGuiTextFilter {
    char               InputBuf[256];
    ImVector_TextRange Filters;
    int                CountGrep;
};
struct ImGuiTextBuffer {
    ImVector_char Buf;
};
struct ImGuiStorage {
    ImVector_Pair Data;
};
struct ImGuiListClipper {
    float StartPosY;
    float ItemsHeight;
    int   ItemsCount, StepNo, DisplayStart, DisplayEnd;
};
struct ImColor {
    ImVec4 Value;
};
struct ImDrawCmd {
    unsigned int   ElemCount;
    ImVec4         ClipRect;
    ImTextureID    TextureId;
    ImDrawCallback UserCallback;
    void*          UserCallbackData;
};
struct ImDrawVert {
    ImVec2 pos;
    ImVec2 uv;
    ImU32  col;
};
struct ImDrawChannel {
    ImVector_ImDrawCmd CmdBuffer;
    ImVector_ImDrawIdx IdxBuffer;
};
enum ImDrawCornerFlags_ {
    ImDrawCornerFlags_TopLeft = 1 << 0,
    ImDrawCornerFlags_TopRight = 1 << 1,
    ImDrawCornerFlags_BotLeft = 1 << 2,
    ImDrawCornerFlags_BotRight = 1 << 3,
    ImDrawCornerFlags_Top = ImDrawCornerFlags_TopLeft | ImDrawCornerFlags_TopRight,
    ImDrawCornerFlags_Bot = ImDrawCornerFlags_BotLeft | ImDrawCornerFlags_BotRight,
    ImDrawCornerFlags_Left = ImDrawCornerFlags_TopLeft | ImDrawCornerFlags_BotLeft,
    ImDrawCornerFlags_Right = ImDrawCornerFlags_TopRight | ImDrawCornerFlags_BotRight,
    ImDrawCornerFlags_All = 0xF
};
enum ImDrawListFlags_ {
    ImDrawListFlags_None = 0,
    ImDrawListFlags_AntiAliasedLines = 1 << 0,
    ImDrawListFlags_AntiAliasedFill = 1 << 1
};
struct ImDrawList {
    ImVector_ImDrawCmd          CmdBuffer;
    ImVector_ImDrawIdx          IdxBuffer;
    ImVector_ImDrawVert         VtxBuffer;
    ImDrawListFlags             Flags;
    const ImDrawListSharedData* _Data;
    const char*                 _OwnerName;
    unsigned int                _VtxCurrentIdx;
    ImDrawVert*                 _VtxWritePtr;
    ImDrawIdx*                  _IdxWritePtr;
    ImVector_ImVec4             _ClipRectStack;
    ImVector_ImTextureID        _TextureIdStack;
    ImVector_ImVec2             _Path;
    int                         _ChannelsCurrent;
    int                         _ChannelsCount;
    ImVector_ImDrawChannel      _Channels;
};
struct ImDrawData {
    bool         Valid;
    ImDrawList** CmdLists;
    int          CmdListsCount;
    int          TotalIdxCount;
    int          TotalVtxCount;
    ImVec2       DisplayPos;
    ImVec2       DisplaySize;
    ImVec2       FramebufferScale;
};
struct ImFontConfig {
    void*          FontData;
    int            FontDataSize;
    bool           FontDataOwnedByAtlas;
    int            FontNo;
    float          SizePixels;
    int            OversampleH;
    int            OversampleV;
    bool           PixelSnapH;
    ImVec2         GlyphExtraSpacing;
    ImVec2         GlyphOffset;
    const ImWchar* GlyphRanges;
    float          GlyphMinAdvanceX;
    float          GlyphMaxAdvanceX;
    bool           MergeMode;
    unsigned int   RasterizerFlags;
    float          RasterizerMultiply;
    char           Name[40];
    ImFont*        DstFont;
};
struct ImFontGlyph {
    ImWchar Codepoint;
    float   AdvanceX;
    float   X0, Y0, X1, Y1;
    float   U0, V0, U1, V1;
};
struct ImFontGlyphRangesBuilder {
    ImVector_int UsedChars;
};
enum ImFontAtlasFlags_ {
    ImFontAtlasFlags_None = 0,
    ImFontAtlasFlags_NoPowerOfTwoHeight = 1 << 0,
    ImFontAtlasFlags_NoMouseCursors = 1 << 1
};
struct ImFontAtlas {
    bool                  Locked;
    ImFontAtlasFlags      Flags;
    ImTextureID           TexID;
    int                   TexDesiredWidth;
    int                   TexGlyphPadding;
    unsigned char*        TexPixelsAlpha8;
    unsigned int*         TexPixelsRGBA32;
    int                   TexWidth;
    int                   TexHeight;
    ImVec2                TexUvScale;
    ImVec2                TexUvWhitePixel;
    ImVector_ImFontPtr    Fonts;
    ImVector_CustomRect   CustomRects;
    ImVector_ImFontConfig ConfigData;
    int                   CustomRectIds[1];
};
struct ImFont {
    ImVector_float       IndexAdvanceX;
    float                FallbackAdvanceX;
    float                FontSize;
    ImVector_ImWchar     IndexLookup;
    ImVector_ImFontGlyph Glyphs;
    const ImFontGlyph*   FallbackGlyph;
    ImVec2               DisplayOffset;
    ImFontAtlas*         ContainerAtlas;
    const ImFontConfig*  ConfigData;
    short                ConfigDataCount;
    ImWchar              FallbackChar;
    float                Scale;
    float                Ascent, Descent;
    int                  MetricsTotalSurface;
    bool                 DirtyLookupTables;
};
struct TextRange {
    const char* b;
    const char* e;
};
struct Pair {
    ImGuiID key;
    union {
        int   val_i;
        float val_f;
        void* val_p;
    };
};
struct CustomRect {
    unsigned int   ID;
    unsigned short Width, Height;
    unsigned short X, Y;
    float          GlyphAdvanceX;
    ImVec2         GlyphOffset;
    ImFont*        Font;
};

typedef struct rizz_api_imgui {
    ImGuiContext* (*CreateContext)(ImFontAtlas* shared_font_atlas);
    void (*DestroyContext)(ImGuiContext* ctx);
    ImGuiContext* (*GetCurrentContext)(void);
    void (*SetCurrentContext)(ImGuiContext* ctx);
    bool (*DebugCheckVersionAndDataLayout)(const char* version_str, size_t sz_io, size_t sz_style,
                                           size_t sz_vec2, size_t sz_vec4, size_t sz_drawvert);
    ImGuiIO* (*GetIO)(void);
    ImGuiStyle* (*GetStyle)(void);
    void (*NewFrame)(void);
    void (*EndFrame)(void);
    void (*Render)(void);
    ImDrawData* (*GetDrawData)(void);
    void (*ShowDemoWindow)(bool* p_open);
    void (*ShowAboutWindow)(bool* p_open);
    void (*ShowMetricsWindow)(bool* p_open);
    void (*ShowStyleEditor)(ImGuiStyle* ref);
    bool (*ShowStyleSelector)(const char* label);
    void (*ShowFontSelector)(const char* label);
    void (*ShowUserGuide)(void);
    const char* (*GetVersion)(void);
    void (*StyleColorsDark)(ImGuiStyle* dst);
    void (*StyleColorsClassic)(ImGuiStyle* dst);
    void (*StyleColorsLight)(ImGuiStyle* dst);
    bool (*Begin)(const char* name, bool* p_open, ImGuiWindowFlags flags);
    void (*End)(void);
    bool (*BeginChild)(const char* str_id, const ImVec2 size, bool border, ImGuiWindowFlags flags);
    bool (*BeginChildID)(ImGuiID id, const ImVec2 size, bool border, ImGuiWindowFlags flags);
    void (*EndChild)(void);
    bool (*IsWindowAppearing)(void);
    bool (*IsWindowCollapsed)(void);
    bool (*IsWindowFocused)(ImGuiFocusedFlags flags);
    bool (*IsWindowHovered)(ImGuiHoveredFlags flags);
    ImDrawList* (*GetWindowDrawList)(void);
    float (*GetWindowWidth)(void);
    float (*GetWindowHeight)(void);
    float (*GetContentRegionAvailWidth)(void);
    float (*GetWindowContentRegionWidth)(void);
    void (*SetNextWindowPos)(const ImVec2 pos, ImGuiCond cond, const ImVec2 pivot);
    void (*SetNextWindowSize)(const ImVec2 size, ImGuiCond cond);
    void (*SetNextWindowSizeConstraints)(const ImVec2 size_min, const ImVec2 size_max,
                                         ImGuiSizeCallback custom_callback,
                                         void*             custom_callback_data);
    void (*SetNextWindowContentSize)(const ImVec2 size);
    void (*SetNextWindowCollapsed)(bool collapsed, ImGuiCond cond);
    void (*SetNextWindowFocus)(void);
    void (*SetNextWindowBgAlpha)(float alpha);
    void (*SetWindowPosVec2)(const ImVec2 pos, ImGuiCond cond);
    void (*SetWindowSizeVec2)(const ImVec2 size, ImGuiCond cond);
    void (*SetWindowCollapsedBool)(bool collapsed, ImGuiCond cond);
    void (*SetWindowFocus)(void);
    void (*SetWindowFontScale)(float scale);
    void (*SetWindowPosStr)(const char* name, const ImVec2 pos, ImGuiCond cond);
    void (*SetWindowSizeStr)(const char* name, const ImVec2 size, ImGuiCond cond);
    void (*SetWindowCollapsedStr)(const char* name, bool collapsed, ImGuiCond cond);
    void (*SetWindowFocusStr)(const char* name);
    float (*GetScrollX)(void);
    float (*GetScrollY)(void);
    float (*GetScrollMaxX)(void);
    float (*GetScrollMaxY)(void);
    void (*SetScrollX)(float scroll_x);
    void (*SetScrollY)(float scroll_y);
    void (*SetScrollHereY)(float center_y_ratio);
    void (*SetScrollFromPosY)(float local_y, float center_y_ratio);
    void (*PushFont)(ImFont* font);
    void (*PopFont)(void);
    void (*PushStyleColorU32)(ImGuiCol idx, ImU32 col);
    void (*PushStyleColor)(ImGuiCol idx, const ImVec4 col);
    void (*PopStyleColor)(int count);
    void (*PushStyleVarFloat)(ImGuiStyleVar idx, float val);
    void (*PushStyleVarVec2)(ImGuiStyleVar idx, const ImVec2 val);
    void (*PopStyleVar)(int count);
    const ImVec4* (*GetStyleColorVec4)(ImGuiCol idx);
    ImFont* (*GetFont)(void);
    float (*GetFontSize)(void);
    ImU32 (*GetColorU32)(ImGuiCol idx, float alpha_mul);
    ImU32 (*GetColorU32Vec4)(const ImVec4 col);
    ImU32 (*GetColorU32U32)(ImU32 col);
    void (*PushItemWidth)(float item_width);
    void (*PopItemWidth)(void);
    float (*CalcItemWidth)(void);
    void (*PushTextWrapPos)(float wrap_local_pos_x);
    void (*PopTextWrapPos)(void);
    void (*PushAllowKeyboardFocus)(bool allow_keyboard_focus);
    void (*PopAllowKeyboardFocus)(void);
    void (*PushButtonRepeat)(bool repeat);
    void (*PopButtonRepeat)(void);
    void (*Separator)(void);
    void (*SameLine)(float local_pos_x, float spacing_w);
    void (*NewLine)(void);
    void (*Spacing)(void);
    void (*Dummy)(const ImVec2 size);
    void (*Indent)(float indent_w);
    void (*Unindent)(float indent_w);
    void (*BeginGroup)(void);
    void (*EndGroup)(void);
    float (*GetCursorPosX)(void);
    float (*GetCursorPosY)(void);
    void (*SetCursorPos)(const ImVec2 local_pos);
    void (*SetCursorPosX)(float local_x);
    void (*SetCursorPosY)(float local_y);
    void (*SetCursorScreenPos)(const ImVec2 pos);
    void (*AlignTextToFramePadding)(void);
    float (*GetTextLineHeight)(void);
    float (*GetTextLineHeightWithSpacing)(void);
    float (*GetFrameHeight)(void);
    float (*GetFrameHeightWithSpacing)(void);
    void (*PushIDStr)(const char* str_id);
    void (*PushIDRange)(const char* str_id_begin, const char* str_id_end);
    void (*PushIDPtr)(const void* ptr_id);
    void (*PushIDInt)(int int_id);
    void (*PopID)(void);
    ImGuiID (*GetIDStr)(const char* str_id);
    ImGuiID (*GetIDRange)(const char* str_id_begin, const char* str_id_end);
    ImGuiID (*GetIDPtr)(const void* ptr_id);
    void (*TextUnformatted)(const char* text, const char* text_end);
    void (*Text)(const char* fmt, ...);
    void (*TextV)(const char* fmt, va_list args);
    void (*TextColored)(const ImVec4 col, const char* fmt, ...);
    void (*TextColoredV)(const ImVec4 col, const char* fmt, va_list args);
    void (*TextDisabled)(const char* fmt, ...);
    void (*TextDisabledV)(const char* fmt, va_list args);
    void (*TextWrapped)(const char* fmt, ...);
    void (*TextWrappedV)(const char* fmt, va_list args);
    void (*LabelText)(const char* label, const char* fmt, ...);
    void (*LabelTextV)(const char* label, const char* fmt, va_list args);
    void (*BulletText)(const char* fmt, ...);
    void (*BulletTextV)(const char* fmt, va_list args);
    bool (*Button)(const char* label, const ImVec2 size);
    bool (*SmallButton)(const char* label);
    bool (*InvisibleButton)(const char* str_id, const ImVec2 size);
    bool (*ArrowButton)(const char* str_id, ImGuiDir dir);
    void (*Image)(ImTextureID user_texture_id, const ImVec2 size, const ImVec2 uv0,
                  const ImVec2 uv1, const ImVec4 tint_col, const ImVec4 border_col);
    bool (*ImageButton)(ImTextureID user_texture_id, const ImVec2 size, const ImVec2 uv0,
                        const ImVec2 uv1, int frame_padding, const ImVec4 bg_col,
                        const ImVec4 tint_col);
    bool (*Checkbox)(const char* label, bool* v);
    bool (*CheckboxFlags)(const char* label, unsigned int* flags, unsigned int flags_value);
    bool (*RadioButtonBool)(const char* label, bool active);
    bool (*RadioButtonIntPtr)(const char* label, int* v, int v_button);
    void (*ProgressBar)(float fraction, const ImVec2 size_arg, const char* overlay);
    void (*Bullet)(void);
    bool (*BeginCombo)(const char* label, const char* preview_value, ImGuiComboFlags flags);
    void (*EndCombo)(void);
    bool (*Combo)(const char* label, int* current_item, const char* const items[], int items_count,
                  int popup_max_height_in_items);
    bool (*ComboStr)(const char* label, int* current_item, const char* items_separated_by_zeros,
                     int popup_max_height_in_items);
    bool (*ComboFnPtr)(const char* label, int* current_item,
                       bool (*items_getter)(void* data, int idx, const char** out_text), void* data,
                       int items_count, int popup_max_height_in_items);
    bool (*DragFloat)(const char* label, float* v, float v_speed, float v_min, float v_max,
                      const char* format, float power);
    bool (*DragFloat2)(const char* label, float v[2], float v_speed, float v_min, float v_max,
                       const char* format, float power);
    bool (*DragFloat3)(const char* label, float v[3], float v_speed, float v_min, float v_max,
                       const char* format, float power);
    bool (*DragFloat4)(const char* label, float v[4], float v_speed, float v_min, float v_max,
                       const char* format, float power);
    bool (*DragFloatRange2)(const char* label, float* v_current_min, float* v_current_max,
                            float v_speed, float v_min, float v_max, const char* format,
                            const char* format_max, float power);
    bool (*DragInt)(const char* label, int* v, float v_speed, int v_min, int v_max,
                    const char* format);
    bool (*DragInt2)(const char* label, int v[2], float v_speed, int v_min, int v_max,
                     const char* format);
    bool (*DragInt3)(const char* label, int v[3], float v_speed, int v_min, int v_max,
                     const char* format);
    bool (*DragInt4)(const char* label, int v[4], float v_speed, int v_min, int v_max,
                     const char* format);
    bool (*DragIntRange2)(const char* label, int* v_current_min, int* v_current_max, float v_speed,
                          int v_min, int v_max, const char* format, const char* format_max);
    bool (*DragScalar)(const char* label, ImGuiDataType data_type, void* v, float v_speed,
                       const void* v_min, const void* v_max, const char* format, float power);
    bool (*DragScalarN)(const char* label, ImGuiDataType data_type, void* v, int components,
                        float v_speed, const void* v_min, const void* v_max, const char* format,
                        float power);
    bool (*SliderFloat)(const char* label, float* v, float v_min, float v_max, const char* format,
                        float power);
    bool (*SliderFloat2)(const char* label, float v[2], float v_min, float v_max,
                         const char* format, float power);
    bool (*SliderFloat3)(const char* label, float v[3], float v_min, float v_max,
                         const char* format, float power);
    bool (*SliderFloat4)(const char* label, float v[4], float v_min, float v_max,
                         const char* format, float power);
    bool (*SliderAngle)(const char* label, float* v_rad, float v_degrees_min, float v_degrees_max,
                        const char* format);
    bool (*SliderInt)(const char* label, int* v, int v_min, int v_max, const char* format);
    bool (*SliderInt2)(const char* label, int v[2], int v_min, int v_max, const char* format);
    bool (*SliderInt3)(const char* label, int v[3], int v_min, int v_max, const char* format);
    bool (*SliderInt4)(const char* label, int v[4], int v_min, int v_max, const char* format);
    bool (*SliderScalar)(const char* label, ImGuiDataType data_type, void* v, const void* v_min,
                         const void* v_max, const char* format, float power);
    bool (*SliderScalarN)(const char* label, ImGuiDataType data_type, void* v, int components,
                          const void* v_min, const void* v_max, const char* format, float power);
    bool (*VSliderFloat)(const char* label, const ImVec2 size, float* v, float v_min, float v_max,
                         const char* format, float power);
    bool (*VSliderInt)(const char* label, const ImVec2 size, int* v, int v_min, int v_max,
                       const char* format);
    bool (*VSliderScalar)(const char* label, const ImVec2 size, ImGuiDataType data_type, void* v,
                          const void* v_min, const void* v_max, const char* format, float power);
    bool (*InputText)(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags,
                      ImGuiInputTextCallback callback, void* user_data);
    bool (*InputTextMultiline)(const char* label, char* buf, size_t buf_size, const ImVec2 size,
                               ImGuiInputTextFlags flags, ImGuiInputTextCallback callback,
                               void* user_data);
    bool (*InputTextWithHint)(const char* label, const char* hint, char* buf, size_t buf_size,
                              ImGuiInputTextFlags flags, ImGuiInputTextCallback callback,
                              void* user_data);
    bool (*InputFloat)(const char* label, float* v, float step, float step_fast, const char* format,
                       ImGuiInputTextFlags flags);
    bool (*InputFloat2)(const char* label, float v[2], const char* format,
                        ImGuiInputTextFlags flags);
    bool (*InputFloat3)(const char* label, float v[3], const char* format,
                        ImGuiInputTextFlags flags);
    bool (*InputFloat4)(const char* label, float v[4], const char* format,
                        ImGuiInputTextFlags flags);
    bool (*InputInt)(const char* label, int* v, int step, int step_fast, ImGuiInputTextFlags flags);
    bool (*InputInt2)(const char* label, int v[2], ImGuiInputTextFlags flags);
    bool (*InputInt3)(const char* label, int v[3], ImGuiInputTextFlags flags);
    bool (*InputInt4)(const char* label, int v[4], ImGuiInputTextFlags flags);
    bool (*InputDouble)(const char* label, double* v, double step, double step_fast,
                        const char* format, ImGuiInputTextFlags flags);
    bool (*InputScalar)(const char* label, ImGuiDataType data_type, void* v, const void* step,
                        const void* step_fast, const char* format, ImGuiInputTextFlags flags);
    bool (*InputScalarN)(const char* label, ImGuiDataType data_type, void* v, int components,
                         const void* step, const void* step_fast, const char* format,
                         ImGuiInputTextFlags flags);
    bool (*ColorEdit3)(const char* label, float col[3], ImGuiColorEditFlags flags);
    bool (*ColorEdit4)(const char* label, float col[4], ImGuiColorEditFlags flags);
    bool (*ColorPicker3)(const char* label, float col[3], ImGuiColorEditFlags flags);
    bool (*ColorPicker4)(const char* label, float col[4], ImGuiColorEditFlags flags,
                         const float* ref_col);
    bool (*ColorButton)(const char* desc_id, const ImVec4 col, ImGuiColorEditFlags flags,
                        ImVec2 size);
    void (*SetColorEditOptions)(ImGuiColorEditFlags flags);
    bool (*TreeNodeStr)(const char* label);
    bool (*TreeNodeStrStr)(const char* str_id, const char* fmt, ...);
    bool (*TreeNodePtr)(const void* ptr_id, const char* fmt, ...);
    bool (*TreeNodeVStr)(const char* str_id, const char* fmt, va_list args);
    bool (*TreeNodeVPtr)(const void* ptr_id, const char* fmt, va_list args);
    bool (*TreeNodeExStr)(const char* label, ImGuiTreeNodeFlags flags);
    bool (*TreeNodeExStrStr)(const char* str_id, ImGuiTreeNodeFlags flags, const char* fmt, ...);
    bool (*TreeNodeExPtr)(const void* ptr_id, ImGuiTreeNodeFlags flags, const char* fmt, ...);
    bool (*TreeNodeExVStr)(const char* str_id, ImGuiTreeNodeFlags flags, const char* fmt,
                           va_list args);
    bool (*TreeNodeExVPtr)(const void* ptr_id, ImGuiTreeNodeFlags flags, const char* fmt,
                           va_list args);
    void (*TreePushStr)(const char* str_id);
    void (*TreePushPtr)(const void* ptr_id);
    void (*TreePop)(void);
    void (*TreeAdvanceToLabelPos)(void);
    float (*GetTreeNodeToLabelSpacing)(void);
    void (*SetNextTreeNodeOpen)(bool is_open, ImGuiCond cond);
    bool (*CollapsingHeader)(const char* label, ImGuiTreeNodeFlags flags);
    bool (*CollapsingHeaderBoolPtr)(const char* label, bool* p_open, ImGuiTreeNodeFlags flags);
    bool (*Selectable)(const char* label, bool selected, ImGuiSelectableFlags flags,
                       const ImVec2 size);
    bool (*SelectableBoolPtr)(const char* label, bool* p_selected, ImGuiSelectableFlags flags,
                              const ImVec2 size);
    bool (*ListBoxStr_arr)(const char* label, int* current_item, const char* const items[],
                           int items_count, int height_in_items);
    bool (*ListBoxFnPtr)(const char* label, int* current_item,
                         bool (*items_getter)(void* data, int idx, const char** out_text),
                         void* data, int items_count, int height_in_items);
    bool (*ListBoxHeaderVec2)(const char* label, const ImVec2 size);
    bool (*ListBoxHeaderInt)(const char* label, int items_count, int height_in_items);
    void (*ListBoxFooter)(void);
    void (*PlotLines)(const char* label, const float* values, int values_count, int values_offset,
                      const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size,
                      int stride);
    void (*PlotLinesFnPtr)(const char* label, float (*values_getter)(void* data, int idx),
                           void* data, int values_count, int values_offset,
                           const char* overlay_text, float scale_min, float scale_max,
                           ImVec2 graph_size);
    void (*PlotHistogramFloatPtr)(const char* label, const float* values, int values_count,
                                  int values_offset, const char* overlay_text, float scale_min,
                                  float scale_max, ImVec2 graph_size, int stride);
    void (*PlotHistogramFnPtr)(const char* label, float (*values_getter)(void* data, int idx),
                               void* data, int values_count, int values_offset,
                               const char* overlay_text, float scale_min, float scale_max,
                               ImVec2 graph_size);
    void (*ValueBool)(const char* prefix, bool b);
    void (*ValueInt)(const char* prefix, int v);
    void (*ValueUint)(const char* prefix, unsigned int v);
    void (*ValueFloat)(const char* prefix, float v, const char* float_format);
    bool (*BeginMainMenuBar)(void);
    void (*EndMainMenuBar)(void);
    bool (*BeginMenuBar)(void);
    void (*EndMenuBar)(void);
    bool (*BeginMenu)(const char* label, bool enabled);
    void (*EndMenu)(void);
    bool (*MenuItemBool)(const char* label, const char* shortcut, bool selected, bool enabled);
    bool (*MenuItemBoolPtr)(const char* label, const char* shortcut, bool* p_selected,
                            bool enabled);
    void (*BeginTooltip)(void);
    void (*EndTooltip)(void);
    void (*SetTooltip)(const char* fmt, ...);
    void (*SetTooltipV)(const char* fmt, va_list args);
    void (*OpenPopup)(const char* str_id);
    bool (*BeginPopup)(const char* str_id, ImGuiWindowFlags flags);
    bool (*BeginPopupContextItem)(const char* str_id, int mouse_button);
    bool (*BeginPopupContextWindow)(const char* str_id, int mouse_button, bool also_over_items);
    bool (*BeginPopupContextVoid)(const char* str_id, int mouse_button);
    bool (*BeginPopupModal)(const char* name, bool* p_open, ImGuiWindowFlags flags);
    void (*EndPopup)(void);
    bool (*OpenPopupOnItemClick)(const char* str_id, int mouse_button);
    bool (*IsPopupOpen)(const char* str_id);
    void (*CloseCurrentPopup)(void);
    void (*Columns)(int count, const char* id, bool border);
    void (*NextColumn)(void);
    int (*GetColumnIndex)(void);
    float (*GetColumnWidth)(int column_index);
    void (*SetColumnWidth)(int column_index, float width);
    float (*GetColumnOffset)(int column_index);
    void (*SetColumnOffset)(int column_index, float offset_x);
    int (*GetColumnsCount)(void);
    bool (*BeginTabBar)(const char* str_id, ImGuiTabBarFlags flags);
    void (*EndTabBar)(void);
    bool (*BeginTabItem)(const char* label, bool* p_open, ImGuiTabItemFlags flags);
    void (*EndTabItem)(void);
    void (*SetTabItemClosed)(const char* tab_or_docked_window_label);
    void (*LogToTTY)(int auto_open_depth);
    void (*LogToFile)(int auto_open_depth, const char* filename);
    void (*LogToClipboard)(int auto_open_depth);
    void (*LogFinish)(void);
    void (*LogButtons)(void);
    bool (*BeginDragDropSource)(ImGuiDragDropFlags flags);
    bool (*SetDragDropPayload)(const char* type, const void* data, size_t sz, ImGuiCond cond);
    void (*EndDragDropSource)(void);
    bool (*BeginDragDropTarget)(void);
    const ImGuiPayload* (*AcceptDragDropPayload)(const char* type, ImGuiDragDropFlags flags);
    void (*EndDragDropTarget)(void);
    const ImGuiPayload* (*GetDragDropPayload)(void);
    void (*PushClipRect)(const ImVec2 clip_rect_min, const ImVec2 clip_rect_max,
                         bool intersect_with_current_clip_rect);
    void (*PopClipRect)(void);
    void (*SetItemDefaultFocus)(void);
    void (*SetKeyboardFocusHere)(int offset);
    bool (*IsItemHovered)(ImGuiHoveredFlags flags);
    bool (*IsItemActive)(void);
    bool (*IsItemFocused)(void);
    bool (*IsItemClicked)(int mouse_button);
    bool (*IsItemVisible)(void);
    bool (*IsItemEdited)(void);
    bool (*IsItemActivated)(void);
    bool (*IsItemDeactivated)(void);
    bool (*IsItemDeactivatedAfterEdit)(void);
    bool (*IsAnyItemHovered)(void);
    bool (*IsAnyItemActive)(void);
    bool (*IsAnyItemFocused)(void);
    void (*SetItemAllowOverlap)(void);
    bool (*IsRectVisible)(const ImVec2 size);
    bool (*IsRectVisibleVec2)(const ImVec2 rect_min, const ImVec2 rect_max);
    double (*GetTime)(void);
    int (*GetFrameCount)(void);
    ImDrawList* (*GetBackgroundDrawList)(void);
    ImDrawList* (*GetForegroundDrawList)(void);
    ImDrawListSharedData* (*GetDrawListSharedData)(void);
    const char* (*GetStyleColorName)(ImGuiCol idx);
    void (*SetStateStorage)(ImGuiStorage* storage);
    ImGuiStorage* (*GetStateStorage)(void);
    void (*CalcListClipping)(int items_count, float items_height, int* out_items_display_start,
                             int* out_items_display_end);
    bool (*BeginChildFrame)(ImGuiID id, const ImVec2 size, ImGuiWindowFlags flags);
    void (*EndChildFrame)(void);
    ImU32 (*ColorConvertFloat4ToU32)(const ImVec4 in);
    int (*GetKeyIndex)(ImGuiKey imgui_key);
    bool (*IsKeyDown)(int user_key_index);
    bool (*IsKeyPressed)(int user_key_index, bool repeat);
    bool (*IsKeyReleased)(int user_key_index);
    int (*GetKeyPressedAmount)(int key_index, float repeat_delay, float rate);
    bool (*IsMouseDown)(int button);
    bool (*IsAnyMouseDown)(void);
    bool (*IsMouseClicked)(int button, bool repeat);
    bool (*IsMouseDoubleClicked)(int button);
    bool (*IsMouseReleased)(int button);
    bool (*IsMouseDragging)(int button, float lock_threshold);
    bool (*IsMouseHoveringRect)(const ImVec2 r_min, const ImVec2 r_max, bool clip);
    bool (*IsMousePosValid)(const ImVec2* mouse_pos);
    void (*ResetMouseDragDelta)(int button);
    ImGuiMouseCursor (*GetMouseCursor)(void);
    void (*SetMouseCursor)(ImGuiMouseCursor type);
    void (*CaptureKeyboardFromApp)(bool want_capture_keyboard_value);
    void (*CaptureMouseFromApp)(bool want_capture_mouse_value);
    const char* (*GetClipboardText)(void);
    void (*SetClipboardText)(const char* text);
    void (*LoadIniSettingsFromDisk)(const char* ini_filename);
    void (*LoadIniSettingsFromMemory)(const char* ini_data, size_t ini_size);
    void (*SaveIniSettingsToDisk)(const char* ini_filename);
    const char* (*SaveIniSettingsToMemory)(size_t* out_ini_size);
    void (*SetAllocatorFunctions)(void* (*alloc_func)(size_t sz, void* user_data),
                                  void (*free_func)(void* ptr, void* user_data), void* user_data);
    void* (*MemAlloc)(size_t size);
    void (*MemFree)(void* ptr);
    void (*GetWindowPos_nonUDT)(ImVec2* pOut);
    void (*GetWindowSize_nonUDT)(ImVec2* pOut);
    void (*GetContentRegionMax_nonUDT)(ImVec2* pOut);
    void (*GetContentRegionAvail_nonUDT)(ImVec2* pOut);
    void (*GetWindowContentRegionMin_nonUDT)(ImVec2* pOut);
    void (*GetWindowContentRegionMax_nonUDT)(ImVec2* pOut);
    void (*GetFontTexUvWhitePixel_nonUDT)(ImVec2* pOut);
    void (*GetCursorPos_nonUDT)(ImVec2* pOut);
    void (*GetCursorStartPos_nonUDT)(ImVec2* pOut);
    void (*GetCursorScreenPos_nonUDT)(ImVec2* pOut);
    void (*GetItemRectMin_nonUDT)(ImVec2* pOut);
    void (*GetItemRectMax_nonUDT)(ImVec2* pOut);
    void (*GetItemRectSize_nonUDT)(ImVec2* pOut);
    void (*CalcTextSize_nonUDT)(ImVec2* pOut, const char* text, const char* text_end,
                                bool hide_text_after_double_hash, float wrap_width);
    void (*ColorConvertU32ToFloat4_nonUDT)(ImVec4* pOut, ImU32 in);
    void (*GetMousePos_nonUDT)(ImVec2* pOut);
    void (*GetMousePosOnOpeningCurrentPopup_nonUDT)(ImVec2* pOut);
    void (*GetMouseDragDelta_nonUDT)(ImVec2* pOut, int button, float lock_threshold);
    void (*LogText)(const char* fmt, ...);
    float (*GET_FLT_MAX)();
    void (*ColorConvertRGBtoHSV)(float r, float g, float b, float* out_h, float* out_s,
                                 float* out_v);
    void (*ColorConvertHSVtoRGB)(float h, float s, float v, float* out_r, float* out_g,
                                 float* out_b);
    // ImGuiIO
    void (*ImGuiIO_AddInputCharacter)(ImGuiIO* self, ImWchar c);
    void (*ImGuiIO_AddInputCharactersUTF8)(ImGuiIO* self, const char* str);
    void (*ImGuiIO_ClearInputCharacters)(ImGuiIO* self);
    ImGuiIO* (*ImGuiIO_ImGuiIO)(void);
    void (*ImGuiIO_destroy)(ImGuiIO* self);
    // ImDrawList
    ImDrawList* (*ImDrawList_ImDrawList)(const ImDrawListSharedData* shared_data);
    void (*ImDrawList_destroy)(ImDrawList* self);
    void (*ImDrawList_PushClipRect)(ImDrawList* self, ImVec2 clip_rect_min, ImVec2 clip_rect_max,
                                    bool intersect_with_current_clip_rect);
    void (*ImDrawList_PushClipRectFullScreen)(ImDrawList* self);
    void (*ImDrawList_PopClipRect)(ImDrawList* self);
    void (*ImDrawList_PushTextureID)(ImDrawList* self, ImTextureID texture_id);
    void (*ImDrawList_PopTextureID)(ImDrawList* self);
    void (*ImDrawList_AddLine)(ImDrawList* self, const ImVec2 a, const ImVec2 b, ImU32 col,
                               float thickness);
    void (*ImDrawList_AddRect)(ImDrawList* self, const ImVec2 a, const ImVec2 b, ImU32 col,
                               float rounding, int rounding_corners_flags, float thickness);
    void (*ImDrawList_AddRectFilled)(ImDrawList* self, const ImVec2 a, const ImVec2 b, ImU32 col,
                                     float rounding, int rounding_corners_flags);
    void (*ImDrawList_AddRectFilledMultiColor)(ImDrawList* self, const ImVec2 a, const ImVec2 b,
                                               ImU32 col_upr_left, ImU32 col_upr_right,
                                               ImU32 col_bot_right, ImU32 col_bot_left);
    void (*ImDrawList_AddQuad)(ImDrawList* self, const ImVec2 a, const ImVec2 b, const ImVec2 c,
                               const ImVec2 d, ImU32 col, float thickness);
    void (*ImDrawList_AddQuadFilled)(ImDrawList* self, const ImVec2 a, const ImVec2 b,
                                     const ImVec2 c, const ImVec2 d, ImU32 col);
    void (*ImDrawList_AddTriangle)(ImDrawList* self, const ImVec2 a, const ImVec2 b, const ImVec2 c,
                                   ImU32 col, float thickness);
    void (*ImDrawList_AddTriangleFilled)(ImDrawList* self, const ImVec2 a, const ImVec2 b,
                                         const ImVec2 c, ImU32 col);
    void (*ImDrawList_AddCircle)(ImDrawList* self, const ImVec2 centre, float radius, ImU32 col,
                                 int num_segments, float thickness);
    void (*ImDrawList_AddCircleFilled)(ImDrawList* self, const ImVec2 centre, float radius,
                                       ImU32 col, int num_segments);
    void (*ImDrawList_AddText)(ImDrawList* self, const ImVec2 pos, ImU32 col,
                               const char* text_begin, const char* text_end);
    void (*ImDrawList_AddTextFontPtr)(ImDrawList* self, const ImFont* font, float font_size,
                                      const ImVec2 pos, ImU32 col, const char* text_begin,
                                      const char* text_end, float wrap_width,
                                      const ImVec4* cpu_fine_clip_rect);
    void (*ImDrawList_AddImage)(ImDrawList* self, ImTextureID user_texture_id, const ImVec2 a,
                                const ImVec2 b, const ImVec2 uv_a, const ImVec2 uv_b, ImU32 col);
    void (*ImDrawList_AddImageQuad)(ImDrawList* self, ImTextureID user_texture_id, const ImVec2 a,
                                    const ImVec2 b, const ImVec2 c, const ImVec2 d,
                                    const ImVec2 uv_a, const ImVec2 uv_b, const ImVec2 uv_c,
                                    const ImVec2 uv_d, ImU32 col);
    void (*ImDrawList_AddImageRounded)(ImDrawList* self, ImTextureID user_texture_id,
                                       const ImVec2 a, const ImVec2 b, const ImVec2 uv_a,
                                       const ImVec2 uv_b, ImU32 col, float rounding,
                                       int rounding_corners);
    void (*ImDrawList_AddPolyline)(ImDrawList* self, const ImVec2* points, int num_points,
                                   ImU32 col, bool closed, float thickness);
    void (*ImDrawList_AddConvexPolyFilled)(ImDrawList* self, const ImVec2* points, int num_points,
                                           ImU32 col);
    void (*ImDrawList_AddBezierCurve)(ImDrawList* self, const ImVec2 pos0, const ImVec2 cp0,
                                      const ImVec2 cp1, const ImVec2 pos1, ImU32 col,
                                      float thickness, int num_segments);
    void (*ImDrawList_PathClear)(ImDrawList* self);
    void (*ImDrawList_PathLineTo)(ImDrawList* self, const ImVec2 pos);
    void (*ImDrawList_PathLineToMergeDuplicate)(ImDrawList* self, const ImVec2 pos);
    void (*ImDrawList_PathFillConvex)(ImDrawList* self, ImU32 col);
    void (*ImDrawList_PathStroke)(ImDrawList* self, ImU32 col, bool closed, float thickness);
    void (*ImDrawList_PathArcTo)(ImDrawList* self, const ImVec2 centre, float radius, float a_min,
                                 float a_max, int num_segments);
    void (*ImDrawList_PathArcToFast)(ImDrawList* self, const ImVec2 centre, float radius,
                                     int a_min_of_12, int a_max_of_12);
    void (*ImDrawList_PathBezierCurveTo)(ImDrawList* self, const ImVec2 p1, const ImVec2 p2,
                                         const ImVec2 p3, int num_segments);
    void (*ImDrawList_PathRect)(ImDrawList* self, const ImVec2 rect_min, const ImVec2 rect_max,
                                float rounding, int rounding_corners_flags);
    void (*ImDrawList_ChannelsSplit)(ImDrawList* self, int channels_count);
    void (*ImDrawList_ChannelsMerge)(ImDrawList* self);
    void (*ImDrawList_ChannelsSetCurrent)(ImDrawList* self, int channel_index);
    void (*ImDrawList_AddCallback)(ImDrawList* self, ImDrawCallback callback, void* callback_data);
    void (*ImDrawList_AddDrawCmd)(ImDrawList* self);
    ImDrawList* (*ImDrawList_CloneOutput)(ImDrawList* self);
    void (*ImDrawList_Clear)(ImDrawList* self);
    void (*ImDrawList_ClearFreeMemory)(ImDrawList* self);
    void (*ImDrawList_PrimReserve)(ImDrawList* self, int idx_count, int vtx_count);
    void (*ImDrawList_PrimRect)(ImDrawList* self, const ImVec2 a, const ImVec2 b, ImU32 col);
    void (*ImDrawList_PrimRectUV)(ImDrawList* self, const ImVec2 a, const ImVec2 b,
                                  const ImVec2 uv_a, const ImVec2 uv_b, ImU32 col);
    void (*ImDrawList_PrimQuadUV)(ImDrawList* self, const ImVec2 a, const ImVec2 b, const ImVec2 c,
                                  const ImVec2 d, const ImVec2 uv_a, const ImVec2 uv_b,
                                  const ImVec2 uv_c, const ImVec2 uv_d, ImU32 col);
    void (*ImDrawList_PrimWriteVtx)(ImDrawList* self, const ImVec2 pos, const ImVec2 uv, ImU32 col);
    void (*ImDrawList_PrimWriteIdx)(ImDrawList* self, ImDrawIdx idx);
    void (*ImDrawList_PrimVtx)(ImDrawList* self, const ImVec2 pos, const ImVec2 uv, ImU32 col);
    void (*ImDrawList_UpdateClipRect)(ImDrawList* self);
    void (*ImDrawList_UpdateTextureID)(ImDrawList* self);
    void (*ImDrawList_GetClipRectMin_nonUDT)(ImVec2* pOut, ImDrawList* self);
    void (*ImDrawList_GetClipRectMax_nonUDT)(ImVec2* pOut, ImDrawList* self);
    // ImDrawList
    ImFont* (*ImFont_ImFont)(void);
    void (*ImFont_destroy)(ImFont* self);
    const ImFontGlyph* (*ImFont_FindGlyph)(ImFont* self, ImWchar c);
    const ImFontGlyph* (*ImFont_FindGlyphNoFallback)(ImFont* self, ImWchar c);
    float (*ImFont_GetCharAdvance)(ImFont* self, ImWchar c);
    bool (*ImFont_IsLoaded)(ImFont* self);
    const char* (*ImFont_GetDebugName)(ImFont* self);
    const char* (*ImFont_CalcWordWrapPositionA)(ImFont* self, float scale, const char* text,
                                                const char* text_end, float wrap_width);
    void (*ImFont_RenderChar)(ImFont* self, ImDrawList* draw_list, float size, ImVec2 pos,
                              ImU32 col, ImWchar c);
    void (*ImFont_RenderText)(ImFont* self, ImDrawList* draw_list, float size, ImVec2 pos,
                              ImU32 col, const ImVec4 clip_rect, const char* text_begin,
                              const char* text_end, float wrap_width, bool cpu_fine_clip);
    void (*ImFont_BuildLookupTable)(ImFont* self);
    void (*ImFont_ClearOutputData)(ImFont* self);
    void (*ImFont_GrowIndex)(ImFont* self, int new_size);
    void (*ImFont_AddGlyph)(ImFont* self, ImWchar c, float x0, float y0, float x1, float y1,
                            float u0, float v0, float u1, float v1, float advance_x);
    void (*ImFont_AddRemapChar)(ImFont* self, ImWchar dst, ImWchar src, bool overwrite_dst);
    void (*ImFont_SetFallbackChar)(ImFont* self, ImWchar c);
    void (*ImFont_CalcTextSizeA_nonUDT)(ImVec2* pOut, ImFont* self, float size, float max_width,
                                        float wrap_width, const char* text_begin,
                                        const char* text_end, const char** remaining);
    // ImFontAtlas
    ImFontAtlas* (*ImFontAtlas_ImFontAtlas)(void);
    void (*ImFontAtlas_destroy)(ImFontAtlas* self);
    ImFont* (*ImFontAtlas_AddFont)(ImFontAtlas* self, const ImFontConfig* font_cfg);
    ImFont* (*ImFontAtlas_AddFontDefault)(ImFontAtlas* self, const ImFontConfig* font_cfg);
    ImFont* (*ImFontAtlas_AddFontFromFileTTF)(ImFontAtlas* self, const char* filename,
                                              float size_pixels, const ImFontConfig* font_cfg,
                                              const ImWchar* glyph_ranges);
    ImFont* (*ImFontAtlas_AddFontFromMemoryTTF)(ImFontAtlas* self, void* font_data, int font_size,
                                                float size_pixels, const ImFontConfig* font_cfg,
                                                const ImWchar* glyph_ranges);
    ImFont* (*ImFontAtlas_AddFontFromMemoryCompressedTTF)(
        ImFontAtlas* self, const void* compressed_font_data, int compressed_font_size,
        float size_pixels, const ImFontConfig* font_cfg, const ImWchar* glyph_ranges);
    ImFont* (*ImFontAtlas_AddFontFromMemoryCompressedBase85TTF)(
        ImFontAtlas* self, const char* compressed_font_data_base85, float size_pixels,
        const ImFontConfig* font_cfg, const ImWchar* glyph_ranges);
    void (*ImFontAtlas_ClearInputData)(ImFontAtlas* self);
    void (*ImFontAtlas_ClearTexData)(ImFontAtlas* self);
    void (*ImFontAtlas_ClearFonts)(ImFontAtlas* self);
    void (*ImFontAtlas_Clear)(ImFontAtlas* self);
    bool (*ImFontAtlas_Build)(ImFontAtlas* self);
    void (*ImFontAtlas_GetTexDataAsAlpha8)(ImFontAtlas* self, unsigned char** out_pixels,
                                           int* out_width, int* out_height,
                                           int* out_bytes_per_pixel);
    void (*ImFontAtlas_GetTexDataAsRGBA32)(ImFontAtlas* self, unsigned char** out_pixels,
                                           int* out_width, int* out_height,
                                           int* out_bytes_per_pixel);
    bool (*ImFontAtlas_IsBuilt)(ImFontAtlas* self);
    void (*ImFontAtlas_SetTexID)(ImFontAtlas* self, ImTextureID id);
    const ImWchar* (*ImFontAtlas_GetGlyphRangesDefault)(ImFontAtlas* self);
    const ImWchar* (*ImFontAtlas_GetGlyphRangesKorean)(ImFontAtlas* self);
    const ImWchar* (*ImFontAtlas_GetGlyphRangesJapanese)(ImFontAtlas* self);
    const ImWchar* (*ImFontAtlas_GetGlyphRangesChineseFull)(ImFontAtlas* self);
    const ImWchar* (*ImFontAtlas_GetGlyphRangesChineseSimplifiedCommon)(ImFontAtlas* self);
    const ImWchar* (*ImFontAtlas_GetGlyphRangesCyrillic)(ImFontAtlas* self);
    const ImWchar* (*ImFontAtlas_GetGlyphRangesThai)(ImFontAtlas* self);
    const ImWchar* (*ImFontAtlas_GetGlyphRangesVietnamese)(ImFontAtlas* self);
    int (*ImFontAtlas_AddCustomRectRegular)(ImFontAtlas* self, unsigned int id, int width,
                                            int height);
    int (*ImFontAtlas_AddCustomRectFontGlyph)(ImFontAtlas* self, ImFont* font, ImWchar id,
                                              int width, int height, float advance_x,
                                              const ImVec2 offset);
    const CustomRect* (*ImFontAtlas_GetCustomRectByIndex)(ImFontAtlas* self, int index);
    void (*ImFontAtlas_CalcCustomRectUV)(ImFontAtlas* self, const CustomRect* rect,
                                         ImVec2* out_uv_min, ImVec2* out_uv_max);
    bool (*ImFontAtlas_GetMouseCursorTexData)(ImFontAtlas* self, ImGuiMouseCursor cursor,
                                              ImVec2* out_offset, ImVec2* out_size,
                                              ImVec2 out_uv_border[2], ImVec2 out_uv_fill[2]);
    // ImGuiPayload
    ImGuiPayload* (*ImGuiPayload_ImGuiPayload)(void);
    void (*ImGuiPayload_destroy)(ImGuiPayload* self);
    void (*ImGuiPayload_Clear)(ImGuiPayload* self);
    bool (*ImGuiPayload_IsDataType)(ImGuiPayload* self, const char* type);
    bool (*ImGuiPayload_IsPreview)(ImGuiPayload* self);
    bool (*ImGuiPayload_IsDelivery)(ImGuiPayload* self);
    // ImGuiListClipper
    ImGuiListClipper* (*ImGuiListClipper_ImGuiListClipper)(int items_count, float items_height);
    void (*ImGuiListClipper_destroy)(ImGuiListClipper* self);
    bool (*ImGuiListClipper_Step)(ImGuiListClipper* self);
    void (*ImGuiListClipper_Begin)(ImGuiListClipper* self, int items_count, float items_height);
    void (*ImGuiListClipper_End)(ImGuiListClipper* self);
    // ImGuiTextFilter
    ImGuiTextFilter* (*ImGuiTextFilter_ImGuiTextFilter)(const char* default_filter);
    void (*ImGuiTextFilter_destroy)(ImGuiTextFilter* self);
    bool (*ImGuiTextFilter_Draw)(ImGuiTextFilter* self, const char* label, float width);
    bool (*ImGuiTextFilter_PassFilter)(ImGuiTextFilter* self, const char* text,
                                       const char* text_end);
    void (*ImGuiTextFilter_Build)(ImGuiTextFilter* self);
    void (*ImGuiTextFilter_Clear)(ImGuiTextFilter* self);
    bool (*ImGuiTextFilter_IsActive)(ImGuiTextFilter* self);
    // ImGuiTextBuffer
    ImGuiTextBuffer* (*ImGuiTextBuffer_ImGuiTextBuffer)(void);
    void (*ImGuiTextBuffer_destroy)(ImGuiTextBuffer* self);
    const char* (*ImGuiTextBuffer_begin)(ImGuiTextBuffer* self);
    const char* (*ImGuiTextBuffer_end)(ImGuiTextBuffer* self);
    int (*ImGuiTextBuffer_size)(ImGuiTextBuffer* self);
    bool (*ImGuiTextBuffer_empty)(ImGuiTextBuffer* self);
    void (*ImGuiTextBuffer_clear)(ImGuiTextBuffer* self);
    void (*ImGuiTextBuffer_reserve)(ImGuiTextBuffer* self, int capacity);
    const char* (*ImGuiTextBuffer_c_str)(ImGuiTextBuffer* self);
    void (*ImGuiTextBuffer_append)(ImGuiTextBuffer* self, const char* str, const char* str_end);
    void (*ImGuiTextBuffer_appendfv)(ImGuiTextBuffer* self, const char* fmt, va_list args);
    void (*ImGuiTextBuffer_appendf)(struct ImGuiTextBuffer* buffer, const char* fmt, ...);
} rizz_api_imgui;