/*
  Copyright 2020 Harold Tay LGPLv3
  Filesystem functions.
 */
#ifndef FIL_H
#define FIL_H

#include <stdbool.h>

struct fil {
  uint32_t tail;                      /* Last cluster number */
  uint32_t seek_cluster;              /* current cluster number... */
  uint32_t seek_offset;               /* corresponding to this offset */
  uint32_t file_size;
  uint32_t head;                      /* First cluster number, may be 0 */
  uint32_t dirent_sector;             /* where the dirent lives */
  uint16_t dirent_offset;             /* where the dirent lives */
  uint16_t wtime;
  uint16_t wdate;
  uint8_t attributes;
};

/*
  On-disk structure of a dirent, and modification for lfn.
 */
struct dirent {
  uint8_t name[11];       /* 0 */
  uint8_t attr;           /* 11 */
  uint8_t resvd;          /* 12 */
  uint8_t time_10;        /* 13 */
  uint16_t ctime;         /* 14 */
  uint16_t cdate;         /* 16 */
  uint16_t adate;         /* 18 */
  uint16_t clust1hi;      /* 20 */
  uint16_t wtime;         /* 22 */
  uint16_t wdate;         /* 24 */
  uint16_t clust1lo;      /* 26 */
  uint32_t size;          /* 28 */
};

#define FIL_TIME_NONE 0
#define FIL_DATE_NONE ((1<<5) | 1)
#define FIL_TIME(h,m,s) (((h)<<11) | (((m)<<5) | (s/2)))
#define FIL_DATE(y,m,d) ((((y)-1980)<<9) | (((m)<<5) | d))

struct lfn_dirent {
  uint8_t ord;
  uint8_t name1[10];
  uint8_t attr;
  uint8_t type;
  uint8_t checksum;
  uint8_t name2[12];
  uint8_t clust1lo[2];
  uint8_t name3[4];
};

#define FIL_ATTR_FILE 0x00
#define FIL_ATTR_DIR  0x10
#define FIL_ATTR_LFN  0x0f

#define FIL_ENOENT           -71
#define FIL_ENOSPC           -72
#define FIL_ENOMEDIUM        -73
#define FIL_EEXIST           -74
#define FIL_EISDIR           -75
#define FIL_EBADNAME         -76
#define FIL_ESECTORSIZE      -77
#define FIL_ESECTORSPERCLUST -78
#define FIL_EBOOTSIG         -79
#define FIL_ENOTDIR          -80
#define FIL_ELOOP            -81
#define FIL_ECHAIN           -82
#define FIL_EEOF             -83
#define FIL_EEMPTY           -84
#define FIL_ESEEK            -85
#define FIL_ENRFATS          -86
#define FIL_EBADSEEK         -87      /* attempt to seek past EOF */
#define FIL_ERANGE           -88

typedef int8_t er_t;

extern er_t fil_sanitise(char fn[11]);

extern er_t fil_reinit(void);

extern er_t fil_init(void);

extern int8_t fil_approx_sd_used_pct(void);

extern uint32_t fil_sector_address(uint32_t cluster_number);

extern er_t fil_find_free_clusters(uint32_t kbytes, uint32_t * addr);

extern er_t fil_seek(struct fil * fp, uint32_t offset);

extern er_t fil_seek_next(struct fil * fp);

/*
  cmp() returns 0 if file found, <0 on error (abort scan), >0 to
  continue.
 */
extern er_t fil_scan_cwd(char fn[11],
  er_t (*cmp)(struct dirent *), struct fil * fp);

extern er_t fil_open(char fn[11], struct fil * fp);

extern er_t fil_save_dirent(struct fil * fp, char fn[11], bool sync);

extern er_t fil_append(struct fil * fp, uint8_t * buf, uint16_t len);

extern er_t fil_create(char fn[11], char lfn[26], struct fil * fp);

extern er_t fil_fchdir(struct fil * fp);

extern er_t fil_chdir(char fn[11]);

extern er_t fil_mkdir(char fn[11], char lfn[26], struct fil * fp);

extern er_t fil_scan_dbg(void);               /* debugging only */

#endif /* FIL_H */
