# ForgetWolf 静态网站

本项目的原创代码采用 GNU AGPL-3.0-only，完整条款见 [LICENSE](LICENSE)。
第三方字体、字形数据及其他引用素材继续适用其各自许可证；相关声明保留在资源目录与 SVG 元数据中。

当前版本为纯静态 HTML + 内联 SVG/SMIL，无浏览器 JS/CSS，无 CGI 运行依赖。
发布目录为 `dist/`；默认主页与 desktop/mobile 均按英语→俄语→中文播放。

首次构建先安装离线生成器依赖：`python -m pip install --target .tools -r requirements-build.txt`。
构建：`python build_static.py`；验证：`python verify_static.py`。
仅导出已有生成结果：`python build_static.py --export-only`。
从工程目录运行 `server.exe`，它只读取 dist/。旧 CGI 源码保留作历史参考。

---

以下为早期 CGI 版本的历史说明，不代表当前预览服务器的工作方式。

# C++ CGI 手写问候

Windows 本地最小示例，CGI 程序和预览服务器均为 C++。

运行（需要 PATH 中有 MinGW g++）：

```powershell
powershell -ExecutionPolicy Bypass -File .\run.ps1
```

打开 http://127.0.0.1:8080/ ，以 Hershey Script 原始单线字形逐笔写出
`Hello, ForgetWolf here.`。完整动画约 9.36 秒，含淡入、停留与淡出；刷新重播。
速度参考与原始参数记录见 `TIMING.md` 和 `timing-baseline/`。
也可访问 http://127.0.0.1:8080/cgi-bin/hello.exe 。
俄语独立预览：http://127.0.0.1:8080/ru ，或 `/cgi-bin/ru.exe`。
英语独立地址：http://127.0.0.1:8080/en （首页仍为英语）。目前不做语言轮播。
在运行窗口按 Ctrl+C 停止。端口 8080 必须空闲。

`hello.cpp` 输出 HTML 和内嵌 SVG；`server.cpp` 接收本地请求，启动 CGI
可执行文件并将输出返回浏览器。服务器仅供本地预览。

页面使用 SVG 原生 SMIL 动画，没有 JavaScript、CSS 样式表或 style 属性。
`color-scheme` 元信息和 `CanvasText` 系统色使背景及笔迹跟随浏览器配色偏好；
深色背景的具体颜色由浏览器决定。

`greeting.hpp` 已包含生成好的笔画，编译和运行不需要 Python 或外部字体。
修改英文文案和时间可编辑 `languages/en.json`，然后执行
`python generate_greeting.py` 并重新编译。

