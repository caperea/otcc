/*
  Obfuscated Tiny C Compiler

  Copyright (C) 2001-2003 Fabrice Bellard

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product and its documentation 
     *is* required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#ifndef TINY
#include <stdarg.h>
#endif
#include <stdio.h>

/* vars: value of variables 
   loc : local variable index
   glo : global variable index
   ind : output code ptr
   rsym: return symbol
   prog: output code
   dstk: define stack
   dptr, dch: macro state
*/
int tok, tokc, tokl, ch, vars, rsym, prog, ind, loc, glo, file, sym_stk, dstk, dptr, dch, last_id;

#define ALLOC_SIZE 99999

/* depends on the init string */
#define TOK_STR_SIZE 48
#define TOK_IDENT    0x100
#define TOK_INT      0x100
#define TOK_IF       0x120
#define TOK_ELSE     0x138
#define TOK_WHILE    0x160
#define TOK_BREAK    0x190
#define TOK_RETURN   0x1c0
#define TOK_FOR      0x1f8
#define TOK_DEFINE   0x218
#define TOK_MAIN     0x250

#define TOK_DUMMY   1
#define TOK_NUM     2

#define LOCAL   0x200

#define SYM_FORWARD 0
#define SYM_DEFINE  1

/* tokens in string heap */
#define TAG_TOK    ' '
#define TAG_MACRO  2 /* STX, Start of Text */

/*
 * pdef - 将字符写入定义栈（dstk）
 * 功能：向定义栈(dstk)中写入一个字符，用于宏定义和标识符存储
 * 输入：t - 要写入的字符
 * 输出：无
 * 状态变化：dstk指针递增，栈中添加一个字符
 * 主要逻辑：将字符t写入dstk指向的位置，然后dstk指针向前移动
 */
pdef(t)
{
    *(char *)dstk++ = t;
}

/*
 * inp - 字符输入函数，支持宏展开
 * 功能：从文件或宏定义中读取下一个字符，支持宏的递归展开
 * 输入：无（使用全局变量）
 * 输出：无（结果存储在全局变量ch中）
 * 状态变化：
 *   - ch: 存储读取的字符
 *   - dptr: 如果从宏读取，指针向前移动；遇到宏结束标记时置0
 *   - dch: 宏展开时的保存字符
 * 主要逻辑：
 *   1. 如果dptr非空，从宏定义中读取字符
 *   2. 遇到TAG_MACRO标记时结束宏展开，恢复之前的字符
 *   3. 否则从文件中读取字符
 */
inp()
{
    if (dptr) {
        ch = *(char *)dptr++;
        if (ch == TAG_MACRO) {
            dptr = 0;
            ch = dch;
        }
    } else
        ch = fgetc(file);
    /*    printf("ch=%c 0x%x\n", ch, ch); */
}

/*
 * isid - 判断字符是否为标识符字符
 * 功能：检查当前字符是否可以作为标识符的一部分
 * 输入：无（使用全局变量ch）
 * 输出：非零表示是标识符字符，零表示不是
 * 状态变化：无
 * 主要逻辑：判断字符是否为字母、数字或下划线
 */
isid()
{
    return isalnum(ch) | ch == '_';
}

/*
 * getq - 读取字符常量，处理转义序列
 * 功能：处理字符常量中的转义字符，主要是\n
 * 输入：无（使用全局变量ch）
 * 输出：无（结果存储在全局变量ch中）
 * 状态变化：ch可能被转换为相应的转义字符
 * 主要逻辑：
 *   1. 如果当前字符是反斜杠，读取下一个字符
 *   2. 如果下一个字符是'n'，将ch设置为换行符
 */
getq()
{
    if (ch == '\\') {
        inp();
        if (ch == 'n')
            ch = '\n';
    }
}

/*
 * next - 词法分析器主函数
 * 功能：解析下一个token，处理标识符、数字、操作符、字符串、注释和预处理指令
 * 输入：无（使用全局变量）
 * 输出：无（结果存储在tok, tokc, tokl等全局变量中）
 * 状态变化：
 *   - tok: 当前token的类型
 *   - tokc: token的值（对于数字和操作符）
 *   - tokl: token的长度或优先级
 *   - last_id: 最后一个标识符的位置
 *   - dstk: 定义栈可能增长（存储标识符和宏定义）
 * 主要逻辑：
 *   1. 跳过空白字符和处理预处理指令（#define）
 *   2. 识别标识符和数字
 *   3. 处理字符常量
 *   4. 处理注释
 *   5. 解析操作符（使用编码的操作符表）
 *   6. 支持宏展开
 */
