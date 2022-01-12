/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BIG_BANG_HPP
#define BIG_BANG_HPP

#include <string>
#include <set>
#include <stack>
#include "math/vector3.hpp"
#include "input/input.hpp"
#include "common/romp_harness.hpp"
#include "gizmo/tool.hpp"
#include "gizmo/undoredo.hpp"
#include "terrain/editor_chunk_terrain.hpp"
#include "project/bigbangd_connection.hpp"
#include "gizmo/snap_provider.hpp"
#include "gizmo/coord_mode_provider.hpp"
#include "item_view.hpp"
#include "cstdmf/concurrency.hpp"
#include "big_bang_progress_bar.hpp"
#include "common/space_mgr.hpp"
#include "guimanager/gui_functor_cpp.hpp"
#include "guimanager/gui_functor_option.hpp"


// Ual Forward Declarations
class ClosedCaptions;
class BigBangCamera;
class SuperModelProgressDisplay;
class ChunkSpace;
typedef SmartPointer<ChunkSpace> ChunkSpacePtr;

namespace Moo
{
	class TerrainBlock;
	typedef SmartPointer<TerrainBlock> TerrainBlockPtr;
}

struct BigBangDebugMessageCallback : public DebugMessageCallback
{
	virtual bool handleMessage( int componentPriority,
		int messagePriority, const char * format, va_list argPtr );
};


/**
 *	This class is the highest authority in the BigBang world editor
 */
