/*
  Copyright 2020 Harold Tay GPLv3
  Test FAT filesystem functions.
 */
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/spi.h>
#include <string.h>
#include "fil.h"
#include "tx.h"
#include "fmt.h"
#include "sd2.h"
#include "power.h"
#include "usart_setup.h"

#define MHZ 48
#include "delay.h"

static void clock_setup(void)
{
  rcc_clock_setup_in_hse_8mhz_out_48mhz();
  usart_setup(USART1, GPIOA, GPIO9, GPIO_AF1, 57600);
  power_on(POWER_MODE_MASTER);
}

static void print_fil(struct fil * fp, char * fn)
{
  tx_puts(fn);
  tx_puts(":\r\ntail            = "); tx_puthex32(fp->tail);
  tx_puts("\r\nseek_cluster    = "); tx_puthex32(fp->seek_cluster);
  tx_puts("\r\nseek_offset     = "); tx_puthex32(fp->seek_offset);
  tx_puts("\r\nfile_size       = "); tx_puthex32(fp->file_size);
  tx_puts("\r\nhead            = "); tx_puthex32(fp->head);
  tx_puts("\r\ndirent_sector   = "); tx_puthex32(fp->dirent_sector);
  tx_puts("\r\ndirent_offset   = "); tx_puthex32(fp->dirent_offset);
  tx_puts("\r\nwtime           = "); tx_puthex32(fp->wtime);
  tx_puts("\r\nwdate           = "); tx_puthex32(fp->wdate);
  tx_puts("\r\nattributes      = "); tx_puthex32(fp->attributes);
  tx_puts("\r\n");
}
static void print_dirent(struct fil * fp, char * fn)
{
  int8_t er;
  struct dirent * dp;
  char name[12];
  uint32_t size;
  uint16_t us;

  er = sd_buffer_checkout(fp->dirent_sector);
  if (er) {
    tx_msg("print_dirent:sd_buffer_checkout failed, ", er);
    for ( ; ; );
  }
  dp = (struct dirent *)(sd_buffer + fp->dirent_offset);
  memcpy(name, dp->name, 11);
  name[11] = '\0';

  tx_puts(fn);
  tx_puts(":\r\nname     = "); tx_puts(name);
  tx_puts("\r\nattr     = "); tx_puthex(dp->attr);
  memcpy(&us, &dp->clust1hi, 2);
  tx_puts("\r\nclust1hi = "); tx_puthex32(us);
  memcpy(&us, &dp->clust1lo, 2);
  tx_puts("\r\nclust1lo = "); tx_puthex32(us);
  memcpy(&size, &dp->size, 4);
  tx_puts("\r\nsize     = "); tx_puthex32(size);
  tx_puts("\r\n");
}

void dump_sector(uint32_t addr)
{
  int8_t er;
  int16_t i;
  struct dirent * dp;

  er = sd_buffer_checkout(addr);
  tx_msg("sd_buffer_checkout returned ", er);
  while (er) ;

  dp = (struct dirent *)sd_buffer;
  for (i = 0; i < 16; i++, dp++) {
    char name[12];
    uint16_t us;
    uint32_t size;
    strncpy(name, (char *)dp->name, 11);
    name[11] = 0;
    tx_puts("\r\nname     = "); tx_puts(name);
    tx_puts("\r\nattr     = "); tx_puthex(dp->attr);
    memcpy(&us, &dp->clust1hi, 2);
    tx_puts("\r\nclust1hi = "); tx_puthex32(us);
    memcpy(&us, &dp->clust1lo, 2);
    tx_puts("\r\nclust1lo = "); tx_puthex32(us);
    memcpy(&size, &dp->size, 4);
    tx_puts("\r\nsize     = "); tx_puthex32(size);
    tx_puts("\r\n");
  }
}

int main(void)
{
  int8_t er;
  int16_t i;
  struct fil f, n, dir;
  char ldfn[] = "long directory name";

  clock_setup();

  for (i = 4; i > 0; i--) {
    tx_msg("Delay ", i);
    delay_ms(1100);
  }

  er = fil_init();
  tx_msg("fil_init returned ", er);

  er = fil_open("LOGFILE TXT", &f);
  tx_msg("fil_open returned ", er);
  while (er);
  print_fil(&f, "LOGFILE TXT");
  print_dirent(&f, "LOGFILE TXT");

  er = fil_mkdir("NEWD       ", ldfn, &dir);
  tx_msg("fil_mkdir(NEWD) returned ", er);
  while (er);
  print_fil(&dir, ldfn);
  dump_sector(fil_sector_address(dir.head));

  er = fil_fchdir(&dir);
  tx_msg("fil_fchdir(&dir) returned ", er);
  while (er);

  er = fil_chdir(0);
  tx_msg("fil_chdir(0) returned ", er);
  while (er);

  er = fil_chdir("NEWD       ");
  tx_msg("fil_chdir(NEWD) returned ", er);
  while (er);

  er = fil_create("NEWFILE TXT", "new file test.tst", &n);
  tx_msg("fil_create NEWFILE TXT returned ", er);
  if (!er) {
    print_fil(&n, "NEWFILE TXT");
    print_dirent(&n, "NEWFILE TXT");
  }

  tx_puts("Append test:\r\n");
  for (i = 0; i < 5000; i++) {
    tx_putc('\r');
    tx_putdec(i);
    er = fil_append(&f, (void *)"012345678\r\n", 11);
    if (er) break;
    er = fil_append(&n, (void *)"012345678\r\n", 11);
    if (er) break;
  }
  if (!er) {
    tx_puts("fil_append success!\r\n");
    er = fil_save_dirent(&f, 0, true);
    if (er) tx_msg("fil_save_dirent returned ", er);
    er = fil_save_dirent(&n, 0, true);
    if (er) tx_msg("fil_save_dirent returned ", er);
    print_fil(&f, "LOGFILE TXT");
    print_dirent(&f, "LOGFILE TXT");
    print_fil(&n, "NEWFILE TXT");
    print_dirent(&f, "NEWFILE TXT");
  }

  tx_puts("Many files test:\r\n");
  for (i = 0; i < (400); i++) {
    char fn[12];
    char lfn[27];
    *fn = 'F';
    strcpy(fn+1, fmt_32x(i) + 4);
    strcat(strcat(strcpy(lfn, fn), " "), fn+1);
    strcat(fn, " TXT");
    fil_sanitise(fn);
    er = fil_create(fn, lfn, &n);
    tx_puts("Creating file "); tx_puts(fn);
    tx_msg(" = ", er);
    if (er) break;
  }

  dump_sector(fil_sector_address(dir.head));

  if (er) tx_puts("Failed!\r\n");
  else tx_puts("Succeeded!\r\n");

  for ( ; ; ) ;
}

