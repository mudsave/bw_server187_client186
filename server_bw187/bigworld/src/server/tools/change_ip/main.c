/*
 *	This simple program is used to change the IP address of an interface of this
 *	machine. It is designed to be run by lcproxymgr.
 *
 *	It runs the script ../scripts/change_ip.sh. It tries to do this as root.
 *	Once this program has been compiled, the following should be done:
 *
 *	Change $MF_ROOT/bigworld/bin/change_ip.sh to correctly change the IP address of the
 *	machine.
 *
 *	Then, as root:
 *		chown root $MF_ROOT/bigworld/bin/scripts/change_ip.sh
 *		chmod 700 $MF_ROOT/bigworld/bin/scripts/change_ip.sh
 *		chown root $MF_ROOT/bigworld/bin/$MF_CONFIG/change_ip
 *		chmod +s $MF_ROOT/bigworld/bin/$MF_CONFIG/change_ip
 */

#include <errno.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/types.h>

int main( int argc, char * argv[] )
{
	if (argc != 3)
	{
		printf( "Usage: %s interface address\n", argv[0] );
		return -1;
	}

	char * uid = getenv( "UID" );

	if (setuid( 0 ) == -1)
	{
		printf( "setuid failed - %s\n", strerror( errno ) );
		printf( "Check file permisions, perhaps you need to do the following (as root):\n"
			"   chown root %s\n"
			"   chmod +s %s\n"
			"Also, refer to the Bigworld Server Guide, under Lcproxymgr Fault Tolerance - Administration\n"
			, argv[0], argv[0] );
		return -1;
	}

	if (execl( "../scripts/change_ip.sh", "change_ip.sh", argv[1], argv[2], 0 ) == -1)
	{
		printf( "execl failed - %s\n", strerror( errno ) );
		return -1;
	}

	return 0;
}

/* main.c */
