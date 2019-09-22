//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//

#include "rizz/imgui.h"
#include "rizz/imgui-extra.h"

#include "imguizmo/ImGuizmo.h"

#include "rizz/app.h"
#include "rizz/core.h"
#include "rizz/graphics.h"
#include "rizz/plugin.h"

#include "sx/allocator.h"
#include "sx/array.h"
#include "sx/io.h"
#include "sx/math.h"
#include "sx/string.h"
#include "sx/timer.h"

#include <alloca.h>
#include <float.h>

#include "font-roboto.h"

SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4190)
#define RIZZ__IMGUI
#include "cimgui/cimgui.h"
SX_PRAGMA_DIAGNOSTIC_POP()

#if SX_PLATFORM_WINDOWS
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#endif

//
#define IMGUI_VERSION "1.69"
#define MAX_VERTS 32768      // 32k
#define MAX_INDICES 98304    // 96k

typedef struct rizz_api_gfx rizz_api_gfx;
static void imgui__render(void);

RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_plugin* the_plugin;
RIZZ_STATE static rizz_api_gfx* the_gfx;
RIZZ_STATE static rizz_api_app* the_app;

////////////////////////////////////////////////////////////////////////////////////////////////////
// API
static rizz_api_imgui the__imgui = {
    .CreateContext = igCreateContext,
    .DestroyContext = igDestroyContext,
    .GetCurrentContext = igGetCurrentContext,
    .SetCurrentContext = igSetCurrentContext,
    .DebugCheckVersionAndDataLayout = igDebugCheckVersionAndDataLayout,
    .GetIO = igGetIO,
    .GetStyle = igGetStyle,
    .NewFrame = igNewFrame,
    .EndFrame = igEndFrame,
    .Render = imgui__render,
    .GetDrawData = igGetDrawData,
    .ShowDemoWindow = igShowDemoWindow,
    .ShowAboutWindow = igShowAboutWindow,
    .ShowMetricsWindow = igShowMetricsWindow,
    .ShowStyleEditor = igShowStyleEditor,
    .ShowStyleSelector = igShowStyleSelector,
    .ShowFontSelector = igShowFontSelector,
    .ShowUserGuide = igShowUserGuide,
    .GetVersion = igGetVersion,
    .StyleColorsDark = igStyleColorsDark,
    .StyleColorsClassic = igStyleColorsClassic,
    .StyleColorsLight = igStyleColorsLight,
    .Begin = igBegin,
    .End = igEnd,
    .BeginChild = igBeginChild,
    .BeginChildID = igBeginChildID,
    .EndChild = igEndChild,
    .IsWindowAppearing = igIsWindowAppearing,
    .IsWindowCollapsed = igIsWindowCollapsed,
    .IsWindowFocused = igIsWindowFocused,
    .IsWindowHovered = igIsWindowHovered,
    .GetWindowDrawList = igGetWindowDrawList,
    .GetWindowWidth = igGetWindowWidth,
    .GetWindowHeight = igGetWindowHeight,
    .GetContentRegionAvailWidth = igGetContentRegionAvailWidth,
    .GetWindowContentRegionWidth = igGetWindowContentRegionWidth,
    .SetNextWindowPos = igSetNextWindowPos,
    .SetNextWindowSize = igSetNextWindowSize,
    .SetNextWindowSizeConstraints = igSetNextWindowSizeConstraints,
    .SetNextWindowContentSize = igSetNextWindowContentSize,
    .SetNextWindowCollapsed = igSetNextWindowCollapsed,
    .SetNextWindowFocus = igSetNextWindowFocus,
    .SetNextWindowBgAlpha = igSetNextWindowBgAlpha,
    .SetWindowPosVec2 = igSetWindowPosVec2,
    .SetWindowSizeVec2 = igSetWindowSizeVec2,
    .SetWindowCollapsedBool = igSetWindowCollapsedBool,
    .SetWindowFocus = igSetWindowFocus,
    .SetWindowFontScale = igSetWindowFontScale,
    .SetWindowPosStr = igSetWindowPosStr,
    .SetWindowSizeStr = igSetWindowSizeStr,
    .SetWindowCollapsedStr = igSetWindowCollapsedStr,
    .SetWindowFocusStr = igSetWindowFocusStr,
    .GetScrollX = igGetScrollX,
    .GetScrollY = igGetScrollY,
    .GetScrollMaxX = igGetScrollMaxX,
    .GetScrollMaxY = igGetScrollMaxY,
    .SetScrollX = igSetScrollX,
    .SetScrollY = igSetScrollY,
    .SetScrollHereY = igSetScrollHereY,
    .SetScrollFromPosY = igSetScrollFromPosY,
    .PushFont = igPushFont,
    .PopFont = igPopFont,
    .PushStyleColorU32 = igPushStyleColorU32,
    .PushStyleColor = igPushStyleColor,
    .PopStyleColor = igPopStyleColor,
    .PushStyleVarFloat = igPushStyleVarFloat,
    .PushStyleVarVec2 = igPushStyleVarVec2,
    .PopStyleVar = igPopStyleVar,
    .GetStyleColorVec4 = igGetStyleColorVec4,
    .GetFont = igGetFont,
    .GetFontSize = igGetFontSize,
    .GetColorU32 = igGetColorU32,
    .GetColorU32Vec4 = igGetColorU32Vec4,
    .GetColorU32U32 = igGetColorU32U32,
    .PushItemWidth = igPushItemWidth,
    .PopItemWidth = igPopItemWidth,
    .CalcItemWidth = igCalcItemWidth,
    .PushTextWrapPos = igPushTextWrapPos,
    .PopTextWrapPos = igPopTextWrapPos,
    .PushAllowKeyboardFocus = igPushAllowKeyboardFocus,
    .PopAllowKeyboardFocus = igPopAllowKeyboardFocus,
    .PushButtonRepeat = igPushButtonRepeat,
    .PopButtonRepeat = igPopButtonRepeat,
    .Separator = igSeparator,
    .SameLine = igSameLine,
    .NewLine = igNewLine,
    .Spacing = igSpacing,
    .Dummy = igDummy,
    .Indent = igIndent,
    .Unindent = igUnindent,
    .BeginGroup = igBeginGroup,
    .EndGroup = igEndGroup,
    .GetCursorPosX = igGetCursorPosX,
    .GetCursorPosY = igGetCursorPosY,
    .SetCursorPos = igSetCursorPos,
    .SetCursorPosX = igSetCursorPosX,
    .SetCursorPosY = igSetCursorPosY,
    .SetCursorScreenPos = igSetCursorScreenPos,
    .AlignTextToFramePadding = igAlignTextToFramePadding,
    .GetTextLineHeight = igGetTextLineHeight,
    .GetTextLineHeightWithSpacing = igGetTextLineHeightWithSpacing,
    .GetFrameHeight = igGetFrameHeight,
    .GetFrameHeightWithSpacing = igGetFrameHeightWithSpacing,
    .PushIDStr = igPushIDStr,
    .PushIDRange = igPushIDRange,
    .PushIDPtr = igPushIDPtr,
    .PushIDInt = igPushIDInt,
    .PopID = igPopID,
    .GetIDStr = igGetIDStr,
    .GetIDRange = igGetIDRange,
    .GetIDPtr = igGetIDPtr,
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
    .CheckboxFlags = igCheckboxFlags,
    .RadioButtonBool = igRadioButtonBool,
    .RadioButtonIntPtr = igRadioButtonIntPtr,
    .ProgressBar = igProgressBar,
    .Bullet = igBullet,
    .BeginCombo = igBeginCombo,
    .EndCombo = igEndCombo,
    .Combo = igCombo,
    .ComboStr = igComboStr,
    .ComboFnPtr = igComboFnPtr,
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
    .TreeNodeStr = igTreeNodeStr,
    .TreeNodeStrStr = igTreeNodeStrStr,
    .TreeNodePtr = igTreeNodePtr,
    .TreeNodeVStr = igTreeNodeVStr,
    .TreeNodeVPtr = igTreeNodeVPtr,
    .TreeNodeExStr = igTreeNodeExStr,
    .TreeNodeExStrStr = igTreeNodeExStrStr,
    .TreeNodeExPtr = igTreeNodeExPtr,
    .TreeNodeExVStr = igTreeNodeExVStr,
    .TreeNodeExVPtr = igTreeNodeExVPtr,
    .TreePushStr = igTreePushStr,
    .TreePushPtr = igTreePushPtr,
    .TreePop = igTreePop,
    .TreeAdvanceToLabelPos = igTreeAdvanceToLabelPos,
    .GetTreeNodeToLabelSpacing = igGetTreeNodeToLabelSpacing,
    .SetNextTreeNodeOpen = igSetNextTreeNodeOpen,
    .CollapsingHeader = igCollapsingHeader,
    .CollapsingHeaderBoolPtr = igCollapsingHeaderBoolPtr,
    .Selectable = igSelectable,
    .SelectableBoolPtr = igSelectableBoolPtr,
    .ListBoxStr_arr = igListBoxStr_arr,
    .ListBoxFnPtr = igListBoxFnPtr,
    .ListBoxHeaderVec2 = igListBoxHeaderVec2,
    .ListBoxHeaderInt = igListBoxHeaderInt,
    .ListBoxFooter = igListBoxFooter,
    .PlotLines = igPlotLines,
    .PlotLinesFnPtr = igPlotLinesFnPtr,
    .PlotHistogramFloatPtr = igPlotHistogramFloatPtr,
    .PlotHistogramFnPtr = igPlotHistogramFnPtr,
    .ValueBool = igValueBool,
    .ValueInt = igValueInt,
    .ValueUint = igValueUint,
    .ValueFloat = igValueFloat,
    .BeginMainMenuBar = igBeginMainMenuBar,
    .EndMainMenuBar = igEndMainMenuBar,
    .BeginMenuBar = igBeginMenuBar,
    .EndMenuBar = igEndMenuBar,
    .BeginMenu = igBeginMenu,
    .EndMenu = igEndMenu,
    .MenuItemBool = igMenuItemBool,
    .MenuItemBoolPtr = igMenuItemBoolPtr,
    .BeginTooltip = igBeginTooltip,
    .EndTooltip = igEndTooltip,
    .SetTooltip = igSetTooltip,
    .SetTooltipV = igSetTooltipV,
    .OpenPopup = igOpenPopup,
    .BeginPopup = igBeginPopup,
    .BeginPopupContextItem = igBeginPopupContextItem,
    .BeginPopupContextWindow = igBeginPopupContextWindow,
    .BeginPopupContextVoid = igBeginPopupContextVoid,
    .BeginPopupModal = igBeginPopupModal,
    .EndPopup = igEndPopup,
    .OpenPopupOnItemClick = igOpenPopupOnItemClick,
    .IsPopupOpen = igIsPopupOpen,
    .CloseCurrentPopup = igCloseCurrentPopup,
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
    .SetTabItemClosed = igSetTabItemClosed,
    .LogToTTY = igLogToTTY,
    .LogToFile = igLogToFile,
    .LogToClipboard = igLogToClipboard,
    .LogFinish = igLogFinish,
    .LogButtons = igLogButtons,
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
    .IsAnyItemHovered = igIsAnyItemHovered,
    .IsAnyItemActive = igIsAnyItemActive,
    .IsAnyItemFocused = igIsAnyItemFocused,
    .SetItemAllowOverlap = igSetItemAllowOverlap,
    .IsRectVisible = igIsRectVisible,
    .IsRectVisibleVec2 = igIsRectVisibleVec2,
    .GetTime = igGetTime,
    .GetFrameCount = igGetFrameCount,
    .GetBackgroundDrawList = igGetBackgroundDrawList,
    .GetForegroundDrawList = igGetForegroundDrawList,
    .GetDrawListSharedData = igGetDrawListSharedData,
    .GetStyleColorName = igGetStyleColorName,
    .SetStateStorage = igSetStateStorage,
    .GetStateStorage = igGetStateStorage,
    .CalcListClipping = igCalcListClipping,
    .BeginChildFrame = igBeginChildFrame,
    .EndChildFrame = igEndChildFrame,
    .ColorConvertFloat4ToU32 = igColorConvertFloat4ToU32,
    .GetKeyIndex = igGetKeyIndex,
    .IsKeyDown = igIsKeyDown,
    .IsKeyPressed = igIsKeyPressed,
    .IsKeyReleased = igIsKeyReleased,
    .GetKeyPressedAmount = igGetKeyPressedAmount,
    .IsMouseDown = igIsMouseDown,
    .IsAnyMouseDown = igIsAnyMouseDown,
    .IsMouseClicked = igIsMouseClicked,
    .IsMouseDoubleClicked = igIsMouseDoubleClicked,
    .IsMouseReleased = igIsMouseReleased,
    .IsMouseDragging = igIsMouseDragging,
    .IsMouseHoveringRect = igIsMouseHoveringRect,
    .IsMousePosValid = igIsMousePosValid,
    .ResetMouseDragDelta = igResetMouseDragDelta,
    .GetMouseCursor = igGetMouseCursor,
    .SetMouseCursor = igSetMouseCursor,
    .CaptureKeyboardFromApp = igCaptureKeyboardFromApp,
    .CaptureMouseFromApp = igCaptureMouseFromApp,
    .GetClipboardText = igGetClipboardText,
    .SetClipboardText = igSetClipboardText,
    .LoadIniSettingsFromDisk = igLoadIniSettingsFromDisk,
    .LoadIniSettingsFromMemory = igLoadIniSettingsFromMemory,
    .SaveIniSettingsToDisk = igSaveIniSettingsToDisk,
    .SaveIniSettingsToMemory = igSaveIniSettingsToMemory,
    .SetAllocatorFunctions = igSetAllocatorFunctions,
    .MemAlloc = igMemAlloc,
    .MemFree = igMemFree,
    .GetWindowPos_nonUDT = igGetWindowPos_nonUDT,
    .GetWindowSize_nonUDT = igGetWindowSize_nonUDT,
    .GetContentRegionMax_nonUDT = igGetContentRegionMax_nonUDT,
    .GetContentRegionAvail_nonUDT = igGetContentRegionAvail_nonUDT,
    .GetWindowContentRegionMin_nonUDT = igGetWindowContentRegionMin_nonUDT,
    .GetWindowContentRegionMax_nonUDT = igGetWindowContentRegionMax_nonUDT,
    .GetFontTexUvWhitePixel_nonUDT = igGetFontTexUvWhitePixel_nonUDT,
    .GetCursorPos_nonUDT = igGetCursorPos_nonUDT,
    .GetCursorStartPos_nonUDT = igGetCursorStartPos_nonUDT,
    .GetCursorScreenPos_nonUDT = igGetCursorScreenPos_nonUDT,
    .GetItemRectMin_nonUDT = igGetItemRectMin_nonUDT,
    .GetItemRectMax_nonUDT = igGetItemRectMax_nonUDT,
    .GetItemRectSize_nonUDT = igGetItemRectSize_nonUDT,
    .CalcTextSize_nonUDT = igCalcTextSize_nonUDT,
    .ColorConvertU32ToFloat4_nonUDT = igColorConvertU32ToFloat4_nonUDT,
    .GetMousePos_nonUDT = igGetMousePos_nonUDT,
    .GetMousePosOnOpeningCurrentPopup_nonUDT = igGetMousePosOnOpeningCurrentPopup_nonUDT,
    .GetMouseDragDelta_nonUDT = igGetMouseDragDelta_nonUDT,
    .LogText = igLogText,
    .GET_FLT_MAX = igGET_FLT_MAX,
    .ColorConvertRGBtoHSV = igColorConvertRGBtoHSV,
    .ColorConvertHSVtoRGB = igColorConvertHSVtoRGB,
    .ImGuiIO_AddInputCharacter = ImGuiIO_AddInputCharacter,
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
    .ImDrawList_AddText = ImDrawList_AddText,
    .ImDrawList_AddTextFontPtr = ImDrawList_AddTextFontPtr,
    .ImDrawList_AddImage = ImDrawList_AddImage,
    .ImDrawList_AddImageQuad = ImDrawList_AddImageQuad,
    .ImDrawList_AddImageRounded = ImDrawList_AddImageRounded,
    .ImDrawList_AddPolyline = ImDrawList_AddPolyline,
    .ImDrawList_AddConvexPolyFilled = ImDrawList_AddConvexPolyFilled,
    .ImDrawList_AddBezierCurve = ImDrawList_AddBezierCurve,
    .ImDrawList_PathClear = ImDrawList_PathClear,
    .ImDrawList_PathLineTo = ImDrawList_PathLineTo,
    .ImDrawList_PathLineToMergeDuplicate = ImDrawList_PathLineToMergeDuplicate,
    .ImDrawList_PathFillConvex = ImDrawList_PathFillConvex,
    .ImDrawList_PathStroke = ImDrawList_PathStroke,
    .ImDrawList_PathArcTo = ImDrawList_PathArcTo,
    .ImDrawList_PathArcToFast = ImDrawList_PathArcToFast,
    .ImDrawList_PathBezierCurveTo = ImDrawList_PathBezierCurveTo,
    .ImDrawList_PathRect = ImDrawList_PathRect,
    .ImDrawList_ChannelsSplit = ImDrawList_ChannelsSplit,
    .ImDrawList_ChannelsMerge = ImDrawList_ChannelsMerge,
    .ImDrawList_ChannelsSetCurrent = ImDrawList_ChannelsSetCurrent,
    .ImDrawList_AddCallback = ImDrawList_AddCallback,
    .ImDrawList_AddDrawCmd = ImDrawList_AddDrawCmd,
    .ImDrawList_CloneOutput = ImDrawList_CloneOutput,
    .ImDrawList_Clear = ImDrawList_Clear,
    .ImDrawList_ClearFreeMemory = ImDrawList_ClearFreeMemory,
    .ImDrawList_PrimReserve = ImDrawList_PrimReserve,
    .ImDrawList_PrimRect = ImDrawList_PrimRect,
    .ImDrawList_PrimRectUV = ImDrawList_PrimRectUV,
    .ImDrawList_PrimQuadUV = ImDrawList_PrimQuadUV,
    .ImDrawList_PrimWriteVtx = ImDrawList_PrimWriteVtx,
    .ImDrawList_PrimWriteIdx = ImDrawList_PrimWriteIdx,
    .ImDrawList_PrimVtx = ImDrawList_PrimVtx,
    .ImDrawList_UpdateClipRect = ImDrawList_UpdateClipRect,
    .ImDrawList_UpdateTextureID = ImDrawList_UpdateTextureID,
    .ImDrawList_GetClipRectMin_nonUDT = ImDrawList_GetClipRectMin_nonUDT,
    .ImDrawList_GetClipRectMax_nonUDT = ImDrawList_GetClipRectMax_nonUDT,
    .ImFont_ImFont = ImFont_ImFont,
    .ImFont_destroy = ImFont_destroy,
    .ImFont_FindGlyph = ImFont_FindGlyph,
    .ImFont_FindGlyphNoFallback = ImFont_FindGlyphNoFallback,
    .ImFont_GetCharAdvance = ImFont_GetCharAdvance,
    .ImFont_IsLoaded = ImFont_IsLoaded,
    .ImFont_GetDebugName = ImFont_GetDebugName,
    .ImFont_CalcWordWrapPositionA = ImFont_CalcWordWrapPositionA,
    .ImFont_RenderChar = ImFont_RenderChar,
    .ImFont_RenderText = ImFont_RenderText,
    .ImFont_BuildLookupTable = ImFont_BuildLookupTable,
    .ImFont_ClearOutputData = ImFont_ClearOutputData,
    .ImFont_GrowIndex = ImFont_GrowIndex,
    .ImFont_AddGlyph = ImFont_AddGlyph,
    .ImFont_AddRemapChar = ImFont_AddRemapChar,
    .ImFont_SetFallbackChar = ImFont_SetFallbackChar,
    .ImFont_CalcTextSizeA_nonUDT = ImFont_CalcTextSizeA_nonUDT,
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
    .ImGuiListClipper_Step = ImGuiListClipper_Step,
    .ImGuiListClipper_Begin = ImGuiListClipper_Begin,
    .ImGuiListClipper_End = ImGuiListClipper_End,
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
#define SOKOL_LOG(s) rizz_log_error(the_core, s)

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
    ImDrawVert* verts;
    uint16_t* indices;
    sg_shader shader;
    sg_pipeline pip;
    sg_bindings bind;
    sg_pass_action pass_action;
    sg_image font_tex;
    bool mouse_btn_down[RIZZ_APP_MAX_MOUSEBUTTONS];
    bool mouse_btn_up[RIZZ_APP_MAX_MOUSEBUTTONS];
    float mouse_wheel_h;
    float mouse_wheel;
    bool keys_down[512];
    ImWchar* char_input;    // sx_array
    ImGuiMouseCursor last_cursor;
    sg_imgui_t sg_imgui;
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
    return sx_malloc((const sx_alloc*)user_data, sz);
}