next()
{
    int t, l, a;

    while (isspace(ch) | ch == '#') {
        if (ch == '#') {
            inp();
            next();
            if (tok == TOK_DEFINE) {
                next();
                pdef(TAG_TOK); /* fill last ident tag */
                *(int *)tok = SYM_DEFINE;
                *(int *)(tok + 4) = dstk; /* define stack */
            }
            /* well we always save the values ! */
            while (ch != '\n') {
                pdef(ch);
                inp();
            }
            pdef(ch);
            pdef(TAG_MACRO);
        }
        inp();
    }
    tokl = 0;
    tok = ch;
    /* encode identifiers & numbers */
    if (isid()) {
        pdef(TAG_TOK);
        last_id = dstk;
        while (isid()) {
            pdef(ch);
            inp();
        }
        if (isdigit(tok)) {
            tokc = strtol(last_id, 0, 0);
            tok = TOK_NUM;
        } else {
            *(char *)dstk = TAG_TOK; /* no need to mark end of string (we
                                        suppose data is initied to zero */
            tok = strstr(sym_stk, last_id - 1) - sym_stk;
            *(char *)dstk = 0;   /* mark real end of ident for dlsym() */
            tok = tok * 8 + TOK_IDENT;
            if (tok > TOK_DEFINE) {
                tok = vars + tok;
                /*        printf("tok=%s %x\n", last_id, tok); */
                /* define handling */
                if (*(int *)tok == SYM_DEFINE) {
                    dptr = *(int *)(tok + 4);
                    dch = ch;
                    inp();
                    next();
                }
            }
        }
    } else {
        inp();
        if (tok == '\'') {
            tok = TOK_NUM;
            getq();
            tokc = ch;
            inp();
            inp();
        } else if (tok == '/' & ch == '*') {
            inp();
            while (ch) {
                while (ch != '*')
                    inp();
                inp();
                if (ch == '/')
                    ch = 0;
            }
            inp();
            next();
        } else
        {
            t = "++#m--%am*@R<^1c/@%[_[H3c%@%[_[H3c+@.B#d-@%:_^BKd<<Z/03e>>`/03e<=0f>=/f<@.f>@1f==&g!=\'g&&k||#l&@.BCh^@.BSi|@.B+j~@/%Yd!@&d*@b";
            while (l = *(char *)t++) {
                a = *(char *)t++;
                tokc = 0;
                while ((tokl = *(char *)t++ - 'b') < 0)
                    tokc = tokc * 64 + tokl + 64;
                if (l == tok & (a == ch | a == '@')) {
#if 0
                    printf("%c%c -> tokl=%d tokc=0x%x\n", 
                           l, a, tokl, tokc);
#endif
                    if (a == ch) {
                        inp();
                        tok = TOK_DUMMY; /* dummy token for double tokens */
                    }
                    break;
                }
            }
        }
    }
#if 0
    {
        int p;

        printf("tok=0x%x ", tok);
        if (tok >= TOK_IDENT) {
            printf("'");
            if (tok > TOK_DEFINE) 
                p = sym_stk + 1 + (tok - vars - TOK_IDENT) / 8;
            else
                p = sym_stk + 1 + (tok - TOK_IDENT) / 8;
            while (*(char *)p != TAG_TOK && *(char *)p)
                printf("%c", *(char *)p++);
            printf("'\n");
        } else if (tok == TOK_NUM) {
            printf("%d\n", tokc);
        } else {
            printf("'%c'\n", tok);
        }
    }
#endif
}

#ifdef TINY
#define skip(c) next()
#else

/*
 * error - 错误处理函数
 * 功能：输出错误信息并终止程序
 * 输入：fmt - 格式化字符串，... - 可变参数
 * 输出：向stderr输出错误信息
 * 状态变化：程序终止（exit(1)）
 * 主要逻辑：
 *   1. 输出当前文件位置
 *   2. 格式化并输出错误信息
 *   3. 终止程序执行
 */
void error(char *fmt,...)
{
    va_list ap;

    va_start(ap, fmt);
    fprintf(stderr, "%d: ", ftell((FILE *)file));
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
    va_end(ap);
}

