from sqlobject import *
from datetime import datetime
from turbogears.database import PackageHub
from turbogears import identity

hub = PackageHub( "web_console" )
__connection__ = hub

class StatGrapherPrefs( SQLObject ):
	class sqlmeta:
		table = "sg_prefs"
	uid = ForeignKey( "User" )
	log = StringCol( length=255 )
	displayPrefs = PickleCol( length=2**16, pickleProtocol=0 )
	user = SingleJoin( "User" )

StatGrapherPrefs.createTable( True )
