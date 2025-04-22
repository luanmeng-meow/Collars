#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>

#define CMP_Active 1
#define CMP_Death  0
#define CMP_LIST_MAX 128
#define CMP_LISTEN_PORT 8100
#define CMP_REFRESH_INTERVAL 3

#define S_Active 1
#define S_Death  0

#define S_SOCK_ERROR -1
#define S_OVERTIME -2
#define S_GIFT_BROKEN -3
#define S_GIFT_EMPTY -4

#define P_Fatal_Error -1
#define P_BUF_LESS -2
#define P_INPUTER_ERROR -3
#define P_PACK_EMPTY -4


#define text_pack    '~'
#define command_pack '$'
#define transer_pack '%'
#define request_pack '#'
#define respons_pack '&'

static char heartbe_pack='@';
/*  
    Format controls the connection successful computer
*/
struct computer
{   int id;
    int sock;
    char sock_status;
    socklen_t sock_len;
    struct sockaddr_in sock_data;

    char sock_ip_c[16];
    unsigned short sock_port_us;
};
static struct computer l_cmp;
static pthread_mutex_t list_mu;
static int list_count=0;
static struct computer *list[CMP_LIST_MAX]={NULL};

/*  
    Formatting access stream data
*/
struct network_string
{   char status;
    int sock;
    int  buf_count;
    char buf[1024];
    char *buf_cur;
    struct computer *computer;
};
static pthread_mutex_t s_mu;
static struct network_string s_data;
static int overtime_alarm=0;

/*  
    Formatting manage data pack
*/
struct pack
{   int cur;
    int maxsize;
    int *message_len;
    char **message;
};
static struct pack p_s[5];
static sem_t p_run;
static pthread_mutex_t p_mu;
static int p_guard_overtime;

/*
    status and sleep init
*/
static int accept_status;
static int accept_sleep;

static int refresh_status;
static int refresh_sleep;

static int guard_status;
static int guard_sleep;


void Signal_service();
void Sig_secure_quit(int sig);

void* Pth_refresh(void *data);
void* Pth_accept(void *data);
void* Pth_manage_pack(void *data);

/*========================================================================================================================*/

static int t_t_var=0;
void t_t(const char *title)
{   if(t_t_var)
        puts(title);
}

void t_ppp(const char *title,int ret)
{   if(ret==-1)
    {   perror(title);
        // abort();
        // exit(-1);
        raise(SIGINT);
    }
}

void t_init_service()
{   pthread_t th[3];
    pthread_mutex_init(&list_mu,NULL);
    pthread_mutex_init(&s_mu,NULL);
    pthread_mutex_init(&p_mu,NULL);
    sem_init(&p_run,0,0);

    pthread_create(&th[0],NULL,Pth_accept,NULL);        //Accept new computer connect fill list
    pthread_create(&th[1],NULL,Pth_refresh,NULL);       //Periodicity refresh active list 
    pthread_create(&th[2],NULL,Pth_manage_pack,NULL);

    Signal_service();   
    //  Register:
    // (1)secure quit
    
    return;
}

void t_destroy_service()
{   
    Sig_secure_quit(0);   
    // (1) Pthread Status=0,Sleep=0  
    // (2) Close listen_sock
    // (3) Free whole list_sock 
    // (4) Return 
    
    // pthread_mutex_destroy(&list_mu);
    // pthread_mutex_destroy(&s_mu);
    // pthread_mutex_destroy(&p_mu);
    // sem_destroy(&p_run);

    return ;
}

int t_check(int sock)
{   
    return send(sock,&heartbe_pack,1,MSG_NOSIGNAL|MSG_DONTWAIT)==1 ? 0:-1;
}

int t_D_EINTR_Read(int fd,char *buf,int size)
{   
    if(fcntl(fd, F_GETFD)==-1 && errno==EBADF)
        return -1;

    if(buf==NULL ||size==0)
        return 0;
    
    int ret;
    int total=size;
    
    while(total)
    {   ret=read(fd,buf,total);
        if(ret==-1 && errno==EINTR)
            continue;
        else if(ret==-1)
            return -1;
        else if(ret==0)
            return size-total;
        else
        {   buf+=ret;
            total-=ret;
        }
    }
    return size-total;
}

