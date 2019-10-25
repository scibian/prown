/*
 * Prown is a simple tool developed to give users the possibility to 
 * own projects (files and repositories).
 * Copyright (C) 2019 EDF SA.

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */ 

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <dirent.h>
#include <pwd.h>
#include "error.h"
#include <getopt.h>


/* static variable for verbose mode */ 
static int verbose;

/*
 * Check if user has access to project 'path' .
 *
 * Returns 0 if valid, 1 otherwise.
 * */
int projectAccess(const char *path)
{
	int result = access(path, W_OK);
	return result;
}

/*set user as the owner of the current file or directory*/
void setOwner(const char *path)
{	
	//use lchown to change owner for symlinks 
	if(verbose == 1){
			printf("changing owner of path %s\n",path);
	}
    if (lchown(path,getuid(),(gid_t)-1) != 0)
	{
		perror("chown");
		exit(EXIT_FAILURE);
	}
	//set rwx to user and rw to group if its not a symlink
	struct stat buf;
	int x = lstat (path, &buf);
	if (!S_ISLNK(buf.st_mode))
	{ 
		if (chmod(path,S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP) != 0)
		{
			perror("chmod");
			exit(EXIT_FAILURE);
		}
	}
}

/*
 * set recursively the user as the owner of the project.
 *
 * Returns 0 if valid, 1 otherwise.
 * */
int projectOwner(char *basepath){
    char path[PATH_MAX];
    struct dirent *dp;
	DIR *dir = opendir(basepath);
    int status = 0;
	// Unable to open directory stream
	if (!dir)
		return 1;
	while ((dp = readdir(dir)) != NULL)
	{
		if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0 && strcmp(dp->d_name, basepath) != 0)
		{
			// Construct new path from our base path
			strcpy(path, basepath);
			strcat(path, "/");
			strcat(path, dp->d_name);
			projectOwner(path);
			setOwner(path);	
		}
	}
    closedir(dir);
	return status;
}

/*
 * Read lines from config file.
 * */
void read_str_from_config_line(char* config_line, char* val) {    
    	char prm_name[100];
    	sscanf(config_line, "%s %s\n", prm_name, val);
}

/*
 * Read config file.
 * */
void read_config_file(char config_filename[], char projectsdir[]) {
	FILE *fp;
    	char buf[50];
	if ((fp=fopen(config_filename, "r")) == NULL) {
        	fprintf(stderr, "Failed to open config file %s \n", config_filename);
        	exit(EXIT_FAILURE);
    	}
    	while(! feof(fp)) {
        	fgets(buf, 100, fp);
        	if (buf[0] == '#' || strlen(buf) < 4) {
            		continue;
        	}	
        	if (strstr(buf, "PROJECTS_DIR ")) {
            		read_str_from_config_line(buf, projectsdir);
        	}
    	}
    	fclose(fp);
}

void usage(int status) {
	if (status != EXIT_SUCCESS)
		printf("Saisissez « prown --help » pour plus d'informations.\n");
	else
	{
		printf("Utilisation: prown[OPTION]... FICHIER...\n");
		printf("Modifier le propriétaire du PROJET, FICHIER ou REPERTOIRE en PROPRIO actuel.\n");
		printf("\n-v, --verbose          afficher en détail les fichiers modifiés\n");
		printf("-h, --help             afficher l'aide et quitter\n");
		printf("\nL'utilisateur doit avoir le droit d'écriture sur le fichier ou le dossier\n");
		printf("qu'il souhaite posséder.\n");
		printf("\nExemples :\n");
		printf("  prown ccnhpc           devenir propriétaire sur le projet ccnhpc\n");
		printf("  prown ccnhpc saturne   devenir propriétaire sur le projet ccnhpc et saturne\n");
		printf("  prown ccnhpc/file      devenir propriétaire sur le fichier ccnhpc/file \n");
		
	}
}

int prownProject(char* path){
	uid_t uid=getuid();
	char projectPath[PATH_MAX]; /* List of project paths*/
	int validargs=0,i,j,nbarg,status;
	char projectroot[PATH_MAX], real_dir[PATH_MAX];
	size_t lenprojectroot;

	read_config_file("/etc/prown.conf", projectroot); 
	lenprojectroot=strlen(projectroot);
	// if the real path is correct
	if (realpath(path, real_dir) != '\0')
	{
		//calculate the real path lengh of the project
		lenprojectroot=strlen(projectroot);
		// if the user hasn't access to the project 
		if (projectAccess(real_dir) != 0) {
			printf("Error: permission denied for project '%s' \n", real_dir);
		}
		// if the user passed path is in the ptoject path 
		else if ((strstr(real_dir, projectroot) != NULL) && (strcmp(real_dir,projectroot)))
		{
			printf("Setting owner of %s  directory %s to %d\n", path, real_dir, uid,lenprojectroot);
			strcpy(projectPath,real_dir);
			struct stat path_stat;
			stat(projectPath, &path_stat);
			if (path_stat.st_mode & S_IFREG)
			{
				printf("owning file %s\n", projectPath);
				setOwner(projectPath);
			}
			if ((long)path_stat.st_uid != 0)
			{
				setOwner(projectPath);
			}
			status=projectOwner(projectPath);
			validargs=1;
			j++;
		}
		//The else case is  when the passed project is not in projects path
		else
		{
				printf("You can't take rights everywhere. Directory '%s' will be ignored\n", path);
		}
	}
	else
	{
			printf("Warning: directory '%s' not found! (ignored)\n", path);
	}

	return validargs;
}

int main(int argc, char **argv) {
	int validargs=0;
	char *options = "hv";
	int longindex;
	int opt;
	int help=0;	
	struct option longopts[] = {
		{"help", no_argument, NULL, 'h'},
		{"verbose", no_argument, NULL, 'v'},
		{NULL, 0, NULL, 0}
	};
	
	while ((opt = getopt_long(argc, argv, options, longopts, &longindex)) != -1) {
		switch (opt) {
		case 'v':
		   verbose++;
		   break;
		case 'h':
		   help=1;
		   usage(EXIT_SUCCESS);
		   break;
		default:
		   usage(EXIT_FAILURE);
		   break;
		}
	}
	if ((argc == 1 || optind == argc)&&(help != 1))
	{
		error(0, 0, "opérande manquant");
		usage(EXIT_FAILURE);
	}
	else
	{
		for(; optind < argc; optind++)
		{	
			char *path = argv[optind];
			prownProject(path);
		}
	}
}

