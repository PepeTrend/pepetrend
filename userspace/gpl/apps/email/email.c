#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#include "cms_util.h"
#include "cms_msg.h"

//<< Joba Yang support ssl
#include <sys/poll.h>

#include "openssl/ssl.h"
#include "openssl/bio.h"
#include "openssl/err.h"

#define EMAIL_CA 		"/etc/CA/email.pem"
#define EMAIL_CA_PATH 	"/etc/CA"
#define SMTP_SSL_PORT   465

BIO * bio;
SSL * ssl;
SSL_CTX * ctx;

//>> Joba Yang

#define SIZE 41984 
//define mail commands
#define EHLO 0
#define AUTH 1
#define USER 2
#define PASS 3
#define MAIL 4
#define RCPT 5
#define DATA 6
#define CONT 7
#define QUIT 8

//Debug Macro
//#define EMAIL_DBG
#ifdef EMAIL_DBG
#define DEBUG fprintf(stderr, "##%s:%d##\n", __FUNCTION__, __LINE__);
#else
#define DEBUG
#endif


static int enable;
static char fromAddr[128];
static char toAddr[128];
static char smtpAddr[128];
static char smtpAcnt[128];
static char smtpPwd[128];
static int  smtpPort;	
static int  intervalTime;
static int  auth_fail = 0;
static int  fw_upgrade = 0;
static int  smtpEnblSsl;
static int  count = 0;
static int  bootup = 1;
static int  keep_looping = 1;
static char auth_alarm_app[16];
static char auth_alarm_ip[17];
static char auth_alarm_username[256];
static char auth_alarm_password[256];
static char fw_upgrade_str[64];
static int  accessMode;
static char OLD_PUBLIC_IP[256];
static char NEW_PUBLIC_IP[256];
static char previous_log[4096];
static int  previous_log_len = 0;
static int  syslogStatus = 0;

typedef enum {
	SYSLOG = 0,
	FW_UPDATE,
	AUTHENTICATION_ALARM,
	PUBLIC_IP_CHANGE,
	TYPE_END
}SendType;

char subject[TYPE_END][64] = 
{
	"Syslog",
	"Firmware update",
	"Authentication alarm",
	"Public IP Change"
};

void base64enc(const char *,char *);
int replace(char *str, char *what, char *by)
{
	char *foo, *bar = str;
    int i = 0;
	while ((foo = strstr(bar, what))) {
        bar = foo + strlen(by);
		memmove(bar,
				foo + strlen(what), strlen(foo + strlen(what)) + 1);
		memcpy(foo, by, strlen(by));
        i++;
	}
    return i;
}

#define CGI_LOG_DATE_TIME 0
#define CGI_LOG_FACILITY  1
#define CGI_LOG_SEVERITY  2
#define CGI_LOG_MESSAGE   3