int t_D_EINTR_Write(int fd,char *buf,int size)
{   if(fcntl(fd, F_GETFD)==-1 && errno==EBADF)
        return -1;

    if(buf==NULL ||size==0)
        return 0;

    int ret;
    int total=size;

    while(total)
    {   ret=write(fd,buf,total);
        if(ret==-1 && errno==EINTR)
            continue;
        else if(ret==-1)
            return -1;
        else
        {   buf+=ret;
            total-=ret;
        }
    }
    return size-total;
}

/*========================================================================================================================*/

void cmp_init_ac(struct computer *cmp,int tsock,const struct sockaddr *tsock_attr)
{   if( cmp==NULL || tsock_attr==NULL ||  tsock==-1 )
        t_ppp("Cmp_init_ac",-1);

    struct sockaddr_in *cur=(struct sockaddr_in *)tsock_attr;
    
    bzero(cmp,sizeof(struct computer));
    cmp->sock_len=sizeof(cmp->sock_data);

    cmp->sock_data.sin_family=cur->sin_family;
    cmp->sock_data.sin_addr.s_addr=cur->sin_addr.s_addr;
    cmp->sock_data.sin_port=cur->sin_port;
    
    cmp->sock=tsock;
    cmp->sock_status=CMP_Active;

    inet_ntop(AF_INET, &cmp->sock_data.sin_addr.s_addr, cmp->sock_ip_c, sizeof(cmp->sock_ip_c));
    cmp->sock_port_us=ntohs(cmp->sock_data.sin_port);\
    
    cmp->id=-1;
}

void cmp_init_user(struct computer *cmp,const char *ip,const unsigned short port)
{   if( cmp==NULL || port<=1024 )
        t_ppp("Cmp_init_us 1",-1);

    bzero(cmp,sizeof(struct computer));
    cmp->sock_len=sizeof(cmp->sock_data);
    
    cmp->sock_status=CMP_Death;
    cmp->sock=socket(AF_INET,SOCK_STREAM,0);
    t_ppp("Cmp_init_us 2",cmp->sock);

    cmp->sock_data.sin_family=AF_INET;

    if(ip==NULL)
    {   cmp->sock_data.sin_addr.s_addr=INADDR_ANY;
        strcpy(cmp->sock_ip_c,"0.0.0.0");
    }
    else
    {   cmp->sock_data.sin_addr.s_addr=inet_addr(ip);
        strcpy(cmp->sock_ip_c,ip);
    }
    
    cmp->sock_data.sin_port=htons(port);
    cmp->sock_port_us=port;

    cmp->id=-1;
}

void cmp_print(struct computer *cmp)
{   
    printf("%s:%hu\n",cmp->sock_ip_c,cmp->sock_port_us);
}

/*========================================================================================================================*/


/*========================================================================================================================*/

int cmp_list_join(struct computer *cmp)
{   if(cmp==NULL || cmp->sock_status==CMP_Death || cmp->id!=-1  )
        return -1;
    t_t("cmp_list_join 0");
    pthread_mutex_lock(&list_mu);

    
    if(list_count== CMP_LIST_MAX)
    {   t_t("cmp_list_join 1");
        pthread_mutex_unlock(&list_mu);
        return -1;
    }
    t_t("cmp_list_join 3");
    
    list[list_count++]=cmp;
    cmp->id=list_count;

    // printf("cmp_list_join 3 [%d]\n",list[list_count-1]->sock);
    pthread_mutex_unlock(&list_mu);
    t_t("cmp_list_join 4");
    
    return 0;
}

int cmp_list_leave(struct computer *cmp)
{   if(cmp==NULL || cmp->id==-1) 
        return -1;

    struct computer *temp=cmp;

    pthread_mutex_lock(&list_mu);

    if(list_count==0)
    {   pthread_mutex_unlock(&list_mu);
        return -1;
    }

    if(cmp->id==list_count)
    {   list[cmp->id-1]=NULL;
        temp->id=-1;
        
        list_count--;
        pthread_mutex_unlock(&list_mu);
        return 0;
    }

    else
    {   for(int i=cmp->id; i<list_count; i++)
        {   list[i-1]=list[i];
            list[i-1]->id--;
        }
        list[list_count-1]=NULL;
               

        list_count--;
        pthread_mutex_unlock(&list_mu);
        return 0;
    }
}

