/*
  Copyright 2020 Harold Tay LGPLv3
  Integrate FAT code with file access code, because reasons.
  Assumes CPU is little endian.
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "sd2.h"
#include "fil.h"

#define DBG_FIL
#ifdef DBG_FIL
#include "tx.h"
#define dbg(x) x
void hexdump(uint8_t * p, uint8_t len)
{
  uint8_t i;
  for (i = 0; ; i++) {
    if (i && !(i & 15)) tx_puts("\r\n");
    else tx_putc(' ');
    if (i >= len) break;
    tx_puthex(p[i]);
  }
}
void dbg_dump_fat_sector(void)
{
  uint32_t * p;
  uint8_t i;

  p = (uint32_t *)sd_buffer;

  for (i = 0; i < 128; i++) {
    tx_puthex32(p[i]);
    if (!(i % 16)) tx_puts("\r\n");
    else tx_putc(' ');
  }
}
static void dbg_print32(char * s, uint32_t x)
{
  tx_puts(s);
  tx_putc('=');
  tx_puthex32(x);
  tx_puts("\r\n");
}
static void dbg_fil_print(struct fil * fp, char * fn)
{
  tx_puts(fn);
  tx_puts(":\r\n");
  dbg_print32("\ttail", fp->tail);
  dbg_print32("\tseek_cluster", fp->seek_cluster);
  dbg_print32("\tseek_offset", fp->seek_offset);
  dbg_print32("\thead", fp->head);
  dbg_print32("\tdirent_sector", fp->dirent_sector);
  dbg_print32("\tdirent_offset", fp->dirent_offset);
}
#else
#define dbg(x) /* nothing */
#define hexdump(a, b) /* nothing */
#define dbg_print32(a, b)
#define dbg_fil_print(a, b)
#endif

#define ASSERT(cond) /* nothing */

static void dump_sector(void)
{
  int8_t row, col, i;
  for (row = i = 0; row < 16; row++) {
    for (col = 0; col < 32; col++) {
      tx_puthex(sd_buffer[i++]);
      tx_putc(' ');
    }
    tx_puts("\r\n");
  }
}

/* XXX needs to be more complete */
er_t fil_sanitise(char fn[11])
{
  int8_t i;
  for (i = 0; i < 11; i++)
    fn[i] = toupper(fn[i]);
  return(0);
}

er_t fil_reinit(void)
{
  return(sd_init());
}

static er_t checksig(void)
{
  if (sd_buffer[510] != 0x55) return(-1);
  if (sd_buffer[511] != 0xaa) return(-1);
  return(0);
}

static er_t fail(er_t er)
{
  dump_sector();
  return(er);
}

static uint8_t fil_sectors_per_cluster;
static uint32_t fil_bytes_per_cluster;   /* May not exceed 32k */
static uint32_t fil_sectors_per_fat;
static uint32_t fil_last_cluster_number;
static uint8_t fil_nr_fats;
static uint32_t fil_root_dir_1st_cluster;
static uint32_t fil_cwd_1st_cluster;
static uint32_t fil_clusters_start;
static uint32_t fil_fat_start;
static struct fil cwd;

#define CLUSTER_MASK 0x0fffffff
#define CHAIN_END 0x0ffffff8
#define IS_EOC(cluster) ((cluster&CLUSTER_MASK) >= CHAIN_END)

static er_t fat_er;
static uint32_t * fat(uint32_t cluster)
{
  uint32_t * p;

  cluster &= CLUSTER_MASK;
  /*
    Check if within FAT limits
   */
  if (cluster > fil_last_cluster_number) {
    fat_er = FIL_ERANGE;
    return(0);
  }
  fat_er = sd_buffer_checkout(fil_fat_start + cluster/128);
  if (fat_er) return(0);
  p = (uint32_t *)sd_buffer;
  return(p + (cluster % 128));
}

static er_t cwd_init(uint32_t head);