class BigBang : public SnapProvider, public CoordModeProvider, public ReferenceCount,
	GUI::ActionMaker<BigBang>, GUI::ActionMaker<BigBang, 1>, GUI::ActionMaker<BigBang, 2>,
	GUI::ActionMaker<BigBang, 3>, GUI::ActionMaker<BigBang, 4>, GUI::ActionMaker<BigBang, 5>,
	GUI::ActionMaker<BigBang, 6>, GUI::ActionMaker<BigBang, 7>, GUI::ActionMaker<BigBang, 8>,
	GUI::OptionMap,
	GUI::UpdaterMaker<BigBang>, GUI::UpdaterMaker<BigBang, 1>, GUI::UpdaterMaker<BigBang, 2>
{
public:
	typedef BigBang This;

	static BigBang& instance();
    ~BigBang();

	bool	init( HINSTANCE hInst, HWND hwndInput, HWND hwndGraphics );
	bool	postLoadThreadInit();
	void	fini();
	bool	canClose( const std::string& action );

	// tool mode updater called by the tool pages
	void updateUIToolMode( const std::string& pyID );

    //commands and properties.  In fini, all of these will
	//have a gui item associated with them.  All gui items
	//will call through to these properties and functions.
    void	timeOfDay( float t );
    void	rainAmount( float a );
	void	farPlane( float f );
	float	farPlane() const;
	float	dTime() const	{ return dTime_; }
    void	focus( bool state );
    void	globalWeather( bool state )	{ globalWeather_ = state; }
    void	propensity( const std::string& weatherSystemName, float amount );

	void	addCommentaryMsg( const std::string& msg, int id = 0 );
	// notify user of an error (gui errors pane & commentary)
	void	addError(Chunk* chunk, ChunkItem* item, const char * format, ...);

	void	reloadAllChunks( bool askBeforeProceed );
	void	changedChunk( Chunk * pChunk, bool rebuildNavmesh = true );
	void	dirtyThumbnail( Chunk * pChunk );
	void	changedTerrainBlock( Chunk * pChunk, bool rebuildNavmesh = true );
	void	dirtyVLO( const std::string& type, const std::string& id, const BoundingBox& bb );
	bool	isDirtyVlo( Chunk* chunk, const std::string& id );
	void	chunkShadowUpdated( Chunk * pChunk );
	void	changedThumbnail( Chunk * pChunk );
	void	changedTerrainBlockOffline( const std::string& chunkName );
	void	resetChangedLists();
	bool	checkForReadOnly() const;
	bool	EnsureNeighbourChunkLoaded( Chunk* chunk, int level );
	bool	EnsureNeighbourChunkLoadedForShadow( Chunk* chunk );

	void	loadNeighbourChunk( Chunk* chunk, int level );
	void	loadChunkForShadow( Chunk* chunk );
	void	saveChunk( const std::string& chunkName, ProgressTask& task );
	void	saveChunk( Chunk* chunk, ProgressTask& task );
	template<typename T>
		bool save( T t, ProgressTask& task, BigBangProgressBar& progress );
	void	save();
	void	quickSave();

    void    environmentChanged() { changedEnvironment_ = true; }

	bool	snapsEnabled() const;
	bool	terrainSnapsEnabled() const;
	bool	obstacleSnapsEnabled() const;
	Vector3	movementSnaps() const;
	float	angleSnaps() const;

	/** 0 = no, 1 = some, 2 = all */
	int	drawBSP() const;

	virtual SnapMode snapMode( ) const;
	virtual bool snapPosition( Vector3& v );
	virtual Vector3 snapNormal( const Vector3& v );
	virtual void snapPositionDelta( Vector3& v );
	virtual void snapAngles( Matrix& v );
	virtual float angleSnapAmount();

	virtual CoordMode getCoordMode() const;

	//utility methods
	void	update( float dTime );
	void	writeStatus();
	
	//app modules can call any of these methods in whatever order they desire.
	//must call beginRender() and endRender() though ( for timing info )
	void	beginRender();
    void	renderRompPreScene();
	void	renderChunks();
	void	renderTerrain( float dTime );
	void	renderEditorGizmos();
	void	renderDebugGizmos();	
	void	renderRompDelayedScene();
	void	renderRompPostScene();
	void	endRender();
	//or app modules can call this on its own
	void	render( float dTime );


	/** The lights influencing the chunk have changed, flag it for recalculation */
	void	changedChunkLighting( Chunk * pChunk );

	/**
	 * If the given chunk is scheduled to have it's lighting recalculated
	 */
	bool	isDirtyLightChunk( Chunk * pChunk );

	/** Mark the given chunk, and everything 500m either side along the x axis dirty */
	void	markTerrainShadowsDirty( Chunk * pChunk );
	/** Mark the chunks in the given area, and everything 500m either side along the x axis dirty */
	void	markTerrainShadowsDirty( const BoundingBox& bb );

	/**
	 * Mark the chunk as dirty for terrain and lighting if it wasn't fully
	 * saved last session
	 */
	void	checkUpToDate( Chunk * pChunk );

	/**
	 * Called by sub fibers to indicate that they'll give up their time slot
	 * now if they're out of time
	 *
	 * Returns if we actually paused or not
	 */
	bool	fiberPause();

    /**
     * Temporarily stop background processing.
     */
    void stopBackgroundCalculation();

	//saving, adding, writing, erasing, removing, deleting low level fns
	struct SaveableObjectBase { virtual bool save( const std::string & ) const = 0; };

	bool	saveAndAddChunkBase( const std::string & resourceID,
		const SaveableObjectBase & saver, bool add, bool addAsBinary );

	void	eraseAndRemoveFile( const std::string & resourceID );

	//2D - 3D mouse mapping
	const Vector3& worldRay() const		{ return worldRay_; }

	HWND hwndGraphics() const			{ return hwndGraphics_; }

	// Test for valid mouse position
	bool cursorOverGraphicsWnd() const;
	// Current mouse position in the window
	POINT currentCursorPosition() const;
	// Calculate a world ray from the given position
	Vector3 getWorldRay(POINT& pt) const;
	Vector3 getWorldRay(int x, int y) const;

	// The connection to bigbangd, used for locking etc
	BWLock::BigBangdConnection*& connection();

	// Add the block to the list that will be rendered read only
	void addReadOnlyBlock( const Matrix& transform, Moo::TerrainBlockPtr pBlock );
	// Setup the fog to draw stuff readonly, ie, 50% red
	void setReadOnlyFog();

	bool isPointInWriteableChunk( const Vector3& pt );

	// Mark the chunk as having dirty shadows, don't call directly, use
	// markTerrainShadowsDirty instead
	void changedTerrainShadows( Chunk * pChunk );

	DebugMessageCallback * getDebugMessageCallback() { return &debugMessageCallback_; }

	static bool messageHandler( int componentPriority, int messagePriority,
		const char * format, va_list argPtr );

	// check if a particular item is selected
	bool isItemSelected( ChunkItemPtr item ) const;
	// if the shell model for the given chunk is in the selection
	bool isChunkSelected( Chunk * pChunk ) const;
	// if there is a chunk in the selection
	bool isChunkSelected() const;
	// if any items in the given chunk are selected
	bool isItemInChunkSelected( Chunk * pChunk ) const;

	bool isInPlayerPreviewMode() const;
	void setPlayerPreviewMode( bool enable );

	std::string getCurrentSpace() const;

	void markChunks();
	void unloadChunks();

	void setSelection( const std::vector<ChunkItemPtr>& items, bool updateSelection = true );
	void getSelection();

	bool drawSelection() const;
	void drawSelection( bool drawingSelection );

	const std::vector<ChunkItemPtr>& selectedItems() const { return selectedItems_; }

	ChunkDirMapping* chunkDirMapping();

	// notify BB that n prim groups were just drawn in the given chunk
	// used to calculate data for the status bar
	void addPrimGroupCount( Chunk* chunk, uint n );

	// get time of day and environment minder and original seconds per hour
	TimeOfDay* timeOfDay() { return romp_->timeOfDay(); }
    EnviroMinder &enviroMinder() { return romp_->enviroMinder(); }
    float secondsPerHour() const { return secsPerHour_; }
    void secondsPerHour( float value ) { secsPerHour_ = value; }
	void refreshWeather();

	void setStatusMessage( unsigned int index, const std::string& value );
	const std::string& getStatusMessage( unsigned int index ) const;

	void loadChunkForThumbnail( const std::string& chunkName );

	// you can save anything!
	template <class C> struct SaveableObjectPtr : public SaveableObjectBase
	{
		SaveableObjectPtr( C & ob ) : ob_( ob ) { }
		bool save( const std::string & chunkID ) const { return ob_->save( chunkID ); }
		C & ob_;
	};
	template <class C> bool saveAndAddChunk( const std::string & resourceID,
		C saver, bool add, bool addAsBinary )
	{
		return this->saveAndAddChunkBase( resourceID, SaveableObjectPtr<C>(saver),
			add, addAsBinary );
	}

	Chunk* workingChunk() const
	{
		return workingChunk_;
	}
	void workingChunk( Chunk* chunk )
	{
		workingChunk_ = chunk;
	}
	bool isWorkingChunk( Chunk* chunk )
	{
		return workingChunk_ == chunk;
	}

	void setCursor( HCURSOR cursor );
	void resetCursor();
	HCURSOR cursor() const
	{
		return cursor_;
	}

	unsigned int dirtyChunks() const;
	static unsigned int getMemoryLoad();
	//-------------------------------------------------
	//Python Interface
	//-------------------------------------------------

	PY_MODULE_STATIC_METHOD_DECLARE( py_worldRay )
	PY_MODULE_STATIC_METHOD_DECLARE( py_farPlane )

	PY_MODULE_STATIC_METHOD_DECLARE( py_save )
	PY_MODULE_STATIC_METHOD_DECLARE( py_quickSave )
	PY_MODULE_STATIC_METHOD_DECLARE( py_update )
	PY_MODULE_STATIC_METHOD_DECLARE( py_render )

	PY_MODULE_STATIC_METHOD_DECLARE( py_revealSelection )
	PY_MODULE_STATIC_METHOD_DECLARE( py_isChunkSelected )
	PY_MODULE_STATIC_METHOD_DECLARE( py_selectAll )

	PY_MODULE_STATIC_METHOD_DECLARE( py_cursorOverGraphicsWnd )
    PY_MODULE_STATIC_METHOD_DECLARE( py_importDataGUI )
    PY_MODULE_STATIC_METHOD_DECLARE( py_exportDataGUI )

	PY_MODULE_STATIC_METHOD_DECLARE( py_rightClick )

	// Constants
	static const int TIME_OF_DAY_MULTIPLIER = 10;

private:
	BigBang();

	bool	inited_;
	bool	updating_;
    bool	chunkManagerInited_;

	Chunk*	workingChunk_;

    bool	initRomp();

    RompHarness * romp_;
    float	dTime_;
	bool	canSeeTerrain_;

	std::vector<class PhotonOccluder*> occluders_;
    
    std::string	projectPath_;
	bool	isInPlayerPreviewMode_;
    bool	globalWeather_;
    double	totalTime_;
    HWND	hwndInput_;
    HWND	hwndGraphics_;

	std::set<Chunk *> changedChunks_;
	std::set<EditorTerrainBlockPtr> changedTerrainBlocks_;
	std::set<Chunk *> changedThumbnailChunks_;
    bool changedEnvironment_;
    float secsPerHour_;

	bool saveChangedFiles( SuperModelProgressDisplay& progress );

	/**
	 * Called once per frame to recalc static lighting, terrain shadows, etc
	 */
	void	doBackgroundUpdating();

	/** Which chunks need their lighting recalculated */
	std::vector<Chunk*> dirtyLightingChunks_;

	/**
	 * As above, but contains names of chunks that haven't yet been loaded,
	 * when they do get loaded, they'll be removed from here and added to the
	 * above list
	 */
	std::set<std::string> nonloadedDirtyLightingChunks_;

	/** Which outside chunks need to have their terrain shadows recalculated */
	std::vector<Chunk*> dirtyTerrainShadowChunks_;

	/**
	 * As above, but contains names of chunks that haven't yet been loaded,
	 * when they do get loaded, they'll be removed from here and added to the
	 * above list
	 */
	std::set<std::string> nonloadedDirtyTerrainShadowChunks_;

	/** Which chunks need their thumbnail recalculated */
	std::vector<Chunk*> dirtyThumbnailChunks_;

	/**
	 * As above, but contains names of chunks that haven't yet been loaded,
	 * when they do get loaded, they'll be removed from here and added to the
	 * above list
	 */
	std::set<std::string> nonloadedDirtyThumbnailChunks_;

	/**
	 * contains all the vlo information about non-loaded chunks
	 */
	typedef std::map<std::string, std::set< std::pair<std::string, std::string> > > DirtyVLOReferences;
	DirtyVLOReferences dirtyVloReferences_;

	bool writeDirtyList();

	Vector3	worldRay_;

	float angleSnaps_;
	Vector3 movementSnaps_;
	Vector3 movementDeltaSnaps_;
	void calculateSnaps();


	volatile bool killingUpdatingFiber_;
	bool settingSelection_;
	LPVOID mainFiber_;
	LPVOID updatingFiber_;
	uint64 switchTime_;

	static void WINAPI backgroundUpdateLoop( PVOID param );
	BWLock::BigBangdConnection* conn_;

	typedef std::pair<Matrix, Moo::TerrainBlockPtr >	BlockInPlace;
	AVectorNoDestructor< BlockInPlace >	readOnlyTerrainBlocks_;

	typedef std::vector< std::string > StringVector;

	static SimpleMutex pendingMessagesMutex_;
	static StringVector pendingMessages_;
	static void postPendingErrorMessages();

	static std::ostream* logFile_;

	static SmartPointer<BigBang> s_instance;

	std::vector<ChunkItemPtr> selectedItems_;

	SmartPointer<BigBangCamera> bigBangCamera_;

	ChunkDirMapping* mapping_;

	// Current chunk that we're counting prim groups for, used for status bar display
	Chunk* currentMonitoredChunk_;
	// Chunk amount of primitive groups in the locator's chunk, used for status bar display
	uint currentPrimGroupCount_;

	// handle the debug messages
	static BigBangDebugMessageCallback debugMessageCallback_;

	SpaceManager* spaceManager_;
	std::string currentSpace_;

	bool changeSpace( const std::string& space, bool reload );
	bool changeSpace( GUI::ItemPtr item );
	bool newSpace( GUI::ItemPtr item );
	bool recentSpace( GUI::ItemPtr item );
	bool clearUndoRedoHistory( GUI::ItemPtr item );
	bool doReloadAllChunks( GUI::ItemPtr item );
	bool doExit( GUI::ItemPtr item );
	bool doReloadAllTextures( GUI::ItemPtr item );
	unsigned int updateUndo( GUI::ItemPtr item );
	unsigned int updateRedo( GUI::ItemPtr item );
	void updateRecentFile();

	bool doExternalEditor( GUI::ItemPtr item );
	unsigned int updateExternalEditor( GUI::ItemPtr item );

	bool initPanels();
	bool loadDefaultPanels( GUI::ItemPtr item = 0 );

	bool touchAllChunk( GUI::ItemPtr item );

	virtual std::string get( const std::string& key ) const;
	virtual bool exist( const std::string& key ) const;
	virtual void set( const std::string& key, const std::string& value );

	std::vector<std::string> statusMessages_;
	DWORD lastModifyTime_;
	bool drawSelection_;

	HCURSOR cursor_;

    bool waitCursor_;
    void showBusyCursor();

	// used to store the state of the last save/quickSave
	bool saveFailed_;
	bool warningOnLowMemory_;
	bool renderDisabled_;
};

class SelectionOperation : public UndoRedo::Operation
{
public:
	SelectionOperation( const std::vector<ChunkItemPtr>& before, const std::vector<ChunkItemPtr>& after );

private:
	virtual void undo();
	virtual bool iseq( const UndoRedo::Operation & oth ) const;

	std::vector<ChunkItemPtr> before_;
	std::vector<ChunkItemPtr> after_;
};

#endif // BIG_BANG_HPP
