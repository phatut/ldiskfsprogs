/*
 * dirblock.c --- directory block routines.
 *
 * Copyright (C) 1995, 1996 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include <stdio.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <time.h>

#include "ext2_fs.h"
#include "ext2fs.h"

errcode_t ext2fs_read_dir_block3(ext2_filsys fs, blk64_t block,
				 void *buf, int flags EXT2FS_ATTR((unused)))
{
	errcode_t	retval;
	char		*p, *end;
	struct ext2_dir_entry *dirent;
	unsigned int	name_len, rec_len;


	retval = io_channel_read_blk64(fs->io, block, 1, buf);
	if (retval)
		return retval;

	p = (char *) buf;
	end = (char *) buf + fs->blocksize;
	while (p < end-8) {
		dirent = (struct ext2_dir_entry *) p;
#ifdef WORDS_BIGENDIAN
		dirent->inode = ext2fs_swab32(dirent->inode);
		dirent->rec_len = ext2fs_swab16(dirent->rec_len);
		dirent->name_len = ext2fs_swab16(dirent->name_len);
#endif
		name_len = dirent->name_len;
#ifdef WORDS_BIGENDIAN
		if (flags & EXT2_DIRBLOCK_V2_STRUCT)
			dirent->name_len = ext2fs_swab16(dirent->name_len);
#endif
		if ((retval = ext2fs_get_rec_len(fs, dirent, &rec_len)) != 0)
			return retval;
		if ((rec_len < 8) || (rec_len % 4)) {
			rec_len = 8;
			retval = EXT2_ET_DIR_CORRUPTED;
		} else if (((name_len & 0xFF) + 8) > rec_len)
			retval = EXT2_ET_DIR_CORRUPTED;
		p += rec_len;
	}
	return retval;
}

/*
 * Compute the total directory entry data length.
 * This includes the filename and an implicit NUL terminator (always present),
 * and optional extensions.  Each extension has a bit set in the high 4 bits of
 * de->file_type, and the extension length is the first byte in each entry.
 */
int ext2_get_dirent_size(struct ext2_dir_entry_2 *de)
{
	return ext2_get_dirent_dirdata_size(de, ~EXT2_FT_MASK);
}

int ext2_get_dirent_dirdata_size(struct ext2_dir_entry_2 *de, char dirdata_flags)
{
	char *len = de->name + de->name_len + 1 /* NUL terminator */;
	int dlen = 0;
	__u8 extra_data_flags = (de->file_type & ~EXT2_FT_MASK) >> 4;

	dirdata_flags >>= 4;
	while ((extra_data_flags & dirdata_flags) != 0) {
		if (extra_data_flags & 1) {
			if (dirdata_flags & 1)
				dlen += *len;

			len += *len;
		}
		extra_data_flags >>= 1;
		dirdata_flags >>= 1;
	}

	/* add NUL terminator byte to dirdata length */
	return dlen + (dlen != 0);
}

errcode_t ext2fs_read_dir_block2(ext2_filsys fs, blk_t block,
				 void *buf, int flags EXT2FS_ATTR((unused)))
{
	return ext2fs_read_dir_block3(fs, block, buf, flags);
}

errcode_t ext2fs_read_dir_block(ext2_filsys fs, blk_t block,
				 void *buf)
{
	return ext2fs_read_dir_block3(fs, block, buf, 0);
}


errcode_t ext2fs_write_dir_block3(ext2_filsys fs, blk64_t block,
				  void *inbuf, int flags EXT2FS_ATTR((unused)))
{
#ifdef WORDS_BIGENDIAN
	errcode_t	retval;
	char		*p, *end;
	char		*buf = 0;
	unsigned int	rec_len;
	struct ext2_dir_entry *dirent;

	retval = ext2fs_get_mem(fs->blocksize, &buf);
	if (retval)
		return retval;
	memcpy(buf, inbuf, fs->blocksize);
	p = buf;
	end = buf + fs->blocksize;
	while (p < end) {
		dirent = (struct ext2_dir_entry *) p;
		if ((retval = ext2fs_get_rec_len(fs, dirent, &rec_len)) != 0)
			return retval;
		if ((rec_len < 8) ||
		    (rec_len % 4)) {
			ext2fs_free_mem(&buf);
			return (EXT2_ET_DIR_CORRUPTED);
		}
		p += rec_len;
		dirent->inode = ext2fs_swab32(dirent->inode);
		dirent->rec_len = ext2fs_swab16(dirent->rec_len);
		dirent->name_len = ext2fs_swab16(dirent->name_len);

		if (flags & EXT2_DIRBLOCK_V2_STRUCT)
			dirent->name_len = ext2fs_swab16(dirent->name_len);
	}
	retval = io_channel_write_blk64(fs->io, block, 1, buf);
	ext2fs_free_mem(&buf);
	return retval;
#else
	return io_channel_write_blk64(fs->io, block, 1, (char *) inbuf);
#endif
}

errcode_t ext2fs_write_dir_block2(ext2_filsys fs, blk_t block,
				 void *inbuf, int flags EXT2FS_ATTR((unused)))
{
	return ext2fs_write_dir_block3(fs, block, inbuf, flags);
}

errcode_t ext2fs_write_dir_block(ext2_filsys fs, blk_t block,
				 void *inbuf)
{
	return ext2fs_write_dir_block3(fs, block, inbuf, 0);
}

