# DIY 小作业 2: FUSE 初体验

## 目标：实现一个简单的用户态文件系统驱动

在 Linux 内核中，文件系统处于内核态之中。然而越来越多的低延迟新硬件使得特权级别切换所需要的开销占比越来越大，将部分驱动在用户态实现并且在用户态管理已经成为一种较多人的选择。你的任务是使用 Linux 给出的 Fuse 框架实现一个简单的用户态文件系统。由于更主要的精力是放在体验 Fuse 上，因此并不需要实现一个一般意义的文件系统。

## 例子：基于FUSE的聊天软件

在同一个电脑上挂载两个用户态文件系统（你写的代码，不妨认为是 `/mnt/bot1` 和 `/mnt/bot2`），执行 `echo "Hello" > /mnt/bot2/bot1` 之后应该在 bot1 中找到一个文件叫 bot2，并且其中内容是 `Hello`。

注意：项目并不一定要求基于 FUSE 的聊天软件，但至少应当包括一个和人的交互。

## 参考资料

1. https://www.kernel.org/doc/html/latest/filesystems/fuse.html
2. https://github.com/libfuse/libfuse
3. https://man7.org/linux/man-pages/man4/fuse.4.html