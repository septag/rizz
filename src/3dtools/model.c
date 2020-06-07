#include "rizz/3dtools.h"

#include "rizz/rizz.h"
#include "sx/allocator.h"
#include "sx/string.h"
#include "sx/os.h"

#define SX_MAX_BUFFER_FIELDS 128
#include "sx/linear-buffer.h"
#undef SX_MAX_BUFFERS_FIELDS

#include <alloca.h>

#define CGLTF_IMPLEMENTATION
#include "../3rdparty/cgltf/cgltf.h"

RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_asset* the_asset;
RIZZ_STATE static rizz_api_gfx* the_gfx;

typedef struct model__vertex_attribute 
{
    const char* semantic;
    int index;
} model__vertex_attribute;

#if 0
static void debug_model(const rizz_model* model)
{
    printf("nodes: %d\n", model->num_nodes);
    for (int i = 0; i < model->num_nodes; i++) {
        rizz_model_node* node = &model->nodes[i];
        printf("\t%s\n", node->name);
    }

    printf("meshes: %d\n", model->num_meshes);
    for (int i = 0; i < model->num_meshes; i++) {
        rizz_model_mesh* mesh = &model->meshes[i];
        for (int sm = 0; sm < mesh->num_submeshes; sm++) {
            rizz_model_submesh* submesh = &mesh->submeshes[sm];
            printf("\tsubmesh: %d (start_index=%d, num_indices=%d)\n", 
                sm+1, 
                submesh->start_index, 
                submesh->num_indices);
        }

        int num_verts = mesh->num_vertices;
        vertex_stream1* vstr1 = (vertex_stream1*)mesh->cpu.vbuffs[0];
        vertex_stream2* vstr2 = (vertex_stream2*)mesh->cpu.vbuffs[1];
        vertex_stream3* vstr3 = (vertex_stream3*)mesh->cpu.vbuffs[2];
        for (int v = 0; v < num_verts; v++) {
            printf("\tVertex: %d\n", v+1);
            printf("\t\tPos: %.3f, %.3f, %.3f\n", vstr1[v].pos.x, vstr1[v].pos.y, vstr1[v].pos.z);
            printf("\t\tNormal: %.3f, %.3f, %.3f\n", vstr2[v].normal.x, vstr2[v].normal.y, vstr2[v].normal.z);
            printf("\t\tTangent: %.3f, %.3f, %.3f\n", vstr2[v].tangent.x, vstr2[v].tangent.y, vstr2[v].tangent.z);
            printf("\t\tUv: %.3f, %.3f\n", vstr3[v].uv.x, vstr3[v].uv.y);
        }

        printf("\tIndices: %d (tris=%d)\n", mesh->num_indices, mesh->num_indices/3);
        uint16_t* indices = (uint16_t*)mesh->cpu.ibuff;
        for (int idx = 0; idx < mesh->num_indices; idx+=3) {
            printf("\t\tTri(%d): %d %d %d\n", (idx/3) + 1, indices[idx], indices[idx+1], indices[idx+2]);
        }
    }
}
#endif

static model__vertex_attribute model__convert_attribute(cgltf_attribute_type type, int index)
{
    if (type == cgltf_attribute_type_position && index == 0)  {
        return (model__vertex_attribute) { .semantic = "POSITION", .index = 0 };
    } else if (type == cgltf_attribute_type_normal && index == 0) {
        return (model__vertex_attribute) { .semantic = "NORMAL", .index = 0 };
    } else if (type == cgltf_attribute_type_tangent && index == 0) {
        return (model__vertex_attribute) { .semantic = "TANGENT", .index = 0 };
    } else if (type == cgltf_attribute_type_texcoord) {
        switch (index) {
        case 0:     return (model__vertex_attribute) { .semantic = "TEXCOORD", .index = 0 };
        case 1:     return (model__vertex_attribute) { .semantic = "TEXCOORD", .index = 1 };
        case 2:     return (model__vertex_attribute) { .semantic = "TEXCOORD", .index = 2 };
        case 3:     return (model__vertex_attribute) { .semantic = "TEXCOORD", .index = 3 };
        default:    return (model__vertex_attribute) { 0 };
        }
    } else if (type == cgltf_attribute_type_color) {
        switch (index) {
        case 0:     return (model__vertex_attribute) { .semantic = "COLOR", .index = 0 };
        case 1:     return (model__vertex_attribute) { .semantic = "COLOR", .index = 0 };
        case 2:     return (model__vertex_attribute) { .semantic = "COLOR", .index = 0 };
        case 3:     return (model__vertex_attribute) { .semantic = "COLOR", .index = 0 };
        default:    return (model__vertex_attribute) { 0 };
        }
    } else if (type == cgltf_attribute_type_joints && index == 0) {
        return (model__vertex_attribute) { .semantic = "BLENDINDICES", .index = 0 };
    } else if (type == cgltf_attribute_type_weights && index == 0) {
        return (model__vertex_attribute) { .semantic = "BLENDWEIGHT", .index = 0 };
    } else {
        return (model__vertex_attribute) { 0 };
    }
}

