# libc++ 实现 `chrono::from_stream` / `chrono::parse` 设计与施工手册

> 目标:在 libc++ 中实现 C++20 `<chrono>` 的解析接口 `from_stream`（每类型重载）
> 和 `parse`（操纵器）。对应提案 **P0355R7 "Extending chrono to Calendars and
> Time Zones"**，标准章节 **[time.parse]** 及各 clock/calendar 的 `*.nonmembers`
> 子条款。libc++ tracking issue: **llvm/llvm-project#99982**（P0355R7 当前为
> *Partial*，输出侧已完成，输入侧缺失）。

---

## 0. 背景与现状

- 输出侧（`operator<<`、`std::format`）已完成：`__chrono/formatter.h`、
  `__chrono/ostream.h`、`__chrono/convert_to_tm.h`（chrono → `tm`）、
  `__chrono/parser_std_format_spec.h`（输出 spec 解析）。
- 输入侧（`from_stream`、`parse`）**完全缺失**：`__chrono/` 下 grep 无
  `from_stream`。这份文档只做输入侧。
- 三大实现里 libstdc++（`bits/chrono_io.h` 的 `_Parser`）和 MSVC STL 都已实现，
  可作对拍参考。**注意：`to_stream` 不在标准里，不要实现**。

### 标准接口清单（要覆盖的全集）

`from_stream`（形状统一，`abbrev`/`offset` 为可选出参）：

```cpp
template <class _CharT, class _Traits, class _Duration, class _Alloc = allocator<_CharT>>
basic_istream<_CharT, _Traits>&
from_stream(basic_istream<_CharT, _Traits>& is, const _CharT* fmt,
            TARGET& tp,
            basic_string<_CharT, _Traits, _Alloc>* abbrev = nullptr,
            minutes* offset = nullptr);
```

`TARGET` 需覆盖：`duration`、`sys_time`、`utc_time`、`tai_time`、`gps_time`、
`file_time`、`local_time`、`day`、`month`、`year`、`weekday`、`month_day`、
`year_month`、`year_month_day`。（时区相关的 utc/tai/gps 及 `%Z/%z` 出参 gate 在
`_LIBCPP_HAS_EXPERIMENTAL_TZDB` / `_LIBCPP_HAS_TIME_ZONE_DATABASE` 后。）

`parse`（[time.parse]，共 8 个重载 —— fmt 有 `const _CharT*` 与 `basic_string`
两种，叠加 abbrev/offset 组合）：

```cpp
parse(fmt, tp)                    // ×2 (fmt 两种)
parse(fmt, tp, abbrev)           // ×2
parse(fmt, tp, offset)           // ×2
parse(fmt, tp, abbrev, offset)   // ×2
```

**[time.parse] 硬约束**：
1. /1 每个 `parse` **以非限定名调 `from_stream`**，触发 ADL。
2. /2 Recommended practice：`parse` 返回的操纵器**不可移动**、**禁止提取进
   lvalue**，防 `fmt` 悬垂。
3. `Parsable<T>` 概念 = `from_stream(declval<basic_istream<charT,traits>&>(), F, tp[, addressof(abbrev)][, &offset])`
   在未求值语境下 well-formed。

---

## 1. 架构总览

三层，从下到上：

```
┌─ __chrono/parser_data.h ─────────────────────────────┐
│  __fields_storage<_Duration>  —— 解析中间态           │
│  (可选的 year/month/day/weekday/hh/mm/ss/subsec/tz…)  │
└──────────────────────────────────────────────────────┘
┌─ __chrono/from_stream.h ─────────────────────────────┐
│  __parse_from_stream()  —— %spec 主循环 (读, 借 time_get)│
│  __to_<target>()        —— 字段 → 目标类型 + 一致性校验 │
│  from_stream(...) 各类型重载 (namespace chrono, 公开)  │
└──────────────────────────────────────────────────────┘
┌─ __chrono/parse.h ───────────────────────────────────┐
│  __from_stream_adl (void from_stream()=delete + concept)│
│  __parse_manip<...> (immovable, friend operator>>)     │
│  parse(...) 8 重载 (namespace chrono, 公开)            │
└──────────────────────────────────────────────────────┘
```

