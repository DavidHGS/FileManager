#include"main.h"
int fs::init_fs(){
    //读取超级快到内存d
    fseek(Disk,SuperBeg,SEEK_SET);
    fread(&super_blk,sizeof(SuperBlk),1,Disk);
    inode_num=0;//当前目录,即根目录的inode值为0
    if(!myopendir(inode_num)){
        printf("Can't open root directory\n");
        return 0;
    }
    return 1;
}
int fs::close_fs(){
    fseek(Disk,SuperBeg,0);
    fwrite(&super_blk,sizeof(SuperBlk),1,Disk);//写回到模拟磁盘
    myclosedir(inode_num);
    return 1;
}

//格式化超级块（低级格式化）
int fs::format_fs(){
    /*格式化inode_map,保留根目录*/
    memset(super_blk.inode_map,0,sizeof(super_blk.inode_map));
    super_blk.inode_map[0]=1;
    super_blk.inode_used=1;
    /*格式化blk_map，第一个磁盘块给根目录*/
    memset(super_blk.blk_map,0,sizeof(super_blk.blk_map));
    super_blk.blk_map[0]=1;
    super_blk.blk_used=1;

    inode_num=0;//起始目录为根目录
    /*读取根目录文件*/
    fseek(Disk,InodeBeg,SEEK_SET);
    fread(&curr_inode,sizeof(Inode),1,Disk);
    curr_inode.file_size=2*sizeof(Dir);
    curr_inode.blk_used[0]=0;
    curr_inode.blk_num=1;
    curr_inode.file_type=Directory;
    //根目录信息写回
    fseek(Disk,InodeBeg,0);
    fwrite(&curr_inode,sizeof(Inode),1,Disk);

    dir_num=2;
    strcpy(dir_table[0].name,".");
    strcpy(dir_table[1].name,"..");//根节点的父目录也是其本身
    dir_table[0].inode_num=dir_table[1].inode_num=0;

    strcpy(path,"@david:root");
    return 1;
}

/***************************磁盘块操作************************************/


int fs::get_blk(){//申请未使用的磁盘块
    int i;
    super_blk.blk_used++;
    for(int i=0;i<BlkNum;i++){
        if(super_blk.blk_map[i]<1){
            super_blk.blk_map[i]=1;
            return i;
        }
    }
    return -1;//磁盘块用完返回-1
}
/***************************磁盘块操作************************************/

/***************************inode操作************************************/
int fs::init_fileinode(int inode){
    Inode tmp;
    fseek(Disk,InodeBeg+sizeof(Inode)*inode,0);
    fread(&tmp,sizeof(Inode),1,Disk);

    tmp.blk_num=0;
    tmp.file_type=File;
    tmp.file_size=0;
    
    //写回Inode
    fseek(Disk,InodeBeg+sizeof(Inode)*inode,0);
    fwrite(&tmp,sizeof(Inode),1,Disk);

    return 1;
}
int fs::init_dirinode(int ichild,int ifather){
    Inode tmp;
    Dir dir[2];
    fseek(Disk,InodeBeg+sizeof(Inode)*ichild,0);
    fread(&tmp,sizeof(Inode),1,Disk);

    int blk_pos=get_blk();

    tmp.blk_num=1;
    tmp.blk_used[0]=blk_pos;
    tmp.file_size=2*sizeof(Dir);
    tmp.file_type=Directory;
    //写回Inode
    fseek(Disk,InodeBeg+sizeof(Inode)*ichild,0);
    fwrite(&tmp,sizeof(Inode),1,Disk);

    strcpy(dir[0].name,".");
    dir[0].inode_num=ichild;

    strcpy(dir[1].name,"..");
    dir[1].inode_num=ifather;
    //写回磁盘
    fseek(Disk,BlockBeg+BlkSize*blk_pos,0);
    fwrite(dir,sizeof(Dir),2,Disk);

    return 1;

}

int fs::myapplyinode(){//申请inode节点
    int i;
    if(super_blk.inode_used>=InodeNum){
        printf("inode used up\n");
        return -1;
    }
    super_blk.inode_used++;

    for(int i=0;i<InodeNum;i++){
        if(super_blk.inode_map[i]<1){//找到一个未用的inode
            super_blk.inode_map[i]=1;
            return i;
        }
    }
}

int fs::myfreeinode(int inode){
    Inode tmp;
    int i;
    fseek(Disk,InodeBeg+sizeof(Inode)*inode,0);
    fread(&tmp,sizeof(Inode),1,Disk);
    //释放磁盘块
    for(int i=0;i<tmp.blk_num;i++){
        super_blk.blk_used--;
        super_blk.blk_map[tmp.blk_used[i]];
    }
    super_blk.inode_map[inode]=0;
    super_blk.inode_used--;
    return 1;
}