static int model__get_stride(sg_vertex_format fmt)
{
    switch (fmt) {
    case SG_VERTEXFORMAT_FLOAT:     return sizeof(float);
    case SG_VERTEXFORMAT_FLOAT2:    return sizeof(float)*2;
    case SG_VERTEXFORMAT_FLOAT3:    return sizeof(float)*3;
    case SG_VERTEXFORMAT_FLOAT4:    return sizeof(float)*4;
    case SG_VERTEXFORMAT_BYTE4:     return sizeof(int8_t)*4;
    case SG_VERTEXFORMAT_BYTE4N:    return sizeof(int8_t)*4;
    case SG_VERTEXFORMAT_UBYTE4:    return sizeof(uint8_t)*4;
    case SG_VERTEXFORMAT_UBYTE4N:   return sizeof(uint8_t)*4;
    case SG_VERTEXFORMAT_SHORT2:    return sizeof(int16_t)*2;
    case SG_VERTEXFORMAT_SHORT2N:   return sizeof(int16_t)*2;
    case SG_VERTEXFORMAT_USHORT2N:  return sizeof(uint16_t)*2;
    case SG_VERTEXFORMAT_SHORT4:    return sizeof(int16_t)*4;
    case SG_VERTEXFORMAT_SHORT4N:   return sizeof(int16_t)*4;
    case SG_VERTEXFORMAT_USHORT4N:  return sizeof(uint16_t)*4;
    case SG_VERTEXFORMAT_UINT10_N2: return 2;
    default:                        return 0;
    }
}

static void model__map_attributes_to_buffer(rizz_model_mesh* mesh, 
                                            const rizz_model_geometry_layout* vertex_layout, 
                                            cgltf_attribute* srcatt)
{
    cgltf_accessor* access = srcatt->data;
    const rizz_vertex_attr* attr = &vertex_layout->attrs[0];
    model__vertex_attribute mapped_att = model__convert_attribute(srcatt->type, srcatt->index);
    while (attr->semantic) {
        if (sx_strequal(attr->semantic, mapped_att.semantic) && attr->semantic_idx == mapped_att.index) {
            int vertex_stride = vertex_layout->buffer_strides[attr->buffer_index];
            uint8_t* src_buff = (uint8_t*)access->buffer_view->buffer->data;
            uint8_t* dst_buff = (uint8_t*)mesh->cpu.vbuffs[attr->buffer_index];
            int dst_offset = vertex_layout->buffer_strides[attr->buffer_index] * mesh->num_vertices + attr->offset;
            int src_offset = access->offset + access->buffer_view->offset;

            int count = access->count;
            int src_data_size = access->stride; 
            int dst_data_size = model__get_stride(attr->format);
            sx_assert(dst_data_size != 0 && "you must explicitly declare formats for vertex_layout attributes");
            int stride = sx_min(dst_data_size, src_data_size);
            for (int i = 0; i < count; i++) {
                sx_memcpy(dst_buff + dst_offset + vertex_stride*i, 
                          src_buff + src_offset + src_data_size*i, 
                          stride);
            }

            break;
        }
        ++attr;
    }
}