static void imgui__free(void* ptr, void* user_data)
{
    sx_free((const sx_alloc*)user_data, ptr);
}

static bool imgui__setup()
{
    sx_assert(g_imgui.ctx == NULL);
    const sx_alloc* alloc = the_core->alloc(RIZZ_MEMID_TOOLSET);
    g_sg_imgui_alloc = alloc;
    the__imgui.SetAllocatorFunctions(imgui__malloc, imgui__free, (void*)alloc);

    g_imgui.last_cursor = ImGuiMouseCursor_COUNT;
    g_imgui.ctx = the__imgui.CreateContext(NULL);
    if (!g_imgui.ctx) {
        rizz_log_error(the_core, "imgui: init failed");
        return false;
    }

    static char ini_filename[64];
    ImGuiIO* conf = the__imgui.GetIO();
    sx_snprintf(ini_filename, sizeof(ini_filename), "%s_imgui.ini", the_app->name());
    conf->IniFilename = ini_filename;

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
    conf->KeyMap[ImGuiKey_Escape] = RIZZ_APP_KEYCODE_ESCAPE;
    conf->KeyMap[ImGuiKey_A] = RIZZ_APP_KEYCODE_A;
    conf->KeyMap[ImGuiKey_C] = RIZZ_APP_KEYCODE_C;
    conf->KeyMap[ImGuiKey_V] = RIZZ_APP_KEYCODE_V;
    conf->KeyMap[ImGuiKey_X] = RIZZ_APP_KEYCODE_X;
    conf->KeyMap[ImGuiKey_Y] = RIZZ_APP_KEYCODE_Y;
    conf->KeyMap[ImGuiKey_Z] = RIZZ_APP_KEYCODE_Z;

    // Setup graphic objects
    g_imgui.verts = (ImDrawVert*)sx_malloc(alloc, sizeof(ImDrawVert) * MAX_VERTS);
    g_imgui.indices = (uint16_t*)sx_malloc(alloc, sizeof(uint16_t) * MAX_INDICES);
    if (!g_imgui.verts || !g_imgui.indices) {
        sx_out_of_memory();
        return false;
    }

    g_imgui.bind.vertex_buffers[0] =
        the_gfx->make_buffer(&(sg_buffer_desc){ .type = SG_BUFFERTYPE_VERTEXBUFFER,
                                                    .usage = SG_USAGE_STREAM,
                                                    .size = sizeof(ImDrawVert) * MAX_VERTS });
    g_imgui.bind.index_buffer =
        the_gfx->make_buffer(&(sg_buffer_desc){ .type = SG_BUFFERTYPE_INDEXBUFFER,
                                                    .usage = SG_USAGE_STREAM,
                                                    .size = sizeof(uint16_t) * MAX_INDICES });

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
                          .content.subimage[0][0].size = font_width * font_height * 4 });

    conf->Fonts->TexID = (ImTextureID)(uintptr_t)g_imgui.font_tex.id;

    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    rizz_shader shader = the_gfx->shader_make_with_data(
        tmp_alloc, k_imgui_vs_size, k_imgui_vs_data, k_imgui_vs_refl_size, k_imgui_vs_refl_data,
        k_imgui_fs_size, k_imgui_fs_data, k_imgui_fs_refl_size, k_imgui_fs_refl_data);
    g_imgui.shader = shader.shd;

    sg_pipeline_desc pip_desc = { .layout.buffers[0].stride = sizeof(ImDrawVert),
                                  .shader = g_imgui.shader,
                                  .index_type = SG_INDEXTYPE_UINT16,
                                  .rasterizer = { .sample_count = 4 },
                                  .blend = { .enabled = true,
                                             .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                                             .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                                             .color_write_mask = SG_COLORMASK_RGB } };
    g_imgui.pip = the_gfx->make_pipeline(
        the_gfx->shader_bindto_pipeline(&shader, &pip_desc, &k__imgui_vertex));

    g_imgui.pass_action = (sg_pass_action){ .colors[0] = { .action = SG_ACTION_DONTCARE },
                                            .depth = { .action = SG_ACTION_DONTCARE },
                                            .stencil = { .action = SG_ACTION_DONTCARE } };

    the_core->tmp_alloc_pop();

    return true;
}

