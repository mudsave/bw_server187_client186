# This file is no longer in use.
# Allan 12/6/03

import particles
import ParticleEditorModule
#import ParticleSystem

jetStream = None

def init( hmmm ):
	# hmmm is auto passed from the call from C, ignore it.. i guess
	global jetStream
	jetStream = particles.createJetStream()
	jetStream.save( "jet_stream.xml" )
	ParticleEditorModule.setParticleSystem( jetStream )


def update( dTime ):
	# hmmm is auto passed from the call from C, ignore it.. i guess
	# print dTime
	jetStream.update( dTime )
	
	
def render( hmmm ):
	# hmmm
	jetStream.render()
	
