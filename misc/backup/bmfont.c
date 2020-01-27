#if 0    // bmfont loading
// text/font
enum rizz_text_flags_ {
    RIZZ_TEXT_ALIGN_CENTER = 0x01,
    RIZZ_TEXT_ALIGN_RIGHT = 0x02,
    RIZZ_TEXT_ALIGN_LEFT = 0x04,
    RIZZ_TEXT_RTL = 0x08,
    RIZZ_TEXT_MULTILINE = 0x10
};
typedef uint32_t rizz_text_flags;

enum rizz_font_flags_ {
    RIZZ_FONT_UNICODE = 0x1,
    RIZZ_FONT_ITALIC = 0x2,
    RIZZ_FONT_BOLD = 0x4,
    RIZZ_FONT_FIXED_HEIGHT = 0x8
};
typedef uint32_t rizz_font_flags;

typedef struct rizz_font {
    char name[32];
    int size;
    int line_height;
    int base;
    int img_width;
    int img_height;
    int char_width;
    rizz_font_flags flags;
    int16_t padding[4];
    int16_t spacing[2];
    rizz_asset img;
    int num_glyphs;
    int num_kerns;
} rizz_font;

typedef struct {
    rizz_font f;
    rizz__font_glyph* glyphs;
    rizz__font_glyph_kern* kerns;
    sx_hashtbl glyph_tbl;    // key: glyph-id, value: index-to-glyphs
} rizz__font;

typedef struct {
    sx_vec2 pos;
    sx_vec2 uv;
    sx_vec3 transform1;
    sx_vec3 transform2;
    sx_color color;
} rizz__text_vertex;

// FNT binary format
#    define RIZZ__FNT_SIGN "BMF"
#    pragma pack(push, 1)
typedef struct {
    int16_t font_size;
    int8_t bit_field;
    uint8_t charset;
    uint16_t stretch_h;
    uint8_t aa;
    uint8_t padding_up;
    uint8_t padding_right;
    uint8_t padding_dwn;
    uint8_t padding_left;
    uint8_t spacing_horz;
    uint8_t spacing_vert;
    uint8_t outline;
    /* name (str) */
} rizz__fnt_info;

typedef struct {
    uint16_t line_height;
    uint16_t base;
    uint16_t scale_w;
    uint16_t scale_h;
    uint16_t pages;
    int8_t bit_field;
    uint8_t alpha_channel;
    uint8_t red_channel;
    uint8_t green_channel;
    uint8_t blue_channel;
} rizz__fnt_common;

typedef struct {
    char path[255];
} rizz__fnt_page;

typedef struct {
    uint32_t id;
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
    int16_t xoffset;
    int16_t yoffset;
    int16_t xadvance;
    uint8_t page;
    uint8_t channel;
} rizz__fnt_char;

typedef struct {
    uint32_t first;
    uint32_t second;
    int16_t amount;
} rizz__fnt_kern;

typedef struct {
    uint8_t id;
    uint32_t size;
} rizz__fnt_block;
#    pragma pack(pop)

typedef struct {
    uint32_t second_id;
    float amount;
} rizz__font_glyph_kern;

typedef struct {
    uint32_t id;
    sx_rect uvs;
    float xoffset;     // pixels to offset right/left
    float yoffset;     // pixels to offset up/down
    float xadvance;    // pixels to advance to next char
    int num_kerns;
    int kern_idx;
} rizz__font_glyph;

// metadata
typedef struct {
    char img_filepath[RIZZ_MAX_PATH];
    int img_width;
    int img_height;
    int num_glyphs;
    int num_kerns;
} rizz__font_metadata;