static void model__setup_buffers(rizz_model_mesh* mesh, const rizz_model_geometry_layout* vertex_layout, 
                                 cgltf_mesh* srcmesh, const sx_alloc* alloc)
{
    // create buffers based on input vertex_layout
    int num_vbuffs = 0;
    int num_vertices = 0;
    int num_indices = 0;

    // count total vertices and indices
    for (int i = 0; i < srcmesh->primitives_count; i++) {
        cgltf_primitive* srcprim = &srcmesh->primitives[i];
        int count = 0;
        for (int k = 0; k < srcprim->attributes_count; k++) {
            cgltf_attribute* srcatt = &srcprim->attributes[k];
            if (count == 0) {
                count = srcatt->data->count;
            }
            sx_assert(count == srcatt->data->count);
        }
        num_vertices += count;
        num_indices += srcprim->indices->count;
    }

    sx_assert(num_vertices > 0 && num_indices > 0);

    // create vertex and index buffers
    sg_index_type index_type = SG_INDEXTYPE_NONE;
    while (vertex_layout->buffer_strides[num_vbuffs]) {
        int buff_stride = vertex_layout->buffer_strides[num_vbuffs];
        mesh->cpu.vbuffs[num_vbuffs] = sx_malloc(alloc, buff_stride*num_vertices);
        if (!mesh->cpu.vbuffs[num_vbuffs]) {
            sx_out_of_memory();
            return;
        }
        sx_memset(mesh->cpu.vbuffs[num_vbuffs], 0x0, buff_stride*num_vertices);
        num_vbuffs++;
    }
    sx_assert(num_vbuffs && "you must at least set one buffer in the `vertex_layout`");
    mesh->num_vbuffs = num_vbuffs;

    index_type = (num_vertices < UINT16_MAX) ? SG_INDEXTYPE_UINT16 : SG_INDEXTYPE_UINT32;
    int index_stride = index_type == SG_INDEXTYPE_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t);
    mesh->cpu.ibuff = sx_malloc(alloc, index_stride*num_indices);
    if (!mesh->cpu.ibuff) {
        sx_out_of_memory();
        return;
    }
    sx_memset(mesh->cpu.ibuff, 0x0, num_indices * index_stride);
    mesh->index_type = index_type;

    // map source vertex buffer to our data
    // map source index buffer to our data
    int start_index = 0;
    for (int i = 0; i < srcmesh->primitives_count; i++) {
        cgltf_primitive* srcprim = &srcmesh->primitives[i];

        // vertices
        int count = 0;
        for (int k = 0; k < srcprim->attributes_count; k++) {
            cgltf_attribute* srcatt = &srcprim->attributes[k];
            model__map_attributes_to_buffer(mesh, vertex_layout, srcatt);
            if (count == 0) {
                count = srcatt->data->count;
            }
            sx_assert(count == srcatt->data->count);
        }

        // indices
        cgltf_accessor* srcindices = srcprim->indices;
        int start_vertex = mesh->num_vertices;
        if (srcindices->component_type == cgltf_component_type_r_16u && index_type == SG_INDEXTYPE_UINT16) {
            uint16_t* indices = (uint16_t*)mesh->cpu.ibuff + start_index;
            uint16_t* _srcindices = (uint16_t*)((uint8_t*)srcindices->buffer_view->buffer->data + 
                                    srcindices->buffer_view->offset);
            for (int k = 0; k < srcindices->count; k++) {
                indices[k] = _srcindices[k] + start_vertex;
            }
        } else if (srcindices->component_type == cgltf_component_type_r_16u && index_type == SG_INDEXTYPE_UINT32) {
            uint32_t* indices = (uint32_t*)mesh->cpu.ibuff + start_index;
            uint16_t* _srcindices = (uint16_t*)srcindices->buffer_view->buffer->data;
            for (int k = 0; k < srcindices->count; k++) {
                indices[k] = (uint32_t)(_srcindices[k] + start_vertex);
            }
        } else if (srcindices->component_type == cgltf_component_type_r_32u && index_type == SG_INDEXTYPE_UINT32) {
            uint32_t* indices = (uint32_t*)mesh->cpu.ibuff + start_index;
            uint32_t* _srcindices = (uint32_t*)srcindices->buffer_view->buffer->data;
            for (int k = 0; k < srcindices->count; k++) {
                indices[k] = _srcindices[k] + (uint32_t)start_vertex;
            }
        }

        rizz_model_submesh* submesh = &mesh->submeshes[i];
        submesh->start_index = start_index;
        submesh->num_indices = srcprim->indices->count;
        start_index += srcprim->indices->count;

        mesh->num_vertices += count;
        mesh->num_indices += srcprim->indices->count;
    }
}