static void imgui__release()
{
    const sx_alloc* alloc = the_core->alloc(RIZZ_MEMID_TOOLSET);

    if (g_imgui.ctx)
        the__imgui.DestroyContext(g_imgui.ctx);

    if (g_imgui.pip.id)
        the_gfx->destroy_pipeline(g_imgui.pip);
    if (g_imgui.bind.vertex_buffers[0].id)
        the_gfx->destroy_buffer(g_imgui.bind.vertex_buffers[0]);
    if (g_imgui.bind.index_buffer.id)
        the_gfx->destroy_buffer(g_imgui.bind.index_buffer);
    if (g_imgui.shader.id)
        the_gfx->destroy_shader(g_imgui.shader);
    if (g_imgui.font_tex.id)
        the_gfx->destroy_image(g_imgui.font_tex);
    if (g_imgui.verts)
        sx_free(alloc, g_imgui.verts);
    if (g_imgui.indices)
        sx_free(alloc, g_imgui.indices);
    sx_array_free(the_core->alloc(RIZZ_MEMID_TOOLSET), g_imgui.char_input);
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

    io->DisplaySize = the_app->sizef();
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
    ImGuizmo_BeginFrame();
    ImGuizmo_SetRect(0, 0, io->DisplaySize.x, io->DisplaySize.y);

    // Update OS mouse cursor with the cursor requested by imgui
    ImGuiMouseCursor mouse_cursor =
        io->MouseDrawCursor ? ImGuiMouseCursor_None : the__imgui.GetMouseCursor();
    if (g_imgui.last_cursor != mouse_cursor) {
        g_imgui.last_cursor = mouse_cursor;
        imgui__update_cursor();
    }
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

        if ((num_verts + dl_num_verts) > MAX_VERTS) {
            sx_assert(0 && "imgui: vertex buffer overflowed");
            break;
        }

        if ((num_indices + dl_num_indices) > MAX_INDICES) {
            sx_assert(0 && "imgui: index buffer overflowed");
            break;
        }

        sx_memcpy(&verts[num_verts], dl->VtxBuffer.Data, dl_num_verts * sizeof(ImDrawVert));

        const ImDrawIdx* src_index_ptr = (ImDrawIdx*)dl->IdxBuffer.Data;
        const uint16_t base_vertex_idx = num_verts;
        for (int i = 0; i < dl_num_indices; i++)
            indices[num_indices++] = src_index_ptr[i] + base_vertex_idx;
        num_verts += dl_num_verts;
    }

    the_gfx->imm.update_buffer(g_imgui.bind.vertex_buffers[0], verts,
                               MAX_VERTS * sizeof(ImDrawVert));
    the_gfx->imm.update_buffer(g_imgui.bind.index_buffer, indices, MAX_INDICES * sizeof(uint16_t));

    // Draw the list
    ImGuiIO* io = the__imgui.GetIO();
    imgui__shader_uniforms uniforms = { .disp_size.x = io->DisplaySize.x,
                                        .disp_size.y = io->DisplaySize.y };
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
                const int scissor_x = (int)(cmd->ClipRect.x);
                const int scissor_y = (int)(cmd->ClipRect.y);
                const int scissor_w = (int)(cmd->ClipRect.z - cmd->ClipRect.x);
                const int scissor_h = (int)(cmd->ClipRect.w - cmd->ClipRect.y);

                sg_image tex = { .id = (uint32_t)(uintptr_t)cmd->TextureId };
                if (tex.id != last_img.id) {
                    g_imgui.bind.fs_images[0] = tex;
                    the_gfx->imm.apply_bindings(&g_imgui.bind);
                }
                the_gfx->imm.apply_scissor_rect(scissor_x, scissor_y, scissor_w, scissor_h, true);
                the_gfx->imm.draw(base_elem, cmd->ElemCount, 1);
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
    the_gfx->imm.begin_default_pass(&g_imgui.pass_action, the_app->width(), the_app->height());
    imgui__draw(the__imgui.GetDrawData());
    the_gfx->imm.end_pass();
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
    the__imgui.PushStyleColorU32(ImGuiCol_WindowBg, 0);
    the__imgui.PushStyleColorU32(ImGuiCol_Border, 0);
    the__imgui.PushStyleVarFloat(ImGuiStyleVar_WindowRounding, 0.0f);

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

