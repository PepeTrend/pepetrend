#include <config.h>
#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_DIRENT_H
#  include <dirent.h>
#else
#    define dirent direct
#    define NAMLEN(dirent) (dirent)->d_namlen
#  ifdef HAVE_SYS_NDIR_H
#    include <sys/ndir.h>
#  endif
#  ifdef HAVE_SYS_DIR_H
#    include <sys/dir.h>
#  endif
#  ifdef HAVE_NDIR_H
#    include <ndir.h>
#  endif
#endif

#include <unistd.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <mystring.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <pwd.h>
#include <grp.h>
#include <errno.h>
//#include <glob.h>

#include "cwd.h"
#include "options.h"
#include "main.h"
#include "login.h"
#include "dirlist.h"

#if 0	//cbj
struct hidegroup *hidegroups = NULL;

void add_to_hidegroups(int gid)
{
    static struct hidegroup *tmp = NULL;
    if (tmp)
        tmp = tmp->next = malloc(sizeof(struct hidegroup));
    else
        hidegroups = tmp = malloc(sizeof(struct hidegroup));
    tmp->next = NULL;
    tmp->data = gid;
}

void hidegroups_init()
{
    char *foo = strdup(config_getoption("HIDE_GROUP"));
	char *foo_save = foo;
    char *bar;
    struct group *tmpgrp;
    while ((bar = strtok(foo, ","))) {
        foo = NULL; /* strtok requirement */
        if ((strcmp(bar, "0")) && (!strtoul(bar, NULL, 10))) {
            /* bar is not numeric */
            if ((tmpgrp = getgrnam(bar)))
                add_to_hidegroups(tmpgrp->gr_gid);
        } else
            if (strtoul(bar, NULL, 10))
                add_to_hidegroups(strtoul(bar, NULL, 10));
    }
	free(foo_save);
}

void hidegroups_end()
{
    struct hidegroup *tmp = hidegroups;
    if (hidegroups)
        while (hidegroups->next) {
            tmp = hidegroups->next;
            free(hidegroups);
            hidegroups = tmp;
        }
}
#endif	//cbj