void cmp_list_show()
{   
    
    pthread_mutex_lock(&list_mu);
    for(int i=0; i<list_count; i++)
        printf("[%d] %s:%hu\n",i+1, list[i]->sock_ip_c, list[i]->sock_port_us);
    pthread_mutex_unlock(&list_mu);

    if(list_count==0)
        puts("no hosting...");
}

void cmp_list_accept_destroy()
{   close(l_cmp.sock);

    pthread_mutex_lock(&list_mu);
    for(int i=0; i<list_count; i++)
    {   close(list[i]->sock);
        free(list[i]);
    }
    pthread_mutex_unlock(&list_mu);
}

void cmp_list_accept_fill()
{   int ret;
    l_cmp.sock=socket(AF_INET,SOCK_STREAM,0);    
    t_ppp("cmp_accept 1",l_cmp.sock);

    cmp_init_user(&l_cmp,NULL,CMP_LISTEN_PORT);

    ret=bind(l_cmp.sock,(struct sockaddr *)&l_cmp.sock_data,l_cmp.sock_len);
    t_ppp("cmp_accept 2",ret);

    ret=listen(l_cmp.sock,100);
    t_ppp("cmp_accept 3",ret);
    
    struct sockaddr t;
    socklen_t t_len=sizeof(t);
    struct computer* member;

    while(accept_status)
    {   do{ 
            ret=accept(l_cmp.sock,&t,&t_len);

        }while(ret==-1 && (errno==EINTR || errno==ECONNABORTED) );
        
        t_t("cmp_list_accept_fill 0");

        if(ret==-1 && errno==EBADF)
        {   t_t("cmp_list_accept_fill 1");
            return;
        }
        t_ppp("cmp_accept 4",ret);

        t_t("cmp_list_accept_fill 2");
        member=malloc(sizeof(struct computer));
        cmp_init_ac(member,ret,&t);
        
        t_t("cmp_list_accept_fill 3");
        ret=cmp_list_join(member); 
        t_ppp("cmp_accept 4",ret);
        
        t_t("cmp_list_accept_fill 4");
        // printf("cmp_list_accept_fill:[%d]\n",member->sock);
        sleep(accept_sleep);
    }
}

void cmp_list_accept_throw()
{   struct computer *temp;

    int array[CMP_LIST_MAX];
    int active=0,ret;

    while(refresh_status)
    {   
        active=0;
        pthread_mutex_lock(&list_mu);
        for(int i=0; i<list_count; i++)
        {   ret=send(list[i]->sock, &heartbe_pack, 1, MSG_NOSIGNAL|MSG_DONTWAIT );
            if(ret==-1)
            {   t_t("cmp_list_accept_throw 1");
                if(s_data.sock==list[i]->sock)
                {   t_t("cmp_list_accept_throw 2");
                    s_data.status=S_Death;   
                }
               t_t("cmp_list_accept_throw 3");
                close(list[i]->sock);
                free(list[i]);
                list[i]=NULL;
                continue;
            }
            else
            {   t_t("cmp_list_accept_throw 0");
                array[active++]=i;
            }
        }
        
        t_t("cmp_list_accept_throw 0-1");
        if(active==list_count)
            goto up1;

        t_t("cmp_list_accept_throw 0-2");
        for(int i=0;i<active;i++)
        {   list[i]=list[array[i]];
            list[i]->id=i+1;
        }
        
        list_count=active;

        
        up1:
        t_t("cmp_list_accept_throw 0-3");
        pthread_mutex_unlock(&list_mu);
        sleep(refresh_sleep);
    }
}

/*========================================================================================================================*/

// socket error is return -1
// inputer argument errnor return 0
// overtime return -2 

