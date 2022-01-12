from sqlobject import *
from datetime import datetime
from turbogears.database import PackageHub
from turbogears import identity
from web_console.common import model

hub = PackageHub( "web_console" )
__connection__ = hub

class ccStartPrefs( model.DictSQLObject ):

	user = ForeignKey( "User" )
	mode = StringCol( length=10 )
	arg = StringCol( length=64 )
	useTags = BoolCol()

	RENAME_COLS = [('uid', 'user_id'),
				   ('uid_id', 'user_id')]


class ccSavedLayouts( model.DictSQLObject ):

	user = ForeignKey( "User" )
	name = StringCol()
	serveruser = StringCol()
	xmldata = StringCol()

	RENAME_COLS = [('uid', 'user_id'),
				   ('uid_id', 'user_id')]

ccStartPrefs.createTable( True )
ccSavedLayouts.createTable( True )
