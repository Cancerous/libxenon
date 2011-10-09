#ifdef WIN32

typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

typedef long long int64_t;
typedef int int32_t;
typedef short int16_t;
typedef char int8_t;
#else
#include <xetypes.h>
#endif

#define MAX_PARTITION_PER_HDD 4

//Partition header

struct _xtaf_partition_hdr_s {
    unsigned char magic[4]; //32
    uint32_t id; //64
    uint32_t sector_per_cluster; //96
    uint32_t root_cluster; //128
};

struct _x_hdd_security_sector_s {
    unsigned char serial_number[0x14];
    unsigned char firmware_revision[0x8];
    unsigned char model_number[0x28];
    unsigned char ms_logo_hash[0x14];
    uint32_t number_of_sector;
    unsigned char signature[0x100];
    uint32_t ms_logo_size;
    void * ms_logo;
};

/**
The file flags and the date and time fields are in the same format as the one used in the FAT file system.
For time, bits 15-11 represent the hour, 10-5 the minutes, 4-0 the seconds.
For date, bits 15-9 are the year, 8-5 are the month, 4-0 are the day.
 **/
struct _xtaf_directory_s {
    unsigned char filename_size;
    unsigned char flags;
    unsigned char filename[0x2A]; //padded with either 0x00 or 0xFF bytes
    uint32_t starting_cluster; //0 for empty files
    uint32_t file_size; //0 for directories
    uint16_t creation_date;
    uint16_t creation_time;
    uint16_t access_date;
    uint16_t access_time;
    uint16_t update_date;
    uint16_t update_time;
};


// stat

typedef struct xtaf_stat {
    uint32_t st_mode; /* Protection */
    uint32_t st_size; /* Taille totale en octets */
    uint32_t st_atime; /* Heure dernier acc�s */
    uint32_t st_mtime; /* Heure derni�re modification */
    uint32_t st_ctime; /* Heure dernier changement �tat */
} xtaf_stat;

// per file information

typedef struct xtaf_file_private {
    /* which partition */
    uint8_t partition_number;

    /* first cluster */
    uint32_t first_cluster;

    /* position in stream */
    uint64_t pos;

    /* information from hdd */
    struct _xtaf_directory_s finfo;

    /* busy flags*/
    uint8_t busy;
} xtaf_file_private;

// per partition information

typedef struct xtaf_partition_private {
    /* which partition */
    uint8_t partition_number;

    /* information from hdd */
    struct _xtaf_partition_hdr_s partition_hdr;

    uint64_t partition_start_offset;

    uint32_t clusters_size;

    uint32_t bytes_per_cluster;
    uint32_t root_entries;
    uint32_t clusters;

    uint64_t fat_offset;
    uint64_t root_offset;
    uint64_t data_offset;
    uint32_t fat_file_size;

    uint64_t extent_offset;
    uint32_t extent_len;
    uint32_t extent_next_cluster;

    uint64_t file_system_size;
    
    char * partition_name;
    
    /**  **/
    uint8_t found;
} xtaf_partition_private;

// static in xtaf.c

typedef struct xtaf_context {
    /** xenon ata things **/
    struct bdev *dev;

    /** used by xtaf_open **/
    uint8_t fat_name[0x2A + 1];

    /** xx partition  **/
    xtaf_partition_private priv[MAX_PARTITION_PER_HDD];
    struct _xtaf_directory_s finfo;

} xtaf_context;

#define RAW_BUF 0x200
#define MAX_SECTS 8

/** xtaf_init init sata, and look for xtaf partition **/
int xtaf_init(struct xtaf_context *ctx, struct bdev *_dev);
/** free memory **/
int xtaf_shutdown(struct xtaf_context *ctx);

/** parse a xtaf partition for a specified file **/
int xtaf_open(xtaf_partition_private *priv, xtaf_file_private * file_private, const char *name);

/** get information for a file opened by xtaf_open **/
int xtaf_stats(xtaf_file_private*, xtaf_stat* st);
/** read a file into a buffer **/
uint64_t xtaf_read_file(xtaf_file_private *, unsigned char * data, unsigned int len);
/** stream func **/
uint64_t xtaf_lseek_file(xtaf_file_private *, uint64_t offset, int whence);

extern struct vfs_fileop_s vfs_xtaf_file_ops;
extern struct vfs_dirop_s vfs_xtaf_dir_ops;
extern struct vfs_mountop_s vfs_xtaf_mount_ops;

int hdd_is_xtaf(struct bdev *dev);