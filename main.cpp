#include <jni.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <atomic>
#include <dirent.h>
#include <fstream>
#include <ctime>

#include <deque>
#include <mutex>
#include <thread>
#include <string>
#include <vector>
#include <utility>

extern "C" {
#include "frida-gumjs.h"
}

#include "script.h"
#include "UI/UIManager.h"
#include "UI/InputHandler.h"
#include "UI/KeyboardBridge.h"

extern "C" const uint8_t dex_data_start[];
extern "C" const uint8_t dex_data_end[];

static JavaVM* g_vm = nullptr;

static std::atomic<bool> g_js_ready{false};
static GumScriptBackend* g_js_backend = nullptr;
static GMainContext* g_js_context = nullptr;
static GMainLoop* g_js_loop = nullptr;

static std::mutex g_js_logMtx;
static std::deque<std::string> g_js_logQueue;
static int g_js_totalLogCount = 0;

static std::string g_files_dir = "";
static std::string g_config_cache = "";
static std::string g_script_cache = "";

// 用于本地日志写入的全局变量
static std::mutex g_logFileMtx;
static std::string g_logFilePath = "";

float g_fab_x = -1.0f;
float g_fab_y = -1.0f;
bool g_touch_penetration = false;
bool g_hide_injection_features = false;

struct ActiveScript {
    int id;
    std::string name;
    GumScript* script;
};
static std::vector<ActiveScript> g_active_scripts;
static std::mutex g_scriptsMtx;
static int g_script_id_counter = 0;

// 线程安全的本地文件日志写入器
void LogToFile(const char* level, const std::string& msg) {
    if (g_logFilePath.empty()) return;
    std::lock_guard<std::mutex> lock(g_logFileMtx);
    FILE* fp = fopen(g_logFilePath.c_str(), "a");
    if (fp) {
        time_t now = time(0);
        struct tm tstruct;
        char buf[80];
        tstruct = *localtime(&now);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tstruct);
        fprintf(fp, "[%s] [%s] %s\n", buf, level, msg.c_str());
        fclose(fp);
    }
}

// 捕获并记录 JNI 层抛出的异常
void CheckJniException(JNIEnv* env, const char* step) {
    if (env->ExceptionCheck()) {
        jthrowable exc = env->ExceptionOccurred();
        env->ExceptionClear();
        jclass excCls = env->GetObjectClass(exc);
        jmethodID toStringMid = env->GetMethodID(excCls, "toString", "()Ljava/lang/String;");
        jstring jStr = (jstring)env->CallObjectMethod(exc, toStringMid);
        if (jStr) {
            const char* str = env->GetStringUTFChars(jStr, nullptr);
            LogToFile("JNI_EXC", std::string(step) + " -> " + str);
            env->ReleaseStringUTFChars(jStr, str);
            env->DeleteLocalRef(jStr);
        }
        env->DeleteLocalRef(excCls);
        env->DeleteLocalRef(exc);
    }
}

bool IsJsEngineReady() { return g_js_ready.load(); }
int GetJsLogCount() { std::lock_guard<std::mutex> lock(g_js_logMtx); return g_js_totalLogCount; }
void ClearJsLogs() { std::lock_guard<std::mutex> lock(g_js_logMtx); g_js_logQueue.clear(); g_js_totalLogCount = 0; }
std::deque<std::string> DrainJsLogs() { std::lock_guard<std::mutex> lock(g_js_logMtx); std::deque<std::string> out; out.swap(g_js_logQueue); return out; }

const char* GetLogFilePath() { return g_files_dir.c_str(); }
const char* GetConfigCache() { return g_config_cache.c_str(); }
const char* GetScriptCache() { return g_script_cache.c_str(); }

void WriteLogToFile(const char* logContent) {
    if (g_files_dir.empty() || !logContent) return;
    FILE* f = fopen(g_files_dir.c_str(), "w");
    if (f) { fprintf(f, "%s", logContent); fclose(f); }
}