static char* rizz__fnt_text_read_keyval(char* token, char* key, char* value, int max_chars)
{
    char* equal = (char*)sx_strchar(token, '=');
    if (equal) {
        sx_strncpy(key, max_chars, token, (int)(intptr_t)(equal - token));

        char* val_start = (char*)sx_skip_whitespace(equal + 1);
        char* val_end;
        if (*val_start == '"') {
            ++val_start;
            val_end = *val_start != '"' ? (char*)sx_strchar(val_start, '"') : val_start;
        } else {
            // search for next whitespace (where key=value ends)
            val_end = val_start;
            while (!sx_isspace(*val_end))
                ++val_end;
        }
        sx_strncpy(value, max_chars, val_start, (int)(intptr_t)(val_end - val_start));
        return *val_end != '"' ? val_end : (val_end + 1);
    } else {
        key[0] = 0;
        value[0] = 0;
        return (char*)sx_skip_word(token);
    }
}

static int rizz__fnt_text_parse_numbers(char* value, int16_t* numbers, int max_numbers)
{
    int index = 0;
    char snum[16];
    char* token = (char*)sx_strchar(value, ',');
    while (token) {
        sx_strncpy(snum, sizeof(snum), value, (int)(intptr_t)(token - value));
        if (index < max_numbers)
            numbers[index++] = (int16_t)sx_toint(snum);
        value = token + 1;
        token = (char*)sx_strchar(value, ',');
    }
    sx_strcpy(snum, sizeof(snum), value);
    if (index < max_numbers)
        numbers[index++] = (int16_t)sx_toint(snum);
    return index;
}

static void rizz__fnt_text_read_info(char* token, rizz_font* font)
{
    char key[32], value[32];

    token = (char*)sx_skip_whitespace(token);
    while (*token) {
        token = rizz__fnt_text_read_keyval(token, key, value, 32);
        if (sx_strequal(key, "face")) {
            sx_strcpy(font->name, 32, value);
        } else if (sx_strequal(key, "size")) {
            font->size = sx_toint(value);
        } else if (sx_strequal(key, "bold")) {
            font->flags |= RIZZ_FONT_BOLD;
        } else if (sx_strequal(key, "italic")) {
            font->flags |= RIZZ_FONT_ITALIC;
        } else if (sx_strequal(key, "unicode") && sx_tobool(value)) {
            font->flags |= RIZZ_FONT_UNICODE;
        } else if (sx_strequal(key, "fixedHeight")) {
            font->flags |= RIZZ_FONT_FIXED_HEIGHT;
        } else if (sx_strequal(key, "padding")) {
            rizz__fnt_text_parse_numbers(value, font->padding, 4);
        } else if (sx_strequal(key, "spacing")) {
            rizz__fnt_text_parse_numbers(value, font->spacing, 2);
        }
        token = (char*)sx_skip_whitespace(token);
    }
}

static void rizz__fnt_text_read_common(char* token, rizz_font* font)
{
    char key[32], value[32];
    token = (char*)sx_skip_whitespace(token);
    while (*token) {
        token = rizz__fnt_text_read_keyval(token, key, value, 32);
        if (sx_strequal(key, "lineHeight")) {
            font->line_height = sx_toint(value);
        } else if (sx_strequal(key, "base")) {
            font->base = sx_toint(value);
        } else if (sx_strequal(key, "scaleW")) {
            font->img_width = sx_toint(value);
        } else if (sx_strequal(key, "scaleH")) {
            font->img_height = sx_toint(value);
        }
        token = (char*)sx_skip_whitespace(token);
    }
}

static void rizz__fnt_text_read_page(char* token, char* img_filepath, int img_filepath_sz,
                                     const char* fnt_path)
{
    char key[32], value[32];
    token = (char*)sx_skip_whitespace(token);
    while (*token) {
        token = rizz__fnt_text_read_keyval(token, key, value, 32);
        if (sx_strequal(key, "file")) {
            char dirname[RIZZ_MAX_PATH];
            sx_os_path_dirname(dirname, sizeof(dirname), fnt_path);
            sx_os_path_join(img_filepath, img_filepath_sz, dirname, value);
        }
        token = (char*)sx_skip_whitespace(token);
    }
}