// gizmo
static void imgui__gizmo_set_rect(const sx_rect rc)
{
    ImGuizmo_SetRect(rc.xmin, rc.ymin, rc.xmax - rc.xmin, rc.ymax - rc.ymin);
}

static void imgui__gizmo_decompose_mat4(const sx_mat4* mat, sx_vec3* translation, sx_vec3* rotation,
                                        sx_vec3* scale)
{
    sx_mat4 transpose = sx_mat4_transpose(mat);
    ImGuizmo_DecomposeMatrixToComponents(transpose.f, translation->f, rotation->f, scale->f);
}

static void imgui__gizmo_compose_mat4(sx_mat4* mat, const sx_vec3 translation,
                                      const sx_vec3 rotation, const sx_vec3 scale)
{
    ImGuizmo_RecomposeMatrixFromComponents(translation.f, rotation.f, scale.f, mat->f);
    sx_mat4_transpose(mat);
}

static inline void imgui__mat4_to_gizmo(float dest[16], const sx_mat4* src)
{
    dest[0] = src->m11;
    dest[1] = src->m21;
    dest[2] = src->m31;
    dest[3] = src->m41;
    dest[4] = src->m12;
    dest[5] = src->m22;
    dest[6] = src->m32;
    dest[7] = src->m42;
    dest[8] = src->m13;
    dest[9] = src->m23;
    dest[10] = src->m33;
    dest[11] = src->m43;
    dest[12] = src->m14;
    dest[13] = src->m24;
    dest[14] = src->m34;
    dest[15] = src->m44;
}

