## Sprite Plugin

Sprite plugin. Loads sprite atlases, renders/animates 2D sprites. The low-level API gives 
the ability to extend it and make 3D billboards or any other specific rendering by the user.

### Features
- Atlas support
- Alpha cropping
- Mesh sprites
- Animation clips
- Animation controller (state machine)

### Usage

#### Atlas
The plugin registers _"atlas"_ asset type on initialization. atlases can be created using 
[atlasc](http://www.github.com/septag/atlasc) command-line tool.  

- To load atlas using _asset_ API, you must provide `rizz_atlas_load_params` as input params for 
`load` functions.
- The object returned by `rizz_api_asset.obj` is a pointer to `rizz_atlas` (see header)

#### Sprites
Retrieve the API at initialization:
```c
the_sprite = the_plugin->get_api(RIZZ_API_SPRITE, 0);
```

- use `create` and `destroy` functions to add/remove sprites.
- To draw the sprites by the default renderer use `draw_xxx` APIs.  
- Default internal buffer sizes are 2k vertices and 6k indices. you can resize them using
`resize_draw_limits` function.  
- To draw using your custom renderer, use `make_drawdata_xxx` functions. It will give you all
  the buffers you need to draw given sprites, and you can manipulate vertex data, shaders and
  other stuff for your specific use.

#### Multi-threading
Some parts of the API is stateless and thread-safe, including `draw` calls, `make_drawdata` functions and property accessors.   
But `create` and `destroy` are not thread-safe, so make sure you don't add or delete sprites while other threads are using the API.

### TODO
- [ ] Add dummy atlases in case if atlas loading fails to prevent crashes on load errors
- [ ] Dummy sprites for atlases/textures that are loading in async mode
- [ ] Hot-reload support
- [x] Sprite debugger (imgui)
- [x] Sprite animation clips
- [x] sprite animation controller

