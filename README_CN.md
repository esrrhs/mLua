# mLua

[<img src="https://img.shields.io/github/license/esrrhs/mLua">](https://github.com/esrrhs/mLua)
[<img src="https://img.shields.io/github/languages/top/esrrhs/mLua">](https://github.com/esrrhs/mLua)
[<img src="https://img.shields.io/github/actions/workflow/status/esrrhs/mLua/cmake.yml?branch=master">](https://github.com/esrrhs/mLua/actions)

> **Lua 内存优化工具集：基于 Protobuf 的 C++ Table 固化、内存分析，以及快速 Table 序列化。**

[English](README.md) | [中文说明](README_CN.md)

---

## 简介
**mLua**（`m` 代表 memory）用于降低 Lua 的内存占用与 GC 压力。它可以把 Lua Table 按 Protobuf 布局固化到紧凑的 C++ 结构中，对静态/动态内存占用做分析（输出 gperftools 风格图或火焰图），并提供快速的 Table 序列化 / 反序列化。

## 特性
* **Protobuf → C++ Table**：用 Protobuf 定义结构，将 Lua Table 数据固化到 C++ 容器，减少内存与 GC 开销。
* **静态与动态内存分析**：导出兼容 gperftools 的 profile，或生成火焰图。
* **快速 Table 归档**：高性能序列化 / 反序列化，可选 LZ4 压缩。

## 前置依赖
* **Lua**：5.3+（脚本运行需要解释器；编译 C++ 部分需要开发头文件与库）
* **编译器与构建工具**：CMake (>= 3.12)，支持 C++11 的编译器（推荐 C++17）
* **可选（tools）**：Go 1.21+（自动编译 `plua` / `proto`）、Graphviz、gperftools/`pprof`
* **平台**：Linux、macOS 或 Windows

## 编译
直接运行编译脚本：
```bash
./build.sh
```
或使用现代 CMake 命令：
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```
产物：
* `libmluacore.so` — 动态库 / Lua 模块（同时复制到工程根目录）
* `build/bin/test_bin` — Lua 测试运行器
* `build/bin/plua` / `build/bin/proto` — Go 工具（环境有 Go 时自动编译）

## 使用方法

### 1. Lua Table 固化到 C++
执行演示 / benchmark 脚本：
```bash
cd test
lua test_cpp_table.lua
# 或: ../build/bin/test_bin test_cpp_table.lua
```
按选项对比 Lua 与 C++ 的内存占用与访问开销：
```text
  1: test_get_set
  2: test_benchmark_lua_simple
  3: test_benchmark_cpp_simple
  4: test_benchmark_lua_map
  5: test_benchmark_cpp_map
  6: test_benchmark_lua_array
  7: test_benchmark_cpp_array
```

### 2. 静态内存占用分析
```bash
cd test
lua test_static_perf.lua
```
然后生成可视化结果（见 [可视化与工具](#可视化与工具)）。

示例图片：

![静态内存](test/static_mem.png)

### 3. 动态内存占用分析
```bash
cd test
lua test_dynamic_perf.lua
```
再用 `tools/show.sh` 生成可视化结果。

示例图片：

![动态内存](test/dynamic_mem.png)

### 4. 序列化 / 反序列化
```bash
cd test
lua test_quick_archiver.lua
```
示例输出：
```text
save old data len:      441
is equal: true
init lua mem KB:        86.9228515625
after init data, lua mem KB:    1716.734375
save data len:  177358
after save data, lua mem KB:    379.5
after load data, lua mem KB:    1488.984375
```

---

## 可视化与工具

`tools/` 目录提供将 `.pro` 采样数据转换为火焰图（SVG）及调用图（DOT/PNG）的脚本，流程与 [pLua](https://github.com/esrrhs/pLua) 一致。

### 可选依赖
- **FlameGraph**：若本地及 PATH 中无 `flamegraph.pl`，`show.sh` 会尝试自动下载。CentOS 下可能需要 `yum install perl-open`。
- **Graphviz**（用于生成调用关系 PNG）：`sudo apt install graphviz` / `sudo yum install graphviz`
- **pprof**（用于生成 DOT 调用图）：`sudo apt install google-perftools` / `sudo yum install gperftools`

### 生成可视化
```bash
cd tools
./show.sh ../test
```

会生成：
- `<name>.fl`：折叠栈
- `<name>.svg`：可交互火焰图
- `<name>.prof`：符号化 pprof profile
- `<name>.dot`：Graphviz 调用图
- `<name>.png`：调用图 PNG（安装了 graphviz 与 pprof 时）

`proto`（有 Go 时同样会自动编译）可将 Protobuf schema 转为 C++ Table 使用的 Lua layout：
```bash
cd tools
go build -o proto proto.go
./proto -d ../test -i cpp_table_proto.proto -o ../test/cpp_table_proto.lua
```

---

## 相关项目
* [lua全家桶](https://github.com/esrrhs/lua-family-bucket)

## 许可证
本项目采用 [MIT License](LICENSE)。