static int rizz__fnt_text_read_count(char* token)
{
    char key[32], value[32];
    token = (char*)sx_skip_whitespace(token);
    token = rizz__fnt_text_read_keyval(token, key, value, 32);
    if (sx_strequal(key, "count"))
        return sx_toint(value);
    sx_assert(0 && "invalid file format");
    return 0;
}

static void rizz__fnt_text_read_char(char* token, rizz__font_glyph* g, int* char_width,
                                     float img_width, float img_height)
{
    char key[32], value[32];
    float x = 0, y = 0, w = 0, h = 0;
    token = (char*)sx_skip_whitespace(token);
    while (*token) {
        token = rizz__fnt_text_read_keyval(token, key, value, 32);
        if (strcmp(key, "id") == 0) {
            g->id = sx_touint(value);
        } else if (strcmp(key, "x") == 0) {
            x = (float)sx_toint(value);
        } else if (strcmp(key, "y") == 0) {
            y = (float)sx_toint(value);
        } else if (strcmp(key, "width") == 0) {
            w = (float)sx_toint(value);
        } else if (strcmp(key, "height") == 0) {
            h = (float)sx_toint(value);
        } else if (strcmp(key, "xoffset") == 0) {
            g->xoffset = (float)sx_toint(value);
        } else if (strcmp(key, "yoffset") == 0) {
            g->yoffset = (float)sx_toint(value);
        } else if (strcmp(key, "xadvance") == 0) {
            int xadvance = sx_toint(value);
            g->xadvance = (float)xadvance;
            *char_width = sx_max(*char_width, xadvance);
        }
        token = (char*)sx_skip_whitespace(token);
    }

    g->uvs = sx_rectwh(x / img_width, y / img_height, w / img_width, h / img_height);
}

static void rizz__fnt_text_read_kern(char* token, rizz__font_glyph_kern* k, int kern_idx,
                                     rizz__font_glyph* glyphs, int num_glyphs)
{
    char key[32], value[32];
    int first_glyph_idx = -1;
    token = (char*)sx_skip_whitespace(token);
    while (*token) {
        token = rizz__fnt_text_read_keyval(token, key, value, 32);
        if (strcmp(key, "first") == 0) {
            uint32_t first_char_id = sx_touint(value);
            for (int i = 0; i < num_glyphs; i++) {
                if (glyphs[i].id != first_char_id)
                    continue;
                first_glyph_idx = i;
                break;
            }
        } else if (strcmp(key, "second") == 0) {
            k->second_id = sx_touint(value);
        } else if (strcmp(key, "amount") == 0) {
            k->amount = (float)sx_toint(value);
        }
        token = (char*)sx_skip_whitespace(token);
    }

    sx_assert(first_glyph_idx != -1);
    if (glyphs[first_glyph_idx].num_kerns == 0)
        glyphs[first_glyph_idx].kern_idx = kern_idx;
    ++glyphs[first_glyph_idx].num_kerns;
}

static bool rizz__fnt_text_check(char* buff, int len)
{
    for (int i = 0; i < len; i++) {
        if (buff[i] <= 0)
            return false;
    }
    return true;
}