static bool model__setup_gpu_buffers(rizz_model* model) 
{
    rizz_model_geometry_layout* layout = &model->layout;
    for (int i = 0; i < model->num_meshes; i++) {
        rizz_model_mesh* mesh = &model->meshes[i];

        int buffer_index = 0;
        while (layout->buffer_strides[buffer_index]) {
            mesh->gpu.vbuffs[buffer_index] = the_gfx->make_buffer(&(sg_buffer_desc) {
                .size = layout->buffer_strides[buffer_index]*mesh->num_vertices,
                .type = SG_BUFFERTYPE_VERTEXBUFFER,
                .content = mesh->cpu.vbuffs[buffer_index]
            });
            if (!mesh->gpu.vbuffs[buffer_index].id) {
                return false;
            }
            buffer_index++;
        }

        mesh->gpu.ibuff = the_gfx->make_buffer(&(sg_buffer_desc) {
            .size = ((mesh->index_type == SG_INDEXTYPE_UINT16) ? sizeof(uint16_t) : sizeof(uint32_t)) * mesh->num_indices,
            .type = SG_BUFFERTYPE_INDEXBUFFER,
            .content = mesh->cpu.ibuff
        });

        if (!mesh->gpu.ibuff.id) {
            return false;
        }
    }

    return true;
}


static int model__find_node_byname(const rizz_model* model, const char* name)
{
    for (int ni = 0; ni < model->num_nodes; ni++) {
        if (sx_strequal(model->nodes[ni].name, name)) {
            return ni;
        }
    }
    return -1;
}


static const rizz_vertex_attr* model__find_attribute(const rizz_model_geometry_layout* layout, 
                                                     const char* semantic, int semantic_index)
{
    const rizz_vertex_attr* attr = &layout->attrs[0];
    while (attr->semantic) {
        if (sx_strequal(attr->semantic, semantic) && attr->semantic_idx == semantic_index) {
            return attr;
        }
        ++attr;
    }
    return NULL;
}

static rizz_model* load_model(const char* filepath, const rizz_model_geometry_layout* vertex_layout)
{
    cgltf_options options = { .type = cgltf_file_type_glb };
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse(&options, mem->data, mem->size, &data);
    if (result != cgltf_result_success) {
        return false;
    }
    result = cgltf_load_buffers(&options, data, NULL);
    if (result != cgltf_result_success) {
        return false;
    }

    if (data->nodes_count == 0) {
        return false;
    }

    rizz_model* model = sx_malloc(alloc, sizeof(rizz_model));
    sx_memset(model, 0x0, sizeof(rizz_model));
    model->bounds = sx_aabb_empty();

    model->nodes = sx_malloc(alloc, sizeof(rizz_model_node)*data->nodes_count);
    model->num_nodes = data->nodes_count;
    sx_memset(model->nodes, 0x0, sizeof(rizz_model_node)*data->nodes_count);

    model->meshes = sx_malloc(alloc, sizeof(rizz_model_mesh)*data->meshes_count);
    model->num_meshes = data->meshes_count;
    sx_memset(model->meshes, 0x0, sizeof(rizz_model_mesh)*data->meshes_count);

    // meshes
    for (int i = 0; i < data->meshes_count; i++) {
        rizz_model_mesh* mesh = &model->meshes[i];
        cgltf_mesh* _mesh = &data->meshes[i];

        mesh->submeshes = sx_malloc(alloc, sizeof(rizz_model_submesh)*_mesh->primitives_count);
        mesh->num_submeshes = _mesh->primitives_count;

        model__setup_buffers(mesh, vertex_layout, _mesh, alloc);
    }

    // nodes
    for (int i = 0; i < data->nodes_count; i++) {
        rizz_model_node* node = &model->nodes[i];
        cgltf_node* _node = &data->nodes[i];

        node->local_tx = sx_tx3d_ident();

        sx_strcpy(node->name, sizeof(node->name), _node->name);
        if (sx_strlen(node->name) != sx_strlen(_node->name)) {
            rizz_log_warn("model: %s, node: '%s' - name is too long (more than 31 characters), "
                          "node setup will probably have errors", filepath, _node->name);
        }

        if (_node->has_scale) {
            // TODO: apply scaling to the model
            sx_assert(0 && "not supported yet. Apply scaling in DCC before exporting");
        }

        if (_node->has_rotation) {
            sx_mat4 rot_mat = sx_quat_mat4(sx_quat4fv(_node->rotation));
            node->local_tx.rot = sx_mat3fv(rot_mat.col1.f, rot_mat.col2.f, rot_mat.col3.f);
        }

        if (_node->has_translation) {
            node->local_tx.pos = sx_vec3fv(_node->translation);
        }

        // assign mesh, find the index of the pointer. it is as same as 
        node->mesh_id = -1;
        for (int mi = 0; mi < data->meshes_count; mi++) {
            if (&data->meshes[mi] == _node->mesh) {
                node->mesh_id = mi;
                break;
            }
        }

        // bounds
        sx_aabb bounds = sx_aabb_empty();
        if (node->mesh_id != -1) {
            rizz_model_mesh* mesh = &model->meshes[node->mesh_id];
            const rizz_vertex_attr* attr = model__find_attribute(vertex_layout, "POSITION", 0);
            int vertex_stride = vertex_layout->buffer_strides[attr->buffer_index];
            uint8_t* vbuff = mesh->cpu.vbuffs[attr->buffer_index];
            for (int v = 0; v < mesh->num_vertices; v++) {
                sx_vec3 pos = *((sx_vec3*)(vbuff + v*vertex_stride + attr->offset));
                sx_aabb_add_point(&bounds, pos);
            }

            model->bounds = sx_aabb_add(&model->bounds, &bounds);
        }
        node->bounds = bounds;
    }

    // build node hierarchy based on node names
    for (int i = 0; i < data->nodes_count; i++) {
        rizz_model_node* node = &model->nodes[i];
        cgltf_node* _node = &data->nodes[i];

        if (_node->parent) {
            node->parent_id = model__find_node_byname(model, _node->parent->name);
        } else {
            node->parent_id = -1;
        }

        node->num_childs = _node->children_count;
        if (_node->children_count > 0) {
            node->children = sx_malloc(alloc, _node->children_count*sizeof(int));
            for (int ci = 0; ci < _node->children_count; ci++) {
                node->children[ci] = model__find_node_byname(model, _node->children[ci]->name);
            }
        } 
    }

    sx_memcpy(&model->layout, vertex_layout, sizeof(rizz_model_geometry_layout));

    debug_model(model);

    sx_mem_destroy_block(mem);
    cgltf_free(data);

    model__setup_gpu_buffers(model);

    *pmodel = model;
    return true;
}


