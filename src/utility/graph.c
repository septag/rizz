#include "rizz/imgui.h"
#include "rizz/utility.h"

#include "sx/math-vec.h"

static inline void sortkeys(rizz_graph* graph)
{
    uint32_t n = graph->num_keys;
    while (n) {
        uint32_t i, newn = 0;
        for (i = 1; i < n; ++i) {
            if ((graph->keys[i - 1].t - graph->keys[i].t) > 0) {
                rizz_graph_key tmp = graph->keys[i - 1];
                graph->keys[i - 1] = graph->keys[i];
                graph->keys[i] = tmp;
                newn = i;
            }
        }
        n = newn;
    }
}

SX_ALLOW_UNUSED static inline float bezier(float a, float b, float c, float d, float t)
{
    float ab = sx_lerp(a, b, t);
    float bc = sx_lerp(b, c, t);
    float cd = sx_lerp(c, d, t);

    float abc = sx_lerp(ab, bc, t);
    float bcd = sx_lerp(bc, cd, t);

    return sx_lerp(abc, bcd, t);
}

void graph__init(rizz_graph* graph, float start, float end)
{
    start = sx_clamp(start, 0, 1);
    end = sx_clamp(end, 0, 1);
    *graph = (rizz_graph){
        .keys[0] = { 0, start },
        .keys[1] = { 1, end },
        .num_keys = 2,
    };
}

bool graph__add_key(rizz_graph* graph, rizz_graph_key key)
{
    if (graph->num_keys == RIZZ_GRAPH_MAX_KEYS)
        return false;    // no room for new keys

    key.t = sx_clamp(key.t, 0.02f, 0.98f);    // avoid overlap on first or last key
    key.value = sx_clamp(key.value, 0, 1);
    uint32_t index = graph->num_keys;
    graph->keys[index] = key;
    graph->num_keys++;
    sortkeys(graph);
    return true;
}

bool graph__move_key(rizz_graph* graph, int index, float t, float value)
{
    if (index > 0 && index < (int)graph->num_keys - 1) {
        float min = graph->keys[index - 1].t + 0.02f;
        float max = graph->keys[index + 1].t - 0.02f;
        t = sx_clamp(t, min, max);
        graph->keys[index].t = t;
    }
    value = sx_clamp(value, 0, 1);
    graph->keys[index].value = value;
    sortkeys(graph);
    return true;
}

bool graph__remove_key(rizz_graph* graph, int index)
{
    if (index <= 0 || index >= (int)graph->num_keys - 1)
        return false;    // dont remove first or last key

    graph->num_keys--;
    sx_memmove(&graph->keys[index], &graph->keys[index + 1],
               (graph->num_keys - index) * sizeof(rizz_graph_key));
    return true;
}

float graph__eval(const rizz_graph* graph, float t)
{
    sx_assert(graph->num_keys);

    if (graph->num_keys == 1)
        return graph->keys[0].value;

    int a = 0;
    int b = 1;

    for (int i = 0; i < (int)graph->num_keys; i++) {
        if (graph->keys[i].t >= t) {
            b = i;
            break;
        } else {
            a = i;
        }
    }

    float av = graph->keys[a].value;
    float bv = graph->keys[b].value;
    float aw = graph->keys[a].rwing;
    float bw = graph->keys[b].lwing;
    float at = graph->keys[a].t;
    float bt = graph->keys[b].t;
    t = (t - at) / (bt - at);
    return bezier(av, av + aw, bv + bw, bv, t);
}

float graph__eval_remap(const rizz_graph* graph, float t, float t_min, float t_max, float v_min,
                        float v_max)
{
    t = (t - t_min) / (t_max - t_min);    // inv_lerp
    return sx_lerp(graph__eval(graph, t), v_min, v_max);
}