static inline sx_mat4 imgui__mat4_from_gizmo(const float src[16])
{
    //    return sx_mat4f(src[0], src[1], src[2], src[3], src[4], src[5], src[6], src[7], src[8],
    //    src[9],
    //                    src[10], src[11], src[12], src[13], src[14], src[15]);
    return sx_mat4v(
        sx_vec4f(src[0], src[1], src[2], src[3]), sx_vec4f(src[4], src[5], src[6], src[7]),
        sx_vec4f(src[8], src[9], src[10], src[11]), sx_vec4f(src[12], src[13], src[14], src[15]));
}

static void imgui__gizmo_translate(sx_mat4* mat, const sx_mat4* view, const sx_mat4* proj,
                                   gizmo_mode mode, sx_mat4* delta_mat, sx_vec3* snap)
{
    float _mat[16];
    float tview[16];
    float tproj[16];
    float _delta[16];
    imgui__mat4_to_gizmo(_mat, mat);
    imgui__mat4_to_gizmo(tview, view);
    imgui__mat4_to_gizmo(tproj, proj);
    ImGuizmo_Manipulate(tview, tproj, TRANSLATE, (enum MODE)mode, _mat, delta_mat ? _delta : NULL,
                        snap ? snap->f : NULL, NULL, NULL);
    if (delta_mat)
        *delta_mat = imgui__mat4_from_gizmo(_delta);
    *mat = imgui__mat4_from_gizmo(_mat);
}

