# MPI Distributed DRC Engine — 设计文档

## 1. 概述

在单机 `drc-engine` 基础上增加 OpenMPI 分布式支持。核心思路：**规范化脚本 + `_G.__newindex` 自动 scatter + 操作重写透明编排 worker**。

## 2. 系统架构

### 角色

| 角色 | 职责 |
|------|------|
| Master (Rank 0) | 跑完整 DRCEngine + MPI-aware Lua 绑定；`lua.script(normalized)` 一行执行全部逻辑 |
| Worker (Rank 1..N) | Lua state + 正常 DRCLayer 绑定；被 MPI 消息驱动，不感知编排逻辑 |

### 核心设计

```
Master 代码（main 函数）:
  ┌─────────────────────────────────────┐
  │ lua.script(normalized_script);      │  ← 唯一的执行入口
  └─────────────────────────────────────┘
                     │
          ┌──────────┼──────────────┐
          ▼          ▼              ▼
   __newindex    __band MPI     width MPI
   (scatter)    (broadcast      (broadcast
                  → gather        → gather
                  → merge)        → merge)
          │          │              │
          ▼          ▼              ▼
       Worker 0..N: Lua + 正常 DRCLayer 绑定
                     tile-local 变量
```

**关键：** master 的 main 函数只有一行执行入口。所有 MPI 逻辑隐藏在：
1. `__newindex` — 全局变量赋值触发 scatter
2. 操作符/方法重写 — `|`、`:width()` 等内部做 MPI 编排

## 3. 执行模型

### 规范化脚本

```
原始:  local w = input(10, 0):sized(0.1):width(0.5)

规范化（两步）:
  Step 1 拆链:  local __t1 = input(10, 0)
                local __t2 = __t1:sized(0.1)
                local w = __t2:width(0.5)

  Step 2 去 local:  __t1 = input(10, 0)
                    __t2 = __t1:sized(0.1)
                    w = __t2:width(0.5)

最终执行:  lua.script([[
    source("input.gds")
    target("output.gds")
    __t1 = input(10, 0)
    __t2 = __t1:sized(0.1)
    w = __t2:width(0.5)
    w:output(1, 0)
    write()
  ]])
```

### `_G.__newindex` — 全局变量赋值拦截

```cpp
// MPI binding setup:
sol::table mt = lua.create_table();
mt[sol::meta_function::new_index] = [&](sol::object key, sol::object value) {
    std::string var = key.as<std::string>();
    
    // 先执行正常赋值（绕过 metatable）
    lua.globals().raw_set(key, value);
    
    // 如果变量有下游引用且是 DRCLayer → scatter
    if (value.is<DRCLayer>() && ref_table.has_downstream_refs(var)) {
        DRCLayer& layer = value.as<DRCLayer>();
        clip_and_scatter(layer, var);
    }
};
lua.globals().set(sol::metatable_key, mt);
```

### 完整执行流程

```
--- source("input.gds") ---
  Master: overridden source()
    ├─ load_layout → 全局 Layout
    └─ 计算 tile 网格（bbox + dbu → nx × ny tiles）

--- target("output.gds") ---
  Master: normal → set_target_path
  （无题赋值，__newindex 不触发）

--- __t1 = input(10, 0) ---
  ① 求值 RHS: overridden input() → 读全局 Layout layer 10 → 全局 Region
  ② 赋值: __newindex("__t1", Region)
      ├─ raw_set("__t1", Region) → master Lua state 持有
      └─ ref_table: __t1 有下游引用 → clip Region → tile_i(halo)
         ├─ UPDATE_VAR(__t1, tile_0) → Worker0: __t1 = tile-local
         └─ UPDATE_VAR(__t1, tile_1) → Worker1: __t1 = tile-local

--- __t2 = __t1:sized(0.1) ---
  ① overridden sized():
      ├─ MPI_Bcast(EXECUTE_RHS, "__t1:sized(0.1)")
      │   Worker0: lua("return __t1:sized(0.1)") → tile-__t2_0
      │   Worker1: lua("return __t1:sized(0.1)") → tile-__t2_1
      ├─ gather → merge → 全局 __t2 Region
      └─ return 给 Lua
  ② 赋值: __newindex("__t2", Region)
      ├─ raw_set → master Lua state
      └─ __t2 有下游引用 → clip → UPDATE_VAR

--- w = __t2:width(0.5) ---
  ① overridden width():
      ├─ MPI_Bcast(EXECUTE_RHS, "__t2:width(0.5)")
      ├─ gather → merge → 全局 w EdgePairs
      └─ return
  ② 赋值: __newindex("w", EdgePairs)
      ├─ raw_set → master Lua state
      └─ w 无下游引用 → 不 scatter（master only）

--- w:output(1, 0) ---
  Master: normal → m_target.layer(1,0) = w EdgePairs
  （无赋值，__newindex 不触发）

--- write() ---
  Master: normal → 写 GDS
```

