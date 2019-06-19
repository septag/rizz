#include "rizz/input.h"

#include "rizz/plugin.h"

#include "sx/allocator.h"
#include "sx/string.h"

rizz_plugin_decl_main(input, plugin, e) {
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP: {
        break;
    }
    case RIZZ_PLUGIN_EVENT_INIT:
        break;
    case RIZZ_PLUGIN_EVENT_LOAD:
        break;
    case RIZZ_PLUGIN_EVENT_UNLOAD:
        break;
    case RIZZ_PLUGIN_EVENT_SHUTDOWN:
        break;
    }

    return 0;
}

rizz_plugin_decl_event_handler(input, e) {
}


static const char* input__deps[] = { "imgui" };
rizz_plugin_implement_info(input, 1000, "input plugin", input__deps, 1);