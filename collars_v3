#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <mqueue.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>
#define SHOW1 0
#define SHOW2 0

static int lis_sock;

static mqd_t feeder;

struct computer
{   int sock; 
    socklen_t sock_len;
    struct sockaddr_in sock_attr;
    char ip[16];
    short port;
};
static struct computer *glo_cmp;
static int glo_cmp_Big;


struct reactor
{   fd_set r_ret;
    fd_set w_ret;
    fd_set e_ret;

    fd_set r_tar;
    fd_set w_tar;
    fd_set e_tar;
};
static struct reactor glo_rec;

void Sig_secure_quit(int sig);
void* pth_read_message();

void p1(int line,int ret)
{   if(ret==-1)
    {   perror("ret ");
        printf("line: %d\n",line);
        Sig_secure_quit(-1);
    }
}

void p2(int line,void *point)
{   if(point==NULL)
    {   perror("poin");
        printf("line: %d\n",line);
        Sig_secure_quit(-1);
    }
}

void show_str(const char *str,int sleeptime)
{   puts(str);
    sleep(sleeptime);
}

void listen_ready(short port)
{   if(port<1025)
        p1(__LINE__,-1);
    
    lis_sock=socket(AF_INET,SOCK_STREAM,0);
    p1(__LINE__,lis_sock);

    struct sockaddr_in lis_sock_attr;
    socklen_t lis_sock_len=sizeof(lis_sock_attr);

    bzero(&lis_sock_attr,lis_sock_len);
    lis_sock_attr.sin_family=AF_INET;
    lis_sock_attr.sin_addr.s_addr=INADDR_ANY;
    lis_sock_attr.sin_port=htons(port);

    int ret;
    ret=bind(lis_sock,(struct sockaddr *)&lis_sock_attr,lis_sock_len);
    p1(__LINE__,ret);

    ret=listen(lis_sock,128);
    p1(__LINE__,ret);

}

void pth_pool_ready(int quantity)
{   if(quantity<1 || 64<quantity)
        p1(__LINE__,-1);

    int ret;
    struct mq_attr mq_data;
    mq_data.mq_flags=0;
    mq_data.mq_maxmsg=10;
    mq_data.mq_msgsize=sizeof(void *);
    feeder=mq_open("/hello",O_CREAT|O_RDWR,0644,&mq_data);
    p1(__LINE__,feeder);

    pthread_t ph;
    for(int i=0; i<quantity; i++)
    {   ret=pthread_create(&ph,NULL,pth_read_message,NULL);
        p1(__LINE__,ret);
    }

}

void sel_reactor_ready()
{   FD_ZERO(&glo_rec.r_tar);
    FD_ZERO(&glo_rec.r_tar);
    FD_ZERO(&glo_rec.r_tar);
    FD_SET(lis_sock,&glo_rec.r_tar);
    
    glo_cmp=(void *)malloc(sizeof(struct computer)*1024);
    p2(__LINE__,glo_cmp);

    glo_cmp[0].sock=lis_sock;
    glo_cmp_Big=0;

    for(int i=1; i<1024; i++)
        glo_cmp[i].sock=-1;
    
}

void sig_registe_ready()
{   signal(SIGINT,Sig_secure_quit);

}

void Sig_secure_quit(int sig)
{   close(lis_sock);    //kill listen socket

    close(feeder);      //kill message queue so pthread all death 
    unlink("/hello");   //clear mqfile

    free(glo_cmp);
    exit(0);
}

void* pth_read_message()
{   int ret;
    struct computer *temporary=NULL;
    char buff[1024];
    while(1)
    {   ret=mq_receive(feeder,(void *)&temporary,sizeof(void *),0);
        if(ret==-1 && errno==EBADF)
            pthread_exit(NULL);

        ret=read(temporary->sock,buff,1024);
        if(ret>0)
        {   
            buff[ret-1]='\0';
            printf("@@ Host %s:%hu say:[%s]\n",temporary->ip, temporary->port,buff);
        }
        else if(ret==0)
        {   
            printf("## Host %s:%hu offline \n",temporary->ip,temporary->port,buff);
            
            FD_CLR(temporary->sock,&glo_rec.r_tar);
            close(temporary->sock);
            temporary->sock=-1;
        }
        else
        {   puts("pthread error");
            pthread_exit(NULL);
        }
    }
}

int main()
{   listen_ready(8104);
    show_str("# Listen socket Ready [8104]",SHOW1);

    pth_pool_ready(30);
    show_str("# Pthread pool Ready [30]",SHOW1);
    
    sel_reactor_ready();
    show_str("# Select reactor Ready [ok]",SHOW1);

    sig_registe_ready();
    show_str("# Signal registation [ok]",SHOW1);

    show_str("# All work is ready to go...",SHOW2);

    int ret,i,cli_sock,mq_ret;
    struct computer *mid;
    struct sockaddr_in temporary_attr;
    socklen_t temporary_len=sizeof(temporary_attr);
    
    system("clear");
    puts("==== Wlecom to Collar ====");
    puts("Port: 8104");
    puts("Program listening...");

    while(1)
    {   /* reset data */
        glo_rec.r_ret=glo_rec.r_tar;
        temporary_len=sizeof(temporary_attr);   
        
        // 1+3+1=5
        ret=select(glo_cmp_Big+5,&glo_rec.r_ret,NULL,NULL,NULL);
        p1(__LINE__,ret);
        
        if(FD_ISSET(lis_sock,&glo_rec.r_ret))
        {   cli_sock=accept(lis_sock,(struct sockaddr *)&temporary_attr,&temporary_len);
            p1(__LINE__,cli_sock);
            
            if(glo_cmp_Big==1023)
            {   puts("# Connection limit");
                close(cli_sock);
                continue;
            }
            for(i=0;i<1024;i++)
            {   if(glo_cmp[i].sock==-1)
                {   glo_cmp[i].sock=cli_sock;
                    glo_cmp[i].sock_attr=temporary_attr;
                    glo_cmp[i].sock_len=temporary_len;
                    inet_ntop(AF_INET,&temporary_attr.sin_addr.s_addr,glo_cmp[i].ip,16);
                    glo_cmp[i].port=ntohs(temporary_attr.sin_port);
                    break;
                }
                else
                    continue;
            }
            if(i==1024)
                p1(__LINE__,-1);
            
            if(i>=glo_cmp_Big)
                glo_cmp_Big=i;
            
            ret--;
            FD_SET(glo_cmp[i].sock,&glo_rec.r_tar);
            printf("## Host %s:%hu online \n",glo_cmp[i].ip,glo_cmp[i].port);
        }

        for(i=1;i<1024;i++)
        {   
            if(ret==0)
                break;

            if(FD_ISSET(glo_cmp[i].sock,&glo_rec.r_ret))
            {   mid=glo_cmp+i;
                mq_ret=mq_send(feeder,(void *)&(mid),sizeof(void *),0);  
                p1(__LINE__,mq_ret);
                ret--;
            }
            else
                continue;
        }
        sleep(1);

    }
    return 0;
}
