const char* INJECTION_HIDE_JS = R"raw(
(function() {
    let spoofedMapsPath = "";
    let spoofedSmapsPath = "";
    let isReady = false;

    function generateSpoofedFiles() {
        Java.perform(function() {
            try {
                var app = Java.use('android.app.ActivityThread').currentApplication();
                var cacheDir = app.getCacheDir().getAbsolutePath();
                spoofedMapsPath = cacheDir + "/.cache_maps_view";
                spoofedSmapsPath = cacheDir + "/.cache_smaps_view";

                // 缓存 Maps 数据
                var realMaps = File.readAllText("/proc/self/maps");
                var lines = realMaps.split("\n");
                var sanitizedLines = [];

                for (var i = 0; i < lines.length; i++) {
                    var l = lines[i];
                    if (l.indexOf("rwx") !== -1) l = l.replace("rwx", "---");
                    if (l.indexOf("[anonymous]") !== -1) {
                        l = l.replace("[anonymous]", "[anon:.bss]");
                        if (l.indexOf("r-x") !== -1) l = l.replace("r-x", "---");
                    }
                    if (l.indexOf("dalvik-DEX") !== -1) l = l.replace("dalvik-DEX", "dalvik-XXX");
                    if (l.indexOf(".so") !== -1 && l.indexOf("/data/") !== -1) {
                        var start = l.indexOf("/data/");
                        var end = l.indexOf(".so", start) + 3;
                        if (end > 2 && end > start) {
                            var fakePath = "/system/lib64/libc.so";
                            while (fakePath.length < (end - start)) fakePath += " ";
                            l = l.substring(0, start) + fakePath + l.substring(end);
                        }
                    }
                    sanitizedLines.push(l);
                }
                File.writeAllText(spoofedMapsPath, sanitizedLines.join("\n"));

                // 缓存 Smaps 数据
                try {
                    var realSmaps = File.readAllText("/proc/self/smaps");
                    realSmaps = realSmaps.replace(/Private_Dirty:\s+\d+\s+kB/g, "Private_Dirty:        0 kB");
                    realSmaps = realSmaps.replace(/\/data\/data\/[^\s]+\.so/g, "/system/lib64/libc.so");
                    File.writeAllText(spoofedSmapsPath, realSmaps);
                } catch(e) {
                    File.writeAllText(spoofedSmapsPath, sanitizedLines.join("\n"));
                }
                
                isReady = true;
            } catch(e) {}
        });
    }

    // 重定向底层文件读取操作
    function hookFileOpen() {
        var openPtr = Module.findExportByName(null, "open");
        var openatPtr = Module.findExportByName(null, "openat");
        var fopenPtr = Module.findExportByName(null, "fopen");

        function attachOpenHook(funcPtr, pathIdx) {
            if (!funcPtr) return;
            Interceptor.attach(funcPtr, {
                onEnter: function(args) {
                    if (!isReady) return;
                    var pathPtr = args[pathIdx];
                    if (pathPtr.isNull()) return;
                    try {
                        var path = pathPtr.readCString();
                        if (path && path.indexOf("/proc/") !== -1) {
                            if (path.indexOf("smaps") !== -1) {
                                args[pathIdx] = Memory.allocUtf8String(spoofedSmapsPath);
                            } else if (path.indexOf("maps") !== -1) {
                                args[pathIdx] = Memory.allocUtf8String(spoofedMapsPath);
                            }
                        }
                    } catch(e) {}
                }
            });
        }

        attachOpenHook(openPtr, 0);
        attachOpenHook(openatPtr, 1);
        attachOpenHook(fopenPtr, 0);
    }

    // 处理文件描述符路径查询
    function hookReadlink() {
        var readlinkPtr = Module.findExportByName(null, "readlink");
        var readlinkatPtr = Module.findExportByName(null, "readlinkat");

        function attachReadlinkHook(funcPtr, pathIdx, bufIdx, sizeIdx) {
            if (!funcPtr) return;
            Interceptor.attach(funcPtr, {
                onEnter: function(args) {
                    this.buf = args[bufIdx];
                    this.size = args[sizeIdx].toInt32();
                },
                onLeave: function(retval) {
                    var s = retval.toInt32();
                    if (s > 0 && s <= this.size) {
                        try {
                            var path = this.buf.readUtf8String(s);
                            if (path && (path.indexOf(".cache_maps_view") !== -1 || path.indexOf(".cache_smaps_view") !== -1)) {
                                var fakePath = "/proc/self/maps";
                                this.buf.writeUtf8String(fakePath);
                                retval.replace(ptr(fakePath.length));
                            }
                        } catch(e) {}
                    }
                }
            });
        }
        attachReadlinkHook(readlinkPtr, 0, 1, 2);
        attachReadlinkHook(readlinkatPtr, 1, 2, 3);
    }

    setTimeout(function() {
        generateSpoofedFiles();
        hookFileOpen();
        hookReadlink();
        console.log("[*]防扫描初始化");
    }, 1000);
})();
)raw";