字体来源：Windell H. Oskay 维护的
[HersheyScript1.svg](https://gitlab.com/oskay/svg-fonts/-/blob/master/fonts/Hershey/HersheyScript1.svg)。
原始文件及署名保存在 `HersheyScript1.svg`，生成的 SVG 也包含原始署名与许可说明。

## 独立俄语页

文案：`Здравствуйте, здесь ФоргетВолк.`，全部统一线宽。
使用 [peko/GOST-FONT](https://github.com/peko/GOST-FONT) 的原始骨架坐标。
`GOST-font.coffee` 仅作为数据读取，不运行 CoffeeScript；相对坐标转换为绝对
SVG 坐标，保留原始几何形状。字距与空格由生成器安排。

- `languages/ru.json`：独立文案及时间配置。
- `generate_russian.py`：生成 `russian.hpp`，仅修改数据后才需要 Python。
- `ru.cpp`：独立 C++ CGI，和英语 `hello.cpp` 分开引用。
- `run.ps1`：同时编译两种语言及本地服务器。

俄语页总时长约 9.363 秒，等于当前英语从加载到写完的时长：等待 0.4 秒，
书写约 8.063 秒（含抬笔间隔），完整停留 0.5 秒，淡出 0.4 秒。
起笔渐显 0.3 秒与书写重叠，不额外延长。播放一次后留白，刷新重播。
所有笔画都在淡出前写完。英语也采用相同的时间安排，两个页面总时长不变。

后续轮播可由统一的语言列表引用这些独立数据并决定顺序，目前不引入轮播逻辑。

## 独立中文页

http://127.0.0.1:8080/zh （也支持 `/zh/`、`/cgi-bin/zh.exe`）。
文案：`你好，我是ForgetWolf`。中文和逗号从用户最新的两张手写图提取轮廓，
经过轻微平滑；英文仍用原有 Hershey Script。只保留单字内部连笔，字与字之间
保留实际间隙和抬笔，不添加跨字、跨标点或中英文之间的连接。

- `lettering/source/`：用户手写参考副本。
- `trace_handwriting.py`：轮廓提取、按手工审核的笔势分配笔画区域并生成 SVG。
- `lettering/ni.svg`、`wo.svg`、`comma.svg`：独立矢量字形及动画。
- `lettering/previous-dpsk/`：替换前的 DeepSeek SVG 备份。
- `languages/zh.json`：分段时间配置。
- `generate_chinese.py`：生成 `chinese.hpp`；`zh.cpp` 输出 CGI 页面。
- `verify_chinese_frames.py`：离线解析本项目的 SMIL 时间轴，生成视觉检查帧；
  不是浏览器实测。完整字形预览在 `lettering/zh-final-proof.png`。

重新描摹需要 Python、Pillow、numpy、vtracer、resvg-py；离线渲染需要 resvg-py。
这些仅用于制作，不是 C++ 编译或网站运行依赖。源图不会嵌入网页。

中文含逗号共 25 段书写轨迹（连笔按实际笔势分组，并非楷书标准笔画计数）。
轮廓和每段墨迹分别矢量化；遮罩内的局部矢量墨迹按沿笔路线的进度依次显露，
避免宽描线遮罩在回折和交叉处同时扫开远处墨迹。整字轮廓裁切保留最终外形。
每一笔的所有显露事件都限制在自身时间段内，不与下一笔重叠。
未开始的笔段完全隐藏；深色模式下笔形自动变白。

“你好”约 2.4 秒，“我是”约 2.95 秒，逗号约 0.25 秒，分段间隔 0.05 秒。
英文名使用剩余书写时间。完整时间仍约 9.363 秒，包含起笔等待、渐显、
完整停留和淡出。英语和俄语页面没有改动。

速度是参考视频整体节奏的近似：用户给的 B 站视频在开头已完成“你好”，
无法从该片段量出完整中文书写时间，2.4 秒不是苹果官方参数或精确测量。


中文行笔复核：参考教育部笔顺学习网的“你、好、我、是”字页作为基本方向参考，
不使用其字形或动画素材；保留用户手写图中的行书合并笔势。
“我”现在为 5 段连续笔势：撇接横、竖钩、提、斜钩（下行后向上收细）、最后撇。
不再追加重复描粗的末笔；左竖与后画的提分区，斜钩与最后撇的交叉墨迹按行笔先后归属。
来源：https://stroke-order.learningweb.moe.edu.tw/dictView.jsp?ID=25105&la=0
慢速离线检查动画：`lettering/wo-slow-proof.gif`（非浏览器录屏）。

“我”的斜钩现使用一条连续贝塞尔轮廓，避免交叉分区和外扩产生毛刺；仍按原方向和原时长显露。

斜钩与末撇的交点使用真实笔画叠压：两笔各有连续轮廓，交点允许重叠，不再互相切走像素，以免平滑后留下白缝。


“我、是”整体复核：`是` 的 7 段动作各有独立、平滑的贝塞尔轮廓，
日字头不再使用下方笔画的像素；下行竖笔、左撇和捺独立显露，交点允许自然叠压。
`我` 的斜钩使用连续下行/上行显露前沿，转折只进行一次，不借用回折附近的提前墨迹。
离线慢速检查：`python verify_wo_shi.py`，生成 `lettering/wo-shi-slow-proof.gif`。

“我”的提笔采用独立连续贝塞尔轮廓，收尾伸入斜笔交点，避免旧分区截短和交点缺口。

“是”的日底横/Z/下行使用四段独立墨迹范围和连续前沿，横画、斜下、折回、竖下按序播放，避免邻近折返段提前凸出。