/*
 * skip - 跳过期望的字符token
 * 功能：检查当前token是否为期望的字符，如果是则读取下一个token，否则报错
 * 输入：c - 期望的字符
 * 输出：无
 * 状态变化：如果匹配，调用next()更新token状态；如果不匹配，程序终止
 * 主要逻辑：
 *   1. 检查当前token是否等于期望字符
 *   2. 如果不匹配，调用error()报错并终止
 *   3. 如果匹配，调用next()读取下一个token
 */
void skip(c)
{
    if (tok != c) {
        error("'%c' expected", c);
    }
    next();
}

#endif

/*
 * o - 输出机器码字节
 * 功能：将整数n的各个字节依次输出到代码缓冲区
 * 输入：n - 要输出的机器码（可能包含多个字节）
 * 输出：无
 * 状态变化：ind指针向前移动，代码缓冲区中添加字节
 * 主要逻辑：
 *   1. 逐字节输出n的低位字节到代码缓冲区
 *   2. 将n右移8位处理下一个字节
 *   3. 直到n为0或-1时停止
 */
o(n)
{
    /* cannot use unsigned, so we must do a hack */
    while (n && n != -1) {
        *(char *)ind++ = n;
        n = n >> 8;
    }
}

/*
 * gsym - 符号地址回填函数
 * 功能：回填符号的地址，解决前向引用问题
 * 输入：t - 需要回填的地址链表头
 * 输出：无
 * 状态变化：修改代码缓冲区中的地址引用
 * 主要逻辑：
 *   1. 遍历需要回填的地址链表
 *   2. 计算相对地址（ind - t - 4）
 *   3. 将计算出的地址写回对应位置
 *   4. 处理链表中的下一个地址
 */
gsym(t)
{
    int n;
    while (t) {
        n = *(int *)t; /* next value */
        *(int *)t = ind - t - 4;
        t = n;
    }
}

/* psym is used to put an instruction with a data field which is a
   reference to a symbol. It is in fact the same as oad ! */
#define psym oad

/*
 * oad - 输出指令和地址
 * 功能：输出一个指令后跟一个4字节地址，用于需要地址操作数的指令
 * 输入：n - 指令码，t - 地址值
 * 输出：返回地址字段的位置
 * 状态变化：ind指针向前移动，代码缓冲区添加指令和地址
 * 主要逻辑：
 *   1. 调用o(n)输出指令码
 *   2. 在当前位置写入4字节地址
 *   3. 保存地址字段的位置
 *   4. ind向前移动4字节
 *   5. 返回地址字段位置供后续回填使用
 */
oad(n, t)
{
    o(n);
    *(int *)ind = t;
    t = ind;
    ind = ind + 4;
    return t;
}

/*
 * li - 加载立即数到EAX寄存器
 * 功能：生成mov指令将立即数加载到EAX寄存器
 * 输入：t - 要加载的立即数值
 * 输出：无
 * 状态变化：代码缓冲区添加mov指令
 * 主要逻辑：生成"mov $t, %eax"指令（机器码0xb8）
 */
li(t)
{
    oad(0xb8, t); /* mov $xx, %eax */
}

/*
 * gjmp - 生成无条件跳转指令
 * 功能：生成jmp指令，支持前向引用
 * 输入：t - 跳转目标地址（可能是前向引用）
 * 输出：返回需要回填的地址位置
 * 状态变化：代码缓冲区添加jmp指令
 * 主要逻辑：生成"jmp target"指令（机器码0xe9），返回地址字段位置
 */
gjmp(t)
{
    return psym(0xe9, t);
}

/*
 * gtst - 生成条件跳转指令
 * 功能：根据EAX的值生成条件跳转（je或jne）
 * 输入：l - 跳转条件（0=je, 1=jne），t - 跳转目标
 * 输出：返回需要回填的地址位置
 * 状态变化：代码缓冲区添加test和条件跳转指令
 * 主要逻辑：
 *   1. 生成"test %eax, %eax"指令测试EAX
 *   2. 根据l的值生成je（0x84）或jne（0x85）指令
 */
gtst(l, t)
{
    o(0x0fc085); /* test %eax, %eax, je/jne xxx */
    return psym(0x84 + l, t);
}

/*
 * gcmp - 生成比较指令序列
 * 功能：比较EAX和ECX，根据比较结果设置EAX为0或1
 * 输入：t - 比较类型（setxx指令的操作码）
 * 输出：无
 * 状态变化：代码缓冲区添加比较指令序列，EAX被设置为比较结果
 * 主要逻辑：
 *   1. 生成"cmp %eax, %ecx"指令
 *   2. 将EAX清零
 *   3. 根据比较结果用setxx指令设置AL（EAX的低8位）
 */
