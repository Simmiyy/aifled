/* 
 * aifled - AneMouse InterfaceLED
 * Copyright (C)2010 Simone Irti <simmiyy@gmail.com>
 * based on sifled 0.5, which is  
 *  Copyright (C)2001 Martin Pot <m.t.pot@ieee.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *                                         
 * Last modified: 011117
 *
 * 
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <linux/kd.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <unistd.h>

const char *banner = 	
		"aifled v0.1 - (c)2010 Simone Irti <simmiyy@gmail.com>\n"
		"(based on sifled v0.5 by Martin Pot <m.t.pot@ieee.org>\n"
		"This program is distributed under the terms of GPL.\n";
const char *help = 
		"%sUsage: %s tty interface [options]\n"
		"\ttty\t\tTty to flash LEDs on, use \"console\" for current.\n"
		"\tinterface\tInterface to monitor, for example: eth0\n"
		"Options:\n"
		"\t-c crt\t\tLED config 3 chars, num, caps and scroll-lock.\n"
		"\t\t\tr = Receive          t = Transmit\n"
		"\t\t\tu = Drop (receive)   i = Drop (transmit)\n"
		"\t\t\tj = Error (receive)  k = Error (transmit)\n"
		"\t\t\tReceive or transmit:\n"
		"\t\t\te = Error            d = Drop\n"
		"\t\t\ta = Activity         c = Collision\n"
		"\t\t\tl = Link is alive (Smoothwall RED interface only)\n"
		"\t\t\tn = None (will not touch this LED)\n"
		"\t\t\tDefault: crt\n"
		"\t-d delay\tLED update delay in ms. Default: 50\n"
		"\t-i\t\tInvert LEDs.\n"
		"\t-a\t\tAlternative LED code for none option.\n"
		"\t-f\t\tFork program into background.\n"
		"\t-v\t\tVerbose mode (-f overrides -v).\n\n"
		"When -f is specified the program create a PID file in /var/run/\n\n";

#define TRUE	1
#define FALSE	0

#define IF_RX		0
#define IF_TX		1
#define IF_COLL		2
#define IF_DROP_RX 	4
#define IF_DROP_TX	5
#define IF_ERR_TX	6
#define IF_ERR_RX	7

#define IF_RXTX		8
#define IF_DROP		9
#define IF_ERR		10
#define IF_NONE		11
#define IF_ALIVE	12

#define OPT_FORK	1
#define OPT_KERNEL_2_0	2

// Simmiyy - Extend options code
#define OPT_KERNEL_2_2	128
#define OPT_KERNEL_2_6	256

#define OPT_INVERT	4
#define OPT_ALTKBCODE	8
#define OPT_VERBOSE	16
#define OPT_VVERBOSE	32

#define OPT_DEBUG	64

unsigned long int if_info[15]; // current interface values.
unsigned long int l_if_info[15]; // last interface values.
unsigned char led_config[3] = {IF_COLL,IF_RX,IF_TX};
unsigned char options = 0;
int ttyfd;

FILE *fOut;
const char *szOutFile = "/tmp/aifled.log";

// Simmiyy - Added the PID file
const char *szPidFile = "/var/run/aifled.pid";

char szMsg[255];

// debug level (0 = none, 1 = startup info, 2 = verbose, 3 = very verbose)
int iDebug = 0;


void status_msg(char *szMessage, int iDebugLvl)
{
	char szMsg[255];

	// check debug level of this message less than app debug level
	if (iDebugLvl <= iDebug)
	{
		// check if debugging to file, and file is open
		if ((options & OPT_DEBUG) && (fOut != NULL))
		{
			sprintf(szMsg,"[%d] %s\n",iDebugLvl,szMessage);
			fprintf(fOut,szMsg);
		}
		// check if very verbose and not forking
		if ((options & OPT_VVERBOSE) && (!options & OPT_FORK))
			printf("%s\n",szMessage);
	}
}

void freakout(char *why)
{
	fprintf(stderr,"Error: %s\n",why);
	exit(1);
}

void set_led(char mode, char flag)
{
	unsigned char last_leds;
	ioctl(ttyfd,KDGETLED,&last_leds);
	if (options & OPT_INVERT)
		mode = !mode;
	if (mode)
		ioctl(ttyfd,KDSETLED,last_leds|flag);
	else
		ioctl(ttyfd,KDSETLED,last_leds&~flag);
	usleep(10);
}

void update_netproc(char *interface)
{
	char b[255];
	char dummy;
	FILE *procfd;
	FILE *f;

	// check if Smoothwall RED interface is up	
	if ((f = fopen("/var/smoothwall/red/active", "r")))
	{
		if_info[IF_ALIVE] = 1;
		fclose (f);
	}
	else
		if_info[IF_ALIVE] = 0;

	// net network interface detail
	if ((procfd = fopen("/proc/net/dev","r")) == NULL)
	{
		char *bp = b;

		status_msg("unable to open /proc/net/dev",2);
                // return;
                // freakout("Unable to open /proc/net/dev.");

		// populate b with dummy values
		sscanf(bp,"%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu", "0 0 0 0 0 0 0 0 0 0 0 0 0 0");
	}

	while(fgets(b,sizeof(b),procfd) !=  NULL)
	{
		char *bp = b;
	
		// skip over white space	
		while(*bp == ' ')
			*bp++;
		
		// look for interface name
		if(strncmp(bp,interface,strlen(interface)) == 0 && *(bp+strlen(interface)) == ':' )
		{
			bp = bp+strlen(interface)+1;

			// check for 2.0 kernel
			if(options & OPT_KERNEL_2_0)
				sscanf(bp,"%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
					&if_info[IF_RX],&if_info[IF_ERR_RX],&if_info[IF_DROP_RX],
					&dummy,&dummy,&if_info[IF_TX],&if_info[IF_ERR_TX],
					&if_info[IF_DROP_TX],&dummy,&if_info[IF_COLL]);

// Simmiyy - Modified the else statement
			// or for 2.2 or 2.4 kernel
			else if(options & OPT_KERNEL_2_2)
				sscanf(bp,"%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
					&if_info[IF_RX],&dummy,&if_info[IF_ERR_RX],&if_info[IF_DROP_RX],
					&dummy,&dummy,&dummy,&dummy,&if_info[IF_TX],&dummy,
					&if_info[IF_ERR_TX],&if_info[IF_DROP_TX],&dummy,&if_info[IF_COLL]);

// Simmiyy - Add 2.6 kernel capabilities
			else // so it's 2.6 kernel
				sscanf(bp,"%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
					&if_info[IF_RX],&dummy,&if_info[IF_ERR_RX],&if_info[IF_DROP_RX],
					&dummy,&dummy,&dummy,&dummy,&if_info[IF_TX],&dummy,
					&if_info[IF_ERR_TX],&if_info[IF_DROP_TX],&dummy,&if_info[IF_COLL],&dummy,&dummy);

			fclose(procfd);
			return;
		}
	}
	sprintf(szMsg,"unable to find interface %s\n", interface);
	status_msg(szMsg,2);
	fclose(procfd);
	// freakout("Unable to find interface.");
}

char is_changed(char temp)
{
	switch(temp)
	{
		case IF_RXTX:
			return (((if_info[IF_TX] != l_if_info[IF_TX]) || (if_info[IF_RX] != l_if_info[IF_RX])) ? TRUE : FALSE);
		case IF_DROP:
			return (((if_info[IF_DROP_TX] != l_if_info[IF_DROP_TX]) || (if_info[IF_DROP_RX] != l_if_info[IF_DROP_RX])) ? TRUE : FALSE);
		case IF_ERR:
			return (((if_info[IF_ERR_TX] != l_if_info[IF_ERR_TX]) || (if_info[IF_ERR_RX] != l_if_info[IF_ERR_RX])) ? TRUE : FALSE);
		default:
			return (if_info[temp] != l_if_info[temp] ? TRUE : FALSE);
	}
}

void update_leds(char *tty)
{
	char i;
	unsigned char real_status_leds[3] = {K_CAPSLOCK,K_NUMLOCK,K_SCROLLLOCK};
	unsigned char real_status;
	unsigned char leds[3] = {LED_NUM,LED_CAP,LED_SCR};
	for(i=0; i < 3; i++)
	{
		if(led_config[i] == IF_NONE && options & OPT_ALTKBCODE)
		{
			ioctl(ttyfd,KDGKBLED,&real_status);
			if(real_status & real_status_leds[i])
				set_led(TRUE,leds[i]);
			else
				set_led(FALSE,leds[i]);
			continue;
		}
		else if (led_config[i] == IF_ALIVE)
		{
			if (if_info[IF_ALIVE] == 1)
				set_led(TRUE,leds[i]);
			else
				set_led(FALSE,leds[i]);
			continue;
		}
		else if(led_config[i] == IF_NONE)
			continue;
		if(is_changed(led_config[i]))
			set_led(TRUE,leds[i]);
		else
			set_led(FALSE,leds[i]);
	}
	memcpy(&l_if_info,&if_info,sizeof(if_info));
}

char select_mode(char mode)
{
	switch(mode)
	{
		case 'r': return IF_RX;
		case 't': return IF_TX;
		case 'e': return IF_ERR;
		case 'c': return IF_COLL;
		case 'd': return IF_DROP;
		case 'a': return IF_RXTX;
		case 'u': return IF_DROP_RX;
		case 'i': return IF_DROP_TX;
		case 'j': return IF_ERR_RX;
		case 'k': return IF_ERR_TX;
		case 'l': return IF_ALIVE;
		default: return IF_NONE;
	}
}

void signal_handler(int signal)
{
	close(ttyfd);

// Simmiyy - Delete the pid file
	remove(szPidFile);

	exit(0);
}

void fork_program()
{
	pid_t program_pid;

	status_msg("about to fork...",3);
	program_pid = fork();

	sprintf(szMsg,"done forking, return value: %d", program_pid);
	status_msg(szMsg,3);

	// check for fork error
	if(program_pid == -1)
	{
		status_msg("unable to fork program",1);
		freakout("Unable to fork program.");
	}
	// stop execution of parent process, but leave child running...
	if(program_pid != 0)
	{

// Simmiyy - The father process create the PID file and exit
		FILE *f = NULL;
		if((f = fopen( szPidFile, "w")) != NULL){
			fprintf( f, "%d", program_pid );
			fclose(f);
		}

		status_msg("parent process terminating...",1);
		_exit(0);

// Simmiyy - Create a session and disconnect the child from controlling tty
	} else {

		setsid();
#ifdef TIOCNOTTY
		int fd_tty;
		if((fd_tty = open("/dev/tty", O_RDWR | O_NOCTTY)) != -1){
			ioctl(fd_tty, TIOCNOTTY, NULL);
			close(fd_tty);
			status_msg("disconnected from controlling tty...",3);
		}
#endif
	}
}

int main(int argc, char *argv[])
{
	FILE *procfd;
	int delay = 50;
	char tty[20] = "/dev/";
	struct utsname uname_dummy;
	char arg_dummy;
	int i;

long j=0;
iDebug = 5;
	
	if(argc < 3) 
	{
		printf(help,banner,argv[0]);	
		exit(0);
	}
	for(arg_dummy=3; arg_dummy < argc; arg_dummy++)
	{
		if(argv[arg_dummy][0] != '-')
		{
			printf("Error: option: %s\n", argv[arg_dummy]);
			exit(1);
		}
		switch(argv[arg_dummy][1])
		{
			case 'c':
				if(argv[arg_dummy+1] == NULL || strlen(argv[arg_dummy+1]) != 3)
					freakout("-c option needs 3 chars");
				led_config[0] = select_mode(argv[arg_dummy+1][0]);
				led_config[1] = select_mode(argv[arg_dummy+1][1]);
				led_config[2] = select_mode(argv[arg_dummy+1][2]);
				arg_dummy++;
				break;
			case 'd':
				if(argv[arg_dummy+1] == NULL)
					freakout("-d option needs an integer");
				delay = atol(argv[arg_dummy+1]);
				arg_dummy++;
				break;
			case 'f':
				options |= OPT_FORK;
				break;
			case 'i':
				options |= OPT_INVERT;
				break;
			case 'a':
				options |= OPT_ALTKBCODE;
				break;
			case 'v':
				if (options & OPT_VERBOSE)
					options |= OPT_VVERBOSE;
				else
					options |= OPT_VERBOSE;
				break;
			case 'D':
				options |= OPT_DEBUG;
				break;
			default:
				printf("Error: option: %s\n",argv[arg_dummy]);
				exit(1);
				break;
		}
	}
//if (options & OPT_VERBOSE)
//	iDebug = 2;
//if (options & OPT_VVERBOSE)
//	iDebug = 3;


	if (options & OPT_DEBUG)
	{
		if ((fOut = fopen(szOutFile, "w")) == NULL)
		{
			status_msg("failed to open output file",1);
		}
		else
		{
			status_msg("starting aifled....",2);
			status_msg("successfully created output file",3);

		}
	}

	// check if forking and verbose
//	if ((options & OPT_VERBOSE) && (options & OPT_FORK))
//		options -= OPT_VERBOSE;
//        if ((options & OPT_VVERBOSE) && (options & OPT_FORK))
//                options -= OPT_VVERBOSE;

	uname(&uname_dummy);

	sprintf(szMsg,"kernel version: %s", uname_dummy.release);
	status_msg(szMsg,3);

	if(strncmp(uname_dummy.release,"2.0",3) == 0)
		options |= OPT_KERNEL_2_0;

// Simmiyy - Extend Kernel family selection
	else if(strncmp(uname_dummy.release,"2.2",3) == 0 || 
		strncmp(uname_dummy.release,"2.4",3) == 0)
		options |= OPT_KERNEL_2_2;
//	else if(strncmp(uname_dummy.release,"2.6",3) == 0)
//		options |= OPT_KERNEL_2_6;

	strcat(tty,argv[1]);

	sprintf(szMsg,"opening tty %s...",tty);
	status_msg(szMsg,2);

	if((ttyfd = open(tty,O_RDWR)) < 0)
	{
		sprintf(szMsg,"unable to open tty %s",tty);
		status_msg(szMsg,1);
		freakout("Unable to open tty.");
	}
	else
	{
		sprintf(szMsg,"successfully opened tty %s",tty);
		status_msg(szMsg,2);
	}

	if(options & OPT_FORK)
		fork_program();
	else
		printf(banner);

//	if (options & OPT_VVERBOSE)
//		printf("running in very verbose mode...\n");
//	else if (options & OPT_VERBOSE)
//		printf("running in verbose mode...\n");

	if (options & OPT_VERBOSE)
	{
		sprintf(szMsg,"monitoring interface %s on tty %s", argv[2], argv[1]);
		status_msg(szMsg,1);

		for (i=0; i<3; i++)
		{
			sprintf(szMsg,"led%d: ", i);
			switch (led_config[i])
			{
				case IF_RX: 
					strcat(szMsg,"receive"); break; 
				case IF_TX:
					strcat(szMsg,"transmit"); break;
				case IF_COLL:
					strcat(szMsg,"collision"); break;
				case IF_DROP_RX:
					strcat(szMsg,"drop (receive)"); break;
				case IF_DROP_TX:
					strcat(szMsg,"drop (transmit)"); break;
				case IF_ERR_TX:
					strcat(szMsg,"error (transmit)"); break;
				case IF_ERR_RX:
					strcat(szMsg,"error (receive)"); break;
				case IF_RXTX:
					strcat(szMsg,"activity"); break;
				case IF_DROP:
					strcat(szMsg,"drop"); break;
				case IF_ERR:
					strcat(szMsg,"error"); break;
				case IF_NONE:
					strcat(szMsg,"none"); break;
				case IF_ALIVE:
					strcat(szMsg,"red interface alive"); break;
			}
			status_msg(szMsg,1);

		}
	}

	status_msg("SIGINT signal_handler....",3);
	signal(SIGINT,signal_handler);

	status_msg("SIGTERM signal_handler....",3);
	signal(SIGTERM,signal_handler);  

	status_msg("update_netproc....",3);
	update_netproc(argv[2]);

	status_msg("memcpy....",3);
	memcpy(&l_if_info,&if_info,sizeof(if_info));
	while(1)
	{
//		if (options & OPT_VVERBOSE)
//		{
//printf("%5d: ",j);
//sprintf(szMsg,"%5d: ",j);
//status_msg(szMsg,2);
//j++;
//			printf("tx:%d rx:%d coll:%d tx_drop:%d rx_drop:%d err_tx:%d err_rx:%d alive:%d\n",
//				if_info[IF_TX],if_info[IF_RX],if_info[IF_COLL],if_info[IF_DROP_TX],
//				if_info[IF_DROP_RX],if_info[IF_ERR_TX],if_info[IF_ERR_RX],if_info[IF_ALIVE]);

			sprintf(szMsg,"tx:%d rx:%d coll:%d tx_drop:%d rx_drop:%d err_tx:%d err_rx:%d alive:%d",
				if_info[IF_TX],if_info[IF_RX],if_info[IF_COLL],if_info[IF_DROP_TX],
				if_info[IF_DROP_RX],if_info[IF_ERR_TX],if_info[IF_ERR_RX],if_info[IF_ALIVE]);
			status_msg(szMsg,2);
//		}

       		status_msg("update_netproc....",3);
		update_netproc(argv[2]);

		status_msg("update_leds....",3);
		update_leds(tty);
		
		status_msg("usleep....",3);
		usleep(delay*1000);

		status_msg("done",3);
	}
	close(ttyfd);
	return 0;
}