void bftpd_stat(char *name, FILE * client)
{
//<< [JAZ-FS-001] Charles Wei: Jazztel customization for storage service: ftpuser , 20120117
#ifdef CTCONFIG_CUSTOMER_JAZ
    struct stat64 statbuf;
#else
    struct stat statbuf;
#endif // CTCONFIG_CUSTOMER_JAZ 
//>> [JAZ-FS-001] End
	char temp[MAXCMD + 3], linktarget[MAXCMD + 5], perm[11], timestr[17];
	//char uid[USERLEN + 1], gid[USERLEN + 1];
    struct tm filetime;
    time_t t;
//<< [JAZ-FS-001] Charles Wei: Jazztel customization for storage service: ftpuser , 20120117
#ifdef CTCONFIG_CUSTOMER_JAZ
    if (lstat64(name, (struct stat64 *) &statbuf) == -1) { // used for command_stat
#else
    if (lstat(name, (struct stat *) &statbuf) == -1) { // used for command_stat
#endif // CTCONFIG_CUSTOMER_JAZ 
//>> [JAZ-FS-001] End
        fprintf(client, "213-Error: %s.\n", strerror(errno));
        return;
    }
#ifdef S_ISLNK
	if (S_ISLNK(statbuf.st_mode)) {
		strcpy(perm, "lrwxrwxrwx");
		temp[readlink(name, temp, sizeof(temp) - 1)] = '\0';
		sprintf(linktarget, " -> %s", temp);
	} else {
#endif
		strcpy(perm, "----------");
		if (S_ISDIR(statbuf.st_mode))
			perm[0] = 'd';
		if (statbuf.st_mode & S_IRUSR)
			perm[1] = 'r';
		if (statbuf.st_mode & S_IWUSR)
			perm[2] = 'w';
		if (statbuf.st_mode & S_IXUSR)
			perm[3] = 'x';
		if (statbuf.st_mode & S_IRGRP)
			perm[4] = 'r';
		if (statbuf.st_mode & S_IWGRP)
			perm[5] = 'w';
		if (statbuf.st_mode & S_IXGRP)
			perm[6] = 'x';
		if (statbuf.st_mode & S_IROTH)
			perm[7] = 'r';
		if (statbuf.st_mode & S_IWOTH)
			perm[8] = 'w';
		if (statbuf.st_mode & S_IXOTH)
			perm[9] = 'x';
		linktarget[0] = '\0';
#ifdef S_ISLNK
	}
#endif
    memcpy(&filetime, localtime(&(statbuf.st_mtime)), sizeof(struct tm));
    time(&t);
    if (filetime.tm_year == localtime(&t)->tm_year)
    	mystrncpy(timestr, ctime(&(statbuf.st_mtime)) + 4, 12);
    else
        strftime(timestr, sizeof(timestr), "%b %d  %G", &filetime);
    //cbj -- without authentication 	
    //mygetpwuid(statbuf.st_uid, passwdfile, uid)[8] = 0;	
    //mygetpwuid(statbuf.st_gid, groupfile, gid)[8] = 0;	
//<< [JAZ-FS-001] Charles Wei: Jazztel customization for storage service: ftpuser , 20120117
#ifdef CTCONFIG_CUSTOMER_JAZ
	fprintf(client, "%s %3i - - %8lu %s %s%s\r\n", perm,
#else
	fprintf(client, "%s %3i %8lu %s %s%s\r\n", perm,
#endif // CTCONFIG_CUSTOMER_JAZ 
//>> [JAZ-FS-001] End
			(int) statbuf.st_nlink,
			(unsigned long) statbuf.st_size,
			timestr, name, linktarget);
}

void dirlist_one_file(char *name, FILE *client, char verbose)
{
//<< [JAZ-FS-001] Charles Wei: Jazztel customization for storage service: ftpuser , 20120117
#ifndef CTCONFIG_CUSTOMER_JAZ
    struct stat statbuf;
#endif // CTCONFIG_CUSTOMER_JAZ 
//>> [JAZ-FS-001] End
/*  cbj
    struct hidegroup *tmp = hidegroups;
    if (!stat(name, (struct stat *) &statbuf)) {
        if (tmp)
            do {
                if (statbuf.st_gid == tmp->data)
                    return;
            } while ((tmp = tmp->next));
    }
*/
    if (verbose)
        bftpd_stat(name, client);
    else
        fprintf(client, "%s\r\n", name);
}

void dirlist(char *name, FILE * client, char verbose)
{
   DIR *directory;
   struct dirent *entry;
   
//<< [JAZ-FS-006] Charles Wei: Jazztel customization for storage service: improve the capability of listing long directory name, 20120130
#if 0
   char dirbuffer[20];
   
   getcwd(dirbuffer,20);   
   directory = opendir(dirbuffer);
   
   if (directory == NULL)
   {
      return;
   } 
#else
   char *dirbuffer = NULL;
   
   dirbuffer = getcwd(NULL, 0);
   if (dirbuffer != NULL)
   directory = opendir(dirbuffer);
   else
      return;
   
   if (directory == NULL)
   {
      free(dirbuffer);
      return;
   } 
#endif
//>> [JAZ-FS-006] End
   else 
   {
      while ((entry = readdir(directory)) != NULL)
      {
         if (entry->d_name[0] == '.')
         {
            if ((entry->d_name[1] == 0) || ( entry->d_name[1] == '.')
               && (entry->d_name[2] == 0))
            {
               continue;
            }
         }
         
         dirlist_one_file(entry->d_name, client, verbose);
      }
   }
   closedir(directory);
//<< [JAZ-FS-006] Charles Wei: Jazztel customization for storage service: improve the capability of listing long directory name, 20120130
   free(dirbuffer);
//>> [JAZ-FS-006] End
}



