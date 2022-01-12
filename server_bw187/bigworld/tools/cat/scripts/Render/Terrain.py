from controls import *

args = \
(
	WatcherCheckBox( "Draw Terrain", "Render/Terrain/draw"),
	WatcherFloatSlider( "Tiling Frequency", "Render/Terrain/texture tile frequency", minMax = (0.0, 1.0) ),
	WatcherFloatSlider( "Specular Diffuse amount", "Render/Terrain/specular diffuse amount", minMax = (0.0, 1.0) ),
	WatcherFloatSlider( "Specular Multiplier", "Render/Terrain/specular multiplier", minMax = (0.0, 1.0) ),
	WatcherIntSlider( "Specular Power", "Render/Terrain/specular power", minMax = (1, 10) ),
)

commands = \
(
)