static rizz_asset_load_data rizz__font_on_prepare(const rizz_asset_load_params* params,
                                                  const void* metadata)
{
    const sx_alloc* alloc = params->alloc ? params->alloc : g_gfx_alloc;
    const rizz__font_metadata* meta = metadata;
    sx_assert(meta);

    int hashtbl_cap = sx_hashtbl_valid_capacity(meta->num_glyphs);
    int total_sz = sizeof(rizz__font) + meta->num_glyphs * sizeof(rizz__font_glyph) +
                   meta->num_kerns * sizeof(rizz__font_glyph_kern) +
                   hashtbl_cap * (sizeof(uint32_t) + sizeof(int));
    rizz__font* font = sx_malloc(alloc, total_sz);
    if (!font) {
        sx_out_of_memory();
        return (rizz_asset_load_data){ .obj = { 0 } };
    }
    sx_memset(font, 0x0, total_sz);

    // initialize font buffers/hashtbl and texture
    uint8_t* buff = (uint8_t*)(font + 1);
    font->glyphs = (rizz__font_glyph*)buff;
    buff += sizeof(rizz__font_glyph) * meta->num_glyphs;
    font->kerns = (rizz__font_glyph_kern*)buff;
    buff += sizeof(rizz__font_glyph_kern) * meta->num_kerns;
    uint32_t* keys = (uint32_t*)buff;
    buff += sizeof(uint32_t) * hashtbl_cap;
    int* values = (int*)buff;

    sx_hashtbl_init(&font->glyph_tbl, hashtbl_cap, keys, values);

    rizz_texture_load_params tparams = { 0 };
    font->f.img = the__asset.load("texture", meta->img_filepath, &tparams, params->flags, alloc,
                                  params->tags);

    return (rizz_asset_load_data){ .obj = { .ptr = font } };
}