gcmp(t)
{
    o(0xc139); /* cmp %eax,%ecx */
    li(0);
    o(0x0f); /* setxx %al */
    o(t + 0x90);
    o(0xc0);
}

/*
 * gmov - 生成内存访问指令
 * 功能：生成mov/lea指令访问变量（全局或局部）
 * 输入：l - 指令类型，t - 变量地址
 * 输出：无
 * 状态变化：代码缓冲区添加内存访问指令
 * 主要逻辑：
 *   1. 生成基础指令码（l + 0x83）
 *   2. 根据变量是否为局部变量选择寻址模式
 *   3. 局部变量使用EBP相对寻址，全局变量使用绝对寻址
 */
gmov(l, t)
{
    o(l + 0x83);
    oad((t < LOCAL) << 7 | 5, t);
}

/*
 * unary - 解析一元表达式
 * 功能：解析一元表达式，包括常量、变量、函数调用、指针操作等
 * 输入：l - 是否允许赋值操作（1表示允许）
 * 输出：无（结果在EAX寄存器中）
 * 状态变化：
 *   - 代码缓冲区添加相应指令
 *   - 可能修改glo（全局数据指针）
 *   - 可能调用next()更新token状态
 * 主要逻辑：
 *   1. 处理字符串字面量（存储到全局数据区）
 *   2. 处理数字常量（加载到EAX）
 *   3. 处理一元运算符（-, +, !, ~）
 *   4. 处理括号表达式
 *   5. 处理指针解引用和类型转换
 *   6. 处理取地址操作（&）
 *   7. 处理变量访问和赋值
 *   8. 处理函数调用（参数压栈、调用、清理栈）
 */
unary(l)
{
    int n, t, a, c;

    n = 1; /* type of expression 0 = forward, 1 = value, other =
              lvalue */
    if (tok == '\"') {
        li(glo);
        while (ch != '\"') {
            getq();
            *(char *)glo++ = ch;
            inp();
        }
        *(char *)glo = 0;
        glo = glo + 4 & -4; /* align heap */
        inp();
        next();
    } else {
        c = tokl;
        a = tokc;
        t = tok;
        next();
        if (t == TOK_NUM) {
            li(a);
        } else if (c == 2) {
            /* -, +, !, ~ */
            unary(0);
            oad(0xb9, 0); /* movl $0, %ecx */
            if (t == '!')
                gcmp(a);
            else
                o(a);
        } else if (t == '(') {
            expr();
            skip(')');
        } else if (t == '*') {
            /* parse cast */
            skip('(');
            t = tok; /* get type */
            next(); /* skip int/char/void */
            next(); /* skip '*' or '(' */
            if (tok == '*') {
                /* function type */
                skip('*');
                skip(')');
                skip('(');
                skip(')');
                t = 0;
            }
            skip(')');
            unary(0);
            if (tok == '=') {
                next();
                o(0x50); /* push %eax */
                expr();
                o(0x59); /* pop %ecx */
                o(0x0188 + (t == TOK_INT)); /* movl %eax/%al, (%ecx) */
            } else if (t) {
                if (t == TOK_INT)
                    o(0x8b); /* mov (%eax), %eax */
                else 
                    o(0xbe0f); /* movsbl (%eax), %eax */
                ind++; /* add zero in code */
            }
        } else if (t == '&') {
            gmov(10, *(int *)tok); /* leal EA, %eax */
            next();
        } else {
            n = *(int *)t;
            /* forward reference: try dlsym */
            if (!n)
                n = dlsym(0, last_id);
            if (tok == '=' & l) {
                /* assignment */
                next();
                expr();
                gmov(6, n); /* mov %eax, EA */
            } else if (tok != '(') {
                /* variable */
                gmov(8, n); /* mov EA, %eax */
                if (tokl == 11) {
                    gmov(0, n);
                    o(tokc);
                    next();
                }
            }
        }
    }

    /* function call */
    if (tok == '(') {
        if (n == 1)
            o(0x50); /* push %eax */

        /* push args and invert order */
        a = oad(0xec81, 0); /* sub $xxx, %esp */
        next();
        l = 0;
        while(tok != ')') {
            expr();
            oad(0x248489, l); /* movl %eax, xxx(%esp) */
            if (tok == ',')
                next();
            l = l + 4;
        }
        *(int *)a = l;
        next();
        if (!n) {
            /* forward reference */
            t = t + 4;
            *(int *)t = psym(0xe8, *(int *)t);
        } else if (n == 1) {
            oad(0x2494ff, l); /* call *xxx(%esp) */
            l = l + 4;
        } else {
            oad(0xe8, n - ind - 5); /* call xxx */
        }
        if (l)
            oad(0xc481, l); /* add $xxx, %esp */
    }
}

