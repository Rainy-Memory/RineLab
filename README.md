# RineLab

RineLab, a simple line-like instant messager based on [FUSE](https://github.com/libfuse/libfuse).

[base.c](base.c) offered a simple RedBlackTree based file system, and [RineLab.c](RineLab.c) offered IM services.

### usage

```text
> echo "Hello!" >> root/a/b
> cat root/b/a
Hello!
```

### dependencies

Need [FUSE](https://github.com/libfuse/libfuse) installed.

Then run:

```shell
> make
> make run
> make init
```

