#include "presets.h"

const char* JS_THREAD_CALL = R"raw(
var base = Process.getModuleByName("{{SO_NAME}}").base;
if (base === null) {
    console.log("[Error] 模块未加载");
} else {
    var hookAddr = base.add(ptr("{{HOOK_OFFSET}}"));
    var targetAddr = base.add(ptr("{{TARGET_OFFSET}}"));
    var types = {{ARG_TYPES}};
    var args = {{ARG_VALUES}};
    var maxCall = {{CALL_COUNT}};
    var interval = {{INTERVAL_MS}};

    try {
        var targetFunc = new NativeFunction(targetAddr, "void", types);
        var count = 0;
        var lastCallTime = 0;
        var listener = null;

        listener = Interceptor.attach(hookAddr, {
            onEnter: function(hookArgs) {
                if (count >= maxCall) {
                    if (listener) {
                        listener.detach();
                        console.log("[Info] 调用达到上限，Hook 已恢复");
                    }
                    return;
                }

                var now = Date.now();
                if (now - lastCallTime >= interval) {
                    lastCallTime = now;
                    count++;
                    try {
                        targetFunc.apply(null, args);
                        console.log("[+] 第 " + count + " 次调用成功");
                    } catch(e) {
                        console.log("[Error] 调用异常: " + e.message);
                    }
                    
                    if (count >= maxCall) {
                        listener.detach();
                        console.log("[Info] 调用达到上限，Hook 已恢复");
                    }
                }
            }
        });
        console.log("[Info] Hook 安装成功，等待跳板触发...");
    } catch(e) {
        console.log("[Error] 初始化异常: " + e.message);
    }
}
)raw";

const char* JS_DIRECT_CALL = R"raw(
var base = Process.getModuleByName("{{SO_NAME}}").base;
if (base === null) {
    console.log("[Error] 模块未加载");
} else {
    var targetAddr = base.add(ptr("{{TARGET_OFFSET}}"));
    var types = {{ARG_TYPES}};
    var args = {{ARG_VALUES}};
    var maxCall = {{CALL_COUNT}};
    var interval = {{INTERVAL_MS}};

    try {
        var targetFunc = new NativeFunction(targetAddr, "void", types);
        var count = 0;
        
        function step() {
            if (count >= maxCall) {
                console.log("[Info] 所有调用执行完毕。");
                return;
            }
            try {
                targetFunc.apply(null, args);
                console.log("[+] 第 " + (count + 1) + " 次直调成功");
            } catch(e) {
                console.log("[Error] 第 " + (count + 1) + " 次直调异常: " + e.message);
            }
            count++;
            if (count < maxCall) {
                setTimeout(step, interval);
            }
        }
        setTimeout(step, 100);
    } catch(e) {
        console.log("[Error] 初始化异常: " + e.message);
    }
}
)raw";