**复用**：`%…` 主循环的结构照 `__chrono/formatter.h` 的
`__format_chrono_using_chrono_specs`（只是读不是写）；月名/星期名/AM-PM 用
`time_get<_CharT>` facet；`statically_widen` 复用。
**新写**：`__fields_storage` 与 finalize（即 `convert_to_tm.h` 的逆向）、
`%z/%Z/%y世纪/小数秒` 的读取。

---

## 2. 涉及改动的文件（全量清单）

新增头文件：

| 文件 | 内容 |
|---|---|
| `libcxx/include/__chrono/parser_data.h` | `__fields_storage` 中间结构 + 字段哨兵 |
| `libcxx/include/__chrono/from_stream.h` | 解析引擎 + finalize + 所有 `from_stream` 重载 |
| `libcxx/include/__chrono/parse.h` | `parse` 操纵器 + `_Parsable` 概念 + ADL 阻断 |

需要登记/挂接的现有文件：

| 文件 | 改动 |
|---|---|
| `libcxx/include/CMakeLists.txt` | 在 `__chrono/` 段（第 257-295 行附近）加 3 个新头 |
| `libcxx/include/module.modulemap.in` | `module chrono { … }`（第 960+ 行）加 3 个子模块 |
| `libcxx/include/chrono` | 在计算/格式化 include 段（第 1103-1136）加 `#include` |
| `libcxx/include/version` | `__cpp_lib_chrono` 由 `201611L` 升到 `201907L`（第 346 行；只在整套 P0355 完成后一次性升） |
| `libcxx/docs/Status/Cxx20Papers.csv` | P0355R7 行状态更新（第 21 行） |
| `libcxx/docs/ReleaseNotes/*.rst` | 加一条 feature note |

测试相关（见 §4）：新增 `libcxx/test/std/time/time.parse/`，各类型的
`from_stream` 测试放到对应 `[time.*]` 镜像目录；更新
`chrono.version.compile.pass.cpp`、transitive-includes 期望。

---

## 3. 分阶段实现计划

每个阶段可独立编译、独立测试、独立提 PR。**先纵向打通一条最薄的路径，再横向铺类型**。

---

### Phase 0 — 脚手架（不含逻辑）

**目标**：新头文件存在、能被 `<chrono>` 包含、模块化构建通过，`from_stream` 对
`sys_seconds` 有一个**空壳声明**（返回 `is` 不做事）。先把构建/模块/传递包含跑通。

**改动**：新建 3 个空头 + CMakeLists + modulemap + `<chrono>` include + guard
（`_LIBCPP_STD_VER >= 20`、`_LIBCPP_HAS_LOCALIZATION`，参照 formatter.h 第 15/70 行）。

**测试**（命令细节见 §4）：
- 一个 `.compile.pass.cpp`：`#include <chrono>` + 取 `from_stream` 地址能编译。
- 传递包含期望：改了 `<chrono>` 的 include，`transitive_includes.gen.py` 会变，
  按 §4 重生成 `transitive_includes/*.csv` 期望。
- **单点模块验证**：`--param enable_modules=clang` 只跑 chrono 子集（见 §4），
  验证新加的 modulemap 条目 / `export` 正确。

**验收**：chrono 子集在普通模式和模块模式下都不回归。

---

### Phase 1 — 引擎骨架 + `sys_time` 的数字型说明符

**目标**：跑通 `%Y %m %d %H %M %S` 及组合 `%F %T`（**不含 locale、不含 tz、不含
小数秒**），`from_stream(is, "%F %T", sys_seconds&)` 可用。

