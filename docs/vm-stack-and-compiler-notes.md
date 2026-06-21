# VM 栈与编译器 locals 笔记

这份笔记总结当前编译器和 VM 里已经搞清楚的几个核心约定：`locals` 的意义、表达式临时值如何影响栈、`class` 声明如何绑定变量、`OP_CALL` 如何处理函数/类调用，以及属性访问和赋值的栈变化。

## 1. locals 的意义

`locals` 是编译期的数据结构，不是运行时值本身。

它的作用是记录当前函数编译期间，局部变量名对应当前调用帧里的哪个栈槽：

```text
locals:
  slot 0 : <closure / reserved>
  slot 1 : a
  slot 2 : b
```

运行时真正的值在 VM 栈上：

```text
frame->slots[0] : <closure>
frame->slots[1] : a 的值
frame->slots[2] : b 的值
```

所以 `locals` 的核心意义是：

```text
把源码里的局部变量名，在编译期解析成固定的栈槽编号。
```

例如编译到：

```lox
print a;
```

如果 `ResolveLocal("a")` 找到 slot 1，编译器就生成：

```text
OP_GET_LOCAL 1
```

VM 运行时不再按名字查 `a`，而是直接读当前 frame 的 slot 1。

## 2. 持久栈区与临时栈值

当前编译器隐含了一个非常重要的栈纪律：

```text
在语句边界，当前 frame 的持久栈区应该只包含 active locals。
```

也就是说：

```text
持久栈高度 == 当前 active locals 数量
```

普通表达式可以临时把值压在 locals 上方，但这些临时值不能跨过语句边界乱留在栈上，否则后面的 local slot 会和实际栈位置错位。

例如：

```lox
fun f() {
  var a = 1;
  b_global;
  var c = 2;
}
```

编译器认为：

```text
slot 1 = a
slot 2 = c
```

`b_global;` 执行中会把全局变量值临时压栈：

```text
slot 0 : <closure>
slot 1 : a
slot 2 : b_global 临时值
```

但因为它是表达式语句，编译器会生成 `OP_POP`：

```cpp
void Compiler::ExpressionStatement()
{
    Expression();
    Consume(SEMICOLON, "Expect ';' after expression.");
    EmitByte(OP_POP);
}
```

所以语句结束后恢复为：

```text
slot 0 : <closure>
slot 1 : a
```

接下来 `var c = 2;` 才能把 `2` 放到 slot 2。

结论：

```text
全局变量读取本身会压栈，但它是否持久留下来，取决于外层语法上下文。
```

## 3. 哪些东西会改变持久栈高度

从“持久栈区”的角度看：

```text
普通访问、普通赋值、计算、print、条件判断、函数调用表达式，都不应该改变 locals 区的持久布局。
```

真正改变持久栈高度的主要是：

```text
局部变量声明：持久栈 +1
离开局部作用域：持久栈 -N
函数调用进入/返回：切换或恢复 CallFrame
```

几个例子：

```lox
a;
```

字节码效果：

```text
OP_GET_* a
OP_POP
```

最终持久栈高度不变。

```lox
a = 1;
```

赋值表达式本身会留下结果，但表达式语句最后 `OP_POP`，最终持久栈高度不变。

```lox
var a = 1;
```

如果是局部变量声明，初始化值会留在栈上，成为新的 local slot，持久栈高度 +1。

```lox
var g = 1;
```

如果是全局变量声明，`OP_DEFINE_GLOBAL` 会把初始化值从栈上弹出，存入全局表，当前 frame 持久栈高度不变。

## 4. DeclareVariable 与 DefineVariable

局部变量声明分两步：

```cpp
DeclareVariable(false);
// 编译 initializer，值被压到栈顶
DefineVariable(global, false);
```

`DeclareVariable()` 的意义是先把名字放进 `locals`，但标记为未初始化：

```text
depth = -1
```

这表示：

```text
这个名字已经占位，但暂时还不能作为可读 local 使用。
```

`DefineVariable()` 在局部作用域里不会发字节码，只做：

```cpp
MarkInitialize();
```

也就是把 `depth` 从 `-1` 改成当前 `scopeDepth`。

这样设计的原因是：初始化表达式编译完成后，值已经在栈顶，此时才可以正式把这个栈槽视为该 local 的值。

例如：

```lox
var a = 1;
```

局部作用域下大致是：

```text
DeclareVariable("a")  -> locals 新增 a, depth = -1
OP_CONSTANT 1         -> 栈顶出现 1
DefineVariable("a")   -> a.depth = currentScopeDepth
```

