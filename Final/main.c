/****************************************************************************
*                   KCW  Implement ext2 file system                         *
*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ext2fs/ext2_fs.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <time.h>
#include "type.h"

// global variables
MINODE minode[NMINODE];
MINODE *root;

PROC   proc[NPROC], *running;

char gpath[128]; // global for tokenized components
char *name[32];  // assume at most 32 components in pathname
int   n;         // number of component strings

int fd, dev;
int nblocks, ninodes, bmap, imap, inode_start, free_blocks, free_inodes; // disk parameters
char pathname[256], pathname2[256], nullpathname[256];
char *disk = "mydisk";

#include "util.c"
#include "cd_ls_pwd.c"
#include "mkdir_creat.c"
#include "rmdir.c"
#include "link_unlink.c"
#include "symlink.c"
#include "open_close_lseek.c"
#include "read_cat.c"
#include "write_cp.c"

int init()
{
  int i, j;
  MINODE *mip;
  PROC   *p;

  printf("init()\n");

  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    mip->dev = mip->ino = 0;
    mip->refCount = 0;
    mip->mounted = 0;
    mip->mptr = 0;
  }
  for (i=0; i<NPROC; i++){
    p = &proc[i];
    p->pid = i;
    p->uid = p->gid = 0;
    p->cwd = 0;
    p->status = FREE;
    for (j=0; j<NFD; j++)
      p->fd[j] = 0;
  }
}

// load root INODE and set root pointer to it
int mount_root()
{
  SUPER *sp;
  GD *gp;
  char buf[BLKSIZE];

  printf("mount_root()\n");

  dev = open(disk, O_RDWR);
  if(dev < 0) {
    printf("can't open open root device\n");
    exit(1);
  }
  /* get super block of rootdev */
  get_block(dev, 1, buf);
  sp = (SUPER *) buf;

  /* Check that the super block is an EXT2 FS */
  if(sp->s_magic != 0xEF53) {
    printf("Not an EXT2 file system\n");
    exit(0);
  }

  //set globals
  ninodes = sp->s_inodes_count;
  nblocks = sp->s_blocks_count;
  free_inodes = sp->s_free_inodes_count;
  free_blocks = sp->s_blocks_count;
  get_block(dev, 2, buf);
  gp = (GD *)buf;
  inode_start = gp->bg_inode_table;
  imap = gp->bg_inode_bitmap;
  bmap = gp->bg_block_bitmap;

  root = iget(dev, 2);

  if(root != NULL) {
    proc[0].cwd = iget(dev, 2);
    proc[1].cwd = iget(dev, 2);
    root->refCount = 1;
    running = &proc[0];
    printf("root mounted\n");
  }
  else {
    printf("root failed to mount :(\n");
    exit(1);
  }
}

