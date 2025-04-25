#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <mqueue.h>
#include <pthread.h>
#include <semaphore.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#define GLOBAL_MaxMessage 100


/* console workbench */
#define con_listen_port 8105
#define con_connect_max 10000
#define con_message_max 1024
#define con_pthread_max 100


struct Data_queue
{   int Max;
    int count;
    void **data;

    int front;
    int tail;
};

struct computer_data
{   
    char ip[16];
    short port;
};

struct computer_message
{   int mes_count;
    
    struct Data_queue *mes_q;
    struct Data_queue *mes_len_q;
};

struct global
{   
    int Max,Big;
    struct pollfd *glo_pfd;
    struct computer_data *glo_cmp;
    struct computer_message *glo_mes;
};
struct global gd;
static int connect_Max;
static int message_Max;

static int lis_sock;
static mqd_t feeder;

void* Pth_RecvMessage(void* data);
void* Pth_poll(void *data);
void Sig_secure_quit(int sig);

// static sem_t successful;

int queue_init(struct Data_queue **Q,int size)
{   *Q=(struct Data_queue *)malloc(sizeof(struct Data_queue));
    if(*Q==NULL)
        return -1;
    
    (*Q)->data=malloc(sizeof(void *)*size);
    if((*Q)->data==NULL)
    {   free(*Q);
        return -1;
    }

    (*Q)->Max=size;
    (*Q)->count=0;
    (*Q)->front=0;
    (*Q)->tail=0;
    return 0;
}

int queue_join(struct Data_queue *Q,void *indata)
{   if(Q->count==Q->Max)
        return -1;

    Q->data[Q->front]=indata;
    Q->front=(Q->front+1)%Q->Max;
    Q->count++;
    return 0;
}

void* queue_leave(struct Data_queue *Q)
{   if(Q->count==0)
        return NULL;
    
    int cur=Q->tail;
    Q->tail=(Q->tail+1)%Q->Max;
    Q->count--;
    return Q->data[cur];
}

void queue_destory(struct Data_queue **Q)
{   if(*Q==NULL)
        return ;
    free((*Q)->data);
    free(*Q);
    (*Q)=NULL;
}

void queue_rebirth(struct Data_queue **Q)
{   
    int max=(*Q)->Max;
    queue_destory(Q);
    queue_init(Q,max);
}

void hurricane(struct computer_message *me)
{   for(int i=0; i<me->mes_count; i++)
    {  
        free(me->mes_q->data[i]);
        free(me->mes_len_q->data[i]);
    }
    me->mes_count=0;
}

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

void p3(int line,int ret)
{   if(ret==-1)
    {   printf("[%#x] Fatal error\n",pthread_self());
        pthread_exit(NULL);
    }
}

void p4(int line,void *poin)
{   if(poin==NULL)
    {   printf("[%#x] Fatal error\n",pthread_self());
        pthread_exit(NULL);
    }
}

void Listen_ready(short port)
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

void Global_ready(int cmp_count,int mes_count)
{   int ret;
    connect_Max=cmp_count;
    message_Max=mes_count;
    gd.glo_cmp=NULL;
    gd.glo_pfd=NULL;
    gd.glo_mes=NULL;

    gd.glo_pfd=malloc(sizeof(struct pollfd)*cmp_count);
    p2(__LINE__,gd.glo_pfd);

    gd.glo_cmp=malloc(sizeof(struct computer_data)*cmp_count);
    if(gd.glo_cmp==NULL)
    {   free(gd.glo_pfd);
        gd.glo_pfd=NULL;
        p2(__LINE__,gd.glo_cmp);
    }
    
    gd.glo_mes=malloc(sizeof(struct computer_message)*cmp_count);
    if(gd.glo_mes==NULL)
    {   free(gd.glo_pfd);
        gd.glo_pfd=NULL;

        free(gd.glo_cmp);
        gd.glo_cmp=NULL;
        p2(__LINE__,gd.glo_mes);
    }

    gd.Max=cmp_count;
    gd.Big=-1;

    for(int i=0; i<cmp_count; i++)
    {   gd.glo_pfd[i].fd=-1;

        gd.glo_mes[i].mes_count=0;

        gd.glo_mes[i].mes_len_q=NULL;    
        ret=queue_init(&gd.glo_mes[i].mes_q,mes_count);
        p1(__LINE__,ret);
        
        ret=queue_init(&gd.glo_mes[i].mes_len_q,mes_count);
        if(ret==-1)
        {   queue_destory(&gd.glo_mes[i].mes_q);
            p1(__LINE__,ret);
        }
    }
}

void Poll_reactor_ready()
{   gd.Big=0;
    gd.glo_pfd[0].fd=lis_sock;
    gd.glo_pfd[0].events=POLLRDNORM|POLLERR;
}

