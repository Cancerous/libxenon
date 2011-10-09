
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>



#ifndef WIN32
#include <diskio/diskio.h>
#include <newlib/vfs.h>
#include <newlib/dirent.h>
#else
#include "fakeio.h"
#endif
#include "xtaf.h"
#if 1
#include <diskio/ata.h>
#endif
//	http://www.free60.org/FATX
//	http://www.free60.org/XTAF
//	http://nds.cmamod.com/x360/test/MSFTMemoryUnit.7z
//	http://free60.git.sourceforge.net/git/gitweb.cgi?p=free60/libxenon;a=blob_plain;f=libxenon/drivers/fat/fat.c;h=ebdabb029fc6d9693eb196b620df697ad4c70d65;hb=refs/heads/ced2911

#define XBOX_1_COMPATIBILY_PARTITION 0x120eb0000
#define XBOX_1_COMPATIBILY_PARTITION_SIZE 0x10000000
#define XBOX_360_DEVKIT_PARTITION	0xA17D0000
#define MU1_PARTITION	0x7FF000
#define mu_size 243273728
#define XBOX_360_PARTITION 0x130eb0000 // HDD1 partition
#define XBOX_360_PARTITION_SIZE 0x1AC1AC6000//(0x0DF94BB0*0x200)-0x130eb0000

#include <debug.h>

#ifdef WIN32
#define LITTLE_ENDIAN
#else
#undef LITTLE_ENDIAN
#endif

// debug message
//#define XTAF_DEBUG 0

static inline uint16_t le16(const uint8_t *p) {
    return p[1] | (p[0] << 8);
}

static inline uint32_t le32(const uint8_t *p) {
    return p[3] | (p[2] << 8) | (p[1] << 16) | (p[0] << 24);
}

static inline uint16_t be16(const uint8_t *p) {
    return p[0] | (p[1] << 8);
}