er_t fil_init(void)
{
  uint16_t us;
  er_t er;
  uint32_t dev_start, dev_nr_sectors;
  uint16_t nr_reserved_sectors;

  dbg(tx_puts("fil_init:calling fil_reinit()...\r\n"));
  er = fil_reinit();
  if (er) return(er);

  dbg(tx_puts("fil_init:calling sd_bread(0)...\r\n"));
  er = sd_bread(0);
  if (er) return(er);

  dbg(tx_puts("fil_init:calling checksig()...\r\n"));
  if (checksig()) {
    dbg(tx_puts("Bad signature\r\n"));
    dbg(dump_sector());
    return(fail(FIL_ENOMEDIUM));
  }

#define COPY4(dst, src) memcpy((void *)(&(dst)), (void *)(src), 4)
#define COPY2(dst, src) memcpy((void *)(&(dst)), (void *)(src), 2)

  COPY4(dev_start, sd_buffer + 446 + 8);
  COPY4(dev_nr_sectors, sd_buffer + 446 + 12);

  er = sd_bread(dev_start);
  if (er) return(er);

  if (checksig()) return(fail(FIL_EBOOTSIG));

  COPY2(us, sd_buffer + 0x0b);
  if (us != 512) return(fail(FIL_ESECTORSIZE));;

  fil_sectors_per_cluster = sd_buffer[0x0d];
  fil_bytes_per_cluster = fil_sectors_per_cluster * 512;
  er = FIL_ESECTORSPERCLUST;
  if (fil_sectors_per_cluster > 64) return(fail(FIL_ESECTORSPERCLUST));

  COPY2(nr_reserved_sectors, sd_buffer + 0x0e);

  fil_nr_fats = sd_buffer[0x10];
  if (fil_nr_fats != 2) return(fail(FIL_ENRFATS));

  COPY4(fil_sectors_per_fat, sd_buffer + 0x24);
  COPY4(fil_root_dir_1st_cluster, sd_buffer + 0x2c);
  fil_cwd_1st_cluster = fil_root_dir_1st_cluster;
  dbg_print32("root_dir_1st_cluster", fil_root_dir_1st_cluster);

  fil_fat_start = dev_start + nr_reserved_sectors;
  fil_clusters_start = fil_fat_start +
    (fil_nr_fats * fil_sectors_per_fat);
  fil_last_cluster_number = (fil_sectors_per_fat * 128) - 1;
  dbg(tx_msg("bpb[0x25] = ", sd_buffer[0x25]));
  dbg(tx_msg("bpb[0x41] = ", sd_buffer[0x41]));

#ifdef DBG_FIL
  {
    uint32_t * p;
    p = fat(0);
    if (!p) tx_msg("fil_init:error getting fat[0]:", fat_er);
    else dbg_print32("fil_init:fat[0]", *p);
    p = fat(1);
    if (!p) tx_msg("fil_init:error getting fat[1]:", fat_er);
    else dbg_print32("fil_init:fat[1]", *p);
  }
#endif

  return(cwd_init(fil_root_dir_1st_cluster));
}

static uint32_t fat_cdr(uint32_t cluster)
{
  uint32_t * p;
  p = fat(cluster & CLUSTER_MASK);
  if (!p) return(0);
  return(*p);
}

static uint32_t free_cluster_hint;
static uint32_t fat_find_free_cluster(void)
{
  uint32_t cluster;

  if (!free_cluster_hint)
    free_cluster_hint = 2;

  for ( ; ; free_cluster_hint++) {
    uint32_t * p;
    if (free_cluster_hint > fil_last_cluster_number) {
      fat_er = FIL_ENOSPC;
      return(0);
    }
    p = fat(free_cluster_hint);
    if (!p) return(0);
    if (*p) continue;
    *p = CHAIN_END;
    break;
  }
  cluster = free_cluster_hint;
  free_cluster_hint++;
  return(cluster);
}

int8_t fil_approx_sd_used_pct(void)
{
  return(fil_last_cluster_number?
    (100UL*free_cluster_hint)/fil_last_cluster_number:0);
}

uint32_t fil_sector_address(uint32_t cluster_number)
{
  return(fil_clusters_start +
    (cluster_number - 2)*fil_sectors_per_cluster);
}

/*
  Clusters will be contiguous.
 */