### 操作重写

每个操作符/方法按此模式：

```cpp
// MPI binding 中的通用模式
DRCLayer mpi_operator(RHS_expr_string) {
    // 1. 广播表达式给所有 worker
    mpi_broadcast_expr(RHS_expr_string);

    // 2. Gather 所有 worker 的 tile-local 结果
    auto tile_results = mpi_gather_results(num_workers);

    // 3. Merge 为全局结果
    return merge_all(tile_results);
}
```

| Lua 操作 | 绑定函数 | 重写行为 |
|---------|---------|---------|
| `input(l, d)` | MPI input | 读全局 Layout → 返回全局 Region（无 MPI） |
| `a \| b` | MPI __band | broadcast "a\|b" → gather → merge → return |
| `a & b` | MPI __band | broadcast "a&b" → gather → merge → return |
| `obj:width(d)` | MPI width | broadcast "obj:width(d)" → gather → merge → return |
| `obj:sized(d)` | MPI sized | broadcast "obj:sized(d)" → gather → merge → return |
| `obj:space(d)` | MPI space | broadcast "obj:space(d)" → gather → merge → return |
| `obj:output(l,d)` | normal | 原始，直接写 m_target |
| `write()` | normal | 原始 |
| `source(p)` | MPI source | load_layout + 计算 tile 网格 |
| `target(p)` | normal | 原始 |

### Worker 消息循环

Worker 使用**正常 DRCLayer 绑定**，只处理 3 种消息：

```cpp
while (true) {
    auto msg = recv();
    switch (msg.type) {
    case EXECUTE_RHS: {
        // 执行表达式，返回结果
        string expr = msg.data;
        auto result = lua.safe_script("return " + expr);
        send_result(serialize(drclayer_from_sol(result)));
        break;
    }
    case UPDATE_VAR: {
        // 直接更新变量的 tile-local 值
        string var = msg.var_name;
        DRCLayer tile_data = deserialize(msg.data);
        lua.globals()[var] = tile_data;
        break;
    }
    case DONE:
        return;
    }
}
```

Worker 不需要 `TILE_ASSIGN`、`EXECUTE_LINE` 等消息——所有变量通过 `UPDATE_VAR` 以已计算好的 tile-local 值下发。Worker 只在 `EXECUTE_RHS` 时执行简短 Lua 表达式，不运行完整语句。

### master 线程安全性

当前方案中，master 在单线程中执行 `lua.script()`。但操作重写内部有 MPI 通信（blocking send/recv），会阻塞 Lua 执行直到所有 worker 返回。这是预期行为：master 是同步编排的。

## 4. 脚本规范化与引用分析

### 规范化

两步变换：

```
链式调用 → SSA 形式 → 全局变量形式
```

### 变量引用表

扫描规范化脚本构建：

| 变量 | 定义行 | 被引用行 | 需要 scatter? |
|------|--------|---------|-------------|
| __t1 | 0 | 1 | ✓ |
| __t2 | 1 | 2 | ✓ |
| a | 0 | 2 | ✓ |
| b | 1 | 2 | ✓ |
| merged | 2 | 3 | ✓ |
| w | 3 | (none) | ✗ |

用于 `__newindex` 判断是否 scatter。

### Halo 推断

第一遍 analysis mode（rank 0）：
```
运行原始脚本，记录所有距离参数 → max_distance → halo
```
用户可通过 `--halo` 覆盖。

## 5. MPI 通信

### 消息类型

```cpp
enum class MPIMsgType : int {
    EXECUTE_RHS,    // Master→Worker: 执行 Lua 表达式并返回结果
    UPDATE_VAR,     // Master→Worker: 更新 tile-local 变量
    DONE,           // Master→Worker: 结束
};
```

只需要 **3 种消息类型**。

### 通信时序

