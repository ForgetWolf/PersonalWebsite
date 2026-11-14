# C++ 静态站点构建

日常构建需要 C++17、C 编译器、libxml2 开发库、CMake 3.21 或更新版本和 Ninja。CMake 默认获取固定版本的 cmark-gfm 并核对 SHA-256。已有相同归档时，可在配置时使用 `-DCMARK_ARCHIVE=<归档路径>`。

从仓库根目录执行，生成的站点在 `dist/`。GitHub Pages 使用同一个 `site` 目标。

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target site
```

欢迎页沿用原始 SVG 字形和时序，构建时生成 CSS。欢迎结束或手动跳过由两条 SVG SMIL 指令衔接。首页终端在构建时执行 `random/message.bf`，核对 `source/main.md`，再生成逐字、纸带和光标的 CSS 动画。子页 Markdown 由 cmark-gfm 渲染。FontForge 只用于重制 GOST 字体。

Windows 本地预览从仓库根目录执行以下命令，地址为 http://127.0.0.1:8087/ 。

```powershell
cmake --build build --target preview
.\build\preview.exe
```

## 重制 GOST 字体

安装支持 WOFF 和 WOFF2 导出的 FontForge 后，从仓库根目录运行以下命令。

```powershell
cmake --build build --target gost-fonts
```

如果 FontForge 不在 PATH，可在配置时用 `-DFONTFORGE_EXECUTABLE=<路径>` 指定命令。C++ 工具读取 `GOST-font.coffee` 并生成 66 个俄文字形的 SFD，随后由 FontForge 导出网页字体。SFD、WOFF 和 WOFF2 都放在 `build/font-rebuild/` 供检查。日常打包读取 `pages/fonts/` 里的字体。
