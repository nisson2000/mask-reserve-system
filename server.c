#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[512];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    // you don't need to change this.
    int id;
    int wait_for_write;  // used by handle_read to know if the header is read or not.
} request;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list

const char* accept_read_header = "ACCEPT_FROM_READ";
const char* accept_write_header = "ACCEPT_FROM_WRITE";
const char* ask_id = "Please enter the id (to check how many masks can be ordered)";

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

typedef struct {
    int id; //customer id
    int adultMask;
    int childrenMask;
} Order;

int handle_read(request* reqP) {
    char buf[512];
    char *token;
    read(reqP->conn_fd, buf, sizeof(buf));
    //printf("strlen = %d\n",strlen(token) );
    memcpy(reqP->buf, buf, strlen(buf));
    return 0;
}

int main(int argc, char** argv) {

    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;

    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading
    char buf[512];
    int buf_len;

    // Initialize server
    init_server((unsigned short) atoi(argv[1]));

    // Loop for handling connections


    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);

    fd_set *master_set,*working_set;
    master_set = (fd_set*)malloc(sizeof(fd_set));
    working_set = (fd_set*)malloc(sizeof(fd_set));
    FD_ZERO(master_set);
    FD_ZERO(working_set);
    FD_SET(svr.listen_fd,master_set);
    int fdmax = svr.listen_fd;
    char internal_lock[20];
    for(int i = 0;i < 20;i++){
        internal_lock[i] = 'N';
    }
    while (1) {
        // TODO: Add IO multiplexing
        
        memcpy(working_set,master_set,sizeof(&master_set));
        //working_set = master_set;
        if(select(FD_SETSIZE,working_set,NULL,NULL,NULL) == -1){
            ERR_EXIT("select");
        }
        for(int i = 0;i < FD_SETSIZE;i++){
            if(FD_ISSET(i,working_set)){
                if(i == svr.listen_fd){// Check new connection
                    clilen = sizeof(cliaddr);
                    conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
                    if (conn_fd < 0) {
                        if (errno == EINTR || errno == EAGAIN) continue;  // try again
                        if (errno == ENFILE) {
                            (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                            continue;
                        }
                    ERR_EXIT("accept");
                    }
                    FD_SET(conn_fd,master_set);
                    requestP[conn_fd].conn_fd = conn_fd;
                    strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
                    fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);
                    sprintf(buf,ask_id);
                    write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                }else{
                    int ret;
                    conn_fd = requestP[i].conn_fd;
                // TODO: handle requests from clients
                #ifdef READ_SERVER      
                    ret = handle_read(&requestP[conn_fd]); // parse data from client to requestP[conn_fd].buf
                    char *token;
                    token = strtok(requestP[conn_fd].buf,"\r");
                    int input_id = atoi(token);
                    int offset = input_id - 902001;
                    if(offset >= 20 || offset < 0){
                        sprintf(buf,"Operation failed.\n");
                        write(requestP[conn_fd].conn_fd,buf,strlen(buf));
                    }else if(internal_lock[offset] == 'W'){
                        sprintf(buf,"Locked.\n");
                        write(requestP[conn_fd].conn_fd,buf,strlen(buf));
                    }else{
                        internal_lock[offset] = 'R';   
                        int fd_for_read = open("./preorderRecord", O_RDONLY);
                        struct flock fl;
                        fl.l_type   = F_RDLCK;  /* F_RDLCK，F_WRLCK，F_UNLCK    */
                        fl.l_whence = SEEK_SET; /* SEEK_SET，SEEK_CUR，SEEK_END */
                        fl.l_start  = offset*12; /* Offset from l_whence         */
                        fl.l_len    = 12;        /* length，0 = to EOF           */
                        fl.l_pid    = getpid(); /* our PID                      */
                        if(fcntl(fd_for_read,F_SETLK,&fl) == -1){
                            sprintf(buf,"Locked.\n");
                            write(requestP[conn_fd].conn_fd,buf,strlen(buf));
                        }else{
                            int p;
                            int *a;
                            a = (int*)malloc(sizeof(int));
                            int adultMask,childrenMask;
                            lseek(fd_for_read,offset*12+4,SEEK_SET);
                            p = read(fd_for_read,a,sizeof(int));
                            adultMask = a[0];
                            lseek(fd_for_read,offset*12+8,SEEK_SET);
                            p = read(fd_for_read,a,sizeof(int));
                            childrenMask = a[0];
                            sprintf(buf,"You can order %d adult mask(s) and %d children mask(s).\n",adultMask,childrenMask);
                            write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                            fl.l_type = F_UNLCK;
                            fcntl(fd_for_read,F_SETLK,&fl);
                            close(fd_for_read);
                        }
                        internal_lock[offset] = 'N';
                    }

                #else
                    int fd_for_rdwr;
                    struct flock fl;
                    int offset;
                    int adultMask,childrenMask;
                    int input_id;
                    int c = 0;
                    fl.l_type   = F_RDLCK;  /* F_RDLCK，F_WRLCK，F_UNLCK    */
                    fl.l_whence = SEEK_SET; /* SEEK_SET，SEEK_CUR，SEEK_END */
                    fl.l_len    = 12;        /* length，0 = to EOF           */
                    fl.l_pid    = getpid(); /* our PID                      */
                    if(requestP[conn_fd].wait_for_write == 0){
                        ret = handle_read(&requestP[conn_fd]);
                        char *token;
                        token = strtok(requestP[conn_fd].buf,"\r");
                        input_id = atoi(token);
                        offset = input_id - 902001;
                        fl.l_start  = offset*12; /* Offset from l_whence         */
                        if(offset >= 20 || offset < 0){
                            sprintf(buf,"Operation failed.\n");
                            write(requestP[conn_fd].conn_fd,buf,strlen(buf));
                        }else if(internal_lock[offset] == 'W'){
                            sprintf(buf,"Locked.\n");
                            write(requestP[conn_fd].conn_fd,buf,strlen(buf));
                        }else{
                            internal_lock[offset] = 'W';
                            fd_for_rdwr = open("./preorderRecord", O_RDWR);
                            if(fcntl(fd_for_rdwr,F_SETLK,&fl) == -1){
                                internal_lock[offset] = 'N';
                                sprintf(buf,"Locked.\n");
                                write(requestP[conn_fd].conn_fd,buf,strlen(buf));
                            }else{
                                requestP[conn_fd].wait_for_write = 1;
                                int p;
                                int *a;
                                a = (int*)malloc(sizeof(int));
                                lseek(fd_for_rdwr,offset*12+4,SEEK_SET);
                                p = read(fd_for_rdwr,a,sizeof(int));
                                adultMask = a[0];
                                lseek(fd_for_rdwr,offset*12+8,SEEK_SET);
                                p = read(fd_for_rdwr,a,sizeof(int));
                                childrenMask = a[0];
                                sprintf(buf,"You can order %d adult mask(s) and %d children mask(s).\n",adultMask,childrenMask);
                                write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                                sprintf(buf,"Please enter the mask type (adult or children) and number of mask you would like to order:\n");
                                write(requestP[conn_fd].conn_fd,buf,strlen(buf));
                                c = 1;
                            } 
                        }
                    }
                    if(c == 1){
                        c = 0;
                        continue;
                    }
                    if(requestP[conn_fd].wait_for_write == 1){
                        ret = handle_read(&requestP[conn_fd]);
                        char *kind,*number;
                        kind = strtok(requestP[conn_fd].buf," ");
                        number = strtok(NULL,"\r");
                        int n = atoi(number);
                        if(strcmp(kind,"adult") == 0 && adultMask >= n && n > 0){
                            lseek(fd_for_rdwr,offset*12+4,SEEK_SET);
                            adultMask = adultMask - n;
                            write(fd_for_rdwr,&adultMask,sizeof(int));
                            sprintf(buf,"Pre-order for %d successed, %d %s mask(s) ordered.\n",input_id,n,kind);
                            write(requestP[conn_fd].conn_fd,buf,strlen(buf));
                        }else if(strcmp(kind,"children") == 0 && childrenMask >= n && n > 0){
                            lseek(fd_for_rdwr,offset*12+8,SEEK_SET);
                            childrenMask = childrenMask - n;
                            write(fd_for_rdwr,&childrenMask,sizeof(int));
                            sprintf(buf,"Pre-order for %d successed, %d %s mask(s) ordered.\n",input_id,n,kind);
                            write(requestP[conn_fd].conn_fd,buf,strlen(buf));
                        }else{
                            sprintf(buf,"Operation failed.\n");
                            write(requestP[conn_fd].conn_fd,buf,strlen(buf));
                        }
                        //sprintf(buf,"%s %s\n",kind,number);
                        //write(requestP[conn_fd].conn_fd,buf,strlen(buf));

                        fl.l_type = F_UNLCK;
                        fcntl(fd_for_rdwr,F_SETLK,&fl);
                        internal_lock[offset] = 'N';
                        requestP[conn_fd].wait_for_write = 0;
                        close(fd_for_rdwr);
                    }
                #endif

                    close(requestP[conn_fd].conn_fd);
                    FD_CLR(conn_fd,master_set);
                    free_request(&requestP[conn_fd]);
                } // end of if i
            } // end of if FD_ISSET
        }//end of for

        
    }
    free(requestP);
    return 0;
}

// ======================================================================================================
// You don't need to know how the following codes are working
#include <fcntl.h>

static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->id = 0;
    reqP->wait_for_write = 0;
}

static void free_request(request* reqP) {
    /*if (reqP->filename != NULL) {
        free(reqP->filename);
        reqP->filename = NULL;
    }*/
    init_request(reqP);
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }

    // Get file descripter table size and initize request table
    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (int i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);

    return;
}