int fs::checkFilename(char *name){//检查目录文件是否重复 返回当前目录文件inode
    for(int i=0;i<dir_num;i++){
        if(strcmp(name,dir_table[i].name)==0){
            return dir_table[i].inode_num;
        }
    }
    return -1;
}

int fs::checkType(char *name){
    Inode tmp;
    int inode;
    for(int i=0;i<dir_num;i++){
        if(strcmp(name,dir_table[i].name)==0){
            inode=dir_table[i].inode_num;
            fseek(Disk,InodeBeg+inode*sizeof(Inode),0);
            fread(&tmp,sizeof(Inode),1,Disk);
            return tmp.file_type;
        }
    }
    return -1;
}


int fs::mycreatefile(int inode,char *name,int type){
    int new_inode;
    int blk_need=0;//本目录需要增加磁盘块则blk_need=1;
    int t;
    //目录项超过最大限制
    if(dir_num>MaxDirNum){
        printf("mkdir fail\n");
        return 0;
    }

    if(checkFilename(name)!=-1){
        printf("create error: file exist\n");
        return 0;
    }
    //需新增磁盘
    if(dir_num/DirPerBlk<(dir_num+1)/DirPerBlk){
        blk_need=1;
    }
    //磁盘块用完
    if(super_blk.blk_used+blk_need>BlkNum){
        printf("block has used up\n");
        return -1;
    }
    if(blk_need==1){//需要增加磁盘块
        t=curr_inode.blk_num++;
        curr_inode.blk_used[t]=get_blk();
    }

    new_inode=myapplyinode();
    //inode 使用完
    if(new_inode==-1){
        printf("inode has used up\n");
        return -1;
    }
    if(type==Directory){
        init_dirinode(new_inode,inode);
    }else if(type==File){
        init_fileinode(new_inode);
    }

    //写入信息到当前目录（内存中）
    strcpy(dir_table[dir_num].name,name);
    dir_table[dir_num++].inode_num=new_inode;
}

int fs::myreadfile(char *name){
    int inode=checkFilename(name),file_blknum;
    Inode tmp;
    char buff[BlkSize];//内存用户缓冲区
    FILE *fp=fopen(BUFF,"w+");//文件缓冲区 副本文件
    //根据inode读出inode节点
    fseek(Disk,InodeBeg+sizeof(Inode)*inode,0);
    fread(&tmp,sizeof(Inode),1,Disk);

    if(tmp.blk_num==0){//空文件关闭
        fclose(fp);
        return 1;
    }
    printf("read\n");

    for(int i=0;i<tmp.blk_num-1;i++){
        file_blknum=tmp.blk_used[i];
        //读出文件包含的磁盘块到内存
        fseek(Disk,BlockBeg+BlkSize*BlkNum,0);
        fread(buff,1,BlkSize,Disk);
        //写入文件缓冲区
        fwrite(buff,1,BlkSize,fp);
        //释放磁盘块
        super_blk.blk_used--;
        super_blk.blk_map[file_blknum]=0;
        tmp.file_size-=BlkSize;
    }
    //读最后一个磁盘块（可能未满）
    file_blknum=tmp.blk_used[tmp.blk_num-1];
    fseek(Disk,BlockBeg+BlkSize*file_blknum,0);
    fread(buff,1,tmp.file_size,Disk);
    fwrite(buff,1,tmp.file_size,fp);
    //释放最后应该磁盘块
    super_blk.blk_used--;
    super_blk.blk_map[file_blknum]=0;

    //修改inode节点信息
    tmp.file_size=0;
    tmp.blk_num=0;
    
    //将修改后的inode节点信息写回磁盘
    fseek(Disk,InodeBeg+sizeof(Inode)*inode,0);
    fwrite(&tmp,sizeof(Inode),1,Disk);
    fclose(fp);
    return 1;
}


int fs::mywritefile(char *name){
    Inode tmp;
    int inode=checkFilename(name),num,file_blknum;
    char buff[BlkSize];
    FILE *fp=fopen(BUFF,"w");
    fseek(Disk,InodeBeg+sizeof(Inode)*inode,0);
    fread(&tmp,sizeof(Inode),1,Disk);

    while((num=fread(buff,1,BlkSize,fp))>0){//从副本文件读入buff，并写回磁盘
        printf("num:%d\n",num);

        if((file_blknum=get_blk())<0){
            printf("blk error: block used up\n");
            break;
        }

        //副本信息写回磁盘
        fseek(Disk,BlockBeg+BlkSize*file_blknum,0);
        fwrite(buff,1,num,Disk);
        //更新inode节点状态
        tmp.blk_used[tmp.blk_num]=file_blknum;
        tmp.blk_num++;
        tmp.file_size+=num;
    }
    //inode节点写回磁盘
    fseek(Disk,InodeBeg+sizeof(Inode)*inode,0);
    fread(&tmp,1,sizeof(Inode),Disk);
    fclose(fp);
    return 1;
}

