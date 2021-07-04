//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//

#include "rizz/imgui.h"
#include "rizz/imgui-extra.h"
#include "rizz/rizz.h"

#include "imgui-internal.h"

#include "sx/allocator.h"
#include "sx/array.h"
#include "sx/io.h"
#include "sx/math-vec.h"
#include "sx/string.h"
#include "sx/timer.h"
#include "sx/pool.h"

#include <alloca.h>
#include <float.h>

#include "font-roboto.h"

SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4190)
#define RIZZ__IMGUI
#include "cimgui/cimgui.h"
SX_PRAGMA_DIAGNOSTIC_POP()

#if SX_PLATFORM_WINDOWS
SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(5105)

#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>

SX_PRAGMA_DIAGNOSTIC_POP()
#endif

//
#define IMGUI_VERSION "1.82"
#define MAX_VERTS 32768      // 32k
#define MAX_INDICES 98304    // 96k
#define MAX_BUFFERED_FRAME_TIMES 120
#define IMGUI_SMALL_MEMORY_SIZE 160

typedef struct rizz_api_gfx rizz_api_gfx;
static void imgui__render(void);

RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_plugin* the_plugin;
RIZZ_STATE static rizz_api_gfx* the_gfx;
RIZZ_STATE static rizz_api_app* the_app;

