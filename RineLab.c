#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include "lib/rbtree.h"

#define logPrint
#define FILE_INIT_CONTENT ""
#define REDUNDANT 5
#define LOG_SIZE 10000

static struct rb_root root = RB_ROOT;
static char * logStr;

static void logInfo(const char * s, const char * a) {
#ifdef logPrint
    strcat(logStr, "call [");
    strcat(logStr, s);
    strcat(logStr, "] for path: [");
    strcat(logStr, a);
    strcat(logStr, "]\n");
#endif
}

struct rinelab_entry {
    char * path;
    void * data;
    bool is_dir;
    struct rb_node node;
};

static struct rinelab_entry * new_entry(const char * path, const char * data, bool is_dir) {
    struct rinelab_entry * ret = (struct rinelab_entry *) malloc(sizeof(struct rinelab_entry));
    memset(ret, 0, sizeof(struct rinelab_entry));
    ret->path = strdup(path);
    if (data != NULL) ret->data = strdup(data);
    ret->is_dir = is_dir;
    return ret;
}

static void free_entry(struct rinelab_entry * pf) {
    if (pf->path != NULL) free(pf->path);
    if (pf->data != NULL) free(pf->data);
    free(pf);
}

// ----start---- RedBlackTree Interface

// macro container_of: see https://blog.csdn.net/s2603898260/article/details/79371024

// return whether insert successful
static bool rbt_insert(struct rb_root * root, struct rinelab_entry * pf) {
    struct rinelab_entry * cur = NULL;
    struct rb_node ** new_node = &(root->rb_node), * pa = NULL;
    while (*new_node != NULL) {
        cur = container_of(*new_node, struct rinelab_entry, node), pa = *new_node;
        int cmp_res = strcmp(pf->path, cur->path);
        if (cmp_res == 0) return false;
        new_node = cmp_res < 0 ? &((*new_node)->rb_left) : &((*new_node)->rb_right);
    }
    rb_link_node(&pf->node, pa, new_node);
    rb_insert_color(&pf->node, root);
    return true;
}

static struct rinelab_entry * rbt_find(struct rb_root * root, const char * path) {
    struct rinelab_entry * pf = NULL;
    struct rb_node * node = root->rb_node;
    while (node != NULL) {
        pf = container_of(node, struct rinelab_entry, node);
        int cmp_res = strcmp(path, pf->path);
        if (cmp_res == 0) return pf;
        node = cmp_res < 0 ? node->rb_left : node->rb_right;
    }
    return NULL;
}

// return whether erase successful
static bool rbt_erase(struct rb_root * root, const char * path) {
    struct rinelab_entry * pf = rbt_find(root, path);
    if (pf == NULL) return false;
    rb_erase(&pf->node, root);
    free_entry(pf);
    return true;
}

static void rbt_clear(struct rb_node * cur) {
    if (cur->rb_left != NULL) rbt_clear(cur->rb_left);
    if (cur->rb_right != NULL) rbt_clear(cur->rb_right);
    struct rinelab_entry * pf = container_of(cur, struct rinelab_entry, node);
    free_entry(pf);
}

// -----end----- RedBlackTree Interface

static struct options {
    const char *filename;
    const char *contents;
    int show_help;
} options;

#define OPTION(t, p)                           \
    { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
        OPTION("-h", show_help),
        OPTION("--help", show_help),
        FUSE_OPT_END
};

static void * rinelab_init(struct fuse_conn_info * conn, struct fuse_config * cfg) {
    (void) conn;
    cfg->kernel_cache = 0;
    return NULL;
}

static int rinelab_getattr(const char * path, struct stat * stbuf, struct fuse_file_info * fi) {
    (void) fi; // this line means fi is not used in the function and casting to void suppress compiler [unused variable] warning.
    logInfo("getattr", path);
    memset(stbuf, 0, sizeof(struct stat));
    struct rinelab_entry * pf = rbt_find(&root, path);
    if (pf == NULL) return -ENOENT;
    if (pf->is_dir) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = strlen(pf->data);
    }
    return 0;
}

static int rinelab_mkdir(const char * path, mode_t mode) {
    (void) mode;
    logInfo("mkdir", path);
    struct rinelab_entry * pf = new_entry(path, NULL, true);
    if (!rbt_insert(&root, pf)) return free_entry(pf), -EEXIST;
    return 0;
}

static int rinelab_rmdir(const char * path) {
    logInfo("rmdir", path);
    return rbt_erase(&root, path) ? 0 : -ENOENT;
}

static int rinelab_mknod(const char * path, mode_t mode, dev_t dev) {
    (void) mode, (void) dev;
    logInfo("mknod", path);
    struct rinelab_entry * pf = new_entry(path, FILE_INIT_CONTENT, false);
    if (!rbt_insert(&root, pf)) return free_entry(pf), -EEXIST;
    return 0;
}

static int rinelab_open(const char * path, struct fuse_file_info * fi) {
    logInfo("open", path);
    struct rinelab_entry * pf = rbt_find(&root, path);
    if (pf == NULL) {
        if ((fi->flags & O_ACCMODE) == O_RDONLY || !(fi->flags & O_CREAT)) return -EPERM;
        pf = new_entry(path, NULL, false);
        rbt_insert(&root, pf);
    } else {
        if (pf->is_dir) return -EISDIR;
    }
    fi->fh = (unsigned long) pf;
    return 0;
}

static int rinelab_release(const char * path, struct fuse_file_info * fi) {
    logInfo("release", path);
    return 0;
}

static int rinelab_unlink(const char * path) {
    logInfo("unlink", path);
    return rbt_erase(&root, path) ? 0 : -ENOENT;
}