er_t fil_find_free_clusters(uint32_t kbytes, uint32_t * clustp)
{
  uint32_t nr, nr_clusters, cluster;
  uint16_t kbytes_per_cluster;
  uint32_t * p;

  {
    ldiv_t d;
    kbytes_per_cluster = fil_sectors_per_cluster / 2;
    d = ldiv(kbytes, kbytes_per_cluster);
    nr_clusters = d.quot;
    if (d.rem) nr_clusters++;
  }

  dbg(tx_msg("fil_find_free_clusters: nr_clusters = ", nr_clusters));

  if (!free_cluster_hint)
    free_cluster_hint = 2;

  cluster = 0;                        /* Shut up compiler */
  for (nr = 0; ; free_cluster_hint++) {
    if (free_cluster_hint >= fil_last_cluster_number)
      return(FIL_ENOSPC);
    p = fat(free_cluster_hint);
    if (!p) return(fat_er);
    if (*p) {                         /* cluster in use */
      nr = 0;
    } else {                          /* free cluster */
      if (!nr) cluster = free_cluster_hint;
      nr++;
      if (nr == nr_clusters) break;
    }
  }

  *clustp = cluster;                  /* will be start of file */

  /* Chain clusters together */
  for (nr = 0; nr < nr_clusters-1; nr++) {
    p = fat(cluster);
    cluster++;
    *p = cluster;
  }
  dbg_print32("fil_find_free_clusters:last cluster", cluster);
  p = fat(cluster);
  *p = CHAIN_END;
  sd_buffer_sync();

#if 0 /* DBG_FIL */ /* too much output, and slow XXX */
  {
    uint32_t i;
    cluster = *clustp;
    dbg_print32("fat sector address", fil_sector_address(cluster));
    tx_msg("at index ", cluster % 128);
    for (i = 0; i < nr_clusters; i++) {
      p = fat(cluster);
      tx_puthex32(cluster);
      tx_puts(" -> ");
      tx_puthex32(*p);
      cluster++;
      tx_puts("\r\n");
    }
    tx_puts("\r\n");
  }
#endif

  return(0);
}

static uint32_t fat_chain_grow(uint32_t tail)
{
  uint32_t * p;
  uint32_t cluster;

  cluster = fat_find_free_cluster();
  if (!cluster) return(0);
  p = fat(cluster);
  if (!p) return(0);
  *p = CHAIN_END;

  if (tail) {
    p = fat(tail);
    if (!p) return(0);
    *p = cluster;
  }
  return(cluster);
}

static er_t cwd_init(uint32_t head)
{
  uint8_t i;

  if (!head)
    head = fil_root_dir_1st_cluster;

  memset(&cwd, 0, sizeof(cwd));
  cwd.tail = cwd.head = head;
  cwd.file_size = fil_bytes_per_cluster;

  for (i = 0; ; i++) {
    uint32_t next;
    if (i > 100) return(FIL_ELOOP);
    dbg_print32("cwd_init:current cluster", cwd.tail);
    next = fat_cdr(cwd.tail);
    if (!next) return(fat_er);
    if (IS_EOC(next)) break;
    cwd.tail = next;
    cwd.file_size += fil_bytes_per_cluster;
  }
  return(0);
}

/* Can only seek to whole sector boundary */
static uint32_t seek_sector;
er_t fil_seek(struct fil * fp, uint32_t offset)
{
  offset &= (~511UL);

  if (offset > fp->file_size) return(FIL_ESEEK);
  if (!fp->head) return(FIL_ESEEK);

  if (!fp->seek_cluster || offset < fp->seek_offset) {
    fp->seek_cluster = fp->head;
    fp->seek_offset = 0;
  }

#define CLUSTER_BDRY(v) (v & ~(fil_bytes_per_cluster-1))
  while (CLUSTER_BDRY(fp->seek_offset) < CLUSTER_BDRY(offset)) {
    uint32_t next;
    next = fat_cdr(fp->seek_cluster);
    if (IS_EOC(next)) return(FIL_ECHAIN);
    if (!next) {
      dbg_print32("Chain ends in 0, not CHAIN_END, fp->seek_cluster",
        fp->seek_cluster);
      return(FIL_ECHAIN);
    }
    fp->seek_cluster = next;
    fp->seek_offset += fil_bytes_per_cluster;
    fp->seek_offset = CLUSTER_BDRY(fp->seek_offset);
  }

  seek_sector = fil_sector_address(fp->seek_cluster) +
    (offset - CLUSTER_BDRY(fp->seek_offset))/512;
  fp->seek_offset = offset;

  return(sd_buffer_checkout(seek_sector));
}

