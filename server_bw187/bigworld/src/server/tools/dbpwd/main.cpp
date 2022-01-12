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

#include<iostream>
//#include "../../../common/db_pwd_des.h"
#include "../../../common/des.h"
using namespace std;

int main( int argc, char * argv[] )
{
    if((argc < 3) || (argv[1][0] != '-') ||((argv[1][1] != 'd')&&(argv[1][1] != 'e')))
    {
        cout<<"useage: \n\t./desTool -d XXXXXX"<<endl<<"\t./desTool -e XXXXXX"<<endl;
        return 0;
    }
    
    //CPwdDesTool dt;
    CDES          dt;
    if(argv[1][1] == 'e')
    {
        string src(argv[2]);
        string des = dt.EncryptString(src);
        string tmp = dt.DecryptString(des);
        if( src == tmp)
        {
            cout<<des<<endl;
        }
        else
        {
            cout<<"password error! please change another password"<<endl;
        }
    }
    else
    {
        cout<<dt.DecryptString(argv[2])<<endl;
    }
    return 0;
}

/* main.c */
