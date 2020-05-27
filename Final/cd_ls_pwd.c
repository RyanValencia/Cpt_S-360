/************* cd_ls_pwd.c file **************/

int chdir(char *pathname)   
{
  printf("chdir %s\n", pathname);
  //printf("under construction READ textbook HOW TO chdir!!!!\n");
  // READ Chapter 11.7.3 HOW TO chdir
  
  int ino;
  MINODE *mip;
  INODE *ip;

  //check if pathname specified or not. If not go to root. If so go to pathname.
  if(strcmp(pathname,"") == 0) running->cwd = root;
  else {
    ino = getino(pathname);

    if(!ino) {
      printf("dir not found");
      return -1;
    }

    mip = iget(dev, ino);
    ip = &mip->INODE;

    // verify mip->INODE is a dir
    if(!S_ISDIR(ip->i_mode)) {
      printf("%s is not a dir\n", pathname);
      return -1;
    }

    //release old cwd and change cwd to mip
    iput(running->cwd);
    running->cwd = mip;
  }
}

int print_file(int ino, char name[]) {
  MINODE *mip;
	INODE *ip;
	char *time, buf[BLKSIZE];
	int i;
  //printf("before setting time\n");

	mip = iget(dev, ino);
	ip = &mip->INODE;

  //strcpy(time, ctime((time_t *)&ip->i_mtime));

  //printf("after setting time\n");
	
  //time[strlen(time) - 1] = 0;

	printf("  ");

	if (S_ISDIR(ip->i_mode)) printf("d");
	else if (S_ISLNK(ip->i_mode)) {
    printf("l");
  }
	else {
    printf("-");
  }

  printf( (ip->i_mode & S_IRUSR) ? "r" : "-");
  printf( (ip->i_mode & S_IWUSR) ? "w" : "-");
  printf( (ip->i_mode & S_IXUSR) ? "x" : "-");
  printf( (ip->i_mode & S_IRGRP) ? "r" : "-");
  printf( (ip->i_mode & S_IWGRP) ? "w" : "-");
  printf( (ip->i_mode & S_IXGRP) ? "x" : "-");
  printf( (ip->i_mode & S_IROTH) ? "r" : "-");
  printf( (ip->i_mode & S_IWOTH) ? "w" : "-");
  printf( (ip->i_mode & S_IXOTH) ? "x" : "-");

	//printf(" %d  %d  %d  %d  %s  %s",
  //    ip->i_uid, ip->i_gid, ip->i_links_count, ip->i_size, time, name);
   
  printf(" %d  %d  %d  %d  \t%s",
    ip->i_uid, ip->i_gid, ip->i_links_count, ip->i_size, name);


	if (S_ISLNK(ip->i_mode)) {
		get_block(mip->dev, ip->i_block[0], buf);
		printf(" -> %s\n", buf);
	}
	else
		printf("\n");   
}

int ls(char *pathname) {
  int ino;
  MINODE *mip;
  char buf[BLKSIZE],dpcopy[256];
  DIR *dp;
  char *cp;
  INODE *ip;
	
  if(strcmp(pathname,"") == 0) ino = running->cwd->ino;
  else ino = getino(pathname);

  if(ino == 0) {
    printf("ino does not exist\n");
    return 0;
  }
    
  mip = iget(dev,ino);
  ip = &mip->INODE;
  printf("%s found, ino: %d = base bno: %d\n", pathname, ino, ip->i_block[0]);

  for(int i = 0; i < 12; i++) {
    if(ip->i_block[i] == 0) return 0;

    get_block(dev,ip->i_block[i],buf);
    dp = (DIR *)buf;
    cp = buf;

    while(cp < buf + BLKSIZE) {
      strncpy(dpcopy,dp->name,dp->name_len);
      dpcopy[dp->name_len] = 0;
      print_file(dp->inode, dpcopy);
      //printf("[%d %s]  ", dp->inode, dpcopy); // print [inode# name]
      cp += dp->rec_len;
      dp = (DIR *)cp;
    }
  }
}

pwd(MINODE *wd) {
  if(wd == root) printf("/");
  else rpwd(wd);
  printf("\n");
}

rpwd(MINODE *wd) {
  int myino, parent_ino;
  char myname[256], buf[BLKSIZE];
  char *cp;
  DIR *dp;
  MINODE *pip;

  if(wd == root) return;

  get_block(dev,wd->INODE.i_block[0],buf);
  dp = (DIR *)buf;
  cp = buf;
  myino = dp->inode;
  cp += dp->rec_len;
  dp = (DIR *)cp;
  parent_ino = dp->inode;


  //parent_ino = findino(wd, &myino);
  pip = iget(dev, parent_ino);

  rpwd(pip);

  if(findmyname(pip, myino, myname) == 0) {
    return -1;
  }

  //rpwd(pip);
  printf("/%s", myname);
}