static bool is_chat_dir(const char * path) {
    int cnt = 0;
    for (const char * cur = path; *cur != '\0'; cur++) if (*cur == '/') cnt++;
    return cnt == 2;
}

static char * reverse_chat_dir(const char * path) {
    char * ret = (char *) malloc(strlen(path) + 1);
    const char * pos;
    int cnt = 0;
    for (const char * cur = path; *cur != '\0'; cur++)
        if (*cur == '/') {
            if (cnt == 0) cnt++;
            else {
                pos = cur;
                break;
            }
        }
    int i = 0;
    for (int cur = pos - path; path[cur] != '\0'; cur++, i++) ret[i] = path[cur];
    ret[i++] = '/';
    for (int cur = 1; path[cur] != '/'; cur++, i++) ret[i] = path[cur];
    ret[i++] = '\0';
    return ret;
}

static int rinelab_write(const char * path, const char * buf, size_t size, off_t offset, struct fuse_file_info * fi) {
    logInfo("write", path);
    struct rinelab_entry * pf = rbt_find(&root, path);
    if (pf == NULL) return -ENOENT;
    assert(pf == (struct rinelab_entry *) fi->fh);
    if (strlen(pf->data) < offset + size) pf->data = realloc(pf->data, offset + size + REDUNDANT);
    memcpy(pf->data + offset, buf, size);
    char * rev;
    if (is_chat_dir(path) && rbt_find(&root, rev = reverse_chat_dir(path)) != NULL) {
        struct rinelab_entry * rpf = rbt_find(&root, rev);
        if (strlen(rpf->data) < offset + size) rpf->data = realloc(rpf->data, offset + size + REDUNDANT);
        memcpy(rpf->data + offset, buf, size);
    }
    free(rev);
    return size;
}

static int rinelab_read(const char * path, char * buf, size_t size, off_t offset, struct fuse_file_info * fi) {
    logInfo("read", path);
    struct rinelab_entry * pf = rbt_find(&root, path);
    if (pf == NULL) return -ENOENT;
    assert(pf == (struct rinelab_entry *) fi->fh);
    size_t file_size = strlen(pf->data);
    if (offset > file_size) return 0;
    size_t remain = file_size - offset;
    size_t real_size = size < remain ? size : remain;
    memcpy(buf, pf->data + offset, real_size);
    return real_size;
}

static inline const char * peel_off_parent(const char * parent, const char * path) {
    const char delim = '/';
    if (strlen(parent) == 1 && parent[0] == '/' && path[0] == '/') return path;
    while (*parent != '\0' && *path != '\0' && *parent == *path) parent++, path++;
    return *parent == '\0' && *path == delim ? path : NULL;
}

static int rinelab_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info * fi, enum fuse_readdir_flags flags) {
    (void) offset, (void) fi, (void) flags;
    logInfo("readdir", path);
    filler(buf, ".", NULL, 0, 0);
    if (strcmp(path, "/") != 0) filler(buf, "..", NULL, 0, 0);
    struct rinelab_entry * dir_entry = rbt_find(&root, path);
    if (dir_entry == NULL) return -ENOENT;
    if (!dir_entry->is_dir) return -ENOTDIR;
    struct rb_node * node = NULL;
    // all children's key is bigger than [path], therefore a forward traverse will collect all its children
    for (node = rb_next(&dir_entry->node); node != NULL; node = rb_next(node)) {
        struct rinelab_entry * pf = rb_entry(node, struct rinelab_entry, node);
        const char * after_peel = peel_off_parent(path, pf->path);
        // not a child of [path]
        if (after_peel == NULL) break;
        // not a direct child of [path], e.g. a grandchildren
        if (strchr(after_peel + 1, '/') != NULL) continue;
        filler(buf, after_peel + 1, NULL, 0, 0);
    }
    return 0;
}

static int rinelab_utimens(const char * path, const struct timespec tv[2], struct fuse_file_info * fi) {
    (void) tv, (void) fi;
    logInfo("utimens", path);
    return 0;
}

static void rinelab_destroy(void * private_data) {
    logInfo("destroy", "none");
    rbt_clear(root.rb_node);
}

static struct fuse_operations rinelab_oper = {
        .init       = rinelab_init,
        .getattr	= rinelab_getattr,
        .readdir	= rinelab_readdir,
        .open		= rinelab_open,
        .release    = rinelab_release,
        .read		= rinelab_read,
        .write      = rinelab_write,
        .mknod      = rinelab_mknod,
        .unlink     = rinelab_unlink,
        .mkdir      = rinelab_mkdir,
        .rmdir      = rinelab_rmdir,
        .utimens    = rinelab_utimens,
        .destroy    = rinelab_destroy,
};

static void show_help(const char *progname) {
    printf("usage: %s [options] <mountpoint>\n\n", progname);
    printf("File-system specific options:\n"
           "\n");
}

int main(int argc, char * argv[]) {
    int ret;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    
    if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1) return 1;
    
    if (options.show_help) {
        show_help(argv[0]);
        assert(fuse_opt_add_arg(&args, "--help") == 0);
        args.argv[0][0] = '\0';
    }
    
    // initialize root directory
    rbt_insert(&root, new_entry("/", NULL, true));

#ifdef logPrint
    // initalize log file
    struct rinelab_entry * pf = new_entry("/log", NULL, false);
    pf->data = logStr = (char *) malloc(LOG_SIZE);
    rbt_insert(&root, pf);
#endif
    
    ret = fuse_main(args.argc, args.argv, &rinelab_oper, NULL);
    fuse_opt_free_args(&args);
    
    return ret;
}