int S_bind(struct computer *cmp)
{   pthread_mutex_lock(&s_mu);
    t_t("S_bind 1");
    if( s_data.sock!= -1 || s_data.computer!=NULL )
    {   printf("S_bind 2 :%d\n",s_data.sock);
        t_t("S_bind 2");
        pthread_mutex_unlock(&s_mu);
        return -1;
    }

    pthread_mutex_lock(&list_mu);
    if(cmp==NULL || cmp->sock_status==CMP_Death)
    {   t_t("S_bind 3");
        pthread_mutex_unlock(&list_mu);
        pthread_mutex_unlock(&s_mu);
        return -1;
    }
    
    if(t_check(cmp->sock))
    {   t_t("S_bind 4");
        pthread_mutex_unlock(&list_mu);
        pthread_mutex_unlock(&s_mu);
        return -1;
    }
    t_t("S_bind 5");
    s_data.computer=cmp;
    s_data.sock=cmp->sock;    
    pthread_mutex_unlock(&list_mu);

    t_t("S_bind 6");
    s_data.status=S_Active;
    s_data.buf_cur=NULL;
    s_data.buf_count=0;
    bzero(s_data.buf,1024);
    pthread_mutex_unlock(&s_mu);
    return 0;
}

int S_unbind()
{   pthread_mutex_lock(&s_mu);
    s_data.computer=NULL;
    s_data.sock=-1;

    s_data.buf_cur=NULL;
    s_data.buf_count=0;
    bzero(s_data.buf,1024);
    
    pthread_mutex_unlock(&s_mu);
    return 0;
}

int S_check()
{   int ret=send(s_data.sock,&heartbe_pack,1,MSG_DONTWAIT|MSG_NOSIGNAL);
    
    t_t("S_check 1");
    
    if(ret==1)
    {   t_t("S_check 2");
        s_data.status=S_Active;
        return 0;
    }
    else
    {   t_t("S_check 3");
        s_data.status=S_Death;
        return -1;
    }
}

int S_fill()
{   
    int ret;   
    up1:
    if(s_data.status==S_Death || S_check())
        return -1;

    // do{
    //     ret=read(s_data.sock,s_data.buf,1024); 
    // }while(ret==-1 && errno==EINTR );
    
    // t_D_EINTR_Read(s_data.sock,s_data.buf,1024);

    ret=read(s_data.sock,s_data.buf,1024);

    if(ret<=0)              //read the ret==0 || ret==-1 therefore socket is error
    {   if(S_check()==-1)   //again check socket
            return -1;
        else if(ret==-1 && errno==EINTR)
            return -1;
        else
            goto up1;   //Extreme error
    }
 
    s_data.buf_cur=s_data.buf;
    return s_data.buf_count=ret;
}

char S_getchar()
{   char ch;

    t_t("S_getchar 1");
    up1:
    pthread_mutex_lock(&s_mu);
    if(s_data.status==S_Death)
    {   t_t("S_getchar 1-1");
        pthread_mutex_unlock(&s_mu);
        return -1;
    }
    
    t_t("S_getchar 2");

    if( s_data.buf_count==0 )
    {   t_t("S_getchar 3");
        if(S_fill()==-1)
        {   pthread_mutex_unlock(&s_mu);
            return -1;
        }
        if(s_data.buf_count==0)
        {   return -1;
            pthread_mutex_unlock(&s_mu);
        }
    }
    t_t("S_getchar 4");
    s_data.buf_count--;
    ch= (*s_data.buf_cur++);
    
    if(ch==heartbe_pack)
        goto up1;
    else
    {   t_t("S_getchar 5");
        pthread_mutex_unlock(&s_mu);
        return ch;
    }
}

int S_getline(char *buf,int size)
{   if(buf==NULL || size==0)
        return 0;
    
    pthread_mutex_lock(&s_mu);
    if(s_data.status==S_Death )
    {   pthread_mutex_unlock(&s_mu);
        return -1;
    }

    char ch;
    int total=0;

    for(int i=0;i<size;i++)
    {   if( s_data.buf_count==0 )
        {   if(S_fill()==-1)
            {   pthread_mutex_unlock(&s_mu);
                return -1;
            }

            if(s_data.buf_count==0)
            {   pthread_mutex_unlock(&s_mu);
                return -1;
            }
        }

        s_data.buf_count--;
        ch=(*s_data.buf_cur++); 
        
        if(ch==heartbe_pack)
        {   i--;
            continue;
        }
        else if(ch!='\n')
        {   buf[i]=ch;
            total++;
            continue;
        }
        else
        {   buf[i]=ch;
            break;    
        }
    }

    pthread_mutex_unlock(&s_mu);
    return total;
}