**改动**：`parser_data.h` 的 `__fields_storage`；`from_stream.h` 的
- `__parse_from_stream()` 主循环（`%` 分派 + 字面匹配 + 空白吞并）；
  子读取器 `__read_int`（跳前导空白、按最多 N 位读十进制，失败置 failbit）；
- `__to_sys_time()` finalize：缺 y/m/d 或 `!ok()` → failbit，且**失败时不写 tp**；
- `from_stream(...)` for `sys_time`（sentry + noskipws，见骨架 §5）。

**标准依据**：[time.clock.system.nonmembers] 的 `from_stream`；[time.format] 的
说明符表（`%Y %m %d %H %M %S %F %T`）。

**测试**（放 `test/std/time/time.clock/time.clock.system/from_stream.pass.cpp`）：
- 正例：`"2026-07-20 13:45:30"` → 期望 sys_time。
- 失败：缺字段、非法日期（`2026-02-30`）、字面不匹配（`"%F"` 配 `"2026/07/20"`）
  → `is.fail()` 为真且 `tp` **未被改写**。
- `eofbit`：完整解析后正好到流尾，`fail()` 应为 false。

---

### Phase 2 — `parse` 操纵器（先只接 sys_time）

**目标**：`is >> chrono::parse("%F %T", tp)` 可用；不可移动 + ADL 定制点就位。

**改动**：`parse.h`
- `namespace __from_stream_adl { void from_stream() = delete; concept __can_from_stream = …; }`；
- `__parse_manip`：删除拷贝/移动，`friend operator>>` 只接 `const&`；
- 先实现 `parse(fmt, tp)` 两个 fmt 重载，`_Parsable` 概念约束。

**标准依据**：[time.parse]/1、/2、/3-5。

**测试**（`test/std/time/time.parse/parse.pass.cpp` + `.verify.cpp`）：
- 正例：`is >> parse("%F", ymd)` 行为等同直接 `from_stream`。
- `.verify.cpp`：`auto m = parse("%F", tp);`（存左值）应编译失败 —— 验证防悬垂。
- `Parsable` 概念：对不支持的类型 `parse(...)` SFINAE 掉（`.verify.cpp`）。

---

### Phase 3 — 日历类型 from_stream

**目标**：`day`、`month`、`year`、`weekday`、`month_day`、`year_month`、
`year_month_day` 的 `from_stream`（各自 finalize + 字段子集校验）。

**改动**：`from_stream.h` 增各类型重载 + `__to_day/__to_month/...`；主循环已够用。

**标准依据**：各 `[time.cal.*]` 的 `*.nonmembers` 子条款。

**测试**：镜像到 `test/std/time/time.cal/time.cal.day/`、`.../time.cal.year/` 等，
各 `from_stream.pass.cpp`。正例 + 缺字段失败 + `!ok()` 失败。

---

### Phase 4 — locale 相关说明符

**目标**：`%a %A`（星期名）、`%b %B %h`（月名）、`%p`（AM/PM）、`%c %x %X %r`
（locale 组合）。

**改动**：主循环里这些 case 改调 `time_get<_CharT>` 的 `get_weekday` /
`get_monthname` / `get`（`%p` 可用 `get` 走 `%p`，或自建 AM/PM 表）；
`%I`+`%p` 在 finalize 里 `__resolve_hour()` 合并成 24 小时。

**标准依据**：[time.format] 说明符表；[time.parse] 对 locale 的处理。

**测试**：`from_stream.locale.pass.cpp`，用 `std::locale` 注入自定义
`time_get`（参照 `test/std/time/.../formatter.*` 里既有的 locale helper），
覆盖 `"C"` locale + 至少一个非 C locale。

---

### Phase 5 — 时区与出参：`%z %Z` + `abbrev`/`offset`

**目标**：`%z`（`±hhmm` / `±hh:mm` / `Z`）读进 `offset`；`%Z`（缩写）读进
`abbrev`；`parse` 的 abbrev/offset 重载。

