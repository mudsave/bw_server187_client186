/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/


#include "pch.hpp"
#include "streamed_animation_channel.hpp"
#include "cstdmf/diary.hpp"
#include "cstdmf/memory_counter.hpp"
#include "cstdmf/binaryfile.hpp"
#include "interpolated_animation_channel.hpp"

DECLARE_DEBUG_COMPONENT2( "Moo", 0 )

memoryCounterDeclare( animation );
memoryCounterDefine( streamAnim, Animation );


/** 
 *	The SEPARATE_READ_THREAD token specifies whether streamed animations
 *	should use a separate thread for ReadFile operations.
 *
 *	If this token is not defined, ReadFile will block the main thread, causing
 *  occasional frame-rate glitches.
 */
#define SEPARATE_READ_THREAD	1


namespace Moo
{

// -----------------------------------------------------------------------------
// Section: StreamedAnimation
// -----------------------------------------------------------------------------

static uint32 s_CRCons = 0;
static uint32 s_CRDest = 0;
static uint32 s_CRLive = uint32(-1);

/**
 *	Constructor
 */
StreamedAnimation::StreamedAnimation() :
	blocks_( NULL )
{
	memoryCounterAdd( animation );
	memoryClaim( this );

	if (s_CRLive == uint32(-1))
	{
		s_CRLive = 0;
		MF_WATCH(	"Memory/StreamedBlocks_Cons", s_CRCons,
					Watcher::WT_READ_ONLY, 
					"Number of streamed animation cache records constructed" );
		MF_WATCH(	"Memory/StreamedBlocks_Dest", s_CRDest,
					Watcher::WT_READ_ONLY, 
					"Number of streamed animation cache records destructed" );
		MF_WATCH(	"Memory/StreamedBlocks_Live", s_CRLive,	
					Watcher::WT_READ_ONLY, 
					"Number of streamed animation cache records currently "
					"in memory" );
		MF_WATCH(	"Memory/StreamedBlocks_CacheSize", 
					StreamedAnimation::s_cacheSize_,
					Watcher::WT_READ_ONLY, 
					"The number of bytes used for streamed blocks" );
		MF_WATCH(	"Memory/StreamedBlocks_CacheMaxSize", 
					StreamedAnimation::s_cacheMaxSize_,
					Watcher::WT_READ_WRITE, 
					"The maximum number bytes that will be used for streamed "
					"animation blocks" );
	}
}

/**
 *	Destructor
 */
StreamedAnimation::~StreamedAnimation()
{
	memoryCounterSub( animation );

	memoryClaim( channels_ );

	if (blocks_ != NULL)
	{
		// get rid of any blocks we have loaded
		for (uint i = 0; i < numBlocks_; i++)
		{
			StreamedBlock & sb = blocks_[i];
			if (sb.data_ != NULL)
			{
				// note: if we are still reading in, the block
				// destructor will wait for it to finish.
				bool owned = sb.data_->owner_ != NULL;
				delete sb.data_;
				MF_ASSERT( sb.data_ == NULL || (!owned) );
			}
		}

		memoryClaim( blocks_ );
		delete [] blocks_;
	}

	memoryClaim( this );
}


/**
 *	Prep a channel for streaming with this streamed animation.
 *	This should only be called for channels that want to be
 *	added after loading, i.e. during conversion.
 */
void StreamedAnimation::prep( StreamedAnimationChannel * pChannel )
{
	MF_ASSERT( blocks_ != NULL );

	StreamedAnimationChannel & sac = *pChannel;
	sac.pBlocks_ = new ChannelBlock*[numBlocks_];
	sac.numBlocks_ = numBlocks_;
	for (uint j = 0; j < numBlocks_; j++) sac.pBlocks_[j] = NULL;

	memoryCounterAdd( animation );
	memoryClaim( sac.pBlocks_ );
}


/**
 *	Add a channel to our list
 */
void StreamedAnimation::add( StreamedAnimationChannel * pChannel )
{
	channels_.push_back( pChannel );

	// if we haven't loaded yet then this is all we need do
	if (blocks_ == NULL) return;

	// if we're here, then this channel was added after we loaded,
	// i.e. it is new and we should add it to the cache.

	// see if this channel punches up the block header size
	int chanNum = channels_.size() - 1;
	bool upSize = ((chanNum & 3) == 0);

	// add this channel's data to each of our blocks
	for (uint i = 0; i < numBlocks_; i++)
	{
		StreamedBlock & sb = blocks_[i];
		ChannelBlock * cb = pChannel->pBlocks_[i];
		uint8 cbWords = cb->calcNumWords();

		// make sure we're not trying to add a channel to an animation
		// that is already partly streamed (eugh) - we don't support it
		if (sb.offset_ != uint32(-1))
		{
			ERROR_MSG( "StreamedAnimation::add: "
				"Cannot add new channel to already loaded streamed anim\n" );
			return;
		}

		// allocate or reallocate the data pointer
		uint32 newsize = (cbWords + int(upSize)) * sizeof(uint32);
		if (sb.data_ == NULL)
			sb.data_ = new CacheRecord( NULL, &sb );
		sb.data_->data_ = realloc( sb.data_->data_, sb.size_ + newsize );

		// set our word count
		if (upSize)
		{
			memmove( ((uint8*)sb.data_->data_) + chanNum + sizeof(uint32),
				((uint8*)sb.data_->data_) + chanNum,
				sb.size_ - chanNum );
			*(uint32*)(((uint8*)sb.data_->data_) + chanNum) = 0;
			sb.size_ += sizeof(uint32);
		}
		((uint8*)sb.data_->data_)[chanNum] = cbWords;

		// and copy our data in
		uint32 cbSize = cbWords * sizeof(uint32);
//		dprintf( "2. cb %d is 0x%08X, size %d\n", i, cb, cbSize );
		memcpy( ((uint8*)sb.data_->data_) + sb.size_, cb, cbSize );
		sb.size_ += cbSize;

		// finally, free the source channel block and reset it to NULL,
		// since they're supposed to point to our data
		free( pChannel->pBlocks_[i] );
		pChannel->pBlocks_[i] = NULL;
	}

	// TODO: If we wanted to actually use this animation now we'd have
	// to go and fix up all the channel block pointers in each channel.
	// Maybe there could be a fixup function to do this if so desired.
}


/**
 *	Load in the data for and set up this streamed animation
 */
void StreamedAnimation::load( const BinaryFile & bf, float totalTime,
	StreamedDataCache * pSource, uint32 streamedOffset )
{
	memoryCounterAdd( animation );

	// store our source
	pSource_ = pSource;

	// find out how many blocks and allocate space for them
	numBlocks_ = (uint32(ceilf( totalTime )) + (STREAM_BLOCK_SIZE-1)) /
		STREAM_BLOCK_SIZE;
	blocks_ = new StreamedBlock[numBlocks_];
	memoryClaim( blocks_ );

	// read in the sizes of each block
	for (uint i = 0; i < numBlocks_; i++)
	{
		StreamedBlock & sb = blocks_[i];
		sb.offset_ = streamedOffset;
		sb.data_ = NULL;
		sb.reader_ = NULL;
		// see if we are not yet in the cache
		if (streamedOffset != uint32(-1))
		{
			bf >> sb.size_;
			streamedOffset += sb.size_;
		}
		else
		{
			sb.size_ = 0;
		}
	}

	// tell all our channels how many blocks to expect
	for (uint i = 0; i < channels_.size(); i++)
	{
		this->prep( &*channels_[i] );
	}
	memoryClaim( channels_ );
}

/**
 *	Save our preload data to the given binary file
 */
void StreamedAnimation::save( BinaryFile & bf )
{
	for (uint i = 0; i < numBlocks_; i++)
	{
		bf << blocks_[i].size_;
	}
}

/**
 *	Save our streamed animation data out to the anim cache.
 *	This method should only be called on a newly created animation.
 */
uint32 StreamedAnimation::dam( uint32 streamedOffset )
{
	uint32 streamedSize = 0;

	// just write out each block
	for (uint i = 0; i < numBlocks_; i++)
	{
		StreamedBlock & sb = blocks_[i];
		sb.offset_ = streamedOffset;
		if (sb.data_ != NULL)
			pSource_->immsave( sb.offset_, sb.size_, sb.data_->data_ );

		streamedOffset += sb.size_;
		streamedSize += sb.size_;
	}

	// and return how much we wrote
	return streamedSize;
}


/**
 *	This method touches the given time in the animation.
 *	Currently this is the only way of getting anything loaded,
 *	i.e. after you already want to use it :)
 */
void StreamedAnimation::touch( float time )
{
	uint32 block = uint32(time) / STREAM_BLOCK_SIZE;
	if (block >= numBlocks_) return;

	StreamedBlock & sb = blocks_[block];	

	// see if we are reading something in
	if (sb.reader_ == NULL)
	{ // not reading in
		
		// see if the data is already there
		if (sb.data_ != NULL)
		{
			sb.data_->touch();
			return;	// hopefully the common case!
		}

		// ok, let's load then
		sb.data_ = new CacheRecord( this, &sb );
		sb.reader_ = pSource_->load( sb.offset_, sb.size_, sb.data_->data_ );
		//dprintf( "0x%08X Started reading block %d\n", this, block );
	}
	else
	{ // is reading in
		
		// see if the reading is complete
		if (pSource_->fileAccessDone( sb.reader_ ))
		{
			//dprintf( "0x%08X Finished reading block %d\n", this, block );
#ifdef SEPARATE_READ_THREAD
			bool ok = sb.reader_->finished_ == 1;
#else
			bool ok = true;
#endif

			delete sb.reader_;
			sb.reader_ = NULL;

			// tell everyone about it then
			if (ok) this->bind( block );
		}

		// make sure it doesn't disappear underneath us anyway
		sb.data_->touch();
	}
}

/**
 *	This private method flicks the given block from our streamed in blocks
 */
void StreamedAnimation::flick( uint32 block )
{
	StreamedBlock & sb = blocks_[block];

	// wait for it to finish loading if it is still doing so
	// this could happen if there are great demands on a very small cache
	if (sb.reader_ != NULL)
	{
		while (!pSource_->fileAccessDone( sb.reader_ ))
			Sleep( 5 );

		delete sb.reader_;
		sb.reader_ = NULL;
	}
	else
	{
		// otherwise loose any channels bound to this block
		this->loose( block );
	}

	// and we no longer have a record of it
	sb.data_ = NULL;

	// we do not delete the CacheRecord because
	// we are being called by its destructor (yep)
}


/**
 *	This method binds a block that has finished being read in.
 */
void StreamedAnimation::bind( uint32 block )
{
	StreamedBlock & sb = blocks_[block];

	// tell all channels about the data
	uint numChannels = channels_.size();
	uint32 offset = (numChannels + 3) & ~3L;
	uint8 * sizes = (uint8*)sb.data_->data_;
	for (uint i = 0; i < numChannels; i++)
	{
		uchar numWords = *sizes++;
		if (numWords != 0)
		{
			channels_[i]->pBlocks_[block] =
				(ChannelBlock*)(((uint8*)sb.data_->data_) + offset);
			offset += uint32(numWords) * sizeof(uint32);
		}
		// if no data in block for this channel,
		// then it just uses its fallback transform.
	}
}

/**
 *	This method looses a block in preparation for unloading.
 */
void StreamedAnimation::loose( uint32 block )
{
	StreamedBlock & sb = blocks_[block];

	// tell all channels about the data
	uint numChannels = channels_.size();
	for (uint i = 0; i < numChannels; i++)
	{
		channels_[i]->pBlocks_[block] = NULL;
	}
}




/**
 *	Constructor
 */
StreamedAnimation::CacheRecord::CacheRecord(
		StreamedAnimation * owner, StreamedBlock * block ) :
	data_( NULL ),
	owner_( owner ),
	next_( NULL ),
	prev_( NULL )
{
	while ( ( s_cacheSize_ + block->size_ ) > s_cacheMaxSize_ &&
			s_cacheTail_ != NULL && s_cacheTail_ != this )
	{
		delete s_cacheTail_;
	}

	data_ = malloc( block->size_ );

	if (owner_ != NULL)
	{
		s_cacheSize_ += block->size_;

		s_CRCons++;
		s_CRLive++;

		memoryCounterAdd( streamAnim );
		memoryClaim( this );
		memoryClaim( data_ );

		this->link();
	}
}

/**
 *	Destructor
 */
StreamedAnimation::CacheRecord::~CacheRecord()
{
	if (owner_ != NULL)
	{
		s_CRDest++;
		s_CRLive--;

		this->unlink();

		StreamedBlock * block = NULL;
		for (uint i = 0; i < owner_->numBlocks_; i++)
			if ((block=&owner_->blocks_[i])->data_ == this) break;
		MF_ASSERT( block );

		MF_ASSERT( ( s_cacheSize_ - block->size_ ) <= s_cacheSize_ );
		s_cacheSize_ -= block->size_;

		owner_->flick( block - owner_->blocks_ );

		memoryCounterSub( streamAnim );
		memoryClaim( data_ );
		memoryClaim( this );
	}

	free( data_ );
}


/**
 *	Touch this cache record
 */
void StreamedAnimation::CacheRecord::touch()
{
	if (s_cacheHead_ == this) return;

	this->unlink();
	this->link();
}


/**
 *	Link into the head of the cache chain
 */
void StreamedAnimation::CacheRecord::link()
{
	next_ = s_cacheHead_;
	s_cacheHead_ = this;
	if (next_ != NULL)
		next_->prev_ = this;
	else
		s_cacheTail_ = this;
}

/**
 *	Unlink from the cache chain
 */
void StreamedAnimation::CacheRecord::unlink()
{
	if (next_ != NULL)
		next_->prev_ = prev_;
	else
		s_cacheTail_ = prev_;
	if (prev_ != NULL)
		prev_->next_ = next_;
	else
		s_cacheHead_ = next_;
	next_ = NULL;
	prev_ = NULL;
}


/// static initialisers
StreamedAnimation::CacheRecord * StreamedAnimation::s_cacheHead_ = NULL;
StreamedAnimation::CacheRecord * StreamedAnimation::s_cacheTail_ = NULL;
size_t StreamedAnimation::s_cacheSize_		= 0;
/**
 *	The maximum number bytes that will be used for streamed animation blocks.
 *	Accessible through the watcher 'Memory/StreamedBlocks_CacheMaxSize'
 *	Setting this value too low can cause the animation system to be starved of 
 *	data resulting in animation glitches and excessive disk usage.
 */
size_t StreamedAnimation::s_cacheMaxSize_	= 1<<20;

THREADLOCAL( StreamedDataCache* ) StreamedAnimation::s_loadingAnimCache_= NULL;
THREADLOCAL( StreamedDataCache* ) StreamedAnimation::s_savingAnimCache_ = NULL;

THREADLOCAL( StreamedAnimation* ) StreamedAnimation::s_currentAnim_ = NULL;



// -----------------------------------------------------------------------------
// Section: ChannelBlock
// -----------------------------------------------------------------------------

uint8 ChannelBlock::calcNumWords() const
{
	uint8 endWord = 255;
	const uint8 * roster;
	uint8 timeSum;

#define calcNumWords_PART( kname, kchar, ktype )					\
	timeSum = 0;													\
	roster = this->kname##Roster();									\
	const ktype * kname##KeyData = this->kname##KeyData();			\
	do																\
	{																\
		uint8 durHere = *roster++;									\
		timeSum += durHere & 0x7F;									\
		if (!(durHere & 0x80)) kname##KeyData++;					\
	} while (timeSum < STREAM_BLOCK_SIZE);							\
	if (!(kchar##Header_.rosterOffset_ & 0x80)) kname##KeyData++;	\
	if (kname##KeyData != this->kname##KeyData())					\
		endWord = ((uint32*)kname##KeyData) - ((uint32*)this);		\

	calcNumWords_PART( scale, s, Vector3 )
	calcNumWords_PART( position, p, Vector3 )
	calcNumWords_PART( rotation, r, Quaternion )

	// if we want to use the fallback transform for everything,
	// then this block is effectively empty
	if (endWord == 255) return 0;

	// otherwise it's just up to endWord
	return endWord;
}

// -----------------------------------------------------------------------------
// Section: StreamedAnimationChannel
// -----------------------------------------------------------------------------


/**
 *	Constructor.
 */
StreamedAnimationChannel::StreamedAnimationChannel() :
	pBlocks_( NULL ),
	numBlocks_( 0 )
{
}


/**
 *	Destructor.
 */
StreamedAnimationChannel::~StreamedAnimationChannel()
{
	memoryCounterSub( animation );

	// our StreamedAnimation had better have forgotten about us!
	if (pBlocks_ != NULL)
	{
		memoryClaim( pBlocks_ );
		delete [] pBlocks_;
		pBlocks_ = NULL;
	}
}

/**
 *	Copy constructor.
 */
StreamedAnimationChannel::StreamedAnimationChannel(
		const StreamedAnimationChannel & other ) :
	pBlocks_( NULL ),
	numBlocks_( 0 ),
	scaleFallback_( other.scaleFallback_ ),
	positionFallback_( other.positionFallback_ ),
	rotationFallback_( other.rotationFallback_ )
{
}


/**
 *	Slow result method
 */
Matrix StreamedAnimationChannel::result( float time ) const
{
	Matrix m;
	this->result( time, m );
	return m;
}


/**
 *	Faster result method
 */
void StreamedAnimationChannel::result( float time, Matrix & out ) const
{
	BlendTransform bt;
	this->result( time, bt );
	bt.output( out );
}

/**
 *	Fastest result method
 */
void StreamedAnimationChannel::result( float ftime, BlendTransform & out ) const
{
	// see if the block has been loaded
	uint32 itime = uint32(ftime);
	uint32 block = itime / STREAM_BLOCK_SIZE;
	itime -= block * STREAM_BLOCK_SIZE;
	ftime -= block * STREAM_BLOCK_SIZE;
	if (block < numBlocks_ && pBlocks_[block] != NULL)
	{
		// use loaded block then
		ChannelBlock & cb = *pBlocks_[block];
		uint8 lowTime, highTime;
		const uint8 * roster;
		uint8 durHere;
		float param, fdur;

#define SAC_result_PART( kname, kheader, ktype, interpolator )				\
		/* find kname key */												\
		highTime = 0;														\
		roster = cb.kname##Roster();										\
		const ktype * kname##KeyData = cb.kname##KeyData();					\
		do																	\
		{																	\
			durHere = *roster++;											\
			highTime += (durHere & 0x7F);									\
			if (!(durHere & 0x80)) kname##KeyData++;						\
		} while (highTime <= itime);										\
																			\
		/* ok, now scaleKeyData-1 at time lowTime = (highTime-durHere&0x7f)
		// is the bottom key, and scaleKeyData at time highTime is the top key.
		// if durHere >> 7 then bottom data is fallback not scaleKeyData-1
		// if (*roster) >> 7 then top data is fallback
		// boundaries:
		// if lowTime is 0 then subtract durBefore from it
		// if highTime is STREAM_BLOCK_SIZE then add durAfter to it
		// if highTime is STREAM_BLOCK_SIZE then look at rosterOffset>>7 not
		//  *roster to see if top data is fallback */						\
		lowTime = highTime - (durHere & 0x7F);								\
																			\
		const ktype * kname##BotKey, * kname##TopKey;						\
		param = ftime;														\
		fdur = float(durHere & 0x7F);										\
																			\
		/* get bot key and update param & fdur */							\
		if (lowTime != 0)													\
		{																	\
			param -= lowTime;												\
		}																	\
		else																\
		{																	\
			float f = kheader.durBefore_;									\
			param += f;														\
			fdur += f;														\
		}																	\
		/*botTime = lowTime ? lowTime : -int(cb.sHeader_.durBefore);*/		\
		kname##BotKey = (!(durHere & 0x80)) ?								\
			kname##KeyData - 1 : &kname##Fallback_;							\
																			\
		/* get top key and update param & fdur */							\
		if (highTime != STREAM_BLOCK_SIZE)									\
		{																	\
			durHere = *roster;												\
			/*topTime = highTime;*/											\
		}																	\
		else																\
		{																	\
			durHere = kheader.rosterOffset_;								\
			/*topTime = highTime + cb.sHeader_.durAfter_;*/					\
			fdur += kheader.durAfter_;										\
		}																	\
		kname##TopKey = (!(durHere & 0x80)) ?								\
			kname##KeyData : &kname##Fallback_;								\
																			\
		ktype kname##Out;													\
		interpolator( &kname##Out, kname##BotKey, kname##TopKey, param / fdur )


		SAC_result_PART( scale, cb.sHeader_, Vector3, XPVec3Lerp );
		SAC_result_PART( position, cb.pHeader_, Vector3, XPVec3Lerp );
		SAC_result_PART( rotation, cb.rHeader_, Quaternion, XPQuaternionSlerp );

		out = BlendTransform( rotationOut, scaleOut, positionOut );

		/*
		out = BlendTransform(
			cb.rotationKey( itime, rotationFallback_ ),
			cb.scaleKey( itime, scaleFallback_ ),
			cb.positionKey( itime, positionFallback_ ) );
		*/
	}
	else
	{
		// use fallback transform
		out = BlendTransform(
			rotationFallback_, scaleFallback_, positionFallback_ );
	}
}



/**
 *	Load method.
 */
bool StreamedAnimationChannel::load( const BinaryFile & bf )
{
	// our StreamedAnimation will take care of setting up the blocks stuff
	if (StreamedAnimation::s_currentAnim_ != NULL)
		StreamedAnimation::s_currentAnim_->add( this );

	this->AnimationChannel::load( bf );

	bf >> scaleFallback_;
	bf >> positionFallback_;
	bf >> rotationFallback_;

	return true;
}


/**
 *	Save method.
 */
bool StreamedAnimationChannel::save( BinaryFile & bf ) const
{
	// our StreamedAnimation will take care of saving out the blocks stuff

	this->AnimationChannel::save( bf );

	bf << scaleFallback_;
	bf << positionFallback_;
	bf << rotationFallback_;

	return true;
}

void StreamedAnimationChannel::preCombine( const AnimationChannel & rOther )
{
	// nup
}

void StreamedAnimationChannel::postCombine( const AnimationChannel & rOther )
{
	// nothing doing
}

/**
 *	Mirror the given interpolated animation channel.
 */
void StreamedAnimationChannel::mirror( const InterpolatedAnimationChannel & iac )
{
	typedef Moo::InterpolatedAnimationChannel::ScaleKeyframes ScaleKeyframes;
	typedef Moo::InterpolatedAnimationChannel::PositionKeyframes PositionKeyframes;
	typedef Moo::InterpolatedAnimationChannel::RotationKeyframes RotationKeyframes;

	// copy the base class AnimationChannel data
	this->identifier( iac.identifier() );

	// make sure we have at least one key of each type
	ScaleKeyframes scaleSingle;
	PositionKeyframes positionSingle;
	RotationKeyframes rotationSingle;
	if (iac.scaleKeys_.empty()) scaleSingle.push_back(
		std::make_pair( 0.f, Vector3( 1.f, 1.f, 1.f ) ) );
	if (iac.positionKeys_.empty()) positionSingle.push_back(
		std::make_pair( 0.f, Vector3( 0.f, 0.f, 0.f ) ) );
	if (iac.rotationKeys_.empty()) rotationSingle.push_back(
		std::make_pair( 0.f, Quaternion( 0.f, 0.f, 0.f, 1.f ) ) );
	const ScaleKeyframes & iac_scaleKeys =
		iac.scaleKeys_.empty() ? scaleSingle : iac.scaleKeys_;
	const PositionKeyframes & iac_positionKeys =
		iac.positionKeys_.empty() ? positionSingle : iac.positionKeys_;
	const RotationKeyframes & iac_rotationKeys =
		iac.rotationKeys_.empty() ? rotationSingle : iac.rotationKeys_;

	// take the first key of each and make it our fallback
	scaleFallback_ = iac_scaleKeys[0].second;
	positionFallback_ = iac_positionKeys[0].second;
	rotationFallback_ = iac_rotationKeys[0].second;

	// now for each block, put the appropriate keys in it

	// sit points to current time, sit-1 (if valid), points to
	// previous time. if sit-1 isn't valid (==sbegin), then a constant
	// will be used, likewise when sit==send.

	// find the first valid keyframe
	ScaleKeyframes::const_iterator sbegin = iac_scaleKeys.begin();
	ScaleKeyframes::const_iterator sit = sbegin;
	ScaleKeyframes::const_iterator send = iac_scaleKeys.end();
    while (sit != send && sit->first < 0.f) 
	{
		sit++; 
	}

	PositionKeyframes::const_iterator pbegin = iac_positionKeys.begin();
	PositionKeyframes::const_iterator pit = pbegin;
	PositionKeyframes::const_iterator pend = iac_positionKeys.end();
    while (pit != pend && pit->first < 0.f) 
	{
		pit++; 
	}
    
	RotationKeyframes::const_iterator rbegin = iac_rotationKeys.begin();
	RotationKeyframes::const_iterator rit = rbegin;
	RotationKeyframes::const_iterator rend = iac_rotationKeys.end();
    while (rit != rend && rit->first < 0.f) 
	{
		rit++; 
	}

	/*
	#define FIND_KEY( itvar, endvar, flagvar, containertype, container )\
		bool flagvar = true;                                            \
		containertype::const_iterator itvar = container.begin();        \
		containertype::const_iterator endvar = container.end();         \
		{                                                               \
			while ((itvar) != endvar && (itvar)->first < 0.f)           \
			{                                                           \
				itvar++;                                                \
			}                                                           \
			if (itvar != container.begin())                             \
			{                                                           \
				flagvar = false;                                        \
			}                                                           \
			itvar--;                                                    \
		}                                                               \
		MF_ASSERT( itvar == _##itvar );                                 \
		MF_ASSERT( flagvar == _##flagvar );

	FIND_KEY( sit, snd, sil, ScaleKeyframes, iac_scaleKeys )
	FIND_KEY( pit, pnd, pil, PositionKeyframes, iac_positionKeys )
	FIND_KEY( rit, rnd, ril, RotationKeyframes, iac_rotationKeys )
	*/

	float curTime = 0.f;
	for (uint i = 0; i < numBlocks_; i++)
	{
		std::vector<uint32>			sRoster;
		std::vector<uint32>			pRoster;
		std::vector<uint32>			rRoster;
		std::vector<Vector3>		sBlock;
		std::vector<Vector3>		pBlock;
		std::vector<Quaternion>		rBlock;
		const uint32	rangeBeg = i*STREAM_BLOCK_SIZE;
		const uint32	rangeEnd = (i+1)*STREAM_BLOCK_SIZE;
		const float		rangeBegF = float(rangeBeg);
		const float		rangeEndF = float(rangeEnd);
	
		// make a first pass and figure out what data we want to use
		bool sin = true, pin = true, rin = true;
		uint32 sbg = (sit != sbegin) ? uint32( (sit-1)->first ) : 0;
		uint32 pbg = (pit != pbegin) ? uint32( (pit-1)->first ) : 0;
		uint32 rbg = (rit != rbegin) ? uint32( (rit-1)->first ) : 0;

		for (uint j = 0; j < STREAM_BLOCK_SIZE; j++, curTime += 1.f)
		{
			// move on the iterators to the appropriate keyframe
			while (sit != send && sit->first <= curTime)
			{
				++sit; 
				sin = true; 
			}
			while (pit != pend && pit->first <= curTime)
			{	
				++pit; 
				pin = true; 
			}
			while (rit != rend && rit->first <= curTime)
			{	
				++rit; 
				rin = true; 
			}

			// put these keys in our block if we haven't already
			if (sin)
			{
				float look = std::max( rangeBegF, (sit != sbegin) ? (sit-1)->first : 0.f );
				float next = (sit == send) ? rangeEndF : sit->first;
				sRoster.push_back( uint32(next - look) );
				const Vector3 & val = (sit != sbegin) ? (sit-1)->second : sit->second;
				if (val != scaleFallback_)
					sBlock.push_back( val );
				else
					sRoster.back() |= (1<<31);
				sin = false;
			}
			if (pin)
			{
				float look = std::max( rangeBegF, (pit != pbegin) ? (pit-1)->first : 0.f );
				float next = (pit == pend) ? rangeEndF : pit->first;
				pRoster.push_back( uint32(next - look) );
				const Vector3 & val = (pit != pbegin) ? (pit-1)->second : pit->second;
				if (val != positionFallback_)
					pBlock.push_back( val );
				else
					pRoster.back() |= (1<<31);
				pin = false;
			}
			if (rin)
			{
				float look = std::max( rangeBegF, (rit != rbegin) ? (rit-1)->first : 0.f );
				float next = (rit == rend) ? rangeEndF : rit->first;
				rRoster.push_back( uint32(next - look) );
				const Quaternion & val = (rit != rbegin) ? (rit-1)->second : rit->second;
				if (val != rotationFallback_)
					rBlock.push_back( val );
				else
					rRoster.back() |= (1<<31);
				rin = false;
			}
		}

		// add the end keys if we have them
		if (sit != send && sit->second != scaleFallback_)
			sBlock.push_back( sit->second );
		else	// maybe should re-add last key instead of fallback?
			sbg |= (1<<31);

		if (pit != pend && pit->second != positionFallback_)
			pBlock.push_back( pit->second );
		else
			pbg |= (1<<31);

		if (rit != rend && rit->second != rotationFallback_)
			rBlock.push_back( rit->second );
		else
			rbg |= (1<<31);


		// now do a second pass to fill in the channel block indexes
		ChannelBlock * pcb = (ChannelBlock*)malloc( sizeof(ChannelBlock) );

		// This is the blendtransform at the start of this channel block
		BlendTransform btBefore;
		iac.result( rangeBegF, btBefore );

		// say how long the first key extends before the block starts
		// (if the first key (in the whole anim) is before this block starts,
		// then move it back to the front of the anim. slightly dogdy yes.)
		if (rangeBeg >= (sbg & ~(1<<31)))
		{
			uint32 val = rangeBeg - (sbg & ~(1<<31));
			if (val != uint32(uint8(val)))
			{
				pcb->sHeader_.durBefore_ = 0;
				if (sRoster.front() & (1<<31))
				{
					if (btBefore.scale != scaleFallback_)
					{
						sRoster.front() &= ~(1<<31);
						sBlock.insert( sBlock.begin(), btBefore.scale );
					}
				}
				else
				{
					if (btBefore.scale != scaleFallback_)
					{
						sBlock.front() =  btBefore.scale;
					}
					else
					{
						sRoster.front() |= (1<<31);
						sBlock.erase( sBlock.begin() );
					}
				}
			}
			else
			{
				pcb->sHeader_.durBefore_ = uint8( val );
			}
		}
		else
		{
			sRoster.front() += (sbg & ~(1<<31)) - rangeBeg;
			pcb->sHeader_.durBefore_ = 0;
		}

		if (rangeBeg >= (pbg & ~(1<<31)))
		{
			uint32 val = rangeBeg - (pbg & ~(1<<31));
			if (val != uint32(uint8(val)))
			{
				pcb->pHeader_.durBefore_ = 0;
				if (pRoster.front() & (1<<31))
				{
					if (btBefore.translate != positionFallback_)
					{
						pRoster.front() &= ~(1<<31);
						pBlock.insert( pBlock.begin(), btBefore.translate );
					}
				}
				else
				{
					if (btBefore.translate != positionFallback_)
					{
						pBlock.front() =  btBefore.translate;
					}
					else
					{
						pRoster.front() |= (1<<31);
						pBlock.erase( pBlock.begin() );
					}
				}
			}
			else
			{
				pcb->pHeader_.durBefore_ = uint8( val );
			}
		}
		else
		{
			pRoster.front() += (pbg & ~(1<<31)) - rangeBeg;
			pcb->pHeader_.durBefore_ = 0;
		}

		if (rangeBeg >= (rbg & ~(1<<31)))
		{
			uint32 val = rangeBeg - (rbg & ~(1<<31));
			if (val != uint32(uint8(val)))
			{
				pcb->rHeader_.durBefore_ = 0;
				if (rRoster.front() & (1<<31))
				{
					if (btBefore.rotate != rotationFallback_)
					{
						rRoster.front() &= ~(1<<31);
						rBlock.insert( rBlock.begin(), btBefore.rotate );
					}
				}
				else
				{
					if (btBefore.rotate != rotationFallback_)
					{
						rBlock.front() =  btBefore.rotate;
					}
					else
					{
						rRoster.front() |= (1<<31);
						rBlock.erase( rBlock.begin() );
					}
				}
			}
			else
			{
				pcb->rHeader_.durBefore_ = uint8( val );
			}
		}
		else
		{
			rRoster.front() += (rbg & ~(1<<31)) - rangeBeg;
			pcb->rHeader_.durBefore_ = 0;
		}


		// This is the blend transform after the current channel block
		BlendTransform btAfter;
		iac.result( rangeEndF, btAfter );

		// say how long the last key extends after the block finishes
		// (assumes there are not two keys at the same integer time)
		uint32 sumTime;

		// the scale part of the animation
		sumTime = -STREAM_BLOCK_SIZE;
		for (uint j = 0; j < sRoster.size(); j++)
			sumTime += sRoster[j] & ~(1<<31);

		if (sumTime == uint32(uint8(sumTime)))
		{
			pcb->sHeader_.durAfter_ = uint8(sumTime);
		}
		else
		{
			if (sbg & (1<<31))
			{
				if (btAfter.scale != this->scaleFallback_)
				{
					sbg &= ~(1<<31);
					sBlock.push_back( btAfter.scale );
				}
			}
			else
			{
				if (btAfter.scale != this->scaleFallback_)
				{
					sBlock.back() = btAfter.scale;
				}
				else
				{
					sBlock.pop_back();
					sbg |= (1<<31);
				}
			}
			pcb->sHeader_.durAfter_ = 0;
		}

		sRoster.back() -= sumTime;

		// the position part of the animation
		sumTime = -STREAM_BLOCK_SIZE;
		for (uint j = 0; j < pRoster.size(); j++)
			sumTime += pRoster[j] & ~(1<<31);

		if (sumTime == uint32(uint8(sumTime)))
		{
			pcb->pHeader_.durAfter_ = uint8(sumTime);
		}
		else
		{
			if (pbg & (1<<31))
			{
				if (btAfter.translate != positionFallback_)
				{
					pbg &= ~(1<<31);
					pBlock.push_back( btAfter.translate );
				}
			}
			else
			{
				if (btAfter.translate != this->positionFallback_)
				{
					pBlock.back() = btAfter.translate;
				}
				else
				{
					pBlock.pop_back();
					pbg |= (1<<31);
				}
			}
			pcb->pHeader_.durAfter_ = 0;
		}

		pRoster.back() -= sumTime;

		// the rotation part of the animation
		sumTime = -STREAM_BLOCK_SIZE;
		for (uint j = 0; j < rRoster.size(); j++)
			sumTime += rRoster[j] & ~(1<<31);

		if (sumTime == uint32(uint8(sumTime)))
		{
			pcb->rHeader_.durAfter_ = uint8(sumTime);
		}
		else
		{
			if (rbg & (1<<31))
			{
				if (btAfter.rotate != rotationFallback_)
				{
					rbg &= ~(1<<31);
					rBlock.push_back( btAfter.rotate );
				}
			}
			else
			{
				if (btAfter.rotate != rotationFallback_)
				{
					rBlock.back() = btAfter.rotate;
				}
				else
				{
					rBlock.pop_back();
					rbg |= (1<<31);
				}
			}
			pcb->rHeader_.durAfter_ = 0;
		}

		rRoster.back() -= sumTime;


		// Create the actual data
		uint srs = sRoster.size();
		uint prs = pRoster.size();
		uint rrs = rRoster.size();
		uint rostersSize = (srs+prs+rrs+3)&~3L;
		uint swd = sBlock.size() * sizeof(Vector3) / sizeof(uint32);
		uint pwd = pBlock.size() * sizeof(Vector3) / sizeof(uint32);
		uint rwd = rBlock.size() * sizeof(Quaternion) / sizeof(uint32);
		pcb = (ChannelBlock*)realloc( pcb, sizeof(ChannelBlock) +
			rostersSize + (swd+pwd+rwd) * sizeof(uint32) );

		// write in the rosters and their offsets
		uint8 * roster = (uint8*)(pcb+1);
		pcb->sHeader_.rosterOffset_ = 0 | ((sbg & (1<<31)) ? 0x80 : 0);
		pcb->pHeader_.rosterOffset_ = srs | ((pbg & (1<<31)) ? 0x80 : 0);
		pcb->rHeader_.rosterOffset_ = srs + prs | ((rbg & (1<<31)) ? 0x80 : 0);
		for (uint j = 0; j < srs; j++)
			*roster++ = uint8(sRoster[j]) | ((sRoster[j] & (1<<31)) ? 0x80 : 0);
		for (uint j = 0; j < prs; j++)
			*roster++ = uint8(pRoster[j]) | ((pRoster[j] & (1<<31)) ? 0x80 : 0);
		for (uint j = 0; j < rrs; j++)
			*roster++ = uint8(rRoster[j]) | ((rRoster[j] & (1<<31)) ? 0x80 : 0);
		for (uint j = srs+prs+rrs; j < rostersSize; j++)
			*roster++ = 0;	// pad to a word

		// and finally copy in the actual key data
		uint rostersWords = rostersSize / sizeof(uint32);
		uint32 * keyData = ((uint32*)(pcb+1)) + rostersWords;
		MF_ASSERT( ((uint8*)keyData) == roster )

		pcb->sHeader_.keyDataOffset_ = rostersWords;
		pcb->pHeader_.keyDataOffset_ = rostersWords + swd;
		pcb->rHeader_.keyDataOffset_ = rostersWords + swd + pwd;
		for (uint j = 0; j < sBlock.size(); j++)
			*((Vector3*&)keyData)++ = sBlock[j];
		for (uint j = 0; j < pBlock.size(); j++)
			*((Vector3*&)keyData)++ = pBlock[j];
		for (uint j = 0; j < rBlock.size(); j++)
			*((Quaternion*&)keyData)++ = rBlock[j];

		// some checking
		if (pcb->sHeader_.keyDataOffset_ == pcb->pHeader_.keyDataOffset_)
		{
			MF_ASSERT( (pcb->sHeader_.rosterOffset_ & 0x80) &&
				(pcb->scaleRoster()[0] & 0x80) );
		}

		// make this our block pointer
		pBlocks_[i] = pcb;
	}
}

StreamedAnimationChannel::TypeRegisterer
	StreamedAnimationChannel::s_rego_( 5, New );



}	// namespace Moo

// streamed_animation_channel.cpp