int fs::mydelfile(int inode,char *name,int deepth){//递归删除
    Inode tmp;
    if(!strcmp(name,".")||!strcmp(name,"..")){
        printf("can't del \"..\" and \".\"\n");
        return 0;
    }

    int ichild=checkFilename(name);//待删除文件的inode 是inode下的子目录

    if(ichild==-1){
        printf("rmdir %s failed :no such file or directory\n",name);
    }

    fseek(Disk,InodeBeg+sizeof(Inode)*ichild,0);
    fread(&tmp,sizeof(Inode),1,Disk);

    if(tmp.file_type=File){//是文件则释放
        myfreeinode(inode);
        if(deepth==0){//到最上层目录 整理目录
            adjustdir(name);
        }
        return 1;
    }else{//是目录则进入下一层
        myenterdir(inode,name);
    }
    
    for(int i=2;i<dir_num;++i){//递归删除该层目录内容
        mydelfile(ichild,dir_table[i].name,deepth+1);
    }

    myenterdir(ichild,"..");//返回上层
    myfreeinode(ichild);

    if(deepth==0){
        if(dir_num/DirPerBlk<(dir_num-1)/DirPerBlk){//需要释放inode中占用的最后一个磁盘块
            curr_inode.blk_num--;
            int k=curr_inode.blk_used[curr_inode.blk_num];
            super_blk.blk_used--;
            super_blk.blk_map[k]=0;
        }
        adjustdir(name);
    }
}

int fs::adjustdir(char *name){
    int i;
    for(i=0;i<dir_num;i++){
        if(strcmp(name,dir_table[i].name)==0){
            break;
        }
    }
    for(i++;i<dir_num;i++){
        dir_table[i-1]=dir_table[i];
    }
    dir_num--;
    return 1;
}

int fs::myopendir(int inode){
    int dircnt=0;
    int left_dirnum;
    int i;
    //读出inode节点
    fseek(Disk,InodeBeg+sizeof(Inode)*inode,0);
    fread(&curr_inode,sizeof(Inode),1,Disk);
    //读出磁盘块内容
    for(i=0;i<curr_inode.blk_num-1;i++){
        fseek(Disk,BlockBeg+BlkSize*curr_inode.blk_used[i],0);
        fread(dir_table+dircnt,sizeof(Dir),DirPerBlk,Disk);
        dircnt+=DirPerBlk;
    }
    left_dirnum=curr_inode.file_size/sizeof(Dir)-DirPerBlk*(curr_inode.blk_num-1);
    fseek(Disk,BlockBeg+BlkSize*curr_inode.blk_used[i],0);
    fread(dir_table+dircnt,sizeof(Dir),left_dirnum,Disk);
    dircnt+=left_dirnum;
    dir_num=dircnt;//更新当前目录目录项数
    inode_num=inode;//更新当前目录对应的inode号
    return 1;
}
int fs::myclosedir(int inode){
    int dircnt=0;
    int i;

    //磁盘写回
    for(i=0;i<curr_inode.blk_num-1;i++){
        fseek(Disk,BlockBeg+BlkSize*curr_inode.blk_used[i],0);
        fwrite(dir_table+dircnt,sizeof(Dir),DirPerBlk,Disk);
        dircnt+=DirPerBlk;
    }
    int left_dirnum=dir_num-dircnt;
    fseek(Disk,BlockBeg+BlkSize*curr_inode.blk_used[i],0);
    fwrite(dir_table+dircnt,sizeof(Dir),left_dirnum,Disk);

    //inode写回
    curr_inode.file_size=dir_num*sizeof(Dir);
    fseek(Disk,InodeBeg+inode*sizeof(Inode),0);
    fwrite(&curr_inode,sizeof(Inode),1,Disk);
    return 1;
}
int fs::myshowdir(int inode)
{
	int i,color=32;
	for(i=0;i<dir_num;++i){
		if(checkType(dir_table[i].name)==Directory){
			/*目录显示绿色*/
			printf("\033[1;%dm%s\t\033[0m", color,dir_table[i].name);
		}
		else{
			printf("%s\t",dir_table[i].name);
		}
		if(!((i+1)%6)) printf("\n");//6个一行
	}
	printf("\n");
 
	return 1;
}