/*      
 *      S_SOCK_ERROR   -1   
 *      S_OVERTIME     -2
 *      S_GIFT_BROKEN  -3   
 *      S_GIFT_EMPTY   -4
 */ 
int S_getgift(char *buf,int size,char giftbox_format,int overtime)
{   if(buf==NULL || size==0 || giftbox_format<0 || overtime<0)
        return 0;

    t_t("S_getgift 1");
    pthread_mutex_lock(&s_mu);
    if(s_data.status==S_Death)
    {   pthread_mutex_unlock(&s_mu);
        return S_SOCK_ERROR;
    }
    t_t("S_getgift 2");
    if(giftbox_format==0)
        giftbox_format=text_pack;
    
    if(overtime==0)
        overtime=60;
    
    char ch;
    int total=0,ret;
    t_t("S_getgift 3");
    alarm(overtime);

    do
    {   if(s_data.buf_count==0 ) 
        {   t_t("S_getgift 4");
            ret=read(s_data.sock, s_data.buf, 1024);
            
            if(ret==-1 && errno==EINTR && overtime_alarm==1 )
            {   overtime_alarm=0;
                pthread_mutex_unlock(&s_mu);
                return S_OVERTIME;
            }

            else if( ret==-1 || ret==0 )
            {   if(S_check()==0)
                    continue;

                else
                {   pthread_mutex_unlock(&s_mu);
                    return S_SOCK_ERROR;
                }
            }

            else
            {   t_t("S_getgift 5");
                s_data.buf_cur=s_data.buf;
                s_data.buf_count=ret;
            }
        }


        t_t("S_getgift 6");
        s_data.buf_count--;
        ch=(*s_data.buf_cur++); 

    }while(ch!=giftbox_format );

    t_t("S_getgift 7");

    for(int i=0; i<size; i++)
    {   
        if(s_data.buf_count==0 ) 
        {   t_t("S_getgift 7-1");
            ret=read(s_data.sock, s_data.buf, 1024);
            if(ret==-1 && errno==EINTR && overtime_alarm==1 )
            {   overtime_alarm==0;
                pthread_mutex_unlock(&s_mu);
                return S_OVERTIME;
            }

            else if( ret==-1 || ret==0 )
            {   if(S_check()==0)
                    continue;
                else
                {   pthread_mutex_unlock(&s_mu);
                    return S_SOCK_ERROR;
                }
            }
            else
            {   s_data.buf_cur=s_data.buf;
                s_data.buf_count=ret;
            }
        }

        t_t("S_getgift 8");
        s_data.buf_count--;
        ch=(*s_data.buf_cur++); 
        
        if(ch==heartbe_pack)
        {   t_t("S_getgift 9-1");
            i--;
            continue;
        }
        else if(ch!=giftbox_format)
        {   t_t("S_getgift 9-2");
            buf[i]=ch;
            total++;
            continue;
        }
        else
        {   t_t("S_getgift 9-3");
            buf[i]=ch;
            break;    
        }
    }
    
    alarm(0);
    
    if(ch==giftbox_format)
    {   
        buf[total]='\0';

        t_t("S_getgift 10");
        printf("S_getgift 10 :<< S_G ### %s ### >>\n",buf);
        
        if(total==0)
        {   t_t("S_getgift 10-1");
            pthread_mutex_unlock(&s_mu);
            return S_GIFT_EMPTY;
        }
        t_t("S_getgift 10-2");
        pthread_mutex_unlock(&s_mu);
        return total;
    }

    if(*(s_data.buf_cur+1)==giftbox_format)
    {   t_t("S_getgift 11");
        pthread_mutex_unlock(&s_mu);
        return total;
    }

    t_t("S_getgift 12");
    pthread_mutex_unlock(&s_mu);
    return S_GIFT_BROKEN;
}

