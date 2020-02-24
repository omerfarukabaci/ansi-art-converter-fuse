/*
 * Compile: gcc ansiart_to_png.c -o ansiart_to_png -Wall -ansi -W -std=c99 -g -ggdb -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -lfuse -lmagic -lansilove
 * Mount: ./ansiart_to_png -d <src_path> <dst_path>;
 */


#define FUSE_USE_VERSION 29

static const char* atp_version = "v0.1";

#include <sys/types.h> 
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/xattr.h>
#include <dirent.h>
#include <unistd.h>
#include <fuse.h>
#include <magic.h>
#include <ansilove.h>

char* rw_path;
char** converted_exts;
size_t converted_ext_count = 0;

static char* translate_path(const char* path)
{
    char *rPath = malloc(sizeof(char) * (strlen(path) + strlen(rw_path) + 1));
    char filename[255];
    char filename_with_extension[255];
    size_t i;
    int fd;

    strcpy(rPath, rw_path);
    if (rPath[strlen(rPath) - 1] == '/') {
        rPath[strlen(rPath) - 1] = '\0';
    }
    strcat(rPath, path);
	
	char *extension_ptr = strrchr(rPath, '.');
	
	if(extension_ptr){
		strncpy(filename, rPath, extension_ptr - rPath);
		filename[extension_ptr - rPath] = '\0';
		for(i = 0; i < converted_ext_count; i++){
			strcpy(filename_with_extension, filename);
			filename_with_extension[strlen(filename)] = '\0';
			strcat(filename_with_extension, converted_exts[i]);
			fd = open(filename_with_extension, O_RDONLY);
			if (fd != -1){
				strcpy(rPath, filename_with_extension);
			}
			close(fd);
		}
	}

    return rPath;
}

int is_text_or_directory(const char* path)
{
    const char *magic_full;
    magic_t magic_cookie;
    int res = 0;
	
    magic_cookie = magic_open(MAGIC_MIME_TYPE);
	
    if (magic_cookie == NULL) {
        fprintf(stderr, "Unable to initialize magic library\n");
        return -1;
    }
    
    if (magic_load(magic_cookie, NULL) != 0) {
        fprintf(stderr, "Cannot load magic database - %s\n", magic_error(magic_cookie));
        magic_close(magic_cookie);
        return -1;
    }
	
    magic_full = magic_file(magic_cookie, path);
    
    fprintf(stdout, "Path: %s Mime-type: %s\n", path, magic_full);
    
    if (strncmp(magic_full, "application/octet-stream", strlen("application/octet-stream")) == 0 || strncmp(magic_full, "text", strlen("text")) == 0)
		res = 1;
	
	if (strncmp(magic_full, "inode/directory", strlen("inode/directory")) == 0)
		res = 2;
    
    magic_close(magic_cookie);

    return res;
}

void convert_extension_to_png(char* dest, char* src){
	size_t extension_length, i;
	char *extension_str;
	char *extension_ptr = strrchr(src, '.');
	size_t file_name_length = extension_ptr ? (size_t)(extension_ptr - src) : strlen(src);
	
	
	extension_length = strlen(src) - file_name_length;
	extension_str = malloc(extension_length + 1);
	strncpy(extension_str, extension_ptr, extension_length);
	extension_str[extension_length] = '\0';
	
	if(!converted_exts){
		converted_exts = malloc(sizeof(char*));
		converted_exts[converted_ext_count] = malloc(extension_length + 1);
		strcpy(converted_exts[converted_ext_count], extension_str);
		converted_ext_count++;
	}
	else{
		for (i = 0; i < converted_ext_count; i++){
			if (strcmp(extension_str, converted_exts[i]) == 0){
				break;
			}
		}

		if (i == converted_ext_count){
			converted_exts[converted_ext_count] = malloc(extension_length + 1);
			strcpy(converted_exts[converted_ext_count], extension_str);
			converted_ext_count++;
		}
	}
	
	strncpy(dest, src, file_name_length);
	dest[file_name_length] = '\0';
	strcat(dest, ".png");
	fprintf(stdout, "Converted file name: %s\n", dest);
	
	free(extension_str);
	
	return;
}

