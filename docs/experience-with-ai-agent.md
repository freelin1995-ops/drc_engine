# 给同事：我和 Agent 做了一个 MPI 分布式 DRC 引擎，我想说说这个过程

上周我和 Agent（opencode）一起，把我们现有的 DRC 引擎加上了 MPI 分布式计算支持。整个过程蛮有意思的，我写下来分享给大家，也希望更多人愿意试试 Agent 辅助开发。

---

## 项目背景

drc-engine 是一个基于 KLayout 的 DRC 检查引擎，用 Lua 写检查脚本，C++ 做底层计算。单进程跑没问题，但大版图很慢。目标很简单：把版图切成 tile，分给多个 worker 并行算，最后合并结果。

听起来不复杂对吧？但现在回头看，整个过程中遇到的问题远比预想的多。

---

## 开始：从 ScriptAnalyzer 入手

Agent 的第一件事不是写代码，而是把整个项目翻了一遍——目录结构、README、Lua 绑定怎么写的、数据怎么流动的。

然后它写了 ScriptAnalyzer，干的事情是：把 Lua 脚本里的链式调用拆成单步赋值，同时建一个引用表记录哪些变量后续还会用到。比如：

```lua
-- 原始脚本
c = a:width(0.5):sized(-0.1)

-- 归一化后
__t1 = a:width(0.5)
c = __t1:sized(-0.1)
```

这样每一步的结果都可以被 MPI 系统拦截、分发。300 行 C++ + 61 行头文件 + 98 行测试，一次过。我 review 的时候提了些命名和边界检查的问题，第二轮全改了。

回头看，ScriptAnalyzer 是整个系统的基础。没有它做归一化和引用分析，后面的变量分发根本没法做。

---

## 最精巧的部分：劫持 Lua

接下来是 MPI-aware 的 Lua 绑定——这是我觉得整个系统设计最巧妙的地方。

问题是这样的：用户在 master 进程里写 `c = a & b`，但 `a` 和 `b` 的数据其实被切成了 tile 分布在各个 worker 上。怎么让这个表达式"自然而然"地变成分布式计算？

Agent 的方案是劫持 Lua 的两个机制：

**赋值拦截 (`__newindex`)**：每次给一个 DRCLayer 变量赋值，自动把它切片发给所有 worker。

**运算符拦截**：`a & b` 不在本地算，而是把表达式字符串发送给所有 worker，每个 worker 在自己的 tile 数据上算一遍，master 收回来合并。

```cpp
layertype[sol::meta_function::bitwise_and] = [](DRCLayer&, DRCLayer&) {
    return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
};
```

这里有个细节：表达式字符串怎么传给 worker？靠归一化时注入的 `__expr()` 调用：

```lua
__expr("a & b")    -- 设置 g_current_expr = "a & b"
__t1 = a & b        -- 触发运算符劫持，master 用 g_current_expr 分发
```

256 行绑定代码，覆盖了所有 DRC 操作。写的时候一个 `__newindex` 的遗漏都会导致 worker 上变量为 nil，得非常仔细。

---

## 协议和序列化：GDS2 的坑

Agent 写了 MPI 通信协议，只有三种消息类型：UPDATE_VAR（分发变量）、EXECUTE_RHS（执行表达式）、DONE（结束）。简单粗暴但够用。

序列化一开始选了 GDS2 格式——毕竟这是版图行业标准，Region 和 Texts 用 GDS2 序列化没问题。

但 GDS2 有个大坑，我们后来才踩到。

---

## 跑测试：4/20 通过

基础架构搭完，编译通过，开始跑测试。MPI 1x1（最简单的场景，1 master + 1 worker）：

```
20 total, 4 passed, 16 failed
```

稍微有点崩溃。不过 Non-MPI 基线全过，说明问题出在 MPI 层。

Agent 做了一个序列化 roundtrip 测试——序列化再反序列化，看数据是否完整：

```cpp
// Region: roundtrip OK
// Edges: original.count()=4, restored.count()=0  ← 丢了！
// EdgePairs: original.count()>0, restored.count()=0  ← 也丢了！
```

原因找到了：GDS2 格式根本不支持 Edges 和 EdgePairs。KLayout 写 GDS2 时把边和边对变成了多边形，读回来就变成 Region 了。这不是我们的 Bug，是 GDS2 格式的限制。

解决方案是为 Edges 和 EdgePairs 写自定义二进制序列化，绕过 GDS2：

```cpp
// Edge: [count][x1,y1,x2,y2] × count
// EdgePair: [count][x1,y1,x2,y2,x3,y3,x4,y4] × count
```

这一修，4/20 直接跳到 17/20。

这里有个感触：如果一开始猜"可能是序列化的问题"然后直接去改，大概率改不到点子上。倒是写一个 roundtrip 测试跑一下，一分钟就能定位。

---

## 最诡异的 Bug：幽灵 Worker

17/20 了，但剩下 3 个失败发生在 MPI 1x1 模式——理论上最不该出错的地方。

Agent 追踪 MPI 通信流程，发现 worker 收到了 `EXECUTE_RHS` 命令，但计算时变量是 nil。明明刚发给它的 `UPDATE_VAR` 啊？

细看原来是 `mpi_evaluate_expr` 的问题：

```cpp
// 原来的代码：broadcast 发给所有 worker
mpi_broadcast(0, EXECUTE_RHS, expr.data(), expr.size());
```

