## Examples

#### [Hello](01-hello/hello.c)
Demonstrates basic application setup. Just clears the view with blue color.

#### [Quad](02-quad/quad.c)
- Most basic rendering. 
- Renders a textured quad on the screen.
- Loading shaders (shaders built with `glslcc` under `/tools` directory)
- Setting up buffers and pipelines
- Basic ortho camera setup
- Loading .basis textures
  
![02-quad](screenshots/02-quad.png)

#### [DrawSprite](03-drawsprite/drawsprite.c)
- Load sprite atlas
- Basic sprite drawing
- Custom sprite drawing
- Wireframe mode rendering
- Basic text drawing

![03-drawsprite](screenshots/03-drawsprite.png)

#### [AnimSprite](04-animsprite/animsprite.c)
- Animate sprites
- Sprite animation controller
- Sprite debugging
  
![04-animsprite](screenshots/04-animsprite.png)

### [PlaySound](05-playsound/playsound.c)
- Loading sound files
- Play sounds
- Sound system debugger

![05-playsound](screenshots/05-playsound.png)

### [SDF](06-sdf/sdf.c)
- Raymarching with SDF shapes
- Basic orbit camera (hold mouse and drag)
- Simple parametric animations

![06-sdf](screenshots/06-sdf.png)

### [NBody](07-nbody/nbody.c)
- Basic compute shader usage
- GPU particle rendering with instancing 
- (Currently only build under windows/Direct3D backend)

![07-nbody](screenshots/07-nbody.png)


### [Draw3d](08-draw3d/draw3d.c)
- Basic model rendering (GLTF)
- Models with sub-meshes/sub-materials
- Models with nodes and hierarchy
- Debug Grid drawing
- Debug Cube drawing

![08-draw3d](screenshots/08-draw3d.png)

### [PathFind](09-pathfind/pathfind.c)
- Contributed by @amin67v
- Basic A* path finding 
- Path priorities/Agents with different cell weights

![09-pathfind](screenshots/09-pathfind.png)

### [CollideBoxes](10-collideboxes/collideboxes.c)
- Basic collision detection between 2k boxes (OBB)
- Ray casting 
- Debug visualizations for collision and raycast

![10-collideboxes](screenshots/10-collideboxes.png)

### [PathSpline](11-pathspline/pathspline.c)
- Demonstrate the _spline_ API presented by _utility_ plugin
- Camera movement along path

![11-pathspline](screenshots/11-pathspline.png)

### [Boids](12-boids/boids.c)
- Contributed by @amin67v
- Boids example, demonstrates the use of job system

![12-boids](screenshots/12-boids.png)