int get_png_size(const char* path){
	int png_size = -1;
	
	if(!strstr(path, ".png")){
		return png_size;
	}

	struct ansilove_ctx ctx;
	struct ansilove_options options;
	char* upath = translate_path(path);

	ansilove_init(&ctx, &options);
	ansilove_loadfile(&ctx, upath);
	ansilove_ansi(&ctx, &options);
	png_size = ctx.png.length;
	ansilove_clean(&ctx);
	
	free(upath);
	return png_size;
}
/******************************
*
* Callbacks for FUSE
*
******************************/

static int atp_getattr(const char *path, struct stat *st_data)
{
    int res;
    int png_size;

	png_size = get_png_size(path);
    
    char *upath = translate_path(path); 

    res = lstat(upath, st_data);
    
    if (png_size != -1){
		st_data->st_size = png_size;
	}

    free(upath);

    if(res == -1) {
        return -errno;
    }

    return 0;
}

static int atp_readlink(const char *path, char *buf, size_t size)
{
    int res;
    char *upath=translate_path(path);
	
    res = readlink(upath, buf, size - 1);
    free(upath);
    if(res == -1) {
        return -errno;
    }
    buf[res] = '\0';
    
    return 0;
}

static int atp_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    DIR *dp;
    struct dirent *de;
    int res, text_or_directory;
    char* upath = NULL;

    (void) fi;
    (void) offset;

	upath = translate_path(path);
    
    dp = opendir(upath);
    
    
    if(dp == NULL) {
        res = -errno;
        free(upath);
        return res;
    }

    while((de = readdir(dp)) != NULL) {
		char file_path[255];
		char filename[255];
		
		struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        
        strcpy(file_path, upath);
        file_path[strlen(upath)] = '\0';
        
        if (file_path[strlen(file_path) - 1] != '/')
			strcat(file_path, "/");
			
		strcat(file_path, de->d_name);
			
		text_or_directory = is_text_or_directory(file_path);
		
        if (text_or_directory == 1){ // file_path is text
			convert_extension_to_png(filename, de->d_name);
				
			if (filler(buf, filename, &st, 0))
				break;
		}
		else if (text_or_directory == 2){// file_path is directory
			if (filler(buf, de->d_name, &st, 0))
				break;
		}
    }

	free(upath);
    closedir(dp);
    return 0;
}

static int atp_mknod(const char *path, mode_t mode, dev_t rdev)
{
    (void)path;
    (void)mode;
    (void)rdev;
    return -EROFS;
}

static int atp_mkdir(const char *path, mode_t mode)
{
    (void)path;
    (void)mode;
    return -EROFS;
}

static int atp_unlink(const char *path)
{
    (void)path;
    return -EROFS;
}

static int atp_rmdir(const char *path)
{
    (void)path;
    return -EROFS;
}

static int atp_symlink(const char *from, const char *to)
{
    (void)from;
    (void)to;
    return -EROFS;
}

static int atp_rename(const char *from, const char *to)
{
    (void)from;
    (void)to;
    return -EROFS;
}

static int atp_link(const char *from, const char *to)
{
    (void)from;
    (void)to;
    return -EROFS;
}

static int atp_chmod(const char *path, mode_t mode)
{
    (void)path;
    (void)mode;
    return -EROFS;

}

static int atp_chown(const char *path, uid_t uid, gid_t gid)
{
    (void)path;
    (void)uid;
    (void)gid;
    return -EROFS;
}

static int atp_truncate(const char *path, off_t size)
{
    (void)path;
    (void)size;
    return -EROFS;
}

static int atp_utime(const char *path, struct utimbuf *buf)
{
    (void)path;
    (void)buf;
    return -EROFS;
}

static int atp_open(const char *path, struct fuse_file_info *finfo)
{
    int res;
    int flags = finfo->flags;

    if ((flags & O_WRONLY) 
		|| (flags & O_RDWR)
		|| (flags & O_CREAT)
		|| (flags & O_EXCL)
		|| (flags & O_TRUNC)
		|| (flags & O_APPEND)) {
        return -EROFS;
    }
	
    char *upath=translate_path(path);
    res = open(upath, flags);
    free(upath);
    
    if(res == -1) {
        return -errno;
    }
    close(res);
    return 0;
}

