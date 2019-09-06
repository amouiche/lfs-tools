/*
 * Copyright (C) 2015 - 2018, IBEROXARXA SERVICIOS INTEGRALES, S.L.
 * Copyright (C) 2015 - 2018, Jaume Olivé Petrus (jolive@whitecatboard.org)
 * Copyright (c) 2019, Arnaud MOuiche
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *     * The WHITECAT logotype cannot be changed, you can remove it, but you
 *       cannot change it in any way. The WHITECAT logotype is:
 *
 *          /\       /\
 *         /  \_____/  \
 *        /_____________\
 *        W H I T E C A T
 *
 *     * Redistributions in binary form must retain all copyright notices printed
 *       to any local or remote output device. This include any reference to
 *       Lua RTOS, whitecatboard.org, Lua, and other copyright notices that may
 *       appear in the future.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Lua RTOS, a tool for make a LFS file system image
 *
 */

#define _GNU_SOURCE
#include "lfs/lfs.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

static struct lfs_config cfg;
static lfs_t lfs;
static uint8_t *data;

static int lfs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
    memcpy(buffer, data + (block * c->block_size) + off, size);
    return 0;
}

static int lfs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {
	memcpy(data + (block * c->block_size) + off, buffer, size);
    return 0;
}

static int lfs_erase(const struct lfs_config *c, lfs_block_t block) {
    memset(data + (block * c->block_size), 0, c->block_size);
    return 0;
}

static int lfs_sync(const struct lfs_config *c) {
	return 0;
}



static int dump_file(const char *srcpath, const char  *dstpath) {
	int r;
	lfs_file_t lfs_f;
	FILE *F = NULL;
	uint8_t buffer[4096];


	r = lfs_file_open(&lfs, &lfs_f, srcpath, LFS_O_RDONLY);
	if (r < 0) {
		fprintf(stderr, "lfs: fail to open %s\n", srcpath);
		return -1;
	}

	F = fopen(dstpath, "wb");
	if (!F) {
		fprintf(stderr, "host: fail to open: %s. errno=%d (%s)\r\n",
				dstpath, errno, strerror(errno));
		r = -1;
		goto dump_file_err;
	}

	do {
		r = lfs_file_read(&lfs, &lfs_f, buffer, sizeof(buffer));
		if (r < 0) {
			fprintf(stderr, "lfs: read failure: %s\n", srcpath);
			break;
		} else if (r == 0) {
			break;
		}
		if (fwrite(buffer, r, 1, F) != 1) {
			fprintf(stderr, "host: write failure: %s\n", dstpath);
			r = -1;
			break;
		}
	} while (1);

dump_file_err:
	if (F) fclose(F);
	lfs_file_close(&lfs, &lfs_f);
	return r;
}


static int dump_dir(const char *srcpath, const char  *dstpath) {
	lfs_dir_t srcdir;
	struct lfs_info dir_info;
	int r;
	int status = 0;

	r = lfs_dir_open(&lfs, &srcdir, srcpath);
	if (r < 0) {
		fprintf(stderr, "lfs: Failed to open dir %s\n", srcpath);
		return -1;
	}

	do {
		r = lfs_dir_read(&lfs, &srcdir, &dir_info);
		if (r <= 0) break;
		if (!strcmp(dir_info.name, ".") || !strcmp(dir_info.name, "..")) {
			continue;
		}

		char *new_srcpath = NULL;
		char *new_dstpath = NULL;
		char t;

		switch (dir_info.type) {
		case LFS_TYPE_DIR:
			t = 'D';
		    break;
		case LFS_TYPE_REG:
			t = 'F';
			break;
		default:
			fprintf(stderr, "lfs: %s/%s: unsupported LFS_TYPE 0x%02x\n", srcpath, dir_info.name, dir_info.type);
			continue;
		}

		if (asprintf(&new_srcpath, "%s/%s", srcpath, dir_info.name) < 0) goto alloc_err;
		if (asprintf(&new_dstpath, "%s/%s", dstpath, dir_info.name) < 0) goto alloc_err;
		printf("%c: %s > %s\n",t, new_srcpath, new_dstpath);

		switch (dir_info.type) {
		case LFS_TYPE_DIR:
			mkdir(new_dstpath, 0777 );
			status = dump_dir(new_srcpath, new_dstpath);
		    break;
		case LFS_TYPE_REG:
			status = dump_file(new_srcpath, new_dstpath);
			break;
		}

alloc_err:
		free(new_srcpath);
		free(new_dstpath);

	} while (!status);


	lfs_dir_close(&lfs, &srcdir);
	return status;
}