```
Master                          Worker 0           Worker 1
  lua.script(...)                 │                   │
                                  │                   │
  --- source("input.gds") ---     │                   │
  加载 Layout，计算 tile 网格       │                   │
                                  │                   │
  --- __t1 = input(10, 0) ---    │                   │
  __newindex 触发:                 │                   │
  ├─ UPDATE_VAR(__t1, tile_0) ─────→┤                   │
  ├─ UPDATE_VAR(__t1, tile_1) ──────────→─────────────┤
  │                               │ __t1 = tile-local │ __t1 = tile-local
  │                               │                   │
  --- __t2 = __t1:sized(0.1) ---  │                   │
  │ MPI sized():                   │                   │
  ├─ EXECUTE_RHS("__t1:sized(0.1)") →┤→────────────────┤
  │                               │ return tile-t2_0  │ return tile-t2_1
  │←────────── result ────────────┤                   │
  │←────────── result ───────────────────────←────────┤
  │ merge → 全局 __t2 Region       │                   │
  │ __newindex:                    │                   │
  ├─ UPDATE_VAR(__t2, tile_0) ─────→┤                   │
  ├─ UPDATE_VAR(__t2, tile_1) ──────────→─────────────┤
  │                               │ __t2 = tile-local │ __t2 = tile-local
  │                               │                   │
  --- w = __t2:width(0.5) ---     │                   │
  ├─ EXECUTE_RHS("__t2:width(0.5)") →┤→────────────────┤
  │                               │ return tile-w_0   │ return tile-w_1
  │←────────── result ────────────┤                   │
  │←────────── result ───────────────────────←────────┤
  │ merge → 全局 w EdgePairs       │                   │
  │ __newindex: w 无下游引用 → 不 scatter              │
  │                               │                   │
  --- w:output(1, 0) ---          │                   │
  master 本地 → m_target           │                   │
                                  │                   │
  --- write() ---                 │                   │
  master 本地 → 写 GDS             │                   │
                                  │                   │
  ├─ DONE ─────────────────────────→┤→────────────────┤
```

### 数据格式

```cpp
struct MPIUpdateVar {
    char var_name[64];    // 变量名
    int data_size;        // 序列化数据大小
    char data[];          // 序列化 DRCLayer
};

struct MPIRHSExec {
    int expr_size;        // 表达式字符串长度
    char expr[];          // Lua 表达式，如 "a | b"
};
```

## 6. 序列化

复用 KLayout GDS 读写能力：

```
序列化:   DRCLayer → 临时 Layout → GDSWriter → 字节数组
反序列化: 字节数组 → GDSReader → Layout → DRCLayer
```

## 7. 代码改造

### 新增模块

```
src/mpi/
├── CMakeLists.txt
├── mpi_master.h / .cc      # 初始化、tile 计算、`lua.script()` 执行入口
├── mpi_worker.h / .cc      # Worker 消息循环
├── mpi_serialize.h / .cc   # DRCLayer 序列化
├── mpi_protocol.h          # 消息类型、结构体
├── mpi_binding.h / .cc     # MPI 版 Lua 绑定（__newindex + 操作重写）
├── script_analyzer.h / .cc # 规范化 + 引用表
└── halo_inferrer.h / .cc   # Halo 推断
```

### mpi_binding.cc

```cpp
// ── MPI 上下文（全局可见）──
struct MPIWorkerContext {
    int num_workers;
    std::vector<Tile> tiles;
    double halo;
    RefTable* ref_table;
};
thread_local MPIWorkerContext* g_mpi_ctx = nullptr;

// ── 注册 __newindex ──
void install_newindex_hook(sol::state& lua) {
    sol::table mt = lua.create_table();
    mt[sol::meta_function::new_index] = [&lua](sol::object key, sol::object value) {
        if (!key.is<std::string>()) {
            lua.globals().raw_set(key, value);
            return;
        }
        std::string var = key.as<std::string>();
        lua.globals().raw_set(key, value);  // 绕过 metatable 直接赋值

        if (value.is<DRCLayer>() && g_mpi_ctx->ref_table->has_downstream_refs(var)) {
            DRCLayer& layer = value.as<DRCLayer>();
            clip_and_scatter(layer, var, *g_mpi_ctx);
        }
    };
    lua.globals().set(sol::metatable_key, mt);
}

// ── 通用 MPI 编排 ──
DRCLayer mpi_evaluate_expr(const std::string& expr) {
    mpi_broadcast_expr(expr);            // EXECUTE_RHS → 所有 worker
    auto results = mpi_gather_results(); // 收 worker 结果
    return merge_all(results);
}

// ── 操作绑定 ──
int mpi_region_band(lua_State* L) {
    // 参数已在栈上，但实际的 MPI 通信在调用此函数前就完成了
    // 因为 __band(lhs, rhs) 中 lhs 和 rhs 都是全局 Region，
    // 真正的工作发生在 master 侧: broadcast → gather → merge 已完成
    // 直接从 merge 缓存取结果

    // 简化实现：每个操作对应一个 expr，缓存结果
    DRCLayer result = g_current_op_result;
    push_region(L, result);
    return 1;
}

void bind_drc_engine_mpi(sol::state& lua, DRCEngine& engine, MPIWorkerContext* ctx) {
    g_mpi_ctx = ctx;
    install_newindex_hook(lua);

    // Region metatable 重写
    auto& rmt = lua["Region"].get<sol::metatable>();
    rmt[sol::meta_function::bitwise_and] = [](const Region& a, const Region& b) {
        return mpi_evaluate_expr(g_current_expr);
    };
    rmt[sol::meta_function::bitwise_or] = [](const Region& a, const Region& b) {
        return mpi_evaluate_expr(g_current_expr);
    };

    // 方法重写
    rmt["width"] = [](const Region& self, double d) {
        return mpi_evaluate_expr(g_current_expr);
    };
    rmt["sized"] = [](const Region& self, double d) {
        return mpi_evaluate_expr(g_current_expr);
    };
    // ... 其他操作类似

    // input() 重写：读全局 Layout
    lua["input"] = [&engine](int layer, int dtype) -> Region {
        return read_global_layer(engine.layout(), layer, dtype);
    };

    // source() 重写：load + 切 tile
    lua["source"] = [&](const std::string& path) {
        engine.load_layout(path);
        compute_tiles(engine, *g_mpi_ctx);
    };

    // output/write/target: 原始行为，不重写
}
```

