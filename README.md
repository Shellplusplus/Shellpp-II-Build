# Shell++ II Build

本仓库保存 Shell++ II 的固件 target profile、多目标构建流程和静态 ELF 验证工具。

完整技术文档统一位于 [`../Shellpp-ii/docs/README.md`](../Shellpp-ii/docs/README.md)。target profile 字段、完整 ABI 准入条件和新增固件流程分别见中央文档中的 profile 模式与固件适配章节。

执行全部 target 构建：

```sh
./build.sh
```

只构建和发布一个已有 target：

```sh
./build.sh --target xiaomi-band-10-pro-3.101.043
```

全目标模式只在全部 target 成功后部署完整 bin 集；单目标模式只替换指定固件 bin，并保留其他固件产物。两个模式都会同步到：

```text
/Users/ikun_cxkpro/Projects/Shell++/Shellpp-ii-installer/_Lua
/Users/ikun_cxkpro/Projects/Shell++/Shellpp-ii-installer/resources/_lua/_Lua
```

同步后脚本使用 `repack_resource.py` 重建安装器根目录中的 `resource.bin` 和 `hashCode`。构建器不改写 Lua、图标、manifest、`uidmap.map` 或编辑器配置；两套 Lua 和图标副本不一致时构建会失败。

运行 `3.101.043` 专用文件系统主机回归测试：

```sh
sh tests/run_native_fs_host_tests.sh
sh tests/run_native_ui_host_tests.sh
```

两个测试都从共享 nativeApp 源码开始，按文件名顺序重放 043 target 补丁。文件系统测试在 ASan/UBSan 下覆盖目录分页、深层遍历、符号链接和未知类型边界、缓存、应用目录操作以及 CPU/内存解析；UI 测试覆盖独立应用管理 page 8、标题、Activity 导航和 mock 注册表读取。主机 mock 不能证明真实固件 ABI、调用约定、LVGL/Activity 行为或设备栈空间正确。
