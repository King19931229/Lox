# 闭包、Upvalue 与编译要点

与 [vm-stack-and-compiler-notes.md](./vm-stack-and-compiler-notes.md) 互补：局部/全局变量编译见该文档 §4–§5。本文档聚焦 `CaptureUpvalue` / `CloseUpvalues`、`openUpvalues`、`ResolveUpvalue`、`OP_CLOSURE`。

---

## 1. 核心数据结构

### 1.1 `UpvalueValue`（堆上）

```cpp
struct UpvalueValue : public Value {
    VMValue* location;       // open: 指向栈槽; closed: 指向自己的 closed
    VMValue  closed;
    UpvalueValue* nextUpvalue;
};
```

- **open**：`location` 指向栈上某个局部变量槽
- **closed**：`location = &closed`，值已从栈拷贝出来
- 对象在**堆**上，**不在栈**上

闭包 `VMClosureValue.upvalues[]` 存的是 **`VMValue` 句柄**（内含 `Value*`，指向堆上 `UpvalueValue`），不是再嵌一份 `UpvalueValue` 结构体。

### 1.2 `openUpvalues`（VM 全局）

- `VM` 的成员，**整条 VM 只有一条链表**，不是每个 `CallFrame` 一份
- 按 `location` 地址**从高到低**排序
- 只串联仍 **open**（`location` 还指着栈）的 `UpvalueValue`
- 与闭包 `upvalues[]` 不是两套数据：同一批堆对象；闭包持指针，链表方便按栈边界批量 close

### 1.3 栈、帧、函数各自独有什么


| 资源                                  | 范围                  |
| ----------------------------------- | ------------------- |
| `stacks[]` / `stackTop`             | **VM 全局一块栈**，所有调用共用 |
| `openUpvalues`                      | **VM 全局一条链表**       |
| `CallFrame`（`ip`、`slots`、`closure`） | 每次**正在执行**的调用一份     |
| `Chunk` 字节码                         | 每个**函数定义**一份        |
| `VMClosureValue` + `upvalues[]`     | 每个**闭包实例**一份        |


**一句话：函数独有的是字节码；栈和 `openUpvalues` 是 VM 公用的。**

---

## 2. `CaptureUpvalue` 与 `CloseUpvalues`

### 2.1 `CaptureUpvalue(local)`

**时机**：`OP_CLOSURE` 且元数据 `isLocal = 1`

1. 在 `openUpvalues` 链表中按 `location` 降序找插入位置
2. 若已有 `location == local` → **复用**已有节点（不 `new`）；多闭包捕获同一局部时共享
3. 否则 **新建** `UpvalueValue`，`location` 指向栈槽，插入链表

### 2.2 `CloseUpvalues(last)`

**语义**：关闭所有 `location >= last` 的 open upvalue

1. `closed = *location`
2. `location = &closed`
3. 从 `openUpvalues` 摘下


| 场景                 | `last`         | 通常关几个               |
| ------------------ | -------------- | ------------------- |
| `OP_CLOSE_UPVALUE` | `stackTop - 1` | 1 个（栈顶 captured 局部） |
| `OP_RETURN`        | `frame->slots` | 本帧全部仍 open 的        |


### 2.3 `OP_CLOSE_UPVALUE`

编译期：`isCaptured` 局部出作用域 emit 此指令；否则 `OP_POP`。

运行期（无操作数）：

1. `CloseUpvalues(stackTop - 1)` — close 指向栈顶这个 local 的 upvalue
2. `Pop()` — 弹掉栈顶 **local 那一格**（不是 `UpvalueValue` 对象）

等价于**带捕获的 `OP_POP`**。

---

## 3. `Compiler::Function` 与 `OP_CLOSURE`

```
子编译器 sub.CompileFunction()
  → VMFunctionValue（独立 Chunk）+ sub.upvalues[]

父 Chunk emit：
  OP_CONSTANT   <fn 原型>
  OP_CLOSURE
    <upvalueCount>
    <isLocal> <index> × N
```

- 子函数 **body** 在子 Chunk；父 Chunk 只留**创建闭包**的指令
- **运行期**执行到 `OP_CLOSURE` 才组装 `VMClosureValue`

---

## 4. `EmitConstant`