static bool rizz__font_on_load(rizz_asset_load_data* data, const rizz_asset_load_params* params,
                               const sx_mem_block* mem)
{
    const sx_alloc* tmp_alloc = the__core.tmp_alloc_push();

    rizz__font* font = data->obj.ptr;
    sx_mem_reader rd;
    sx_mem_init_reader(&rd, mem->data, mem->size);

    // read first 3-bytes and check if we have binary or (probably) text formats
    char sign_str[4];
    sx_mem_read(&rd, sign_str, 3);
    sign_str[3] = '\0';
    if (sx_strequal(sign_str, RIZZ__FNT_SIGN)) {
        // Load binary
        uint8_t file_ver;
        sx_mem_read(&rd, &file_ver, sizeof(file_ver));
        if (file_ver != 3) {
            rizz__log_warn("loading font '%s' failed: invalid file version", params->path);
            return false;
        }

        //
        rizz__fnt_block block;
        rizz__fnt_info info;
        rizz__fnt_common common;
        sx_mem_read(&rd, &block, sizeof(block));
        sx_assert(block.id == 1);
        sx_mem_read(&rd, &info, sizeof(info));
        sx_mem_read(&rd, font->f.name,
                    (int)sx_min(sizeof(font->f.name), block.size - sizeof(info)));

        // Common
        sx_mem_read(&rd, &block, sizeof(block));
        sx_assert(block.id == 2);
        sx_mem_read(&rd, &common, sizeof(common));

        if (common.pages > 1) {
            rizz__log_warn("loading font '%s' failed: should contain only 1 page", params->path);
            return false;
        }

        // Font pages (textures)
        rizz__fnt_page page;
        char dirname[RIZZ_MAX_PATH];
        char img_filepath[RIZZ_MAX_PATH];
        sx_os_path_dirname(dirname, sizeof(dirname), params->path);

        sx_mem_read(&rd, &block, sizeof(block));
        sx_assert(block.id == 3);
        sx_mem_read(&rd, page.path, block.size);
        sx_os_path_join(img_filepath, sizeof(img_filepath), dirname, page.path);

        // Characters (glyphs)
        sx_mem_read(&rd, &block, sizeof(block));
        sx_assert(block.id == 4);
        int num_glyphs = block.size / sizeof(rizz__fnt_char);
        rizz__fnt_char* chars = sx_malloc(tmp_alloc, sizeof(rizz__fnt_char) * num_glyphs);
        sx_assert(chars);
        sx_mem_read(&rd, chars, block.size);

        // Kernings
        int last_r = rd.pos != rd.top ? sx_mem_read(&rd, &block, sizeof(block)) : 0;
        int num_kerns = block.size / sizeof(rizz__fnt_kern);
        rizz__fnt_kern* kerns = NULL;
        if (block.id == 5 && num_kerns > 0 && last_r == sizeof(block)) {
            kerns = sx_malloc(tmp_alloc, sizeof(rizz__fnt_kern) * num_kerns);
            sx_assert(kerns);
            sx_mem_read(&rd, kerns, block.size);
        }

        // Create font
        font->f.size = info.font_size;
        font->f.line_height = common.line_height;
        font->f.base = common.base;
        font->f.img_width = common.scale_w;
        font->f.img_height = common.scale_h;
        font->f.num_glyphs = num_glyphs;
        font->f.num_kerns = num_kerns;

        float img_width = (float)common.scale_w;
        float img_height = (float)common.scale_h;

        // image is already loaded in on_prepare
        // glyph_tbl is already initialized in on_prepare
        int16_t char_width = 0;
        for (int i = 0; i < num_glyphs; i++) {
            const rizz__fnt_char* ch = &chars[i];
            font->glyphs[i].id = ch->id;
            font->glyphs[i].uvs =
                sx_rectwh((float)ch->x / img_width, (float)ch->y / img_height,
                          (float)ch->width / img_width, (float)ch->height / img_height);
            font->glyphs[i].xadvance = (float)ch->xadvance;
            font->glyphs[i].xoffset = (float)ch->xoffset;
            font->glyphs[i].yoffset = (float)ch->yoffset;

            sx_hashtbl_add(&font->glyph_tbl, ch->id, i);

            char_width = sx_max(char_width, ch->xadvance);
        }
        font->f.char_width = char_width;

        if (num_kerns > 0) {
            for (int i = 0; i < num_kerns; i++) {
                const rizz__fnt_kern* kern = &kerns[i];
                // Find char and set it's kerning index
                uint32_t id = kern->first;
                for (int k = 0; k < num_glyphs; k++) {
                    if (id == font->glyphs[k].id) {
                        if (font->glyphs[k].num_kerns == 0)
                            font->glyphs[k].kern_idx = i;
                        ++font->glyphs[k].num_kerns;
                        break;
                    }
                }

                font->kerns[i].second_id = kern->second;
                font->kerns[i].amount = (float)kern->amount;
            }
        }
    } else {
        // text
        // read line by line and parse needed information
        char* buff = sx_malloc(tmp_alloc, (size_t)mem->size + 1);
        if (!buff) {
            the__core.tmp_alloc_pop();
            sx_out_of_memory();
            return false;
        }
        sx_memcpy(buff, mem->data, mem->size);
        buff[mem->size] = '\0';

        if (!rizz__fnt_text_check(buff, mem->size)) {
            rizz__log_warn("loading font '%s' failed: not a valid fnt text file", params->path);
            the__core.tmp_alloc_pop();
            return false;
        }

        // check if it's character
        char* eol;
        char img_filepath[RIZZ_MAX_PATH];
        char line[1024];
        int char_idx = 0;
        int kern_idx = 0;

        while ((eol = (char*)sx_strchar(buff, '\n'))) {
            sx_strncpy(line, sizeof(line), buff, (int)(intptr_t)(eol - buff));
            char name[32];
            char* name_end = (char*)sx_skip_word(line);
            sx_assert(name_end);
            sx_strncpy(name, sizeof(name), line, (int)(intptr_t)(name_end - line));
            if (sx_strequal(name, "info")) {
                rizz__fnt_text_read_info(name_end, &font->f);
            } else if (sx_strequal(name, "common")) {
                rizz__fnt_text_read_common(name_end, &font->f);
            } else if (sx_strequal(name, "chars")) {
                font->f.num_glyphs = rizz__fnt_text_read_count(name_end);
            } else if (sx_strequal(name, "kernings")) {
                font->f.num_kerns = rizz__fnt_text_read_count(name_end);
            } else if (sx_strequal(name, "page")) {
                rizz__fnt_text_read_page(name_end, img_filepath, sizeof(img_filepath),
                                         params->path);
            } else if (sx_strequal(name, "char")) {
                // image is already loaded in on_prepare
                // glyph_tbl is already initialized in on_prepare
                rizz__fnt_text_read_char(name_end, &font->glyphs[char_idx], &font->f.char_width,
                                         (float)font->f.img_width, (float)font->f.img_height);
                sx_hashtbl_add(&font->glyph_tbl, font->glyphs[char_idx].id, char_idx);
                char_idx++;
            } else if (sx_strequal(name, "kerning")) {
                rizz__fnt_text_read_kern(name_end, &font->kerns[kern_idx], kern_idx, font->glyphs,
                                         font->f.num_glyphs);
                kern_idx++;
            }

            buff = (char*)sx_skip_whitespace(eol);
        }
    }    // if (binary)

    the__core.tmp_alloc_pop();
    return true;
}