void usage() {
	fprintf(stdout, "usage: dumplfs -b <block-size> -r <read-size> -p <prog-size> -i <image-file-path> -o <output-dir>\r\n");
}

static int is_number(const char *s) {
	const char *c = s;

	while (*c) {
		if ((*c < '0') || (*c > '9')) {
			return 0;
		}
		c++;
	}

	return 1;
}

static int is_hex(const char *s) {
	const char *c = s;

	if (*c++ != '0') {
		return 0;
	}

	if (*c++ != 'x') {
		return 0;
	}

	while (*c) {
		if (((*c < '0') || (*c > '9')) && ((*c < 'A') || (*c > 'F')) && ((*c < 'a') || (*c > 'f'))) {
			return 0;
		}
		c++;
	}

	return 1;
}

static int to_int(const char *s) {
	if (is_number(s)) {
		return atoi(s);
	} else if (is_hex(s)) {
		return (int)strtol(s, NULL, 16);
	}

	return -1;
}

int main(int argc, char **argv) {
    char *dstdir = NULL;   // Destination directory
    char *src = NULL;   // Source image
    int c;              // Current option
    int block_size = 0; // Block size
    int read_size = 0;  // Read size
    int prog_size = 0;  // Prog size
    int fs_size = 0;    // File system size
    int err;

	while ((c = getopt(argc, argv, "o:i:b:p:r:")) != -1) {
		switch (c) {
		case 'o':
			dstdir = optarg;
			break;

		case 'i':
			src = optarg;
			break;

		case 'b':
			block_size = to_int(optarg);
			break;

		case 'p':
			prog_size = to_int(optarg);
			break;

		case 'r':
			read_size = to_int(optarg);
			break;
		}
	}

    if ((src == NULL) || (dstdir == NULL) || (block_size <= 0) || (prog_size <= 0) ||
        (read_size <= 0)) {
    		usage();
        exit(1);
    }

    // Mount the file system
    cfg.read  = lfs_read;
    cfg.prog  = lfs_prog;
    cfg.erase = lfs_erase;
    cfg.sync  = lfs_sync;

    
    cfg.context     = NULL;

    /* read the data from src */
    FILE *img = fopen(src, "rb");
    if (!img) {
    	fprintf(stderr, "can't open image file: errno=%d (%s)\r\n", errno, strerror(errno));
		return -1;
	}
    /* find the size of the filesystem */
    fseek(img, 0, SEEK_END);
    fs_size = ftell(img);
    if (fs_size < 0) {
    	fprintf(stderr, "can't read the image file file: errno=%d (%s)\r\n", errno, strerror(errno));
		return -1;
    }
    fseek(img, 0, SEEK_SET);

    if (fs_size % block_size) {
    	fprintf(stderr, "image size is not aligned to block_size\r\n");
    	return -1;
    }

    cfg.block_size  = block_size;
    cfg.read_size   = read_size;
    cfg.prog_size   = prog_size;
    cfg.block_count = fs_size / cfg.block_size;
    cfg.lookahead   = 128;

	data = malloc(fs_size);
	if (!data) {
		fprintf(stderr, "no memory for mount\r\n");
		return -1;
	}

	if (fread(data, fs_size, 1, img) != 1) {
		fprintf(stderr, "Fail to read the image file\r\n");
		return -1;
	}
	fclose(img);

	err = lfs_mount(&lfs, &cfg);
	if (err < 0) {
		fprintf(stderr, "mount error: error=%d\r\n", err);
		return -1;
	}

	err = dump_dir("", dstdir);

#if 0
	compact(src);

	FILE *img = fopen(dst, "wb+");

	if (!img) {
		fprintf(stderr, "can't create image file: errno=%d (%s)\r\n", errno, strerror(errno));
		return -1;
	}

	fwrite(data, 1, fs_size, img);
#endif

	return 0;
}