int S_putchar(char ch)
{   pthread_mutex_lock(&s_mu);
    if(s_data.status==S_Death)
    {   pthread_mutex_unlock(&s_mu);
        return -1;
    }

    if(S_check()==-1)
    {   return -1;
        pthread_mutex_unlock(&s_mu);
    }

    int ret=t_D_EINTR_Write(s_data.sock,&ch,1);
    if(ret==-1 && errno==EPIPE)
        s_data.status=S_Death;

    pthread_mutex_unlock(&s_mu);
    return ret;
}

int S_puts(char *buf)
{   if(buf==NULL)
        return 0;

    pthread_mutex_lock(&s_mu);
    if(s_data.status==S_Death)
    {   pthread_mutex_unlock(&s_mu);
        return -1;
    }

    if(S_check()==-1)
    {   pthread_mutex_unlock(&s_mu);
        return -1;
    }

    int ret=t_D_EINTR_Write(s_data.sock,buf,strlen(buf));
    if(ret==-1 && errno==EPIPE)
        s_data.status=S_Death;

    pthread_mutex_unlock(&s_mu);
    return ret;
}

int S_putgift(char *buf,char giftbox_format)
{   int size=strlen(buf);
    if(size==0)
        return 0;
    size+=3;

    if(giftbox_format<0)
        return 0;
    if(giftbox_format==0)
        giftbox_format=text_pack;
    
    char f_cable[3]={giftbox_format,giftbox_format,0};
    char b_cable[2]={giftbox_format,0};

    char *temp_buf=malloc(size);
    if(temp_buf==NULL)
        return 0;

    pthread_mutex_lock(&s_mu);
    if(s_data.status==S_Death)
    {   pthread_mutex_unlock(&s_mu);
        free(temp_buf);

        return S_SOCK_ERROR;
    }

    bzero(temp_buf,size);
    strcpy(temp_buf,f_cable);
    strcat(temp_buf,buf);
    strcat(temp_buf,b_cable);

    int ret=t_D_EINTR_Write(s_data.sock,temp_buf,size);
    if(ret==-1 && errno==EPIPE)
        s_data.status=S_Death;

    free(temp_buf);
    pthread_mutex_unlock(&s_mu);
    return ret;
}


int P_guard_init(int overtime,int limit_array[5])
{   if(overtime<0 )
        return -1;

    t_t("P_guard_init 1");
    if(limit_array==NULL)
    {   t_t("P_guard_init 2");
        for(int i=0; i<5; i++)
        {   p_s[i].message=NULL;
            p_s[i].message_len=NULL;
            p_s[i].cur=0;
            p_s[i].maxsize=100;
        }
    }
    else
    {   for(int i=0; i<5; i++)
        {   p_s[i].message=NULL;
            p_s[i].message_len=NULL;
            p_s[i].cur=0;
            p_s[i].maxsize=limit_array[i];
        }
    }
    t_t("P_guard_init 3");
    p_guard_overtime=overtime;
    return 0;
}

int P_guard_destory()
{   alarm(1);
    pthread_mutex_lock(&p_mu);
    
    for(int i=0,count; i<5; i++)
    {   
        count=p_s[i].cur;
        for(int j=0; j<count; j++)
        {   
            free(p_s[i].message[j]);
            p_s[i].message[j]=NULL;
        }
        t_t("P_guard_destory 2");
        free(p_s[i].message_len);
        p_s[i].message_len=NULL;

        free(p_s[i].message);
        p_s[i].message=NULL;
    }
    t_t("P_guard_destory 2");
    pthread_mutex_unlock(&p_mu);
}