void CopyLogToClipboard(const char* logContent) {
    if (!g_vm || !logContent) return;
    JNIEnv* env = nullptr;
    bool attached = false;
    if (g_vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        g_vm->AttachCurrentThread(&env, nullptr);
        attached = true;
    }
    if (env->PushLocalFrame(32) != 0) {
        if (attached) g_vm->DetachCurrentThread();
        return;
    }
    jclass activityThreadCls = env->FindClass("android/app/ActivityThread");
    if (activityThreadCls) {
        jmethodID currentApplicationMid = env->GetStaticMethodID(activityThreadCls, "currentApplication", "()Landroid/app/Application;");
        if (currentApplicationMid) {
            jobject app = env->CallStaticObjectMethod(activityThreadCls, currentApplicationMid);
            if (app) {
                jclass contextCls = env->GetObjectClass(app);
                jmethodID getSystemServiceMid = env->GetMethodID(contextCls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
                if (getSystemServiceMid) {
                    jstring serviceName = env->NewStringUTF("clipboard");
                    jobject clipboardManager = env->CallObjectMethod(app, getSystemServiceMid, serviceName);
                    if (clipboardManager) {
                        jclass clipboardManagerCls = env->GetObjectClass(clipboardManager);
                        jmethodID setPrimaryClipMid = env->GetMethodID(clipboardManagerCls, "setPrimaryClip", "(Landroid/content/ClipData;)V");
                        if (setPrimaryClipMid) {
                            jclass clipDataCls = env->FindClass("android/content/ClipData");
                            if (clipDataCls) {
                                jmethodID newPlainTextMid = env->GetStaticMethodID(clipDataCls, "newPlainText", "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;");
                                if (newPlainTextMid) {
                                    jstring label = env->NewStringUTF("ModMenuLogs");
                                    jstring textToCopy = env->NewStringUTF(logContent);
                                    jobject clipData = env->CallStaticObjectMethod(clipDataCls, newPlainTextMid, label, textToCopy);
                                    if (clipData) {
                                        env->CallVoidMethod(clipboardManager, setPrimaryClipMid, clipData);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    env->PopLocalFrame(nullptr);
    if (attached) g_vm->DetachCurrentThread();
}

void SaveConfigToFile(const char* content) {
    if (g_files_dir.empty() || !content) return;
    std::string dir = g_files_dir;
    size_t pos = dir.find_last_of('/');
    if (pos != std::string::npos) dir = dir.substr(0, pos);
    std::string path = dir + "/modmenu_config.ini";
    FILE* f = fopen(path.c_str(), "w");
    if (f) { fprintf(f, "%s", content); fclose(f); g_config_cache = content; }
}

void SaveScriptToFile(const char* content) {
    if (g_files_dir.empty() || !content) return;
    std::string dir = g_files_dir;
    size_t pos = dir.find_last_of('/');
    if (pos != std::string::npos) dir = dir.substr(0, pos);
    std::string path = dir + "/modmenu_script.js";
    FILE* f = fopen(path.c_str(), "w");
    if (f) { fprintf(f, "%s", content); fclose(f); g_script_cache = content; }
}

void LoadConfigToCache() {
    if (g_files_dir.empty()) return;
    std::string dir = g_files_dir;
    size_t pos = dir.find_last_of('/');
    if (pos != std::string::npos) dir = dir.substr(0, pos);

    std::string configPath = dir + "/modmenu_config.ini";
    FILE* f1 = fopen(configPath.c_str(), "r");
    if (f1) {
        fseek(f1, 0, SEEK_END); long fsize = ftell(f1); fseek(f1, 0, SEEK_SET);
        if (fsize > 0 && fsize < 8192) { g_config_cache.resize(fsize); fread(&g_config_cache[0], 1, fsize, f1); }
        fclose(f1);
    }
    std::string scriptPath = dir + "/modmenu_script.js";
    FILE* f2 = fopen(scriptPath.c_str(), "r");
    if (f2) {
        fseek(f2, 0, SEEK_END); long fsize = ftell(f2); fseek(f2, 0, SEEK_SET);
        if (fsize > 0 && fsize < 65536) { g_script_cache.resize(fsize); fread(&g_script_cache[0], 1, fsize, f2); }
        fclose(f2);
    }
}

static void PushJsLog(const std::string& line) {
    std::lock_guard<std::mutex> lock(g_js_logMtx);
    g_js_logQueue.push_back(line);
    g_js_totalLogCount++;
    if (g_js_logQueue.size() > 200) g_js_logQueue.pop_front();
}

// 解析 Frida 引擎发送的 JSON 消息体，捕获输出及致命崩溃
static void OnJsMessage(const gchar* message, GBytes* data, gpointer user_data) {
    JsonParser* parser = json_parser_new();
    if (json_parser_load_from_data(parser, message, -1, nullptr)) {
        JsonNode* root_node = json_parser_get_root(parser);
        if (JSON_NODE_HOLDS_OBJECT(root_node)) {
            JsonObject* root = json_node_get_object(root_node);
            const gchar* type = json_object_get_string_member(root, "type");
            if (type && strcmp(type, "send") == 0) {
                const gchar* payload = json_object_get_string_member(root, "payload");
                if (payload) {
                    PushJsLog(payload);
                    LogToFile("JS_SEND", payload);
                }
            } else if (type && strcmp(type, "log") == 0) {
                const gchar* payload = json_object_get_string_member(root, "payload");
                if (payload) {
                    PushJsLog(payload);
                    LogToFile("JS_LOG", payload);
                }
            } else if (type && strcmp(type, "error") == 0) {
                const gchar* desc = json_object_get_string_member(root, "description");
                const gchar* stack = json_object_get_string_member(root, "stack");
                if (desc) {
                    PushJsLog(std::string("[JS_Error] ") + desc);
                    LogToFile("JS_EXC", std::string("Description: ") + desc);
                }
                if (stack) {
                    LogToFile("JS_STACK", stack);
                }
            } else { 
                PushJsLog(std::string("[msg] ") + message); 
                LogToFile("JS_MSG", message);
            }
        }
    }
    g_object_unref(parser);
}

static gboolean ExecuteJsIdle(gpointer user_data) {
    auto* data = (std::pair<std::string, std::string>*)user_data;
    std::string name = data->first;
    std::string code = data->second;
    if (!g_js_backend) return G_SOURCE_REMOVE;
    GError* error = nullptr;
    GumScript* script = gum_script_backend_create_sync(g_js_backend, "user-script", code.c_str(), nullptr, nullptr, &error);
    if (!script) {
        std::string errMsg = std::string("[error] create failed: ") + (error ? error->message : "unknown");
        PushJsLog(errMsg);
        LogToFile("JS_ERR", errMsg);
        if (error) g_error_free(error);
        delete data;
        return G_SOURCE_REMOVE;
    }
    gum_script_set_message_handler(script, OnJsMessage, nullptr, nullptr);
    gum_script_load_sync(script, nullptr);
    std::lock_guard<std::mutex> lock(g_scriptsMtx);
    int new_id = ++g_script_id_counter;
    g_active_scripts.push_back({new_id, name, script});
    PushJsLog("[info] 脚本加载成功: " + name);
    LogToFile("JS_LOAD", "Script loaded: " + name);
    delete data;
    return G_SOURCE_REMOVE;
}

static gboolean StopScriptIdle(gpointer user_data) {
    GumScript* script = (GumScript*)user_data;
    if (script) {
        gum_script_unload_sync(script, nullptr);
        g_object_unref(script);
    }
    return G_SOURCE_REMOVE;
}

std::vector<std::pair<int, std::string>> GetActiveScripts() {
    std::lock_guard<std::mutex> lock(g_scriptsMtx);
    std::vector<std::pair<int, std::string>> out;
    for (auto& s : g_active_scripts) out.push_back({s.id, s.name});
    return out;
}

void StopScript(int id) {
    GumScript* scriptToUnload = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_scriptsMtx);
        for (auto it = g_active_scripts.begin(); it != g_active_scripts.end(); ++it) {
            if (it->id == id) {
                scriptToUnload = it->script;
                g_active_scripts.erase(it);
                break;
            }
        }
    }
    if (scriptToUnload) {
        GSource* src = g_idle_source_new();
        g_source_set_callback(src, StopScriptIdle, scriptToUnload, nullptr);
        g_source_attach(src, g_js_context);
        g_source_unref(src);
        PushJsLog("[info] 已停止脚本，ID: " + std::to_string(id));
    }
}

void StopAllScripts() {
    std::vector<GumScript*> scriptsToUnload;
    {
        std::lock_guard<std::mutex> lock(g_scriptsMtx);
        for (auto& s : g_active_scripts) scriptsToUnload.push_back(s.script);
        g_active_scripts.clear();
    }
    for (auto script : scriptsToUnload) {
        GSource* src = g_idle_source_new();
        g_source_set_callback(src, StopScriptIdle, script, nullptr);
        g_source_attach(src, g_js_context);
        g_source_unref(src);
    }
    PushJsLog("[info] 已停止并清理所有脚本");
}

void HideFridaThreadsLoop() {
    while (true) {
        sleep(1);
        DIR* dir = opendir("/proc/self/task");
        if (dir) {
            struct dirent* ent;
            while ((ent = readdir(dir)) != NULL) {
                if (ent->d_name[0] == '.') continue;
                std::string comm_path = std::string("/proc/self/task/") + ent->d_name + "/comm";
                std::ifstream comm_file(comm_path);
                if (comm_file.is_open()) {
                    std::string name;
                    std::getline(comm_file, name);
                    comm_file.close();
                    if (name.find("gum-js-loop") != std::string::npos || 
                        name.find("pool-frida") != std::string::npos ||
                        name.find("frida") != std::string::npos) {
                        std::ofstream out(comm_path);
                        if (out.is_open()) {
                            out << "worker\n"; 
                            out.close();
                        }
                    }
                }
            }
            closedir(dir);
        }
    }
}

// Frida V8 引擎线程入口，初始化 GumJS 并启动 GLib 主循环
static void JsThreadEntry() {
    pthread_setname_np(pthread_self(), "worker");
    gum_init_embedded();
    g_js_context = g_main_context_new();
    g_main_context_push_thread_default(g_js_context);
    g_js_backend = gum_script_backend_obtain_v8();
    if (!g_js_backend) { 
        PushJsLog("[error] Failed to obtain V8 backend"); 
        LogToFile("ENGINE", "Failed to obtain V8 backend");
        return; 
    }
    g_js_loop = g_main_loop_new(g_js_context, FALSE);
    g_js_ready = true;
    PushJsLog("[info] JsEngine is ready");
    LogToFile("ENGINE", "JsEngine is ready");
    g_main_loop_run(g_js_loop);
    g_main_context_pop_thread_default(g_js_context);
}

void ExecuteJs(const std::string& userCode, const std::string& name) {
    if (!g_js_ready || !g_js_context) { 
        PushJsLog("[error] engine not ready"); 
        LogToFile("ENGINE", "ExecuteJs called before engine ready");
        return; 
    }
    std::string fullCode = "if (typeof console === 'undefined') {\n  var console = { log: function() {\n    var parts = [];\n    for (var i = 0; i < arguments.length; i++)\n      parts.push(String(arguments[i]));\n    send(parts.join(' '));\n  } };\n}\n" + userCode;
    std::string display_name = name;
    if (display_name.empty() || display_name == "自定义脚本") {
        display_name = userCode.substr(0, 15);
        for (char& c : display_name) if (c == '\n' || c == '\r') c = ' ';
        display_name += "...";
    }
    auto* heap = new std::pair<std::string, std::string>(display_name, fullCode);
    GSource* src = g_idle_source_new();
    g_source_set_callback(src, ExecuteJsIdle, heap, nullptr);
    g_source_attach(src, g_js_context);
    g_source_unref(src);
}

void InitJsEngine() { 
    if (g_js_ready) return; 
    std::thread(HideFridaThreadsLoop).detach();
    std::thread(JsThreadEntry).detach(); 
}


jobject get_context(JNIEnv* env);
jclass LoadClassFromLoader(JNIEnv* env, jobject loader, const char* className);
void RegisterRendererNatives(JNIEnv* env, jclass rendererCls);

jobject get_context(JNIEnv* env) {
    jclass cls = env->FindClass("android/app/ActivityThread");
    jmethodID mid1 = env->GetStaticMethodID(cls, "currentActivityThread", "()Landroid/app/ActivityThread;");
    jobject at = env->CallStaticObjectMethod(cls, mid1);
    jmethodID mid2 = env->GetMethodID(cls, "getApplication", "()Landroid/app/Application;");
    return env->CallObjectMethod(at, mid2);
}

// 使用 DexClassLoader 从私有目录加载 DEX
jobject LoadDexLoader(JNIEnv* env, const std::string& cacheDir) {
    std::string dexPath = cacheDir + "/.tmp_mod.dex";
    std::string optDir = cacheDir + "/oat";
    
    remove(dexPath.c_str());

    FILE* fp = fopen(dexPath.c_str(), "wb");
    if (!fp) {
        LogToFile("DEX_ERR", "fopen dexPath failed.");
        return nullptr;
    }
    fwrite(dex_data_start, 1, dex_data_end - dex_data_start, fp);
    fclose(fp);

    // 规避 Android 14+ (API 34) 限制加载可写 DEX 文件的安全策略
    chmod(dexPath.c_str(), 0444);
    mkdir(optDir.c_str(), 0777);

    jclass threadCls = env->FindClass("java/lang/Thread");
    jmethodID curThreadMid = env->GetStaticMethodID(threadCls, "currentThread", "()Ljava/lang/Thread;");
    jobject thread = env->CallStaticObjectMethod(threadCls, curThreadMid);
    jmethodID getLoaderMid = env->GetMethodID(threadCls, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
    jobject parentLoader = env->CallObjectMethod(thread, getLoaderMid);

    jclass loaderCls = env->FindClass("dalvik/system/DexClassLoader");
    jmethodID ctor = env->GetMethodID(loaderCls, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");
    jstring jDexPath = env->NewStringUTF(dexPath.c_str());
    jstring jOptPath = env->NewStringUTF(optDir.c_str());
    
    jobject loader = env->NewObject(loaderCls, ctor, jDexPath, jOptPath, nullptr, parentLoader);
    env->DeleteLocalRef(jDexPath);
    env->DeleteLocalRef(jOptPath);

    if (env->ExceptionCheck()) { 
        CheckJniException(env, "LoadDexLoader");
        return nullptr; 
    }
    return loader;
}

jclass LoadClassFromLoader(JNIEnv* env, jobject loader, const char* className) {
    jclass loaderCls = env->GetObjectClass(loader);
    jmethodID loadClassMid = env->GetMethodID(loaderCls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring jName = env->NewStringUTF(className);
    jclass result = (jclass)env->CallObjectMethod(loader, loadClassMid, jName);
    env->DeleteLocalRef(jName);
    env->DeleteLocalRef(loaderCls);
    return result;
}

void native_on_surface_created(JNIEnv* env, jclass clazz) { UIManager::Init(); }
void native_on_surface_changed(JNIEnv* env, jclass clazz, jint w, jint h) { UIManager::OnSurfaceChanged(w, h); }
void native_on_draw_frame(JNIEnv* env, jclass clazz) { UIManager::OnFrame(); }
jboolean native_on_touch(JNIEnv* env, jclass clazz, jint action, jfloat x, jfloat y) { return InputHandler::Handle(action, x, y) ? JNI_TRUE : JNI_FALSE; }

void native_on_ime_text(JNIEnv* env, jclass clazz, jstring text) {
    if (!text) return;
    const char* str = env->GetStringUTFChars(text, nullptr);
    if (str) {
        KeyboardBridge::OnImeText(str);
        env->ReleaseStringUTFChars(text, str);
    }
}
void native_on_keyboard_closed(JNIEnv* env, jclass clazz) { KeyboardBridge::OnJavaKeyboardClosed(); }

void RegisterRendererNatives(JNIEnv* env, jclass rendererCls) {
    JNINativeMethod methods[] = {
        { "nativeOnSurfaceCreated", "()V",          (void*)native_on_surface_created },
        { "nativeOnSurfaceChanged", "(II)V",        (void*)native_on_surface_changed },
        { "nativeOnDrawFrame",      "()V",          (void*)native_on_draw_frame },
        { "nativeOnTouch",          "(IFF)Z",       (void*)native_on_touch },
        { "nativeOnImeText",        "(Ljava/lang/String;)V", (void*)native_on_ime_text },
        { "nativeOnKeyboardClosed", "()V",          (void*)native_on_keyboard_closed },
    };
    env->RegisterNatives(rendererCls, methods, 6);
}

static void* init_thread(void* arg) {
    JNIEnv* env = nullptr;
    g_vm->AttachCurrentThread(&env, nullptr);

    InitJsEngine();

    const char* targetCls = "com.example.application3.MyRenderer";
    std::string cacheDir = "";
    bool configChecked = false;

    // 轮询等待 ActivityThread 与 Application 初始化完成
    for (int retry = 0; retry < 20; retry++) {
        sleep(1);
        jobject context = get_context(env);
        if (!context) { env->ExceptionClear(); continue; }

        if (cacheDir.empty()) {
            jclass contextCls = env->GetObjectClass(context);
            jmethodID getCacheDirMid = env->GetMethodID(contextCls, "getCacheDir", "()Ljava/io/File;");
            jobject cacheDirFile = env->CallObjectMethod(context, getCacheDirMid);
            if (cacheDirFile) {
                jclass fileCls = env->GetObjectClass(cacheDirFile);
                jmethodID getAbsolutePathMid = env->GetMethodID(fileCls, "getAbsolutePath", "()Ljava/lang/String;");
                jstring pathStr = (jstring)env->CallObjectMethod(cacheDirFile, getAbsolutePathMid);
                if (pathStr) {
                    const char* pathChars = env->GetStringUTFChars(pathStr, nullptr);
                    cacheDir = std::string(pathChars);
                    env->ReleaseStringUTFChars(pathStr, pathChars);
                }
                env->DeleteLocalRef(cacheDirFile);
            }
            env->DeleteLocalRef(contextCls);
            
            jclass contextCls2 = env->GetObjectClass(context);
            // 优先获取外部私有目录 (/storage/emulated/0/Android/data/包名/files/)，便于用户直接访问日志
            jmethodID getExtFilesDirMid = env->GetMethodID(contextCls2, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
            jobject extFilesDir = env->CallObjectMethod(context, getExtFilesDirMid, nullptr);
            
            // 外部目录不可用时，回退到内部私有目录 (需 Root 才能访问)
            if (!extFilesDir) {
                env->ExceptionClear();
                jmethodID getFilesDirMid = env->GetMethodID(contextCls2, "getFilesDir", "()Ljava/io/File;");
                extFilesDir = env->CallObjectMethod(context, getFilesDirMid);
            }

            if (extFilesDir) {
                jclass fileCls = env->GetObjectClass(extFilesDir);
                jmethodID getAbsolutePathMid = env->GetMethodID(fileCls, "getAbsolutePath", "()Ljava/lang/String;");
                jstring pathStr = (jstring)env->CallObjectMethod(extFilesDir, getAbsolutePathMid);
                if (pathStr) {
                    const char* pathChars = env->GetStringUTFChars(pathStr, nullptr);
                    g_files_dir = std::string(pathChars) + "/modmenu_log.txt";
                    g_logFilePath = std::string(pathChars) + "/Frida_IMGUI_Log.txt";
                    env->ReleaseStringUTFChars(pathStr, pathChars);
                }
                env->DeleteLocalRef(extFilesDir);
            }
            env->DeleteLocalRef(contextCls2);
            
            LogToFile("INIT", "Target Context acquired. Log path: " + g_logFilePath);
            LoadConfigToCache();
        }

        if (cacheDir.empty()) continue;

        if (!configChecked) {
            configChecked = true;
            g_hide_injection_features = (g_config_cache.find("hide_injection=1") != std::string::npos);

            if (g_hide_injection_features) {
                LogToFile("INIT", "Hide Injection enabled. Waiting for JsEngine...");
                for (int i = 0; i < 50 && !g_js_ready.load(); i++) {
                    usleep(100000);
                }
                
                if (g_js_ready) {
                    ExecuteJs(INJECTION_HIDE_JS, "防扫描模式");
                    LogToFile("INIT", "INJECTION_HIDE_JS executed.");
                } else {
                    LogToFile("ERROR", "JsEngine not ready, injection aborted.");
                }
            }
        }

        LogToFile("INIT", "Start unpacking and loading DEX...");
        jobject loader = LoadDexLoader(env, cacheDir);
        if (!loader) continue;

        jclass rendererCls = LoadClassFromLoader(env, loader, targetCls);
        
        
        std::string dexPath = cacheDir + "/.tmp_mod.dex";
        unlink(dexPath.c_str());

        if (!rendererCls) { 
            CheckJniException(env, "LoadClassFromLoader");
            LogToFile("ERROR", "Failed to find class: " + std::string(targetCls));
            continue; 
        }

        LogToFile("INIT", "Class loaded successfully. Registering native methods...");
        RegisterRendererNatives(env, rendererCls);
        KeyboardBridge::Init(env, rendererCls, g_vm);

        jmethodID getActivityMid = env->GetStaticMethodID(rendererCls, "getActivity", "()Landroid/app/Activity;");
        jobject activity = env->CallStaticObjectMethod(rendererCls, getActivityMid);
        if (!activity) { 
            CheckJniException(env, "getActivity");
            LogToFile("ERROR", "Failed to get Activity via reflection.");
            env->DeleteLocalRef(rendererCls);
            continue; 
        }

        LogToFile("INIT", "Activity acquired. Mounting UI via createAndAttach...");
        jmethodID createAndAttachMid = env->GetStaticMethodID(rendererCls, "createAndAttach", "(Landroid/app/Activity;)V");
        env->CallStaticVoidMethod(rendererCls, createAndAttachMid, activity);
        
        CheckJniException(env, "createAndAttach");
        LogToFile("INIT", "Mount process completed. UI should be active now.");
        
        env->DeleteLocalRef(activity);
        env->DeleteLocalRef(rendererCls);
        break;
    }

    g_vm->DetachCurrentThread();
    return nullptr;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_vm = vm;
    pthread_t tid;
    pthread_create(&tid, nullptr, init_thread, nullptr);
    return JNI_VERSION_1_6;
}