/************* mkdir_creat.c file **************/

make_Dir(char *pathname) {
    MINODE *mip, *pip;
    char parent[64], child[64], pcopy1[256], pcopy2[256];
    int dev, pino;

    if (pathname[0] == '/') {
        mip = root;
        dev = root->dev;
    }
    else {
        mip = running->cwd;
        dev = running->cwd->dev;
    }

    //making copies of the pathname 
    //because running dirname() and basename() would destroy it
    strcpy(pcopy1, pathname);
    strcpy(pcopy2, pathname);
    strcpy(parent, dirname(pcopy1));
    strcpy(child, basename(pcopy2));

    printf("creating dir at %s\n", pathname);

    pino = getino(parent);
    printf("parent ino: %d\n", pino);

    //verify parent exists and is a DIR

    if (!pino) {
        printf("parent %s does not exist", parent);
        return -1;
    }

    pip = iget(dev, pino);

    if (!S_ISDIR(pip->INODE.i_mode)) {
        printf("parent %s is not a DIR\n", parent);
        return -1;
    }

    //verify child does not exist in the parent directory
    if (search(&(pip->INODE), child) != 0) {
        printf("cannot mkdir, child exists in %s\n", parent);
        return -1;
    }

    mymkdir(pip, child);

    pip->INODE.i_links_count++;
    pip->dirty = 1; //reminder: dirty indicates INODE has been modified

    iput(pip);

    return 0;
}

int mymkdir(MINODE *pip, char *name) {
    MINODE *mip;
    INODE *ip;
    DIR *dp;
    char *cp;
    char buf[BLKSIZE];
    int ino, bno;

    ino = ialloc(dev);
    bno = balloc(dev);
    printf("ino = %d\tbno = %d\n", ino, bno);

    mip = iget(dev, ino);
    ip = &mip->INODE;

    ip->i_mode = 0x41ED;		// OR 040755: DIR type and permissions
    ip->i_uid  = running->uid;	// Owner uid 
    ip->i_gid  = running->gid;	// Group Id
    ip->i_size = BLKSIZE;		// Size in bytes 
    ip->i_links_count = 2;	    // Links count=2 because of . and ..
    ip->i_atime = time(0L);     // set to current time
    ip->i_ctime = time(0L);
    ip->i_mtime = time(0L);
    ip->i_blocks = 2;           // LINUX: Blocks count in 512-byte chunks (. and ..)
    ip->i_block[0] = bno;       // new DIR has one data block   
    
    // i_block[1] to i_block[14] = 0;
    for (int i = 1; i < 15; i++) {
        ip->i_block[i] = 0;
    }

    mip->dirty = 1;             // mark minode dirty
    iput(mip);                  // write INODE to disk

    get_block(dev, bno, buf);

    // write . to a buf
    dp = (DIR *)buf;
    cp = buf;

    dp->inode = ino;
    dp->rec_len = 12;
    dp->name_len = strlen(".");
    dp->name[0] = '.';

    //move pointer to after . and write .. to the buf
    cp += dp->rec_len;
    dp = (DIR *)cp;

    dp->inode = pip->ino;
    dp->rec_len = 1012; //. is 12 bytes so 1024(the BLKSIZE) - 12 = 1012
    dp->name_len = strlen("..");
    dp->name[0] = '.';
    dp->name[1] = '.';

    // write buf[] to the disk block bno
    put_block(dev, bno, buf);

    //enter name ENTRY into parent's directory
    enter_name(pip, ino, name);

    return 1;
}