**改动**：
- 主循环 `%z` → `__read_offset`（手写，镜像输出侧 `__format_zone_offset`），
  `%Z` → `__read_abbrev`；
- `from_stream` 把 `__fields.__offset/__abbrev` 回填到出参；
- `parse` 补 `parse(fmt,tp,abbrev)` / `(fmt,tp,offset)` / `(fmt,tp,abbrev,offset)`
  共 6 个重载；
- sys_time finalize：有 `%z` 时 `tp -= offset`（回 UTC）；`local_time` **不减**。

**标准依据**：[time.parse]/6-8 等；[time.format] 的 `%z %Z`。

**测试**：`from_stream.tz.pass.cpp`：`"+0930"`、`"+09:30"`、`"Z"`、缺失时出参不动；
`parse` 的 abbrev/offset 重载 `.pass.cpp` + 概念 `.verify.cpp`。

---

### Phase 6 — 其余 clock 类型 + 小数秒 + 边角

**目标**：`utc_time`（闰秒 60 秒）、`tai_time`、`gps_time`、`file_time`、
`local_time` 的 `from_stream`；`%S` 小数秒（按目标 `Duration` 精度）；`%y` 两位
年世纪推断；`%j` day-of-year。

**改动**：
- clock 各 finalize，镜像 `convert_to_tm.h` 里 utc/tai/gps 的 offset/闰秒逻辑的逆；
  gate 在 TZDB 宏后；
- `%S` 读整秒后若遇小数点，按 `hh_mm_ss<Duration>::fractional_width` 读小数、
  正确舍入或置 failbit；
- `duration` 的 from_stream（[time.duration] io）。

**测试**：各 clock 目录 `from_stream.pass.cpp`；小数秒精度矩阵（`seconds` /
`milliseconds` / `microseconds` / floating rep）；闰秒 `"…:60"` 只对 utc 合法。

---

### Phase 7 — 收尾：feature macro + 状态 + release notes

**目标**：整套完成后翻开总开关。

**改动**：
- `include/version`：`__cpp_lib_chrono` `201611L` → `201907L`（**仅当输入侧全绿**）；
- `docs/Status/Cxx20Papers.csv` 第 21 行 P0355R7 → `|Complete|`（若其余部分也齐）；
- `docs/ReleaseNotes` 增条目；
- 更新 issue #99982 勾选项。

**测试**：
- `test/std/language.support/support.limits/support.limits.general/chrono.version.compile.pass.cpp`
  断言宏值 = `201907L`；
- `test/std/language.support/support.limits/support.limits.general/version.version.compile.pass.cpp` 同步；
- 全量 `check-cxx`。

---

## 4. 测试策略与约定

### 4.1 测试放哪、怎么命名

- **目录镜像标准子条款**：`from_stream` 测试进对应类型目录
  （sys_time → `time.clock/time.clock.system/`，日历 → `time.cal/...`）；
  `parse` 进新目录 `test/std/time/time.parse/`。
- **文件后缀**：运行期 `*.pass.cpp`；编译失败/概念 `*.verify.cpp`；纯编译
  `*.compile.pass.cpp`。
- **Lit 头注释**必带：
  ```cpp
  // UNSUPPORTED: c++03, c++11, c++14, c++17
  // UNSUPPORTED: no-localization
  // 涉及 tz 的加: // UNSUPPORTED: no-filesystem, libcpp-has-no-experimental-tzdb, no-tzdb
  ```
- **对拍**：把 libstdc++/MSVC 的 `from_stream`/`parse` 测试搬来做正例来源，
  逐条验证行为一致（尤其失败语义、eofbit）。
- **失败语义单测**要独立成组：缺字段、越界、字面不匹配、精度不足、
  出参在失败时是否保持不变。

### 4.2 单点运行 lit（开发主循环）

