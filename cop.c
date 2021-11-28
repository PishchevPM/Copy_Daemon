#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <sys/wait.h>

void logprint (const char* mestype, const char* mes, const char* param)
{
    char s[100];
    time_t tim = time (NULL);
    strftime (s, 100, "[%a %b %d %H:%M:%S %Y]", localtime (&tim));
    fprintf (stdout, "%s %s: %s %s\n", s, mestype, mes, param);
    fflush (stdout);
}

int dirnamecopy (char* dist, const char* source)
{
	strcpy(dist, source);
    int len = strlen(source);
            
    if (len == 0 || dist[len - 1] != '/')
    {
        dist[len] = '/';
        return len + 1;
    }

    return len;
}

int dir_routine(const char* dest, const char* source)
{
	DIR *dirs, *dird;
	char *paths = calloc (256, 1), *pathd = calloc (256, 1); 
	char *ends = paths, *endd = pathd;
	struct dirent *d;
	struct stat sinfo, dinfo;

	dirs = opendir(source);	
    dird = opendir(dest);
    if (!dird)
    {
        logprint ("INFO", "Created directory", dest);
        mkdir (dest, 0777);
    }
    dird = opendir (dest);
    
	if (dirs)
    {
		ends += dirnamecopy (paths, source);
        endd += dirnamecopy (pathd, dest);

		while ((d = (struct dirent *)readdir(dirs)) != NULL)
        {
			if (strcmp(d->d_name, ".") == 0 || (strcmp(d->d_name, "..") == 0))
			    continue;
			strcpy(ends, d->d_name);
            strcpy(endd, d->d_name);
			if (!stat(paths, &sinfo))
            {
				if (S_ISDIR(sinfo.st_mode)) 
				{   
                    dir_routine(pathd, paths);
                }
				else if (S_ISREG(sinfo.st_mode)) 
                {                    
                    if (stat(pathd, &dinfo) || difftime(dinfo.st_mtim.tv_sec, sinfo.st_mtim.tv_sec) <= 0)
                    {
                        char tmp[1024] = "cp ";
                        strcat(tmp, paths);
                        strcat(tmp, " ");
                        strcat(tmp, pathd);
                        system (tmp);
                        logprint ("INFO", "Copied file", pathd);
                    }
				}	
			}
		}
	}
    else
        logprint ("ERROR", "Cant open directory", source);

    free (paths);
    free (pathd);
        
	return 0;
}

int main(int argc, char* argv[])
{	
    int pid = fork();
    if (pid < 0)
    {
        logprint ("ERROR", "Can't fork", NULL);
        exit (-1);
    }
    else if (pid > 0)
        exit (0);

    char dir[256];
    getcwd(dir, 256);
    int len = strlen (dir);

    if (len == 0 || dir[len - 1] != '/')
    {
        dir[len] = '/';
        len++;
    }
    printf ("%s\n", dir);

    if (setsid () < 0)
        exit (-1);
    umask(0);
    chdir ("/");
    fclose (stdin);
    fclose (stdout);
    fclose (stderr);

    strcpy (dir + len, "copydaemon.log");
    stdout = fopen (dir, "a+");
    if (stdout == NULL)
        exit (-1);

    strcpy (dir + len, "copydaemon.cfg");
    stdin = fopen (dir, "r");
    if (stdin == NULL)
    {
        logprint ("ERROR", "Cant open config file. Please, use configurator", NULL);
        exit (-1);
    }

    key_t key;
    int semid;
    struct sembuf mybuf;
    pid_t p = -2;

    strcpy (dir + len, "copydaemon");
    if((key = ftok(dir, 0)) < 0)
    {
        logprint ("ERROR", "Cant gey key for file", NULL);
        exit(-1);
    }

    if((semid = semget(key, 1, 0666 | IPC_CREAT)) < 0)
    {
        logprint ("ERROR", "Cant gey semid", NULL);
        exit(-1);
    }

    while (1)
    {
        p = fork ();
        if (p < 0)
        {
            logprint ("ERROR", "Cant fork", NULL);
        }
        else if (p == 0)
        {
            break;
        }

        mybuf.sem_op = -1;
        mybuf.sem_flg = 0;
        mybuf.sem_num = 0;

        if(semop(semid, &mybuf, 1) < 0){
            printf("Can\'t wait for condition\n");
            exit(-1);
        }

        
    }

	char source[256], dest[256];
    long delay;
    fgets (&source[0], 256, stdin);
    strchr (&source[0], '\n')[0] = '\0';
    fgets (&dest[0], 256, stdin);
    strchr (&dest[0], '\n')[0] = '\0';
    fscanf (stdin, "%ld", &delay);

    logprint ("INIT", "source is ", &source[0]);
    logprint ("INIT", "destination is ", &dest[0]);

    while (1)
    {
        time_t tmp1 = time (NULL);
        dir_routine(&dest[0], &source[0]);
        time_t tmp2 = time (NULL);
        sleep (delay - (tmp2 - tmp1));
    }
		
	return 0;
}