static int atp_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *finfo)
{
    int res;
    (void)finfo;

    char *upath=translate_path(path);
    
    struct ansilove_ctx ctx;
    struct ansilove_options options;
    ansilove_init(&ctx, &options);
    ansilove_loadfile(&ctx, upath);
    ansilove_ansi(&ctx, &options);
    
    
    free(upath);
        
    int offset_size = ctx.png.length - offset;
    
    if (size > (size_t) offset_size)
        size = offset_size;
        
    memcpy(buf, ctx.png.buffer + offset, size);

    res = size;

    if(res == -1) {
        res = -errno;
    }

    ansilove_clean(&ctx);
    return res;
}

static int atp_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *finfo)
{
    (void)path;
    (void)buf;
    (void)size;
    (void)offset;
    (void)finfo;
    return -EROFS;
}

static int atp_statfs(const char *path, struct statvfs *st_buf)
{
    int res;
    char *upath=translate_path(path);

    res = statvfs(upath, st_buf);
    free(upath);
    if (res == -1) {
        return -errno;
    }
    return 0;
}

static int atp_release(const char *path, struct fuse_file_info *finfo)
{
    (void) path;
    (void) finfo;
    return 0;
}

static int atp_fsync(const char *path, int crap, struct fuse_file_info *finfo)
{
    (void) path;
    (void) crap;
    (void) finfo;
    return 0;
}

static int atp_access(const char *path, int mode)
{
    int res;
    char *upath=translate_path(path);

    if (mode & W_OK)
        return -EROFS;

    res = access(upath, mode);
    free(upath);
    if (res == -1) {
        return -errno;
    }
    return res;
}

static int atp_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    (void)path;
    (void)name;
    (void)value;
    (void)size;
    (void)flags;
    return -EROFS;
}

static int atp_getxattr(const char *path, const char *name, char *value, size_t size)
{
    int res;

    char *upath=translate_path(path);
    res = lgetxattr(upath, name, value, size);
    free(upath);
    if(res == -1) {
        return -errno;
    }
    return res;
}

static int atp_listxattr(const char *path, char *list, size_t size)
{
    int res;

    char *upath=translate_path(path);
    res = llistxattr(upath, list, size);
    free(upath);
    if(res == -1) {
        return -errno;
    }
    return res;

}

static int atp_removexattr(const char *path, const char *name)
{
    (void)path;
    (void)name;
    return -EROFS;

}

struct fuse_operations atp_oper = {
    .getattr     = atp_getattr,
    .readlink    = atp_readlink,
    .readdir     = atp_readdir,
    .mknod       = atp_mknod,
    .mkdir       = atp_mkdir,
    .symlink     = atp_symlink,
    .unlink      = atp_unlink,
    .rmdir       = atp_rmdir,
    .rename      = atp_rename,
    .link        = atp_link,
    .chmod       = atp_chmod,
    .chown       = atp_chown,
    .truncate    = atp_truncate,
    .utime       = atp_utime,
    .open        = atp_open,
    .read        = atp_read,
    .write       = atp_write,
    .statfs      = atp_statfs,
    .release     = atp_release,
    .fsync       = atp_fsync,
    .access      = atp_access,
    .setxattr    = atp_setxattr,
    .getxattr    = atp_getxattr,
    .listxattr   = atp_listxattr,
    .removexattr = atp_removexattr
};

enum {
    KEY_VERSION,
};

static int atp_parse_opt(void *data, const char *arg, int key,
                          struct fuse_args *outargs)
{
    (void) data;
    
    switch (key)
    {
    case FUSE_OPT_KEY_NONOPT:
        if (rw_path == NULL)
        {
            rw_path = strdup(arg);
            return 0;
        }
        else
        {
            return 1;
        }
    case FUSE_OPT_KEY_OPT:
        return 1;
    case KEY_VERSION:
        fprintf(stdout, "ATP version %s\n", atp_version);
        exit(0);
    default:
        fprintf(stderr, "See `%s -h' for usage\n", outargs->argv[0]);
        exit(1);
    }
    return 1;
}

static struct fuse_opt atp_opts[] = {
    FUSE_OPT_KEY("-V",          KEY_VERSION),
    FUSE_OPT_KEY("--version",   KEY_VERSION),
    FUSE_OPT_END
};

int main(int argc, char *argv[])
{
	size_t i;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    fuse_opt_parse(&args, NULL, atp_opts, atp_parse_opt);
    fuse_main(args.argc, args.argv, &atp_oper, NULL);
    
    for(i = 0; i < converted_ext_count; i++)
		free(converted_exts[i]);

	free(converted_exts);
	
    return 0;
}
