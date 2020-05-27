/************* link_unlink.c file **************/
extern char pathname[256], pathname2[256];


int link() {
    char buf[BLKSIZE], temp[128], newfile[256], oldfile[256], *parent, *child;
    MINODE *omip, *pip;
    INODE *ip;
    int oino, pino;

    // verify oldfile exists and is not a DIR
    strcpy(oldfile, pathname);
    strcpy(newfile, pathname2);

    oino = getino(oldfile);

    if (!oino) {
        printf("oldfile(%s) ino does not exist\n", oldfile);
        return -1;
    }
    omip = iget(dev, oino);

    if (S_ISDIR(omip->INODE.i_mode)) {
        printf("cannot link; %s is a directory\n", oldfile);
        return -1;
    }
    
    // newfile must not exist yet
    if (getino(newfile)) {
        printf("cannot link, %s is an existing file", newfile);
        return -1;
    }

    // creat newfile with the same inode number of oldfile
    strcpy(temp, newfile);
    parent = dirname(newfile);
    child = basename(temp);
    pino = getino(parent);
    pip = iget(dev, pino);

    // creat entry in new parent DIR with same inode number of oldfile
    enter_name(pip, oino, child);

    omip->INODE.i_links_count++;    // inc INODE'S link_count by 1
    omip->dirty = 1;                 // for write back by iput(omip)

    ip = &pip->INODE;
    ip->i_atime = time(0L);
    pip->dirty = 1;

    iput(omip);
    iput(pip);

}

int unlink() {
    MINODE *mip, *pmip;
    INODE *ip;
    int ino, pino;
    char buf[BLKSIZE], pcopy1[128], pcopy2[128], *parent, *child;

    strcpy(pcopy1, pathname);
    strcpy(pcopy2, pathname);

    // get filename's minode
    ino = getino(pathname);
    mip = iget(dev, ino);
    
    //verify file exists
    if (!ino) {
        printf("file(%s) ino does not exist\n", pathname);
        return -1;
    }

    if(!mip) {
        printf("missing minode\n");
        return -1;
    }
    
    // check if it's a REG or symbolic LNK file; can not be a DIR
    if (S_ISDIR(mip->INODE.i_mode)) {
        printf("unlink failed; %s is a directory\n", pathname);
        return -1;
    }


    //decrement INODE's i_links_count by 1
    // if i_links_count == 0, rm pathname by dealloc its data blocks using truncate()
    ip = &mip->INODE;
    ip->i_links_count--;
    printf("link count: %d\n", ip->i_links_count);
    if (ip->i_links_count == 0) {
        truncate(mip);
        printf("truncate successful\n");
        idealloc(mip->dev, ino);
    }

    //remove name entry from parent DIR's data block
    parent = dirname(pcopy1);
    child = basename(pcopy2);
    
    pino = getino(parent);
    pmip = iget(mip->dev, pino);

    rm_child(pmip, child);

    //update parent then write back to disk
    ip = &pmip->INODE;
    pmip->dirty = 1;
    ip->i_atime = time(0L);
    ip->i_mtime = time(0L);
    iput(pmip);

    mip->dirty = 1;
    iput(mip);

    return 0;
}

//this function deallocates ALL data blocks of an INODE
/* 1. release mip->INODE's data blocks;
     a file may have 12 direct blocks, 256 indirect blocks and 256*256
     double indirect data blocks. release them all.
  2. update INODE's time field 
  3. set INODE's size to 0 and mark Minode[ ] dirty*/
int truncate(MINODE *mip) {
    int *ibuf, *dibuf; // indirect and double indirect bufs
    char buf[BLKSIZE];
    INODE *ip;

    ip = &mip->INODE;

    // Dealloc direct blocks
    for (int i = 0; i < 12; i++) {
        if (ip->i_block[i] != 0) bdealloc(mip->dev, ip->i_block[i]);
    }

    // Dealloc indirect blocks
    get_block(mip->dev, ip->i_block[12], buf);
    ibuf = (int *)buf;
    for (int i = 0; i < 256; i++) {
        if (ibuf[i] != 0) bdealloc(mip->dev, ibuf[i]);
        else break;
    }

    // Dealloc double indirect blocks
    get_block(mip->dev, ip->i_block[13], buf);
    ibuf = (int *)buf;
    for (int i = 0; i < 256; i++) {
        if (ibuf[i] != 0) {
            get_block(mip->dev, ibuf[i], buf);
            dibuf = (int *)buf;
            for (int j = 0; j < 256; j++) {
                if (dibuf[j] != 0) bdealloc(mip->dev, dibuf[j]);
                else break;
            }
        }
        else break;
    }

    //mark minode as dirty, set proper inode properties
    mip->dirty = 1;
    ip->i_atime = time(0L);
    ip->i_mtime = time(0L);
    ip->i_size = 0;
    iput(mip);
}