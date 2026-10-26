#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/resource.h>
#include "registry.h"
extern void __gcov_dump(void);

int main(void)
{
	/* XDG branch */
	setenv("XDG_STATE_HOME", "/tmp/opencode/regstate", 1);
	mkdir("/tmp/opencode/regstate", 0755);
	/* HOME branch */
	{
		unsetenv("XDG_STATE_HOME");
		setenv("HOME", "/tmp/opencode/regstate", 1);
		char *d = registry_dir();
		printf("dir=%s\n", d ? d : "(null)");
		free(d);
		setenv("XDG_STATE_HOME", "/tmp/opencode/regstate", 1);
	}

	/* register/unregister */
	printf("reg=%d\n", registry_register("/tmp", "/tmp/x.db"));
	printf("unreg=%d\n", registry_unregister());

	/* truncation failure */
	{
		char *big = malloc(5000);
		memset(big, 'a', 4999); big[4999] = '\0';
		setenv("XDG_STATE_HOME", big, 1);
		printf("reg_big=%d\n", registry_register("/tmp", "/tmp/x.db"));
		printf("unreg_big=%d\n", registry_unregister());
		free(big);
		setenv("XDG_STATE_HOME", "/tmp/opencode/regstate", 1);
	}

	/* list_paths: opendir failure (XDG points to file) */
	{
		char tmpf[] = "/tmp/opencode/regfileXXXXXX";
		int fd = mkstemp(tmpf); close(fd);
		setenv("XDG_STATE_HOME", tmpf, 1);
		char **r=NULL,**d=NULL; pid_t *p=NULL; int n=0;
		printf("list_fail=%d\n", registry_list_paths(&r,&d,&p,&n));
		setenv("XDG_STATE_HOME", "/tmp/opencode/regstate", 1);
		unlink(tmpf);
	}

	/* malformed reg file + dead pid */
	{
		mkdir("/tmp/opencode/regstate/pycools", 0755);
		FILE *f = fopen("/tmp/opencode/regstate/pycools/dead.reg", "w");
		fprintf(f, "999999\n/tmp\n/tmp/dead.db\n"); fclose(f);
		f = fopen("/tmp/opencode/regstate/pycools/bad.reg", "w");
		fprintf(f, "not a reg\n"); fclose(f);
		char **r=NULL,**d=NULL; pid_t *p=NULL; int n=0;
		printf("list_dead=%d n=%d\n", registry_list_paths(&r,&d,&p,&n), n);
		for (int i=0;i<n;i++){free(r[i]);free(d[i]);}
		free(r);free(d);free(p);
	}

	/* realloc: create 9 alive reg files */
	{
		char dir[] = "/tmp/opencode/regstate/pycools";
		for (int i=0;i<9;i++) {
			char path[4096]; snprintf(path,sizeof(path),"%s/%d.reg",dir,1000+i);
			FILE *f=fopen(path,"w");
			fprintf(f,"%d\n/tmp\n/tmp/%d.db\n",(int)getpid(),i); fclose(f);
		}
		char **r=NULL,**d=NULL; pid_t *p=NULL; int n=0;
		printf("list_many=%d n=%d\n", registry_list_paths(&r,&d,&p,&n), n);
		for (int i=0;i<n;i++){free(r[i]);free(d[i]);}
		free(r);free(d);free(p);
		/* cleanup */
		for (int i=0;i<9;i++) {
			char path[4096]; snprintf(path,sizeof(path),"%s/%d.reg",dir,1000+i);
			unlink(path);
		}
	}

	/* overlap */
	{
		mkdir("/tmp/sub",0755);
	mkdir("/tmp/longdir",0755);
	mkdir("/tmp/longdir/sub",0755);
	registry_register("/tmp/longdir", "/tmp/o.db");
		printf("overlap_same=%d\n", registry_overlap("/tmp"));
		printf("overlap_sub=%d\n", registry_overlap("/tmp/sub"));
		printf("overlap_none=%d\n", registry_overlap("/tmp/other"));
		registry_unregister();
	}

	/* registry_stop */
	{
		pid_t child = fork();
		if (child == 0) { signal(SIGTERM, SIG_IGN); pause(); _exit(0); }
		printf("stop_ok=%d\n", registry_stop(child));
		kill(child, SIGKILL); waitpid(child,NULL,0);
		printf("stop_fail=%d\n", registry_stop(999999));
	}

	/* registry_dir with no XDG and no HOME? set HOME empty */
	{
		unsetenv("XDG_STATE_HOME");
		unsetenv("HOME");
		char *d = registry_dir();
		printf("dir2=%s\n", d ? d : "(null)");
		free(d);
		setenv("XDG_STATE_HOME", "/tmp/opencode/regstate", 1);
	}


	/* force state_dir allocation failure */
	{
		pid_t child = fork();
		if (child == 0) {
			struct rlimit rl; rl.rlim_cur = rl.rlim_max = 4096;
			setrlimit(RLIMIT_AS, &rl);
			setenv("XDG_STATE_HOME", "/tmp/opencode/regstate", 1);
			char *d = registry_dir();
			printf("dir_fail=%s\n", d ? d : "(null)");
			fflush(stdout);
			free(d);
			__gcov_dump();
			_exit(0);
		}
		waitpid(child, NULL, 0);
	}
	setenv("PYCOOL_REGISTRY_FAIL","1",1);
	char *d2 = registry_dir();
	printf("dir_fail2=%s\n", d2 ? d2 : "(null)");
	free(d2);
	unsetenv("PYCOOL_REGISTRY_FAIL");
	return 0;
}