typedef struct rizz_model_context 
{
    const sx_alloc* alloc;
    rizz_model blank_model;
    rizz_model failed_model;
    rizz_model_geometry_layout default_layout;
} rizz_model_context;

RIZZ_STATE static rizz_model_context g_model;

static void* model__cgltf_alloc(void* user, cgltf_size size)
{
    const sx_alloc* alloc = user;
    return sx_malloc(alloc, size);
}

static void model__cgltf_free(void* user, void* ptr)
{
    const sx_alloc* alloc = user;
    sx_free(alloc, ptr);
}

static rizz_asset_load_data model__on_prepare(const rizz_asset_load_params* params, const sx_mem_block* mem)
{
    const sx_alloc* alloc = params->alloc ? params->alloc : g_model.alloc;
    const rizz_model_load_params* lparams = params->params;
    const rizz_model_geometry_layout* layout = lparams->layout.buffer_strides[0] > 0 ? 
        &lparams->layout : &g_model.default_layout;

    char ext[32];
    sx_os_path_ext(ext, sizeof(ext), params->path);
    if (sx_strequalnocase(ext, "glb")) {
        cgltf_options options = {
            .type = cgltf_file_type_glb,
            .memory = {
                .alloc = model__cgltf_alloc,
                .free = model__cgltf_free,
                .user_data = (void*)g_model.alloc
            }
        };
        cgltf_data* data;
        cgltf_result result = cgltf_parse(&options, mem->data, mem->size, &data);
        if (result != cgltf_result_success) {
            rizz_log_warn("cannot parse GLTF file: %s", params->path);
            return (rizz_asset_load_data) { 0 };
        }

        // allocate memory
        sx_linear_buffer buff;
        sx_linear_buffer_init(&buff, rizz_model, 0);
        sx_linear_buffer_addtype(&buff, rizz_model, rizz_model_node, nodes, data->nodes_count, 0);
        sx_linear_buffer_addtype(&buff, rizz_model, rizz_model_mesh, meshes, data->meshes_count, 0);
        rizz_model_mesh* tmp_meshes = alloca(sizeof(rizz_model_mesh)*data->meshes_count);
        int** tmp_children = alloca(sizeof(int*)*data->nodes_count);
        sx_assert_rel(tmp_meshes && tmp_children);
        sx_memset(tmp_children, 0x0, sizeof(int*)*data->nodes_count);
        sx_memset(tmp_meshes, 0x0, sizeof(rizz_model_mesh)*data->meshes_count);
        
        // allocate space for buffers and assign them later
        for (int i = 0; i < data->nodes_count; i++) {
            cgltf_node* node = &data->nodes[i];
            if (node->children_count) {
                sx_linear_buffer_addptr(&buff, &tmp_children[i], int, node->children_count, 0);
            }
        }

        for (int i = 0; i < data->meshes_count; i++) {
            cgltf_mesh* mesh = &data->meshes[i];
            sg_index_type index_type = SG_INDEXTYPE_NONE;
        
            sx_linear_buffer_addptr(&buff, &tmp_meshes[i].submeshes, rizz_model_submesh, mesh->primitives_count, 0);

            int num_vertices = 0;
            int num_indices = 0;
            for (int k = 0; k < mesh->primitives_count; k++) {
                cgltf_primitive* prim = &mesh->primitives[k];
                int count = 0;
                for (int aa = 0; aa < prim->attributes_count; aa++) {
                    cgltf_attribute* srcatt = &prim->attributes[aa];
                    if (count == 0) {
                        count = srcatt->data->count;
                    }
                    sx_assert(count == srcatt->data->count);
                }
                num_vertices += count;
                num_indices += mesh->primitives[k].indices->count;
            }
            sx_assert_rel(num_vertices > 0 && num_indices > 0);
            tmp_meshes[i].num_vertices = num_vertices;
            tmp_meshes[i].num_indices = num_indices;

            // buffers
            int buffer_index = 0;
            while (layout->buffer_strides[buffer_index] > 0) {
                int vertex_size = layout->buffer_strides[buffer_index];
                sx_linear_buffer_addptr(&buff, &tmp_meshes[i].cpu.vbuffs[buffer_index], uint8_t, 
                                        num_vertices*vertex_size, 0);
                buffer_index++;
            }
            tmp_meshes[i].num_vbuffs = buffer_index;

            index_type = (num_vertices < UINT16_MAX) ? SG_INDEXTYPE_UINT16 : SG_INDEXTYPE_UINT32;
            int index_stride = index_type == SG_INDEXTYPE_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t);
            sx_linear_buffer_addptr(&buff, &tmp_meshes[i].cpu.ibuff, uint8_t, index_stride*num_indices, 0);
            tmp_meshes[i].index_type = index_type;
        }

        // allocate memory with one call
        rizz_model* model = sx_linear_buffer_calloc(&buff, alloc);
        if (!model) {
            sx_out_of_memory();
            return (rizz_asset_load_data) { 0 };
        }

        model->num_nodes = data->nodes_count;
        model->num_meshes = data->meshes_count;

        for (int i = 0; i < data->nodes_count; i++) {
            rizz_model_node* node = &model->nodes[i];
            node->children = tmp_children[i];
        }
        sx_memcpy(model->meshes, tmp_meshes, sizeof(rizz_model_mesh)*data->meshes_count);

        return (rizz_asset_load_data) { .obj.ptr = model, .user = data };
    }
}

