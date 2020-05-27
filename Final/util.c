/*********** util.c file ****************/

int get_block(int dev, int blk, char *buf)
{
   lseek(dev, (long)blk*BLKSIZE, 0);
   read(dev, buf, BLKSIZE);
}   
int put_block(int dev, int blk, char *buf)
{
   lseek(dev, (long)blk*BLKSIZE, 0);
   write(dev, buf, BLKSIZE);
}   

int tokenize(char *pathname)
{
  int i;
  char *s;
  printf("tokenize %s\n", pathname);

  strcpy(gpath, pathname);   // tokens are in global gpath[ ]
  n = 0;

  s = strtok(gpath, "/");
  while(s){
    name[n] = s;
    n++;
    s = strtok(0, "/");
  }

  for (i= 0; i<n; i++)
    printf("%s  ", name[i]);
  printf("\n");
}

// return minode pointer to loaded INODE
MINODE *iget(int dev, int ino)
{
  int i;
  MINODE *mip;
  char buf[BLKSIZE];
  int blk, offset;
  INODE *ip;

  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    if (mip->dev == dev && mip->ino == ino){
       mip->refCount++;
       //printf("found [%d %d] as minode[%d] in core\n", dev, ino, i);
       return mip;
    }
  }
    
  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    if (mip->refCount == 0){
       //printf("allocating NEW minode[%d] for [%d %d]\n", i, dev, ino);
       mip->refCount = 1;
       mip->dev = dev;
       mip->ino = ino;

       // get INODE of ino into buf[ ]    
       blk    = (ino-1)/8 + inode_start;
       offset = (ino-1) % 8;

       //printf("iget: ino=%d blk=%d offset=%d\n", ino, blk, offset);

       get_block(dev, blk, buf);
       ip = (INODE *)buf + offset;
       // copy INODE to mp->INODE
       mip->INODE = *ip;
       return mip;
    }
  }   
  printf("PANIC: no more free minodes\n");
  return 0;
}

void iput(MINODE *mip)
{
 int i, block, offset;
 char buf[BLKSIZE];
 INODE *ip;

 if (mip == 0) return;

 mip->refCount--;
 
 if (mip->refCount > 0)  // minode is still in use
    return;
 if (!mip->dirty)        // INODE has not changed; no need to write back
    return;
 
 /* write INODE back to disk */
 block = ((mip->ino - 1) / 8) + inode_start;
 offset = (mip->ino - 1) % 8;

 get_block(mip->dev, block, buf);

 ip = (INODE *)buf + offset;
 *ip = mip->INODE;
 put_block(mip->dev, block, buf); //write back to disk

} 

int search(MINODE *mip, char *name)
{
   char *cp, c, sbuf[BLKSIZE], temp[256];
   DIR *dp;
   INODE *ip;

   printf("search for %s in MINODE = [%d, %d]\n", name, mip->dev, mip->ino);
   ip = &(mip->INODE);

   /*** search for name in mip's data blocks: ASSUME i_block[0] ONLY ***/

   get_block(dev, ip->i_block[0], sbuf);
   dp = (DIR *)sbuf;
   cp = sbuf;
   printf("  ino   rlen  nlen  name\n");

   while (cp < sbuf + BLKSIZE){
     strncpy(temp, dp->name, dp->name_len);
     temp[dp->name_len] = 0;
     printf("%4d  %4d  %4d    %s\n", 
           dp->inode, dp->rec_len, dp->name_len, temp);
     if (strcmp(temp, name)==0){
        printf("found %s : ino = %d\n", temp, dp->inode);
        return dp->inode;
     }
     cp += dp->rec_len;
     dp = (DIR *)cp;
   }
   return 0;
}

int getino(char *pathname)
{
  int i, ino, blk, disp;
  char buf[BLKSIZE];
  INODE *ip;
  MINODE *mip;

  printf("getino: pathname=%s\n", pathname);
  if (strcmp(pathname, "/")==0)
      return 2;
  
  // starting mip = root OR CWD
  if (pathname[0]=='/')
     mip = root;
  else
     mip = running->cwd;

  mip->refCount++;         // because we iput(mip) later
  
  tokenize(pathname);

  for (i=0; i<n; i++){
      printf("===========================================\n");
      printf("getino: i=%d name[%d]=%s\n", i, i, name[i]);
 
      ino = search(mip, name[i]);

      if (ino==0){
         iput(mip);
         printf("name %s does not exist\n", name[i]);
         return 0;
      }
      iput(mip);                // release current mip
      mip = iget(dev, ino);     // get next mip
   }

  iput(mip);                   // release mip  
   return ino;
}

