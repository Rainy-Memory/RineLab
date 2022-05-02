top: RineLab.c lib/rbtree.c
	gcc -Wall RineLab.c lib/rbtree.c `pkg-config fuse3 --cflags --libs` -o RineLab

run:
	mkdir root
	./RineLab root

init:
	mkdir a
	touch a/b
	mkdir b
	touch b/a

clean:
	rm RineLab
	umount root
	rm -rf root