void P_guard()
{   
    int ret;
    char ch,pack_buf[1024];
    
    t_t("P_guard 0");
    while(guard_status)
    {   sem_wait(&p_run);
        pthread_mutex_lock(&p_mu);

        t_t("P_guard 1");

        switch(ch=S_getchar())
        {   case text_pack:
            {   ch=0;
                ret=S_getgift(pack_buf,1024,text_pack,p_guard_overtime);
                break;
            }
            case command_pack:
            {   ch=1;
                ret=S_getgift(pack_buf,1024,command_pack,p_guard_overtime);
                break;
            }
            case transer_pack:
            {   ch=2;
                ret=S_getgift(pack_buf,1024,transer_pack,p_guard_overtime);
                break;
            }
            case request_pack:
            {   ch=3;
                ret=S_getgift(pack_buf,1024,request_pack,p_guard_overtime);
                break;
            }
            case respons_pack:
            {   ch=4;
                ret=S_getgift(pack_buf,1024,respons_pack,p_guard_overtime);
                break;
            }
            case -1:
                guard_status=0;

            default:
                goto up1;
        };
        
        t_t("P_guard 3");

        if(ret>0)
        {   t_t("P_guard 4");
            if(p_s[ch].cur==p_s[ch].maxsize)
                goto up1;
            
            t_t("P_guard 5");
            p_s[ch].message=(void *)realloc(p_s[ch].message,(p_s[ch].cur+1)*sizeof(char*));
            if(p_s[ch].message==NULL)
            {   t_t("P_guard 5-0");
                guard_status=0;
                goto up1;
            }

            t_t("P_guard 6");
            p_s[ch].message[p_s[ch].cur]=(void *)malloc(ret*sizeof(char));
            if(p_s[ch].message[p_s[ch].cur]==NULL)
            {   t_t("P_guard 6-0");

                free(p_s[ch].message);
                guard_status=0;
                goto up1;
            }


            t_t("P_guard 7");
            p_s[ch].message_len=(int *)realloc( p_s[ch].message_len, (p_s[ch].cur+1)*sizeof(int) );
            if(p_s[ch].message_len==NULL)
            {   
                t_t("P_guard 7-0");
                free(p_s[ch].message);
                free(p_s[ch].message[p_s[ch].cur]);
                guard_status=0;
                goto up1;
            }

            t_t("P_guard 8");

            strncpy(p_s[ch].message[p_s[ch].cur],pack_buf,ret);
            p_s[ch].message_len[p_s[ch].cur]=ret;
            p_s[ch].cur++;
            t_t("P_guard 9");

            goto up1;
        }
        else if(ret==-2 || ret==-3 || ret==-4)
        {   t_t("P_guard 10");
            goto up1;
        }
        else 
        {   t_t("P_guard 11");
            guard_status=0;
            goto up1;
        }

        up1:
        t_t("P_guard 12");
        bzero(&pack_buf,1024);
        pthread_mutex_unlock(&p_mu);
        sem_post(&p_run);
        sleep(guard_sleep);
    }
}

void P_start()
{   sem_post(&p_run);

}

void P_stop()
{   sem_wait(&p_run);

}
/*      
 *      P_Fatal_Error -1
 *      P_INPUTER_ERROR -2
 *      P_BUF_LESS -3
 *      P_PACK_EMPTY -4
 *      
 *
 */
int P_getgift(char type_num,char *buf,int size)
{   if(type_num<0 || 4<type_num || buf==NULL)
        return P_INPUTER_ERROR;
    
    pthread_mutex_lock(&p_mu);
    if(p_s[type_num].cur==0)
    {   pthread_mutex_unlock(&p_mu);
        return P_PACK_EMPTY;
    }
    
    if(size<p_s[type_num].message_len[0])
    {   pthread_mutex_unlock(&p_mu);
        return P_BUF_LESS;
    }
    struct pack *target=&p_s[type_num];
 
    int count=target->cur;
    char *cur=target->message[0];
    int len=target->message_len[0];
    strncpy(buf,cur,len);
    
    free(cur);

    printf("P_G ### %s ###\n",buf);

    for(int i=1; i<count; i++)
    {   
        target->message[i-1]=target->message[i];
        target->message_len[i-1]=target->message_len[i];
    }

    target->message_len=realloc(target->message_len,(target->cur-1)*sizeof(int));
    if(target->message_len==NULL)
    {   pthread_mutex_unlock(&p_mu);
        return P_Fatal_Error;
    }
    
    target->message=realloc(target->message_len,(target->cur-1)*sizeof(char *));
    if(target->message==NULL)
    {   pthread_mutex_unlock(&p_mu);
        return P_Fatal_Error;
    }

    target->cur--;
    pthread_mutex_unlock(&p_mu);
    return len;
}