/*
 * sum - 解析二元表达式（运算符优先级）
 * 功能：使用递归下降法解析二元表达式，处理运算符优先级
 * 输入：l - 当前运算符优先级层次
 * 输出：无（结果在EAX寄存器中）
 * 状态变化：
 *   - 代码缓冲区添加运算指令
 *   - 可能调用next()更新token状态
 * 主要逻辑：
 *   1. 如果优先级为1，调用unary()处理一元表达式
 *   2. 否则递归处理更高优先级的表达式
 *   3. 处理当前优先级的运算符
 *   4. 对于逻辑运算符（&&, ||），生成短路求值代码
 *   5. 对于算术和比较运算符，生成相应的机器指令
 */
sum(l)
{
    int t, n, a;

    if (l-- == 1)
        unary(1);
    else {
        sum(l);
        a = 0;
        while (l == tokl) {
            n = tok;
            t = tokc;
            next();

            if (l > 8) {
                a = gtst(t, a); /* && and || output code generation */
                sum(l);
            } else {
                o(0x50); /* push %eax */
                sum(l);
                o(0x59); /* pop %ecx */
                
                if (l == 4 | l == 5) {
                    gcmp(t);
                } else {
                    o(t);
                    if (n == '%')
                        o(0x92); /* xchg %edx, %eax */
                }
            }
        }
        /* && and || output code generation */
        if (a && l > 8) {
            a = gtst(t, a);
            li(t ^ 1);
            gjmp(5); /* jmp $ + 5 */
            gsym(a);
            li(t);
        }
    }
}

/*
 * expr - 解析完整表达式
 * 功能：解析完整的表达式，从最低优先级开始
 * 输入：无
 * 输出：无（结果在EAX寄存器中）
 * 状态变化：通过sum()函数改变代码生成状态
 * 主要逻辑：调用sum(11)从最低优先级开始解析表达式
 */
expr()
{
    sum(11);
}


/*
 * test_expr - 解析测试表达式
 * 功能：解析表达式并生成条件跳转，用于if/while等语句
 * 输入：无
 * 输出：返回条件跳转指令的地址（用于后续回填）
 * 状态变化：
 *   - 通过expr()生成表达式计算代码
 *   - 通过gtst()生成条件跳转指令
 * 主要逻辑：
 *   1. 调用expr()计算表达式值
 *   2. 调用gtst(0,0)生成je指令（表达式为假时跳转）
 */
test_expr()
{
    expr();
    return gtst(0, 0);
}

/*
 * block - 解析语句块
 * 功能：解析各种类型的语句（if、while、for、复合语句、表达式语句等）
 * 输入：l - 用于break语句的跳转链表指针
 * 输出：无
 * 状态变化：
 *   - 代码缓冲区添加语句对应的指令
 *   - 可能修改rsym（return语句跳转链）
 *   - 可能调用next()更新token状态
 * 主要逻辑：
 *   1. if语句：处理条件跳转和else分支
 *   2. while/for循环：处理循环条件和跳转
 *   3. 复合语句（{}）：处理局部声明和嵌套语句
 *   4. return语句：生成返回跳转
 *   5. break语句：添加到break跳转链
 *   6. 表达式语句：计算表达式值
 */
block(l)
{
    int a, n, t;

    if (tok == TOK_IF) {
        next();
        skip('(');
        a = test_expr();
        skip(')');
        block(l);
        if (tok == TOK_ELSE) {
            next();
            n = gjmp(0); /* jmp */
            gsym(a);
            block(l);
            gsym(n); /* patch else jmp */
        } else {
            gsym(a); /* patch if test */
        }
    } else if (tok == TOK_WHILE | tok == TOK_FOR) {
        t = tok;
        next();
        skip('(');
        if (t == TOK_WHILE) {
            n = ind;
            a = test_expr();
        } else {
            if (tok != ';')
                expr();
            skip(';');
            n = ind;
            a = 0;
            if (tok != ';')
                a = test_expr();
            skip(';');
            if (tok != ')') {
                t = gjmp(0);
                expr();
                gjmp(n - ind - 5);
                gsym(t);
                n = t + 4;
            }
        }
        skip(')');
        block(&a);
        gjmp(n - ind - 5); /* jmp */
        gsym(a);
    } else if (tok == '{') {
        next();
        /* declarations */
        decl(1);
        while(tok != '}')
            block(l);
        next();
    } else {
        if (tok == TOK_RETURN) {
            next();
            if (tok != ';')
                expr();
            rsym = gjmp(rsym); /* jmp */
        } else if (tok == TOK_BREAK) {
            next();
            *(int *)l = gjmp(*(int *)l);
        } else if (tok != ';')
            expr();
        skip(';');
    }
}

