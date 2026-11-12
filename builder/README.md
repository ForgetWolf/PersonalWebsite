# C++ 静态站点构建

构建从 Markdown 和动画源数据直接生成静态网站，无需 Python。C++ 用 `greeting.hpp`、`russian.hpp` 和 `chinese.hpp` 生成桌面和移动端的六种欢迎顺序。移动端沿用各行现有的笔画分组，构建时根据路径计算英俄文的行位置。

首页构建会离线执行 `random/message.bf`，核对 `source/main.md`，再把纸带、指针和文字的变化写成 SVG 动画。输出继续使用可复制的 SVG 文字。终端的几何、菜单和标题从 `templates/home-*.svg` 读取，纸带数字和字符使用 `templates/din-glyphs.xml` 中从现有 DIN 字体提取的字形。

C++17 使用 libxml2 组装首页，使用 cmark-gfm 渲染子页正文。子页标题和导航沿用 `templates/` 里的 SVG，CMake 负责复制 CSS、字体和 favicon。cmark-gfm 支持表格、删除线和脚注。默认安全渲染会省略 Markdown 中的原始 HTML。

从 PersonalWebsite 根目录运行。需要 C++17 工具链、用于编译 cmark-gfm 的 C 编译器、libxml2 开发库、CMake 3.21 或以上，以及 Ninja。GitHub Pages 使用同一打包目标。

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target package
```

首次配置时 CMake 会从 GitHub 获取固定版本的 cmark-gfm 并校验 SHA-256。也可以通过 `-DCMARK_ARCHIVE=<归档路径>` 使用已下载的同一份归档。Windows 上请用正斜杠填写该路径。

输出在仓库根目录的 `dist/`。Windows 上可从仓库根目录运行编译好的 `server.exe` 预览。

可用相同输入生成原 Python 版作对照，核对 SVG 节点、动画属性和输出文字。比较 SVG 坐标时允许等价的数字写法，例如 `101` 和 `101.0`。从干净目录构建时只需源文件和静态素材，无需预先生成首页 SVG 或子页 HTML。

## 重制 GOST 字体

安装支持 WOFF 和 WOFF2 导出的 FontForge 后，从仓库根目录运行以下命令。

```powershell
cmake --build build --target gost-fonts
```

如果 FontForge 不在 PATH，可在配置时用 `-DFONTFORGE_EXECUTABLE=<路径>` 指定命令。C++ 工具读取 `GOST-font.coffee` 并生成 66 个俄文字形的 SFD，随后由 FontForge 导出网页字体。SFD、WOFF 和 WOFF2 都放在 `build/font-rebuild/` 供检查。日常打包读取 `pages/fonts/` 里的字体。