broadcast 发给了 所有 worker，包括空闲的。但变量更新只发给了活跃 worker（有 tile 的那些）。空闲 worker 收到表达式，用 nil 变量去算，当然崩了。

修法很简单——改成点对点发送，只给活跃 worker：

```cpp
int active = std::min(num_workers, (int)g_mpi_ctx->tiles.size());
for (int i = 0; i < active; i++) {
    mpi_send(i + 1, EXECUTE_RHS, expr.data(), expr.size());
}
```

**20/20，全过了。** 从发现问题到修好不到5分钟。

这种 Bug 特别有意思——它只在 worker 数量 > tile 数量时才出现，而我们的测试场景刚好满足条件。并发系统的边界条件经常这样。

---

## 测试通过了但还没完

20/20 通过了，我以为可以交差了。但 Agent 自己做了一遍代码审查，逐文件看。

在 `engine.cc` 的 `interacting` 方法里它发现了一个当前测试完全覆盖不到的 Bug：

```cpp
DRCLayer DRCLayer::interacting(const DRCLayer& other) const {
    if (m_type == Region && other.m_type == Region ...)   // 有
    if (m_type == Edges && other.m_type == Region ...)     // 有
    if (m_type == Edges && other.m_type == Edges ...)     // 有
    // EdgePairs + Region → 直接 return 空！               // 没有！
    return DRCLayer();
}
```

EdgePairs 和 Region 的 `interacting` 没实现！而在 `clip_to_tile` 里正好用到了：

```cpp
} else if (global.type() == DRCLayer::EdgePairs) {
    return global.interacting(DRCLayer(new db::Region(tile_region)));
    // 这里会返回空结果，数据全丢了
}
```

这是个定时炸弹——现在不炸，但未来用 2x2 模式处理 EdgePair 时一定会炸。

另外 `operator|` 也缺了 EdgePairs 的合并分支，也修了。

这种 Bug 如果不现在修，等真正触发的时候大概率是在最紧急的节点上。Agent 做代码审查的时候不会走神，会检查到每个分支路径，这是人很难做到的。

---

## 2x2 模式：不是 Bug，是代价

所有 Bug 修完后跑 2x2 模式（4 个 tile），20 个测试里 14 个过、6 个挂。

仔细看了这 6 个失败：

- Width/Space check：thin shape 被切到不同 tile，单个 tile 看不到违规
- Corner detection：正方形被切开后角点特征变了，检出 0 个角（应该 4 个）
- Sep/Overlap check：跨 tile 边界的空间关系检测不到
- Extended out：边被切后延伸结果不同

根因都是一样的——tile 分解的边界效应。这不是 Bug，是 tile-based 并行算法本身的代价。商业 DRC 工具一样有这个问题，靠增大 halo 区域来缓解。

我决定接受这 6 个失败，不追了。知道什么时候该停下，也挺重要的。

---

## 最后看一眼结果

我让 Agent 跑一个布尔运算的例子，三种模式各一遍：

```
Step 1: Layer A  count=1  area=4,000,000
Step 1: Layer B  count=1  area=4,000,000
Step 2: A & B    count=1  area=1,000,000
Step 3: A | B    count=1  area=7,000,000
Step 4: A - B    count=1  area=3,000,000
Step 5: A ~ B    count=1  area=6,000,000
Step 6: OR  = A + B - AND → 7,000,000 = 7,000,000 ✓
Step 7: XOR = OR - AND     → 6,000,000 = 6,000,000 ✓
```

Non-MPI、MPI 1x1、MPI 2x2——三种模式，数字一模一样。

看到这个结果的时候，说实话挺踏实的。

---

## 一些感受

整个过程里 Agent 做了一些我做不到或懒得做的事：

**它不会走神。** 几百行代码的审查，它会老老实实检查每个方法签名、每个类型分支。`interacting` 里缺了 EdgePairs 分支这种事，人审查很容易漏过去。

**它沿着因果链追。** 从 16/20 失败到定位 GDS2 序列化的问题，它不是猜，而是写 roundtrip 测试让数据自己说话。这在调试中特别高效。

**它做完之后还会继续找问题。** 测试全通过的时候人容易放松，但它继续做代码审查找出了两个隐藏 Bug。

但它也有明显的局限：

**它不能自己跑程序。** 所有测试都得我帮它执行，它看不到直接输出。我是它和真实世界之间的桥梁。

**它的领域知识有限。** GDS2 不支持 Edges 这种事，它不可能从文档里推导出来，得实际跑一遍才知道。

**判断力还是得靠人。** 2x2 的 6 个失败该不该修？这是架构判断，Agent 只能分析原因，决策还是得我来做。

---

## 总的来说

4686 行代码，7 次提交，从零到 MPI 分布式 DRC 引擎全部跑通。

Agent 不是一个更快的搜索引擎，也不是代码补全工具。当你把它当搭档——一起思考、一起实验、一起验证——的时候，它发挥的价值最大。它能追着你问"这里 EdgePairs 的分支呢？"，也能在你说"可以了"之后继续审查出隐藏 Bug。

试试吧。不是因为它能替你写代码，而是因为它能帮你想到你没看到的东西。

---

*所有内容基于真实开发记录。drc-engine 项目仓库包含完整 git 历史。*