int main(int argc, char *argv[ ])
{
  int ino, fdint, pos, originalOffset;
  char buf[BLKSIZE];
  char line[128], cmd[32];
 
  if (argc > 1) disk = argv[1];
  
  fd = open(disk, O_RDWR);

  printf("checking EXT2 FS ....");
  if ((fd = open(disk, O_RDWR)) < 0) {
    printf("open %s failed\n", disk);
    exit(1);
  }
  dev = fd;    // fd is the global dev 

  /********** read super block  ****************/
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;

  /* verify it's an ext2 file system ***********/
  if (sp->s_magic != 0xEF53) {
      printf("magic = %x is not an ext2 filesystem\n", sp->s_magic);
      exit(1);
  }     
  printf("EXT2 FS OK\n");
  ninodes = sp->s_inodes_count;
  nblocks = sp->s_blocks_count;

  printf("ninodes = %d nblocks = %d\n", ninodes, nblocks);


  get_block(dev, 2, buf); 
  gp = (GD *)buf;

  bmap = gp->bg_block_bitmap;
  imap = gp->bg_inode_bitmap;
  inode_start = gp->bg_inode_table;
  printf("bmp=%d imap=%d inode_start = %d\n", bmap, imap, inode_start);

  init();  
  mount_root();
  printf("root refCount = %d\n", root->refCount);

  printf("creating P0 as running process\n");
  running = &proc[0];
  running->status = READY;
  running->cwd = iget(dev, 2);
  printf("root refCount = %d\n", root->refCount);

  // WRTIE code here to create P1 as a USER process
  
  while(1){
    printf("input command : [ls|cd|pwd|mkdir|creat|rmdir|link|unlink|symlink|readlink|  open|close|lseek|read|cat|write|cp|mv|quit] ");
    fgets(line, 128, stdin);
    line[strlen(line)-1] = 0;

    if (line[0]==0)
       continue;
    pathname[0] = 0;

    sscanf(line, "%s %s %s", cmd, pathname, pathname2);
    printf("cmd=%s | pathname=%s | pathname2= %s\n", cmd, pathname, pathname2);
  
    if (strcmp(cmd, "ls")==0)
       ls(pathname);
    else if (strcmp(cmd, "cd")==0)
       chdir(pathname);
    else if (strcmp(cmd, "pwd")==0)
       pwd(running->cwd);
    else if (strcmp(cmd, "quit")==0)
       quit();
    else if (strcmp(cmd, "mkdir") == 0) {
      if (strcmp(pathname, "") == 0) printf("pathname missing\n");
      else if (make_Dir(pathname) == 0) printf("Directory created\n");
    }
    else if (strcmp(cmd, "creat") == 0) {
      if (strcmp(pathname, "") == 0) printf("pathname missing\n");
      else if (creat_file(pathname) == 0) printf("File created\n");
    }
    else if (strcmp(cmd, "rmdir") == 0) {
      if (strcmp(pathname, "") == 0) printf("pathname missing\n");
      else if (rmdir() == 0) printf("Directory removed\n");
    }
    else if (strcmp(cmd, "link") == 0) {
      if (strcmp(pathname, "") == 0 || strcmp(pathname2, "") == 0) printf("pathname missing\n");
      else if (link() == 0) {
        printf("Link created\n");
        strcpy(pathname2, nullpathname);
      }
    }
    else if (strcmp(cmd, "unlink") == 0) {
      if (strcmp(pathname, "") == 0) printf("pathname missing\n");
      else if (unlink() == 0) printf("Link removed\n");
    }
    else if (strcmp(cmd, "symlink") == 0) {
      if (strcmp(pathname, "") == 0 || strcmp(pathname2, "") == 0) printf("pathname missing\n");
      else if (symlink() == 0) {
        printf("Symbolic link created\n");
        strcpy(pathname2, nullpathname);
      }
    }
    else if (strcmp(cmd, "readlink") == 0) {
      if (strcmp(pathname, "") == 0) printf("pathname missing\n");
      else if (readlink() == 0) printf("Link read\n");
    }
    else if (strcmp(cmd, "open") == 0) {
      if (strcmp(pathname, "") == 0 || strcmp(pathname2, "") == 0) printf("pathname and/or mode# missing\n");
      else if (open_file(pathname, pathname2) > -1) {
        printf("File opened.\n");
        strcpy(pathname2, nullpathname);
      }
    }
    else if (strcmp(cmd, "close") == 0) {
      if (strcmp(pathname, "") == 0) printf("pathname missing\n");
      else {
        fdint = atoi(pathname);
        if(close_file(fdint) == 0) printf("File close\n");
      } 
    }
    else if (strcmp(cmd, "pfd") == 0) {
      pfd(); // this command is essential in being able to use lseek
    }
    else if(strcmp(cmd, "lseek") == 0) {
      if (strcmp(pathname, "") == 0 || strcmp(pathname2, "") == 0) printf("either fd or pos not specified\n");
      else {
          fdint = atoi(pathname);
          pos = atoi(pathname2);
          if (originalOffset = mylseek(fdint, pos) > -1) printf("lseek successful, file offset moved from %d to %d\n", originalOffset, pos);
      }
    }
    else if(strcmp(cmd, "read") == 0) {
      if (strcmp(pathname, "") == 0 || strcmp(pathname2, "") == 0) printf("either fd or nbyte not specified\n");
      else {
          if (read_file() > -1) {
            printf("File successfully read\n");
          }
      }
    }
    else if(strcmp(cmd, "cat") == 0) {
      if (strcmp(pathname, "") == 0) printf("filename not specified, cannot display file\n");
      else {
        cat(pathname);
      }
    }
    else if(strcmp(cmd, "write") == 0) {
      if (strcmp(pathname, "") == 0 || strcmp(pathname2, "") == 0) printf("either fd or string being written in not specified\n");
      else {
          if (write_file() > -1) {
            printf("File successfully written in\n");
          }
      } 
    }
    else if(strcmp(cmd, "cp") == 0) {
      if (strcmp(pathname, "") == 0 || strcmp(pathname2, "") == 0) printf("copyfile and/or newfile in not specified\n");
      else {
          if (copy_file() > -1) {
            printf("File successfully copied in\n");
          }
      } 
    }
    else if(strcmp(cmd, "mv") == 0) {
      if (strcmp(pathname, "") == 0 || strcmp(pathname2, "") == 0) printf("source and/or destination in not specified\n");
      else {
          if (mv_file() > -1) {
            printf("File successfully moved in\n");
          }
      } 
    }
  }
}

int quit()
{
  int i;
  MINODE *mip;
  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    if (mip->refCount > 0)
      iput(mip);
  }
  exit(0);
}