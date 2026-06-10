# 和 AI Agent 结对编程的一夜：从绝望到交付

> 本文记录了一次真实的 AI Agent 协作开发经历，希望能帮助我的同事理解 Agent 的真正威力，并激发大家尝试的兴趣。

## 背景

我们的项目是一个基于 KLayout 库的 DRC（Design Rule Check）引擎，核心挑战是把原本单进程的 DRC 检查改造成 MPI 分布式版本——把版图切成多个 tile，分发给多个 worker 并行计算，最后合并结果。

听起来简单，但分布式系统的复杂性远超想象：序列化、通信协议、worker 调度、边界效应……每一个环节都可能藏着致命 bug。

那天晚上，整套 MPI 流程跑起来了，但测试大面积失败。我面对的是一个我并不完全熟悉的 C++ 代码库、一个我不了解的 KLayout 库 API、以及 MPI 并行调试这个老大难问题。

我打开了 opencode，开始了和 Agent 的结对编程。

---

## 第一幕：绝望的红色——16/20 测试失败

第一轮测试，MPI 1x1 模式（单 tile，最简单的分布式场景），20 个测试只有 4 个通过。2x2 模式更惨，几乎全军覆没。

Agent 没有慌。它做的第一件事不是猜测，而是**系统化排查**：

```bash
# 先跑 Non-MPI 模式基线
./build/src/cli/drc-check testdata/run_tests.lua
# Result: 20/20 PASS ✅

# 再跑 MPI 1x1
mpirun -np 2 ./build_mpi/src/cli/drc-check --mpi-tiles 1x1 --halo 0.5 ...
# Result: 4/20 PASS ❌
```

**基线全过、MPI 全崩**——问题锁定在 MPI 层。这是 Agent 的第一个关键能力：**不盲目开药，先做对照实验缩小问题范围。**

---

## 第二幕：剥洋葱——逐层定位序列化 Bug

Agent 发现第一个线索：`count()` 返回 0，但 `area()` 有值。这说明数据到了，但不完整。

它追踪了序列化链路：

```
Worker 计算 → GDS2 序列化 → MPI 传输 → GDS2 反序列化 → Master 合并
```

然后做了一个关键实验——写了一个序列化 roundtrip 测试：

```cpp
// 序列化再反序列化，看数据是否完整
auto serialized = serialize_drclayer(original);
auto restored = deserialize_drclayer(serialized.data(), serialized.size());
// Edges: original.count()=4, restored.count()=0 ❌
// EdgePairs: original.count()>0, restored.count()=0 ❌
// Region: original.count()=1, restored.count()=1 ✅
```

**根本原因找到了：** GDS2 格式不支持 Edges 和 EdgePairs 类型！KLayout 在写入 GDS2 时把边和边对转换成了多边形，读回来变成了 Region，不再是 Edges/EdgePairs。

Agent 没有犹豫，立刻实现了**自定义二进制序列化**：

```cpp
// Edge: 4 个 int32 坐标 (p1.x, p1.y, p2.x, p2.y)
// EdgePair: 2 个 Edge = 8 个 int32 坐标
static std::vector<char> serialize_edges(const db::Edges* edges) {
    // count + [x1,y1,x2,y2] per edge
}
static std::vector<char> serialize_edgepairs(const db::EdgePairs* eps) {
    // count + [x1,y1,x2,y2,...] per edgepair
}
```

这一修，测试从 4/20 直接跳到 17/20。

**关键洞察：** Agent 不是在"搜索答案"，而是在**设计和执行诊断实验**。它不会赌，它会验证。

---

## 第三幕：幽灵 Worker——找出隐藏的并发 Bug

17/20 通过了，但还有 3 个失败。而且是在 MPI 1x1 模式（只有 1 个 tile、1 个 worker）下失败，这很诡异。

Agent 看了看 MPI 通信日志，发现了一个匪夷所思的现象：

```
Worker 1 收到 EXECUTE_RHS，但变量是 nil！
```

一个只有 1 个 worker 的场景，变量怎么会是 nil？

Agent 追踪了 `mpi_evaluate_expr` 的发送逻辑：

```cpp
// 修复前：广播给所有 worker
mpi_broadcast(0, EXECUTE_RHS, expr.data(), expr.size());
for (int i = 0; i < num_workers; i++) {
    auto msg = mpi_recv(i + 1);
}
```

问题在于：**我们把表达式广播给了所有 worker（包括空闲的）**，但只给活跃 worker 发送了变量更新。空闲 worker 收到表达式后，用 nil 变量去求值，当然崩了。

修复方案简洁优雅：

```cpp
// 修复后：只发送给活跃 worker
int active = std::min(num_workers, (int)g_mpi_ctx->tiles.size());
for (int i = 0; i < active; i++) {
    mpi_send(i + 1, EXECUTE_RHS, expr.data(), expr.size());
}
for (int i = 0; i < active; i++) {
    auto msg = mpi_recv(i + 1);
}
```