所有 libc++ 测试都是 lit 测试，用 build 里的 `llvm-lit` 驱动。**不要**直接
`python xxx.gen.py`（那只会把生成的测试源码打到屏幕，不会真正测）。

```bash
LIT=<build>/bin/llvm-lit          # 或 libcxx/utils/libcxx-lit

# 只跑你正在写的单个文件
$LIT -sv libcxx/test/std/time/time.clock/time.clock.system/from_stream.pass.cpp

# 只跑 chrono 时间子集
$LIT -sv libcxx/test/std/time/

# 只跑 parse 目录
$LIT -sv libcxx/test/std/time/time.parse/
```

`-sv` = succinct+verbose，逐条显示子测试（`.gen.py` 展开出来的 `//---` 分片也会
各自成为一个子测试名）。

### 4.3 单点跑「模块模式」（验证 modulemap / export）

`module.modulemap.in` 的正确性靠**用 Clang 模块编译测试**来验证，开关是 lit 参数
`--param enable_modules=clang`（内部加 `-fmodules -fcxx-modules`）：

```bash
# 只用模块模式跑 chrono 子集 —— 验证你新加的 module 条目和 export 够不够
$LIT -sv --param enable_modules=clang libcxx/test/std/time/
```

漏 module 条目或 `export` 时，这一步会在编译期报「头不属于任何模块 / 缺 export」。
比全量 `run-buildbot generic-modules` 快得多，是开发时的首选。

> **重要**：改了 `module.modulemap.in` 或 `CMakeLists.txt` 后要**重跑 CMake**
> （或让 ninja 触发 reconfigure），因为 `module.modulemap` 是 `configure_file`
> 从 `.in` 生成的（`include/CMakeLists.txt:1700`）；不重配置，lit 用的还是旧地图。

### 4.4 三个生成式测试（`.gen.py`）—— 你会碰到的

| 脚本 | 何时触发 | 怎么处理 |
|---|---|---|
| `test/libcxx/transitive_includes.gen.py` | 你给 `<chrono>` 加了 `#include` | 期望存于 `test/libcxx/transitive_includes/cxx*.csv`；跑该测试看 diff，按报错更新对应 csv（或用 libc++ 的重生成流程），diff 一并提交 |
| `test/libcxx/module_std.gen.py` | 你新增了 `chrono::from_stream` / `chrono::parse` 等**公开名字** | `import std;` 的导出要能匹配；跑它，失败按提示补 `std.cppm` 的导出 |
| `test/libcxx/module_std_compat.gen.py` | 同上（`std.compat`） | 同上 |

跑法同 4.2：`$LIT -sv test/libcxx/transitive_includes.gen.py`。

### 4.5 收尾的全量验证

各阶段用 4.2/4.3 单点迭代；合 PR 前再跑一次较全的：

```bash
$LIT -sv libcxx/test/std/time/ libcxx/test/libcxx/transitive_includes.gen.py \
        libcxx/test/libcxx/module_std.gen.py
# 模块回归：
libcxx/utils/ci/run-buildbot generic-modules   # 权威但慢，PR 前或 CI 跑
```

---

## 5. 关键实现骨架（参考，非最终）

`from_stream`（sentry + 引擎 + finalize + 状态）：

```cpp
template <class _CharT, class _Traits, class _Duration, class _Alloc = allocator<_CharT>>
_LIBCPP_HIDE_FROM_ABI basic_istream<_CharT, _Traits>&
from_stream(basic_istream<_CharT, _Traits>& __is, const _CharT* __fmt,
            sys_time<_Duration>& __tp,
            basic_string<_CharT, _Traits, _Alloc>* __abbrev = nullptr,
            minutes* __offset = nullptr) {
  typename basic_istream<_CharT, _Traits>::sentry __s(__is, /*noskipws=*/true);
  if (__s) {
    __fields_storage<_Duration> __f;
    __chrono::__parse_from_stream(__is, __fmt, __f);   // 引擎，失败即置 failbit
    if (!__is.fail()) {
      sys_time<_Duration> __out;
      if (__chrono::__to_sys_time(__f, __out)) {       // finalize + 一致性校验
        __tp = __out;                                  // 仅成功才写 tp
        if (__abbrev && __f.__has_abbrev()) *__abbrev = __f.__abbrev_;
        if (__offset && __f.__has_offset()) *__offset = __f.__offset_;
      } else
        __is.setstate(ios_base::failbit);
    }
  }
  return __is;
}
```

