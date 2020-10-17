## utility plugin

Provides common functionality useful for most projects.

### Spline usage notes :
- You can use spline api with custom node struct :

```c
struct my_node {
    rizz_spline3d_node base; 
    sx_quat rot;
    float fov;
};

// or

struct my_node {
    RIZZ_SPLINE3D_NODE_FIELDS
    sx_quat rot;
    float fov;
};
```

- You also need to assign **node_stride** and **usereval** on rizz_splineXX_desc : 

```c

static void my_custom_eval(
    const rizz_spline3d_node n1, 
    const rizz_spline3d_node n2, 
    float t,
    sx_vec3* r)
{
    // cast to custom types
    const my_node* node1 = (my_node*)n1;
    const my_node* node2 = (my_node*)n2;
    my_node* result = (my_node*)r;
    
    // custom eval 
    result->fov = sx_lerp(node1->fov, node2->fov, t);
    result->rot = sx_quat_lerp(node1->rot, node2->rot, t);
}


rizz_spline3d_desc desc = (rizz_spline3d_desc) {
    .nodes = my_nodes,
    .num_nodes = 4,
    .node_stride = sizeof(struct my_node),
    .loop = true,
    .norm = false,
    .time = t,
    .usereval = my_custom_eval,
};

my_node result;
the_utility->spline.eval3d(&desc, (sx_vec3*)(&result));
```
