## Sound Plugin

This is a sound plugin. mostly inspired by [soloud](http://sol.gfxile.net/soloud/) sound library. 
The API will be similar to _soloud_ but likely it will be simpler.  
As it's backend, it uses [sokol_audio](https://github.com/floooh/sokol/blob/master/sokol_audio.h) that 
implements the following platform-specific backends:

```
    - Windows: WASAPI
    - Linux: ALSA (link with asound)
    - macOS/iOS: CoreAudio (link with AudioToolbox)
    - emscripten: WebAudio with ScriptProcessorNode
```

### Features

- Simple 2D sound playing
- OGG/WAV format support
- Master volume/pan
- Clocked/looping/singleton audio playback
- Debugger view

### Limitations

- Current version only supports mono audio sources. However, it still accepts multi-channel audio, 
  But converts them to mono upon load.
  