static bool model__on_load(rizz_asset_load_data* data, const rizz_asset_load_params* params, const sx_mem_block* mem)
{
    const sx_alloc* alloc = params->alloc ? params->alloc : g_model.alloc;
    const rizz_model_load_params* lparams = params->params;
    const rizz_model_geometry_layout* layout = lparams->layout.buffer_strides[0] > 0 ? 
        &lparams->layout : &g_model.default_layout;

    char ext[32];
    sx_os_path_ext(ext, sizeof(ext), params->path);
    if (sx_strequalnocase(ext, "glb")) {
        rizz_temp_alloc_begin(tmp_alloc);

        cgltf_data* gltf = data->user;
        cgltf_options options = {
            .type = cgltf_file_type_glb,
            .memory = {
                .alloc = model__cgltf_alloc,
                .free = model__cgltf_free,
                .user_data = (void*)tmp_alloc
            }
        };
        cgltf_result result = cgltf_load_buffers(&options, gltf, NULL);
        if (result != cgltf_result_success) {
            return false;
        }

        rizz_temp_alloc_end(tmp_alloc);
    }

    return true;
}

static void model__on_finalize(rizz_asset_load_data* data, const rizz_asset_load_params* params, 
                               const sx_mem_block* mem)
{

}

static void model__on_reload(rizz_asset handle, rizz_asset_obj prev_obj, const sx_alloc* alloc)
{

}

static void model__on_release(rizz_asset_obj obj, const sx_alloc* alloc)
{

}

bool model_init(void)
{

}

void model_release(void)
{

}