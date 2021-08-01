# Basis Universal texture format support plugin

[basis_universal](https://github.com/BinomialLLC/basis_universal) is a universal texture format that decodes 
to your specific compressed format at runtime. It is ideal for use in multi-platform (especially mobile) environments, 
where you need to maintain minimal size and be able to transcode to different hardware supported formats at runtime.

The asset type name is "texture_basisu" with `rizz_texture_load_params` (same as textures) as load parameters for asset loader. 
With the exception of `fmt` field, which is mandatory when loading basis files, because basis files needs to transcode to a valid 
format at runtime and you need to provide it explicitly, here's an example usage:

```
rizz_asset img = the_asset->load("texture_basisu", "/path/to/image.basis",
                        &(rizz_texture_load_params){ .fmt = IS_MOBILE ? SG_PIXELFORMAT_ETC2_RGB8 : SG_PIXELFORMAT_BC1_RGBA },
                        0, NULL, 0);
```

You can look into _02-quad_ example for actual usage of this format and plugin.