const char* JS_FULL_DUMP = R"raw(
var outputPath = "";
function log(msg) { console.log("[FullDump] " + msg); }
Java.perform(function() {
    try {
        var app = Java.use('android.app.ActivityThread').currentApplication();
        var filesDir = app.getExternalFilesDir(null) || app.getFilesDir();
        outputPath = filesDir.getAbsolutePath() + "/il2cpp_full_dump.txt";
        log("目标输出路径: " + outputPath);
        startDump();
    } catch(e) { log("获取私有目录失败: " + e.message); }
});
function startDump() {
    try {
        var mod = Process.getModuleByName("libil2cpp.so");
        if (!mod) { log("找不到 libil2cpp.so"); return; }
        function getApi(name, retType, argTypes) {
            var ptr = mod.findExportByName(name);
            return ptr ? new NativeFunction(ptr, retType, argTypes) : null;
        }
        var domain_get = getApi("il2cpp_domain_get", 'pointer', []);
        var domain_get_assemblies = getApi("il2cpp_domain_get_assemblies", 'pointer', ['pointer', 'pointer']);
        var assembly_get_image = getApi("il2cpp_assembly_get_image", 'pointer', ['pointer']);
        var image_get_name = getApi("il2cpp_image_get_name", 'pointer', ['pointer']);
        var image_get_class_count = getApi("il2cpp_image_get_class_count", 'uint32', ['pointer']);
        var image_get_class = getApi("il2cpp_image_get_class", 'pointer', ['pointer', 'uint32']);
        var class_get_name = getApi("il2cpp_class_get_name", 'pointer', ['pointer']);
        var class_get_namespace = getApi("il2cpp_class_get_namespace", 'pointer', ['pointer']);
        var class_get_fields = getApi("il2cpp_class_get_fields", 'pointer', ['pointer', 'pointer']);
        var class_get_methods = getApi("il2cpp_class_get_methods", 'pointer', ['pointer', 'pointer']);
        var field_get_name = getApi("il2cpp_field_get_name", 'pointer', ['pointer']);
        var field_get_offset = getApi("il2cpp_field_get_offset", 'int', ['pointer']);
        var method_get_name = getApi("il2cpp_method_get_name", 'pointer', ['pointer']);
        var class_get_parent = getApi("il2cpp_class_get_parent", 'pointer', ['pointer']);
        var field_get_type = getApi("il2cpp_field_get_type", 'pointer', ['pointer']);
        var type_get_name = getApi("il2cpp_type_get_name", 'pointer', ['pointer']);
        var method_is_static = getApi("il2cpp_method_is_static", 'bool', ['pointer']);
        var method_get_return_type = getApi("il2cpp_method_get_return_type", 'pointer', ['pointer']);
        var method_get_param_count = getApi("il2cpp_method_get_param_count", 'uint32', ['pointer']);
        var method_get_param = getApi("il2cpp_method_get_param", 'pointer', ['pointer', 'uint32']);
        function safeReadString(ptr) { if (!ptr || ptr.isNull()) return ""; try { return ptr.readCString(); } catch(e) { return ""; } }
        function getTypeStr(typePtr) {
            if (!typePtr || typePtr.isNull() || !type_get_name) return "Type_Hidden";
            try { var namePtr = type_get_name(typePtr); return namePtr.isNull() ? "Unknown" : namePtr.readCString(); } catch(e) { return "Error"; }
        }
        var domain = domain_get();
        var countPtr = Memory.alloc(4);
        var assemblies = domain_get_assemblies(domain, countPtr);
        var assembly_count = countPtr.readU32();
        var file = new File(outputPath, "wb");
        if (!file) { log("无法创建文件"); return; }
        var currentAssembly = 0;
        var totalClasses = 0;
        function processAssembly() {
            if (currentAssembly >= assembly_count) {
                file.flush(); file.close(); log("全量 Dump 完成。共解析类: " + totalClasses); return;
            }
            try {
                var asm = assemblies.add(currentAssembly * Process.pointerSize).readPointer();
                var img = assembly_get_image(asm);
                var imgName = safeReadString(image_get_name(img));
                if (!imgName) imgName = "UnknownAssembly";
                var classCount = image_get_class_count(img);
                file.write("\n=========================================\nAssembly: " + imgName + "\n=========================================\n");
                for (var i = 0; i < classCount; i++) {
                    try {
                        var klass = image_get_class(img, i);
                        if (klass.isNull()) continue;
                        var ns = safeReadString(class_get_namespace(klass));
                        var clsName = safeReadString(class_get_name(klass));
                        if (!clsName) continue;
                        var fullName = ns ? (ns + "." + clsName) : clsName;
                        var parentName = "None";
                        if (class_get_parent) {
                            var pKlass = class_get_parent(klass);
                            if (!pKlass.isNull()) { parentName = safeReadString(class_get_name(pKlass)); if (!parentName) parentName = "UnknownParent"; }
                        }
                        file.write("\nClass: " + fullName + " : " + parentName + "\n");
                        var fieldIter = Memory.alloc(Process.pointerSize); fieldIter.writePointer(ptr(0)); var field;
                        while ((field = class_get_fields(klass, fieldIter)) && !field.isNull()) {
                            try {
                                var fName = safeReadString(field_get_name(field));
                                if (!fName) continue;
                                var fOffset = field_get_offset(field);
                                var fType = "Type_Hidden";
                                if (field_get_type) fType = getTypeStr(field_get_type(field));
                                file.write("    " + fType + " " + fName + " 0x" + fOffset.toString(16) + "\n");
                            } catch(e) {}
                        }
                        var methodIter = Memory.alloc(Process.pointerSize); methodIter.writePointer(ptr(0)); var method;
                        while ((method = class_get_methods(klass, methodIter)) && !method.isNull()) {
                            try {
                                var mName = safeReadString(method_get_name(method));
                                if (!mName) continue;
                                var mPtr = method.readPointer();
                                var mOffsetStr = "0x0";
                                if (!mPtr.isNull() && mPtr.compare(mod.base) >= 0 && mPtr.compare(mod.base.add(mod.size)) <= 0) { mOffsetStr = "0x" + mPtr.sub(mod.base).toString(16); }
                                var staticStr = "";
                                if (method_is_static && method_is_static(method)) staticStr = "static ";
                                var retType = "void";
                                if (method_get_return_type) retType = getTypeStr(method_get_return_type(method));
                                var paramsStr = "";
                                if (method_get_param_count && method_get_param) {
                                    var pCount = method_get_param_count(method);
                                    for (var p = 0; p < pCount; p++) { paramsStr += getTypeStr(method_get_param(method, p)) + (p < pCount - 1 ? ", " : ""); }
                                }
                                file.write("    " + staticStr + retType + " " + mName + "(" + paramsStr + ") " + mOffsetStr + "\n");
                            } catch(e) {}
                        }
                        totalClasses++;
                    } catch (e) {}
                }
                log("进度: " + (currentAssembly + 1) + "/" + assembly_count + " | 累计类: " + totalClasses);
            } catch (e) {}
            currentAssembly++;
            setTimeout(processAssembly, 0);
        }
        log("开始全量安全 Dump..."); processAssembly();
    } catch (e) { log("致命错误: " + e.message); }
}
)raw";