`parse` 操纵器 + ADL 定制点（照 libstdc++/MSVC 结构）：

```cpp
namespace __from_stream_adl {
  void from_stream() = delete;                          // 阻断非限定名查找
  template <class _CharT, class _Tp, class... _Rest>
  concept __can_from_stream = requires(basic_istream<_CharT>& __is,
                                       const _CharT* __f, _Tp& __t, _Rest... __r) {
    from_stream(__is, __f, __t, __r...);                // intentional ADL
  };
}

template <class _CharT, class _Tp>
struct __parse_manip {
  const _CharT* __fmt_;
  _Tp*          __tp_;
  __parse_manip(const __parse_manip&)            = delete;  // immovable
  __parse_manip& operator=(const __parse_manip&) = delete;

  template <class _Traits>
  friend basic_istream<_CharT, _Traits>&
  operator>>(basic_istream<_CharT, _Traits>& __is, const __parse_manip& __m) {  // 只接 const&
    using namespace __from_stream_adl;
    from_stream(__is, __m.__fmt_, *__m.__tp_);
    return __is;
  }
};

template <class _CharT, __from_stream_adl::__can_from_stream<_CharT, char_traits<_CharT>> _Parsable>
_LIBCPP_HIDE_FROM_ABI auto parse(const _CharT* __fmt, _Parsable& __tp) {
  return __parse_manip<_CharT, _Parsable>{__fmt, std::addressof(__tp)};
}
// … abbrev / offset / basic_string-fmt 组合共 8 个重载
```

---

## 6. 失败/边界清单（测试重灾区，逐条要有用例）

1. sentry 用 `noskipws=true`，自己控制空白；`%n`/`%t`/格式内空白 → 吞流里 0+ 空白。
2. finalize 必需字段不齐 → `failbit`，**目标对象保持不变**。
3. `%S` 小数秒：按目标 `Duration` 精度读；精度不足按 [time.format] 语义处理。
4. `%I`+`%p` 合并 24 小时；只有 `%I` 无 `%p` 的行为要定义清楚。
5. `%y` 两位年世纪推断（照标准/POSIX 规则）。
6. `%z`：`±hhmm` / `±hh:mm` / `Z` 都认；有 `%z` 时 sys_time 减偏移回 UTC，
   local_time 不减。
7. `utc_time` 闰秒：`%S` 读到 `60` 要能表示，finalize 走 `utc_clock::from_sys` 逆。
8. `eofbit` vs `failbit`：到流尾但完整解析 → 不置 failbit；到尾且不完整 → failbit。
9. `parse` 操纵器悬垂防护：`basic_string` 版 fmt 的临时串场景必须被 immovable +
   禁左值提取挡住（`.verify.cpp` 覆盖）。

---

## 7. 参考

- 提案：P0355R7 <https://wg21.link/P0355R7>
- 标准：[time.parse]、各 `[time.clock.*.nonmembers]`、`[time.cal.*]`、[time.format]
- libc++ tracking：llvm/llvm-project#99982
- 对拍实现：libstdc++ `libstdc++-v3/include/bits/chrono_io.h`（`_Parser`）、
  MSVC STL `stl/inc/chrono`（`from_stream` + `_Parser`）
- 输出侧对称参考：`__chrono/formatter.h`（`__format_chrono_using_chrono_specs`）、
  `__chrono/convert_to_tm.h`（finalize 的逆向蓝本）