### Master 执行（main）

```cpp
int run_master(int argc, char* argv[]) {
    // 阶段一：脚本分析
    ScriptAnalyzer analyzer(script_path);
    analyzer.normalize();
    analyzer.build_ref_table();

    double halo = analyzer.infer_halo_or_use_override();

    // 阶段二：初始化引擎 + MPI 绑定
    DRCEngine engine;
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::math,
                       sol::lib::string, sol::lib::table);

    MPIWorkerContext ctx;
    ctx.num_workers = num_workers;
    ctx.halo = halo;
    ctx.ref_table = &analyzer.ref_table();

    bind_drc_engine_mpi(lua, engine, &ctx);

    // 阶段三：一行执行
    lua.script(analyzer.normalized_script());

    // 通知所有 worker 结束
    broadcast_done(num_workers);
}
```

### Worker 主循环

```cpp
int run_worker(int rank) {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::math,
                       sol::lib::string, sol::lib::table);
    bind_drc_engine(lua, engine);  // 正常绑定

    while (true) {
        auto msg = recv_msg();
        switch (msg.type) {
        case EXECUTE_RHS: {
            auto expr = recv_expr(msg);
            auto result = lua.safe_script("return " + expr);
            DRCLayer layer = sol_to_drclayer(result);
            send_result(serialize(layer));
            break;
        }
        case UPDATE_VAR: {
            auto var = recv_var_name(msg);
            auto data = recv_data(msg);
            lua.globals()[var] = deserialize(data);
            break;
        }
        case DONE:
            return 0;
        }
    }
}
```

### CLI 入口

```cpp
int main(int argc, char* argv[]) {
#ifdef DRC_USE_MPI
    MPI_Init(&argc, &argv);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == 0) return run_master(argc, argv);
    else           return run_worker(rank);
#else
    // 原单机模式
#endif
}
```

### 不变的部分

- `include/drc/engine.h` — 完全不变
- `src/drc/engine.cc` — 完全不变
- `src/drc/lua_binding.cc` — 完全不变（worker 直接使用；master 使用 mpi_binding.cc）

## 8. 构建与运行

### 构建

```bash
cmake -B build -DDRC_USE_MPI=ON
cmake --build build
```

### 运行

```bash
mpirun -np 8 ./build/src/cli/drc-check --mpi-tiles 4x4 script.drc
mpirun -np 8 ./build/src/cli/drc-check --mpi-tiles 4x4 --halo 5.0 script.drc
./build/src/cli/drc-check script.drc  # 单机模式
```

## 9. 错误处理与限制

| 场景 | 处理 |
|------|------|
| Worker 结果为空 | 正常，发送空 Region |
| Worker 崩溃 | MPI 自动检测，`MPI_Abort` |
| Halo 不足 | 用户 `--halo` 覆盖 |
| Lua 脚本错误 | master 侧 `lua.script()` 抛异常 → 退出 |
| 表达式错误 | worker 返回错误信息 → master abort |

限制：tile ≤ worker；统一 halo；序列化开销。