int P_getmuch(char type_num)
{   return p_s[type_num].cur; 
}

/*========================================================================================================================*/

void* Pth_manage_pack(void *data)
{   pthread_detach(pthread_self());
    t_t("Pth_manage_pack 1");
    guard_status=1;
    guard_sleep=0;

    if(!P_guard_init(10,NULL))
    {   t_t("Pth_manage_pack 2");
        guard_status=1;
        guard_sleep=0;
        P_guard();
    }
    else
    {   t_t("Pth_manage_pack 3");
        guard_status=0;
        guard_sleep=0;
    }
    
    t_t("Pth_manage_pack 4");
    pthread_exit(NULL);
}

void* Pth_accept(void *data)
{   pthread_detach(pthread_self());
    
    accept_status=1;
    accept_sleep=0;

    cmp_list_accept_fill();
    pthread_exit(NULL);
}

void* Pth_refresh(void *data)
{   pthread_detach(pthread_self());
    
    refresh_status=1;
    refresh_sleep=3;
           
    cmp_list_accept_throw();
    pthread_exit(NULL);
}

/*========================================================================================================================*/

void Sig_secure_quit(int sig)
{   accept_status=0;
    accept_sleep=0;

    refresh_status=0;
    refresh_sleep=0;

    guard_status=0;
    guard_sleep=0;
    t_t("Sig_secure_quit 0");
    sleep(1);
    
    t_t("Sig_secure_quit 1");
    cmp_list_accept_destroy();
    
    t_t("Sig_secure_quit 2");
    P_guard_destory();
    

    t_t("Sig_secure_quit 3");
    pthread_mutex_destroy(&list_mu);
    pthread_mutex_destroy(&s_mu);
    pthread_mutex_destroy(&p_mu);
    sem_destroy(&p_run);
   
    exit(0);
}

void Sig_overtime_gift()
{   overtime_alarm=1;

}

void Sig_write_pipe()
{   

}

void Signal_service()
{   t_t("Signal_service 1");

    struct sigaction data;
    data.sa_flags=0;
    data.sa_handler=Sig_overtime_gift;
    sigemptyset(&data.sa_mask);
    sigaction(SIGALRM,&data,NULL);
    
    signal(SIGPIPE,Sig_write_pipe);
    signal(SIGINT,Sig_secure_quit);
    t_t("Signal_service 2");
}

/*========================================================================================================================*/

#if 1
int main1()
{   t_t("statr");
    t_init_service();

    char buff[1024];
    int ret;
    S_unbind();     //不要他妈的删掉这段代码

    while(1)
    {   do
        {   printf("对话没有连接上...\n");
            sleep(2);
        }while(list_count==0);
        
        sleep(3);
        
        S_bind(list[0]);
        P_start();

        t_t("main0");
        while(guard_status)
        {   t_t("main1");
            ret=P_getmuch(3);
            if(ret>0)
            {   puts("哇呜，好像有新东西来了");
                P_stop();
                ret=P_getgift(3,buff,1024);
                P_start();

                buff[ret]='\0';
                t_t("main2");
                printf("say: [%s]\n",buff);
                S_putgift("hello 你好,我收到了你的请求",0);
                
                sleep(2);

            }
            else if(ret==0)
            {   t_t("main3");
                printf("目前没有收到新请求!\n");
                sleep(1);
                continue;
            }
        };

        t_t("main5");
        P_stop();
        S_unbind();
        printf("对话连接断开...\n");
        sleep(3);
    }   


    t_destroy_service();
    return 0;
}

int main2()
{
    struct computer data;
    cmp_init_user(&data,"192.168.1.130",8081);
    data.sock=socket(AF_INET,SOCK_STREAM,0);
    

    connect(data.sock, (struct sockaddr *)&data.sock_data, data.sock_len );
    
    while(1)
    {   write(data.sock,"$",1);

        sleep(1);
    }
    
}
#endif 


int main()
{   t_t("hello");
    main1();

    // main2();
}
