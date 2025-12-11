# OTCC Learning Project

## 项目简介

这是一个学习 Fabrice Bellard 的 **Obfuscated Tiny C Compiler (OTCC)** 源代码的项目。OTCC 是一个极其精巧的 C 编译器，由 Fabrice Bellard 在 2002 年为参加国际混淆 C 代码竞赛 (IOCCC) 而编写。

### 什么是 OTCC？

OTCC 的设计目标是编写一个能够**自编译**的最小 C 编译器。Bellard 选择了一个足够通用的 C 子集来编写小型 C 编译器，然后扩展这个 C 子集，直到达到竞赛允许的最大尺寸：2048 字节的 C 源代码（不包括 ';'、'{'、'}' 和空格字符）。

### 技术特点

- **目标架构**: 生成 i386 机器码
- **原始版本**: 只能在 i386 Linux 上运行，依赖字节序和非对齐访问
- **执行方式**: 在内存中生成程序并直接启动，通过 `dlsym()` 解析外部符号
- **便携版本**: OTCCELF 变体，直接生成动态链接的 i386 ELF 可执行文件，无需依赖任何 binutils 工具

> **注意**: Bellard 的另一个著名项目 [TinyCC](https://bellard.org/tcc/) 就是基于 OTCC 的源代码开发的完整 ISO C99 编译器！

## 项目文件说明

本项目包含以下文件：

- **`otcc.c`** - 原始混淆版本（仅在 i386 Linux 上运行）
- **`otccelf.c`** - ELF 输出版本（具有更好的可移植性）
- **`otccn.c`** - 非混淆版本（用于学习和文档目的，不支持自编译）
- **`otccelfn.c`** - ELF 版本的非混淆代码（用于学习和文档目的）
- **`otccex.c`** - 示例 C 程序，展示 OTCC 支持的 C 子集

## 编译方法

### 编译 OTCC

```bash
gcc -m32 -O2 -Wl,-z,execstack -no-pie otcc.c -o otcc -ldl
```

### 编译 OTCCELF

```bash
gcc -m32 -O2 otccelf.c -o otccelf
```

### 自编译测试

```bash
./otccelf otccelf.c otccelf1
```

### 编译选项说明

- **`-Wl,-z,execstack`** - 现代 Linux 系统需要此选项来强制启用可执行数据段
- **`-no-pie`** - 某些 Linux 系统需要此选项来禁用位置无关可执行文件（OTCC 依赖 C 分配的数据地址小于 0x80000000，OTCCELF 没有此限制）
## OTCC 支持的 C 子集

参考 `otccex.c` 文件可以了解 OTCC 支持的 C 程序示例。

### 表达式

- **二元运算符**（按优先级递减顺序）：
  - `*` `/` `%`
  - `+` `-`
  - `>>` `<<`
  - `<` `<=` `>` `>=`
  - `==` `!=`
  - `&`
  - `^`
  - `|`
  - `=`
  - `&&`
  - `||`

- **逻辑运算符**: `&&` 和 `||` 具有与 C 相同的语义：从左到右求值和短路求值
- **括号**: 支持括号表达式
- **一元运算符**: `&`、`*`（指针解引用）、`-`（取负）、`+`、`!`、`~`、后缀 `++` 和 `--`

### 类型和变量

- **类型**: 只能声明有符号整数（`int`）变量和函数
- **变量**: 不能在声明时初始化
- **函数声明**: 只解析旧式 K&R 函数声明（隐式整数返回值，参数无类型）
- **指针**: 指针解引用（`*`）只能用于显式转换为 `char *`、`int *` 或 `int (*)()` （函数指针）
- **左值**: `++`、`--` 和一元 `&` 只能用于变量左值；`=` 只能用于变量或 `*`（指针解引用）左值

### 函数调用

- 支持标准 i386 调用约定的函数调用
- 支持带显式转换的函数指针
- 函数可以在声明前使用
- 可以使用 libc 中的任何函数或变量（OTCC 使用 libc 动态链接器解析未定义符号）

### 控制结构

- **代码块**: 支持 `{` `}` 代码块
- **条件**: 支持 `if` 和 `else`
- **循环**: 支持 `while` 和 `for` 循环
- **跳转**: 支持 `break` 退出循环，`return` 返回函数值

### 其他特性

- **标识符**: 与 C 相同的解析方式，支持局部变量但没有局部命名空间
- **数字**: 支持十进制、十六进制（`0x` 或 `0X` 前缀）、八进制（`0` 前缀）
- **预处理**: 支持不带函数参数的 `#define`，不支持宏递归，忽略其他预处理指令
- **字符串**: 支持 C 字符串和字符常量，只识别 `\n`、`\"`、`\'` 和 `\\` 转义序列
- **注释**: 支持 C 风格注释（不支持 C++ 风格注释）
- **错误处理**: 对于错误的程序不显示错误信息
- **内存限制**: 代码、数据和符号大小限制为 100KB（可在源代码中修改）

## 使用方法

### OTCC 调用方式

```bash
otcc prog.c [args]...
```

或者通过标准输入提供 C 源代码。`args` 参数会传递给 `prog.c` 的 `main` 函数（`argv[0]` 是 `prog.c`）。

#### 使用示例

```bash
# 示例编译和执行
otcc otccex.c 10

# 自编译
otcc otcc.c otccex.c 10

# 迭代自编译
otcc otcc.c otcc.c otccex.c 10
```

#### 脚本解释器模式

可以在 C 源文件开头添加以下行来作为脚本解释器使用：

```bash
#!/usr/local/bin/otcc
```

### OTCCELF 调用方式

```bash
otccelf prog.c prog
chmod 755 prog
```

其中 `prog` 是要生成的 ELF 文件名。

#### 性能对比

虽然生成的 i386 代码质量不如 GCC，但对于小型源文件，生成的 ELF 可执行文件要小得多：

```c
#include <stdio.h>

main() 
{
    printf("Hello World\n");
    return 0;
}
```

| 编译器 | 可执行文件大小（字节） |
|--------|----------------------|
| OTCCELF | 424 |
| GCC (stripped) | 2448 |

## 相关链接

- [TinyCC](https://bellard.org/tcc/) - 完整的小型 C 编译器
- [Tiny ELF programs](http://www.muppetlabs.com/~breadbox/software/tiny/) - Brian Raiter 的微型 ELF 程序
- [Linux assembly projects](http://asm.sourceforge.net/resources.html) - Linux 汇编项目
- [OTCC 原始页面](https://bellard.org/otcc/) - Fabrice Bellard 的官方页面

## 许可证

- 混淆版本的 OTCC 和 OTCCELF 为公共领域
- 非混淆版本采用类 BSD 许可证（详见源代码开头的许可证说明）