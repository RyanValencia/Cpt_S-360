/************* rmdir.c file **************/

extern char pathname[256];

int rmdir() {
    int ino, pino;
    MINODE *mip, *pip;
    INODE *ip;
    char *parent, *child, pcopy1[256], pcopy2[256];

    strcpy(pcopy1, pathname);
    strcpy(pcopy2, pathname);

    //get in-memory INODE of pathname
    ino = getino(pathname);

    if(!ino) {
        printf("inode %s not found\n", pathname);
        return -1;
    }

    //verify INODE is a DIR
    mip = iget(dev, ino);
    ip = &mip->INODE;

    if (!S_ISDIR(ip->i_mode)) {
        printf("dir %s not found\n", pathname);
        return -1;
    }

    //verify DIR is empty
    if (checkEmpty(mip) != 1) {
        printf("dir %s is not empty\n", pathname);
        return -1;
    }

    printf("Removing %s\n", pathname);
    
    //deallocate its data blocks and inode
    for (int i = 0; i < 12; i++) {
        if (ip->i_block[i] == 0) continue;
        bdealloc(mip->dev, ip->i_block[i]);
    }
    
    //get parent's ino and inode
    parent = dirname(pcopy1);
    child = basename(pcopy2);
    pino = getino(parent);

    pip = iget(mip->dev, pino);
    printf("Removing ino: %d, pino: %d\n", mip->ino, pino);
    idealloc(mip->dev, mip->ino);
    mip->dirty = 1;
    iput(mip);

    //remove name from parent directory
    rm_child(pip, child);
    
    //decrease parent link count; mark pip dirty
    ip = &pip->INODE;
    ip->i_links_count--;
    ip->i_atime = time(0L);
    ip->i_ctime = time(0L);
    ip->i_mtime = time(0L);
    pip->dirty = 1;
    iput(pip);   

    return 0;

}

int rm_child(MINODE *parent, char *name) {
    char buf[BLKSIZE], dpcopy[256];
    DIR *dp, *prev_dp, *last_dp;
    char *cp, *last_cp, *start, *end;
    INODE *ip = &parent->INODE;

    //search parent INODE's data block(s) for the entry of name
    for (int i = 0; i < 12; i++) {
        //erase name entry from parent directory
        if (ip->i_block[i] != 0) {
            get_block(parent->dev, ip->i_block[i], buf);

            dp = (DIR *)buf;
            cp = buf;

            //search all entries in each block
            while(cp < buf + BLKSIZE) {
                strncpy(dpcopy, dp->name, dp->name_len);
                dpcopy[dp->name_len] = 0;

                if (strcmp(dpcopy, name) == 0) {

                    //if child is the first entry
                    if (cp == buf && cp + dp->rec_len == buf + 1024) {
                        parent->INODE.i_size -= BLKSIZE;
                        bdealloc(parent->dev, ip-> i_block[i]);

                        ip->i_size -= BLKSIZE;
                        while (ip->i_block[i + 1] && i + 1 < 12) {
                            i++;
                            get_block(parent->dev, ip->i_block[i], buf);
                            put_block(parent->dev, ip->i_block[i - 1], buf);
                        }
                    }

                    //if child is the last entry
                    else if (cp +dp->rec_len == buf + BLKSIZE) {
                        prev_dp->rec_len += dp->rec_len;
                        put_block(parent->dev, ip->i_block[i], buf);
                    }

                    //child is between first and last entry
                    else {
                        last_dp = (DIR *)buf;
                        last_cp = buf;

                        while (last_cp + last_dp->rec_len < buf + BLKSIZE) {
                            last_cp += last_dp->rec_len;
                            last_dp = (DIR *)last_cp;
                        }

                        last_dp->rec_len += dp->rec_len;

                        start = cp + dp->rec_len;
                        end = buf + BLKSIZE;

                        memmove(cp, start, end - start);

                        put_block(parent->dev, ip->i_block[i], buf);
                    }

                    //write parent's data block back to disk
                    parent->dirty = 1;
                    iput(parent);
                    return 0;
                }
        
                prev_dp = dp;
                cp += dp->rec_len;
                dp = (DIR *)cp;
            }
        }
    }
    printf("child %s not found\n", name);
    return -1;
}

int checkEmpty(MINODE *mip) {
    char buf[BLKSIZE], blockName[256];
    INODE *ip;
    char *cp;
    DIR *dp;

    ip = &mip->INODE;

    printf("Checking DIR contents\n");

    if (ip->i_links_count > 2) {
        printf("link count > 2\n");
        return 0;
    }

    //traverse data blocks for number of entries = 2
    else if (ip->i_links_count == 2) {
        for (int i = 0; i < 12; i++) {
            if (ip->i_block[i] != 0) {
                get_block(mip->dev, ip->i_block[i], buf);

                dp = (DIR *)buf;
                cp = buf;
                while (cp < buf + BLKSIZE) {
                    strncpy(blockName, dp->name, dp->name_len);
                    blockName[dp->name_len] = 0;
                    if (strcmp(blockName, ".") != 0 && strcmp(blockName, "..") != 0) {
                        return 0;
                    }
                    cp += dp->rec_len;
                    dp = (DIR *)cp;
                }
            }
        }
    }
    printf("returning 1\n");
    return 1;
}