**20/20 通过。** 这只用了不到 5 分钟。

---

## 第四幕：代码审查中发现隐藏 Bug

测试全过了，Agent 没有停下来。它主动做了一遍完整代码审查，逐文件检查每个 MPI 相关文件。

在审查 `engine.cc` 的 `interacting` 方法时，它发现了一个**测试没有覆盖到的 Bug**：

```cpp
// 修复前：EdgePairs + Region 的情况没有被处理
DRCLayer DRCLayer::interacting(const DRCLayer& other) const {
    if (m_type == Region && other.m_type == Region ...)  ✅
    if (m_type == Edges && other.m_type == Region ...)     ✅
    if (m_type == Edges && other.m_type == Edges ...)      ✅
    // EdgePairs + Region → 返回空 DRCLayer()！              ❌
    return DRCLayer();
}
```

这个 Bug 在当前测试中没有触发，但在 `clip_to_tile` 函数中使用了：

```cpp
} else if (global.type() == DRCLayer::EdgePairs) {
    return global.interacting(DRCLayer(new db::Region(tile_region)));
    // 这里 EdgePairs + Region 返回空！
}
```

如果未来有人在 2x2 模式下对 EdgePair 类型做 tile 裁剪，就会拿到空数据。

**这就是 Agent 的真正价值：它不只是修你看到的 Bug，它还能找到你没看到的。**

---

## 第五幕：15→20——2x2 模式的 6 个失败

2x2 模式下有 6 个测试失败。Agent 逐个分析后发现，它们有一个共同的根本原因——**tile 分解的边界效应**：

- **Width/Space check**：thin shape 被切到不同 tile，单个 tile 看不到违规
- **Corner detection**：正方形被切开后，每个 tile 看到的是矩形，角点特征变了
- **Perimeter doubled**：边界上的边被两个 tile 重复计算
- **enclosing_check**：边界产生了虚假违规

这不是代码 Bug，而是 tile-based 并行算法的固有局限。类似的问题在商业 DRC 工具中也存在，通常通过增大 halo 区域来缓解。

Agent 给出了明确结论：**这是架构特性，不是 Bug，可以接受。**

---

## 反思：Agent 到底改变了什么？

### 1. 诊断效率的质变

传统调试：`print → 猜测 → 改代码 → 再跑 → 再猜`。

Agent 的方式：**设计实验 → 收集证据 → 精确诊断 → 最小修复**。它不会在黑暗中乱摸，而是点亮一盏灯，看清再走。

### 2. 全覆盖审查

人类审查代码容易走神、遗漏。Agent 可以逐行检查每个文件，而且它**真的理解代码语义**，不是机械地匹配模式。这次它发现 interacting Bug 就是在审查中发现的，而不是测试暴露的。

### 3. 上下文记忆

一晚上我们改了十几个文件，涉及序列化、协议、绑定、引擎核心逻辑。Agent 记住了所有上下文——哪些文件改了、为什么改、改了什么。它不会忘记半小时前发现的线索。

### 4. 从"能不能用"到"能不能交付"

测试全通过的瞬间，Agent 没有停下。它做了完整的代码审查，发现隐藏 Bug，补全类型支持，验证所有模式，最后提交代码。**它追求的是交付质量，不是"能跑了就行"。**

---

## 给同事的建议

### 什么时候用 Agent？

| 场景 | 效果 |
|------|------|
| Bug 定位与修复 | ⭐⭐⭐⭐⭐ Agent 最擅长的 |
| 代码审查 | ⭐⭐⭐⭐⭐ 不走神的全覆盖审查 |
| 系统化测试 | ⭐⭐⭐⭐ 精确诊断，设计实验 |
| 架构设计 | ⭐⭐⭐ 可以讨论，但需要你来判断 |
| 探索性学习 | ⭐⭐⭐⭐ 帮你快速理解陌生代码库 |

### 怎么用效果最好？

1. **给 Agent 足够的上下文**——告诉它项目背景、代码结构、运行方式
2. **让它跑实验，不要让它猜**——"帮我写个测试验证这个猜想"比"帮我猜猜 Bug 在哪"有效得多
3. **信任它的审查结果**——它找出的 Bug 往往是你不会注意到的
4. **让它在修复后跑完整验证**——修一个 Bug 引入另一个 Bug 是最常见的悲剧

### 最重要的一点

**Agent 不是工具，是伙伴。** 今晚的经历告诉我：最好的工作方式不是"我写代码，Agent 辅助"，而是**我和 Agent 一起思考、一起实验、一起验证**。它补我看不到的盲区，我给它方向和判断。

20/20 测试通过的那一刻，我知道：这不是运气，这是方法论的胜利。

---

*本文基于真实开发经历整理。所有 Bug、修复和测试结果均为实际记录。*

*项目仓库：drc-engine — 一个支持 MPI 分布式计算的 DRC 引擎*