**编译期**：`AddConstant` 写入当前 Chunk 常量表 → emit `OP_CONSTANT`（或 `OP_CONSTANT_LONG`）+ 索引  
**运行期**：`READ_CONSTANT()` / `READ_LONG_CONSTANT()` → `Push` 到求值栈 `stacks[]`

---

## 5. `ResolveLocal` vs `ResolveUpvalue`

变量名读写由 `NamedVariable` 编译（见 [vm-stack-and-compiler-notes.md §5](./vm-stack-and-compiler-notes.md)）：栈顶留结果；赋值多一条 `SET_*`。

```text
ResolveLocal   → GET_LOCAL / SET_LOCAL
ResolveUpvalue → GET_UPVALUE / SET_UPVALUE
否则           → GET_GLOBAL / SET_GLOBAL
```

### `ResolveUpvalue(name)`

1. `enclosing == nullptr` → `-1`
2. 外层 `ResolveLocal` 命中 → `isCaptured = true`，`AddUpvalue(外层 slot, isLocal=true)`
3. 否则递归外层 `ResolveUpvalue` → `AddUpvalue(外层 upvalue 下标, isLocal=false)`
4. 返回**当前函数** `upvalues[]` 索引

### `isLocal` 记在哪


| 位置                                     | 含 `isLocal`？ |
| -------------------------------------- | ------------ |
| `OP_CLOSURE` 后操作数（父 Chunk）             | 有            |
| `GET_UPVALUE` / `SET_UPVALUE`（子 Chunk） | 无，只有下标       |



| `isLocal` | `OP_CLOSURE` 运行期                                          |
| --------- | --------------------------------------------------------- |
| `true`    | `CaptureUpvalue(&caller.slots[index])`，可能进 `openUpvalues` |
| `false`   | 复制 `caller.closure.upvalues[index]`，不新 capture 栈          |


---

## 6. 运行时栈变化速查


| 字节码             | 栈变化                                                                                  |
| --------------- | ------------------------------------------------------------------------------------ |
| `CONSTANT`      | push 常量                                                                              |
| `CLOSURE`       | pop 函数原型 → capture/继承 → push 闭包                                                      |
| `GET_LOCAL i`   | push `slots[i]`                                                                      |
| `GET_UPVALUE i` | push `*UpvalueValue::location`（经 `upvalues[i].value` 取堆对象） |
| `CALL n`        | 新建帧，`frame->slots` = callee 地址                                                       |
| `CLOSE_UPVALUE` | `CloseUpvalues(stackTop - 1)` → `Pop()` 弹栈顶 **local 格**                                  |
| `RETURN`        | `Pop()` 返回值 → `CloseUpvalues(frame->slots)` → `stackTop = frame->slots` → `Push` 返回值 |
| `DEFINE_GLOBAL` | pop → `globalSlots`                                                                  |


`CLOSE_UPVALUE`：栈顶是 **local**，先 close 再 Pop 这一格。  
`RETURN`：栈顶是 **返回值**，先 Pop 返回值；close 本帧 captured；整帧局部栈作废；再 Push 返回值。

---

## 7. 闭包生命周期简例

```lox
var f;
{
  var x = "initial";
  fun getter() { return x; }
  f = getter;
  x = "updated";
}
print f();  // updated
```

1. `OP_CLOSURE` → `CaptureUpvalue(&x)`，open
2. `x = "updated"` → `SET_LOCAL` 写栈槽（仍 open，`location` 仍指同一格）
3. `OP_CLOSE_UPVALUE` → close，`location` 指 `closed`
4. `f()` → `GET_UPVALUE 0` → `"updated"`

---

## 8. 易错点


| 误解                                    | 正解                        |
| ------------------------------------- | ------------------------- |
| upvalue 对象在栈上                         | 在堆上；栈上是被指向的值              |
| `openUpvalues` 按调用帧分                  | VM 全局一条链                  |
| 闭包 `upvalues[]` 与 `openUpvalues` 两套数据 | 同一批 `UpvalueValue`；组织方式不同 |
| `isLocal` 在 `GET_UPVALUE` 里           | 只在 `OP_CLOSURE` 操作数里      |
| 每次 `CaptureUpvalue` 都 `new`           | 同一栈槽复用已有节点                |
| `OP_CLOSE_UPVALUE` 的 Pop 弹 upvalue    | 弹 **local 栈格**            |
| 编译完就创建闭包                              | 运行到 `OP_CLOSURE` 才创建      |


