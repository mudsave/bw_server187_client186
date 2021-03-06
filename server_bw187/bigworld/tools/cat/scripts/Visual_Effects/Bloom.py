from controls import *

args = \
(
	WatcherCheckBox( "Bloom Effect Enabled", "Client Settings/fx/Bloom/enable" ),
	
	WatcherFloatEnum( "Filter Mode", "Client Settings/fx/Bloom/filter mode",
		(	( "4 x 4",		1 ),
			( "24 x 24",	3 )
		) ),
	
	WatcherCheckBox( "Bloom Blur", "Client Settings/fx/Bloom/bloom and blur" ),
	
	WatcherFloatSlider( "Colour Attenuation", "Client Settings/fx/Bloom/colour attenuation", minMax = (0.0, 8.0) ),
	
	WatcherIntSlider( "Number of Passes", "Client Settings/fx/Bloom/num passes", minMax = (1, 30) )
)

commands = \
(
)