er_t fil_seek_next(struct fil * fp)
{
  return(fil_seek(fp, fp->seek_offset + 512));
}

#ifdef DBG_FIL
/*
  Scans, lists cwd.
 */
er_t fil_scan_dbg(void)
{
  uint32_t offset;
  er_t er;

  er = fil_seek(&cwd, offset = 0);
  for ( ; !er; er = fil_seek(&cwd, offset += 32)) { 
    uint32_t cluster;
    struct dirent * dp;
    char fn[13];

    dp = (struct dirent *)(sd_buffer + offset%512);
    if (dp->attr == FIL_ATTR_LFN) continue;
    if (!*(dp->name)) continue;
    if (*(dp->name) == 0xe5) continue;
    strncpy(fn, (char *)(dp->name), 11);
    fn[11] = ':';
    fn[12] = '\0';
    tx_puts(fn);
    tx_putdec32(dp->size);
    tx_puts("\r\n");
    /* trace cluster chain */
    cluster = (dp->clust1hi << 16) | dp->clust1lo;
    cluster &= CLUSTER_MASK;
    while (cluster && cluster < CHAIN_END) {
      tx_putc(' ');
      tx_puthex32(cluster);
      tx_puts("\r\n");
      cluster = fat_cdr(cluster);
    }
  }
  return(er==FIL_ECHAIN?0:er);
}
#endif /* DBG_FIL */

er_t fil_scan_cwd(char fn[11],
  er_t (*cmp)(struct dirent *), struct fil * fp)
{
  er_t er;
  uint8_t i;
  struct dirent * dp;
  uint16_t us;

  for (er = fil_seek(&cwd, 0); !er; er = fil_seek_next(&cwd)) {
    for (i = 0; i < 16; i++) {
      dp = (struct dirent *)(sd_buffer + (32*i));
      if (fn) {
        if (0 == strncmp((char *)dp->name, fn, 11)) goto found;
      } else {
        er = cmp(dp);
        if (!er) goto found;
        if (er < 0) return(er);
      }
    }
  }
  return(er?er:FIL_ENOENT);
found:
  fp->seek_cluster = 0;
  fp->seek_offset = 0;
  COPY4(fp->file_size, &dp->size);
  COPY2(us, &dp->clust1hi);
  fp->head = us << 16;
  COPY2(us, &dp->clust1lo);
  fp->head |= us;
  fp->dirent_sector = fil_sector_address(cwd.seek_cluster) +
    (cwd.seek_offset & (fil_bytes_per_cluster-1))/512;
  fp->dirent_offset = i * 32;
  COPY2(fp->wtime, &dp->wtime);
  COPY2(fp->wdate, &dp->wdate);
  fp->attributes = dp->attr;

  fp->tail = fp->head;
  while (fp->tail) {
    uint32_t next;
    next = fat_cdr(fp->tail);
    if (!next) return(fat_er);
    if (IS_EOC(next)) break;
    fp->tail = next;
  }
  return(0);
}

er_t fil_open(char fn[11], struct fil * fp)
{
  return(fil_scan_cwd(fn, 0, fp));
}

er_t fil_save_dirent(struct fil * fp, char fn[11], bool flag_sync)
{
  er_t er;
  struct dirent * dp;
  uint16_t us;

  if (!fp->dirent_sector) return(0);
  er = sd_buffer_checkout(fp->dirent_sector);
  if (er) return(er);

  dp = (struct dirent *)(sd_buffer + fp->dirent_offset);

  if (fn) {
    uint8_t i;
    for (i = 0; i < 11; i++)
      dp->name[i] = toupper(fn[i]);
  }

  if (!(fp->attributes & FIL_ATTR_DIR))
    memcpy(&dp->size, &fp->file_size, 4);
  else
    memset(&dp->size, 0, 4);
  us = fp->head;
  memcpy(&dp->clust1lo, &us, 2);
  us = fp->head >> 16;
  memcpy(&dp->clust1hi, &us, 2);
  dp->attr = fp->attributes;
  dp->time_10 = 0;
  memcpy(&dp->cdate, &fp->wdate, 2);
  memcpy(&dp->adate, &fp->wdate, 2);
  memcpy(&dp->wdate, &fp->wdate, 2);
  memcpy(&dp->ctime, &fp->wtime, 2);
  memcpy(&dp->wtime, &fp->wtime, 2);
  return(flag_sync?sd_buffer_sync():0);
}