const char* JS_LITE_DUMP = R"raw(
var outputPath = "";
function log(msg) { console.log("[LiteDump] " + msg); }
Java.perform(function() {
    try {
        var app = Java.use('android.app.ActivityThread').currentApplication();
        var filesDir = app.getExternalFilesDir(null) || app.getFilesDir();
        outputPath = filesDir.getAbsolutePath() + "/il2cpp_lite_dump.txt";
        log("目标输出路径: " + outputPath);
        startDump();
    } catch(e) { log("获取私有目录失败: " + e.message); }
});
function startDump() {
    try {
        var mod = Process.getModuleByName("libil2cpp.so");
        if (!mod) { log("找不到 libil2cpp.so"); return; }
        function getApi(name, retType, argTypes) { var ptr = mod.findExportByName(name); return ptr ? new NativeFunction(ptr, retType, argTypes) : null; }
        var domain_get = getApi("il2cpp_domain_get", 'pointer', []);
        var domain_get_assemblies = getApi("il2cpp_domain_get_assemblies", 'pointer', ['pointer', 'pointer']);
        var assembly_get_image = getApi("il2cpp_assembly_get_image", 'pointer', ['pointer']);
        var image_get_name = getApi("il2cpp_image_get_name", 'pointer', ['pointer']);
        var image_get_class_count = getApi("il2cpp_image_get_class_count", 'uint32', ['pointer']);
        var image_get_class = getApi("il2cpp_image_get_class", 'pointer', ['pointer', 'uint32']);
        var class_get_name = getApi("il2cpp_class_get_name", 'pointer', ['pointer']);
        var class_get_namespace = getApi("il2cpp_class_get_namespace", 'pointer', ['pointer']);
        var class_get_fields = getApi("il2cpp_class_get_fields", 'pointer', ['pointer', 'pointer']);
        var class_get_methods = getApi("il2cpp_class_get_methods", 'pointer', ['pointer', 'pointer']);
        var field_get_name = getApi("il2cpp_field_get_name", 'pointer', ['pointer']);
        var field_get_offset = getApi("il2cpp_field_get_offset", 'int', ['pointer']);
        var method_get_name = getApi("il2cpp_method_get_name", 'pointer', ['pointer']);
        var class_get_parent = getApi("il2cpp_class_get_parent", 'pointer', ['pointer']);
        var field_get_type = getApi("il2cpp_field_get_type", 'pointer', ['pointer']);
        var type_get_name = getApi("il2cpp_type_get_name", 'pointer', ['pointer']);
        var method_is_static = getApi("il2cpp_method_is_static", 'bool', ['pointer']);
        var method_get_return_type = getApi("il2cpp_method_get_return_type", 'pointer', ['pointer']);
        var method_get_param_count = getApi("il2cpp_method_get_param_count", 'uint32', ['pointer']);
        var method_get_param = getApi("il2cpp_method_get_param", 'pointer', ['pointer', 'uint32']);
        function safeReadString(ptr) { if (!ptr || ptr.isNull()) return ""; try { return ptr.readCString(); } catch(e) { return ""; } }
        function getTypeStr(typePtr) {
            if (!typePtr || typePtr.isNull() || !type_get_name) return "Type_Hidden";
            try { var namePtr = type_get_name(typePtr); return namePtr.isNull() ? "Unknown" : namePtr.readCString(); } catch(e) { return "Error"; }
        }
        var domain = domain_get();
        var countPtr = Memory.alloc(4);
        var assemblies = domain_get_assemblies(domain, countPtr);
        var assembly_count = countPtr.readU32();
        var file = new File(outputPath, "wb");
        if (!file) { log("无法创建文件"); return; }
        var currentAssembly = 0;
        var totalClasses = 0;
        function processAssembly() {
            if (currentAssembly >= assembly_count) {
                file.flush(); file.close(); log("轻量 Dump 完成。共解析类: " + totalClasses); return;
            }
            try {
                var asm = assemblies.add(currentAssembly * Process.pointerSize).readPointer();
                var img = assembly_get_image(asm);
                var imgName = safeReadString(image_get_name(img));
                if (imgName === "Assembly-CSharp.dll") {
                    var classCount = image_get_class_count(img);
                    file.write("\nAssembly: " + imgName + "\n");
                    for (var i = 0; i < classCount; i++) {
                        try {
                            var klass = image_get_class(img, i);
                            if (klass.isNull()) continue;
                            var ns = safeReadString(class_get_namespace(klass));
                            var clsName = safeReadString(class_get_name(klass));
                            if (!clsName) continue;
                            if (ns.indexOf("System") === 0 || ns.indexOf("UnityEngine") === 0 || ns.indexOf("Unity.") === 0) continue;
                            var fullName = ns ? (ns + "." + clsName) : clsName;
                            var parentName = "None";
                            if (class_get_parent) {
                                var pKlass = class_get_parent(klass);
                                if (!pKlass.isNull()) { parentName = safeReadString(class_get_name(pKlass)); if (!parentName) parentName = "UnknownParent"; }
                            }
                            file.write("\nClass: " + fullName + " : " + parentName + "\n");
                            var fieldIter = Memory.alloc(Process.pointerSize); fieldIter.writePointer(ptr(0)); var field;
                            while ((field = class_get_fields(klass, fieldIter)) && !field.isNull()) {
                                try {
                                    var fName = safeReadString(field_get_name(field));
                                    if (!fName) continue;
                                    var fOffset = field_get_offset(field);
                                    var fType = "Type_Hidden";
                                    if (field_get_type) fType = getTypeStr(field_get_type(field));
                                    file.write("    " + fType + " " + fName + " 0x" + fOffset.toString(16) + "\n");
                                } catch(e) {}
                            }
                            var methodIter = Memory.alloc(Process.pointerSize); methodIter.writePointer(ptr(0)); var method;
                            while ((method = class_get_methods(klass, methodIter)) && !method.isNull()) {
                                try {
                                    var mName = safeReadString(method_get_name(method));
                                    if (!mName) continue;
                                    var mPtr = method.readPointer();
                                    var mOffsetStr = "0x0";
                                    if (!mPtr.isNull() && mPtr.compare(mod.base) >= 0 && mPtr.compare(mod.base.add(mod.size)) <= 0) { mOffsetStr = "0x" + mPtr.sub(mod.base).toString(16); }
                                    var staticStr = "";
                                    if (method_is_static && method_is_static(method)) staticStr = "static ";
                                    var retType = "void";
                                    if (method_get_return_type) retType = getTypeStr(method_get_return_type(method));
                                    var paramsStr = "";
                                    if (method_get_param_count && method_get_param) {
                                        var pCount = method_get_param_count(method);
                                        for (var p = 0; p < pCount; p++) { paramsStr += getTypeStr(method_get_param(method, p)) + (p < pCount - 1 ? ", " : ""); }
                                    }
                                    file.write("    " + staticStr + retType + " " + mName + "(" + paramsStr + ") " + mOffsetStr + "\n");
                                } catch(e) {}
                            }
                            totalClasses++;
                        } catch (e) {}
                    }
                }
            } catch (e) {}
            currentAssembly++;
            setTimeout(processAssembly, 0);
        }
        log("开始轻量 Dump..."); processAssembly();
    } catch (e) { log("致命错误: " + e.message); }
}
)raw";