static inline uint32_t be32(const uint8_t *p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static inline uint32_t bswap32(uint32_t t) {
    return ((t & 0xFF) << 24) | ((t & 0xFF00) << 8) | ((t & 0xFF0000) >> 8) | ((t & 0xFF000000) >> 24);
}

static inline uint16_t bswap16(uint16_t x) {
    return ((x << 8) & 0xff00) | ((x >> 8) & 0x00ff);
}

static inline uint32_t host2be32(uint32_t val) {
#ifdef LITTLE_ENDIAN
    return bswap32(val);
#else
    return val;
#endif
}

static inline uint16_t host2be16(uint16_t val) {
#ifdef LITTLE_ENDIAN
    return bswap32(val);
#else
    return val;
#endif
}

static inline uint32_t read32be(uint32_t val) {
#ifdef LITTLE_ENDIAN
    return bswap32(val);
#else
    return val;
#endif
}

static inline uint16_t read16be(uint16_t val) {
#ifdef LITTLE_ENDIAN
    return bswap16(val);
#else
    return val;
#endif
}

// for global use
static struct xtaf_context *pCtx = NULL;

static uint8_t raw_buf[RAW_BUF]; // __attribute__((aligned(32)));
static uint32_t current = -1;

// for error
static uint64_t last_read_sector = 0;

#ifdef WIN32

static FILE * fd = NULL;

static void XTAFError(char * format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    printf(buffer);
    va_end(args);

    printf("Last read sector : 0x%16x\r\n", last_read_sector);
    printf("Current position : 0x%16x\r\n", _ftelli64(fd));

    system("PAUSE");
}

static void _finit(const char *filename) {
    fd = fopen(filename, "rb");
    if (fd == NULL) {
        printf("error\n");
        exit(0);
    }
};

static int sata_read(struct xtaf_context * ctx, unsigned char * data, uint64_t sector, uint32_t len) {
    if (fd == NULL)
        //_finit("D:\\samba\\hdd_dump\\hdd.bin");
        _finit("z:\\hdd_360");
    //_finit("D:\\samba\\hdd_dump\\MSFTMemoryUnit.001");
    //_finit("c:\\rawprint\\MSFTMemoryUnit.001");
    uint64_t err = 0;


    _fseeki64(fd, sector*RAW_BUF, SEEK_SET);
    //fseek(fd,sector*RAW_BUF,SEEK_SET);
    //fseek(fd,0xFFFFFFFF,SEEK_CUR);
    err = _ftelli64(fd);
    fread(data, RAW_BUF, 1, fd);

    last_read_sector = sector;

#if 0// XTAF_DEBUG
    printf("Seek to : %016llx\r\n", sector * RAW_BUF);
#endif

    return 0;
}
#else

static void XTAFError(char * format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    printf(buffer);
    va_end(args);

    printf("Last read sector : 0x%08x\r\n", last_read_sector);
    exit(-1);
}

static int sata_read(struct xtaf_context * ctx, unsigned char * data, uint64_t sector, uint32_t len) {
    last_read_sector = sector;
    return ctx->dev->ops->read(ctx->dev, raw_buf, sector, 1) < 0 ? : 0;
}
#endif

static int raw_read(struct xtaf_context * ctx, uint64_t sector) {
    return sata_read(ctx, raw_buf, sector, 1);
}

/** read  **/
static int read(xtaf_partition_private *priv, uint8_t *data, uint64_t offset, uint32_t len) {
    offset += priv->partition_start_offset;

    while (len) {
        uint32_t buf_off = offset % RAW_BUF;
        uint32_t n;

        int err;
        if (!buf_off && !((data - raw_buf)&31) && len >= RAW_BUF) {
            n = len / RAW_BUF;
            if (n > MAX_SECTS)
                n = MAX_SECTS;
            /* aligned */
            //err = ctx->dev->ops->read(ctx->dev, data, offset / RAW_BUF, n);
            err = sata_read(pCtx, data, offset / RAW_BUF, n);
            if (err < n)
                return err;
            n *= RAW_BUF;
        } else {
            /* non-aligned */
            n = RAW_BUF - buf_off;
            if (n > len)
                n = len;
            err = raw_read(pCtx, offset / RAW_BUF);
            if (err)
                return err;

            memcpy(data, raw_buf + buf_off, n);
        }

        data += n;
        offset += n;
        len -= n;
    }

    return 0;
}

/** return the size of the file alocation table **/
static uint32_t getFatSize(xtaf_partition_private *priv) {
    struct _xtaf_partition_hdr_s * part_hdr = &priv->partition_hdr;

    uint64_t file_system_size = priv->file_system_size;
    uint32_t spc = part_hdr->sector_per_cluster; /* sectors per cluster */

    uint64_t numclusters = file_system_size / (512 * spc);
    uint8_t fatmult = numclusters >= 0xfff4 ? 4 : 2;
    uint64_t fatsize = numclusters * fatmult;

    if (fatsize % 4096 != 0)
        fatsize = (((fatsize / 4096) + 1) * 4096); /* round up if not multiple of 4 kB */

    return fatsize;
}

/** **/
static const char * parse_path(struct xtaf_context *ctx, const char * path) {
    int i = 0; // strlen((char*)ctx->fat_name+1);

    memset(ctx->fat_name, 0, 0x2A + 1);

    while (*path == '/')
        path++;

    while (*path && *path != '/') {
        ctx->fat_name[i++] = *path++;
    };
    return path;
};

/** parse a sector and get file/directory information  **/
static int xtaf_parse_entry(xtaf_partition_private * priv, struct _xtaf_directory_s * data) {
    if (priv->extent_offset == 0) {
        priv->extent_offset = priv->root_offset;
    }

    memset(data, 0, sizeof (struct _xtaf_directory_s));

    int read_err = read(priv, (unsigned char*) data, priv->extent_offset, sizeof (struct _xtaf_directory_s));

    if (read_err == 0) {

        priv->extent_offset += sizeof (struct _xtaf_directory_s);

        data->file_size = read32be(data->file_size);
        data->starting_cluster = read32be(data->starting_cluster);

        //end of table
        if (data->filename_size == 0xFF) {
            //XTAFError("xtaf_parse_entry : not an entry\r\n"); // not an error
            return 1;
        }
        return 0;
    }
    printf("xtaf_parse_entry : Read error ... %x\n", read_err);
    return -1;
}

/** build a date time **/
static inline uint32_t xtaf_build_time(uint16_t date, uint32_t time) {
    return 0;
}

/** get information for a file opened by xtaf_open **/
int xtaf_stats(xtaf_file_private *priv, xtaf_stat* st) {
    st->st_size = priv->finfo.file_size;
    st->st_mode = priv->finfo.flags;
    st->st_atime = xtaf_build_time(priv->finfo.access_date, priv->finfo.access_time);
    st->st_ctime = xtaf_build_time(priv->finfo.creation_date, priv->finfo.creation_time);
    st->st_mtime = xtaf_build_time(priv->finfo.update_date, priv->finfo.update_time);
    return 0;
};

/** get next cluster used by the file stream **/
static uint32_t xfat_get_next_cluster(xtaf_partition_private *priv, uint32_t cluster_id) {

    struct _xtaf_partition_hdr_s * part_hdr = &priv->partition_hdr;

    //se position a l'adresse de la fat
    uint64_t file_system_size = priv->file_system_size;
    uint32_t spc = part_hdr->sector_per_cluster; /* sectors per cluster */

    uint64_t numclusters = file_system_size / (512 * spc);
    uint8_t fatmult = numclusters >= 0xfff4 ? sizeof (uint32_t) : sizeof (uint16_t);

    uint32_t next;

    read(priv, (unsigned char*) &next, priv->fat_offset + (cluster_id * fatmult), fatmult);

    if (fatmult == sizeof (uint32_t)) {
        next = host2be32(next);
    } else {
        next = host2be16(next);
    }

#if 0 //XTAF_DEBUG
    printf("next cluster : %08x\n", next);
#endif

    return next;
};

/** read a file into a buffer **/
uint64_t xtaf_read_file(xtaf_file_private *fpriv, unsigned char * data, unsigned int len) {
    if (data == NULL) {

        return -1;
    }
    if (len == 0) {
        return -1;
    }

    //    printf("xtaf_read_file\r\n");

    // Read the files - 0x416140
    uint64_t cluster = (fpriv->finfo.starting_cluster);
    uint64_t copied = 0;

    xtaf_partition_private * parpriv = &pCtx->priv[fpriv->partition_number];

    uint64_t cluster_size = parpriv->clusters_size;

    uint64_t offset = ((cluster - 1) * cluster_size) + parpriv->root_offset + fpriv->pos;


    // Parse each dir entry
    while (len) {
        uint64_t n;

        n = ((cluster_size)<(len) ? (cluster_size) : (len));

        read(parpriv, data + copied, offset, n);

        // for next loop
        cluster = xfat_get_next_cluster(parpriv, cluster);
        offset = ((cluster - 1) * cluster_size) + parpriv->root_offset;

        len -= n;
        copied += n;

        // add
        fpriv->pos += n;
    }
#ifdef XTAF_DEBUG
    //        printf("xtaf_read_file : red %llx\r\n",copied);
#endif
    return copied;
};

#define XTAF_ENTRY_FAKE_ENTRY

// add hdd0 and hdd1
int xtaf_readir_fake_entry(xtaf_partition_private *priv, struct dirent * dir){
    return 0;
}

int xtaf_readdir(xtaf_partition_private *priv, struct dirent * dir) {

    // look for next dir based on the extent_offset
    int err = xtaf_parse_entry(priv, &pCtx->finfo);

    if(err==0){
        // found
#if XTAF_DEBUG
        printf(" xtaf_readdir - Found : %s\r\n", pCtx->finfo.filename);
#endif     
        //strncpy(dir->d_name,pCtx->finfo.filename,pCtx->finfo.filename_size);
        memset(dir->d_name,0,NAME_MAX);
        memcpy(dir->d_name,pCtx->finfo.filename,pCtx->finfo.filename_size);
        printf("type : %d\r\n",pCtx->finfo.file_size);
        dir->d_type = (pCtx->finfo.file_size == 0)?DT_DIR:DT_REG;
        dir->d_reclen = (pCtx->finfo.file_size == 0)?-1:pCtx->finfo.file_size;
        dir->d_namlen = pCtx->finfo.filename_size;
    }

    return err;
}

/** parse a xtaf partition for a specified file **/
int xtaf_opendir(xtaf_partition_private *priv, xtaf_file_private * file_private, const char *name) {
    //_xtaf_directory_s entry;
    memset(&pCtx->finfo, 0, sizeof (struct _xtaf_directory_s));
    memset(pCtx->fat_name, 0, 0x2A);
    memset(file_private, 0, sizeof (xtaf_file_private));

    priv->extent_offset = 0;
    priv->extent_next_cluster = 0;
    priv->extent_offset = 0;

    const char *tt = name;

    while (1) {
        tt = parse_path(pCtx, tt);
        if (tt[0] == 0) {
#ifdef XTAF_DEBUG
            printf("Dir Found : %s\r\n", name);
#endif
            return 0;
        }
        //printf("parse_path %s\r\n",tt);
        while (1) {
            // browse the fat
            int err = xtaf_parse_entry(priv, &pCtx->finfo);
            if (err == 1) {
#ifdef XTAF_DEBUG
                printf("xtaf_parse_entry failed\r\n");
#endif
                return -1;
            } else if (err == 0) {
#ifdef XTAF_DEBUG
                printf("found : %s\r\n", pCtx->finfo.filename);
                printf("%s - %s \r\n", (char*) pCtx->fat_name, (char*) pCtx->finfo.filename);
#endif
            } else {
#ifdef XTAF_DEBUG
                printf("error : %d\r\n", err);
#endif
                return -1;
            }

            // check if the wanted file/dir is the same as the found one
            if (strnicmp((char*) pCtx->fat_name, (char*) pCtx->finfo.filename, pCtx->finfo.filename_size) == 0) {
                if (pCtx->finfo.file_size == 0) {
                    uint64_t cluster_size = priv->clusters_size;

                    priv->extent_offset = ((pCtx->finfo.starting_cluster - 1) * cluster_size) + priv->root_offset;

                    break;
                } else {
#ifdef XTAF_DEBUG
                    printf("odd found a file ....\r\n");
#endif
                    return -1;
                }
            }
        }
    }
    printf("Can't open\n");
    //can't open
    return -1;
};

/** parse a xtaf partition for a specified file **/
int xtaf_open(xtaf_partition_private *priv, xtaf_file_private * file_private, const char *name) {
#ifdef XTAF_DEBUG
    printf("xtaf_open %s\r\n", name);
#endif
    //_xtaf_directory_s entry;
    memset(&pCtx->finfo, 0, sizeof (struct _xtaf_directory_s));
    memset(pCtx->fat_name, 0, 0x2A);

    // set it 0
    priv->extent_offset = 0;
    priv->extent_next_cluster = 0;
    priv->extent_offset = 0;

    const char *tt = name;

    while (1) {
        tt = parse_path(pCtx, tt);
        //printf("parse_path %s\r\n",tt);
        while (1) {
            // browse the fat
            int err = xtaf_parse_entry(priv, &pCtx->finfo);
            if (err == 1) {
#ifdef XTAF_DEBUG
                printf("xtaf_parse_entry failed\r\n");
#endif
                return -1;
            } else if (err == 0) {
#ifdef XTAF_DEBUG
                printf("found : %s\r\n", pCtx->finfo.filename);
                printf("%s - %s \r\n", (char*) pCtx->fat_name, (char*) pCtx->finfo.filename);
#endif
            } else {
#ifdef XTAF_DEBUG
                printf("error : %d\r\n", err);
#endif
                return -1;
            }

            // check if the wanted file/dir is the same as the found one
            if (strnicmp((char*) pCtx->fat_name, (char*) pCtx->finfo.filename, pCtx->finfo.filename_size) == 0) {
                if (pCtx->finfo.file_size == 0) {
                    uint64_t cluster_size = priv->clusters_size;

                    priv->extent_offset = ((pCtx->finfo.starting_cluster - 1) * cluster_size) + priv->root_offset;
#ifdef XTAF_DEBUG
                    printf("Dir Found : %s\r\n", (char*) pCtx->fat_name);
#endif
                    break;
                } else {
                    file_private->partition_number = priv->partition_number;
                    file_private->first_cluster = pCtx->finfo.starting_cluster;
                    file_private->pos = 0;
                    memcpy(&file_private->finfo, &pCtx->finfo, sizeof (struct _xtaf_directory_s));
#ifdef XTAF_DEBUG
                    printf("File found !!!\r\n");
#endif
                    return 0;
                }
            }
        }
    }
    printf("Can't open\n");
    //can't open
    return -1;
};

static int xtaf_init_fs(struct xtaf_partition_private *part_info) {
    struct _xtaf_partition_hdr_s * part_hdr = &part_info->partition_hdr;

    part_info->fat_file_size = getFatSize(part_info);
    part_info->clusters_size = part_hdr->sector_per_cluster * 0x200;
    part_info->fat_offset = 0x1000;
    part_info->root_offset = part_info->fat_offset + part_info->fat_file_size;

    part_info->extent_offset = 0;

#if	XTAF_DEBUG
    printf("fat_file_size       =	%08x\n", part_info->fat_file_size);
    printf("fat_file_size       =	%d Ko\n", (part_info->fat_file_size) / 1024);

    printf("\n");

    printf("_xtaf_partition_hdr_s\n");
    char magic[5];
    memcpy(magic, (const char*) part_hdr->magic, 4);
    magic[4] = 0;
    printf("magic               =	%s\n", magic);
    printf("id                  =	%08x\n", part_hdr->id);
    printf("sector_per_cluster  =	%08x\n", part_hdr->sector_per_cluster);

    printf("\n");

    printf("fat_offset          =	%08x\n", part_info->fat_offset + part_info->partition_start_offset);
    printf("root_cluster        =	%08x\n", part_hdr->root_cluster);
    printf("clusters_size       =	%08x\n", part_info->clusters_size);
    printf("root_offset         =	%16llx\n", part_info->root_offset + part_info->partition_start_offset);
#endif

    return 0;
}

/** return 0 if hdd is retail, 1 if is hdd is devkit **/
static int xtaf_check_hdd_type() {
    // read start of hdd
    if (raw_read(pCtx, 0)) {
        uint32_t * hdd_hdr = (uint32_t *) raw_buf;
        if (hdd_hdr[0] == 0) {
            return 0;
        }
        return 1;
    }
    //failed
    return -1;
}

typedef struct xtaf_partition_table {
    char name[8];
    uint64_t offset;
    uint64_t length;
} xtaf_partition_table;

xtaf_partition_table partition_table[] = {
    {"hdd0", 0x120eb0000, 0x10000000}, // Xbox 1 Backwards Compatibility
    {"hdd1", 0x130eb0000, 0}, // 360
    {"null", 0, 0}// End
};

/** xtaf_init init sata, and look for xtaf partition **/
int xtaf_init(struct xtaf_context *ctx, struct bdev *_dev) {
    if (pCtx == NULL) {
        pCtx = ctx;
    }
#ifndef WIN32
    //xenon_ata_init();
    //ctx->dev = bdev_open("sda");
#endif

    ctx->dev = _dev;
    if (_dev == NULL) {
        printf("Dev is nulll\r\n\r\n");
    }

#ifndef WIN32
    struct xenon_ata_device *dev = _dev->ctx;
    printf("Device %s size :%08x\n", _dev->name, dev->size);
#else
    struct xenon_ata_device *dev = 0;
#endif

    int err;
    int partition_nbr = 0;

    int is_devkit_hdd = xtaf_check_hdd_type();

    if (is_devkit_hdd == 1) {
        printf("Devkit hdd not supported\r\n");
        return -1;
    }

    int found = 0;

    // use only 1 parition for now ...
    while (1) {
        xtaf_partition_private * priv = &ctx->priv[partition_nbr];

        priv->partition_number = partition_nbr;

        priv->partition_start_offset = partition_table[partition_nbr].offset;

        // priv->partition_start_offset = XBOX_360_PARTITION;
        // priv->partition_start_offset = XBOX_360_DEVKIT_PARTITION;

        if (partition_table[partition_nbr].length == 0) {
#ifndef WIN32
            priv->file_system_size = ((uint64_t) dev->size * XENON_DISK_SECTOR_SIZE) - priv->partition_start_offset;
#else
            priv->file_system_size = 120034123776 - priv->partition_start_offset; // hdd_dump file size
#endif
        } else {
            priv->file_system_size = partition_table[partition_nbr].length;
        }

        // Read the header
        err = read(priv, (unsigned char*) &priv->partition_hdr, 0, sizeof (struct _xtaf_partition_hdr_s));

        if (err)
            return err;

        if (memcmp(priv->partition_hdr.magic, "XTAF", 4) == 0) {
            // Bswap partition header (for little endian cpu)
            priv->partition_hdr.id = host2be32(priv->partition_hdr.id);
            priv->partition_hdr.root_cluster = host2be32(priv->partition_hdr.root_cluster);
            priv->partition_hdr.sector_per_cluster = host2be32(priv->partition_hdr.sector_per_cluster);
            priv->partition_name = partition_table[partition_nbr].name;
            priv->found = 1;
            //return xtaf_init_fs(priv);
            xtaf_init_fs(priv);
            found++;
        }
        else{
            priv->found = 0;
        }

        partition_nbr++;
        if (partition_table[partition_nbr].offset == 0)
            break;
    }

    return found;
}

uint64_t xtaf_lseek_file(xtaf_file_private * fpriv, uint64_t offset, int whence) {
    uint64_t real_offset = 0;

    switch (whence) {
        case SEEK_SET:
            real_offset = offset;
            break;

        case SEEK_CUR:
            real_offset = fpriv->pos + offset;
            break;

        case SEEK_END:
            real_offset = fpriv->finfo.file_size + offset;
            break;

            // Unknow
        default:
            return -1;
    }

    // min/max ...
    if (real_offset < 0)
        real_offset = 0;

    if (real_offset > fpriv->finfo.file_size)
        real_offset = fpriv->finfo.file_size;

    fpriv->pos = real_offset;

    return fpriv->pos;
}


struct xtaf_dirent{
    struct dirent d;
    xtaf_file_private p;
    uint8_t busy;
};
// diskio interface
static xtaf_context xtafDiskIoCtx;
static xtaf_file_private openfiles[MAXDD];
static struct xtaf_dirent opendirs[MAXDD];

/** file ops **/
ssize_t _xtaf_read(struct vfs_file_s *file, void *dst, size_t len) {
    int fd = (int) file->priv[0];
    return xtaf_read_file(&openfiles[fd], dst, len);
};

off_t _xtaf_lseek(struct vfs_file_s *file, size_t offset, int whence) {
    int fd = (int) file->priv[0];
    return xtaf_lseek_file(&openfiles[fd], offset, whence);
};

void _xtaf_close(struct vfs_file_s *file) {
    TR;
    int fd = (int) file->priv[0];
    openfiles[fd].busy = 0;
};

int _xtaf_fstat(struct vfs_file_s *file, struct stat *buf) {
    int fd = (int) file->priv[0];
    xtaf_stat xst;
    if (xtaf_stats(&openfiles[fd], &xst) == 0) {
        buf->st_mode = xst.st_mode;
        buf->st_atime = xst.st_atime;
        buf->st_ctime = xst.st_ctime;
        buf->st_mtime = xst.st_mtime;
    }
    return 0;
};

/** dir ops **/
int _xtaf_closedir(struct vfs_dir_s *dirp){
    TR;
    int fd = (int) dirp->priv[0];
    opendirs[fd].busy = 0;
    memset(&opendirs[fd].p,0,sizeof(struct xtaf_dirent));
    return 0;
};


struct dirent*  _xtaf_fake_readdir(struct vfs_dir_s *dirp){
    // fake entry
    int fd = (int) dirp->priv[0];
    
    int s_entry = opendirs[fd].p.first_cluster;
    
    if (partition_table[s_entry].offset == 0) {
        return NULL;
    }

    // next entry
    opendirs[fd].p.first_cluster++;
    
    // copy entry
    opendirs[fd].d.d_type = DT_DIR;
    strcpy(opendirs[fd].d.d_name, partition_table[s_entry].name);
    opendirs[fd].d.d_namlen = 4;//hdd0 - hdd1
    
    TR;
    printf("%s\r\n",partition_table[s_entry].name);
    
    return &opendirs[fd].d;
}

struct dirent*  _xtaf_readdir(struct vfs_dir_s *dirp){
    int fd = (int) dirp->priv[0];
    
    printf("opendirs[fd].p.partition_number = %d\r\n",opendirs[fd].p.partition_number);
    
    if(opendirs[fd].p.partition_number==0xFF){
        TR;
        return _xtaf_fake_readdir(dirp);
    }
//    TR;
//    printf("fd:%d\r\n",fd);
//    printf("part:%d\r\n",opendirs[fd].p.partition_number);
    if( xtaf_readdir(&xtafDiskIoCtx.priv[opendirs[fd].p.partition_number], &opendirs[fd].d) == 0){
    //if( xtaf_readdir(&xtafDiskIoCtx.priv[1], &opendirs[fd].d) == 0){
        return &opendirs[fd].d;
    }
    
    return NULL;
};

/** mount ops **/
int _xtaf_open(struct vfs_file_s *file, struct mount_s *mount, const char *filename, int oflags, int perm) {
    TR;
    int selected_part;
    int i = 0;
    int fd = -1;
    // get the first non busy fd
    for (i = 0; i < MAXDD; i++) {
        if (openfiles[i].busy == 0) {
            fd = i;
            break;
        }
    }
    // select the good partition
    selected_part = 0;
    while (1) {
        if (partition_table[selected_part].offset == 0) {
            // partiton name not found ....
            printf("partiton name not found for %s\r\n", filename);
            return -1;

        }

        if (memcmp(filename + 1, partition_table[selected_part].name, 4) == 0) {
            break;
        }

        selected_part++;
    }

    char * xtaf_name = filename + 6; //6 "/hddX/"
    printf("xtaf_name %s\r\n", xtaf_name);
    if (fd >= 0) {
        memset(&openfiles[fd], 0, sizeof (xtaf_file_private));
        int err = xtaf_open(&xtafDiskIoCtx.priv[selected_part], &openfiles[fd], xtaf_name);
        if (err < 0) {


        } else {
            file->priv[0] = (void*) fd;
            file->ops = &vfs_xtaf_file_ops;
            openfiles[fd].busy = 1;
        }
    }
    return fd < 0;
};

int _xtaf_opendir(struct vfs_dir_s *dir, struct mount_s *mount, const char *dirname) {
    TR;
    int selected_part;
    int i = 0;
    int fd = -1;
    // get the first non busy fd
    for (i = 0; i < MAXDD; i++) {
        if (opendirs[i].busy == 0) {
            fd = i;
            break;
        }
    }
    
    if (fd < 0) {
        return -1;
    }
    
    // select the good partition
    selected_part = 0;
    while (1) {
        if (partition_table[selected_part].offset == 0) {
            // partiton name not found ....
            printf("partiton name not found for %s\r\n", dirname);
            //return -1;
            selected_part=0xFF;
            break;

        }

        if (memcmp(dirname + 1, partition_table[selected_part].name, 4) == 0) {
            break;
        }

        selected_part++;
    }
    
    if(selected_part!=0xFF){
        char * xtaf_name = dirname + 6; //6 "/hddX/"
        printf("xtaf_name %s\r\n", xtaf_name);
        if (fd >= 0) {
            memset(&opendirs[fd], 0, sizeof (struct xtaf_dirent));
            
            // is the partition found ?
            if(xtafDiskIoCtx.priv[selected_part].found){
                int err = xtaf_opendir(&xtafDiskIoCtx.priv[selected_part], &opendirs[fd].p, xtaf_name);
                if (err < 0) {
                        printf("xtaf_opendir error\r\n");

                } else {
                    printf("part:%d\r\n",selected_part);
                    printf("fd:%d\r\n",fd);
                    dir->priv[0] = (void*) fd;
                    dir->ops = &vfs_xtaf_dir_ops;
                    opendirs[fd].busy = 1;
                    opendirs[fd].p.partition_number = selected_part;
                }
            }
            else{
                return -1;
            }
            
        }
    }
    else{
        // fake entry
        printf("Fake entry\r\n");
        
        dir->priv[0] = (void*) fd;
        dir->ops = &vfs_xtaf_dir_ops;
        opendirs[fd].busy = 1;
        opendirs[fd].p.partition_number =0xFF;
    }

    
    return fd < 0;
};

void _xtaf_mount(struct mount_s *mount, struct bdev * device) {
    mount->priv[0] = (void*) device;
    int err = xtaf_init(&xtafDiskIoCtx, device);
    int i = 0;
    if (err == 0) {
        printf("_xtaf_mount failed...\r\n");
    }
    // set all busy flags to 0
    for (i = 0; i < MAXDD; i++) {
        openfiles[i].busy = 0;
    }
};

void _xtaf_umount(struct mount_s *mount) {
    // xtaf_shutdown(&xtafDiskIoCtx);
};

// diskio.c
#include <iso9660/iso9660.h>
#include <fat/fat.h>

// struct vfs_fileop_s vfs_iso9660_file_ops = {.read = _iso9660_read, .lseek = _iso9660_lseek, .fstat = _iso9660_fstat, .close = _iso9660_close};
struct vfs_dirop_s vfs_xtaf_dir_ops = 
{
    .readdir = _xtaf_readdir, 
    .closedir = _xtaf_closedir
};

struct vfs_fileop_s vfs_xtaf_file_ops = {
    .read = _xtaf_read,
    .lseek = _xtaf_lseek,
    .fstat = _xtaf_fstat,
    .close = _xtaf_close
};


struct vfs_mountop_s vfs_xtaf_mount_ops = {
    .open = _xtaf_open,
    .opendir = _xtaf_opendir,
    .mount = _xtaf_mount,
    .umount = _xtaf_umount
};

/** return 1 if hdd is xtaf **/
int hdd_is_xtaf(struct bdev *dev) {
    // josh sector
    dev->ops->read(dev, raw_buf, 4, 1);
    {
        // retail
        if (memcmp("Josh", raw_buf, 4) == 0) {
            return 1;
        }
    }

    // check for devkit hdd
    dev->ops->read(dev, raw_buf, 0, 1);
    {
        uint32_t * hdd_hdr = (uint32_t *) raw_buf;
        // devkit
        if (hdd_hdr[0] == 0x00020000) {
            return 1;
        }
    }

    return 0; // hope it's fat ...
}
