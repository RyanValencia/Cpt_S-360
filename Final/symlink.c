/************* symlink.c file **************/
extern char pathname[256], pathname2[256];

int symlink() {
    char buf[BLKSIZE], oldfile[60], newfile[60], *parent, *child,
     temp[60], temp2[60];
    MINODE *omip, *nmip, *pip;
    INODE *oip, *nip;
    int oino, nino, pino;

    // ASSUME: oldfile has <= 60 chars, including the ending NULL byte
    // making an error precaution anyways so it works when i demo it
    if (strlen(pathname) > 60 || strlen(pathname2) > 60) {
        printf("symlink failed. One or both pathnames exceeds 60 chars.\n");
        return -1;
    }

    strcpy(oldfile, pathname);
    strcpy(newfile, pathname2);

    // verify oldfile exists (either a DIR or a REG file)
    oino = getino(oldfile);

    if (!oino) {
        printf("%s does not exist\n", oldfile);
        return -1;
    }

    omip = iget(dev, oino);
    oip = &omip->INODE;

    if (!S_ISDIR(oip->i_mode) && !S_ISREG(oip->i_mode)) {
        printf("%s is not a DIR or a REG\n", oldfile);
        return -1;
    }

    //check: newfile does not yet exist

    strcpy(temp, newfile);
    strcpy(temp2, newfile);
    parent = dirname(temp);
    child = basename(temp2);

    pino = getino(parent);
    pip = iget(omip->dev, pino);

    if (search(&pip->INODE, child)) {
        printf("%s already exists in %s\n", child, parent);
        return -1;
    }

    // creat a FILE /x/y/z
    creat_file(pathname2);

    //change /x/y/z's type to LNK
    nino = getino(newfile);
    nmip = iget(pip->dev, nino);
    nip = &nmip->INODE;

    nip->i_mode = 0120000;
    nip->i_size = strlen(oldfile);

    get_block(nmip->dev, nip->i_block[0], buf);
    strcpy(buf, oldfile);
    strcpy(nip->i_block, buf);
    put_block(nmip->dev, nip->i_block[0], buf);

    nmip->dirty = 1;
    iput(nmip);

    // mark newfile parent minode dirty
    pip->dirty = 1;
    iput(pip);
}

int readlink() {
    MINODE *mip;
    INODE *ip;
    int ino;
    char buf[BLKSIZE], *fileContents;

    // get INODE of pathname into a minode[]
    ino = getino(pathname);

    // check INODE is a symbolic Link file.
    if (!ino) {
        printf("INODE for %s does not exist\n", pathname);
        return -1;
    }

    mip = iget(dev, ino);
    ip = &mip->INODE;
    if (!S_ISLNK(ip->i_mode)) {
        printf("%s is not a symbolic link file\n", pathname);
        return -1;
    }

    // return its string contents in INODE.i_block[]
    // I'm just going to print it here instead of returning it
    get_block(mip->dev, ip->i_block[0], buf);
    fileContents = buf;
    printf("Link file contents: \n%s\n", fileContents);

    return 0;
}