此时这个栈顶值就成为 `a` 的 local slot。

## 5. global 与 local 的解析顺序

变量访问不是先天知道是 local 还是 global，而是按顺序解析：

```text
ResolveLocal(name)
ResolveUpvalue(name)
否则当作 global
```

如果当前函数的 `locals` 里找到了名字，就生成 `OP_GET_LOCAL` / `OP_SET_LOCAL`。

如果当前函数找不到，但外层函数能找到，就生成 upvalue 相关字节码。

如果 local 和 upvalue 都找不到，才生成 `OP_GET_GLOBAL` / `OP_SET_GLOBAL`。

所以：

```lox
fun f() {
  var a_local = 1;
  print b_global;
  var c_local = 2;
}
```

`locals` 只会记录：

```text
slot 1 : a_local
slot 2 : c_local
```

`b_global` 不会进入 `locals`。它只是一次全局读取表达式，执行后结果由外层 `print` 消费。

## 6. class 声明的正确顺序

类声明本质上类似：

```lox
var Foo = <class named "Foo">;
```

正确顺序应该是：

```cpp
Consume(IDENTIFIER, "Expect class name.");
uint32_t nameConstant = IdentifierConstant(parser.previous);
DeclareVariable(false);

EmitBytes(OP_CLASS, nameConstant);
DefineVariable(nameConstant, false);

Consume(LEFT_BRACE, "Expect '{' before class body.");
Consume(RIGHT_BRACE, "Expect '}' after class body.");
```

`nameConstant` 是类名字符串在常量表里的索引，例如 `"Foo"`。

它不是 local slot。

`OP_CLASS nameConstant` 的运行时意义是：

```text
读取常量表里的 "Foo"
创建 <class Foo>
把类对象压栈
```

然后 `DefineVariable()` 才把这个类对象绑定到变量名 `Foo`。

局部作用域下：

```text
DeclareVariable("Foo") -> locals 新增 Foo, depth = -1
OP_CLASS "Foo"         -> 栈顶出现 <class Foo>
DefineVariable("Foo")  -> Foo.depth = currentScopeDepth
```

全局作用域下：

```text
OP_CLASS "Foo"             -> 栈顶出现 <class Foo>
OP_DEFINE_GLOBAL "Foo"     -> 弹出类对象，存进 globals["Foo"]
```

## 7. Pratt parser 中的 call 何时触发

`OP_CALL` 不是在类声明时生成的，而是在编译调用表达式时生成的。

例如：

```lox
Foo()
foo(1, 2)
getFn()()
```

在 Pratt parser 里，`LEFT_PAREN` 同时有 prefix 和 infix 两种含义：

```cpp
rules[LEFT_PAREN] = { &Compiler::Grouping, &Compiler::Call, PREC_CALL };
```

当 `(` 出现在表达式开头：

```lox
(1 + 2)
```

它走 prefix：`Grouping()`。

当 `(` 出现在已经解析好的左表达式后面：

```lox
Foo()
```

它走 infix：`Call()`。

也就是说：

```text
先编译左边 callee，例如 Foo
然后 ParsePrecedence 的 while 看到当前 token 是 (
由于 LEFT_PAREN 的 infix 是 Call
于是进入 Compiler::Call()
```

`Compiler::Call()` 会编译参数列表，然后生成：

```cpp
EmitBytes(OP_CALL, argCount);
```

所以：

```lox
Foo(1, 2)
```

大概生成：

```text
OP_GET_GLOBAL "Foo"
OP_CONSTANT 1
OP_CONSTANT 2
OP_CALL 2
```

## 8. OP_CALL 与 class 实例化

调用前栈布局是：

```text
... [callee] [arg1] [arg2] ... [argN]
```

`OP_CALL` 通过：

```cpp
VMValue callee = Peek(argCount);
```

拿到参数下面的 callee。

如果 callee 是普通函数，调用结束后要把整个调用现场折叠成一个返回值：

```text
... [callee] [arg1] [arg2] -> ... [result]
```

native 函数当前实现是：

```cpp
VMValue result = nativeFunction->function(argCount, stackTop - argCount);
stackTop -= argCount + 1;
Push(result);
```

这等价于：

```text
弹掉 callee 和参数，再压回 result
```

类调用也是调用表达式：

```lox
Foo()
```

语义是创建实例。

如果当前还没有实现 `init`，class 分支可以直接把调用现场折叠成实例：

