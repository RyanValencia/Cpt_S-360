/************* read_cat.c file **************/
extern char pathname[256], pathname2[256];

int read_file() { 
    char buf[BLKSIZE];
    int fd, nbytes;
    //ASSUME: file is opened for RD or RW;

    fd = atoi(pathname);
    nbytes = atoi(pathname2);
    
    //ask for a fd  and  nbytes to read;    
    if (fd < 0 || fd >= NFD) {
        printf("cannot read file, fd %d is out of range\n", fd);
        return -1;
    }
    
    //verify that fd is indeed opened for RD or RW;
    if (!running->fd[fd]) {
        printf("cannot read file, fd %d is not open\n",fd);
        return -1;
    }

    if (running->fd[fd]->mode == 1 || running->fd[fd]->mode == 3) {
        printf("cannot read file, mode is not RD or RW\n");
        return -1;
    }

    int count = myread(fd, buf, nbytes);
    printf("\nmyread: read %d char from file descriptor %d\n", count, fd);
    return count;
}

int myread(int fd, char buf[], int nbytes) {
    MINODE *mip;
    INODE *ip;
    OFT *file;
    int avil; // avil is the available bytes in the file
    int lblk; //logical block number
    int startByte, blk, *ibuf, ind_blk, ind_offset, remain;
    int count = 0;
    char readbuf[BLKSIZE], tempbuf[BLKSIZE];
    char *cp, *cq = buf; // cq points at buf[ ]
	strcpy(buf, ""); //clear buf
    file = running->fd[fd];
    mip = file->mptr;
    ip = &mip->INODE;
    int fileSize = file->mptr->INODE.i_size;
    int offset = file->offset; 
    
    //avil = fileSize - OFT's offset
    avil = fileSize - offset;
    
    while (nbytes && avil) {
        lblk = file->offset / BLKSIZE;
        startByte = file->offset % BLKSIZE;

        if (lblk < 12){             // lblk is a direct block  
           blk = ip->i_block[lblk]; // map LOGICAL lblk to PHYSICAL blk
        }
        else if (lblk >= 12 && lblk < 256 + 12) {  //  indirect blocks 
            get_block(mip->dev, ip->i_block[12], readbuf);
            ibuf = (int *)readbuf;
            blk = *ibuf + (lblk - 12);
        }
        else{ //  double indirect blocks
            get_block(mip->dev, ip->i_block[13], readbuf);
            ind_blk = (lblk - 256 - 12) / 256;
            ind_offset = (lblk - 256 - 12) % 256;

            ibuf = (int *)readbuf + ind_blk;
            get_block(mip->dev, *ibuf, readbuf);

            ibuf = (int *)readbuf + ind_offset;
            blk = *ibuf;
        } 
        /* get the data block into readbuf[BLKSIZE] */
        //clear the buffers
        strcpy(readbuf, "");
        strcpy(tempbuf, "");
        get_block(mip->dev, blk, readbuf);

        /* copy from startByte to buf[ ], at most remain bytes in this block */
        cp = readbuf + startByte;   
        remain = BLKSIZE - startByte;   // number of bytes remain in readbuf[]

        if (nbytes < avil) {
            strncpy(tempbuf, cp, nbytes); 
            strcat(cq, tempbuf);
            file->offset += nbytes;
            count += nbytes;
            avil -= nbytes;
            nbytes -= nbytes; //need to make !nbytes
        }
        else {
            strncpy(tempbuf, cp, avil);
            strcat(cq, tempbuf);
            file->offset += avil;
            count += avil;
            avil -= avil; //need to make !avil
        }
    }
    return count;
}

int cat(char *filename) {
    char mybuf[BLKSIZE];
    int n;

    //open file for read
    int fd = open_file(filename, "0");
    if (fd < 0) return -1;
    putchar('\n');
    strcpy(mybuf, "");
    while(n = myread(fd, mybuf, BLKSIZE)) {
        mybuf[n] = '\0';             // as a null terminated string
        //printf("%s", mybuf);
        //spit out chars from mybuf[ ] but handle \n properly;
        for (int i = 0; i <= n; i++) {
           putchar(mybuf[i]);
        }
    }
    putchar('\n');
    putchar('\n');
    close_file(fd);
    return 0;
}