int findmyname(MINODE *parent, u32 myino, char *myname) 
{
  // WRITE YOUR code here:
  // search parent's data block for myino;
  // copy its name STRING to myname[ ];

  char buf[BLKSIZE], *cp;
  DIR *dp;
  
  if (myino == root->ino) {
     strcpy(myname, "/");
     return -1;
  }

  if (!parent) {
     printf("parent ino %d doesn't exist", parent->ino);
     return 0;
  }

  if(!S_ISDIR(parent->INODE.i_mode)) {
     printf("parent ino %d not in a directory", parent->ino);
     return 0;
  }

  for (int i = 0; i <12; i++) {
     if (parent->INODE.i_block[i]) {
        get_block(parent->dev, parent->INODE.i_block[i], buf);
        dp = (DIR *)buf;
        cp = buf;

        while (cp < buf+BLKSIZE) {
           if (dp->inode == myino) {
              strcpy(myname, dp->name);
              myname[dp->name_len] = '\0';
              return 1;
           }

           cp += dp->rec_len;
           dp = (DIR *)cp;
        }
     }
   }
   return 0;
}

int findino(MINODE *mip, u32 *myino) // myino = ino of . return ino of ..
{
  char buf[BLKSIZE], *cp;   
  DIR *dp;

  get_block(mip->dev, mip->INODE.i_block[0], buf);
  cp = buf; 
  dp = (DIR *)buf;
  *myino = dp->inode;
  cp += dp->rec_len;
  dp = (DIR *)cp;
  return dp->inode;
}

int ialloc(int dev)  // allocate an inode number from inode_bitmap
{
  int  i;
  char buf[BLKSIZE];

   // read inode_bitmap block
  get_block(dev, imap, buf);

  for (i=0; i < ninodes; i++){
    if (tst_bit(buf, i)==0){
        set_bit(buf, i);
        put_block(dev, imap, buf);
        printf("allocated ino = %d\n", i+1); // bits count from 0; ino from 1
        return i+1;
    }
  }
  return 0;
}

int tst_bit(char *buf, int bit) {
  return (buf[bit / 8] & (1 << bit % 8));
}

int set_bit(char *buf, int bit) {
  buf[bit / 8] |= (1 << bit % 8);
}

int decFreeBlocks(int dev) {
   char buf[BLKSIZE];

   // dec free blocks count in SUPER and GD
   get_block(dev, 1, buf);
   sp = (SUPER *)buf;
   sp->s_free_blocks_count--;
   put_block(dev, 1, buf);
   get_block(dev, 2, buf);
   gp = (GD *)buf;
   gp->bg_free_blocks_count--;
   put_block(dev, 2, buf);
}

int balloc(int dev) {
   char buf[BLKSIZE];

   get_block(dev, bmap, buf);

   for (int i =0; i < nblocks; i++) {
      if (tst_bit(buf, i) == 0) {
         set_bit(buf, i);
         decFreeBlocks(dev);

         put_block(dev, bmap, buf);
         return i;
      }
   }

   printf("balloc(): no more free blocks\n");
   return 0;
}


int idealloc(int dev, int ino)  // deallocate an ino number
{
  int i;  
  char buf[BLKSIZE];

  if (ino > ninodes){
    printf("inumber %d out of range\n", ino);
    return 0;
  }

  // get inode bitmap block
  get_block(dev, imap, buf);
  clr_bit(buf, ino-1);

  // write buf back
  put_block(dev, imap, buf);
}

//clear bit in char buf[BLKSIZE]
int clr_bit(char *buf, int bit) {
   buf[bit / 8] &= ~(1 << bit % 8);
}

// deallocate a blk number
int bdealloc(int dev, int blk) {
   // WRITE YOUR OWN CODE to deallocate a block number blk
   char buf[BLKSIZE];

   //clear bit in bmap
   get_block(dev, bmap, buf);
   clr_bit(buf, blk-1);
   put_block(dev, bmap, buf);

   //update SUPER counter
   get_block(dev, 1, buf);
   sp =  (SUPER *)buf;
   sp->s_free_blocks_count++;
   put_block(dev, 1, buf);

   //update GD counter
   get_block(dev, 2, buf);
   gp = (GD *)buf;
   gp->bg_free_blocks_count++;
   put_block(dev, 2, buf);
}
