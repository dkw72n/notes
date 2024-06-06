当 ro.debuggable 为 0 时，可通过 patch zygote 启用 GLES layer


调试层加载判定代码如下：
```
private boolean debugLayerEnabled(Bundle coreSettings, String packageName, ApplicationInfo ai) {
    // Only enable additional debug functionality if the following conditions are met:
    // 1. App is debuggable or device is rooted or layer injection metadata flag is true
    // 2. ENABLE_GPU_DEBUG_LAYERS is true
    // 3. Package name is equal to GPU_DEBUG_APP
    if (!isDebuggable() && !canInjectLayers(ai)) {
        return false;
    }
    final int enable = coreSettings.getInt(Settings.Global.ENABLE_GPU_DEBUG_LAYERS, 0);
    if (enable == 0) {
        return false;
    }
    final String gpuDebugApp = coreSettings.getString(Settings.Global.GPU_DEBUG_APP, "");
    if (packageName == null
            || (gpuDebugApp.isEmpty() || packageName.isEmpty())
            || !gpuDebugApp.equals(packageName)) {
        return false;
    }
    return true;
}
```

可以看到，有个 `isDebuggale()` 函数，这个函数碰巧是 native 实现，可稳定 hook。

```
bool isDebuggable_native() {
    return android::GraphicsEnv::getInstance().isDebuggable();
}
```
只需要 patch 这个函数即可。

这里直接 patch 成 200080d2c0035fd6 即 `mov x0, #1; ret;` 即可绕过现在，对任意应用开启 GLES Layer。