creat_file(char *pathname) {
    MINODE *mip, *pip;
    char parent[64], child[64], pcopy1[256], pcopy2[256];
    int dev, pino;

    if (pathname[0] == '/') {
        mip = root;
        dev = root->dev;
    }
    else {
        mip = running->cwd;
        dev = running->cwd->dev;
    }

    //making copies of the pathname 
    //because running dirname() and basename() would destroy it
    strcpy(pcopy1, pathname);
    strcpy(pcopy2, pathname);
    strcpy(parent, dirname(pcopy1));
    strcpy(child, basename(pcopy2));

    printf("creating file at %s\n", pathname);

    pino = getino(parent);
    printf("parent ino: %d\n", pino);

    //verify parent exists

    if (!pino) {
        printf("parent %s does not exist", parent);
        return -1;
    }

    pip = iget(dev, pino);

    //verify child does not exist in the parent directory
    if (search(&(pip->INODE), child) != 0) {
        printf("cannot creat, child exists in %s\n", parent);
        return -1;
    }

    my_creat(pip, child);
    pip->dirty = 1; //reminder: dirty indicates INODE has been modified
    iput(pip);

    return 0;
}

int my_creat(MINODE *pip, char *name) {
    MINODE *mip;
    INODE *ip;
    DIR *dp;
    char *cp;
    char buf[BLKSIZE];
    int ino;

    //allocate the inode, since this is creating a file there is no balloc()
    ino = ialloc(dev);
    printf("ino = %d\n", ino);

    //get inode into memory
    mip = iget(dev, ino);
    ip = &mip->INODE;

    ip->i_mode = 0x81B6;		// OR 0x81A4: File type and permissions
    ip->i_uid  = running->uid;	// Owner uid 
    ip->i_gid  = running->gid;	// Group Id
    ip->i_size = 0;		        // File size in bytes 
    ip->i_links_count = 1;	    // Links count=1 because of it being a file
    ip->i_atime = time(0L);     // set to current time
    ip->i_ctime = time(0L);
    ip->i_mtime = time(0L);
    ip->i_blocks = 0;           // there is no data block  
    
    // i_block[1] to i_block[14] = 0;
    for (int i = 1; i < 15; i++) {
        ip->i_block[i] = 0;
    }

    mip->dirty = 1;             // mark minode dirty
    iput(mip);                  // write INODE to disk

    //enter name ENTRY into parent's directory
    enter_name(pip, ino, name);

    return 1;
}

int enter_name(MINODE *pip, int ino, char *name) {
   INODE *ip;
   DIR *dp;
   char *cp, buf[BLKSIZE];
   int i, bno, need_length, ideal_length, remain;

   ip = &pip->INODE;

   for (i = 0; i < 12; i++) {
      if(ip->i_block[i] == 0) break;
      bno = ip->i_block[i];
      get_block(pip->dev, ip->i_block[i], buf);
      dp = (DIR *)buf;
      cp = buf;

      //step into the last entry in the data block
      while (cp + dp->rec_len < buf+BLKSIZE) {
         cp += dp->rec_len;
         dp = (DIR *)cp;
      }

      ideal_length = 4 * ((8 + dp->name_len + 3) / 4);
      need_length = 4 * ((8 + strlen(name) + 3) / 4);
      remain = dp->rec_len - ideal_length;

      //check if there's enough space left in block
      if (remain >= need_length) {
         dp->rec_len = ideal_length;
         cp += dp->rec_len;
         dp = (DIR *)cp;

         //add entry to block
         dp->inode = ino;
         dp->rec_len = remain;
         dp->name_len = strlen(name);
         strcpy(dp->name, name);

         //write block into disk
         put_block(dev, bno, buf);
         return 1;
      }
   }

   //if here, there wasn't enough space in the block, so allocate new block

   // allocate new data block; increment parent size by BLKSIZE
   bno = balloc(dev);
   ip->i_size += BLKSIZE;
   ip->i_block[i] = bno;
   pip->dirty = 1;

   //enter new entry as the first entry in new data block with rec_len=BLKSIZE
   get_block(dev, bno, buf);
   dp = (DIR *)buf;
   cp = buf;
   dp->inode = ino;
   dp->rec_len = BLKSIZE;
   dp->name_len = strlen(name);
   strcpy(dp->name, name);

   //write block to disk
   put_block(dev, ino, buf);
   return 1;
}