/*
 * decl - 解析声明（变量声明和函数定义）
 * 功能：解析变量声明和函数定义，分配内存空间
 * 输入：l - 是否为局部声明（1为局部，0为全局）
 * 输出：无
 * 状态变化：
 *   - loc: 局部变量时递增（分配栈空间）
 *   - glo: 全局变量时递增（分配全局空间）
 *   - 符号表更新变量地址信息
 *   - 代码缓冲区添加函数代码
 * 主要逻辑：
 *   1. 变量声明：分配内存空间并记录地址
 *   2. 函数定义：
 *      - 设置函数入口地址
 *      - 解析参数列表
 *      - 生成函数序言（push %ebp, mov %esp, %ebp）
 *      - 解析函数体
 *      - 生成函数尾声（leave, ret）
 *      - 回填局部变量空间大小
 */
decl(l)
{
    int a;

    while (tok == TOK_INT | tok != -1 & !l) {
        if (tok == TOK_INT) {
            next();
            while (tok != ';') {
                if (l) {
                    loc = loc + 4;
                    *(int *)tok = -loc;
                } else {
                    *(int *)tok = glo;
                    glo = glo + 4;
                }
                next();
                if (tok == ',') 
                    next();
            }
            skip(';');
        } else {
            /* patch forward references (XXX: do not work for function
               pointers) */
            gsym(*(int *)(tok + 4));
            /* put function address */
            *(int *)tok = ind;
            next();
            skip('(');
            a = 8;
            while (tok != ')') {
                /* read param name and compute offset */
                *(int *)tok = a;
                a = a + 4;
                next();
                if (tok == ',')
                    next();
            }
            next(); /* skip ')' */
            rsym = loc = 0;
            o(0xe58955); /* push   %ebp, mov %esp, %ebp */
            a = oad(0xec81, 0); /* sub $xxx, %esp */
            block(0);
            gsym(rsym);
            o(0xc3c9); /* leave, ret */
            *(int *)a = loc; /* save local variables */
        }
    }
}

/*
 * main - 编译器主函数
 * 功能：初始化编译器环境，执行编译过程，运行生成的代码
 * 输入：n - 命令行参数个数，t - 命令行参数数组
 * 输出：返回被编译程序的main函数执行结果
 * 状态变化：
 *   - 初始化所有全局变量和内存区域
 *   - 设置符号表（关键字）
 *   - 分配代码、数据、变量等缓冲区
 * 主要逻辑：
 *   1. 处理命令行参数，确定输入文件
 *   2. 初始化符号表，预设C语言关键字
 *   3. 分配内存缓冲区（代码、全局数据、变量表）
 *   4. 开始词法分析和语法分析
 *   5. 解析全局声明
 *   6. 执行生成的机器码（调用编译后的main函数）
 */
main(n, t)
{
    file = stdin;
    if (n-- > 1) {
        t = t + 4;
        file = fopen(*(int *)t, "r");
    }
    // Allocate symbol table and initialize keywords
    sym_stk = calloc(1, ALLOC_SIZE);
    strcpy(sym_stk, " int if else while break return for define main ");
    dstk = sym_stk + TOK_STR_SIZE;
    
    // Allocate global data space
    glo = calloc(1, ALLOC_SIZE);
    ind = prog = calloc(1, ALLOC_SIZE);
    vars = calloc(1, ALLOC_SIZE);
    inp();
    next();
    decl(0);
#ifdef TEST
    { 
        FILE *f;
        f = fopen(*(char **)(t + 4), "w");
        fwrite((void *)prog, 1, ind - prog, f);
        fclose(f);
        return 0;
    }
#else
    return (*(int (*)())*(int *)(vars + TOK_MAIN)) (n, t);
#endif
}