int fs::myenterdir(int inode,char *name){
    int ichild=checkFilename(name);
    if(ichild==-1){
        printf("cd:%s, No such file or Directory\n",name);
        return -1;
    }

    myclosedir(inode);
    myopendir(ichild);
    return 1;
}


void fs::change_path(char *name){
    int pos;
    if(strcmp(name,".")==0){
        return ;
    }
    else if(strcmp(name,"..")==0){//返回上层目录，要去掉最后一个'/'
        pos=strlen(path)-1;
        for(;pos>=0;pos--){
            if(path[pos]!='/'){
                path[pos]='\0';
            }else{
                path[pos]='\0';//去掉最后一个'/'
                break;
            }
        }
    }else{//否则在路径末尾添加子目录
        strcat(path,"/");
        strcat(path,name);
    }
    return ;
}

void fs::fdisk(){
    printf("Partition\t%-10s\t%-10s\t%-10s\t%-10s\t%-10s\t\n","used","start","end","size","Id");
    printf("superblk area\t%-10d\t%-10d\t%-10d\t%-10d\t%-10d\t\n",super_blk.blk_used*4+super_blk.inode_used*4+2*4,SuperBeg,InodeBeg,InodeBeg-SuperBeg,0);
    printf("Inode area\t%-10d\t%-10d\t%-10d\t%-10d\t%-10d\t\n",sizeof(Inode)*super_blk.inode_used,InodeBeg,BlockBeg,BlockBeg-InodeBeg,1);
    printf("Data area\t%-10d\t%-10d\t%-10d\t%-10d\t%-10d\t\n",BlkSize*super_blk.blk_used,BlockBeg,100*1021*1024,100*1024*1024-BlockBeg,2);
}

//指令集合
char* command[]={"fmt","quit","mkdir","rmdir","cd","ls","mk","rm","vim","fdisk-l"};

int main(){
    char comm[30],name[30];
    char *arg[]={"vim",BUFF,NULL};
    int i,quit=0,choice,status;
    fs fs;
    fs.Disk=fopen(DISK,"r+");
    
    fs.init_fs();
    while(1){
        printf("\033[1;34m%s#\033[0m",fs.path);
        scanf("%s",comm);
        choice =-1;
        for(i=0;i<CommanNum;i++){
            if(strcmp(comm,command[i])==0){
                choice=i;
                break;
            }
        }
        switch(choice){
            /*格式化文件系统*/
            case 0: fs.format_fs();//格式化文件系统
                    break;
            case 1: quit=1;//退出
                    break;
            case 2: scanf("%s",name);//创建子目录
                    fs.mycreatefile(fs.inode_num,name,Directory);
                    break;
            case 3: scanf("%s",name);//删除子目录
                    if(fs.checkType(name)!=Directory)
                        printf("remove dir %s fail : not a  dir\n",name);
                    else 
                        fs.mydelfile(fs.inode_num,name,0);
                    break;
            case 4: scanf("%s",name);//改变路径
                    if(fs.checkType(name)!=Directory)
                        printf("cd %s fail: Not a dir\n",name);
                    else
                        if(fs.myenterdir(fs.inode_num,name)){
                            fs.change_path(name);
                        }
            case 5: fs.myshowdir(fs.inode_num);//显示当前目录内容
                    break;
            case 6: scanf("%s",name);//创建文件
                    fs.mycreatefile(InodeNum,name,File);
                    break;
            case 7: scanf("%s",name);//删除文件
                    if(fs.checkType(name)!=File)
                        printf("del %s error: Not a file\n",name);
                    else
                        fs.mydelfile(fs.inode_num,name,0);
                    break;
            case 8: scanf("%s",name);//对文件进行编辑
                    if(fs.checkType(name)!=File){
                        printf("edit %s error: Not a file\n",name);
                    }else
                    fs.myreadfile(name);//将数据写到副本
                    if(fork()==0){//子进程编写文件
                        execvp("vim",arg);
                    }
                    wait(&status);//父进程等待子进程结束
                    fs.mywritefile(name);//副本信息写回磁盘
                    break;
            case 9: fs.fdisk();//显示磁盘信息
                    break;
            default:
                    printf("%s command not found\n",comm);//其他操作跳出当前循环
                    break;
        }
        if(quit) break;
    }
    // Inode tmp;
    // tmp.blk_num=1;
    // tmp.blk_used[0]=2;
    // tmp.file_size=1024;
    // fseek(Disk,InodeBeg,0);
    // fwrite(&tmp,sizeof(Inode),1,Disk);

    // Inode tmp2;
    // fseek(Disk,InodeBeg+sizeof(Inode),0);
    // fread(&tmp2,sizeof(Inode),1,Disk);

    fs.close_fs();
    fclose(fs.Disk);
    return 0;
}
