#include "rizz/3dtools.h"
#include "3dtools-internal.h"

#include "rizz/rizz.h"
#include "rizz/imgui.h"

#include "sx/allocator.h"
#include "sx/math.h"
#include "sx/string.h"
#include "sx/os.h"
#include "sx/lin-alloc.h"
#include "sx/handle.h"
#include "sx/array.h"

#define SX_MAX_BUFFER_FIELDS 128
#include "sx/linear-buffer.h"
#undef SX_MAX_BUFFERS_FIELDS

#include <alloca.h>

#define CGLTF_IMPLEMENTATION
#include "../3rdparty/cgltf/cgltf.h"

RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_asset* the_asset;
RIZZ_STATE static rizz_api_gfx* the_gfx;
RIZZ_STATE static rizz_api_imgui* the_imgui;

typedef struct model__vertex_attribute 
{
    const char* semantic;
    int index;
} model__vertex_attribute;

typedef struct rizz_model_context 
{
    const sx_alloc* alloc;
    rizz_model blank_model;
    rizz_model failed_model;
    rizz_model_geometry_layout default_layout;
    rizz_material_data* materials;
    sx_handle_pool* material_handles; 
} rizz_model_context;

RIZZ_STATE static rizz_model_context g_model;

static rizz_material model__create_material_from_gltf(cgltf_material* gltf_mtl)
{
    rizz_material_alpha_mode alpha_mode;
    switch (gltf_mtl->alpha_mode) {
    case cgltf_alpha_mode_opaque:      alpha_mode = RIZZ_MATERIAL_ALPHAMODE_OPAQUE;     break;
    case cgltf_alpha_mode_mask:        alpha_mode = RIZZ_MATERIAL_ALPHAMODE_MASK;       break;
    case cgltf_alpha_mode_blend:       alpha_mode = RIZZ_MATERIAL_ALPHAMODE_BLEND;      break;
    default:                           sx_assert(0); alpha_mode = RIZZ_MATERIAL_ALPHAMODE_OPAQUE; break;
    }

    rizz_material_data mtl = {
        .has_metal_roughness = gltf_mtl->has_pbr_metallic_roughness,
        .has_specular_glossiness = gltf_mtl->has_pbr_specular_glossiness,
        .has_clearcoat = gltf_mtl->has_clearcoat,
        .pbr_metallic_roughness = {
            .base_color_factor.f[0] = gltf_mtl->pbr_metallic_roughness.base_color_factor[0],
            .base_color_factor.f[1] = gltf_mtl->pbr_metallic_roughness.base_color_factor[1],
            .base_color_factor.f[2] = gltf_mtl->pbr_metallic_roughness.base_color_factor[2],
            .base_color_factor.f[3] = gltf_mtl->pbr_metallic_roughness.base_color_factor[3],
            .metallic_factor = gltf_mtl->pbr_metallic_roughness.metallic_factor,
            .roughness_factor = gltf_mtl->pbr_metallic_roughness.roughness_factor
        },
        .pbr_specular_glossiness = {
            .diffuse_factor.f[0] = gltf_mtl->pbr_specular_glossiness.diffuse_factor[0],
            .diffuse_factor.f[1] = gltf_mtl->pbr_specular_glossiness.diffuse_factor[1],
            .diffuse_factor.f[2] = gltf_mtl->pbr_specular_glossiness.diffuse_factor[2],
            .diffuse_factor.f[3] = gltf_mtl->pbr_specular_glossiness.diffuse_factor[3],
            .specular_factor.f[0] = gltf_mtl->pbr_specular_glossiness.specular_factor[0],
            .specular_factor.f[1] = gltf_mtl->pbr_specular_glossiness.specular_factor[1],
            .specular_factor.f[2] = gltf_mtl->pbr_specular_glossiness.specular_factor[2],
            .glossiness_factor = gltf_mtl->pbr_specular_glossiness.glossiness_factor,
        },
        .clearcoat = {
            .clearcoat_factor = gltf_mtl->clearcoat.clearcoat_factor,
            .clearcoat_roughness_factor = gltf_mtl->clearcoat.clearcoat_roughness_factor
        },
        .emissive_factor.f[0] = gltf_mtl->emissive_factor[0],
        .emissive_factor.f[1] = gltf_mtl->emissive_factor[1],
        .emissive_factor.f[2] = gltf_mtl->emissive_factor[2],
        .emissive_factor.f[3] = gltf_mtl->emissive_factor[3],
        .alpha_mode = alpha_mode,
        .alpha_cutoff = gltf_mtl->alpha_cutoff,
        .double_sided = gltf_mtl->double_sided,
        .unlit = gltf_mtl->unlit
    };
    
    sx_handle_t handle = sx_handle_new_and_grow(g_model.material_handles, g_model.alloc);
    int index = sx_handle_index(handle);
    if (index < sx_array_count(g_model.materials)) {
        g_model.materials[index] = mtl;
    } else {
        sx_array_push(g_model.alloc, g_model.materials, mtl);
    }

    return (rizz_material) { .id = handle };
}