const char* JS_ULTRA_LITE_DUMP = R"raw(
var outputPath = "";
function log(msg) { console.log("[UltraLiteDump] " + msg); }
Java.perform(function() {
    try {
        var app = Java.use('android.app.ActivityThread').currentApplication();
        var filesDir = app.getExternalFilesDir(null) || app.getFilesDir();
        outputPath = filesDir.getAbsolutePath() + "/il2cpp_ultra_lite_dump.txt";
        log("目标输出路径: " + outputPath);
        startDump();
    } catch(e) { log("获取私有目录失败: " + e.message); }
});
function startDump() {
    try {
        var mod = Process.getModuleByName("libil2cpp.so");
        if (!mod) { log("找不到 libil2cpp.so"); return; }
        function getApi(name, retType, argTypes) { var ptr = mod.findExportByName(name); return ptr ? new NativeFunction(ptr, retType, argTypes) : null; }
        var domain_get = getApi("il2cpp_domain_get", 'pointer', []);
        var domain_get_assemblies = getApi("il2cpp_domain_get_assemblies", 'pointer', ['pointer', 'pointer']);
        var assembly_get_image = getApi("il2cpp_assembly_get_image", 'pointer', ['pointer']);
        var image_get_name = getApi("il2cpp_image_get_name", 'pointer', ['pointer']);
        var image_get_class_count = getApi("il2cpp_image_get_class_count", 'uint32', ['pointer']);
        var image_get_class = getApi("il2cpp_image_get_class", 'pointer', ['pointer', 'uint32']);
        var class_get_name = getApi("il2cpp_class_get_name", 'pointer', ['pointer']);
        var class_get_namespace = getApi("il2cpp_class_get_namespace", 'pointer', ['pointer']);
        var class_get_fields = getApi("il2cpp_class_get_fields", 'pointer', ['pointer', 'pointer']);
        var class_get_methods = getApi("il2cpp_class_get_methods", 'pointer', ['pointer', 'pointer']);
        var field_get_name = getApi("il2cpp_field_get_name", 'pointer', ['pointer']);
        var method_get_name = getApi("il2cpp_method_get_name", 'pointer', ['pointer']);
        var field_get_type = getApi("il2cpp_field_get_type", 'pointer', ['pointer']);
        var type_get_name = getApi("il2cpp_type_get_name", 'pointer', ['pointer']);
        var method_is_static = getApi("il2cpp_method_is_static", 'bool', ['pointer']);
        var method_get_return_type = getApi("il2cpp_method_get_return_type", 'pointer', ['pointer']);
        var method_get_param_count = getApi("il2cpp_method_get_param_count", 'uint32', ['pointer']);
        var method_get_param = getApi("il2cpp_method_get_param", 'pointer', ['pointer', 'uint32']);
        function safeReadString(ptr) { if (!ptr || ptr.isNull()) return ""; try { return ptr.readCString(); } catch(e) { return ""; } }
        function getTypeStr(typePtr) {
            if (!typePtr || typePtr.isNull() || !type_get_name) return "Type_Hidden";
            try { var namePtr = type_get_name(typePtr); if (namePtr.isNull()) return "Unknown"; var str = namePtr.readCString(); return str.replace(/System\./g, ""); } catch(e) { return "Error"; }
        }
        var domain = domain_get();
        var countPtr = Memory.alloc(4);
        var assemblies = domain_get_assemblies(domain, countPtr);
        var assembly_count = countPtr.readU32();
        var file = new File(outputPath, "wb");
        if (!file) { log("无法创建文件"); return; }
        var currentAssembly = 0;
        var totalClasses = 0;
        function processAssembly() {
            if (currentAssembly >= assembly_count) {
                file.flush(); file.close(); log("超轻量 Dump 完成。共解析类: " + totalClasses); return;
            }
            try {
                var asm = assemblies.add(currentAssembly * Process.pointerSize).readPointer();
                var img = assembly_get_image(asm);
                var imgName = safeReadString(image_get_name(img));
                if (imgName === "Assembly-CSharp.dll") {
                    var classCount = image_get_class_count(img);
                    for (var i = 0; i < classCount; i++) {
                        try {
                            var klass = image_get_class(img, i);
                            if (klass.isNull()) continue;
                            var ns = safeReadString(class_get_namespace(klass));
                            var clsName = safeReadString(class_get_name(klass));
                            if (!clsName) continue;
                            if (ns.indexOf("System") === 0 || ns.indexOf("UnityEngine") === 0 || ns.indexOf("Unity.") === 0) continue;
                            var fullName = ns ? (ns + "." + clsName) : clsName;
                            file.write("\n" + fullName + "\n");
                            var fieldIter = Memory.alloc(Process.pointerSize); fieldIter.writePointer(ptr(0)); var field;
                            while ((field = class_get_fields(klass, fieldIter)) && !field.isNull()) {
                                try {
                                    var fName = safeReadString(field_get_name(field));
                                    if (!fName) continue;
                                    var fType = "Type_Hidden";
                                    if (field_get_type) fType = getTypeStr(field_get_type(field));
                                    file.write("    " + fType + " " + fName + "\n");
                                } catch(e) {}
                            }
                            var methodIter = Memory.alloc(Process.pointerSize); methodIter.writePointer(ptr(0)); var method;
                            while ((method = class_get_methods(klass, methodIter)) && !method.isNull()) {
                                try {
                                    var mName = safeReadString(method_get_name(method));
                                    if (!mName) continue;
                                    var staticStr = "";
                                    if (method_is_static && method_is_static(method)) staticStr = "static ";
                                    var retType = "void";
                                    if (method_get_return_type) retType = getTypeStr(method_get_return_type(method));
                                    var paramsStr = "";
                                    if (method_get_param_count && method_get_param) {
                                        var pCount = method_get_param_count(method);
                                        for (var p = 0; p < pCount; p++) { paramsStr += getTypeStr(method_get_param(method, p)) + (p < pCount - 1 ? ", " : ""); }
                                    }
                                    file.write("    " + staticStr + retType + " " + mName + "(" + paramsStr + ")\n");
                                } catch(e) {}
                            }
                            totalClasses++;
                        } catch (e) {}
                    }
                }
            } catch (e) {}
            currentAssembly++;
            setTimeout(processAssembly, 0);
        }
        log("开始超轻量 Dump..."); processAssembly();
    } catch (e) { log("致命错误: " + e.message); }
}
)raw";

const std::vector<PresetScript>& GetBuiltInScripts() {
    static std::vector<PresetScript> scripts = {
        { "il2cpp 全面 Dump", "导出所有类、方法、字段及偏移", JS_FULL_DUMP },
        { "il2cpp 轻量 Dump", "仅导出 Assembly-CSharp 及核心类，速度快", JS_LITE_DUMP },
        { "il2cpp 超轻量 Dump", "极致压缩体积，去除所有偏移和父类", JS_ULTRA_LITE_DUMP }
    };
    return scripts;
}