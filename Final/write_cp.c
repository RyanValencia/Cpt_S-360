/************* write_cp.c file **************/
extern char pathname[256], pathname2[256];

//write_file runs pretty similarly to read_file()
int write_file() {
    OFT *file;
    char buf[BLKSIZE], writebuf[BLKSIZE];
    int fd, nbytes;

    strcpy(writebuf, pathname2);
    fd = atoi(pathname);

    if (fd <0 || fd >= NFD) {
        printf("cannot write; fd %d is out of range\n", fd);
        return -1;
    }
    if (running->fd[fd] != 1) {
        printf("cannot write; fd %d is not open\n", fd);
        return -1;
    }

    file = running->fd[fd];

    if (file->mode == 0) {
        printf("cannot write; mode is Read-only\n");
        return -1;
    }

    nbytes = strlen(writebuf);
    return mywrite(fd, writebuf, nbytes);
}

int mywrite(int fd, char writebuf[], int nbytes) {
    MINODE *mip;
    INODE *ip;
    OFT *file;
    char buf[BLKSIZE], tempbuf[BLKSIZE];
    int startByte, blk, lblk, *ibuf, ind_blk, ind_offset, temp_block, remain;
    int bytesWritten;
    
    file = running->fd[fd];
    mip = file->mptr;
    ip = &mip->INODE;

    while (nbytes > 0) {
        blk = 0;
        lblk = file->offset / BLKSIZE;
        startByte = file->offset % BLKSIZE;

        //try writing into direct 
        if (lblk < 12) {
            if (ip->i_block[lblk] == 0) {
                ip->i_block[lblk] = balloc(mip->dev);
            }
            blk = ip->i_block[lblk];
        }
        //if direct blocks are full, write in indirect blocks
        else if (lblk >= 12 && lblk < 268) {
            //prep indirect blocks for being written into
            if (ip->i_block[12] == 0) {
                ip->i_block[12] = balloc(mip->dev);

                // make sure entire block is all 0's
                get_block(mip->dev, ip->i_block[12], buf);
                for (int i = 0; i < BLKSIZE; i++) {
                    buf[i] = 0;
                }
                put_block(mip->dev, ip->i_block[12], buf);
            }

            get_block(mip->dev, ip->i_block[12], buf);

            ibuf = (int *)buf;
            blk = ibuf[lblk - 12];
            if(blk == 0) {
                blk = balloc(mip->dev);
                ibuf[lblk - 12] = blk;
            }

            put_block(mip->dev, ip->i_block[12], buf);
        }
        // if the indirect blocks fill, use the double indirect blocks
        else {
            if(ip->i_block[13] == 0) {
                ip->i_block[13] = balloc(mip->dev);
                get_block(mip->dev, ip->i_block[13], buf);
                ibuf = (int *)buf;

                for (int i = 0; i < 256; i++) {
                    ibuf[i] = 0;
                }
                put_block(mip->dev, ip->i_block[13], buf);
            }

            get_block(mip->dev, ip->i_block[13], buf);

            ibuf = (int *)buf;

            ind_blk = (lblk - 268) / 256;
            ind_offset = (lblk - 268) % 256;

            blk = (int)ibuf[ind_blk];

            if (!blk) {
                blk = balloc(mip->dev);

                ibuf[ind_blk] = blk;

                put_block(mip->dev, ip->i_block[13], buf);
                get_block(mip-dev, blk, buf);

                for(int i = 0; i < BLKSIZE; i++) {
                    buf[i] = 0;
                }

                put_block(mip->dev, blk, buf);
            }

            get_block(mip->dev, blk, buf);
            temp_block = blk;

            ibuf = (int *)buf;
            blk = (int)ibuf[ind_offset];

            if(!blk) {
                blk = balloc(mip->dev);
                ibuf[ind_offset] = blk;
                put_block(mip->dev, temp_block, buf);
            }
            printf("Block: %d\n", blk);
        }

        char *cp = buf + startByte;
        remain = BLKSIZE - startByte;

        printf("writing to bno: %d; Start byte: %d ; N Bytes: %d; Remainder: %d; Offset: %d;\n",
         blk, startByte, nbytes, remain, file->offset);
        
        bytesWritten = nbytes;
        
        if((remain > 0) && (nbytes > 0)) {
            strncpy(tempbuf, writebuf, remain);
            strncpy(cp, tempbuf, nbytes);
            file->offset += nbytes;

            if (file->offset > ip->i_size) {
                ip->i_size += nbytes;
            }
            nbytes -= remain;
            remain -= remain;
        }

        put_block(mip->dev, blk, buf);
    }

    mip->dirty = 1;

    ip->i_mtime = time(0L);
    ip->i_atime = time(0L);
    iput(mip);

    printf("mywrite: wrote %d bytes to fd %d\n", bytesWritten, fd);
    return nbytes;
}

int copy_file() {
    int fd, *gd, nbytes;
    char buf[BLKSIZE];
    //src == pathname | dest == pathname2
    //fd open src for READ
    if (fd = open_file(pathname, "0") < 0) {
        printf("copy failed: could not open file for copying\n");
        return -1;
    }
    //gd open dest for RDWR|CREAT
    if(open_file(pathname2, "2") < 0) {
        printf("Creating file %s and opening for RDWR\n\n\n", pathname2);
        if(creat_file(pathname2) < 0) return -1;
        if(open_file(pathname2, "2") < 0) return -1;
        // when the above statement was being used to set gd, it would always be set to 0, even when it says it returned 1. so the following is a workaround
        gd = fd+1;
    }
    printf("gd: %d\n", gd);
    while ((nbytes = myread(fd, buf, BLKSIZE)) > 0) {
        mywrite(gd, buf, nbytes);
    }
    close_file(fd);
    close_file(gd);
}

int mv_file() {
    int src_ino, dest_ino;


    //verify src exists
    src_ino = getino(pathname);

    if (!src_ino) {
        printf("cannot move, src ino does not exist\n");
        return -1;
    }

    //check whether src is on the same dev as src

    //Case 1: same dev
    //link dest with src
    if(link() < 0) return -1;
    
    //unlink src
    unlink();

    //Case 2: not the same dev
    // incomplete


    printf("File moved from %s to %s\n", pathname, pathname2);
    return 0;
}