static void imgui__gizmo_rotation(sx_mat4* mat, const sx_mat4* view, const sx_mat4* proj,
                                  gizmo_mode mode, sx_mat4* delta_mat, float* snap)
{
    float _mat[16];
    float tview[16];
    float tproj[16];
    float _delta[16];
    imgui__mat4_to_gizmo(_mat, mat);
    imgui__mat4_to_gizmo(tview, view);
    imgui__mat4_to_gizmo(tproj, proj);
    ImGuizmo_Manipulate(tview, tproj, ROTATE, (enum MODE)mode, _mat, delta_mat ? _delta : NULL,
                        snap, NULL, NULL);
    if (delta_mat)
        *delta_mat = imgui__mat4_from_gizmo(_delta);
    *mat = imgui__mat4_from_gizmo(_mat);
}

static void imgui__gizmo_scale(sx_mat4* mat, const sx_mat4* view, const sx_mat4* proj,
                               gizmo_mode mode, sx_mat4* delta_mat, float* snap)
{
    float _mat[16];
    float tview[16];
    float tproj[16];
    float _delta[16];
    imgui__mat4_to_gizmo(_mat, mat);
    imgui__mat4_to_gizmo(tview, view);
    imgui__mat4_to_gizmo(tproj, proj);
    ImGuizmo_Manipulate(tview, tproj, SCALE, (enum MODE)mode, _mat, delta_mat ? _delta : NULL, snap,
                        NULL, NULL);
    if (delta_mat)
        *delta_mat = imgui__mat4_from_gizmo(_delta);
    *mat = imgui__mat4_from_gizmo(_mat);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// memory debugger
static void imgui__dual_progress_bar(float fraction1, float fraction2, const sx_vec2 ctrl_size,
                                     const char* overlay1, const char* overlay2)
{

    ImDrawList* draw_list = the__imgui.GetWindowDrawList();

    sx_vec2 pos;
    sx_vec2 size = sx_vec2f(ctrl_size.x < 0 ? the__imgui.GetContentRegionAvailWidth() : ctrl_size.x,
                            ctrl_size.y);

    the__imgui.GetCursorScreenPos_nonUDT(&pos);
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

    the__imgui.CalcTextSize_nonUDT(&overlay2_size, overlay2, NULL, false, 0);
    float text2_y = pos.y + (h - overlay2_size.y) * 0.5f;
    if (end2.x + style->ItemSpacing.x + overlay2_size.x < _max.x)
        overlay2_pos = sx_vec2f(end2.x + style->ItemSpacing.x, text2_y);
    else
        overlay2_pos = sx_vec2f(end2.x - style->ItemSpacing.x - overlay2_size.x, text2_y);

    sx_vec2 end1 = sx_vec2f(sx_lerp(pos.x, _max.x, fraction1), _max.y);
    the__imgui.CalcTextSize_nonUDT(&overlay1_size, overlay1, NULL, false, 0);
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
        the__imgui.CalcTextSize_nonUDT(&overlay1_size, overlay1, NULL, false, 0);
        if (end1.x + style->ItemSpacing.x + overlay1_size.x < _max.x)
            overlay1_pos = sx_vec2f(end1.x + style->ItemSpacing.x, text1_y);
        else
            overlay1_pos = sx_vec2f(end1.x - style->ItemSpacing.x - overlay1_size.x, text1_y);
    }

    if (!fracs_equal) {
        the__imgui.ImDrawList_AddRectFilled(draw_list, pos, end2, prog2_color.n, 0, 0);

        // Text2
        if (overlay2)
            the__imgui.ImDrawList_AddText(draw_list, overlay2_pos, text_color.n, overlay2, NULL);
    }

    // fraction1 rect
    the__imgui.ImDrawList_AddRectFilled(draw_list, pos, end1, prog_color.n, 0, 0);

    // Text1
    the__imgui.ImDrawList_AddText(draw_list, overlay1_pos, text_color.n, overlay1, NULL);

    // the__imgui.PopClipRect();
    the__imgui.NewLine();
}

