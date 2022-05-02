top: RineLab.c lib/rbtree.c
	gcc -Wall RineLab.c lib/rbtree.c `pkg-config fuse3 --cflags --libs` -o RineLab

run:
	mkdir root
	./RineLab root

init:
	mkdir root/a
	touch root/a/b
	mkdir root/b
	touch root/b/a

clean:
	rm RineLab
	umount root
	rm -rf root