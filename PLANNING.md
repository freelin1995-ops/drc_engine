# MPI 模式剩余问题清单

## 🔴 高优先级

### H1. Worker 表达式求值失败时静默吞掉结果
- **文件**: `src/mpi/mpi_worker.cc:28-35`
- **描述**: Worker 执行 `sol::safe_script` 失败时（如变量未散播），发回空消息。master 在 `mpi_evaluate_expr` 中看到 `size == 0` 直接跳过，将该 tile 视为无数据。**被跳过 tile 的所有 DRC 结果静默丢失**，无任何错误提示。
- **影响**: DRC 结果不完整，用户无法感知
- **修复方向**: Worker 发送错误标志 + 错误信息；master 检查并中止/警告

### H2. EdgePairs 合并产生重复（halo 重叠区双倍计数）
- **文件**: `src/mpi/mpi_binding.cc:216-220`（`mpi_evaluate_expr` 合并），`src/drc/engine.cc:67-68`（EdgePairs `operator|` 用 `+` 拼接）
- **描述**: `mpi_evaluate_expr` 收集各 tile 结果后用 `operator|` 合并。对于 EdgePairs，`operator|` 调用 `FlatEdgePairs::add` 做**简单拼接**，tile 间 halo 重叠区的 edge pairs 会出现两次。影响所有 DRC 检查：`width`、`space`、`notch`、`enclosing_check`、`sep_check`、`overlap_check`。
- **影响**: DRC 违规计数在 MPI 模式下偏高
- **修复方向**: 合并后去重，或改用 set-based 合并

### H3. MPI 通信无错误检查
- **文件**: `src/mpi/mpi_protocol.cc:11-81`
- **描述**: `MPI_Send`、`MPI_Recv`、`MPI_Bcast`、`MPI_Gather`、`MPI_Gatherv` 的返回值均未检查。worker 崩溃时 `MPI_Recv` 永久阻塞，master 挂起。错误数据通过 `MPI_Gatherv` 也可能被静默接受。
- **影响**: 故障时行为不可预测（挂起或静默错误）
- **修复方向**: 添加返回值检查 + 超时 + 异常传播

---

## 🟡 中优先级

### M1. ScriptAnalyzer 多语句行处理错误
- **文件**: `src/mpi/script_analyzer.cc:56-66`（`split_lines`），`src/mpi/script_analyzer.cc:84-91`（`extract_assigned_var`）
- **描述**: `split_lines` 只按换行符分割。一行中出现多个语句（如 `local a = f(); local b = g()`）被当做一个整体处理。`extract_assigned_var` 的正则只匹配**第一个**等号左侧的变量名。第二个变量赋值在 normalized 脚本中被静默丢弃，worker 上该变量为 nil。
- **影响**: 多语句行的后续变量在 MPI 模式下静默为 nil
- **修复方向**: `split_lines` 增加分号检测，一条 Lua 行按分号拆分为多条逻辑行

### M2. MPI 模式下测试覆盖缺口
- **文件**: `testdata/mpi_scenarios.lua`
- **描述**: 以下操作在 MPI 模式下没有 tile 跨越场景的测试：
  - `notch`（凹槽检测）
  - `space`（间距检测）
  - `sep_check`（分离检测）
  - `overlap_check`（交叠检测）
  - `enclosing_check`（包围检测）
  - `edges:inside(region)`、`edges:outside(region)`
- **影响**: 上述操作在 MPI 下的正确性无保障
- **修复方向**: 逐项添加 tile 跨越测试

### M3. HaloInferrer 只取第一个数值参数
- **文件**: `src/mpi/halo_inferrer.cc:34,39`
- **描述**: 正则 `std::regex_search(args, num_match, num_re)` 只匹配参数列表中**第一个**数字。对于多参数操作如 `extended(b, e, o, i, join)`，若 `o` > `b`，则 halo 偏小。`sized` 有特殊 `abs` 处理，但第一个参数不一定最大。
- **影响**: 多距离参数操作的 halo 可能不足，tile 边界结果可能出错
- **修复方向**: 提取所有数值参数，取最大值

### M4. 未知序列化类型标签静默返回空
- **文件**: `src/mpi/mpi_serialize.cc:83-135`（序列化），`src/mpi/mpi_serialize.cc:202-243`（反序列化）
- **描述**: `serialize_drclayer` 写入 `uint8_t` 类型标签，`deserialize_drclayer` 未校验标签是否在枚举范围内。未知/损坏标签走 `default` 分支返回空 `DRCLayer()`。无日志、无错误提示。
- **影响**: MPI 通信数据损坏时静默丢失
- **修复方向**: 校验标签范围，未知标签报错/中止

### M5. `clip_to_tile` 未处理的类型组合静默返回空
- **文件**: `src/mpi/mpi_binding.cc:240-266`
- **描述**: `clip_to_tile` 分别处理 Region/Edges/EdgePairs/Texts。其中 Edges/EdgePairs 的 tile 裁剪调用 `global.interacting(DRCLayer(new db::Region(tile_region)))`。若 `interacting` 遇到未实现的类型组合（如 EdgePairs 与 Region），`engine.cc:141-151` 返回空。结果是该 tile 的数据静默丢失。
- **影响**: 非标准类型组合在 tile 裁剪时静默返回空
- **修复方向**: 在 `clip_to_tile` 中显式处理 Edges/EdgePairs 的 tile 交集，或证明现有路径覆盖所有情况

---

## 🟢 低优先级

| ID | 问题 | 文件 | 说明 |
|----|------|------|------|
| L1 | int32 溢出（>2GB GDS） | `mpi_serialize.cc:41` | GDS 流大小用 `int32_t`，超过 2GB 截断。极低概率 |
| L2 | 空 Region 序列化格式不一致 | `mpi_serialize.cc:89-98` | null region 写 4 字节零，空 region 写真实 GDS 流。无功能影响 |
| L3 | ScriptAnalyzer 前缀匹配脆弱 | `script_analyzer.cc:111-125` | `source(path)` vs `source (path)` 中间空格数量必须完全匹配 |
| L4 | ScriptAnalyzer 未过滤完整 Lua 关键字 | `script_analyzer.cc:93-109` | `for`/`while`/`if` 等误识别为变量引用，可能引起不必要的散播 |
| L5 | `output_rdb` 未暴露 | `lua_binding.cc` / `mpi_binding.cc` | 两种模式下均不可用，非 MPI 专有问题 |
| L6 | Worker 无超时/心跳 | `mpi_worker.cc:20-66` | master 崩溃后 worker 永久阻塞 |
| L7 | 多个 `source()` 调用 | `mpi_master.cc:93-114` | 二次 `source` 毁掉之前布局（与非 MPI 一致） |
| L8 | `__expr` 对 master-local 操作多余 | `mpi_binding.cc:169-180` | `with_area`/`with_perimeter`/`length` 等不调 `mpi_evaluate_expr`，`__expr` 设置 `g_current_expr` 是空操作 |

---

## 修复优先级建议

1. **H1** — Worker 失败静默吞结果（最严重，静默数据丢失）
2. **H2** — EdgePairs 合并重复（所有 DRC 检查计数不准确）
3. **H3** — MPI 通信无错误检查（故障时行为不可预测）
4. **M1** — ScriptAnalyzer 多语句行（复杂脚本静默出错）
5. **M2** — 测试覆盖缺口（验证其他修复的正确性）
6. **M3** — HaloInferrer 只取第一个参数
7. **M4** — 未知序列化类型标签
8. **M5** — `clip_to_tile` 未处理类型组合
