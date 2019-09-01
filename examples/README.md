## Examples

#### [Hello](01-hello/hello.c)
Demonstrates basic application setup. Just clears the view with blue color.

#### [Quad](02-quad/quad.c)
- Most basic rendering. 
- Renders a textured quad on the screen.
- Loading shaders (shaders built with `glslcc` under `/tools` directory)
- Setting up buffers and pipelines
- Basic ortho camera setup
  
![02-quad](screenshots/02-quad.png)

#### [DrawSprite](03-drawsprite/drawsprite.c)
- Load sprite atlas
- Basic sprite drawing
- Custom sprite drawing
- Wireframe mode rendering

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
