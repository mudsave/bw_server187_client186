from controls import *

args = \
(
	WatcherCheckBox( "Draw", "Client Settings/Water/draw" ),
        
        WatcherCheckBox( "Draw_Dynamics", "Client Settings/Water/Draw Dynamics" ),
        WatcherCheckBox( "Draw_Player", "Client Settings/Water/Draw Player" ),
        WatcherCheckBox( "Draw_Trees", "Client Settings/Water/Draw Trees" ),
        
        WatcherFloat( "Scene_Cull_Distance", "Client Settings/Water/Scene/Cull Distance" ),
        
        WatcherCheckBox( "Disable_DPVS", "Client Settings/Water/Scene/Disable dpvs" ),
        
        WatcherCheckBox( "Draw_Refraction", "Client Settings/Water/draw refraction" ),
        WatcherCheckBox( "Simple_Refraction", "Client Settings/Water/Simple refraction" ),
        
        WatcherCheckBox( "Wireframe", "Client Settings/Water/wireframe" ),
        
        WatcherFloat( "Reflection_Fudge", "Client Settings/Water/Scene/Reflection Fudge" ),
        
        WatcherFloat( "Water_Speed_Squared", "Client Settings/Water/water speed square" ),
        
        WatcherFloat( "Max_Reflection_Distance", "Client Settings/Water/Max Reflection Distance" ),
        WatcherInt( "Max_Reflections", "Client Settings/Water/Max Reflections" ),
        
        WatcherFloat( "Character_Impact", "Client Settings/Water/character impact" ),
        
)

commands = \
(
)