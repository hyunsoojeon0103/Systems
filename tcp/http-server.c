#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
static void die(const char* s);
int logToStdOut(const char* clientAddr,const char* method,const char* uri, const char* httpV,const char* s);
void sendForm(const int clientSock);
void sendHtml(const int clientSock,const char* s);
void sendHttpHeader(const int clientSock, const char*);
void sendErr(const int clientSock, const char* s);
int main(int argc, char** argv)
{
     if(argc != 5)
     {
         fprintf(stderr,"%s <server_port> <web_root> <mdb-lookup-host> <mdb-lookup-port>\n",argv[0]);
         exit(1);
     }
     char* webRoot = argv[2];
     int mdbPort = atoi(argv[4]);

     struct hostent *he;
     if((he = gethostbyname(argv[3])) == NULL)
        die("gethostbyname failed"); 
     char* mdbIP = inet_ntoa(*(struct in_addr*)he->h_addr);
     
     int mdbSock;
     if((mdbSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
         die("mdbSocket failed");
     FILE* response = fdopen(mdbSock,"r");
   
     struct sockaddr_in servAddr;
     memset(&servAddr, 0, sizeof(servAddr));
     servAddr.sin_family = AF_INET;
     servAddr.sin_addr.s_addr = inet_addr(mdbIP);
     servAddr.sin_port = htons(mdbPort);

     if(connect(mdbSock,(struct sockaddr*) &servAddr, sizeof(servAddr)) < 0)
         die("connect failed");

     int servSock;
     if((servSock = socket(AF_INET,SOCK_STREAM,0)) < 0)
         die("servSocket failed");
     
     int servPort = atoi(argv[1]);
     struct sockaddr_in servAddr2;
     memset(&servAddr2, 0, sizeof(servAddr2));
     servAddr2.sin_family = AF_INET;
     servAddr2.sin_addr.s_addr = htonl(INADDR_ANY);
     servAddr2.sin_port = htons(servPort);
   
     if(bind(servSock,(struct sockaddr*)&servAddr2,sizeof(servAddr2)) < 0 )
         die("bind failed");

     if(listen(servSock,5) < 0)
         die("listen failed");
    
     int clientSock;
     socklen_t clientLen;
     struct sockaddr_in clientAddr;

     while(1)
     {
         char reqBuff[500];
         FILE* request;
         do
         {
             clientLen = sizeof(clientAddr);
             if((clientSock = accept(servSock,(struct sockaddr*) &clientAddr, &clientLen)) < 0)
                 die("accept Failed");
             request = fdopen(clientSock,"r");
         }while(!fgets(reqBuff,sizeof(reqBuff),request) && logToStdOut(inet_ntoa(clientAddr.sin_addr),"","","","400 Bad Request") && !fclose(request));
               
         char* token_separators = "\t \r\n"; 
         char* method = strtok(reqBuff, token_separators);
         char* requestURI = strtok(NULL, token_separators);
         char* httpVersion = strtok(NULL, token_separators);
        
         char uri[300];
         strcpy(uri,requestURI);
          
         if(strcmp(method,"GET") != 0)
         {
             sendHttpHeader(clientSock,"501 Not Implemented");
             sendErr(clientSock,"501 Not Implemented");
             logToStdOut(inet_ntoa(clientAddr.sin_addr),method,uri,httpVersion,"501 Not Implemented");
         }
         else if(uri[0] != '/')
         {
             sendHttpHeader(clientSock,"400 Bad Request");
             sendErr(clientSock,"400 Bad Request");
             logToStdOut(inet_ntoa(clientAddr.sin_addr),method,uri,httpVersion,"400 Bad Request");
         }
         else if(strstr(uri,"/../") || strcmp(strrchr(uri,'/'),"/..") == 0)
         { 
             sendHttpHeader(clientSock,"403 Forbidden");
             sendErr(clientSock,"403 Forbidden");
             logToStdOut(inet_ntoa(clientAddr.sin_addr),method,uri,httpVersion,"403 Forbidden");
         }
         else if(strcmp(uri,"/mdb-lookup") == 0)
         {
             logToStdOut(inet_ntoa(clientAddr.sin_addr),method,uri,httpVersion," 200 OK");
             sendHttpHeader(clientSock,"200 OK");  
             sendForm(clientSock);
         }
         else if(strstr(uri,"/mdb-lookup?key=")) 
         {
             char* temp = "/mdb-lookup?key=";
             int isInvalid = 0;
             for(int i =0; i < strlen(temp) && !isInvalid; ++i)
                if(uri[i] != temp[i])
                    isInvalid = 1;
             if(!isInvalid)
             { 
                 char* query = strrchr(uri,'=');
                 ++query;
                 int len = strlen(query);
                 query[len++] = '\n';
                 if(send(mdbSock,query,len,0) != len)
                      die("send0 failed");
                 
                 sendHttpHeader(clientSock,"200 OK");  
                 sendHtml(clientSock,"<html><body>");
                 sendForm(clientSock);
                 sendHtml(clientSock,"<p><table border>\n");
                 query[len -1] = '\0';
                 fprintf(stdout,"looking up [%s]: %s \"%s %s %s\" 200 OK\n",query,inet_ntoa(clientAddr.sin_addr),method,uri,httpVersion);

                 char temp[1000];
                 while(strcmp(fgets(temp,sizeof(temp),response),"\n") != 0)
                 {
                     sendHtml(clientSock,"<tr><td>");
                     sendHtml(clientSock,temp);
                 }
                 sendHtml(clientSock,"</table></body></html>");
             }
         }
         else
         {
             char filePath[100];
             strcpy(filePath,webRoot);
             strcat(filePath,uri);
             
             struct stat st; 
             stat(filePath,&st);

             if(S_ISDIR(st.st_mode))
                (filePath[strlen(filePath) -1] == '/') ? strcat(filePath,"index.html") : strcat(filePath,"/index.html");

             FILE* file = fopen(filePath,"rb");
             if(file != NULL)
             {
                 char buff[4096];
                 logToStdOut(inet_ntoa(clientAddr.sin_addr),method,uri,httpVersion,"200 OK");
                 sendHttpHeader(clientSock,"200 OK");  
                 int r;
                 while(r = fread(buff,1,sizeof(buff),file))
                     if(send(clientSock,buff,r,0) != r)
                         die("send failed");
                 fclose(file);
             }
             else
             {
                 sendHttpHeader(clientSock,"404 Not Found");  
                 sendErr(clientSock,"404 Not Found");
                 logToStdOut(inet_ntoa(clientAddr.sin_addr),method,uri,httpVersion,"404 Not Found");
             }
         }
         fclose(request);
         close(clientSock);
     }  
     return 0;
}
static void die(const char* s)
{
     perror(s);
     exit(1);
}
int logToStdOut(const char* clientAddr,const char* method,const char* uri, const char* httpV,const char* s)
{
     if(fprintf(stdout,"%s \"%s %s %s\" %s\n",clientAddr,method,uri,httpV,s))
         return 1;
     return 0;
}
void sendHtml(const int clientSock,const char* s)
{
     char buff[4096];
     snprintf(buff,sizeof(buff),"%s",s);
     if(send(clientSock,buff,strlen(buff),0) != strlen(buff))
         die("send failed");
}
void sendForm(const int clientSock)
{
    const char *form =
            "<h1>mdb-lookup</h1>\n"
            "<p>\n"
            "<form method=GET action=/mdb-lookup>\n"
            "lookup: <input type=text name=key>\n"
            "<input type=submit>\n"
            "</form>\n"
            "<p>\n";

    sendHtml(clientSock,form);
}
void sendHttpHeader(const int clientSock,const char* s)
{
    char buff[100];
    snprintf(buff,sizeof(buff),"HTTP/1.0 %s\r\n\r\n",s);
    if(send(clientSock,buff,strlen(buff),0) != strlen(buff))
         die("send failed");
}
void sendErr(const int clientSock, const char* s)
{
    char buff[100];
    snprintf(buff,sizeof(buff),"<html><body><h1>%s</h1></body></html>",s);
    if(send(clientSock,buff,strlen(buff),0) != strlen(buff))
        die("send failed");
}