```cpp
if (callee.value->type == TYPE_CLASS)
{
    VMValue instance = VM::Create(new Compiler::VMInstanceValue(callee));
    stackTop[-argCount - 1] = instance;
    stackTop -= argCount;
    return true;
}
```

这里：

```cpp
stackTop[-argCount - 1]
```

正好是 callee 槽位。

所以栈变化是：

```text
... [Foo] [arg1] [arg2]
... [instance] [arg1] [arg2]
... [instance]
```

不能简单 `Push(instance)`，否则会变成：

```text
... [Foo] [arg1] [arg2] [instance]
```

调用现场没有被收掉，后续栈布局会错。

clox 里先只替换 callee 槽位，是为了后续支持 `init`：

```text
... [Foo] [arg1] [arg2]
... [instance] [arg1] [arg2]
```

这样可以继续把原来的参数用于初始化器调用。

如果没有 `init`，还需要把参数收掉，最终只留下 instance。

## 9. 属性访问与赋值

`dot()` 编译属性访问：

```cpp
static void dot(bool canAssign) {
  consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
  uint8_t name = identifierConstant(&parser.previous);

  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    emitBytes(OP_SET_PROPERTY, name);
  } else {
    emitBytes(OP_GET_PROPERTY, name);
  }
}
```

它是 `.` 的 infix 解析函数。

进入 `dot()` 前，左边对象表达式已经编译完，运行时对象值已经在栈上。

### 读取属性

源码：

```lox
obj.field
```

编译后的字节码形态大致是：

```text
<编译 obj 表达式>
OP_GET_PROPERTY nameConstant
```

其中：

```text
constants[nameConstant] = "field"
```

运行时栈变化只看栈里的值：

```text
编译 obj 表达式后: [obj]
执行 OP_GET_PROPERTY 后: [value]
```

`OP_GET_PROPERTY` 负责从 instance 字段表读取 `"field"`。从栈效果看，它消费一个对象值，并留下一个属性值。

实现上可以是：

```cpp
VMValue instance = Pop();
VMValue value = GetField(instance, name);
Push(value);
```

也可以是原地覆盖：

```cpp
VMValue instance = stackTop[-1];
VMValue value = GetField(instance, name);
stackTop[-1] = value;
```

两种实现的栈效果相同。这里说的“替换”只是最终栈效果，不是要求必须用某一种具体实现。

### 设置属性

源码：

```lox
obj.field = value
```

编译后的字节码形态大致是：

```text
<编译 obj 表达式>
<编译 value 表达式>
OP_SET_PROPERTY nameConstant
```

其中：

```text
constants[nameConstant] = "field"
```

运行时栈变化只看栈里的值：

```text
编译 obj 表达式后: [obj]
编译 value 表达式后: [obj][value]
执行 OP_SET_PROPERTY 后: [value]
```

`OP_SET_PROPERTY` 使用：

```text
peek(1) = obj
peek(0) = value
```

然后执行：

```text
obj.fields["field"] = value
```

最后它要把对象从栈上移走，但保留 value 作为赋值表达式的结果。

一种实现方式是：

```cpp
VMValue value = Pop();
VMValue instance = Pop();
SetField(instance, name, value);
Push(value);
```

也可以用原地覆盖：

```cpp
VMValue value = stackTop[-1];
VMValue instance = stackTop[-2];
SetField(instance, name, value);
stackTop[-2] = value;
stackTop--;
```

两种最终栈效果一样：

```text
[obj][value] -> [value]
```

如果外层是表达式语句：

```lox
obj.field = value;
```

最后还会有 `OP_POP`，所以整条语句结束后持久栈高度不变。

## 10. 最核心的总规则

可以把当前编译器和 VM 的关系压缩成这几条：

```text
1. 表达式通常负责留下一个结果在栈顶。
2. 外层语法结构决定这个结果是消费、保存、返回，还是丢弃。
3. locals 表只对应当前 frame 的持久 local 槽位。
4. 临时值可以出现在 locals 上方，但必须按语法结构的栈约定被处理掉。
5. 局部变量声明是少数会让持久栈高度增长的语法结构。
6. 离开作用域会弹出该作用域声明的 locals。
7. 如果某条编译路径漏掉必要的 OP_POP，或者某条 VM 指令没有按约定收栈，后续 local slot 就会错位。
```

最重要的一句话：

```text
编译器分配 local slot 时，默认当前 frame 的持久栈区只由 active locals 组成；所有表达式临时值都必须被外层语法或 VM 指令正确收束。
```
