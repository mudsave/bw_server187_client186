#!/bin/sh
wget -O Register http://transfer.gyyx.cn:81/CsServiceV2/AutoActivate.asmx?wsdl
#../../bin/linux386/wsdl2h -s -o http://transfer.gyyx.cn:81/CsServiceV2/AutoActivate.asmx?wsdl
#../../bin/linux386/soapcpp2 -i GameAccount.h  > /dev/null
wsdl2h -s  Register
soapcpp2 -i Register.h