////////////////////////////////////////////////////////////////////////////////////////////////////
// API
rizz_api_imgui the__imgui = {
    .CreateContext = igCreateContext,
    .DestroyContext = igDestroyContext,
    .GetCurrentContext = igGetCurrentContext,
    .SetCurrentContext = igSetCurrentContext,
    .GetIO = igGetIO,
    .GetStyle = igGetStyle,
    .NewFrame = igNewFrame,
    .EndFrame = igEndFrame,
    .Render = imgui__render,
    .GetDrawData = igGetDrawData,
    .ShowMetricsWindow = igShowMetricsWindow,
    .GetVersion = igGetVersion,
    .StyleColorsDark = igStyleColorsDark,
    .StyleColorsLight = igStyleColorsLight,
    .StyleColorsClassic = igStyleColorsClassic,
    .Begin = igBegin,
    .End = igEnd,
    .BeginChild_Str = igBeginChild_Str,
    .BeginChild_ID = igBeginChild_ID,
    .EndChild = igEndChild,
    .IsWindowAppearing = igIsWindowAppearing,
    .IsWindowCollapsed = igIsWindowCollapsed,
    .IsWindowFocused = igIsWindowFocused,
    .IsWindowHovered = igIsWindowHovered,
    .GetWindowDrawList = igGetWindowDrawList,
    .GetWindowDpiScale = igGetWindowDpiScale,
    .GetWindowPos = igGetWindowPos,
    .GetWindowSize = igGetWindowSize,
    .GetWindowWidth = igGetWindowWidth,
    .GetWindowHeight = igGetWindowHeight,
    .GetWindowViewport = igGetWindowViewport,
    .SetNextWindowPos = igSetNextWindowPos,
    .SetNextWindowSize = igSetNextWindowSize,
    .SetNextWindowSizeConstraints = igSetNextWindowSizeConstraints,
    .SetNextWindowContentSize = igSetNextWindowContentSize,
    .SetNextWindowCollapsed = igSetNextWindowCollapsed,
    .SetNextWindowFocus = igSetNextWindowFocus,
    .SetNextWindowBgAlpha = igSetNextWindowBgAlpha,
    .SetNextWindowViewport = igSetNextWindowViewport,
    .SetWindowPos_Vec2 = igSetWindowPos_Vec2,
    .SetWindowSize_Vec2 = igSetWindowSize_Vec2,
    .SetWindowCollapsed_Bool = igSetWindowCollapsed_Bool,
    .SetWindowFocus_Nil = igSetWindowFocus_Nil,
    .SetWindowFontScale = igSetWindowFontScale,
    .SetWindowPos_Str = igSetWindowPos_Str,
    .SetWindowSize_Str = igSetWindowSize_Str,
    .SetWindowCollapsed_Str = igSetWindowCollapsed_Str,
    .SetWindowFocus_Str = igSetWindowFocus_Str,
    .GetContentRegionAvail = igGetContentRegionAvail,
    .GetContentRegionMax = igGetContentRegionMax,
    .GetWindowContentRegionMin = igGetWindowContentRegionMin,
    .GetWindowContentRegionMax = igGetWindowContentRegionMax,
    .GetWindowContentRegionWidth = igGetWindowContentRegionWidth,
    .GetScrollX = igGetScrollX,
    .GetScrollY = igGetScrollY,
    .SetScrollX_Float = igSetScrollX_Float,
    .SetScrollY_Float = igSetScrollY_Float,
    .GetScrollMaxX = igGetScrollMaxX,
    .GetScrollMaxY = igGetScrollMaxY,
    .SetScrollHereX = igSetScrollHereX,
    .SetScrollHereY = igSetScrollHereY,
    .SetScrollFromPosX_Float = igSetScrollFromPosX_Float,
    .SetScrollFromPosY_Float = igSetScrollFromPosY_Float,
    .PushFont = igPushFont,
    .PopFont = igPopFont,
    .PushStyleColor_U32 = igPushStyleColor_U32,
    .PushStyleColor_Vec4 = igPushStyleColor_Vec4,
    .PopStyleColor = igPopStyleColor,
    .PushStyleVar_Float = igPushStyleVar_Float,
    .PushStyleVar_Vec2 = igPushStyleVar_Vec2,
    .PopStyleVar = igPopStyleVar,
    .PushAllowKeyboardFocus = igPushAllowKeyboardFocus,
    .PopAllowKeyboardFocus = igPopAllowKeyboardFocus,
    .PushButtonRepeat = igPushButtonRepeat,
    .PopButtonRepeat = igPopButtonRepeat,
    .PushItemWidth = igPushItemWidth,
    .PopItemWidth = igPopItemWidth,
    .SetNextItemWidth = igSetNextItemWidth,
    .CalcItemWidth = igCalcItemWidth,
    .PushTextWrapPos = igPushTextWrapPos,
    .PopTextWrapPos = igPopTextWrapPos,
    .GetFont = igGetFont,
    .GetFontSize = igGetFontSize,
    .GetFontTexUvWhitePixel = igGetFontTexUvWhitePixel,
    .GetColorU32_Col = igGetColorU32_Col,
    .GetColorU32_Vec4 = igGetColorU32_Vec4,
    .GetColorU32_U32 = igGetColorU32_U32,
    .GetStyleColorVec4 = igGetStyleColorVec4,
    .Separator = igSeparator,
    .SameLine = igSameLine,
    .NewLine = igNewLine,
    .Spacing = igSpacing,
    .Dummy = igDummy,
    .Indent = igIndent,
    .Unindent = igUnindent,
    .BeginGroup = igBeginGroup,
    .EndGroup = igEndGroup,
    .GetCursorPos = igGetCursorPos,
    .GetCursorPosX = igGetCursorPosX,
    .GetCursorPosY = igGetCursorPosY,
    .SetCursorPos = igSetCursorPos,
    .SetCursorPosX = igSetCursorPosX,
    .SetCursorPosY = igSetCursorPosY,
    .GetCursorStartPos = igGetCursorStartPos,
    .GetCursorScreenPos = igGetCursorScreenPos,
    .SetCursorScreenPos = igSetCursorScreenPos,
    .AlignTextToFramePadding = igAlignTextToFramePadding,
    .GetTextLineHeight = igGetTextLineHeight,
    .GetTextLineHeightWithSpacing = igGetTextLineHeightWithSpacing,
    .GetFrameHeight = igGetFrameHeight,
    .GetFrameHeightWithSpacing = igGetFrameHeightWithSpacing,
    .PushID_Str = igPushID_Str,
    .PushID_StrStr = igPushID_StrStr,
    .PushID_Ptr = igPushID_Ptr,
    .PushID_Int = igPushID_Int,
    .PopID = igPopID,
    .GetID_Str = igGetID_Str,
    .GetID_StrStr = igGetID_StrStr,
    .GetID_Ptr = igGetID_Ptr,
    .TextUnformatted = igTextUnformatted,
    .Text = igText,
    .TextV = igTextV,
    .TextColored = igTextColored,
    .TextColoredV = igTextColoredV,
    .TextDisabled = igTextDisabled,
    .TextDisabledV = igTextDisabledV,
    .TextWrapped = igTextWrapped,
    .TextWrappedV = igTextWrappedV,
    .LabelText = igLabelText,
    .LabelTextV = igLabelTextV,
    .BulletText = igBulletText,
    .BulletTextV = igBulletTextV,
    .Button = igButton,
    .SmallButton = igSmallButton,
    .InvisibleButton = igInvisibleButton,
    .ArrowButton = igArrowButton,
    .Image = igImage,
    .ImageButton = igImageButton,
    .Checkbox = igCheckbox,
    .CheckboxFlags_IntPtr = igCheckboxFlags_IntPtr,
    .CheckboxFlags_UintPtr = igCheckboxFlags_UintPtr,
    .RadioButton_Bool = igRadioButton_Bool,
    .RadioButton_IntPtr = igRadioButton_IntPtr,
    .ProgressBar = igProgressBar,
    .Bullet = igBullet,
    .BeginCombo = igBeginCombo,
    .EndCombo = igEndCombo,
    .Combo_Str_arr = igCombo_Str_arr,
    .Combo_Str = igCombo_Str,
    .Combo_FnBoolPtr = igCombo_FnBoolPtr,
    .DragFloat = igDragFloat,
    .DragFloat2 = igDragFloat2,
    .DragFloat3 = igDragFloat3,
    .DragFloat4 = igDragFloat4,
    .DragFloatRange2 = igDragFloatRange2,
    .DragInt = igDragInt,
    .DragInt2 = igDragInt2,
    .DragInt3 = igDragInt3,
    .DragInt4 = igDragInt4,
    .DragIntRange2 = igDragIntRange2,
    .DragScalar = igDragScalar,
    .DragScalarN = igDragScalarN,
    .SliderFloat = igSliderFloat,
    .SliderFloat2 = igSliderFloat2,
    .SliderFloat3 = igSliderFloat3,
    .SliderFloat4 = igSliderFloat4,
    .SliderAngle = igSliderAngle,
    .SliderInt = igSliderInt,
    .SliderInt2 = igSliderInt2,
    .SliderInt3 = igSliderInt3,
    .SliderInt4 = igSliderInt4,
    .SliderScalar = igSliderScalar,
    .SliderScalarN = igSliderScalarN,
    .VSliderFloat = igVSliderFloat,
    .VSliderInt = igVSliderInt,
    .VSliderScalar = igVSliderScalar,
    .InputText = igInputText,
    .InputTextMultiline = igInputTextMultiline,
    .InputTextWithHint = igInputTextWithHint,
    .InputFloat = igInputFloat,
    .InputFloat2 = igInputFloat2,
    .InputFloat3 = igInputFloat3,
    .InputFloat4 = igInputFloat4,
    .InputInt = igInputInt,
    .InputInt2 = igInputInt2,
    .InputInt3 = igInputInt3,
    .InputInt4 = igInputInt4,
    .InputDouble = igInputDouble,
    .InputScalar = igInputScalar,
    .InputScalarN = igInputScalarN,
    .ColorEdit3 = igColorEdit3,
    .ColorEdit4 = igColorEdit4,
    .ColorPicker3 = igColorPicker3,
    .ColorPicker4 = igColorPicker4,
    .ColorButton = igColorButton,
    .SetColorEditOptions = igSetColorEditOptions,
    .TreeNode_Str = igTreeNode_Str,
    .TreeNode_StrStr = igTreeNode_StrStr,
    .TreeNode_Ptr = igTreeNode_Ptr,
    .TreeNodeV_Str = igTreeNodeV_Str,
    .TreeNodeV_Ptr = igTreeNodeV_Ptr,
    .TreeNodeEx_Str = igTreeNodeEx_Str,
    .TreeNodeEx_StrStr = igTreeNodeEx_StrStr,
    .TreeNodeEx_Ptr = igTreeNodeEx_Ptr,
    .TreeNodeExV_Str = igTreeNodeExV_Str,
    .TreeNodeExV_Ptr = igTreeNodeExV_Ptr,
    .TreePush_Str = igTreePush_Str,
    .TreePush_Ptr = igTreePush_Ptr,
    .TreePop = igTreePop,
    .GetTreeNodeToLabelSpacing = igGetTreeNodeToLabelSpacing,
    .CollapsingHeader_TreeNodeFlags = igCollapsingHeader_TreeNodeFlags,
    .CollapsingHeader_BoolPtr = igCollapsingHeader_BoolPtr,
    .SetNextItemOpen = igSetNextItemOpen,
    .Selectable_Bool = igSelectable_Bool,
    .Selectable_BoolPtr = igSelectable_BoolPtr,
    .BeginListBox = igBeginListBox,
    .EndListBox = igEndListBox,
    .ListBox_Str_arr = igListBox_Str_arr,
    .ListBox_FnBoolPtr = igListBox_FnBoolPtr,
    .PlotLines_FloatPtr = igPlotLines_FloatPtr,
    .PlotLines_FnFloatPtr = igPlotLines_FnFloatPtr,
    .PlotHistogram_FloatPtr = igPlotHistogram_FloatPtr,
    .PlotHistogram_FnFloatPtr = igPlotHistogram_FnFloatPtr,
    .Value_Bool = igValue_Bool,
    .Value_Int = igValue_Int,
    .Value_Uint = igValue_Uint,
    .Value_Float = igValue_Float,
    .BeginMenuBar = igBeginMenuBar,
    .EndMenuBar = igEndMenuBar,
    .BeginMainMenuBar = igBeginMainMenuBar,
    .EndMainMenuBar = igEndMainMenuBar,
    .BeginMenu = igBeginMenu,
    .EndMenu = igEndMenu,
    .MenuItem_Bool = igMenuItem_Bool,
    .MenuItem_BoolPtr = igMenuItem_BoolPtr,
    .BeginTooltip = igBeginTooltip,
    .EndTooltip = igEndTooltip,
    .SetTooltip = igSetTooltip,
    .SetTooltipV = igSetTooltipV,
    .BeginPopup = igBeginPopup,
    .BeginPopupModal = igBeginPopupModal,
    .EndPopup = igEndPopup,
    .OpenPopup = igOpenPopup,
    .OpenPopupOnItemClick = igOpenPopupOnItemClick,
    .CloseCurrentPopup = igCloseCurrentPopup,
    .BeginPopupContextItem = igBeginPopupContextItem,
    .BeginPopupContextWindow = igBeginPopupContextWindow,
    .BeginPopupContextVoid = igBeginPopupContextVoid,
    .IsPopupOpen_Str = igIsPopupOpen_Str,
    .BeginTable = igBeginTable,
    .EndTable = igEndTable,
    .TableNextRow = igTableNextRow,
    .TableNextColumn = igTableNextColumn,
    .TableSetColumnIndex = igTableSetColumnIndex,
    .TableSetupColumn = igTableSetupColumn,
    .TableSetupScrollFreeze = igTableSetupScrollFreeze,
    .TableHeadersRow = igTableHeadersRow,
    .TableHeader = igTableHeader,
    .TableGetSortSpecs = igTableGetSortSpecs,
    .TableGetColumnCount = igTableGetColumnCount,
    .TableGetColumnIndex = igTableGetColumnIndex,
    .TableGetRowIndex = igTableGetRowIndex,
    .TableGetColumnName_Int = igTableGetColumnName_Int,
    .TableGetColumnFlags = igTableGetColumnFlags,
    .TableSetBgColor = igTableSetBgColor,
    .Columns = igColumns,
    .NextColumn = igNextColumn,
    .GetColumnIndex = igGetColumnIndex,
    .GetColumnWidth = igGetColumnWidth,
    .SetColumnWidth = igSetColumnWidth,
    .GetColumnOffset = igGetColumnOffset,
    .SetColumnOffset = igSetColumnOffset,
    .GetColumnsCount = igGetColumnsCount,
    .BeginTabBar = igBeginTabBar,
    .EndTabBar = igEndTabBar,
    .BeginTabItem = igBeginTabItem,
    .EndTabItem = igEndTabItem,
    .TabItemButton = igTabItemButton,
    .SetTabItemClosed = igSetTabItemClosed,
    .DockSpace = igDockSpace,
    .DockSpaceOverViewport = igDockSpaceOverViewport,
    .SetNextWindowDockID = igSetNextWindowDockID,
    .SetNextWindowClass = igSetNextWindowClass,
    .GetWindowDockID = igGetWindowDockID,
    .IsWindowDocked = igIsWindowDocked,
    .LogToTTY = igLogToTTY,
    .LogToFile = igLogToFile,
    .LogToClipboard = igLogToClipboard,
    .LogFinish = igLogFinish,
    .LogButtons = igLogButtons,
    .LogTextV = igLogTextV,
    .BeginDragDropSource = igBeginDragDropSource,
    .SetDragDropPayload = igSetDragDropPayload,
    .EndDragDropSource = igEndDragDropSource,
    .BeginDragDropTarget = igBeginDragDropTarget,
    .AcceptDragDropPayload = igAcceptDragDropPayload,
    .EndDragDropTarget = igEndDragDropTarget,
    .GetDragDropPayload = igGetDragDropPayload,
    .PushClipRect = igPushClipRect,
    .PopClipRect = igPopClipRect,
    .SetItemDefaultFocus = igSetItemDefaultFocus,
    .SetKeyboardFocusHere = igSetKeyboardFocusHere,
    .IsItemHovered = igIsItemHovered,
    .IsItemActive = igIsItemActive,
    .IsItemFocused = igIsItemFocused,
    .IsItemClicked = igIsItemClicked,
    .IsItemVisible = igIsItemVisible,
    .IsItemEdited = igIsItemEdited,
    .IsItemActivated = igIsItemActivated,
    .IsItemDeactivated = igIsItemDeactivated,
    .IsItemDeactivatedAfterEdit = igIsItemDeactivatedAfterEdit,
    .IsItemToggledOpen = igIsItemToggledOpen,
    .IsAnyItemHovered = igIsAnyItemHovered,
    .IsAnyItemActive = igIsAnyItemActive,
    .IsAnyItemFocused = igIsAnyItemFocused,
    .GetItemRectMin = igGetItemRectMin,
    .GetItemRectMax = igGetItemRectMax,
    .GetItemRectSize = igGetItemRectSize,
    .SetItemAllowOverlap = igSetItemAllowOverlap,
    .GetMainViewport = igGetMainViewport,
    .IsRectVisible_Nil = igIsRectVisible_Nil,
    .IsRectVisible_Vec2 = igIsRectVisible_Vec2,
    .GetTime = igGetTime,
    .GetFrameCount = igGetFrameCount,
    .GetBackgroundDrawList_Nil = igGetBackgroundDrawList_Nil,
    .GetForegroundDrawList_Nil = igGetForegroundDrawList_Nil,
    .GetBackgroundDrawList_ViewportPtr = igGetBackgroundDrawList_ViewportPtr,
    .GetForegroundDrawList_ViewportPtr = igGetForegroundDrawList_ViewportPtr,
    .GetDrawListSharedData = igGetDrawListSharedData,
    .GetStyleColorName = igGetStyleColorName,
    .SetStateStorage = igSetStateStorage,
    .GetStateStorage = igGetStateStorage,
    .CalcListClipping = igCalcListClipping,
    .BeginChildFrame = igBeginChildFrame,
    .EndChildFrame = igEndChildFrame,
    .CalcTextSize = igCalcTextSize,
    .ColorConvertU32ToFloat4 = igColorConvertU32ToFloat4,
    .ColorConvertFloat4ToU32 = igColorConvertFloat4ToU32,
    .ColorConvertRGBtoHSV = igColorConvertRGBtoHSV,
    .ColorConvertHSVtoRGB = igColorConvertHSVtoRGB,
    .GetKeyIndex = igGetKeyIndex,
    .IsKeyDown = igIsKeyDown,
    .IsKeyPressed = igIsKeyPressed,
    .IsKeyReleased = igIsKeyReleased,
    .GetKeyPressedAmount = igGetKeyPressedAmount,
    .CaptureKeyboardFromApp = igCaptureKeyboardFromApp,
    .IsMouseDown = igIsMouseDown,
    .IsMouseClicked = igIsMouseClicked,
    .IsMouseReleased = igIsMouseReleased,
    .IsMouseDoubleClicked = igIsMouseDoubleClicked,
    .IsMouseHoveringRect = igIsMouseHoveringRect,
    .IsMousePosValid = igIsMousePosValid,
    .IsAnyMouseDown = igIsAnyMouseDown,
    .GetMousePos = igGetMousePos,
    .GetMousePosOnOpeningCurrentPopup = igGetMousePosOnOpeningCurrentPopup,
    .IsMouseDragging = igIsMouseDragging,
    .GetMouseDragDelta = igGetMouseDragDelta,
    .ResetMouseDragDelta = igResetMouseDragDelta,
    .GetMouseCursor = igGetMouseCursor,
    .SetMouseCursor = igSetMouseCursor,
    .CaptureMouseFromApp = igCaptureMouseFromApp,
    .GetClipboardText = igGetClipboardText,
    .SetClipboardText = igSetClipboardText,
    .LoadIniSettingsFromDisk = igLoadIniSettingsFromDisk,
    .LoadIniSettingsFromMemory = igLoadIniSettingsFromMemory,
    .SaveIniSettingsToDisk = igSaveIniSettingsToDisk,
    .SaveIniSettingsToMemory = igSaveIniSettingsToMemory,
    .DebugCheckVersionAndDataLayout = igDebugCheckVersionAndDataLayout,
    .SetAllocatorFunctions = igSetAllocatorFunctions,
    .GetAllocatorFunctions = igGetAllocatorFunctions,
    .MemAlloc = igMemAlloc,
    .MemFree = igMemFree,
    .GetPlatformIO = igGetPlatformIO,
    .UpdatePlatformWindows = igUpdatePlatformWindows,
    .RenderPlatformWindowsDefault = igRenderPlatformWindowsDefault,
    .DestroyPlatformWindows = igDestroyPlatformWindows,
    .FindViewportByID = igFindViewportByID,
    .FindViewportByPlatformHandle = igFindViewportByPlatformHandle,
    .ImHashData = igImHashData,
    .ImHashStr = igImHashStr,
    .ImAlphaBlendColors = igImAlphaBlendColors,
    .ImIsPowerOfTwo_Int = igImIsPowerOfTwo_Int,
    .ImIsPowerOfTwo_U64 = igImIsPowerOfTwo_U64,
    .ImUpperPowerOfTwo = igImUpperPowerOfTwo,
    .ImStricmp = igImStricmp,
    .ImStrnicmp = igImStrnicmp,
    .ImStrncpy = igImStrncpy,
    .ImStrdup = igImStrdup,
    .ImStrdupcpy = igImStrdupcpy,
    .ImStrchrRange = igImStrchrRange,
    .ImStrlenW = igImStrlenW,
    .ImStreolRange = igImStreolRange,
    .ImStrbolW = igImStrbolW,
    .ImStristr = igImStristr,
    .ImStrTrimBlanks = igImStrTrimBlanks,
    .ImStrSkipBlank = igImStrSkipBlank,
    .ImFormatString = igImFormatString,
    .ImFormatStringV = igImFormatStringV,
    .ImParseFormatFindStart = igImParseFormatFindStart,
    .ImParseFormatFindEnd = igImParseFormatFindEnd,
    .ImParseFormatTrimDecorations = igImParseFormatTrimDecorations,
    .ImParseFormatPrecision = igImParseFormatPrecision,
    .ImCharIsBlankA = igImCharIsBlankA,
    .ImCharIsBlankW = igImCharIsBlankW,
    .ImTextStrToUtf8 = igImTextStrToUtf8,
    .ImTextCharFromUtf8 = igImTextCharFromUtf8,
    .ImTextStrFromUtf8 = igImTextStrFromUtf8,
    .ImTextCountCharsFromUtf8 = igImTextCountCharsFromUtf8,
    .ImTextCountUtf8BytesFromChar = igImTextCountUtf8BytesFromChar,
    .ImTextCountUtf8BytesFromStr = igImTextCountUtf8BytesFromStr,
    .ImFileOpen = igImFileOpen,
    .ImFileClose = igImFileClose,
    .ImFileGetSize = igImFileGetSize,
    .ImFileRead = igImFileRead,
    .ImFileWrite = igImFileWrite,
    .ImFileLoadToMemory = igImFileLoadToMemory,
    .ImPow_Float = igImPow_Float,
    .ImPow_double = igImPow_double,
    .ImLog_Float = igImLog_Float,
    .ImLog_double = igImLog_double,
    .ImAbs_Float = igImAbs_Float,
    .ImAbs_double = igImAbs_double,
    .ImSign_Float = igImSign_Float,
    .ImSign_double = igImSign_double,
    .ImMin = igImMin,
    .ImMax = igImMax,
    .ImClamp = igImClamp,
    .ImLerp_Vec2Float = igImLerp_Vec2Float,
    .ImLerp_Vec2Vec2 = igImLerp_Vec2Vec2,
    .ImLerp_Vec4 = igImLerp_Vec4,
    .ImSaturate = igImSaturate,
    .ImLengthSqr_Vec2 = igImLengthSqr_Vec2,
    .ImLengthSqr_Vec4 = igImLengthSqr_Vec4,
    .ImInvLength = igImInvLength,
    .ImFloor_Float = igImFloor_Float,
    .ImFloor_Vec2 = igImFloor_Vec2,
    .ImModPositive = igImModPositive,
    .ImDot = igImDot,
    .ImRotate = igImRotate,
    .ImLinearSweep = igImLinearSweep,
    .ImMul = igImMul,
    .ImBezierCubicCalc = igImBezierCubicCalc,
    .ImBezierCubicClosestPoint = igImBezierCubicClosestPoint,
    .ImBezierCubicClosestPointCasteljau = igImBezierCubicClosestPointCasteljau,
    .ImBezierQuadraticCalc = igImBezierQuadraticCalc,
    .ImLineClosestPoint = igImLineClosestPoint,
    .ImTriangleContainsPoint = igImTriangleContainsPoint,
    .ImTriangleClosestPoint = igImTriangleClosestPoint,
    .ImTriangleBarycentricCoords = igImTriangleBarycentricCoords,
    .ImTriangleArea = igImTriangleArea,
    .ImGetDirQuadrantFromDelta = igImGetDirQuadrantFromDelta,
    .ImBitArrayTestBit = igImBitArrayTestBit,
    .ImBitArrayClearBit = igImBitArrayClearBit,
    .ImBitArraySetBit = igImBitArraySetBit,
    .ImBitArraySetBitRange = igImBitArraySetBitRange,
    .GetCurrentWindowRead = igGetCurrentWindowRead,
    .GetCurrentWindow = igGetCurrentWindow,
    .FindWindowByID = igFindWindowByID,
    .FindWindowByName = igFindWindowByName,
    .UpdateWindowParentAndRootLinks = igUpdateWindowParentAndRootLinks,
    .CalcWindowNextAutoFitSize = igCalcWindowNextAutoFitSize,
    .IsWindowChildOf = igIsWindowChildOf,
    .IsWindowAbove = igIsWindowAbove,
    .IsWindowNavFocusable = igIsWindowNavFocusable,
    .GetWindowAllowedExtentRect = igGetWindowAllowedExtentRect,
    .SetWindowPos_WindowPtr = igSetWindowPos_WindowPtr,
    .SetWindowSize_WindowPtr = igSetWindowSize_WindowPtr,
    .SetWindowCollapsed_WindowPtr = igSetWindowCollapsed_WindowPtr,
    .SetWindowHitTestHole = igSetWindowHitTestHole,
    .FocusWindow = igFocusWindow,
    .FocusTopMostWindowUnderOne = igFocusTopMostWindowUnderOne,
    .BringWindowToFocusFront = igBringWindowToFocusFront,
    .BringWindowToDisplayFront = igBringWindowToDisplayFront,
    .BringWindowToDisplayBack = igBringWindowToDisplayBack,
    .SetCurrentFont = igSetCurrentFont,
    .GetDefaultFont = igGetDefaultFont,
    .GetForegroundDrawList_WindowPtr = igGetForegroundDrawList_WindowPtr,
    .Initialize = igInitialize,
    .Shutdown = igShutdown,
    .UpdateHoveredWindowAndCaptureFlags = igUpdateHoveredWindowAndCaptureFlags,
    .StartMouseMovingWindow = igStartMouseMovingWindow,
    .StartMouseMovingWindowOrNode = igStartMouseMovingWindowOrNode,
    .UpdateMouseMovingWindowNewFrame = igUpdateMouseMovingWindowNewFrame,
    .UpdateMouseMovingWindowEndFrame = igUpdateMouseMovingWindowEndFrame,
    .AddContextHook = igAddContextHook,
    .RemoveContextHook = igRemoveContextHook,
    .CallContextHooks = igCallContextHooks,
    .TranslateWindowsInViewport = igTranslateWindowsInViewport,
    .ScaleWindowsInViewport = igScaleWindowsInViewport,
    .DestroyPlatformWindow = igDestroyPlatformWindow,
    .GetViewportPlatformMonitor = igGetViewportPlatformMonitor,
    .MarkIniSettingsDirty_Nil = igMarkIniSettingsDirty_Nil,
    .MarkIniSettingsDirty_WindowPtr = igMarkIniSettingsDirty_WindowPtr,
    .ClearIniSettings = igClearIniSettings,
    .CreateNewWindowSettings = igCreateNewWindowSettings,
    .FindWindowSettings = igFindWindowSettings,
    .FindOrCreateWindowSettings = igFindOrCreateWindowSettings,
    .FindSettingsHandler = igFindSettingsHandler,
    .SetNextWindowScroll = igSetNextWindowScroll,
    .SetScrollX_WindowPtr = igSetScrollX_WindowPtr,
    .SetScrollY_WindowPtr = igSetScrollY_WindowPtr,
    .SetScrollFromPosX_WindowPtr = igSetScrollFromPosX_WindowPtr,
    .SetScrollFromPosY_WindowPtr = igSetScrollFromPosY_WindowPtr,
    .ScrollToBringRectIntoView = igScrollToBringRectIntoView,
    .GetItemID = igGetItemID,
    .GetItemStatusFlags = igGetItemStatusFlags,
    .GetActiveID = igGetActiveID,
    .GetFocusID = igGetFocusID,
    .GetItemsFlags = igGetItemsFlags,
    .SetActiveID = igSetActiveID,
    .SetFocusID = igSetFocusID,
    .ClearActiveID = igClearActiveID,
    .GetHoveredID = igGetHoveredID,
    .SetHoveredID = igSetHoveredID,
    .KeepAliveID = igKeepAliveID,
    .MarkItemEdited = igMarkItemEdited,
    .PushOverrideID = igPushOverrideID,
    .GetIDWithSeed = igGetIDWithSeed,
    .ItemSize_Vec2 = igItemSize_Vec2,
    .ItemSize_Rect = igItemSize_Rect,
    .ItemAdd = igItemAdd,
    .ItemHoverable = igItemHoverable,
    .IsClippedEx = igIsClippedEx,
    .SetLastItemData = igSetLastItemData,
    .FocusableItemRegister = igFocusableItemRegister,
    .FocusableItemUnregister = igFocusableItemUnregister,
    .CalcItemSize = igCalcItemSize,
    .CalcWrapWidthForPos = igCalcWrapWidthForPos,
    .PushMultiItemsWidths = igPushMultiItemsWidths,
    .PushItemFlag = igPushItemFlag,
    .PopItemFlag = igPopItemFlag,
    .IsItemToggledSelection = igIsItemToggledSelection,
    .GetContentRegionMaxAbs = igGetContentRegionMaxAbs,
    .ShrinkWidths = igShrinkWidths,
    .LogBegin = igLogBegin,
    .LogToBuffer = igLogToBuffer,
    .LogRenderedText = igLogRenderedText,
    .LogSetNextTextDecoration = igLogSetNextTextDecoration,
    .BeginChildEx = igBeginChildEx,
    .OpenPopupEx = igOpenPopupEx,
    .ClosePopupToLevel = igClosePopupToLevel,
    .ClosePopupsOverWindow = igClosePopupsOverWindow,
    .IsPopupOpen_ID = igIsPopupOpen_ID,
    .BeginPopupEx = igBeginPopupEx,
    .BeginTooltipEx = igBeginTooltipEx,
    .GetTopMostPopupModal = igGetTopMostPopupModal,
    .FindBestWindowPosForPopup = igFindBestWindowPosForPopup,
    .FindBestWindowPosForPopupEx = igFindBestWindowPosForPopupEx,
    .NavInitWindow = igNavInitWindow,
    .NavMoveRequestButNoResultYet = igNavMoveRequestButNoResultYet,
    .NavMoveRequestCancel = igNavMoveRequestCancel,
    .NavMoveRequestForward = igNavMoveRequestForward,
    .NavMoveRequestTryWrapping = igNavMoveRequestTryWrapping,
    .GetNavInputAmount = igGetNavInputAmount,
    .GetNavInputAmount2d = igGetNavInputAmount2d,
    .CalcTypematicRepeatAmount = igCalcTypematicRepeatAmount,
    .ActivateItem = igActivateItem,
    .SetNavID = igSetNavID,
    .PushFocusScope = igPushFocusScope,
    .PopFocusScope = igPopFocusScope,
    .GetFocusedFocusScope = igGetFocusedFocusScope,
    .GetFocusScope = igGetFocusScope,
    .SetItemUsingMouseWheel = igSetItemUsingMouseWheel,
    .IsActiveIdUsingNavDir = igIsActiveIdUsingNavDir,
    .IsActiveIdUsingNavInput = igIsActiveIdUsingNavInput,
    .IsActiveIdUsingKey = igIsActiveIdUsingKey,
    .IsMouseDragPastThreshold = igIsMouseDragPastThreshold,
    .IsKeyPressedMap = igIsKeyPressedMap,
    .IsNavInputDown = igIsNavInputDown,
    .IsNavInputTest = igIsNavInputTest,
    .GetMergedKeyModFlags = igGetMergedKeyModFlags,
    .DockContextInitialize = igDockContextInitialize,
    .DockContextShutdown = igDockContextShutdown,
    .DockContextClearNodes = igDockContextClearNodes,
    .DockContextRebuildNodes = igDockContextRebuildNodes,
    .DockContextNewFrameUpdateUndocking = igDockContextNewFrameUpdateUndocking,
    .DockContextNewFrameUpdateDocking = igDockContextNewFrameUpdateDocking,
    .DockContextGenNodeID = igDockContextGenNodeID,
    .DockContextQueueDock = igDockContextQueueDock,
    .DockContextQueueUndockWindow = igDockContextQueueUndockWindow,
    .DockContextQueueUndockNode = igDockContextQueueUndockNode,
    .DockContextCalcDropPosForDocking = igDockContextCalcDropPosForDocking,
    .DockNodeBeginAmendTabBar = igDockNodeBeginAmendTabBar,
    .DockNodeEndAmendTabBar = igDockNodeEndAmendTabBar,
    .DockNodeGetRootNode = igDockNodeGetRootNode,
    .DockNodeGetDepth = igDockNodeGetDepth,
    .GetWindowDockNode = igGetWindowDockNode,
    .GetWindowAlwaysWantOwnTabBar = igGetWindowAlwaysWantOwnTabBar,
    .BeginDocked = igBeginDocked,
    .BeginDockableDragDropSource = igBeginDockableDragDropSource,
    .BeginDockableDragDropTarget = igBeginDockableDragDropTarget,
    .SetWindowDock = igSetWindowDock,
    .DockBuilderDockWindow = igDockBuilderDockWindow,
    .DockBuilderGetNode = igDockBuilderGetNode,
    .DockBuilderGetCentralNode = igDockBuilderGetCentralNode,
    .DockBuilderAddNode = igDockBuilderAddNode,
    .DockBuilderRemoveNode = igDockBuilderRemoveNode,
    .DockBuilderRemoveNodeDockedWindows = igDockBuilderRemoveNodeDockedWindows,
    .DockBuilderRemoveNodeChildNodes = igDockBuilderRemoveNodeChildNodes,
    .DockBuilderSetNodePos = igDockBuilderSetNodePos,
    .DockBuilderSetNodeSize = igDockBuilderSetNodeSize,
    .DockBuilderSplitNode = igDockBuilderSplitNode,
    .DockBuilderCopyDockSpace = igDockBuilderCopyDockSpace,
    .DockBuilderCopyNode = igDockBuilderCopyNode,
    .DockBuilderCopyWindowSettings = igDockBuilderCopyWindowSettings,
    .DockBuilderFinish = igDockBuilderFinish,
    .BeginDragDropTargetCustom = igBeginDragDropTargetCustom,
    .ClearDragDrop = igClearDragDrop,
    .IsDragDropPayloadBeingAccepted = igIsDragDropPayloadBeingAccepted,
    .SetWindowClipRectBeforeSetChannel = igSetWindowClipRectBeforeSetChannel,
    .BeginColumns = igBeginColumns,
    .EndColumns = igEndColumns,
    .PushColumnClipRect = igPushColumnClipRect,
    .PushColumnsBackground = igPushColumnsBackground,
    .PopColumnsBackground = igPopColumnsBackground,
    .GetColumnsID = igGetColumnsID,
    .FindOrCreateColumns = igFindOrCreateColumns,
    .GetColumnOffsetFromNorm = igGetColumnOffsetFromNorm,
    .GetColumnNormFromOffset = igGetColumnNormFromOffset,
    .TableOpenContextMenu = igTableOpenContextMenu,
    .TableSetColumnEnabled = igTableSetColumnEnabled,
    .TableSetColumnWidth = igTableSetColumnWidth,
    .TableSetColumnSortDirection = igTableSetColumnSortDirection,
    .TableGetHoveredColumn = igTableGetHoveredColumn,
    .TableGetHeaderRowHeight = igTableGetHeaderRowHeight,
    .TablePushBackgroundChannel = igTablePushBackgroundChannel,
    .TablePopBackgroundChannel = igTablePopBackgroundChannel,
    .GetCurrentTable = igGetCurrentTable,
    .TableFindByID = igTableFindByID,
    .BeginTableEx = igBeginTableEx,
    .TableBeginInitMemory = igTableBeginInitMemory,
    .TableBeginApplyRequests = igTableBeginApplyRequests,
    .TableSetupDrawChannels = igTableSetupDrawChannels,
    .TableUpdateLayout = igTableUpdateLayout,
    .TableUpdateBorders = igTableUpdateBorders,
    .TableUpdateColumnsWeightFromWidth = igTableUpdateColumnsWeightFromWidth,
    .TableDrawBorders = igTableDrawBorders,
    .TableDrawContextMenu = igTableDrawContextMenu,
    .TableMergeDrawChannels = igTableMergeDrawChannels,
    .TableSortSpecsSanitize = igTableSortSpecsSanitize,
    .TableSortSpecsBuild = igTableSortSpecsBuild,
    .TableGetColumnNextSortDirection = igTableGetColumnNextSortDirection,
    .TableFixColumnSortDirection = igTableFixColumnSortDirection,
    .TableGetColumnWidthAuto = igTableGetColumnWidthAuto,
    .TableBeginRow = igTableBeginRow,
    .TableEndRow = igTableEndRow,
    .TableBeginCell = igTableBeginCell,
    .TableEndCell = igTableEndCell,
    .TableGetCellBgRect = igTableGetCellBgRect,
    .TableGetColumnName_TablePtr = igTableGetColumnName_TablePtr,
    .TableGetColumnResizeID = igTableGetColumnResizeID,
    .TableGetMaxColumnWidth = igTableGetMaxColumnWidth,
    .TableSetColumnWidthAutoSingle = igTableSetColumnWidthAutoSingle,
    .TableSetColumnWidthAutoAll = igTableSetColumnWidthAutoAll,
    .TableRemove = igTableRemove,
    .TableGcCompactTransientBuffers = igTableGcCompactTransientBuffers,
    .TableGcCompactSettings = igTableGcCompactSettings,
    .TableLoadSettings = igTableLoadSettings,
    .TableSaveSettings = igTableSaveSettings,
    .TableResetSettings = igTableResetSettings,
    .TableGetBoundSettings = igTableGetBoundSettings,
    .TableSettingsInstallHandler = igTableSettingsInstallHandler,
    .TableSettingsCreate = igTableSettingsCreate,
    .TableSettingsFindByID = igTableSettingsFindByID,
    .BeginTabBarEx = igBeginTabBarEx,
    .TabBarFindTabByID = igTabBarFindTabByID,
    .TabBarFindMostRecentlySelectedTabForActiveWindow =
        igTabBarFindMostRecentlySelectedTabForActiveWindow,
    .TabBarAddTab = igTabBarAddTab,
    .TabBarRemoveTab = igTabBarRemoveTab,
    .TabBarCloseTab = igTabBarCloseTab,
    .TabBarQueueReorder = igTabBarQueueReorder,
    .TabBarProcessReorder = igTabBarProcessReorder,
    .TabItemEx = igTabItemEx,
    .TabItemCalcSize = igTabItemCalcSize,
    .TabItemBackground = igTabItemBackground,
    .TabItemLabelAndCloseButton = igTabItemLabelAndCloseButton,
    .RenderText = igRenderText,
    .RenderTextWrapped = igRenderTextWrapped,
    .RenderTextClipped = igRenderTextClipped,
    .RenderTextClippedEx = igRenderTextClippedEx,
    .RenderTextEllipsis = igRenderTextEllipsis,
    .RenderFrame = igRenderFrame,
    .RenderFrameBorder = igRenderFrameBorder,
    .RenderColorRectWithAlphaCheckerboard = igRenderColorRectWithAlphaCheckerboard,
    .RenderNavHighlight = igRenderNavHighlight,
    .FindRenderedTextEnd = igFindRenderedTextEnd,
    .RenderArrow = igRenderArrow,
    .RenderBullet = igRenderBullet,
    .RenderCheckMark = igRenderCheckMark,
    .RenderMouseCursor = igRenderMouseCursor,
    .RenderArrowPointingAt = igRenderArrowPointingAt,
    .RenderArrowDockMenu = igRenderArrowDockMenu,
    .RenderRectFilledRangeH = igRenderRectFilledRangeH,
    .RenderRectFilledWithHole = igRenderRectFilledWithHole,
    .TextEx = igTextEx,
    .ButtonEx = igButtonEx,
    .CloseButton = igCloseButton,
    .CollapseButton = igCollapseButton,
    .ArrowButtonEx = igArrowButtonEx,
    .Scrollbar = igScrollbar,
    .ScrollbarEx = igScrollbarEx,
    .ImageButtonEx = igImageButtonEx,
    .GetWindowScrollbarRect = igGetWindowScrollbarRect,
    .GetWindowScrollbarID = igGetWindowScrollbarID,
    .GetWindowResizeID = igGetWindowResizeID,
    .SeparatorEx = igSeparatorEx,
    .CheckboxFlags_S64Ptr = igCheckboxFlags_S64Ptr,
    .CheckboxFlags_U64Ptr = igCheckboxFlags_U64Ptr,
    .ButtonBehavior = igButtonBehavior,
    .DragBehavior = igDragBehavior,
    .SliderBehavior = igSliderBehavior,
    .SplitterBehavior = igSplitterBehavior,
    .TreeNodeBehavior = igTreeNodeBehavior,
    .TreeNodeBehaviorIsOpen = igTreeNodeBehaviorIsOpen,
    .TreePushOverrideID = igTreePushOverrideID,
    .DataTypeGetInfo = igDataTypeGetInfo,
    .DataTypeFormatString = igDataTypeFormatString,
    .DataTypeApplyOp = igDataTypeApplyOp,
    .DataTypeApplyOpFromText = igDataTypeApplyOpFromText,
    .DataTypeCompare = igDataTypeCompare,
    .DataTypeClamp = igDataTypeClamp,
    .InputTextEx = igInputTextEx,
    .TempInputText = igTempInputText,
    .TempInputScalar = igTempInputScalar,
    .TempInputIsActive = igTempInputIsActive,
    .GetInputTextState = igGetInputTextState,
    .ColorTooltip = igColorTooltip,
    .ColorEditOptionsPopup = igColorEditOptionsPopup,
    .ColorPickerOptionsPopup = igColorPickerOptionsPopup,
    .PlotEx = igPlotEx,
    .ShadeVertsLinearColorGradientKeepAlpha = igShadeVertsLinearColorGradientKeepAlpha,
    .ShadeVertsLinearUV = igShadeVertsLinearUV,
    .GcCompactTransientMiscBuffers = igGcCompactTransientMiscBuffers,
    .GcCompactTransientWindowBuffers = igGcCompactTransientWindowBuffers,
    .GcAwakeTransientWindowBuffers = igGcAwakeTransientWindowBuffers,
    .ErrorCheckEndFrameRecover = igErrorCheckEndFrameRecover,
    .DebugDrawItemRect = igDebugDrawItemRect,
    .DebugStartItemPicker = igDebugStartItemPicker,
    .DebugNodeColumns = igDebugNodeColumns,
    .DebugNodeDockNode = igDebugNodeDockNode,
    .DebugNodeDrawList = igDebugNodeDrawList,
    .DebugNodeDrawCmdShowMeshAndBoundingBox = igDebugNodeDrawCmdShowMeshAndBoundingBox,
    .DebugNodeStorage = igDebugNodeStorage,
    .DebugNodeTabBar = igDebugNodeTabBar,
    .DebugNodeTable = igDebugNodeTable,
    .DebugNodeTableSettings = igDebugNodeTableSettings,
    .DebugNodeWindow = igDebugNodeWindow,
    .DebugNodeWindowSettings = igDebugNodeWindowSettings,
    .DebugNodeWindowsList = igDebugNodeWindowsList,
    .DebugNodeViewport = igDebugNodeViewport,
    .DebugRenderViewportThumbnail = igDebugRenderViewportThumbnail,
    .ImFontAtlasGetBuilderForStbTruetype = igImFontAtlasGetBuilderForStbTruetype,
    .ImFontAtlasBuildInit = igImFontAtlasBuildInit,
    .ImFontAtlasBuildSetupFont = igImFontAtlasBuildSetupFont,
    .ImFontAtlasBuildPackCustomRects = igImFontAtlasBuildPackCustomRects,
    .ImFontAtlasBuildFinish = igImFontAtlasBuildFinish,
    .ImFontAtlasBuildRender8bppRectFromString = igImFontAtlasBuildRender8bppRectFromString,
    .ImFontAtlasBuildRender32bppRectFromString = igImFontAtlasBuildRender32bppRectFromString,
    .ImFontAtlasBuildMultiplyCalcLookupTable = igImFontAtlasBuildMultiplyCalcLookupTable,
    .ImFontAtlasBuildMultiplyRectAlpha8 = igImFontAtlasBuildMultiplyRectAlpha8,
    .LogText = igLogText,
    .GET_FLT_MAX = igGET_FLT_MAX,
    .GET_FLT_MIN = igGET_FLT_MIN,
    .ImGuiIO_AddInputCharacter = ImGuiIO_AddInputCharacter,
    .ImGuiIO_AddInputCharacterUTF16 = ImGuiIO_AddInputCharacterUTF16,
    .ImGuiIO_AddInputCharactersUTF8 = ImGuiIO_AddInputCharactersUTF8,
    .ImGuiIO_ClearInputCharacters = ImGuiIO_ClearInputCharacters,
    .ImGuiIO_ImGuiIO = ImGuiIO_ImGuiIO,
    .ImGuiIO_destroy = ImGuiIO_destroy,
    .ImDrawList_ImDrawList = ImDrawList_ImDrawList,
    .ImDrawList_destroy = ImDrawList_destroy,
    .ImDrawList_PushClipRect = ImDrawList_PushClipRect,
    .ImDrawList_PushClipRectFullScreen = ImDrawList_PushClipRectFullScreen,
    .ImDrawList_PopClipRect = ImDrawList_PopClipRect,
    .ImDrawList_PushTextureID = ImDrawList_PushTextureID,
    .ImDrawList_PopTextureID = ImDrawList_PopTextureID,
    .ImDrawList_GetClipRectMin = ImDrawList_GetClipRectMin,
    .ImDrawList_GetClipRectMax = ImDrawList_GetClipRectMax,
    .ImDrawList_AddLine = ImDrawList_AddLine,
    .ImDrawList_AddRect = ImDrawList_AddRect,
    .ImDrawList_AddRectFilled = ImDrawList_AddRectFilled,
    .ImDrawList_AddRectFilledMultiColor = ImDrawList_AddRectFilledMultiColor,
    .ImDrawList_AddQuad = ImDrawList_AddQuad,
    .ImDrawList_AddQuadFilled = ImDrawList_AddQuadFilled,
    .ImDrawList_AddTriangle = ImDrawList_AddTriangle,
    .ImDrawList_AddTriangleFilled = ImDrawList_AddTriangleFilled,
    .ImDrawList_AddCircle = ImDrawList_AddCircle,
    .ImDrawList_AddCircleFilled = ImDrawList_AddCircleFilled,
    .ImDrawList_AddNgon = ImDrawList_AddNgon,
    .ImDrawList_AddNgonFilled = ImDrawList_AddNgonFilled,
    .ImDrawList_AddText_Vec2 = ImDrawList_AddText_Vec2,
    .ImDrawList_AddText_FontPtr = ImDrawList_AddText_FontPtr,
    .ImDrawList_AddPolyline = ImDrawList_AddPolyline,
    .ImDrawList_AddConvexPolyFilled = ImDrawList_AddConvexPolyFilled,
    .ImDrawList_AddBezierCubic = ImDrawList_AddBezierCubic,
    .ImDrawList_AddBezierQuadratic = ImDrawList_AddBezierQuadratic,
    .ImDrawList_AddImage = ImDrawList_AddImage,
    .ImDrawList_AddImageQuad = ImDrawList_AddImageQuad,
    .ImDrawList_AddImageRounded = ImDrawList_AddImageRounded,
    .ImDrawList_PathClear = ImDrawList_PathClear,
    .ImDrawList_PathLineTo = ImDrawList_PathLineTo,
    .ImDrawList_PathLineToMergeDuplicate = ImDrawList_PathLineToMergeDuplicate,
    .ImDrawList_PathFillConvex = ImDrawList_PathFillConvex,
    .ImDrawList_PathStroke = ImDrawList_PathStroke,
    .ImDrawList_PathArcTo = ImDrawList_PathArcTo,
    .ImDrawList_PathArcToFast = ImDrawList_PathArcToFast,
    .ImDrawList_PathBezierCubicCurveTo = ImDrawList_PathBezierCubicCurveTo,
    .ImDrawList_PathBezierQuadraticCurveTo = ImDrawList_PathBezierQuadraticCurveTo,
    .ImDrawList_PathRect = ImDrawList_PathRect,
    .ImDrawList_AddCallback = ImDrawList_AddCallback,
    .ImDrawList_AddDrawCmd = ImDrawList_AddDrawCmd,
    .ImDrawList_CloneOutput = ImDrawList_CloneOutput,
    .ImDrawList_ChannelsSplit = ImDrawList_ChannelsSplit,
    .ImDrawList_ChannelsMerge = ImDrawList_ChannelsMerge,
    .ImDrawList_ChannelsSetCurrent = ImDrawList_ChannelsSetCurrent,
    .ImDrawList_PrimReserve = ImDrawList_PrimReserve,
    .ImDrawList_PrimUnreserve = ImDrawList_PrimUnreserve,
    .ImDrawList_PrimRect = ImDrawList_PrimRect,
    .ImDrawList_PrimRectUV = ImDrawList_PrimRectUV,
    .ImDrawList_PrimQuadUV = ImDrawList_PrimQuadUV,
    .ImDrawList_PrimWriteVtx = ImDrawList_PrimWriteVtx,
    .ImDrawList_PrimWriteIdx = ImDrawList_PrimWriteIdx,
    .ImDrawList_PrimVtx = ImDrawList_PrimVtx,
    .ImDrawList__ResetForNewFrame = ImDrawList__ResetForNewFrame,
    .ImDrawList__ClearFreeMemory = ImDrawList__ClearFreeMemory,
    .ImDrawList__PopUnusedDrawCmd = ImDrawList__PopUnusedDrawCmd,
    .ImDrawList__OnChangedClipRect = ImDrawList__OnChangedClipRect,
    .ImDrawList__OnChangedTextureID = ImDrawList__OnChangedTextureID,
    .ImDrawList__OnChangedVtxOffset = ImDrawList__OnChangedVtxOffset,
    .ImDrawList__CalcCircleAutoSegmentCount = ImDrawList__CalcCircleAutoSegmentCount,
    .ImDrawList__PathArcToFastEx = ImDrawList__PathArcToFastEx,
    .ImDrawList__PathArcToN = ImDrawList__PathArcToN,
    .ImFont_ImFont = ImFont_ImFont,
    .ImFont_destroy = ImFont_destroy,
    .ImFont_FindGlyph = ImFont_FindGlyph,
    .ImFont_FindGlyphNoFallback = ImFont_FindGlyphNoFallback,
    .ImFont_GetCharAdvance = ImFont_GetCharAdvance,
    .ImFont_IsLoaded = ImFont_IsLoaded,
    .ImFont_GetDebugName = ImFont_GetDebugName,
    .ImFont_CalcTextSizeA = ImFont_CalcTextSizeA,
    .ImFont_CalcWordWrapPositionA = ImFont_CalcWordWrapPositionA,
    .ImFont_RenderChar = ImFont_RenderChar,
    .ImFont_RenderText = ImFont_RenderText,
    .ImFont_BuildLookupTable = ImFont_BuildLookupTable,
    .ImFont_ClearOutputData = ImFont_ClearOutputData,
    .ImFont_GrowIndex = ImFont_GrowIndex,
    .ImFont_AddGlyph = ImFont_AddGlyph,
    .ImFont_AddRemapChar = ImFont_AddRemapChar,
    .ImFont_SetGlyphVisible = ImFont_SetGlyphVisible,
    .ImFont_SetFallbackChar = ImFont_SetFallbackChar,
    .ImFont_IsGlyphRangeUnused = ImFont_IsGlyphRangeUnused,
    .ImFontAtlas_ImFontAtlas = ImFontAtlas_ImFontAtlas,
    .ImFontAtlas_destroy = ImFontAtlas_destroy,
    .ImFontAtlas_AddFont = ImFontAtlas_AddFont,
    .ImFontAtlas_AddFontDefault = ImFontAtlas_AddFontDefault,
    .ImFontAtlas_AddFontFromFileTTF = ImFontAtlas_AddFontFromFileTTF,
    .ImFontAtlas_AddFontFromMemoryTTF = ImFontAtlas_AddFontFromMemoryTTF,
    .ImFontAtlas_AddFontFromMemoryCompressedTTF = ImFontAtlas_AddFontFromMemoryCompressedTTF,
    .ImFontAtlas_AddFontFromMemoryCompressedBase85TTF =
        ImFontAtlas_AddFontFromMemoryCompressedBase85TTF,
    .ImFontAtlas_ClearInputData = ImFontAtlas_ClearInputData,
    .ImFontAtlas_ClearTexData = ImFontAtlas_ClearTexData,
    .ImFontAtlas_ClearFonts = ImFontAtlas_ClearFonts,
    .ImFontAtlas_Clear = ImFontAtlas_Clear,
    .ImFontAtlas_Build = ImFontAtlas_Build,
    .ImFontAtlas_GetTexDataAsAlpha8 = ImFontAtlas_GetTexDataAsAlpha8,
    .ImFontAtlas_GetTexDataAsRGBA32 = ImFontAtlas_GetTexDataAsRGBA32,
    .ImFontAtlas_IsBuilt = ImFontAtlas_IsBuilt,
    .ImFontAtlas_SetTexID = ImFontAtlas_SetTexID,
    .ImFontAtlas_GetGlyphRangesDefault = ImFontAtlas_GetGlyphRangesDefault,
    .ImFontAtlas_GetGlyphRangesKorean = ImFontAtlas_GetGlyphRangesKorean,
    .ImFontAtlas_GetGlyphRangesJapanese = ImFontAtlas_GetGlyphRangesJapanese,
    .ImFontAtlas_GetGlyphRangesChineseFull = ImFontAtlas_GetGlyphRangesChineseFull,
    .ImFontAtlas_GetGlyphRangesChineseSimplifiedCommon =
        ImFontAtlas_GetGlyphRangesChineseSimplifiedCommon,
    .ImFontAtlas_GetGlyphRangesCyrillic = ImFontAtlas_GetGlyphRangesCyrillic,
    .ImFontAtlas_GetGlyphRangesThai = ImFontAtlas_GetGlyphRangesThai,
    .ImFontAtlas_GetGlyphRangesVietnamese = ImFontAtlas_GetGlyphRangesVietnamese,
    .ImFontAtlas_AddCustomRectRegular = ImFontAtlas_AddCustomRectRegular,
    .ImFontAtlas_AddCustomRectFontGlyph = ImFontAtlas_AddCustomRectFontGlyph,
    .ImFontAtlas_GetCustomRectByIndex = ImFontAtlas_GetCustomRectByIndex,
    .ImFontAtlas_CalcCustomRectUV = ImFontAtlas_CalcCustomRectUV,
    .ImFontAtlas_GetMouseCursorTexData = ImFontAtlas_GetMouseCursorTexData,
    .ImGuiPayload_ImGuiPayload = ImGuiPayload_ImGuiPayload,
    .ImGuiPayload_destroy = ImGuiPayload_destroy,
    .ImGuiPayload_Clear = ImGuiPayload_Clear,
    .ImGuiPayload_IsDataType = ImGuiPayload_IsDataType,
    .ImGuiPayload_IsPreview = ImGuiPayload_IsPreview,
    .ImGuiPayload_IsDelivery = ImGuiPayload_IsDelivery,
    .ImGuiListClipper_ImGuiListClipper = ImGuiListClipper_ImGuiListClipper,
    .ImGuiListClipper_destroy = ImGuiListClipper_destroy,
    .ImGuiListClipper_Begin = ImGuiListClipper_Begin,
    .ImGuiListClipper_End = ImGuiListClipper_End,
    .ImGuiListClipper_Step = ImGuiListClipper_Step,
    .ImGuiTextFilter_ImGuiTextFilter = ImGuiTextFilter_ImGuiTextFilter,
    .ImGuiTextFilter_destroy = ImGuiTextFilter_destroy,
    .ImGuiTextFilter_Draw = ImGuiTextFilter_Draw,
    .ImGuiTextFilter_PassFilter = ImGuiTextFilter_PassFilter,
    .ImGuiTextFilter_Build = ImGuiTextFilter_Build,
    .ImGuiTextFilter_Clear = ImGuiTextFilter_Clear,
    .ImGuiTextFilter_IsActive = ImGuiTextFilter_IsActive,
    .ImGuiTextBuffer_ImGuiTextBuffer = ImGuiTextBuffer_ImGuiTextBuffer,
    .ImGuiTextBuffer_destroy = ImGuiTextBuffer_destroy,
    .ImGuiTextBuffer_begin = ImGuiTextBuffer_begin,
    .ImGuiTextBuffer_end = ImGuiTextBuffer_end,
    .ImGuiTextBuffer_size = ImGuiTextBuffer_size,
    .ImGuiTextBuffer_empty = ImGuiTextBuffer_empty,
    .ImGuiTextBuffer_clear = ImGuiTextBuffer_clear,
    .ImGuiTextBuffer_reserve = ImGuiTextBuffer_reserve,
    .ImGuiTextBuffer_c_str = ImGuiTextBuffer_c_str,
    .ImGuiTextBuffer_append = ImGuiTextBuffer_append,
    .ImGuiTextBuffer_appendfv = ImGuiTextBuffer_appendfv,
    .ImGuiTextBuffer_appendf = ImGuiTextBuffer_appendf
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// sokol impl with dummy-backend for imgui
RIZZ_STATE const sx_alloc* g_sg_imgui_alloc;

#define SOKOL_MALLOC(s) sx_malloc(g_sg_imgui_alloc, s)
#define SOKOL_FREE(p) sx_free(g_sg_imgui_alloc, p)
#define SOKOL_ASSERT(c) sx_assert(c)
#define SOKOL_LOG(s) rizz_log_error(s)

SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function")
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-variable")
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-parameter")
#define SOKOL_GFX_IMGUI_IMPL
#define SOKOL_API_DECL static
#define SOKOL_API_IMPL static
#define SOKOL_GFX_INCLUDED
#include "sokol/util/sokol_gfx_imgui.h"
SX_PRAGMA_DIAGNOSTIC_POP();

#include rizz_shader_path(shaders_h, imgui.frag.h)
#include rizz_shader_path(shaders_h, imgui.vert.h)

typedef struct imgui__context {
    ImGuiContext* ctx;
    sx_pool* small_mem_pool;
    int max_verts;
    int max_indices;
    ImDrawVert* verts;
    uint16_t* indices;
    sg_shader shader;
    sg_pipeline pip;
    sg_bindings bind;
    sg_image font_tex;
    rizz_gfx_stage stage;
    bool mouse_btn_down[RIZZ_APP_MAX_MOUSEBUTTONS];
    bool mouse_btn_up[RIZZ_APP_MAX_MOUSEBUTTONS];
    float mouse_wheel_h;
    float mouse_wheel;
    bool keys_down[512];
    ImWchar* char_input;    // sx_array
    ImGuiMouseCursor last_cursor;
    sg_imgui_t sg_imgui;
    float fts[MAX_BUFFERED_FRAME_TIMES];
    int ft_iter;
    int ft_iter_nomod;
    ImGuiID dock_space_id;
    bool docking;
} imgui__context;

typedef struct imgui__shader_uniforms {
    sx_vec2 disp_size;
    float padd[2];
} imgui__shader_uniforms;

static rizz_vertex_layout k__imgui_vertex = {
    .attrs[0] = { .semantic = "POSITION", .offset = offsetof(ImDrawVert, pos) },
    .attrs[1] = { .semantic = "TEXCOORD", .offset = offsetof(ImDrawVert, uv) },
    .attrs[2] = { .semantic = "COLOR",
                  .offset = offsetof(ImDrawVert, col),
                  .format = SG_VERTEXFORMAT_UBYTE4N }
};

RIZZ_STATE static imgui__context g_imgui;

static void* imgui__malloc(size_t sz, void* user_data)
{
    const sx_alloc* fallback_alloc = (const sx_alloc*)user_data;
    return (sz <= IMGUI_SMALL_MEMORY_SIZE)
               ? sx_pool_new_and_grow(g_imgui.small_mem_pool, fallback_alloc)
               : sx_malloc(fallback_alloc, sz);
}

static void imgui__free(void* ptr, void* user_data)
{
    const sx_alloc* fallback_alloc = (const sx_alloc*)user_data;
    if (ptr) {
        if (sx_pool_valid_ptr(g_imgui.small_mem_pool, ptr)) {
            sx_pool_del(g_imgui.small_mem_pool, ptr);
        } else {
            sx_free(fallback_alloc, ptr);
        }
    }
}

static bool imgui__resize_buffers(int max_verts, int max_indices)
{
    const sx_alloc* alloc = the_core->alloc(RIZZ_MEMID_TOOLSET);

    g_imgui.verts = (ImDrawVert*)sx_realloc(alloc, g_imgui.verts, sizeof(ImDrawVert) * max_verts);
    g_imgui.indices = (uint16_t*)sx_realloc(alloc, g_imgui.indices, sizeof(uint16_t) * max_indices);
    if (!g_imgui.verts || !g_imgui.indices) {
        sx_out_of_memory();
        return false;
    }

    the_gfx->destroy_buffer(g_imgui.bind.vertex_buffers[0]);
    the_gfx->destroy_buffer(g_imgui.bind.index_buffer);
    g_imgui.bind.vertex_buffers[0] =
        the_gfx->make_buffer(&(sg_buffer_desc){ .type = SG_BUFFERTYPE_VERTEXBUFFER,
                                                .usage = SG_USAGE_STREAM,
                                                .size = sizeof(ImDrawVert) * max_verts,
                                                .label = "imgui_vbuff" });
    g_imgui.bind.index_buffer =
        the_gfx->make_buffer(&(sg_buffer_desc){ .type = SG_BUFFERTYPE_INDEXBUFFER,
                                                .usage = SG_USAGE_STREAM,
                                                .size = sizeof(uint16_t) * max_indices,
                                                .label = "imgui_ibuff" });
    if (!g_imgui.bind.vertex_buffers[0].id || !g_imgui.bind.index_buffer.id) {
        return false;
    }

    g_imgui.max_verts = max_verts;
    g_imgui.max_indices = max_indices;
    return true;
}

static void apply_theme(void)
{
    ImGuiStyle* style = the__imgui.GetStyle();
    
    style->WindowTitleAlign = sx_vec2f(0.5f, 0.5f);

    style->ScrollbarSize = 18;
    style->GrabMinSize = 12;
    style->WindowBorderSize = 1;
    style->ChildBorderSize = 0;
    style->PopupBorderSize = 0;
    style->FrameBorderSize = 0;
    style->TabBorderSize = 0;

    style->WindowRounding = 0;
    style->ChildRounding = 3;
    style->FrameRounding = 3;
    style->PopupRounding = 3;
    style->ScrollbarRounding = 3;
    style->GrabRounding = 3;
    style->TabRounding = 2;

    style->AntiAliasedFill = true;
    style->AntiAliasedLines = true;

    style->Colors[ImGuiCol_Text]                   = sx_vec4f(1.00f, 1.00f, 1.00f, 0.89f);
    style->Colors[ImGuiCol_TextDisabled]           = sx_vec4f(1.00f, 1.00f, 1.00f, 0.39f);
    style->Colors[ImGuiCol_WindowBg]               = sx_vec4f(0.20f, 0.20f, 0.20f, 1.00f);
    style->Colors[ImGuiCol_ChildBg]                = sx_vec4f(0.24f, 0.24f, 0.24f, 1.00f);
    style->Colors[ImGuiCol_PopupBg]                = sx_vec4f(0.20f, 0.20f, 0.20f, 1.00f);
    style->Colors[ImGuiCol_Border]                 = sx_vec4f(1.00f, 1.00f, 1.00f, 0.10f);
    style->Colors[ImGuiCol_BorderShadow]           = sx_vec4f(0.18f, 0.18f, 0.18f, 1.00f);
    style->Colors[ImGuiCol_FrameBg]                = sx_vec4f(0.14f, 0.14f, 0.14f, 1.00f);
    style->Colors[ImGuiCol_FrameBgHovered]         = sx_vec4f(1.00f, 1.00f, 1.00f, 0.08f);
    style->Colors[ImGuiCol_FrameBgActive]          = sx_vec4f(1.00f, 1.00f, 1.00f, 0.12f);
    style->Colors[ImGuiCol_TitleBg]                = sx_vec4f(0.22f, 0.22f, 0.22f, 1.00f);
    style->Colors[ImGuiCol_TitleBgActive]          = sx_vec4f(0.14f, 0.14f, 0.14f, 1.00f);
    style->Colors[ImGuiCol_TitleBgCollapsed]       = sx_vec4f(0.00f, 0.00f, 0.00f, 0.51f);
    style->Colors[ImGuiCol_MenuBarBg]              = sx_vec4f(0.14f, 0.14f, 0.14f, 1.00f);
    style->Colors[ImGuiCol_ScrollbarBg]            = sx_vec4f(0.02f, 0.02f, 0.02f, 0.53f);
    style->Colors[ImGuiCol_ScrollbarGrab]          = sx_vec4f(0.31f, 0.31f, 0.31f, 1.00f);
    style->Colors[ImGuiCol_ScrollbarGrabHovered]   = sx_vec4f(0.41f, 0.41f, 0.41f, 1.00f);
    style->Colors[ImGuiCol_ScrollbarGrabActive]    = sx_vec4f(0.51f, 0.51f, 0.51f, 1.00f);
    style->Colors[ImGuiCol_CheckMark]              = sx_vec4f(0.80f, 0.47f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_SliderGrab]             = sx_vec4f(0.39f, 0.39f, 0.39f, 1.00f);
    style->Colors[ImGuiCol_SliderGrabActive]       = sx_vec4f(0.80f, 0.47f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_Button]                 = sx_vec4f(0.33f, 0.33f, 0.33f, 1.00f);
    style->Colors[ImGuiCol_ButtonHovered]          = sx_vec4f(1.00f, 1.00f, 1.00f, 0.39f);
    style->Colors[ImGuiCol_ButtonActive]           = sx_vec4f(1.00f, 1.00f, 1.00f, 0.55f);
    style->Colors[ImGuiCol_Header]                 = sx_vec4f(0.00f, 0.00f, 0.00f, 0.39f);
    style->Colors[ImGuiCol_HeaderHovered]          = sx_vec4f(1.00f, 1.00f, 1.00f, 0.16f);
    style->Colors[ImGuiCol_HeaderActive]           = sx_vec4f(1.00f, 1.00f, 1.00f, 0.16f);
    style->Colors[ImGuiCol_Separator]              = sx_vec4f(1.00f, 1.00f, 1.00f, 0.15f);
    style->Colors[ImGuiCol_SeparatorHovered]       = sx_vec4f(0.80f, 0.47f, 0.00f, 0.50f);
    style->Colors[ImGuiCol_SeparatorActive]        = sx_vec4f(0.80f, 0.47f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_ResizeGrip]             = sx_vec4f(1.00f, 1.00f, 1.00f, 0.25f);
    style->Colors[ImGuiCol_ResizeGripHovered]      = sx_vec4f(1.00f, 1.00f, 1.00f, 0.31f);
    style->Colors[ImGuiCol_ResizeGripActive]       = sx_vec4f(0.80f, 0.47f, 0.00f, 0.86f);
    style->Colors[ImGuiCol_Tab]                    = sx_vec4f(0.14f, 0.14f, 0.14f, 1.00f);
    style->Colors[ImGuiCol_TabHovered]             = sx_vec4f(0.80f, 0.47f, 0.00f, 0.25f);
    style->Colors[ImGuiCol_TabActive]              = sx_vec4f(0.80f, 0.47f, 0.00f, 0.59f);
    style->Colors[ImGuiCol_TabUnfocused]           = sx_vec4f(0.24f, 0.24f, 0.24f, 1.00f);
    style->Colors[ImGuiCol_TabUnfocusedActive]     = sx_vec4f(0.10f, 0.10f, 0.10f, 1.00f);
    style->Colors[ImGuiCol_PlotLines]              = sx_vec4f(0.86f, 0.86f, 0.86f, 1.00f);
    style->Colors[ImGuiCol_PlotLinesHovered]       = sx_vec4f(0.80f, 0.47f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_PlotHistogram]          = sx_vec4f(0.80f, 0.47f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_PlotHistogramHovered]   = sx_vec4f(1.00f, 0.89f, 0.62f, 1.00f);
    style->Colors[ImGuiCol_TextSelectedBg]         = sx_vec4f(0.80f, 0.47f, 0.00f, 0.25f);
    style->Colors[ImGuiCol_DragDropTarget]         = sx_vec4f(1.00f, 0.86f, 0.00f, 0.86f);
    style->Colors[ImGuiCol_NavHighlight]           = sx_vec4f(0.80f, 0.47f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_NavWindowingHighlight]  = sx_vec4f(1.00f, 1.00f, 1.00f, 0.71f);
    style->Colors[ImGuiCol_NavWindowingDimBg]      = sx_vec4f(0.80f, 0.80f, 0.80f, 0.20f);
    style->Colors[ImGuiCol_ModalWindowDimBg]       = sx_vec4f(0.80f, 0.80f, 0.80f, 0.35f);
}

static bool imgui__init(void)
{
    sx_assert(g_imgui.ctx == NULL);

    g_imgui.docking = the_app->config()->imgui_docking;

    const sx_alloc* alloc = the_core->alloc(RIZZ_MEMID_TOOLSET);
    g_sg_imgui_alloc = alloc;
    g_imgui.small_mem_pool = sx_pool_create(alloc, IMGUI_SMALL_MEMORY_SIZE, 1000);
    if (!g_imgui.small_mem_pool) {
        sx_memory_fail();
        return false;
    }

    the__imgui.SetAllocatorFunctions(imgui__malloc, imgui__free, (void*)alloc);

    g_imgui.last_cursor = ImGuiMouseCursor_COUNT;
    g_imgui.ctx = the__imgui.CreateContext(NULL);
    if (!g_imgui.ctx) {
        rizz_log_error("imgui: init failed");
        return false;
    }

    static char ini_filename[64];
    ImGuiIO* conf = the__imgui.GetIO();
    sx_snprintf(ini_filename, sizeof(ini_filename), "%s_imgui.ini", the_app->name());
    conf->IniFilename = ini_filename;

    float fb_scale = the_app->dpiscale();
    conf->DisplayFramebufferScale = sx_vec2f(fb_scale, fb_scale);

    the__imgui.StyleColorsDark(NULL);
    ImFontConfig font_conf = { .FontDataOwnedByAtlas = true,
                               .OversampleH = 3,
                               .OversampleV = 1,
                               .RasterizerMultiply = 1.0f,
                               .GlyphMaxAdvanceX = FLT_MAX };

    the__imgui.ImFontAtlas_AddFontFromMemoryCompressedTTF(
        conf->Fonts, k_imgui_roboto_compressed_data, k_imgui_roboto_compressed_size, 14.0f,
        &font_conf, NULL);

    conf->KeyMap[ImGuiKey_Tab] = RIZZ_APP_KEYCODE_TAB;
    conf->KeyMap[ImGuiKey_LeftArrow] = RIZZ_APP_KEYCODE_LEFT;
    conf->KeyMap[ImGuiKey_RightArrow] = RIZZ_APP_KEYCODE_RIGHT;
    conf->KeyMap[ImGuiKey_UpArrow] = RIZZ_APP_KEYCODE_UP;
    conf->KeyMap[ImGuiKey_DownArrow] = RIZZ_APP_KEYCODE_DOWN;
    conf->KeyMap[ImGuiKey_PageUp] = RIZZ_APP_KEYCODE_PAGE_UP;
    conf->KeyMap[ImGuiKey_PageDown] = RIZZ_APP_KEYCODE_PAGE_DOWN;
    conf->KeyMap[ImGuiKey_Home] = RIZZ_APP_KEYCODE_HOME;
    conf->KeyMap[ImGuiKey_End] = RIZZ_APP_KEYCODE_END;
    conf->KeyMap[ImGuiKey_Insert] = RIZZ_APP_KEYCODE_INSERT;
    conf->KeyMap[ImGuiKey_Delete] = RIZZ_APP_KEYCODE_DELETE;
    conf->KeyMap[ImGuiKey_Backspace] = RIZZ_APP_KEYCODE_BACKSPACE;
    conf->KeyMap[ImGuiKey_Space] = RIZZ_APP_KEYCODE_SPACE;
    conf->KeyMap[ImGuiKey_Enter] = RIZZ_APP_KEYCODE_ENTER;
    conf->KeyMap[ImGuiKey_KeyPadEnter] = RIZZ_APP_KEYCODE_KP_ENTER;
    conf->KeyMap[ImGuiKey_Escape] = RIZZ_APP_KEYCODE_ESCAPE;
    conf->KeyMap[ImGuiKey_A] = RIZZ_APP_KEYCODE_A;
    conf->KeyMap[ImGuiKey_C] = RIZZ_APP_KEYCODE_C;
    conf->KeyMap[ImGuiKey_V] = RIZZ_APP_KEYCODE_V;
    conf->KeyMap[ImGuiKey_X] = RIZZ_APP_KEYCODE_X;
    conf->KeyMap[ImGuiKey_Y] = RIZZ_APP_KEYCODE_Y;
    conf->KeyMap[ImGuiKey_Z] = RIZZ_APP_KEYCODE_Z;

    if (g_imgui.docking) {
        conf->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        conf->BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
        conf->ConfigWindowsResizeFromEdges = true;
    }
    
    // Setup graphic objects
    if (!imgui__resize_buffers(MAX_VERTS, MAX_INDICES)) {
        rizz_log_error("imgui: could not create vertex/index buffers");
        return false;
    }

    uint8_t* font_pixels;
    int font_width, font_height, bpp;
    the__imgui.ImFontAtlas_GetTexDataAsRGBA32(conf->Fonts, &font_pixels, &font_width, &font_height,
                                              &bpp);
    g_imgui.font_tex = the_gfx->make_image(
        &(sg_image_desc){ .width = font_width,
                          .height = font_height,
                          .pixel_format = SG_PIXELFORMAT_RGBA8,
                          .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
                          .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
                          .min_filter = SG_FILTER_LINEAR,
                          .mag_filter = SG_FILTER_LINEAR,
                          .content.subimage[0][0].ptr = font_pixels,
                          .content.subimage[0][0].size = font_width * font_height * 4,
                          .label = "imgui_font" });

    conf->Fonts->TexID = (ImTextureID)(uintptr_t)g_imgui.font_tex.id;

    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    sx_scope(the_core->tmp_alloc_pop()) {
        rizz_shader shader = the_gfx->shader_make_with_data(
            tmp_alloc, k_imgui_vs_size, k_imgui_vs_data, k_imgui_vs_refl_size, k_imgui_vs_refl_data,
            k_imgui_fs_size, k_imgui_fs_data, k_imgui_fs_refl_size, k_imgui_fs_refl_data);
        g_imgui.shader = shader.shd;

        sg_pipeline_desc pip_desc = { .layout.buffers[0].stride = sizeof(ImDrawVert),
                                      .shader = g_imgui.shader,
                                      .index_type = SG_INDEXTYPE_UINT16,                                  
                                      .rasterizer = { .cull_mode = SG_CULLMODE_NONE },
                                      .blend = { .enabled = true,
                                                 .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                                                 .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                                                 .color_write_mask = SG_COLORMASK_RGB },
                                      .label = "imgui" };
        g_imgui.pip = the_gfx->make_pipeline(
            the_gfx->shader_bindto_pipeline(&shader, &pip_desc, &k__imgui_vertex));

        apply_theme();
    } // scope

    g_imgui.stage = the_gfx->stage_register("imgui", (rizz_gfx_stage) {0});

    return true;
}

static void imgui__release()
{
    const sx_alloc* alloc = the_core->alloc(RIZZ_MEMID_TOOLSET);

    if (g_imgui.ctx)
        the__imgui.DestroyContext(g_imgui.ctx);

    the_gfx->destroy_pipeline(g_imgui.pip);
    the_gfx->destroy_buffer(g_imgui.bind.vertex_buffers[0]);
    the_gfx->destroy_buffer(g_imgui.bind.index_buffer);
    the_gfx->destroy_shader(g_imgui.shader);
    the_gfx->destroy_image(g_imgui.font_tex);
    sx_free(alloc, g_imgui.verts);
    sx_free(alloc, g_imgui.indices);
    sx_array_free(alloc, g_imgui.char_input);
    sx_pool_destroy(g_imgui.small_mem_pool, alloc);
}

static void imgui__update_cursor()
{
#if SX_PLATFORM_WINDOWS
    ImGuiIO* io = the__imgui.GetIO();
    if (io->ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
        return;

    ImGuiMouseCursor imgui_cursor = the__imgui.GetMouseCursor();
    if (imgui_cursor == ImGuiMouseCursor_None || io->MouseDrawCursor) {
        SetCursor(NULL);
    } else {
        // Show OS mouse cursor
        LPTSTR win32_cursor = IDC_ARROW;
        switch (imgui_cursor) {
        case ImGuiMouseCursor_Arrow:
            win32_cursor = IDC_ARROW;
            break;
        case ImGuiMouseCursor_TextInput:
            win32_cursor = IDC_IBEAM;
            break;
        case ImGuiMouseCursor_ResizeAll:
            win32_cursor = IDC_SIZEALL;
            break;
        case ImGuiMouseCursor_ResizeEW:
            win32_cursor = IDC_SIZEWE;
            break;
        case ImGuiMouseCursor_ResizeNS:
            win32_cursor = IDC_SIZENS;
            break;
        case ImGuiMouseCursor_ResizeNESW:
            win32_cursor = IDC_SIZENESW;
            break;
        case ImGuiMouseCursor_ResizeNWSE:
            win32_cursor = IDC_SIZENWSE;
            break;
        case ImGuiMouseCursor_Hand:
            win32_cursor = IDC_HAND;
            break;
        }
        SetCursor(LoadCursor(NULL, win32_cursor));
    }
#endif
}

static void imgui__frame()
{
    ImGuiIO* io = the__imgui.GetIO();
    the_app->window_size(&io->DisplaySize);
    io->DeltaTime = (float)sx_tm_sec(the_core->delta_tick());
    if (io->DeltaTime == 0) {
        io->DeltaTime = 0.033f;
    }

    for (int i = 0; i < RIZZ_APP_MAX_MOUSEBUTTONS; i++) {
        if (g_imgui.mouse_btn_down[i]) {
            g_imgui.mouse_btn_down[i] = false;
            io->MouseDown[i] = true;
        } else if (g_imgui.mouse_btn_up[i]) {
            g_imgui.mouse_btn_up[i] = false;
            io->MouseDown[i] = false;
        }
    }

    io->KeyShift = the_app->key_pressed(RIZZ_APP_KEYCODE_LEFT_SHIFT) ||
                   the_app->key_pressed(RIZZ_APP_KEYCODE_RIGHT_SHIFT);

    io->KeyAlt = the_app->key_pressed(RIZZ_APP_KEYCODE_LEFT_ALT) ||
                   the_app->key_pressed(RIZZ_APP_KEYCODE_RIGHT_ALT);

    io->KeyCtrl = the_app->key_pressed(RIZZ_APP_KEYCODE_LEFT_CONTROL) ||
                   the_app->key_pressed(RIZZ_APP_KEYCODE_RIGHT_CONTROL);

    io->KeySuper = the_app->key_pressed(RIZZ_APP_KEYCODE_LEFT_SUPER) ||
                   the_app->key_pressed(RIZZ_APP_KEYCODE_RIGHT_SUPER);

    io->MouseWheel = g_imgui.mouse_wheel;
    io->MouseWheelH = g_imgui.mouse_wheel_h;
    g_imgui.mouse_wheel_h = g_imgui.mouse_wheel = 0;

    sx_memcpy(io->KeysDown, g_imgui.keys_down, sizeof(io->KeysDown));
    sx_memset(g_imgui.keys_down, 0x0, sizeof(g_imgui.keys_down));

    for (int i = 0; i < sx_array_count(g_imgui.char_input); i++) {
        the__imgui.ImGuiIO_AddInputCharacter(io, g_imgui.char_input[i]);
    }
    sx_array_clear(g_imgui.char_input);

    if (io->WantTextInput && !the_app->keyboard_shown()) {
        the_app->show_keyboard(true);
    }
    if (!io->WantTextInput && the_app->keyboard_shown()) {
        // the_app->show_keyboard(false);
    }

    the__imgui.NewFrame();

    if (g_imgui.docking) {
        g_imgui.dock_space_id = the__imgui.DockSpaceOverViewport(
            the__imgui.GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode, NULL);
    }

    imgui__imguizmo_begin();
    imgui__imguizmo_setrect(0, 0, io->DisplaySize.x, io->DisplaySize.y);

    // Update OS mouse cursor with the cursor requested by imgui
    ImGuiMouseCursor mouse_cursor =
        io->MouseDrawCursor ? ImGuiMouseCursor_None : the__imgui.GetMouseCursor();
    if (g_imgui.last_cursor != mouse_cursor) {
        g_imgui.last_cursor = mouse_cursor;
        imgui__update_cursor();
    }

    imgui__log_update();
}

static void imgui__draw(ImDrawData* draw_data)
{
    sx_assert(draw_data);
    if (draw_data->CmdListsCount == 0)
        return;

    // Fill vertex/index buffers
    int num_verts = 0;
    int num_indices = 0;
    ImDrawVert* verts = g_imgui.verts;
    uint16_t* indices = g_imgui.indices;

    for (int dlist = 0; dlist < draw_data->CmdListsCount; dlist++) {
        const ImDrawList* dl = draw_data->CmdLists[dlist];
        const int dl_num_verts = dl->VtxBuffer.Size;
        const int dl_num_indices = dl->IdxBuffer.Size;

        int max_verts = g_imgui.max_verts;
        int max_indices = g_imgui.max_indices;
        if ((num_verts + dl_num_verts) > max_verts) {
            rizz_log_warn("imgui:maximum vertex count %d exceeded, growing buffers", g_imgui.max_verts);
            max_verts <<= 1;
        }

        if ((num_indices + dl_num_indices) > max_indices) {
            rizz_log_warn("imgui:maximum index count %d exceeded, growing buffers", g_imgui.max_indices);
            max_indices <<= 1;
        }

        if (max_verts > g_imgui.max_verts || max_indices > g_imgui.max_indices) {
            bool _r = imgui__resize_buffers(max_verts, max_indices);
            sx_unused(_r);
            sx_assert_always(_r && "imgui: vertex/index buffer creation failed");
            verts = g_imgui.verts;
            indices = g_imgui.indices;
        }

        sx_memcpy(&verts[num_verts], dl->VtxBuffer.Data, dl_num_verts * sizeof(ImDrawVert));

        const ImDrawIdx* src_index_ptr = (ImDrawIdx*)dl->IdxBuffer.Data;
        sx_assert(num_verts <= UINT16_MAX);
        const uint16_t base_vertex_idx = (uint16_t)num_verts;
        for (int i = 0; i < dl_num_indices; i++)
            indices[num_indices++] = src_index_ptr[i] + base_vertex_idx;
        num_verts += dl_num_verts;
    }

    the_gfx->imm.update_buffer(g_imgui.bind.vertex_buffers[0], verts, num_verts * sizeof(ImDrawVert));
    the_gfx->imm.update_buffer(g_imgui.bind.index_buffer, indices, num_indices * sizeof(uint16_t));

    // Draw the list
    ImGuiIO* io = the__imgui.GetIO();
    sx_vec2 fb_scale = draw_data->FramebufferScale;   // = (1, 1) unless high-dpi
    sx_vec2 fb_pos = draw_data->DisplayPos;           // = (0, 0) unless multi-view
    sx_vec2 display_size = sx_vec2f(io->DisplaySize.x, io->DisplaySize.y);

    imgui__shader_uniforms uniforms = { .disp_size = display_size };
    int base_elem = 0;
    sg_image last_img = { 0 }; 
    g_imgui.bind.fs_images[0] = the_gfx->texture_white();
    the_gfx->imm.apply_pipeline(g_imgui.pip);
    the_gfx->imm.apply_bindings(&g_imgui.bind);
    the_gfx->imm.apply_uniforms(SG_SHADERSTAGE_VS, 0, &uniforms, sizeof(uniforms));

    for (int dlist = 0; dlist < draw_data->CmdListsCount; dlist++) {
        const ImDrawList* dl = draw_data->CmdLists[dlist];
        for (const ImDrawCmd* cmd = (const ImDrawCmd*)dl->CmdBuffer.Data;
             cmd != (const ImDrawCmd*)dl->CmdBuffer.Data + dl->CmdBuffer.Size; ++cmd) {
            if (!cmd->UserCallback) {
                sx_vec4 clip_rect = {{
                    (cmd->ClipRect.x - fb_pos.x) * fb_scale.x,
                    (cmd->ClipRect.y - fb_pos.y) * fb_scale.y,
                    (cmd->ClipRect.z - fb_pos.x) * fb_scale.x,
                    (cmd->ClipRect.w - fb_pos.y) * fb_scale.y
                }};

                if (clip_rect.x < display_size.x && clip_rect.y < display_size.y && 
                    clip_rect.z >= 0.0f && clip_rect.w >= 0.0f) {
                    const int scissor_x = (int)(clip_rect.x);
                    const int scissor_y = (int)(clip_rect.y);
                    const int scissor_w = (int)(clip_rect.z - clip_rect.x);
                    const int scissor_h = (int)(clip_rect.w - clip_rect.y);

                    sg_image tex = { .id = (uint32_t)(uintptr_t)cmd->TextureId };
                    if (tex.id != last_img.id) {
                        g_imgui.bind.fs_images[0] = tex;
                        the_gfx->imm.apply_bindings(&g_imgui.bind);
                    }
                    the_gfx->imm.apply_scissor_rect(scissor_x, scissor_y, scissor_w, scissor_h, true);
                    the_gfx->imm.draw(base_elem, cmd->ElemCount, 1);
                }
            } else {
                cmd->UserCallback(dl, cmd);
            }

            base_elem += cmd->ElemCount;
        }    // foreach ImDrawCmd
    }        // foreach DrawList
}

static void imgui__render(void)
{
    igRender();

    // Draw imgui primitives
    if (the_gfx->imm.begin(g_imgui.stage)) {
        the_gfx->imm.begin_default_pass(
            &(sg_pass_action){ .colors[0] = { .action = SG_ACTION_LOAD },
                               .depth = { .action = SG_ACTION_DONTCARE },
                               .stencil = { .action = SG_ACTION_DONTCARE } },
            the_app->width(), the_app->height());
        imgui__draw(the__imgui.GetDrawData());
        the_gfx->imm.end_pass();
        the_gfx->imm.end();
    }
}

static ImDrawList* imgui__begin_fullscreen_draw(const char* name)
{
    ImGuiIO* io = the__imgui.GetIO();
    const uint32_t flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs |
                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                           ImGuiWindowFlags_NoBringToFrontOnFocus;
    the__imgui.SetNextWindowSize(io->DisplaySize, 0);
    the__imgui.SetNextWindowPos(SX_VEC2_ZERO, 0, SX_VEC2_ZERO);
    the__imgui.PushStyleColor_U32(ImGuiCol_WindowBg, 0);
    the__imgui.PushStyleColor_U32(ImGuiCol_Border, 0);
    the__imgui.PushStyleVar_Float(ImGuiStyleVar_WindowRounding, 0.0f);

    the__imgui.Begin(name, NULL, flags);
    ImDrawList* dlist = the__imgui.GetWindowDrawList();
    the__imgui.End();

    the__imgui.PopStyleVar(1);
    the__imgui.PopStyleColor(2);

    return dlist;
}

static void imgui__draw_cursor(ImDrawList* drawlist, ImGuiMouseCursor cursor, sx_vec2 pos,
                               float scale)
{
    if (cursor == ImGuiMouseCursor_None)
        return;
    sx_assert(cursor > ImGuiMouseCursor_None && cursor < ImGuiMouseCursor_COUNT);

    const ImU32 col_shadow = sx_color4u(0, 0, 0, 48).n;
    const ImU32 col_border = sx_color4u(0, 0, 0, 255).n;        // Black
    const ImU32 col_fill = sx_color4u(255, 255, 255, 255).n;    // White

    ImGuiIO* conf = the__imgui.GetIO();
    sx_vec2 offset, size, uv[4];
    if (the__imgui.ImFontAtlas_GetMouseCursorTexData(conf->Fonts, cursor, &offset, &size, &uv[0],
                                                     &uv[2])) {
        pos = sx_vec2_sub(pos, offset);
        const ImTextureID tex_id = conf->Fonts->TexID;
        the__imgui.ImDrawList_PushTextureID(drawlist, tex_id);
        the__imgui.ImDrawList_AddImage(
            drawlist, tex_id, sx_vec2_add(pos, sx_vec2_mulf(sx_vec2f(1, 0), scale)),
            sx_vec2_add(sx_vec2_add(pos, sx_vec2_mulf(sx_vec2f(1, 0), scale)),
                        sx_vec2_mulf(size, scale)),
            uv[2], uv[3], col_shadow);
        the__imgui.ImDrawList_AddImage(
            drawlist, tex_id, sx_vec2_add(pos, sx_vec2_mulf(sx_vec2f(2, 0), scale)),
            sx_vec2_add(sx_vec2_add(pos, sx_vec2_mulf(sx_vec2f(2, 0), scale)),
                        sx_vec2_mulf(size, scale)),
            uv[2], uv[3], col_shadow);
        the__imgui.ImDrawList_AddImage(drawlist, tex_id, pos,
                                       sx_vec2_add(pos, sx_vec2_mulf(size, scale)), uv[2], uv[3],
                                       col_border);
        the__imgui.ImDrawList_AddImage(drawlist, tex_id, pos,
                                       sx_vec2_add(pos, sx_vec2_mulf(size, scale)), uv[0], uv[1],
                                       col_fill);
        the__imgui.ImDrawList_PopTextureID(drawlist);
    }
}

static sx_vec2 imgui__project_to_screen(const sx_vec3 pt, const sx_mat4* mvp, const sx_rect* vp)
{
    sx_vec4 trans = sx_vec4f(pt.x, pt.y, pt.z, 1.0f);

    trans = sx_mat4_mul_vec4(mvp, trans);
    trans = sx_vec4_mulf(trans, 0.5f / trans.w);
    trans.x += 0.5f;
    trans.y += 0.5f;
    trans.y = 1.0f - trans.y;

    if (!vp) {
        sx_vec2 vp_size = the__imgui.GetIO()->DisplaySize;
        trans.x *= vp_size.x;
        trans.y *= vp_size.y;
    } else {
        trans.x *= (vp->xmax - vp->xmin);
        trans.y *= (vp->ymax - vp->ymin);
        trans.x += vp->xmin;
        trans.y += vp->ymin;
    }
    return sx_vec2f(trans.x, trans.y);
}

static void imgui__labelv_all(const ImVec4* name_color, const ImVec4* text_color, float offset,
                              float spacing, const char* name, const char* fmt, va_list args)
{
    char formatted[512];
    char name_w_colon[128];
    sx_strcat(sx_strcpy(name_w_colon, sizeof(name_w_colon), name), sizeof(name_w_colon), ":");
    sx_vsnprintf(formatted, sizeof(formatted), fmt, args);

    the__imgui.TextColored(*name_color, name_w_colon);
    the__imgui.SameLine(offset, spacing);
    the__imgui.TextColored(*text_color, formatted);
}

static void imgui__label(const char* name, const char* fmt, ...)
{
    const ImVec4* text_color = the__imgui.GetStyleColorVec4(ImGuiCol_Text);
    const ImVec4* name_color = the__imgui.GetStyleColorVec4(ImGuiCol_TextDisabled);

    va_list args;
    va_start(args, fmt);
    imgui__labelv_all(name_color, text_color, 0, -1.0f, name, fmt, args);
    va_end(args);
}

static void imgui__label_colored(const ImVec4* name_color, const ImVec4* text_color,
                                 const char* name, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    imgui__labelv_all(name_color, text_color, 0, -1.0f, name, fmt, args);
    va_end(args);
}

static void imgui__label_spacing(float offset, float spacing, const char* name, const char* fmt, ...)
{
    const ImVec4* text_color = the__imgui.GetStyleColorVec4(ImGuiCol_Text);
    const ImVec4* name_color = the__imgui.GetStyleColorVec4(ImGuiCol_TextDisabled);

    va_list args;
    va_start(args, fmt);
    imgui__labelv_all(name_color, text_color, offset, spacing, name, fmt, args);
    va_end(args);
}

static void imgui__label_colored_spacing(const ImVec4* name_color, const ImVec4* text_color,
                                         float offset, float spacing, const char* name,
                                         const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    imgui__labelv_all(name_color, text_color, offset, spacing, name, fmt, args);
    va_end(args);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// memory debugger
static void imgui__dual_progress_bar(float fraction1, float fraction2, const sx_vec2 ctrl_size,
                                     const char* overlay1, const char* overlay2)
{

    ImDrawList* draw_list = the__imgui.GetWindowDrawList();

    sx_vec2 pos;
    sx_vec2 region;
    the__imgui.GetContentRegionAvail(&region);
    sx_vec2 size = sx_vec2f(ctrl_size.x < 0 ? region.x : ctrl_size.x, ctrl_size.y);

    the__imgui.GetCursorScreenPos(&pos);
    sx_vec2 _max = sx_vec2_add(pos, size);

    fraction1 = sx_min(fraction1, 1.0f);
    fraction2 = sx_min(fraction2, 1.0f);

    float h = _max.y - pos.y;
    bool fracs_equal = sx_equal(fraction1, fraction2, 0.001f);

    sx_vec2 overlay1_size, overlay2_size;
    sx_vec2 overlay1_pos, overlay2_pos;
    const sx_vec4* text_color_f = the__imgui.GetStyleColorVec4(ImGuiCol_Text);
    sx_color text_color =
        sx_color4f(text_color_f->x, text_color_f->y, text_color_f->z, text_color_f->w);
    const sx_vec4* prog_color_f = the__imgui.GetStyleColorVec4(ImGuiCol_PlotHistogram);
    sx_color prog_color =
        sx_color4f(prog_color_f->x, prog_color_f->y, prog_color_f->z, prog_color_f->w);
    sx_color prog2_color = sx_color4f(prog_color_f->x * 0.3f, prog_color_f->y * 0.3f,
                                      prog_color_f->z * 0.3f, prog_color_f->w);

    // the__imgui.PushClipRect(pos, _max, false);

    // background rect
    const sx_vec4* bg_color_f = the__imgui.GetStyleColorVec4(ImGuiCol_FrameBg);
    sx_color bg_color = sx_color4f(bg_color_f->x, bg_color_f->y, bg_color_f->z, bg_color_f->w);
    the__imgui.ImDrawList_AddRectFilled(draw_list, pos, _max, bg_color.n, 0.0f, 0);

    ImGuiStyle* style = the__imgui.GetStyle();
    pos.x += style->FrameBorderSize;
    pos.y += style->FrameBorderSize;
    _max.x -= style->FrameBorderSize;
    _max.y -= style->FrameBorderSize;

    // fraction2 rect
    sx_vec2 end2 = sx_vec2f(sx_lerp(pos.x, _max.x, fraction2), _max.y);

    the__imgui.CalcTextSize(&overlay2_size, overlay2, NULL, false, 0);
    float text2_y = pos.y + (h - overlay2_size.y) * 0.5f;
    if (end2.x + style->ItemSpacing.x + overlay2_size.x < _max.x)
        overlay2_pos = sx_vec2f(end2.x + style->ItemSpacing.x, text2_y);
    else
        overlay2_pos = sx_vec2f(end2.x - style->ItemSpacing.x - overlay2_size.x, text2_y);

    sx_vec2 end1 = sx_vec2f(sx_lerp(pos.x, _max.x, fraction1), _max.y);
    the__imgui.CalcTextSize(&overlay1_size, overlay1, NULL, false, 0);
    float text1_y = pos.y + (h - overlay1_size.y) * 0.5f;
    if (end1.x + style->ItemSpacing.x + overlay1_size.x < _max.x)
        overlay1_pos = sx_vec2f(end1.x + style->ItemSpacing.x, text1_y);
    else
        overlay1_pos = sx_vec2f(end1.x - style->ItemSpacing.x - overlay1_size.x, text1_y);

    sx_rect overlay1_rect =
        sx_rectwh(overlay1_pos.x, overlay1_pos.y, overlay1_size.x, overlay1_size.y);
    sx_rect overlay2_rect =
        sx_rectwh(overlay2_pos.x, overlay2_pos.y, overlay2_size.x, overlay2_size.y);
    if (!fracs_equal && sx_rect_test_rect(overlay1_rect, overlay2_rect)) {
        int text_size = sx_strlen(overlay1) + sx_strlen(overlay2) + 4;
        char* text = alloca(text_size);
        sx_snprintf(text, text_size, "%s (%s)", overlay1, overlay2);
        overlay1 = text;
        overlay2 = NULL;

        // recalculate
        the__imgui.CalcTextSize(&overlay1_size, overlay1, NULL, false, 0);
        if (end1.x + style->ItemSpacing.x + overlay1_size.x < _max.x)
            overlay1_pos = sx_vec2f(end1.x + style->ItemSpacing.x, text1_y);
        else
            overlay1_pos = sx_vec2f(end1.x - style->ItemSpacing.x - overlay1_size.x, text1_y);
    }

    if (!fracs_equal) {
        the__imgui.ImDrawList_AddRectFilled(draw_list, pos, end2, prog2_color.n, 0, 0);

        // Text2
        if (overlay2)
            the__imgui.ImDrawList_AddText_Vec2(draw_list, overlay2_pos, text_color.n, overlay2, NULL);
    }

    // fraction1 rect
    the__imgui.ImDrawList_AddRectFilled(draw_list, pos, end1, prog_color.n, 0, 0);

    // Text1
    the__imgui.ImDrawList_AddText_Vec2(draw_list, overlay1_pos, text_color.n, overlay1, NULL);

    // the__imgui.PopClipRect();
    the__imgui.NewLine();
}

static void imgui__graphics_debugger(const rizz_gfx_trace_info* info, bool* p_open)
{
    sx_assert(info);
    the__imgui.SetNextWindowSizeConstraints(sx_vec2f(500.0f, 350.0f), sx_vec2f(FLT_MAX, FLT_MAX),
                                            NULL, NULL);

    float ft = the_core->delta_time()*1000.0f;

    g_imgui.fts[g_imgui.ft_iter] = ft;
    g_imgui.ft_iter = (g_imgui.ft_iter + 1) % MAX_BUFFERED_FRAME_TIMES;
    ++g_imgui.ft_iter_nomod;
    float fts[MAX_BUFFERED_FRAME_TIMES];
    int ft_count;
    if (g_imgui.ft_iter_nomod > g_imgui.ft_iter) {
        sx_memcpy(fts, &g_imgui.fts[g_imgui.ft_iter], sizeof(float)*(MAX_BUFFERED_FRAME_TIMES - g_imgui.ft_iter));
        sx_memcpy(&fts[MAX_BUFFERED_FRAME_TIMES - g_imgui.ft_iter], g_imgui.fts, g_imgui.ft_iter*sizeof(float));
        ft_count = MAX_BUFFERED_FRAME_TIMES;
    } else {
        sx_memcpy(fts, g_imgui.fts, g_imgui.ft_iter*sizeof(float));
        ft_count = g_imgui.ft_iter;
    }

    if (the__imgui.Begin("Graphics Debugger", p_open, 0)) {
        if (the__imgui.BeginTabBar("#gfx_debugger_tabs", 0)) {
            if (the__imgui.BeginTabItem("General", NULL, 0)) {
                const rizz_gfx_perframe_trace_info* pf = &info->pf[RIZZ_GFX_TRACE_COMMON];

                const float text_offset = 120.0f;
                the__imgui.PushItemWidth(-1);
                the__imgui.PlotHistogram_FloatPtr("##Ft_plot", fts, ft_count, 0, NULL, 0, 32.0f, 
                                                  sx_vec2f(0, 50.0f), sizeof(float));
                the__imgui.PopItemWidth();

                if (the__imgui.BeginTable("PerFrame", 3, ImGuiTableFlags_SizingStretchSame, SX_VEC2_ZERO, 0)) {
                    the__imgui.TableNextColumn();
                    imgui__label_spacing(text_offset, -1.0f, "Fps", "%.1f", the_core->fps());
                    imgui__label_spacing(text_offset, -1.0f, "Fps (mean)", "%.1f", the_core->fps_mean());
                    imgui__label_spacing(text_offset, -1.0f, "FrameTime (ms)", "%.1f", ft);
                    the__imgui.TableNextColumn();
                    imgui__label_spacing(text_offset, -1.0f, "Draws", "%d", pf->num_draws);
                    imgui__label_spacing(text_offset, -1.0f, "Instances", "%d", pf->num_instances);
                    imgui__label_spacing(text_offset, -1.0f, "Elements", "%d", pf->num_elements);
                    the__imgui.TableNextColumn();
                    imgui__label_spacing(text_offset, -1.0f, "Active Pipelines", "%d", pf->num_apply_pipelines);
                    imgui__label_spacing(text_offset, -1.0f, "Active Passes", "%d", pf->num_apply_passes);
                    the__imgui.EndTable();
                }
                the__imgui.Separator();
                
                imgui__label_spacing(text_offset, -1.0f, "Pipelines", "%d", info->num_pipelines);
                imgui__label_spacing(text_offset, -1.0f, "Shaders", "%d", info->num_shaders);
                imgui__label_spacing(text_offset, -1.0f, "Passes", "%d", info->num_passes);
                imgui__label_spacing(text_offset, -1.0f, "Images", "%d", info->num_images);
                imgui__label_spacing(text_offset, -1.0f, "Buffers", "%d", info->num_buffers);
                the__imgui.Separator();

                char size_text[32];
                char peak_text[32];
                sx_snprintf(size_text, sizeof(size_text), "%$.2d", info->texture_size);
                sx_snprintf(peak_text, sizeof(peak_text), "%$.2d", info->texture_peak);
                the__imgui.Text("Textures");
                the__imgui.SameLine(100.0f, -1);
                imgui__dual_progress_bar(
                    (float)((double)info->texture_size / (double)info->texture_peak), 1.0f,
                    sx_vec2f(-1.0f, 14.0f), size_text, peak_text);

                sx_snprintf(size_text, sizeof(size_text), "%$.2d", info->render_target_size);
                sx_snprintf(peak_text, sizeof(peak_text), "%$.2d", info->render_target_peak);
                the__imgui.Text("RTs");
                the__imgui.SameLine(100.0f, -1);
                imgui__dual_progress_bar(
                    (float)((double)info->render_target_size / (double)info->render_target_peak),
                    1.0f, sx_vec2f(-1.0f, 14.0f), size_text, peak_text);

                sx_snprintf(size_text, sizeof(size_text), "%$.2d", info->buffer_size);
                sx_snprintf(peak_text, sizeof(peak_text), "%$.2d", info->buffer_peak);
                the__imgui.Text("Buffers");
                the__imgui.SameLine(100.0f, -1);
                imgui__dual_progress_bar(
                    (float)((double)info->buffer_size / (double)info->buffer_peak), 1.0f,
                    sx_vec2f(-1.0f, 14.0f), size_text, peak_text);

                the__imgui.EndTabItem();
            }

            if (the__imgui.BeginTabItem("Capture", NULL, 0)) {
                sg_imgui_draw_capture_content(&g_imgui.sg_imgui);
                the__imgui.EndTabItem();
            }
            if (the__imgui.BeginTabItem("Shaders", NULL, 0)) {
                sg_imgui_draw_shaders_content(&g_imgui.sg_imgui);
                the__imgui.EndTabItem();
            }
            if (the__imgui.BeginTabItem("Pipelines", NULL, 0)) {
                sg_imgui_draw_pipelines_content(&g_imgui.sg_imgui);
                the__imgui.EndTabItem();
            }
            if (the__imgui.BeginTabItem("Passes", NULL, 0)) {
                sg_imgui_draw_passes_content(&g_imgui.sg_imgui);
                the__imgui.EndTabItem();
            }
            if (the__imgui.BeginTabItem("Images", NULL, 0)) {
                sg_imgui_draw_images_content(&g_imgui.sg_imgui);
                the__imgui.EndTabItem();
            }
            if (the__imgui.BeginTabItem("Buffers", NULL, 0)) {
                sg_imgui_draw_buffers_content(&g_imgui.sg_imgui);
                the__imgui.EndTabItem();
            }
            if (the__imgui.BeginTabItem("Caps", NULL, 0)) {
                sg_imgui_draw_capabilities_content(&g_imgui.sg_imgui);
                the__imgui.EndTabItem();
            }

            if (the__imgui.BeginTabItem("ImGui", NULL, 0)) {
                const rizz_gfx_perframe_trace_info* pf = &info->pf[RIZZ_GFX_TRACE_IMGUI];
                imgui__label_spacing(100.0f, 0, "Draws", "%d", pf->num_draws);
                imgui__label_spacing(100.0f, 0, "Instances", "%d", pf->num_instances);
                imgui__label_spacing(100.0f, 0, "Elements", "%d", pf->num_elements);

                static bool show_metrics = false;
                the__imgui.Checkbox("Show Metrics", &show_metrics);
                if (show_metrics) {
                    the__imgui.ShowMetricsWindow(&show_metrics);
                }
                the__imgui.EndTabItem();
            }

            the__imgui.EndTabBar();
        }
    }
    the__imgui.End();
}

static bool imgui__is_capturing_mouse(void) 
{
    return igGetIO()->WantCaptureMouse;
}

static bool imgui__is_capturing_keyboard(void)
{
    return igGetIO()->WantCaptureKeyboard;
}

static ImGuiID imgui__dock_space_id(void)
{
    return g_imgui.dock_space_id;
}

static rizz_api_imgui_extra the__imgui_extra = {
    .graphics_debugger = imgui__graphics_debugger,
    .show_log = imgui__show_log,
    .dual_progress_bar = imgui__dual_progress_bar,
    .begin_fullscreen_draw = imgui__begin_fullscreen_draw,
    .draw_cursor = imgui__draw_cursor,
    .project_to_screen = imgui__project_to_screen,
    .is_capturing_mouse = imgui__is_capturing_mouse,
    .is_capturing_keyboard = imgui__is_capturing_keyboard,
    .dock_space_id = imgui__dock_space_id,
    .label = imgui__label,
    .label_colored = imgui__label_colored,
    .label_spacing = imgui__label_spacing,
    .label_colored_spacing = imgui__label_colored_spacing
};

rizz_plugin_decl_event_handler(imgui, e)
{
    ImGuiIO* io = the__imgui.GetIO();
    switch (e->type) {
    case RIZZ_APP_EVENTTYPE_MOUSE_DOWN: {
        sx_vec2 scale = io->DisplayFramebufferScale;
        io->MousePos = sx_vec2f(e->mouse_x / scale.x, e->mouse_y / scale.y);
        g_imgui.mouse_btn_down[e->mouse_button] = true;
        break;
    }
    case RIZZ_APP_EVENTTYPE_MOUSE_UP: {
        sx_vec2 scale = io->DisplayFramebufferScale;
        io->MousePos = sx_vec2f(e->mouse_x / scale.x, e->mouse_y / scale.y);
        g_imgui.mouse_btn_up[e->mouse_button] = true;
        break;
    }
    case RIZZ_APP_EVENTTYPE_MOUSE_MOVE: {
        sx_vec2 scale = io->DisplayFramebufferScale;
        io->MousePos = sx_vec2f(e->mouse_x / scale.x, e->mouse_y / scale.y);
        break;
    }
    case RIZZ_APP_EVENTTYPE_MOUSE_ENTER:
    case RIZZ_APP_EVENTTYPE_MOUSE_LEAVE:
        for (int i = 0; i < 3; i++) {
            g_imgui.mouse_btn_down[i] = false;
            g_imgui.mouse_btn_up[i] = false;
            io->MouseDown[i] = false;
        }
        break;
    case RIZZ_APP_EVENTTYPE_MOUSE_SCROLL:
        g_imgui.mouse_wheel_h = e->scroll_x;
        g_imgui.mouse_wheel += e->scroll_y;
        break;
    case RIZZ_APP_EVENTTYPE_KEY_DOWN:
        g_imgui.keys_down[e->key_code] = true;
        break;
    case RIZZ_APP_EVENTTYPE_KEY_UP:
        g_imgui.keys_down[e->key_code] = false;
        break;
    case RIZZ_APP_EVENTTYPE_CHAR:
        sx_array_push(the_core->alloc(RIZZ_MEMID_TOOLSET), g_imgui.char_input,
                      (ImWchar)e->char_code);
        break;
    case RIZZ_APP_EVENTTYPE_UPDATE_CURSOR:
        imgui__update_cursor();
        break;
    case RIZZ_APP_EVENTTYPE_RESIZED:
        the_app->window_size(&io->DisplaySize);
        imgui__imguizmo_setrect(0, 0, io->DisplaySize.x, io->DisplaySize.y);
        break;
    default:
        break;
    }
}

typedef enum {
    GFX_COMMAND_MAKE_BUFFER = 0,
    GFX_COMMAND_MAKE_IMAGE,
    GFX_COMMAND_MAKE_SHADER,
    GFX_COMMAND_MAKE_PIPELINE,
    GFX_COMMAND_MAKE_PASS,
    _GFX_COMMAND_MAKE_COUNT,
    _GFX_COMMAND_MAKE_ = INT32_MAX
} rizz__gfx_command_make;


static void imgui__submit_make_commands(const void* cmdbuff, int cmdbuff_sz)
{
    sx_mem_reader r;
    sx_mem_init_reader(&r, cmdbuff, cmdbuff_sz);

    while (r.top != r.pos) {
        int32_t _cmd;
        sx_mem_read_var(&r, _cmd);
        switch ((rizz__gfx_command_make)_cmd) {
        case GFX_COMMAND_MAKE_BUFFER: {
            sg_buffer buf;
            sg_buffer_desc desc;
            sx_mem_read_var(&r, buf);
            sx_mem_read_var(&r, desc);
            _sg_imgui_make_buffer(&desc, buf, &g_imgui.sg_imgui);
        } break;
        case GFX_COMMAND_MAKE_IMAGE: {
            sg_image img;
            sg_image_desc desc;
            sx_mem_read_var(&r, img);
            sx_mem_read_var(&r, desc);
            _sg_imgui_make_image(&desc, img, &g_imgui.sg_imgui);
        } break;
        case GFX_COMMAND_MAKE_SHADER: {
            sg_shader shd;
            sg_shader_desc desc;
            sx_mem_read_var(&r, shd);
            sx_mem_read_var(&r, desc);
            _sg_imgui_make_shader(&desc, shd, &g_imgui.sg_imgui);
        } break;
        case GFX_COMMAND_MAKE_PIPELINE: {
            sg_pipeline pip;
            sg_pipeline_desc desc;
            sx_mem_read_var(&r, pip);
            sx_mem_read_var(&r, desc);
            _sg_imgui_make_pipeline(&desc, pip, &g_imgui.sg_imgui);
        } break;
        case GFX_COMMAND_MAKE_PASS: {
            sg_pass pass;
            sg_pass_desc desc;
            sx_mem_read_var(&r, pass);
            sx_mem_read_var(&r, desc);
            _sg_imgui_make_pass(&desc, pass, &g_imgui.sg_imgui);
        } break;
        default:
            sx_assert(0);
        }
    }
}

rizz_plugin_decl_main(imgui, plugin, e)
{
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP: {
        imgui__frame();
        break;
    }

    case RIZZ_PLUGIN_EVENT_INIT: {
        the_plugin = plugin->api;
        the_core = (rizz_api_core*)the_plugin->get_api(RIZZ_API_CORE, 0);
        the_gfx = (rizz_api_gfx*)the_plugin->get_api(RIZZ_API_GFX, 0);
        the_app = (rizz_api_app*)the_plugin->get_api(RIZZ_API_APP, 0);

        if (!imgui__init()) {
            return -1;
        }

        sx_assert(the_plugin);
        imgui__get_imguizmo_api(&the__imgui_extra.gizmo);
        the_plugin->inject_api("imgui", 0, &the__imgui);
        the_plugin->inject_api("imgui_extra", 0, &the__imgui_extra);

        void* make_cmdbuff;
        int make_cmdbuff_sz;
        the_gfx->internal_state(&make_cmdbuff, &make_cmdbuff_sz);

        sg_imgui_init(&g_imgui.sg_imgui, the_gfx);

        // submit all make commands to the imgui
        if (make_cmdbuff_sz) {
            imgui__submit_make_commands(make_cmdbuff, make_cmdbuff_sz);
        }

        {
            const char* log_buff_size_str = the_app->config_meta_value("imgui", "log_buffer_size");
            int log_buff_size = log_buff_size_str ? sx_toint(log_buff_size_str) : 64;
            imgui__log_init(the_core, the_app, g_sg_imgui_alloc, log_buff_size*1024);
        }
        break;
    }

    case RIZZ_PLUGIN_EVENT_LOAD: {
        the__imgui.SetCurrentContext(g_imgui.ctx);
        the__imgui.SetAllocatorFunctions(imgui__malloc, imgui__free, (void*)g_sg_imgui_alloc);
        imgui__get_imguizmo_api(&the__imgui_extra.gizmo);
        the_plugin->inject_api("imgui", 0, &the__imgui);
        the_plugin->inject_api("imgui_extra", 0, &the__imgui_extra);
        break;
    }

    case RIZZ_PLUGIN_EVENT_UNLOAD:
        break;

    case RIZZ_PLUGIN_EVENT_SHUTDOWN:
        sx_assert(the_plugin);
        imgui__log_release();
        sg_imgui_discard(&g_imgui.sg_imgui);
        imgui__release();
        the_plugin->remove_api("imgui", 0);
        the_plugin->remove_api("imgui_extra", 0);
        break;
    }

    return 0;
}

rizz_plugin_implement_info(imgui, 1790, "dear-imgui plugin", NULL, 0);