void graph__edit_multiple(const rizz_api_imgui* gui, rizz_graph** graphs, const char** names,
                          sx_color* colors, int* selected, int num_graphs)
{
    *selected = sx_clamp(*selected, 0, num_graphs - 1);
    rizz_graph* graph = graphs[*selected];

    if (names && names[0])
        gui->PushID_Str(names[0]);
    else
        gui->PushID_Str("graph");

    sx_vec2 rpos, rsize, mpos;
    gui->GetMousePos(&mpos);
    gui->GetCursorScreenPos(&rpos);
    rsize = sx_vec2f(gui->CalcItemWidth(), gui->CalcItemWidth() / 2);
    ImDrawList* dlst = gui->GetWindowDrawList();
    rpos = sx_vec2_addf(rpos, 2.0f);
    rsize = sx_vec2_subf(rsize, 4.0f);
    int count = graph->num_keys;
    sx_rect clip_rect = sx_rectwh(rpos.x, rpos.y, rsize.x, rsize.y);

    bool mpos_out = !sx_rect_test_point(
        sx_rectwh(rpos.x - 20, rpos.y - 20, rsize.x + 40, rsize.y + 40),    // rect
        mpos);

    // draws background
    {
        sx_vec2 p_min = rpos;
        sx_vec2 p_max = sx_vec2_add(rpos, rsize);
        const sx_color c = sx_color4u(255, 255, 255, 100);
        gui->ImDrawList_AddRectFilled(dlst, p_min, p_max, SX_COLOR_BLACK.n, 0, 0);
        const int gridx = 11;
        const int gridy = 7;
        for (int i = 0; i < gridx; i++) {
            float x = rpos.x + (i / (float)(gridx - 1)) * rsize.x;
            gui->ImDrawList_AddLine(dlst, sx_vec2f(x, rpos.y), sx_vec2f(x, rpos.y + rsize.y), c.n,
                                    1);
        }
        for (int i = 0; i < gridy; i++) {
            float y = rpos.y + (i / (float)(gridy - 1)) * rsize.y;
            gui->ImDrawList_AddLine(dlst, sx_vec2f(rpos.x, y), sx_vec2f(rpos.x + rsize.x, y), c.n,
                                    1);
        }
    }

    gui->ImDrawList_PushClipRect(dlst, sx_vec2fv(clip_rect.vmin), sx_vec2fv(clip_rect.vmax), true);
    // draw curve
    const int segments = 64;

    for (int g = 0; g < num_graphs; g++) {
        if (g == *selected)
            continue;    // draw active graph later

        rizz_graph* cur_graph = graphs[g];
        sx_color c = colors[g];
        for (int i = 0; i < segments; i++) {
            float t1 = (i + 0) / (float)segments;
            float t2 = (i + 1) / (float)segments;
            float v1 = graph__eval(cur_graph, t1);
            float v2 = graph__eval(cur_graph, t2);
            sx_vec2 p1 = sx_vec2f(rpos.x + t1 * rsize.x, rpos.y + (1 - v1) * rsize.y);
            sx_vec2 p2 = sx_vec2f(rpos.x + t2 * rsize.x, rpos.y + (1 - v2) * rsize.y);
            gui->ImDrawList_AddLine(dlst, p1, p2, c.n, 2);
        }
    }
    // now draw active graph curve
    {
        sx_color c = colors[*selected];
        for (int i = 0; i < segments; i++) {
            float t1 = (i + 0) / (float)segments;
            float t2 = (i + 1) / (float)segments;
            float v1 = graph__eval(graph, t1);
            float v2 = graph__eval(graph, t2);
            sx_vec2 p1 = sx_vec2f(rpos.x + t1 * rsize.x, rpos.y + (1 - v1) * rsize.y);
            sx_vec2 p2 = sx_vec2f(rpos.x + t2 * rsize.x, rpos.y + (1 - v2) * rsize.y);
            gui->ImDrawList_AddLine(dlst, p1, p2, c.n, 2);
        }
    }
    gui->PopClipRect();
    clip_rect = sx_rectv(sx_vec2_subf(sx_vec2fv(clip_rect.vmin), 4), sx_vec2_addf(sx_vec2fv(clip_rect.vmax), 4));
    gui->ImDrawList_PushClipRect(dlst, sx_vec2fv(clip_rect.vmin), sx_vec2fv(clip_rect.vmax), true);
    // draw keys
    int del_i = -1;
    for (int i = 0; i < count; i++) {
        gui->PushID_Int(i);
        sx_vec2 kpos = sx_vec2f(rpos.x + graph->keys[i].t * rsize.x,
                                rpos.y + rsize.y * (1 - graph->keys[i].value));

        sx_color c = colors[*selected];
        gui->ImDrawList_AddCircleFilled(dlst, kpos, 6, SX_COLOR_BLACK.n, 16);
        gui->ImDrawList_AddCircleFilled(dlst, kpos, 4, c.n, 16);

        gui->SetCursorScreenPos(sx_vec2_subf(kpos, 8));
        gui->InvisibleButton("graph-inv-btn", sx_vec2f(16, 16), 0);
        if (gui->IsItemActive()) {
            float t = (mpos.x - rpos.x) / rsize.x;
            float value = 1 - ((mpos.y - rpos.y) / rsize.y);
            graph__move_key(graph, i, t, value);

            if (mpos_out)
                gui->ImDrawList_AddCircleFilled(dlst, kpos, 8, sx_color4u(255, 0, 0, 200).n, 16);
        }
        if (gui->IsItemDeactivated() && mpos_out) {
            del_i = i;    // mark i for remove later
        }
        if (gui->BeginPopupContextItem("graph-key-popup", 1)) {
            float t = graph->keys[i].t, v = graph->keys[i].value;
            gui->DragFloat("Time", &t, 0.05f, 0.0f, 1.0f, "%.2f", 1);
            gui->DragFloat("Value", &v, 0.05f, 0.0f, 1.0f, "%.2f", 1);
            graph__move_key(graph, i, t, v);
            gui->DragFloat("R-Wing", &graph->keys[i].rwing, 0.05f, -1.0f, 1.0f, "%.2f", 1);
            gui->DragFloat("L-Wing", &graph->keys[i].lwing, 0.05f, -1.0f, 1.0f, "%.2f", 1);
            gui->EndPopup();
        }

        const float wdist = 25.0f;
        {
            sx_vec2 kposl = sx_vec2_add(kpos, sx_vec2f(-wdist, -graph->keys[i].lwing * wdist));
            gui->ImDrawList_AddLine(dlst, kpos, kposl, c.n, 2);
            gui->ImDrawList_AddCircleFilled(dlst, kposl, 2, c.n, 16);
            gui->SetCursorScreenPos(sx_vec2_subf(kposl, 4));
            gui->InvisibleButton("graph-lwing", sx_vec2f(8, 8), 0);
            if (gui->IsItemActive()) {
                graph->keys[i].lwing = (kpos.y - mpos.y) / wdist;
                graph->keys[i].lwing = sx_clamp(graph->keys[i].lwing, -1, 1);
            }
        }
        {
            sx_vec2 kposr = sx_vec2_add(kpos, sx_vec2f(wdist, -graph->keys[i].rwing * wdist));
            gui->ImDrawList_AddLine(dlst, kpos, kposr, c.n, 2);
            gui->ImDrawList_AddCircleFilled(dlst, kposr, 2, c.n, 16);
            gui->SetCursorScreenPos(sx_vec2_subf(kposr, 4));
            gui->InvisibleButton("graph-rwing", sx_vec2f(8, 8), 0);
            if (gui->IsItemActive()) {
                graph->keys[i].rwing = (kpos.y - mpos.y) / wdist;
                graph->keys[i].rwing = sx_clamp(graph->keys[i].rwing, -1, 1);
            }
        }

        gui->PopID();
    }
    gui->PopClipRect();

    // invisible button for add new key
    gui->SetCursorScreenPos(rpos);
    if (gui->InvisibleButton("graph-add-key", rsize, 0)) {
        float t = (mpos.x - rpos.x) / rsize.x;
        float value = 1 - ((mpos.y - rpos.y) / rsize.y);
        graph__add_key(graph, (rizz_graph_key){ t, value });
    }

    if (del_i != -1)
        graph__remove_key(graph, del_i);

    if (names) {
        gui->SameLine(0, 4);
        if (num_graphs > 1) {
            gui->ListBox_Str_arr("", selected, names, num_graphs, 4 /* TODO: calc listbox height*/);
        } else {
            gui->Text(names[0]);
        }
    }

    gui->PopID();
}

void graph__edit(const rizz_api_imgui* gui, const char* label, rizz_graph* graph, sx_color color)
{
    const char* _names[1] = { label };
    int _selected = 0;
    graph__edit_multiple(gui, &graph, _names, &color, &_selected, 1);
}