void Pth_pool_ready(int pth_count)
{   int ret;
    struct mq_attr mq_data;
    mq_data.mq_flags=0;
    mq_data.mq_maxmsg=10;
    mq_data.mq_msgsize=sizeof(int);

    // ret=sem_init(&successful,0,0);
    // p1(__LINE__,ret);

    feeder=mq_open("/hello",O_CREAT|O_RDWR,0644,&mq_data);
    p1(__LINE__,feeder);

    pthread_t th;
    for(int i=0; i<pth_count; i++)
    {   ret=pthread_create(&th,NULL,Pth_RecvMessage,NULL);
        p1(__LINE__,ret);
    }
    
    ret=pthread_create(&th,NULL,Pth_poll,NULL);
    p1(__LINE__,ret);
}

void Sig_registra_ready()
{   signal(SIGINT,Sig_secure_quit);
    
}

void Sig_secure_quit(int sig)
{   close(lis_sock);
    close(feeder);
    for(int i=0; i<=gd.Big; i++)
    {   if(gd.glo_pfd[i].fd!=-1)
            close(gd.glo_pfd[i].fd);
    }

    for(int i=0;i<gd.Max;i++)
    {   if(gd.glo_mes[i].mes_count>0)
            hurricane(&gd.glo_mes[i]);
    }

    for(int i=0; i<connect_Max; i++)
    {   queue_destory(&gd.glo_mes[i].mes_q);
        queue_destory(&gd.glo_mes[i].mes_len_q);
    }

    free(gd.glo_cmp);
    free(gd.glo_pfd);
    free(gd.glo_mes);

    mq_unlink("/hello");
    // sem_destroy(&successful);
    exit(0);
}

void* Pth_RecvMessage(void* data)
{   pthread_detach(pthread_self());

    struct computer_message *mes;
    int cur,ret;
    char mes_tbuf[1024];
    int *size;
    char *mes_buf;
    
    while(1)
    {   ret=mq_receive(feeder,(char *)&cur,sizeof(int),0);
        if(ret==-1 && errno==EBADE)
            pthread_exit(NULL);

        mes=&gd.glo_mes[cur];        
        ret=read(gd.glo_pfd[cur].fd,mes_tbuf,1023);
        if(ret>0)
        {   mes_tbuf[ret]='\0';
            mes_buf=(char *)malloc(ret+1);
            p4(__LINE__,mes_buf);
        
            size=(int *)malloc(sizeof(int));
            if(size==NULL)
            {   free(mes_buf);
                p4(__LINE__,size);
            }
            
            strncpy(mes_buf,mes_tbuf,ret+1);
            queue_join(mes->mes_q,mes_buf);

            *size=ret;
            queue_join(mes->mes_len_q,size);

            mes->mes_count++;
        }
        else
        {   close(gd.glo_pfd[cur].fd);
            
            gd.glo_pfd[cur].fd=-1;
            if(gd.Big==cur)
                gd.Big--;

            hurricane(&gd.glo_mes[cur]);
            queue_rebirth(&gd.glo_mes[cur].mes_q);
            queue_rebirth(&gd.glo_mes[cur].mes_len_q);
        }

    }
}

void* Pth_poll(void *data)
{   pthread_detach(pthread_self());
    
    int i,ret,client;
    struct sockaddr_in sock_attr;
    socklen_t sock_len=sizeof(sock_attr);
    while(1)
    {   ret=poll(gd.glo_pfd,gd.Big+1,-1);
        if(gd.glo_pfd[0].revents & (POLLRDNORM| POLLERR))
        {   client=accept(lis_sock,(struct sockaddr *)&sock_attr,&sock_len);   

            for(i=1; i<gd.Big+2; i++)
            {   if(gd.glo_pfd[i].fd==-1)
                    break;
            }
            if(i>=gd.Big)
                gd.Big=i;

            gd.glo_pfd[i].fd=client;
            gd.glo_pfd[i].events=POLLRDNORM;
            
            inet_ntop(AF_INET,&sock_attr.sin_addr.s_addr,gd.glo_cmp[i].ip,16);
            gd.glo_cmp[i].port=ntohs(sock_attr.sin_port);

            if(--ret==0)
                continue;
        }

        for(i=1; i<=gd.Big; i++)
        {   if(gd.glo_pfd[i].fd==-1)
                continue;

            if(gd.glo_pfd[i].revents & (POLLRDNORM | POLLERR) )
            {   mq_send(feeder,(char *)&i,sizeof(int),0);
                usleep(1);
                ret--;
            }

            if(ret==0)
                break;
        }
    }
    
}

void Monitor(int interval)
{   while(1)
    {   
        system("clear");
        puts("===== Collar Monitor system ====");
        for(int i=1; i<=gd.Big; i++)
        {   if(gd.glo_pfd[i].fd!=-1)
                printf("# %s:%hu mesage:[%d] \n",gd.glo_cmp[i].ip, gd.glo_cmp[i].port, gd.glo_mes[i].mes_count);

        }
        sleep(interval);
    }
}


int main()
{   Listen_ready(con_listen_port);
    Global_ready(con_connect_max,con_message_max);
    Poll_reactor_ready();
    Pth_pool_ready(con_pthread_max);

    Monitor(1);

    Sig_secure_quit(0);
}