er_t fil_append(struct fil * fp, uint8_t * buf, uint16_t len)
{
  er_t er;

  if (!len) return(0);

  do {
    uint16_t used, avail, count;
    uint32_t sector;

    /* Need a/new cluster? */
    used = fp->file_size & (fil_bytes_per_cluster-1);
    if (!used) {
      uint32_t cluster;
      cluster = fat_chain_grow(fp->tail);
      if (!cluster) return(fat_er);
      fp->tail = cluster;
      if (!fp->head)
        fp->head = cluster;
      used = 0;
    }

    sector = fil_sector_address(fp->tail);
    sector += (fp->file_size & (fil_bytes_per_cluster-1))/512;
    er = sd_buffer_checkout(sector);
    if (er) return(er);

    used = fp->file_size & 511;
    avail = 512 - used;
    if (len > avail) count = avail;
    else count = len;
    memcpy(sd_buffer + used, buf, count);
    fp->file_size += count;
    len -= count;
    buf += count;

    if (!(fp->file_size & 1023)) {
      er = fil_save_dirent(fp, 0, false);
      if (er) return(er);
    }

  } while (len > 0);

  return(0);
}

/* Adds another whole cluster */
static er_t grow_dir(struct fil * fp)
{
  er_t er;
  uint8_t buf[16];
  uint16_t c;

  memset(buf, 0, sizeof(buf));
  for (c = fil_bytes_per_cluster/sizeof(buf); c > 0; c--) {
    er = fil_append(fp, buf, sizeof(buf));
    if (er) return(er);
  }
  return(0);
}