static void model__destroy_material(rizz_material mtl)
{
    sx_assert(mtl.id);
    sx_assert(sx_handle_valid(g_model.material_handles, mtl.id));
    sx_handle_del(g_model.material_handles, mtl.id);
}

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
                                            cgltf_attribute* srcatt, int start_vertex)
{
    cgltf_accessor* access = srcatt->data;
    const rizz_vertex_attr* attr = &vertex_layout->attrs[0];
    model__vertex_attribute mapped_att = model__convert_attribute(srcatt->type, srcatt->index);
    while (attr->semantic) {
        if (sx_strequal(attr->semantic, mapped_att.semantic) && attr->semantic_idx == mapped_att.index) {
            int vertex_stride = vertex_layout->buffer_strides[attr->buffer_index];
            uint8_t* src_buff = (uint8_t*)access->buffer_view->buffer->data;
            uint8_t* dst_buff = (uint8_t*)mesh->cpu.vbuffs[attr->buffer_index];
            int dst_offset =  start_vertex * vertex_stride + attr->offset;
            int src_offset = (int)(access->offset + access->buffer_view->offset);

            int count = (int)access->count;
            int src_data_size = (int)access->stride; 
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
                                 cgltf_mesh* srcmesh)
{
    // create buffers based on input vertex_layout
    sg_index_type index_type = mesh->index_type;

    // map source vertex buffer to our data
    // map source index buffer to our data
    int start_index = 0;
    int start_vertex = 0;
    for (int i = 0; i < (int)srcmesh->primitives_count; i++) {
        cgltf_primitive* srcprim = &srcmesh->primitives[i];

        // vertices
        int count = 0;
        for (cgltf_size k = 0; k < srcprim->attributes_count; k++) {
            cgltf_attribute* srcatt = &srcprim->attributes[k];
            model__map_attributes_to_buffer(mesh, vertex_layout, srcatt, start_vertex);
            if (count == 0) {
                count = (int)srcatt->data->count;
            }
            sx_assert(count == (int)srcatt->data->count);
        }

        // indices
        cgltf_accessor* srcindices = srcprim->indices;
        if (srcindices->component_type == cgltf_component_type_r_16u && index_type == SG_INDEXTYPE_UINT16) {
            uint16_t* indices = (uint16_t*)mesh->cpu.ibuff + start_index;
            uint16_t* _srcindices = (uint16_t*)((uint8_t*)srcindices->buffer_view->buffer->data + 
                                    srcindices->buffer_view->offset);
            for (cgltf_size k = 0; k < srcindices->count; k++) {
                indices[k] = _srcindices[k] + start_vertex;
            }
            // flip the winding
            for (int k = 0, num_tris = (int)srcindices->count/3; k < num_tris; k++) {
                int ii = k*3;
                sx_swap(indices[ii], indices[ii+2], uint16_t);
            }
        } else if (srcindices->component_type == cgltf_component_type_r_16u && index_type == SG_INDEXTYPE_UINT32) {
            uint32_t* indices = (uint32_t*)mesh->cpu.ibuff + start_index;
            uint16_t* _srcindices = (uint16_t*)srcindices->buffer_view->buffer->data;
            for (cgltf_size k = 0; k < srcindices->count; k++) {
                indices[k] = (uint32_t)(_srcindices[k] + start_vertex);
            }
            // flip the winding
            for (int k = 0, num_tris = (int)srcindices->count/3; k < num_tris; k++) {
                int ii = k*3;
                sx_swap(indices[ii], indices[ii+2], uint32_t);
            }
        } else if (srcindices->component_type == cgltf_component_type_r_32u && index_type == SG_INDEXTYPE_UINT32) {
            uint32_t* indices = (uint32_t*)mesh->cpu.ibuff + start_index;
            uint32_t* _srcindices = (uint32_t*)srcindices->buffer_view->buffer->data;
            for (cgltf_size k = 0; k < srcindices->count; k++) {
                indices[k] = _srcindices[k] + (uint32_t)start_vertex;
            }
            // flip the winding
            for (int k = 0, num_tris = (int)srcindices->count/3; k < num_tris; k++) {
                int ii = k*3;
                sx_swap(indices[ii], indices[ii+2], uint32_t);
            }
        }

        rizz_model_submesh* submesh = &mesh->submeshes[i];
        submesh->start_index = start_index;
        submesh->num_indices = (int)srcprim->indices->count;
        start_index += (int)srcprim->indices->count;
        start_vertex += count;
    }
}

static bool model__setup_gpu_buffers(rizz_model* model, sg_usage vbuff_usage, sg_usage ibuff_usage) 
{
    rizz_model_geometry_layout* layout = &model->layout;
    for (int i = 0; i < model->num_meshes; i++) {
        rizz_model_mesh* mesh = &model->meshes[i];

        if (vbuff_usage != _SG_USAGE_DEFAULT) {
        int buffer_index = 0;
            while (layout->buffer_strides[buffer_index]) {
                mesh->gpu.vbuffs[buffer_index] = the_gfx->make_buffer(&(sg_buffer_desc) {
                    .size = layout->buffer_strides[buffer_index]*mesh->num_vertices,
                    .type = SG_BUFFERTYPE_VERTEXBUFFER,
                    .usage = vbuff_usage,
                    .content = mesh->cpu.vbuffs[buffer_index]
                });
                if (!mesh->gpu.vbuffs[buffer_index].id) {
                    return false;
                }
                buffer_index++;
            }
        }

        if (ibuff_usage != _SG_USAGE_DEFAULT) {
            mesh->gpu.ibuff = the_gfx->make_buffer(&(sg_buffer_desc) {
                .size = ((mesh->index_type == SG_INDEXTYPE_UINT16) ? 
                    sizeof(uint16_t) : sizeof(uint32_t)) * mesh->num_indices,
                .type = SG_BUFFERTYPE_INDEXBUFFER,
                .usage = ibuff_usage,
                .content = mesh->cpu.ibuff
            });
        }

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

static void* model__cgltf_alloc(void* user, cgltf_size size)
{
    const sx_alloc* alloc = user;
    return sx_malloc(alloc, size);
}

static void model__cgltf_free(void* user, void* ptr)
{
    // we are using linear allocator, so we won't need free
    sx_unused(user);
    sx_unused(ptr);
}

static rizz_asset_load_data model__on_prepare(const rizz_asset_load_params* params, const sx_mem_block* mem)
{
    const sx_alloc* alloc = params->alloc ? params->alloc : g_model.alloc;
    const rizz_model_load_params* lparams = params->params;
    const rizz_model_geometry_layout* layout = lparams->layout.buffer_strides[0] > 0 ? 
        &lparams->layout : &g_model.default_layout;

    char ext[32];
    sx_os_path_ext(ext, sizeof(ext), params->path);
    if (sx_strequalnocase(ext, ".glb")) {
        // TODO: this method of allocating 4x size of the model as a temp memory is not effective (and not safe)
        //       we can modify to use temp allocator for parsing json and another actual to put real data
        sx_linalloc linalloc;
        void* parse_buffer = sx_malloc(g_model.alloc, (size_t)mem->size*4);
        if (!parse_buffer) {
            sx_out_of_memory();
            return (rizz_asset_load_data) { {0} };
        }

        sx_linalloc_init(&linalloc, parse_buffer, (size_t)mem->size*4);
        cgltf_options options = {
            .type = cgltf_file_type_glb,
            .memory = {
                .alloc = model__cgltf_alloc,
                .free = model__cgltf_free,
                .user_data = (void*)&linalloc.alloc
            }
        };
        cgltf_data* data;
        cgltf_result result = cgltf_parse(&options, mem->data, (size_t)mem->size, &data);
        if (result != cgltf_result_success) {
            rizz_log_warn("cannot parse GLTF file: %s", params->path);
            return (rizz_asset_load_data) { {0} };
        }

        if (data->nodes_count == 0) {
            rizz_log_warn("model '%s' doesn't have any nodes inside", params->path);
            return (rizz_asset_load_data) { {0} };
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
        for (cgltf_size i = 0; i < data->nodes_count; i++) {
            cgltf_node* node = &data->nodes[i];
            if (node->children_count) {
                sx_linear_buffer_addptr(&buff, &tmp_children[i], int, node->children_count, 0);
            }
        }

        for (cgltf_size i = 0; i < data->meshes_count; i++) {
            cgltf_mesh* mesh = &data->meshes[i];
            sg_index_type index_type = SG_INDEXTYPE_NONE;
        
            sx_linear_buffer_addptr(&buff, &tmp_meshes[i].submeshes, rizz_model_submesh, mesh->primitives_count, 0);
            tmp_meshes[i].num_submeshes = (int)mesh->primitives_count;

            int num_vertices = 0;
            int num_indices = 0;
            for (cgltf_size k = 0; k < mesh->primitives_count; k++) {
                cgltf_primitive* prim = &mesh->primitives[k];
                int count = 0;
                for (cgltf_size aa = 0; aa < prim->attributes_count; aa++) {
                    cgltf_attribute* srcatt = &prim->attributes[aa];
                    if (count == 0) {
                        count = (int)srcatt->data->count;
                    }
                    sx_assert(count == (int)srcatt->data->count);
                }
                num_vertices += count;
                num_indices += (int)mesh->primitives[k].indices->count;
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
            return (rizz_asset_load_data) { {0} };
        }

        model->num_nodes = (int)data->nodes_count;
        model->num_meshes = (int)data->meshes_count;

        // create materials
        for (int i = 0; i < model->num_meshes; i++) {
            rizz_model_mesh* mesh = &tmp_meshes[i];
            for (int k = 0; k < mesh->num_submeshes; k++) {
                sx_assert (mesh->num_submeshes == (int)data->meshes[i].primitives_count);
                cgltf_primitive* prim = &data->meshes[i].primitives[k];
                if (prim->material) {
                    tmp_meshes[i].submeshes[k].mtl = model__create_material_from_gltf(prim->material);
                }
            }
        }

        for (cgltf_size i = 0; i < data->nodes_count; i++) {
            rizz_model_node* node = &model->nodes[i];
            node->children = tmp_children[i];
        }
        sx_memcpy(model->meshes, tmp_meshes, sizeof(rizz_model_mesh)*data->meshes_count);

        return (rizz_asset_load_data) { .obj.ptr = model, .user1 = data, .user2 = parse_buffer };
    }

    return (rizz_asset_load_data) { {0} };
}

static bool model__on_load(rizz_asset_load_data* data, const rizz_asset_load_params* params, const sx_mem_block* mem)
{
    sx_unused(mem);
    
    const rizz_model_load_params* lparams = params->params;
    const rizz_model_geometry_layout* layout = lparams->layout.buffer_strides[0] > 0 ? 
        &lparams->layout : &g_model.default_layout;

    rizz_model* model = data->obj.ptr;
    char ext[32];
    sx_os_path_ext(ext, sizeof(ext), params->path);
    if (sx_strequalnocase(ext, ".glb")) {
        rizz_temp_alloc_begin(tmp_alloc);
        sx_unused(tmp_alloc);

        cgltf_data* gltf = data->user1;
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

        // meshes
        for (cgltf_size i = 0; i < gltf->meshes_count; i++) {
            rizz_model_mesh* mesh = &model->meshes[i];
            cgltf_mesh* _mesh = &gltf->meshes[i];

            sx_strcpy(mesh->name, sizeof(mesh->name), _mesh->name);
            model__setup_buffers(mesh, layout, _mesh);
        }

        // nodes
        for (cgltf_size i = 0; i < gltf->nodes_count; i++) {
            rizz_model_node* node = &model->nodes[i];
            cgltf_node* _node = &gltf->nodes[i];

            node->local_tx = sx_tx3d_ident();

            sx_strcpy(node->name, sizeof(node->name), _node->name);
            if (sx_strlen(node->name) != sx_strlen(_node->name)) {
                rizz_log_warn("model: %s, node: '%s' - name is too long (more than 31 characters), "
                              "node setup will probably have errors", params->path, _node->name);
            }

            if (_node->has_scale) {
                // TODO: apply scaling to the model
                //sx_assert(0 && "not supported yet. Apply scaling in DCC before exporting");
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
            for (cgltf_size mi = 0; mi < gltf->meshes_count; mi++) {
                if (&gltf->meshes[mi] == _node->mesh) {
                    node->mesh_id = (int)mi;
                    break;
                }
            }

            // bounds
            sx_aabb bounds = sx_aabb_empty();
            if (node->mesh_id != -1) {
                rizz_model_mesh* mesh = &model->meshes[node->mesh_id];
                const rizz_vertex_attr* attr = model__find_attribute(layout, "POSITION", 0);
                int vertex_stride = layout->buffer_strides[attr->buffer_index];
                uint8_t* vbuff = mesh->cpu.vbuffs[attr->buffer_index];
                for (int v = 0; v < mesh->num_vertices; v++) {
                    sx_vec3 pos = *((sx_vec3*)(vbuff + v*vertex_stride + attr->offset));
                    sx_aabb_add_point(&bounds, pos);
                }
            }
            node->bounds = bounds;
        }

        // build node hierarchy based on node names
        for (cgltf_size i = 0; i < gltf->nodes_count; i++) {
            rizz_model_node* node = &model->nodes[i];
            cgltf_node* _node = &gltf->nodes[i];

            if (_node->parent) {
                node->parent_id = model__find_node_byname(model, _node->parent->name);
            } else {
                node->parent_id = -1;
            }

            node->num_childs = (int)_node->children_count;
            if (_node->children_count > 0) {
                for (cgltf_size ci = 0; ci < _node->children_count; ci++) {
                    node->children[ci] = model__find_node_byname(model, _node->children[ci]->name);
                }
            } 
        }

        sx_memcpy(&model->layout, layout, sizeof(rizz_model_geometry_layout));

        rizz_temp_alloc_end(tmp_alloc);
    }

    return true;
}

static void model__on_finalize(rizz_asset_load_data* data, const rizz_asset_load_params* params, 
                               const sx_mem_block* mem)
{
    sx_unused(mem);

    const rizz_model_load_params* lparams = params->params;
    rizz_model* model = data->obj.ptr;

    model__setup_gpu_buffers(model, lparams->vbuff_usage, lparams->ibuff_usage);
    sx_free(g_model.alloc, data->user2);    // free parse_buffer (see on_prepare for allocation)
}

static void model__on_reload(rizz_asset handle, rizz_asset_obj prev_obj, const sx_alloc* alloc)
{
    sx_unused(handle);
    sx_unused(prev_obj);
    sx_unused(alloc);
    // TODO: update all instances of the model
}

static void model__on_release(rizz_asset_obj obj, const sx_alloc* alloc)
{
    rizz_model* model = obj.ptr;
    if (!alloc) {
        alloc = g_model.alloc;
    }

    // destroy gpu buffers and materials
    for (int i = 0; i < model->num_meshes; i++) {
        rizz_model_mesh* mesh = &model->meshes[i];
        for (int k = 0; k < mesh->num_vbuffs; k++) {
            the_gfx->destroy_buffer(mesh->gpu.vbuffs[k]);
        }
        the_gfx->destroy_buffer(mesh->gpu.ibuff);

        for (int k = 0; k < mesh->num_submeshes; k++) {
            rizz_model_submesh* submesh = &mesh->submeshes[k];
            if (submesh->mtl.id) {
                model__destroy_material(submesh->mtl);
            }
        }
    }

    sx_free(alloc, model);
}

bool model__init(rizz_api_core* core, rizz_api_asset* asset, rizz_api_gfx* gfx, rizz_api_imgui* imgui)
{
    the_core = core;
    the_asset = asset;
    the_gfx = gfx;
    the_imgui = imgui;
    g_model.alloc = the_core->alloc(RIZZ_MEMID_GRAPHICS);

    // default layout, this will be passed when no layout is set for loading models (matches rizz_prims3d_vertex)
    g_model.default_layout = (rizz_model_geometry_layout) {
        .attrs[0] = { .semantic="POSITION", .offset=offsetof(rizz_prims3d_vertex, pos), .format=SG_VERTEXFORMAT_FLOAT3},
        .attrs[1] = { .semantic="NORMAL", .offset=offsetof(rizz_prims3d_vertex, normal), .format=SG_VERTEXFORMAT_FLOAT3},
        .attrs[2] = { .semantic="TEXCOORD", .offset=offsetof(rizz_prims3d_vertex, uv), .format=SG_VERTEXFORMAT_FLOAT2},
        .attrs[3] = { .semantic="COLOR", .offset=offsetof(rizz_prims3d_vertex, color), .format=SG_VERTEXFORMAT_UBYTE4N},
        .buffer_strides[0] = sizeof(rizz_prims3d_vertex)
    };

    // blank_model
    {
        const sx_alloc* alloc = g_model.alloc;
        rizz_model* blank_model = &g_model.blank_model;
        blank_model->num_nodes = 1;
        blank_model->nodes = sx_malloc(alloc, sizeof(rizz_model_node));
        sx_assert_rel(blank_model->nodes);
        sx_memset(blank_model->nodes, 0x0, sizeof(rizz_model_node));
        blank_model->nodes[0].bounds = sx_aabbwhd(1.0f, 1.0f, 1.0f);
        blank_model->nodes[0].local_tx = sx_tx3d_ident();
        blank_model->nodes[0].mesh_id = -1;
        blank_model->nodes[0].parent_id = -1;
    }

    // failed_model
    {
        const sx_alloc* alloc = g_model.alloc;
        rizz_model* failed_model = &g_model.failed_model;
        failed_model->num_nodes = 1;
        failed_model->nodes = sx_malloc(alloc, sizeof(rizz_model_node));
        sx_assert_rel(failed_model->nodes);
        sx_memset(failed_model->nodes, 0x0, sizeof(rizz_model_node));
        failed_model->nodes[0].parent_id = -1;
        failed_model->nodes[0].bounds = sx_aabbwhd(1.0f, 1.0f, 1.0f);
        failed_model->nodes[0].local_tx = sx_tx3d_ident();

        failed_model->meshes = sx_malloc(alloc, sizeof(rizz_model_mesh));
        sx_assert_rel(failed_model->meshes);
        failed_model->num_meshes = 1;
        sx_memset(failed_model->meshes, 0x0, sizeof(rizz_model_mesh));
        rizz_model_mesh* mesh = failed_model->meshes;

        rizz_prims3d_geometry geo;
        if (prims3d__generate_box_geometry(alloc, &geo)) {
            mesh->num_vbuffs = 1;
            mesh->cpu.vbuffs[0] = geo.verts;
            mesh->cpu.ibuff = geo.indices;
            mesh->gpu.vbuffs[0] = the_gfx->make_buffer(&(sg_buffer_desc) {
                .size = sizeof(rizz_prims3d_vertex)*geo.num_verts,
                .type = SG_BUFFERTYPE_VERTEXBUFFER,
                .content = geo.verts,
                .label = "__failed_model_vbuff__"
            });
            mesh->gpu.ibuff = the_gfx->make_buffer(&(sg_buffer_desc) {
                .size = sizeof(uint16_t)*geo.num_indices,
                .type = SG_BUFFERTYPE_INDEXBUFFER,
                .content = geo.indices,
                .label = "__failed_model_ibuff__"
            });
            mesh->num_vertices = geo.num_verts;
            mesh->num_indices = geo.num_indices;
            mesh->index_type = SG_INDEXTYPE_UINT16;
            mesh->submeshes = sx_malloc(alloc, sizeof(rizz_model_submesh));
            sx_assert_rel(mesh->submeshes);
            mesh->submeshes[0].start_index = 0;
            mesh->submeshes[0].num_indices = geo.num_indices;
            mesh->submeshes[0].mtl.id = 0;
        } else {
            return false;
        }

        sx_memcpy(&failed_model->layout, &g_model.default_layout, sizeof(rizz_model_geometry_layout));
    }

    the_asset->register_asset_type("model", (rizz_asset_callbacks) {
        .on_prepare = model__on_prepare,
        .on_load = model__on_load,
        .on_finalize = model__on_finalize,
        .on_release = model__on_release,
        .on_reload = model__on_reload
    }, "rizz_model_load_params", sizeof(rizz_model_load_params), 
        (rizz_asset_obj) {.ptr = &g_model.failed_model}, 
        (rizz_asset_obj) {.ptr = &g_model.blank_model}, 0);

    g_model.material_handles = sx_handle_create_pool(g_model.alloc, 100);
    if (!g_model.material_handles) {
        sx_out_of_memory();
        return false;
    }

    return true;
}

void model__release(void)
{
    // release dummy models
    {
        const sx_alloc* alloc = g_model.alloc;
        
        sx_free(alloc, g_model.blank_model.nodes);

        sx_free(alloc, g_model.failed_model.nodes);
        prims3d__free_geometry(&(rizz_prims3d_geometry) {
            .verts = g_model.failed_model.meshes[0].cpu.vbuffs[0],
            .indices = g_model.failed_model.meshes[0].cpu.ibuff
        }, alloc);
        sx_free(alloc, g_model.failed_model.meshes[0].submeshes);
        the_gfx->destroy_buffer(g_model.failed_model.meshes[0].gpu.vbuffs[0]);
        the_gfx->destroy_buffer(g_model.failed_model.meshes[0].gpu.ibuff);
        sx_free(alloc, g_model.failed_model.meshes);
    }

    // TODO: destroy all remaining instances

    sx_array_free(g_model.alloc, g_model.materials);
    sx_handle_destroy_pool(g_model.material_handles, g_model.alloc);

    the_asset->unregister_asset_type("model");
}

const rizz_model* model__get(rizz_asset model_asset)
{
#if RIZZ_DEV_BUILD
    sx_assert_rel(sx_strequal(the_asset->type_name(model_asset), "model") && "asset handle is not a model");
#endif
    return (const rizz_model*)the_asset->obj(model_asset).ptr;
}

void model__set_imgui(rizz_api_imgui* imgui)
{
    the_imgui = imgui;
}

const rizz_material_data* model__get_material(rizz_material mtl) 
{
    sx_assert(mtl.id);
    sx_assert(sx_handle_valid(g_model.material_handles, mtl.id));

    return &g_model.materials[sx_handle_index(mtl.id)];
}
