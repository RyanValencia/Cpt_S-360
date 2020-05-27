/* /************* open_close_lseek.c file **************/

int open_file(char *filename, char *flags) {
    MINODE *mip;
    INODE *ip;
    OFT *file;
    int ino, dev, mode;

    //1. ask for a pathname and mode to open: You may use mode = 0|1|2|3 for R|W|RW|APPEND

    //get file's minode    
    if (filename[0]=='/') dev = root->dev;          // root INODE's dev
    else dev = running->cwd->dev;  
    ino = getino(filename);

    mip = iget(dev, ino);
    ip = &mip->INODE;

    if(!S_ISREG(ip->i_mode)) {
        printf("Cannot open, %s is not a REGULAR file\n", filename);
        return -1;
    }

    //Check whether the file is ALREADY opened with INCOMPATIBLE mode:
    //we can check the mode given by using flags; REMINDER: it's stored as a char so a switch isn't the simpler route
    
    if (strcmp(flags, "0") == 0) mode = 0;
    else if (strcmp(flags, "1") == 0) mode = 1;
    else if (strcmp(flags, "2") == 0) mode = 2;
    else if (strcmp(flags, "3") == 0) mode = 3;
    else {
        printf("invalid mode - 0|1|2|3 for R|W|RW|APPEND\n");
        return -1;
    }
 
    if(checkOpen(mip) == 1) {
        printf("File already open and is not R-ONLY\n");
        return -1;
    }
    else if(checkOpen(mip) == 2) {
        printf("File is already open but is R-ONLY\n");
    }
    else printf("File is not already open\n");

    //allocate a FREE OpenFileTable (OFT) and fill in values
    file = (OFT *)malloc(sizeof(OFT));
    file->mode = mode;  // mode = 0|1|2|3 for R|W|RW|APPEND 
    file->refCount = 1;
    file->mptr = mip;  // point at the file's minode[]

    switch(mode) {
        case 0 : file->offset = 0;     // R: offset = 0
                  break;
        case 1 : truncate(mip);        // W: truncate file to 0 size
                  file->offset = 0;
                  break;
        case 2 : file->offset = 0;     // RW: do NOT truncate file
                  break;
        case 3 : file->offset =  mip->INODE.i_size;  // APPEND mode
                  break;
        default: printf("invalid mode - 0|1|2|3 for R|W|RW|APPEND\n");
                  return -1;
    }

    //find the SMALLEST i  in running PROC's fd[] such that fd[i] is NULL
    for (int i = 0; i < NFD; i++) {
        if (running->fd[i] == NULL) {
            running->fd[i] = file;
            if (mode != 0) ip->i_mtime = time(0L);
            ip->i_atime = time(0L);
            mip->dirty = 1;
            iput(mip);
            printf("%s opened with fd: %d in mode %d (0|1|2|3 -> R|W|RW|APPEND)\n", filename, i, mode);
            return i;
        }
    }

    printf("Cannot open, no space in fd array\n");
    ip->i_atime = time(0L);
    iput(mip);
    return -1;
}

//check if the file is open and if so, matches the R mode
int checkOpen(MINODE *mip) {
    for (int i = 0; i < NFD; i++) {
        if (running->fd[i] != NULL) {
            if (running->fd[i]->mptr->ino == mip->ino 
            && running->fd[i]->mode != 0 
            && running->fd[i]->mptr->refCount > 0) {
                return 1; //file is open and incorrect mode
            }
            else if (running->fd[i]->mptr->ino == mip->ino 
            && running->fd[i]->mode == 0 
            && running->fd[i]->mptr->refCount > 0) {
                return 2; // file is open and mode is R
            }
        }
    }
    return 0; // file not open
}

int close_file(int fd) {
    MINODE *mip;
    INODE *ip;
    OFT *file;
    
    //verify fd is within range
    if(fd < 0 || fd >= NFD) {
        printf("invalid fd: %d\n", fd);
        return -1;
    }

    //verify running->fd[fd] is pointing at an OFT entry
    if(!running->fd[fd]) {
        printf("fd %d not open\n", fd);
        return -1;
    }

    file = running->fd[fd];
    running->fd[fd] = 0;
    file->refCount--;
    if (file->refCount > 0) return 0;
    // last user of this OFT entry ==> dispose of the Minode[]
    mip = file->mptr;
    mip->INODE.i_atime = time(0L); //need to update last access time.
    iput(mip);
    return 0; 
}

//can't call it "lseek()", function already exists
int mylseek(int fd, int pos) {
    MINODE *mip;
    INODE *ip;
    OFT *file;
    int originalOffset, isize;

    //change OFT entry's offset to position but make sure NOT to over run ends of file
    //make sure the fd and pos given are valid, and the file is open
    if (fd >= NFD) {
        printf("fd %d is invalid\n", fd);
        return -1;
    }

    if (!running->fd[fd]) {
        printf("fd %d is not open\n", fd);
        return -1;
    }

    isize = running->fd[fd]->mptr->INODE.i_size;
    if (pos > isize) {
        printf("position %d is out of range; i_size is %d\n", pos, isize);
        return -1;
    }

    originalOffset = running->fd[fd]->offset;
    running->fd[fd]->offset = pos;
    return originalOffset;
}

//print file descriptors
int pfd() {
    char *modechar;
    printf("fd     mode     offset    INODE\n---    ----     ------    --------\n");
    for(int i = 0; i < NFD; i++) {
        if(running->fd[i]) {
            if (running->fd[i]->mode == 0) modechar = "READ  ";
            else if (running->fd[i]->mode == 1) modechar = "WRITE ";
            else if (running->fd[i]->mode == 2) modechar = "RDWR  ";
            else if (running->fd[i]->mode == 3) modechar = "APPEND";
            printf(" %d     %s      %d      [%d, %d]\n",
            i, modechar, running->fd[i]->offset, running->fd[i]->mptr->dev, running->fd[i]->mptr->ino);
        }
    }
    printf("----------------------------------------\n");
}