static void rizz__font_on_finalize(rizz_asset_load_data* data, const rizz_asset_load_params* params,
                                   const sx_mem_block* mem)
{
    sx_unused(data);
    sx_unused(params);
    sx_unused(mem);
}

static void rizz__font_on_reload(rizz_asset handle, rizz_asset_obj prev_obj, const sx_alloc* alloc)
{
    sx_unused(handle);
    sx_unused(prev_obj);
    sx_unused(alloc);
}

static void rizz__font_on_release(rizz_asset_obj obj, const sx_alloc* alloc)
{
    rizz__font* font = obj.ptr;
    sx_assert(font);

    if (!alloc)
        alloc = g_gfx_alloc;

    if (font->f.img.id)
        the__asset.unload(font->f.img);

    sx_free(alloc, font);
}

static void rizz__font_on_read_metadata(void* metadata, const rizz_asset_load_params* params,
                                        const sx_mem_block* mem)
{
    rizz__font_metadata* meta = metadata;
    sx_mem_reader rd;
    sx_mem_init_reader(&rd, mem->data, mem->size);

    // read first 3-bytes and check if we have binary or (probably) text formats
    char sign_str[4];
    sx_mem_read(&rd, sign_str, 3);
    sign_str[3] = '\0';
    if (sx_strequal(sign_str, RIZZ__FNT_SIGN)) {
        uint8_t file_ver;
        sx_mem_read(&rd, &file_ver, sizeof(file_ver));
        if (file_ver != 3) {
            rizz__log_warn("loading font '%s' failed: invalid file version", params->path);
            return;
        }

        rizz__fnt_block block;
        rizz__fnt_info info;
        rizz__fnt_common common;
        char name[256];

        sx_mem_read(&rd, &block, sizeof(block));
        sx_assert(block.id == 1);
        sx_mem_read(&rd, &info, sizeof(info));
        sx_mem_read(&rd, name, block.size - sizeof(info));

        sx_mem_read(&rd, &block, sizeof(block));
        sx_assert(block.id == 2);
        sx_mem_read(&rd, &common, sizeof(common));
        if (common.pages > 1) {
            rizz__log_warn("loading font '%s' failed: should contain only 1 page", params->path);
            return;
        }

        // Font pages (textures)
        rizz__fnt_page page;
        char dirname[RIZZ_MAX_PATH];
        sx_os_path_dirname(dirname, sizeof(dirname), params->path);
        sx_mem_read(&rd, &block, sizeof(block));
        sx_assert(block.id == 3);
        sx_mem_read(&rd, page.path, block.size);
        sx_os_path_join(meta->img_filepath, sizeof(meta->img_filepath), dirname, page.path);
        sx_os_path_unixpath(meta->img_filepath, sizeof(meta->img_filepath), meta->img_filepath);

        meta->img_height = common.scale_w;
        meta->img_height = common.scale_h;

        sx_mem_read(&rd, &block, sizeof(block));
        sx_assert(block.id == 4);
        sx_mem_seekr(&rd, block.size, SX_WHENCE_CURRENT);
        meta->num_glyphs = block.size / sizeof(rizz__fnt_char);

        // Kernings
        int r = rd.pos != rd.top ? sx_mem_read(&rd, &block, sizeof(block)) : 0;
        if (block.id == 5 && r == sizeof(block))
            meta->num_kerns = block.size / sizeof(rizz__fnt_kern);
    } else {
        // text
        // read line by line and parse needed information
        const sx_alloc* tmp_alloc = the__core.tmp_alloc_push();
        char* buff = sx_malloc(tmp_alloc, (size_t)mem->size + 1);
        if (!buff) {
            sx_out_of_memory();
            return;
        }
        sx_memcpy(buff, mem->data, mem->size);
        buff[mem->size] = '\0';

        if (!rizz__fnt_text_check(buff, mem->size)) {
            rizz__log_warn("loading font '%s' failed: not a valid fnt text file", params->path);
            return;
        }

        // check if it's character
        char* eol;
        rizz_font fnt = { .name = { 0 } };
        char img_filepath[RIZZ_MAX_PATH];
        char line[1024];
        while ((eol = (char*)sx_strchar(buff, '\n'))) {
            sx_strncpy(line, sizeof(line), buff, (int)(intptr_t)(eol - buff));
            char name[32];
            char* name_end = (char*)sx_skip_word(line);
            sx_assert(name_end);
            sx_strncpy(name, sizeof(name), line, (int)(intptr_t)(name_end - line));
            if (sx_strequal(name, "info"))
                rizz__fnt_text_read_info(name_end, &fnt);
            else if (sx_strequal(name, "common"))
                rizz__fnt_text_read_common(name_end, &fnt);
            else if (sx_strequal(name, "chars"))
                fnt.num_glyphs = rizz__fnt_text_read_count(name_end);
            else if (sx_strequal(name, "kernings"))
                fnt.num_kerns = rizz__fnt_text_read_count(name_end);
            else if (sx_strequal(name, "page"))
                rizz__fnt_text_read_page(name_end, img_filepath, sizeof(img_filepath),
                                         params->path);

            buff = (char*)sx_skip_whitespace(eol);
        }

        sx_strcpy(meta->img_filepath, sizeof(meta->img_filepath), img_filepath);
        meta->img_width = fnt.img_width;
        meta->img_height = fnt.img_height;
        meta->num_glyphs = fnt.num_glyphs;
        meta->num_kerns = fnt.num_kerns;

        the__core.tmp_alloc_pop();
    }
}

