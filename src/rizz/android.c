#include "rizz/android.h"

#if SX_PLATFORM_ANDROID

#    include "rizz/core.h"

#    include "sx/string.h"

#    include "android/native_activity.h"
#    include <jni.h>
#    include <pthread.h>

// app.c
const void* sapp_android_get_native_activity(void);
void* sapp_android_get_native_window(void);

static void android__detach_cb(void* p)
{
    rizz__log_debug("Detached current thread");
    const ANativeActivity* activity = p;
    (*activity->vm)->DetachCurrentThread(activity->vm);
}

static JNIEnv* android__attach_current_thread()
{
    JNIEnv* env;
    const ANativeActivity* activity = sapp_android_get_native_activity();
    if ((*activity->vm)->GetEnv(activity->vm, (void**)&env, JNI_VERSION_1_6) == JNI_OK) {
        return env;
    }

    (*activity->vm)->AttachCurrentThread(activity->vm, &env, NULL);
    pthread_key_create((pthread_key_t*)activity, android__detach_cb);
    return env;
}

bool rizz_android_get_method(rizz_android_method* method, const char* method_name,
                             const char* method_sig, const char* class_path)
{
    sx_assert(method);
    sx_assert(method_name);
    sx_assert(class_path);
    sx_assert(method_sig);

    JNIEnv* env = android__attach_current_thread();
    jclass clazz = (*env)->FindClass(env, class_path);
    if (!clazz) {
        rizz__log_error("JNI class not found: %s", class_path);
        return false;
    }

    jmethodID mid = (*env)->GetMethodID(env, clazz, method_name, method_sig);
    if (!mid) {
        char _class_path[512];
        rizz__log_error("JNI method not found: %s.%s (%s)",
                       sx_replacechar(_class_path, sizeof(_class_path), class_path, '/', '.'),
                       method_name, method_sig);
        return false;
    }

    *method = (rizz_android_method){ .env = env, .clazz = clazz, .id = mid };

    return true;
}

bool rizz_android_get_static_method(rizz_android_method* method, const char* method_name,
                                    const char* method_sig, const char* class_path)
{
    sx_assert(method);
    sx_assert(method_name);
    sx_assert(class_path);
    sx_assert(method_sig);

    JNIEnv* env = android__attach_current_thread();
    jclass clazz = (*env)->FindClass(env, class_path);
    if (!clazz) {
        rizz__log_error("JNI class not found: %s", class_path);
        return false;
    }

    jmethodID mid = (*env)->GetStaticMethodID(env, clazz, method_name, method_sig);
    if (!mid) {
        char _class_path[512];
        rizz__log_error("JNI static method not found: %s.%s (%s)",
                       sx_replacechar(_class_path, sizeof(_class_path), class_path, '/', '.'),
                       method_name, method_sig);
        return false;
    }

    *method = (rizz_android_method){ .env = env, .clazz = clazz, .id = mid };

    return true;
}

const char* rizz_android_cache_dir()
{
    static char cache_dir[512];

    rizz_android_method getCacheDir;
    if (!rizz_android_get_method(&getCacheDir, "getCacheDir", "()Ljava/io/File;",
                                 "android/app/NativeActivity")) {
        sx_assertf(0, "invalid method: android.app.NativeActivity.getCacheDir");
        return NULL;
    }

    const ANativeActivity* activity = sapp_android_get_native_activity();
    jobject jcache_dir =
        (*getCacheDir.env)->CallObjectMethod(getCacheDir.env, activity->clazz, getCacheDir.id);

    rizz_android_method getPath;
    if (!rizz_android_get_method(&getPath, "getPath", "()Ljava/lang/String;", "java/io/File")) {
        sx_assertf(0, "invalid method: java.io.file.getPath");
        return NULL;
    }

    jstring jpath = (*getPath.env)->CallObjectMethod(getPath.env, jcache_dir, getPath.id);
    const char* path = (*getPath.env)->GetStringUTFChars(getPath.env, jpath, NULL);
    sx_strcpy(cache_dir, sizeof(cache_dir), path);
    (*getPath.env)->ReleaseStringUTFChars(getPath.env, jpath, path);

    return cache_dir;
}

AAssetManager* rizz_android_asset_mgr() 
{
    const ANativeActivity* activity = sapp_android_get_native_activity();
    return activity->assetManager;
}

bool rizz_android_window_size(int* x, int* y)
{
    ANativeWindow* window = sapp_android_get_native_window();
    if (window) {
        sx_assert(x);
        sx_assert(y);

        *x = ANativeWindow_getWidth(window);
        *y = ANativeWindow_getHeight(window);

        return true;
    } else {
        sx_assertf(0, "window is not created");
        return false;
    }
}

#endif    // SX_PLATFORM_ANDROID