static char * cpy_lfn;
static void cpy(uint8_t * dst, int8_t len)
{
  for ( ; len > 0; len--) {
    if ((*dst++ = *cpy_lfn)) cpy_lfn++;
    *dst++ = '\0';
  }
}
er_t fil_create(char fn[11], char lfn[26], struct fil * fp)
{
  er_t er;
  struct { uint32_t sector, offset; } slots[3];
  int8_t nr, nr_slots_reqd, i, nr_errors;
  uint32_t offset;

  nr_slots_reqd = (lfn?3:1);

  nr = 0;
  nr_errors = 0;
  for (offset = 0; ; offset += 512) {
    struct dirent * dp;
    er = fil_seek(&cwd, offset);
    if (er) {
      ASSERT(0 == nr_errors);
      if (nr_errors > 0) return(er);
      nr_errors++;
      dbg(tx_msg("fil_create:fil_seek = ", er));
      dbg_print32("fil_create:with offset ", offset);
      if (FIL_ECHAIN != er && FIL_ESEEK != er) return(er);
      dbg_fil_print(&cwd, "cwd:");
      er = grow_dir(&cwd);
      dbg(tx_msg("fil_create:grow_dir = ", er));
      dbg_fil_print(&cwd, "cwd:");
      if (er) return(er);
      er = fil_seek(&cwd, offset);
      dbg(tx_msg("fil_create:fil_seek (again) = ", er));
      if (er) return(er);
    }

    for (i = 0; i < 16; i++) {
      dp = (struct dirent *)(sd_buffer + i*32);
#if 0
      {
        int8_t j;
        tx_puts("Slot ");
        tx_putdec(i);
        tx_putc(':');
        for (j = 0; j < 11; j++) {
          tx_puthex(dp->name[j]);
          tx_putc(' ');
        }
        tx_puts("\r\n");
      }
#endif
      if (0 == strncmp((void *)dp->name, fn, 11)) return(FIL_EEXIST);
      if (0x00 == *dp->name || 0xe5 == *dp->name) {
        slots[nr].sector = seek_sector;
        slots[nr].offset = i*32;
        nr++;
        if (nr == nr_slots_reqd) goto create_file;
      } else {
        nr = 0;
      }
    }
  }
  /* Not Reached */

create_file:
  dbg(tx_puts("Found slots for file \""));
  dbg(tx_puts(fn));
  dbg(tx_puts("\" at sector offset "));
  dbg(tx_puthex32(offset));
  dbg(tx_puts("\r\n"));

  if (lfn) {
    uint8_t cs;
    struct lfn_dirent * lp;

    for (i = cs = 0; i < 11; i++)
      cs = (cs&1?0x80:0) + (cs>>1) + (uint8_t)fn[i];

    cpy_lfn = lfn;
    lp = 0;  /* XXX shut up, compiler */
    ASSERT(nr_slots_reqd > 2);
    for (i = nr_slots_reqd-2; i >= 0; i--) {
      er = sd_buffer_checkout(slots[i].sector);
      if (er) return(er);
      lp = (struct lfn_dirent *)(sd_buffer + slots[i].offset);
      memset(lp, 0, sizeof(*lp));
      cpy(lp->name1, sizeof(lp->name1)/2);
      lfn += sizeof(lp->name1)/2;
      cpy(lp->name2, sizeof(lp->name2)/2);
      lfn += sizeof(lp->name2)/2;
      cpy(lp->name3, sizeof(lp->name3)/2);
      lfn += sizeof(lp->name3)/2;
      lp->checksum = cs;
      lp->attr = FIL_ATTR_LFN;
      lp->ord =                       /* min value is 1, not 0 */
        (nr_slots_reqd - 1) - i;
    }
    lp->ord |= 0x40;
  }

  i = nr_slots_reqd - 1;
  memset(fp, 0, sizeof(*fp));
  fp->dirent_sector = slots[i].sector;
  fp->dirent_offset = slots[i].offset;
  fp->wdate = FIL_DATE_NONE;
  dbg(tx_puts(fn));
  dbg_print32(",dirent address", (slots[i].sector * 512)
    + slots[i].offset);
  return(fil_save_dirent(fp, fn, true));
}

er_t fil_fchdir(struct fil * fp)
{
  if (!fp)
    return(cwd_init(fil_root_dir_1st_cluster));
  ASSERT(fp->file_size);
  ASSERT(fp->head);
  if (!(fp->attributes & FIL_ATTR_DIR))
    return(FIL_ENOTDIR);
  return(cwd_init(fp->head));
}

er_t fil_chdir(char fn[11])
{
  er_t er;

  if (fn) {
    struct fil f;
    er = fil_open(fn, &f);
    if (er) return(er);
    return(fil_fchdir(&f));
  }
  return(cwd_init(fil_root_dir_1st_cluster));
}

er_t fil_mkdir(char fn[11], char lfn[11], struct fil * fp)
{
  er_t er;
  struct dirent * dp;
  uint32_t head;

  er = fil_create(fn, lfn, fp);
  if (er) return(er);
  er = grow_dir(fp);
  if (er) return(er);
  fp->attributes |= FIL_ATTR_DIR;
  er = sd_buffer_checkout(fil_sector_address(fp->head));
  if (er) return(er);

  dp = (struct dirent *)(sd_buffer);
  strncpy((void *)dp[0].name, ".          ", 11);
  dp[0].attr = FIL_ATTR_DIR;
  dp[0].clust1lo = fp->head & 0xffff;
  dp[0].clust1hi = fp->head >> 16;
  strncpy((void *)dp[1].name, "..         ", 11);
  dp[1].attr = FIL_ATTR_DIR;

  head = cwd.head;
  if (head == fil_root_dir_1st_cluster) head = 0;

  dp[1].clust1lo = head & 0xffff;
  dbg(tx_msg("\"..\" clust1lo = ", dp[1].clust1lo));
  dp[1].clust1hi = head >> 16;
  dbg(tx_msg("\"..\" clust1hi = ", dp[1].clust1hi));
  return(fil_save_dirent(fp, 0, true));
}