static void imgui__graphics_debugger(const rizz_gfx_trace_info* info, bool* p_open)
{
    sx_assert(info);
    the__imgui.SetNextWindowSizeConstraints(sx_vec2f(400.0f, 300.0f), sx_vec2f(FLT_MAX, FLT_MAX),
                                            NULL, NULL);
    if (the__imgui.Begin("Graphics Debugger", p_open, 0)) {
        if (the__imgui.BeginTabBar("#gfx_debugger_tabs", 0)) {
            if (the__imgui.BeginTabItem("General", NULL, 0)) {
                the__imgui.LabelText("Draws", "%d", info->num_draws);
                the__imgui.LabelText("Instances", "%d", info->num_instances);
                the__imgui.LabelText("Elements", "%d", info->num_elements);
                the__imgui.Separator();
                the__imgui.LabelText("Active Pipelines", "%d", info->num_apply_pipelines);
                the__imgui.LabelText("Active Passes", "%d", info->num_apply_passes);
                the__imgui.Separator();
                the__imgui.LabelText("Pipelines", "%d", info->num_pipelines);
                the__imgui.LabelText("Shaders", "%d", info->num_shaders);
                the__imgui.LabelText("Passes", "%d", info->num_passes);
                the__imgui.LabelText("Images", "%d", info->num_images);
                the__imgui.LabelText("Buffers", "%d", info->num_buffers);
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

            the__imgui.EndTabBar();
        }
    }
    the__imgui.End();
}

static void imgui__memory_debugger(const rizz_mem_info* info, bool* p_open)
{
    static bool peaks = true;
    static int selected_heap = -1;

    // TODO: add search filters to items in allocations
    the__imgui.SetNextWindowSizeConstraints(sx_vec2f(600.0f, 100.0f), sx_vec2f(FLT_MAX, FLT_MAX),
                                            NULL, NULL);

    if (the__imgui.Begin("Memory Debugger", p_open, 0)) {
        if (the__imgui.CollapsingHeader("Heap", ImGuiTreeNodeFlags_DefaultOpen)) {
            the__imgui.Columns(3, NULL, false);
            the__imgui.LabelText("Count", "%d", info->heap_count);
            the__imgui.NextColumn();
            the__imgui.NextColumn();
            the__imgui.Checkbox("Peaks", &peaks);
            the__imgui.Columns(1, NULL, false);
            the__imgui.Separator();

            char size_text[32];
            char peak_text[32];
            const sx_vec2 progress_size = sx_vec2f(-1.0f, 14.0f);

            if (peaks)
                sx_snprintf(peak_text, sizeof(peak_text), "%$.2d", info->heap_max);
            sx_snprintf(size_text, sizeof(size_text), "%$.2d", info->heap);

            the__imgui.Text("Heap");
            the__imgui.SameLine(100.0f, -1);
            if (peaks) {
                imgui__dual_progress_bar((float)info->heap / (float)info->heap_max, 1.0f,
                                         progress_size, size_text, peak_text);
            } else {
                the__imgui.ProgressBar((float)info->heap / (float)info->heap_max, progress_size,
                                       size_text);
            }

            for (int i = 0; i < _RIZZ_MEMID_COUNT; i++) {
                const rizz_trackalloc_info* t = &info->trackers[i];
                if (peaks)
                    sx_snprintf(peak_text, sizeof(peak_text), "%$.2d", t->peak);
                sx_snprintf(size_text, sizeof(size_text), "%$.2d", t->size);
                if (the__imgui.Selectable(t->name, selected_heap == i,
                                          ImGuiSelectableFlags_SpanAllColumns, SX_VEC2_ZERO)) {
                    selected_heap = i;
                }
                the__imgui.SameLine(100.0f, -1);

                float p = (float)t->size / (float)info->heap;
                if (peaks) {
                    imgui__dual_progress_bar(p, (float)t->peak / (float)info->heap, progress_size,
                                             size_text, peak_text);
                } else {
                    the__imgui.ProgressBar(p, progress_size, size_text);
                }
            }

            if (selected_heap != -1) {
                char text[32];
                const rizz_trackalloc_info* t = &info->trackers[selected_heap];
                ImGuiListClipper clipper;
                int num_items = t->num_items;

                if (num_items) {
                    the__imgui.Separator();
                    the__imgui.Columns(6, NULL, false);
                    the__imgui.SetColumnWidth(0, 35.0f);
                    the__imgui.Text("#");
                    the__imgui.NextColumn();
                    the__imgui.SetColumnWidth(1, 150.0f);
                    the__imgui.Text("Ptr");
                    the__imgui.NextColumn();
                    the__imgui.SetColumnWidth(2, 50.0f);
                    the__imgui.Text("Size");
                    the__imgui.NextColumn();
                    the__imgui.SetColumnWidth(3, 100.0f);
                    the__imgui.Text("File");
                    the__imgui.NextColumn();
                    the__imgui.SetColumnWidth(4, 200.0f);
                    the__imgui.Text("Function");
                    the__imgui.NextColumn();
                    the__imgui.SetColumnWidth(5, 35.0f);
                    the__imgui.Text("Line");
                    the__imgui.NextColumn();
                    the__imgui.Separator();

                    the__imgui.Columns(1, NULL, false);
                    the__imgui.BeginChild("AllocationList",
                                          sx_vec2f(the__imgui.GetWindowContentRegionWidth(), -1.0f),
                                          false, 0);
                    the__imgui.Columns(6, NULL, false);
                    the__imgui.ImGuiListClipper_Begin(&clipper, num_items, -1.0f);
                    while (the__imgui.ImGuiListClipper_Step(&clipper)) {
                        int start = num_items - clipper.DisplayStart - 1;
                        int end = num_items - clipper.DisplayEnd;
                        for (int i = start; i >= end; i--) {
                            const rizz_track_alloc_item* mitem = &t->items[i];

                            sx_snprintf(text, sizeof(text), "%d", i + 1);
                            the__imgui.SetColumnWidth(0, 35.0f);
                            the__imgui.Text(text);
                            the__imgui.NextColumn();

                            sx_snprintf(text, sizeof(text), "0x%p", mitem->ptr);
                            the__imgui.SetColumnWidth(1, 150.0f);
                            the__imgui.Text(text);
                            the__imgui.NextColumn();

                            sx_snprintf(text, sizeof(text), "%$.2d", mitem->size);
                            the__imgui.SetColumnWidth(2, 50.0f);
                            the__imgui.Text(text);
                            the__imgui.NextColumn();

                            the__imgui.SetColumnWidth(3, 100.0f);
                            the__imgui.Text(mitem->file);
                            the__imgui.NextColumn();

                            the__imgui.SetColumnWidth(4, 200.0f);
                            the__imgui.Text(mitem->func);
                            the__imgui.NextColumn();

                            the__imgui.SetColumnWidth(5, 35.0f);
                            sx_snprintf(text, sizeof(text), "%d", mitem->line);
                            the__imgui.Text(text);
                            the__imgui.NextColumn();
                        }
                    }
                    the__imgui.ImGuiListClipper_End(&clipper);
                    the__imgui.EndChild();
                }    //
            }        // list-clipper
        }            // Heap

        // temp allocators
        if (the__imgui.CollapsingHeader("Temp Allocators", 0)) {
            char text[32];
            char size_text[32];
            char peak_text[32];
            for (int i = 0; i < info->num_temp_allocs; i++) {
                const rizz_linalloc_info* l = &info->temp_allocs[i];
                sx_snprintf(text, sizeof(text), "Temp #%d", i + 1);
                sx_snprintf(size_text, sizeof(size_text), "%$.2d", l->offset);
                sx_snprintf(peak_text, sizeof(peak_text), "%$.2d", l->peak);
                float o = (float)l->offset / (float)l->size;
                float p = (float)l->peak / (float)l->size;
                the__imgui.Text(text);
                the__imgui.SameLine(100.0f, -1);
                imgui__dual_progress_bar(o, p, sx_vec2f(-1.0f, 14.0f), size_text, peak_text);
            }
        }
    }

    the__imgui.End();
}

static rizz_api_imgui_extra the__imgui_debug_tools = {
    .memory_debugger = imgui__memory_debugger,
    .graphics_debugger = imgui__graphics_debugger,
    .begin_fullscreen_draw = imgui__begin_fullscreen_draw,
    .draw_cursor = imgui__draw_cursor,
    .project_to_screen = imgui__project_to_screen,
    .gizmo_hover = ImGuizmo_IsOver,
    .gizmo_using = ImGuizmo_IsUsing,
    .gizmo_set_window = ImGuizmo_SetDrawlist,
    .gizmo_set_ortho = ImGuizmo_SetOrthographic,
    .gizmo_enable = ImGuizmo_Enable,
    .gizmo_set_rect = imgui__gizmo_set_rect,
    .gizmo_decompose_mat4 = imgui__gizmo_decompose_mat4,
    .gizmo_compose_mat4 = imgui__gizmo_compose_mat4,
    .gizmo_translate = imgui__gizmo_translate,
    .gizmo_rotation = imgui__gizmo_rotation,
    .gizmo_scale = imgui__gizmo_scale
};

rizz_plugin_decl_event_handler(imgui, e)
{
    ImGuiIO* io = the__imgui.GetIO();
    switch (e->type) {
    case RIZZ_APP_EVENTTYPE_MOUSE_DOWN:
        io->MousePos = sx_vec2f(e->mouse_x, e->mouse_y);
        g_imgui.mouse_btn_down[e->mouse_button] = true;
        break;
    case RIZZ_APP_EVENTTYPE_MOUSE_UP:
        io->MousePos = sx_vec2f(e->mouse_x, e->mouse_y);
        g_imgui.mouse_btn_up[e->mouse_button] = true;
        break;
    case RIZZ_APP_EVENTTYPE_MOUSE_MOVE:
        io->MousePos = sx_vec2f(e->mouse_x, e->mouse_y);
        break;
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
        io->DisplaySize = the_app->sizef();
        ImGuizmo_SetRect(0, 0, io->DisplaySize.x, io->DisplaySize.y);
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
            assert(0);
        }
    }
}

