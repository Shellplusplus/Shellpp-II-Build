# Shell++ II Build

构建 Shell++ II 的 ARM NuttX 原生模块，并更新同级安装包仓库中的生成资源。

## 用法

将本仓库与 `shellpp-ii`、`shellpp-ii-installer` 放在同一目录。需要 macOS、Apple Clang、Rust 工具链（`rust-lld`）、Python 3、Node.js 和 `sips`。

```sh
./build.sh
```

默认构建目标为 Xiaomi Band 10 Pro 固件 `3.101.036`。成功后生成：

- `out/xiaomi-band-10-pro-3.101.036/shellpp_ii.bin`：已校验的原生模块。
- `../shellpp-ii-installer/_Lua/`：编辑器使用的模块和图标。
- `../shellpp-ii-installer/resources/_lua/_Lua/`：写入 `resource.bin` 的打包资源。

可通过 `CLANG`、`RUST_LLD`、`PYTHON` 和 `NODE` 环境变量覆盖工具路径。
