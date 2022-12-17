#ifndef _REENTRANT
#define _REENTRANT
#endif

#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

int master_fd = -1;
pthread_mutex_t accept_con_mutex = PTHREAD_MUTEX_INITIALIZER;


/**********************************************
 * init
   - port is the number of the port you want the server to be
     started on
   - initializes the connection acception/handling system
   - if init encounters any errors, it will call exit().
************************************************/
void init(int port) {
  int sd; 
  struct sockaddr_in addr;
   
  sd = socket(PF_INET, SOCK_STREAM, 0);
  if (sd == -1) {
    perror("socket allocation failed \n");
    exit(-1);
  }

  int enable = 1;
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char*) &enable, sizeof(int));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port); 

  bind (sd, (struct sockaddr*) &addr, sizeof(addr));
  
  listen (sd, 5);
   
  master_fd = sd;
  printf("UTILS.O: Server Started on Port %d\n", port);
}


/**********************************************
 * accept_connection - takes no parameters
   - returns a file descriptor for further request processing.
     DO NOT use the file descriptor on your own -- use
     get_request() instead.
   - if the return value is negative, the thread calling
     accept_connection must should ignore request.
***********************************************/
int accept_connection(void) {
   int newsock;
   struct sockaddr_in new_recv_addr;
   uint addr_len;
   addr_len = sizeof(new_recv_addr);
   
   pthread_mutex_lock(&accept_con_mutex);

   newsock = accept(master_fd, (struct sockaddr*) &new_recv_addr, &addr_len);

   pthread_mutex_unlock(&accept_con_mutex);

   return newsock;
}


/**********************************************
 * get_request
   - parameters:
      - fd is the file descriptor obtained by accept_connection()
        from where you wish to get a request
      - filename is the location of a character buffer in which
        this function should store the requested filename. (Buffer
        should be of size 1024 bytes.)
   - returns 0 on success, nonzero on failure. You must account
     for failures because some connections might send faulty
     requests. This is a recoverable error - you must not exit
     inside the thread that called get_request. After an error, you
     must NOT use a return_request or return_error function for that
     specific 'connection'.
************************************************/
int get_request(int fd, char *filename) {

  char buf[2048];

  if ((read(fd, buf, sizeof(char)*2047)) == -1){
    perror("error reading fd \n");
  }
  buf[2047] = '\0'; // convert buffer to string since it'll be assumed to be one later

  char* method = strtok(buf, " \t\r\n");
  char* filePath = strtok(NULL," \t");
  char* protocol = strtok(NULL, " \t\r\n"); 

  if (strcmp(method, "GET") != 0) {
    close(fd);
    return -1;
  }
  else if (strncmp(protocol, "HTTP/1.0", 8) != 0 && strncmp(protocol, "HTTP/1.1", 8) != 0) {
    close(fd);
    return -1;
  }
  else if ((strstr(filePath, "..") )|| (strstr(filePath, "//"))){
    close(fd);
    return -1;
  }
  else {
    strncpy(filename, filePath, 1023);
    return 0;
  }
}


/**********************************************
 * return_result
   - returns the contents of a file to the requesting client
   - parameters:
      - fd is the file descriptor obtained by accept_connection()
        to where you wish to return the result of a request
      - content_type is a pointer to a string that indicates the
        type of content being returned. possible types include
        "text/html", "text/plain", "image/gif", "image/jpeg" cor-
        responding to .html, .txt, .gif, .jpg files.
      - buf is a pointer to a memory location where the requested
        file has been read into memory (the heap). return_result
        will use this memory location to return the result to the
        user. (remember to use -D_REENTRANT for CFLAGS.) you may
        safely deallocate the memory after the call to
        return_result (if it will not be cached).
      - numbytes is the number of bytes the file takes up in buf
   - returns 0 on success, nonzero on failure.
************************************************/
int return_result(int fd, char *content_type, char *buf, int numbytes) {

  char contentLength[1024]; 
  char contentType[1024];

  char httpResponse[] = "HTTP/1.0 200 OK\n\0";
  sprintf(contentLength, "Content-Length: %d\n", numbytes);
  sprintf(contentType, "Content-Type: %s\n", content_type);
  char closeConnection[] = "Connection: Close\n\n\0";

  // header
  write(fd, httpResponse, strlen(httpResponse));
  write(fd, contentLength, strlen(contentLength));
  write(fd, contentType, strlen(contentType));
  write(fd, closeConnection, strlen(closeConnection));
  // file contents
  write(fd, buf, numbytes);


  close(fd);

  return 0;
}


/**********************************************
 * return_error
   - returns an error message in response to a bad request
   - parameters:
      - fd is the file descriptor obtained by accept_connection()
        to where you wish to return the error
      - buf is a pointer to the location of the error text
   - returns 0 on success, nonzero on failure.
************************************************/
int return_error(int fd, char *buf) { 

  int messagelen = strlen(buf);
  char contentLength[35];

  char notFound[] = "HTTP/1.0 404 Not Found\n\0"; // size = 24
  sprintf(contentLength, "Content-Length: %d\n", messagelen);
  char closeConnection[] = "Connection: Close\n\n\0 ";

  // header
  write(fd, notFound, strlen(notFound));
  write(fd, contentLength, strlen(contentLength));
  write(fd, closeConnection, strlen(closeConnection));
  // error message
  write(fd, buf, messagelen);  

  close(fd);

  return 0;
}