static void rizz__font_init()
{
    rizz__refl_field(rizz__font_metadata, char[RIZZ_MAX_PATH], img_filepath, "img_filepath");
    rizz__refl_field(rizz__font_metadata, int, img_width, "img_width");
    rizz__refl_field(rizz__font_metadata, int, img_height, "img_height");
    rizz__refl_field(rizz__font_metadata, int, num_glyphs, "num_glyphs");
    rizz__refl_field(rizz__font_metadata, int, num_kerns, "num_kerns");

    // TODO: create default async/fail objects

    the__asset.register_asset_type(
        "font",
        (rizz_asset_callbacks){ .on_prepare = rizz__font_on_prepare,
                                .on_load = rizz__font_on_load,
                                .on_finalize = rizz__font_on_finalize,
                                .on_reload = rizz__font_on_reload,
                                .on_release = rizz__font_on_release,
                                .on_read_metadata = rizz__font_on_read_metadata },
        NULL, 0, "rizz__font_metadata", sizeof(rizz__font_metadata),
        (rizz_asset_obj){ .ptr = NULL }, (rizz_asset_obj){ .ptr = NULL }, 0);
}

static const rizz_font* rizz__font_get(rizz_asset font_asset)
{
    return (const rizz_font*)the__asset.obj(font_asset).ptr;
}

#endif    // bmfont