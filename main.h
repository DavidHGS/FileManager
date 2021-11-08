#include<unistd.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<wait.h>
#define InodeNum 1024//i节点数目
#define BlkNum (80*1024)//磁盘块数目
#define BlkSize 1024//磁盘块大小为1k
#define BlkPerNode 1024//每个文件包含的最大磁盘块数目
#define DISK "Disk"
#define BUFF "buff"//读写文件时的缓冲文件
#define SuperBeg 0//超级块的起始地址
#define InodeBeg sizeof(SuperBlk)//i节点区的起始地址
#define BlockBeg (InodeBeg+InodeNum*sizeof(Inode))//数据区起始地址
#define DirPerBlk (BlkSize/sizeof(Dir))//每个磁盘块包含的最大目录项 32
#define MaxDirNum (BlkPerNode*DirPerBlk)//每个目录最大文件数 1024*32
#define Directory 0
#define File 1
#define CommanNum (sizeof(command)/sizeof(char*))//指令数目

typedef struct {
    int inode_map[InodeNum];//i节点位图
    int blk_map[BlkNum];//磁盘块位图
    int inode_used;//已被使用的i节点数目
    int blk_used;//已被使用的磁盘块数目
}SuperBlk;//(1024+80*1024+2)*4Bytes=331784B=0X51008

typedef struct{
    int blk_used[BlkPerNode];//占用的磁盘块编号
    int blk_num;//占用磁盘块数目
    int file_size;
    int file_type;
}Inode;//1027*4Bytes=4108B

typedef struct{
    char name[30];
    short inode_num;//目录对应的inode
}Dir;//32byte

class fs{
public:
    Dir dir_table[MaxDirNum];//将当前目录文件的内容都载入内存
    int dir_num;//当前目录的目录项数
    int inode_num;//当前目录的inode号
    Inode curr_inode;//当前目录的inode节点
    SuperBlk super_blk;//文件系统的超级块
    FILE* Disk;//模拟磁盘文件
    char path[40]="@david:root";
    int init_fs();//初始化文件系统
    int close_fs();//关闭文件系统
    int format_fs();//低级格式化文件系统（格式化superblk，并还原根目录）
    int get_blk();//获得一个未使用的磁盘块号
    int init_fileinode(int);//初始化文件inode
    int init_dirinode(int ichild,int ifather);//初始化目录inode，分配一个磁盘块存放目录
    int myapplyinode();//获得一个inode节点号
    int myfreeinode(int inode);//释放inode节点及其所使用的磁盘块号
    int checkFilename(char *name);//找到当前目录下的文件对应的inode号
    int checkType(char *name);//检查文件类型
    int mycreatefile(int inode,char *name,int type);//创建文件
    int myreadfile(char *name);//读文件到副本文件(buff)
    int mywritefile(char *name);//从副本文件中写回到Disk
    int mydelfile(int inode,char *name,int deepth);//递归删除目录
    int adjustdir(char *name);//调整删除后的目录
    int myopendir(int inode);//将inode所对应的目录设置为当前目录
    int myclosedir(int inode);//关闭当前目录,并将其对应的inode和磁盘数据写回磁盘
    int myshowdir(int inode);//显示inode所对应的目录中的所有文件名
    int myenterdir(int inode,char *name);//进入到inode对应的文件中name对应的目录文件
    void change_path(char *name);//更新路径信息
    void fdisk();//显示分区信息
};