rizz_plugin_decl_main(imgui, plugin, e)
{
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP:
        imgui__frame();
        break;

    case RIZZ_PLUGIN_EVENT_INIT: {
        the_plugin = plugin->api;
        the_core = (rizz_api_core*)the_plugin->get_api(RIZZ_API_CORE, 0);
        the_gfx = (rizz_api_gfx*)the_plugin->get_api(RIZZ_API_GFX, 0);
        the_app = (rizz_api_app*)the_plugin->get_api(RIZZ_API_APP, 0);

        if (!imgui__setup()) {
            return -1;
        }

        sx_assert(the_plugin);
        the_plugin->inject_api("imgui", 0, &the__imgui);
        the_plugin->inject_api("imgui_extra", 0, &the__imgui_debug_tools);

        void* make_cmdbuff;
        int make_cmdbuff_sz;
        the_gfx->internal_state(&make_cmdbuff, &make_cmdbuff_sz);

        sg_imgui_init(&g_imgui.sg_imgui, the_gfx);

        // submit all make commands to the imgui
        if (make_cmdbuff_sz) {
            imgui__submit_make_commands(make_cmdbuff, make_cmdbuff_sz);
        }

        //imgui__frame();
        break;
    }

    case RIZZ_PLUGIN_EVENT_LOAD: {
        the__imgui.SetCurrentContext(g_imgui.ctx);
        the__imgui.SetAllocatorFunctions(imgui__malloc, imgui__free, (void*)g_sg_imgui_alloc);
        the_plugin->inject_api("imgui", 0, &the__imgui);
        the_plugin->inject_api("imgui_extra", 0, &the__imgui_debug_tools);
        break;
    }

    case RIZZ_PLUGIN_EVENT_UNLOAD:
        break;

    case RIZZ_PLUGIN_EVENT_SHUTDOWN:
        sx_assert(the_plugin);
        sg_imgui_discard(&g_imgui.sg_imgui);
        imgui__release();
        the_plugin->remove_api("imgui", 0);
        the_plugin->remove_api("imgui_extra", 0);
        break;
    }

    return 0;
}

rizz_plugin_implement_info(imgui, 1690, "dear-imgui plugin", NULL, 0);