void GetLogData(char *line, char *data, int field){
   char date[4], times[9];
   char *dot = NULL, *cp = NULL;
#ifdef SUPPORT_SNTP                  
   char month[4];
#else
   static char months[12][4] =
      { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
   static int daysOfMonth[12] =
      { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
   int days = 0, i = 0, j = 0;
#endif

   if ( line == NULL ) return;
   data[0] = '\0';

   switch (field) {
      case CGI_LOG_DATE_TIME:
#ifdef SUPPORT_SNTP
        strncpy(month,line,3);
        month[3] = '\0';
            strncpy(date,&line[4],2);
        date[2] = '\0';
        strncpy(times, &line[7],8);
            times[8] = '\0';
        sprintf(data, "%s %s %s", month,date,times);
#else
            // format of date/time as follow: "Jan  1 00:00:00"
            // need to convert to "1st day 00:00:00"
            strncpy(date, line, 3);
            date[3] = '\0';
            for ( i = 0; i < 12; i++ ) {
                if ( cmsUtl_strcmp(months[i], date) == 0 )
                   break;
            }
            if ( i < 12 ) {
                for ( j = 0; j < i; j++ )
                   days += daysOfMonth[j];
            }
            strncpy(date, &line[4], 2);
            date[2] = '\0';
            days += atoi(date);
            strncpy(times, &line[7], 8);
            times[8] = '\0';
            switch (days) {
               case 1:
                  sprintf(data, "%dst day %s", days, times);
                  break;
               case 2:
                  sprintf(data, "%dnd day %s", days, times);
                  break;
               case 3:
                  sprintf(data, "%drd day %s", days, times);
                  break;
               default:
                  sprintf(data, "%dth day %s", days, times);
                  break;
            }
#endif
         break;
      case CGI_LOG_FACILITY:
         dot = strchr(&line[16], '.');
         if ( dot != NULL ) {
            for ( cp = (dot - 1); cp != NULL && *cp !=  ' ' ; cp-- )
               ;
            if ( ++cp != NULL ) {
               strncpy(data, cp, dot - cp);
               data[dot - cp] = '\0';
            }
         }
         break;
      case CGI_LOG_SEVERITY:
         dot = strchr(&line[16], '.');
         if ( dot != NULL ) {
            for ( cp = (dot + 1); cp != NULL && *cp !=  ' ' ; cp++ )
               ;
            if ( cp != NULL ) {
               dot++;
               strncpy(data, dot, cp - dot);
               data[cp - dot] = '\0';
            }
         }
         break;
      case CGI_LOG_MESSAGE:
         dot = strchr(&line[16], '.');
         if ( dot != NULL ) {
            for ( cp = (dot + 1); cp != NULL && *cp !=  ' ' ; cp++ )
               ;
            if ( ++cp != NULL )
               strcpy(data, cp);
         }
         break;
      default:
         data[0] = '\0';
         break;
   }
}


static int inputAttachedFile(char *alarmcontext, int onFull)
{
	char line[4096];
	int readPtr = BCM_SYSLOG_FIRST_READ;
	#if 0
	sprintf(alarmcontext, "%s--------------060102040900050806070505\r\n", alarmcontext);
	sprintf(alarmcontext, "%sContent-Type: text/plain;\r\n", alarmcontext);
	sprintf(alarmcontext, "%s name=\"syslog.log\"\r\n", alarmcontext);
	sprintf(alarmcontext, "%sContent-Transfer-Encoding: 7bit\r\n", alarmcontext);
	sprintf(alarmcontext, "%sContent-Disposition: attachment;\r\n", alarmcontext);
	sprintf(alarmcontext, "%s filename=\"syslog.log\"\r\n\r\n", alarmcontext);	
	#else
	strcat(alarmcontext, "--------------060102040900050806070505\r\n");
	strcat(alarmcontext, "Content-Type: text/plain;\r\n");
	strcat(alarmcontext, " name=\"syslog.log\"\r\n");
	strcat(alarmcontext, "Content-Transfer-Encoding: 7bit\r\n");
	strcat(alarmcontext, "Content-Disposition: attachment;\r\n");
	strcat(alarmcontext, " filename=\"syslog.log\"\r\n\r\n");
	#endif
	DEBUG
    if(onFull){
		int i = 0;
		int line_cnt = 0;
		char date[32];
		char log[4096];
		char allLog[16385];
		char *log_ptr;
		readPtr = cmslog_readLite(readPtr, allLog);
		//replace '\0' to '\n'
		while(i < readPtr - 1){
			if(allLog[i] == '\0'){
				int line_len;
				int remove_tail = 0;
				line[line_cnt] = '\0';
				line_cnt = -1;
				if(previous_log_len > 0){
					#ifdef EMAIL_DBG
					printf("previous_log = %s\n line = %s\n", previous_log, line);
					#endif
					//sprintf(line, "%s%s", previous_log, line);
					strcat(previous_log, line);
					strcpy(line, previous_log);
					previous_log_len = 0;
					#ifdef EMAIL_DBG
					printf("connect!! @~@ line = %s\n", previous_log);
					#endif
				}
				//remove tail of 
				line_len = strlen(line);
				while(line_len > 0){
					remove_tail++;
					line_len /= 10;
				}
				line[strlen(line) - remove_tail - 1] = '\r';
				line[strlen(line) - remove_tail] = '\n';
				line[strlen(line) - remove_tail + 1] = '\0';
				//remove other msg beside date and log
				GetLogData(line, date, CGI_LOG_DATE_TIME);
				replace(date, "\n", "\r\n");
				GetLogData(line, log, CGI_LOG_MESSAGE);
				replace(log, "\n", "\r\n");
				//remove msg like "(none) daemon.warn kernel:"
				log_ptr = strstr(log, ":");
				log_ptr++;
				#ifdef EMAIL_DBG
				printf("line %s date %s log %s\n", line, date, log_ptr);
				#endif

				strcat(alarmcontext, date);
				strcat(alarmcontext, " ");
				strcat(alarmcontext, log_ptr);
			}
			else {
				line[line_cnt] = allLog[i];
			}
			i++;
			line_cnt++;
		}
		if(line_cnt > 0){
			line[line_cnt] = '\0';
			previous_log_len = line_cnt - 1;
			strcpy(previous_log, line);
			#ifdef EMAIL_DBG
			printf("previous_log %s previous_log_len %d\n", previous_log, previous_log_len);
			#endif
		}
		#ifdef EMAIL_DBG
		printf("########### all log ########## readPtr %d\n", readPtr);
		printf("%s", alarmcontext);
		printf("##############################\n");
		#endif
    }
	else {
		int i = 0;
		int line_cnt = 0;
		char date[32];
		char log[4096];
		char allLog[16385];
		char *log_ptr;
		readPtr = cmslog_readLite(readPtr, allLog);
		//printf("readPtr = %d\n allLog = %s\n", readPtr, allLog);
		//replace '\0' to '\n'
		if(readPtr == 0){
			#ifdef EMAIL_DBG
			printf("empty log, don't send email\n");
			#endif
			return -1;
		}
		else {
			while(i < readPtr - 1){
				if(allLog[i] == '\0'){
					int line_len;
					int remove_tail = 0;
					line[line_cnt] = '\0';
					line_cnt = -1;
					line_len = strlen(line);
					while(line_len > 0){
						remove_tail++;
						line_len /= 10;
					}
					line[strlen(line) - remove_tail - 1] = '\r';
					line[strlen(line) - remove_tail] = '\n';
					line[strlen(line) - remove_tail + 1] = '\0';
					//remove other msg beside date and log
					GetLogData(line, date, CGI_LOG_DATE_TIME);
					replace(date, "\n", "\r\n");
					GetLogData(line, log, CGI_LOG_MESSAGE);
					replace(log, "\n", "\r\n");
					//remove msg like "(none) daemon.warn kernel:"
					log_ptr = strstr(log, ":");
					if(log_ptr != NULL)
						log_ptr++;
					#ifdef EMAIL_DBG
					if(log_ptr != NULL)
						printf("line %s date %s log %s\n", line, date, log_ptr);
					else
						printf("line %s date %s log %s\n", line, date, log);
					#endif

					strcat(alarmcontext, date);
					strcat(alarmcontext, " ");
					if(log_ptr != NULL)
						strcat(alarmcontext, log_ptr);
					else
						strcat(alarmcontext, log);
				}
				else {
					line[line_cnt] = allLog[i];
				}
				i++;
				line_cnt++;
			}	
			#ifdef EMAIL_DBG
			printf("########### all log ########## readPtr %d\n", readPtr);
			printf("%s", alarmcontext);
			printf("##############################\n");
			#endif		
		}
	}
	sprintf(alarmcontext, "%s--------------060102040900050806070505--\r\n", alarmcontext);
	return 0;
}
static int inputSubject(char *alarmcontext, SendType type)
{
	char ctime[64];
	char week[4], day[4], month[4], stime[16], year[6];
	time_t cur_time = time(NULL);
	ctime_r(&cur_time, ctime);
    ctime[strlen(ctime)-1] = '\0';
	sscanf(ctime, "%s %s %s %s %s", week, month, day, stime, year);
	
	sprintf(alarmcontext, "%sDate: %s, %s %s %s %s +0800\r\n", alarmcontext, week, day, month, year, stime);
	sprintf(alarmcontext, "%sFrom: %s <%s>\r\n", alarmcontext, fromAddr, fromAddr);
	sprintf(alarmcontext, "%sUser-Agent: Mozilla/5.0 (X11; Linux i686; rv:14.0) Gecko/20120714 Thunderbird/14.0\r\n", alarmcontext);
	sprintf(alarmcontext, "%sMIME-Version: 1.0\r\n", alarmcontext);
	sprintf(alarmcontext, "%sTo: %s\r\n", alarmcontext, toAddr);
	if(type == AUTHENTICATION_ALARM)
		sprintf(alarmcontext, "%sSubject: %s (%s) - %s %s %s %s %s\r\n", alarmcontext, subject[type], auth_alarm_app, week, day, month, year, stime);
	else if(type == FW_UPDATE)
		sprintf(alarmcontext, "%sSubject: %s - %s %s %s %s %s\r\n", alarmcontext, fw_upgrade_str, week, day, month, year, stime);
	else if(type == PUBLIC_IP_CHANGE)
		sprintf(alarmcontext, "%sSubject: Public IP change - %s %s %s %s %s\r\n", alarmcontext, week, day, month, year, stime);
	else
		sprintf(alarmcontext, "%sSubject: %s - %s %s %s %s %s\r\n", alarmcontext, subject[type], week, day, month, year, stime);
	sprintf(alarmcontext, "%sContent-Type: multipart/mixed;\r\n", alarmcontext);
	return 0;
}
static int inputBody(char *alarmcontext, SendType type)
{
	char boardid[32];
	char szSerialNumber[32] = {0};
	UINT8  macAddr[6];
	
    sprintf(alarmcontext, "%s boundary=\"------------060102040900050806070505\"\r\n\r\n", alarmcontext);
	sprintf(alarmcontext, "%sThis is a multi-part message in MIME format.\r\n", alarmcontext);
	sprintf(alarmcontext, "%s--------------060102040900050806070505\r\n", alarmcontext);
	sprintf(alarmcontext, "%sContent-Type: text/plain; charset=ISO-8859-1; format=flowed\r\n", alarmcontext);
	sprintf(alarmcontext, "%sContent-Transfer-Encoding: 7bit\r\n\r\n", alarmcontext);
	devCtl_getBaseMacAddress(macAddr);
	devCtl_getSerialNumber(szSerialNumber);	
	devCtl_getBoardID(boardid, 32);
	if(type == AUTHENTICATION_ALARM){
		sprintf(alarmcontext, "%s====== Authentication alarm ======\r\n", alarmcontext);
		if(strcmp(auth_alarm_app, "Consoled"))
			sprintf(alarmcontext, "%sIP : %s\r\n", alarmcontext, auth_alarm_ip);
		sprintf(alarmcontext, "%sUsername : %s\r\n", alarmcontext, auth_alarm_username);
		sprintf(alarmcontext, "%sPassword : %s\r\n", alarmcontext, auth_alarm_password);
		if(strcmp(auth_alarm_app, "Consoled")){
			switch(accessMode){
				case NETWORK_ACCESS_LAN_SIDE:
					sprintf(alarmcontext, "%sAccess From : LAN\r\n", alarmcontext);
					break;
				case NETWORK_ACCESS_WAN_SIDE:
					sprintf(alarmcontext, "%sAccess From : WAN\r\n", alarmcontext);
					break;	
				default: //Do not do anything because accessMode should be LAN or WAN
					break;
			}
		}
		sprintf(alarmcontext, "%s==================================\r\n\r\n", alarmcontext);
	}
	else if(type == PUBLIC_IP_CHANGE){
		sprintf(alarmcontext, "%s====== Public IP change ======\r\n", alarmcontext);
		sprintf(alarmcontext, "%sOld IP address : %s\r\n", alarmcontext, OLD_PUBLIC_IP);
		sprintf(alarmcontext, "%sNew IP address : %s\r\n", alarmcontext, NEW_PUBLIC_IP);
		sprintf(alarmcontext, "%s==================================\r\n\r\n", alarmcontext);		
	}
	sprintf(alarmcontext, "%s======== CPE information =========\r\n", alarmcontext);
	sprintf(alarmcontext, "%sBoard ID : %s\r\n", alarmcontext, boardid);
	sprintf(alarmcontext, "%sSerial number : %s\r\n", alarmcontext, szSerialNumber);
	sprintf(alarmcontext, "%sMAC address : %02X:%02X:%02X:%02X:%02X:%02X\r\n", alarmcontext, macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
	sprintf(alarmcontext, "%s==================================\r\n", alarmcontext);
	return 0;
}
static int initMsg(char **msg, char **n_return, SendType type, int onFull)
{
	char MAIL_BUF[128];
	char RCPT_BUF[128];
	char EHLO_BUF[128];
	char alarmcontext[SIZE]="";
	char username[64]="";//mail username
	char passwd[64]="";//mail passwd	
	
	DEBUG;
	sprintf(EHLO_BUF, "helo %s\n", smtpAddr);
	sprintf(MAIL_BUF, "mail from:<%s>\n", fromAddr);
	sprintf(RCPT_BUF, "rcpt to:<%s>\n", toAddr);	
	msg[EHLO]=EHLO_BUF;
	msg[AUTH]="auth login\n";
	base64enc(smtpAcnt,username);
	strcat(username,"\n");
	msg[USER]=username;
	base64enc(smtpPwd,passwd);
	strcat(passwd,"\n");
	msg[PASS]=passwd;
	msg[MAIL]=MAIL_BUF;
	msg[RCPT]=RCPT_BUF;
	msg[DATA]="data\n";
	msg[QUIT]="quit\n";

	n_return[EHLO]="250";
	n_return[AUTH]="334";
	n_return[USER]="334";
	n_return[PASS]="235";
	n_return[MAIL]="250";
	n_return[RCPT]="250";
	n_return[DATA]="354";
	n_return[CONT]="250";
	DEBUG;
	/* ======Load syslog here======= */	
	inputSubject(alarmcontext, type);
	DEBUG;
	inputBody(alarmcontext, type);
	DEBUG;
	if(type == SYSLOG)
		if(	inputAttachedFile(alarmcontext, onFull) == -1)
			return -1;
	msg[CONT]=alarmcontext; 
	return 0;
}

void *get_sockaddr_ip(struct sockaddr* sa)
{
    if(sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int email(int onFull, SendType type)
{
	int sockfd = 0;
	struct sockaddr server_addr;
	struct hostent *server_ip;
	int numbytes=0,i=0;
	char buff[512]="",tmp[4]="";
	char *msg[9]={""};
	char *n_return[9]={""}; //return number
    struct addrinfo hint, *servinfo, *p;
	int ret = 0;
	char addr[40] = {0};
	memset(&hint, 0, sizeof hint);
	hint.ai_family = AF_UNSPEC;
	hint.ai_socktype = SOCK_STREAM;	
	
	if(!enable)	{
		printf("Email syslog function disable!\n");
		return 0;
	}
	// Jazztel no need notification of firmware upgrade
	if(type == FW_UPDATE)return 0;
	if(initMsg(msg, n_return, type, onFull) == -1){
		#ifdef EMAIL_DBG
		printf("empty lite system log, don't send\n");
		#endif
		return 0;
	}
	if(!cmsUtl_isValidIpAddress(AF_INET, smtpAddr) && !cmsUtl_isValidIpAddress(AF_INET6, smtpAddr)){
		if ((ret = getaddrinfo(smtpAddr, NULL, &hint, &servinfo)) != 0)
		{
			printf("smtp server: %s [%s]\n" , gai_strerror(ret) , smtpAddr);
			return -1;
		}
		for(p = servinfo ; p != NULL; p = p->ai_next)
		{
			//if(p->ai_family == AF_INET6)continue;
			//DEBUG;
			if((sockfd = socket(p->ai_family , p->ai_socktype , p->ai_protocol)) == -1)
			{
				printf("email: open socket error\n");
				continue;
			}
			DEBUG;
			break;
		}
		if(p != NULL){
			((struct sockaddr_in *)p->ai_addr)->sin_port=htons(smtpPort);//short,network byte order
			//connect server
			if(connect(sockfd, (struct sockaddr *)p->ai_addr, p->ai_addrlen)==-1)
			{
				perror("connect error");
				return(-1);
			}	
			DEBUG;		
		}
		else {
			printf("query -> %s FAIL\n" , smtpAddr);		
			return -1;
		}
	}
	else {
		struct sockaddr server_addr;
		int size = 0;
		memset(&server_addr, 0, sizeof(struct sockaddr));
		if(cmsUtl_isValidIpAddress(AF_INET, smtpAddr)){
			struct sockaddr_in *addr = (struct sockaddr_in*)&server_addr;
			addr->sin_family = AF_INET;
			addr->sin_port = smtpPort;
			inet_pton(AF_INET, smtpAddr, get_sockaddr_ip(&server_addr));
			size = sizeof(struct sockaddr_in);
			if((sockfd=socket(AF_INET,SOCK_STREAM,0))==-1)
			{
				perror("socket error");
				return(-1);
			}				
		}
		else {
			struct sockaddr_in6 *addr = (struct sockaddr_in*)&server_addr;
			addr->sin6_family = AF_INET6;
			addr->sin6_port = smtpPort;
			inet_pton(AF_INET6, smtpAddr, get_sockaddr_ip(&server_addr));
			size = sizeof(struct sockaddr_in6);
			if((sockfd=socket(AF_INET6,SOCK_STREAM,0))==-1)
			{
				perror("socket error");
				return(-1);
			}			
		}
		if(connect(sockfd, &server_addr, size)==-1)
		{
			perror("connect error");
			return(-1);
		}	
		DEBUG;				
	}
	

#if 1
	//SSL enable, use OpenSSL for connection
    if(smtpEnblSsl){	
	    ctx = SSL_CTX_new(SSLv3_client_method());

	    /* Setup the connection */
	    ssl = SSL_new (ctx);
		DEBUG;
		SSL_set_fd(ssl, sockfd);
		SSL_connect(ssl);		
		DEBUG;
		//clean tmp
		p = SSL_read(ssl, buff, SIZE);
		DEBUG;
		if(p <= 0){
	        SSL_CTX_free(ctx);
			SSL_free (ssl);
	        return -1;
		}
		DEBUG;
		for(i=0;i<4;i++)
			tmp[i]='\0';
		strncpy(tmp,buff,3);
		DEBUG;
		if(strcmp(tmp,"220")!=0){
	        SSL_CTX_free(ctx);	
			SSL_free (ssl);
			return (-1);
		}
		DEBUG;
		i=EHLO;
		while(i<QUIT)
		{
			if(i == CONT){
				int len = strlen(msg[i]);
				if(len > 1300){
					int cnt = 0;
					char frag[1300] = {0};
					while((len - cnt) > 1300){ //If message large than 1300, do fragment
						strncpy(frag, msg[i] + cnt, 1299);
						frag[1299] = '\0';
						if((numbytes=SSL_write(ssl, frag, strlen(frag))) == -1)
						{
							perror("send error");
							break;
						}
						cnt += 1299;
					}				
					if((numbytes=SSL_write(ssl, msg[i] + cnt, strlen(msg[i] + cnt))) == -1)
					{
						perror("send error");
						break;
					}
				}
				else if((numbytes=SSL_write(ssl, msg[i], strlen(msg[i]))) == -1)
				{
					perror("send error");
					break;
				}
				if((numbytes=SSL_write(ssl, ".\r\n", 3)) == -1)
				{
					perror("send error");
					break;
				}
			}
			else if((numbytes=SSL_write(ssl, msg[i], strlen(msg[i]))) == -1)
			{
				perror("send error");
				break;
			}
			if((numbytes=SSL_read(ssl,buff,SIZE)) == -1)
			{
				perror("recv error");
				break;
			} 
			strncpy(tmp,buff,3);
		#ifdef EMAIL_DBG
			fprintf(stderr, "command:%s\n",msg[i]);
			fprintf(stderr, "return buff:%s\n",buff);
			fprintf(stderr, "should return:%s\n",n_return[i]);
		#endif
			if(strcmp(tmp, n_return[i])==0) 
				i++;
			else
				break;
		}
		SSL_CTX_free(ctx);		
		SSL_free (ssl);
    }
	else{
		DEBUG;
		//if connect success,server return "220"
		if((numbytes=recv(sockfd,buff,SIZE,0))==-1)
		{
			perror("recv error");
			return(-1);
		} 
		DEBUG;
		//clean tmp
		for(i=0;i<4;i++)
			tmp[i]='\0';
		strncpy(tmp,buff,3);
		if(strcmp(tmp,"220")!=0)
			return (-1);
		DEBUG;
		//send msgs. if any step has a mistake,the "while" will be breaked,then send "quit" to end connection 
		#ifdef EMAIL_DBG
		fprintf(stderr, "begin send msgs\n");
		#endif
		i=EHLO;
		while(i<QUIT)
		{
			if(i == CONT){
				#if 1
				int len = strlen(msg[i]);
				if(len > 1300){
				//if(0){
					//printf("===large than 1300=== len = %d\n", len);
					int cnt = 0;
					char frag[1300] = {0};
					while((len - cnt) > 1300){ //If message large than 1300, do fragment
						strncpy(frag, msg[i] + cnt, 1299);
						frag[1299] = '\0';
						//printf("##%s:%d## frag %s\n", __FUNCTION__, __LINE__, frag);
						if((numbytes=send(sockfd, frag, strlen(frag), 0))==-1)
						{
							perror("send error");
							break;
						}
						//printf("##%s:%d## numbytes %d\n", __FUNCTION__, __LINE__, numbytes);
						cnt += 1299;
					}				
					//printf("last packet, msg[i] + cnt = %s len = %d", (msg[i] + cnt), strlen(msg[i] + cnt));
					if((numbytes=send(sockfd, msg[i] + cnt, strlen(msg[i] + cnt), 0))==-1)
					{
						perror("send error");
						break;
					}
				}
				else if((numbytes=send(sockfd, msg[i], strlen(msg[i]), 0))==-1)
				{
					perror("send error");
					break;
				}
				#endif
				if((numbytes=send(sockfd, ".\r\n", 3, 0))==-1)
				{
					perror("send error");
					break;
				}
			}
			else if((numbytes=send(sockfd, msg[i], strlen(msg[i]), 0))==-1)
			{
				perror("send error");
				break;
			}
			//sleep(1);we dont have to use it,because recv() can choke itself until it received data
			if((numbytes=recv(sockfd,buff,SIZE,0)) == -1)
			{
				perror("recv error");
				break;
			} 
			strncpy(tmp,buff,3);
			#ifdef EMAIL_DBG
			fprintf(stderr, "command:%s\n",msg[i]);
			fprintf(stderr, "return buff:%s\n",buff);
			fprintf(stderr, "should return:%s\n",n_return[i]);
			#endif
			if(strcmp(tmp, n_return[i])==0) 
				i++;
			else
				break;
		}
		DEBUG;
		//send quit to end mail connection
		if((numbytes=send(sockfd,msg[QUIT],strlen(msg[QUIT]),0))==-1)
		{
			perror("send error");
			return(-1);
		}		
	}
#endif
	DEBUG;
	system("killall -17 syslogd");
	if(sockfd != -1)
		close(sockfd);	
	return (0);
}

/*-------------------------
base64 encode function
-------------------------*/
void base64enc(const char *instr, char *outstr)
{ 
    char *table="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int   instr_len=0,i=0,j=0,pad=0; 
    unsigned char buf1[4]="", buf2[4]=""; 
   
    instr_len=strlen(instr);    
    pad=instr_len%3;
    
    for(i=0; i<instr_len; i +=3) 
        { 
         if(i == instr_len-pad)
                  strncpy(buf1,&instr[i],pad);
            else
               strncpy(buf1,&instr[i],3);
  
            buf2[0] = buf1[0] >> 2; 
            buf2[1] = ((buf1[0] & 0x03) << 4) | (buf1[1] >> 4); 
            buf2[2] = ((buf1[1] & 0x0f) << 2) | (buf1[2] >> 6); 
            buf2[3] = buf1[2] & 0x3f; 
            for(j=0;j<4;j++) 
                  buf2[j]=table[buf2[j]];
               
         if(i==instr_len-pad)
               for(j=3;j>pad;j--)
                     buf2[j]='=';
       	strncat(outstr, buf2, 4);
      }
   
} 

static void stopEmail(void)
{
	keep_looping = 0;
}
static void sendEmail(void)
{
	FILE *p;
	p = fopen("/var/emailContent", "r");
	DEBUG;
	if(p != NULL){
		char buf[512];
		SendType type = SYSLOG;
		int i = 0;
		//First line show what type to send
		fgets(buf, 512, p);
		while(i != TYPE_END){
			#ifdef EMAIL_DBG
			printf("subject[%d] = %s len %d\n", i, subject[i], strlen(subject[i]));
			#endif
			if(strncmp(buf, subject[i], strlen(subject[i])) == 0){
				type = i;
				break;
			}
			i++;
		}
		#ifdef EMAIL_DBG
		printf("Send Email type = %d buf %s len %d\n", type, buf, strlen(buf));
		#endif
		fflush(stdout);
		if(type == AUTHENTICATION_ALARM){
			//while(fgets(buf, 128, p) != NULL){
			DEBUG;
			fgets(buf, 512, p);
			DEBUG;
			#ifdef EMAIL_DBG
			printf("buf %s\n", buf);		
			#endif
			if(strstr(buf, "Consoled"))
				sscanf(buf, "%s %s %s %d", auth_alarm_app, auth_alarm_username, auth_alarm_password, &accessMode);
			else
				sscanf(buf, "%s %s %s %s %d", auth_alarm_app, auth_alarm_ip, auth_alarm_username, auth_alarm_password, &accessMode);
			#ifdef EMAIL_DBG
			printf("auth_alarm %s %s %s %s %d\n", auth_alarm_app, auth_alarm_ip, auth_alarm_username, auth_alarm_password, accessMode);		
			#endif
			//}
			
		}
		else if(type == FW_UPDATE){
			strcpy(fw_upgrade_str, buf);
		}
		else if(type == PUBLIC_IP_CHANGE){
			DEBUG;
			fgets(buf, 512, p);
			DEBUG;
			fflush(stdout);
			#ifdef EMAIL_DBG
			printf("buf %s\n", buf);
			#endif
			sscanf(buf, "%s %s", OLD_PUBLIC_IP, NEW_PUBLIC_IP);
			#ifdef EMAIL_DBG
			printf("old IP %s new IP %s\n", OLD_PUBLIC_IP, NEW_PUBLIC_IP);		
			#endif
		}
		fclose(p);
		if(auth_fail == 0 && type == AUTHENTICATION_ALARM);
		else if(fw_upgrade == 0 && type == FW_UPDATE);
		else {
			if(type == SYSLOG && syslogStatus == 0);
			else if(email(1, type) == -1){
				#ifdef EMAIL_DBG
				printf("E-mail delivery failed.\n");
				#endif
				syslog(LOG_CRIT, JAZ_TAG"E-mail delivery failed.");
			}
		}
		//If this is send for syslog back up, restart counting
		if(type == SYSLOG)
			count = 0;
	}
	else
		fprintf(stderr, "Can not open /var/emailContent\n");
}
static void updateEmailCfg(void)
{
	FILE *p;
    p = fopen("/var/emailcfg", "r");
    if(p != NULL){
        fscanf(p, "%d", &enable);
        fscanf(p, "%s", fromAddr);
        fscanf(p, "%s", toAddr);
        fscanf(p, "%s", smtpAddr);
        fscanf(p, "%d", &smtpPort);
        fscanf(p, "%s", smtpAcnt);
        fscanf(p, "%s", smtpPwd);
		fscanf(p, "%d", &intervalTime);
		fscanf(p, "%d", &auth_fail);
		fscanf(p, "%d", &fw_upgrade);
		fscanf(p, "%d", &syslogStatus);
		fscanf(p, "%d", &smtpEnblSsl);
		printf("smtpEnblSsl %d\n", smtpEnblSsl);
		count = 0; 
        fclose(p);
    }
    else {
        printf("Can not open /var/emailcfg\n");
    }
}
int main(void)
{
	signal(SIGKILL, &stopEmail);
	signal(SIGUSR1, &sendEmail);
	signal(SIGUSR2, &updateEmailCfg);

    ERR_load_BIO_strings();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

	updateEmailCfg();
	while(keep_looping){
		//DEBUG;
		if(count++ > intervalTime && intervalTime != 0 && syslogStatus != 0){
			if(email(0, SYSLOG) == -1){
				#ifdef EMAIL_DBG
				printf("E-mail delivery failed.\n");
				#endif
				syslog(LOG_WARNING, JAZ_TAG"E-mail delivery failed.");
			}
			count = 0;
		}
		sleep(1);
	}
	return 0;
} 
