#include "ReymentaServerApp.h"

void ReymentaServerApp::prepare( Settings *settings )
{
	settings->setWindowSize( 1440, 900 );
}

void ReymentaServerApp::setup()
{
	int wr;
	// parameters
	mParameterBag = ParameterBag::create();
	// utils
	mBatchass = Batchass::create(mParameterBag);
	mBatchass->log("setup");

	wr = mBatchass->getWindowsResolution();

	setWindowSize(mParameterBag->mMainWindowWidth, mParameterBag->mMainWindowHeight);
	// 12 is enough for a router
	setFrameRate(12.0f);
	setWindowPos(ivec2(0, 0));

	// if mStandalone, put on the 2nd screen
	if (mParameterBag->mStandalone)
	{
		setWindowSize(mParameterBag->mRenderWidth, mParameterBag->mRenderHeight);
		setWindowPos(ivec2(mParameterBag->mRenderX, mParameterBag->mRenderY));
	}

	// setup shaders and textures
	mBatchass->setup();

	mParameterBag->mMode = MODE_WARP;
	mParameterBag->iResolution.x = mParameterBag->mRenderWidth;
	mParameterBag->iResolution.y = mParameterBag->mRenderHeight;
	mParameterBag->mRenderResolution = ivec2(mParameterBag->mRenderWidth, mParameterBag->mRenderHeight);

	mBatchass->log("createRenderWindow, resolution:" + toString(mParameterBag->iResolution.x) + "x" + toString(mParameterBag->iResolution.y));
	mParameterBag->mRenderResoXY = vec2(mParameterBag->mRenderWidth, mParameterBag->mRenderHeight);
	mParameterBag->mRenderPosXY = ivec2(mParameterBag->mRenderX, mParameterBag->mRenderY);

	// instanciate the console class
	mConsole = AppConsole::create(mParameterBag, mBatchass);

	// imgui
	margin = 3;
	inBetween = 3;
	// mPreviewFboWidth 80 mPreviewFboHeight 60 margin 10 inBetween 15 mPreviewWidth = 160;mPreviewHeight = 120;
	w = mParameterBag->mPreviewFboWidth + margin;
	h = mParameterBag->mPreviewFboHeight * 2.3;
	largeW = (mParameterBag->mPreviewFboWidth + margin) * 4;
	largeH = (mParameterBag->mPreviewFboHeight + margin) * 5;
	largePreviewW = mParameterBag->mPreviewWidth + margin;
	largePreviewH = (mParameterBag->mPreviewHeight + margin) * 2.4;
	displayHeight = mParameterBag->mMainDisplayHeight - 50;
	mouseGlobal = false;
	showConsole = showGlobal = showTextures = showAudio = showMidi = showChannels = showShaders = true;
	showTest = showTheme = showOSC = showFbos = false;

	// set ui window and io events callbacks
	ui::initialize();

	// warping
	mUseBeginEnd = false;
	updateWindowTitle();
	disableFrameRate();

	// initialize warps
	mSettings = getAssetPath( "" ) / "warps.xml";
	if( fs::exists( mSettings ) ) {
		// load warp settings from file if one exists
		mWarps = Warp::readSettings( loadFile( mSettings ) );
	}
	else {
		// otherwise create a warp from scratch
		mWarps.push_back( WarpBilinear::create() );
		mWarps.push_back( WarpPerspective::create() );
		mWarps.push_back( WarpPerspectiveBilinear::create() );
	}

	// load test image
	try {
		mImage = gl::Texture::create( loadImage( loadAsset( "help.png" ) ), 
									  gl::Texture2d::Format().loadTopDown().mipmap( true ).minFilter( GL_LINEAR_MIPMAP_LINEAR ) );

		mSrcArea = mImage->getBounds();

		// adjust the content size of the warps
		Warp::setSize( mWarps, mImage->getSize() );
	}
	catch( const std::exception &e ) {
		console() << e.what() << std::endl;
	}
}

void ReymentaServerApp::cleanup()
{
	// save warp settings
	Warp::writeSettings( mWarps, writeFile( mSettings ) );
}

void ReymentaServerApp::update()
{
	// there is nothing to update
}

void ReymentaServerApp::draw()
{
	// clear the window and set the drawing color to white
	gl::clear();
	gl::color( Color::white() );

	if( mImage ) {
		// iterate over the warps and draw their content
		for( auto &warp : mWarps ) {
			// there are two ways you can use the warps:
			if( mUseBeginEnd ) {
				// a) issue your draw commands between begin() and end() statements
				warp->begin();

				// in this demo, we want to draw a specific area of our image,
				// but if you want to draw the whole image, you can simply use: gl::draw( mImage );
				gl::draw( mImage, mSrcArea, warp->getBounds() );

				warp->end();
			}
			else {
				// b) simply draw a texture on them (ideal for video)

				// in this demo, we want to draw a specific area of our image,
				// but if you want to draw the whole image, you can simply use: warp->draw( mImage );
				warp->draw( mImage, mSrcArea );
			}
		}
	}
}

void ReymentaServerApp::resize()
{
	// tell the warps our window has been resized, so they properly scale up or down
	Warp::handleResize( mWarps );
}

void ReymentaServerApp::mouseMove( MouseEvent event )
{
	// pass this mouse event to the warp editor first
	if( !Warp::handleMouseMove( mWarps, event ) ) {
		// let your application perform its mouseMove handling here
	}
}

void ReymentaServerApp::mouseDown( MouseEvent event )
{
	// pass this mouse event to the warp editor first
	if( !Warp::handleMouseDown( mWarps, event ) ) {
		// let your application perform its mouseDown handling here
	}
}

void ReymentaServerApp::mouseDrag( MouseEvent event )
{
	// pass this mouse event to the warp editor first
	if( !Warp::handleMouseDrag( mWarps, event ) ) {
		// let your application perform its mouseDrag handling here
	}
}

void ReymentaServerApp::mouseUp( MouseEvent event )
{
	// pass this mouse event to the warp editor first
	if( !Warp::handleMouseUp( mWarps, event ) ) {
		// let your application perform its mouseUp handling here
	}
}

void ReymentaServerApp::keyDown( KeyEvent event )
{
	// pass this key event to the warp editor first
	if( !Warp::handleKeyDown( mWarps, event ) ) {
		// warp editor did not handle the key, so handle it here
		switch( event.getCode() ) {
			case KeyEvent::KEY_ESCAPE:
				// quit the application
				quit();
				break;
			case KeyEvent::KEY_f:
				// toggle full screen
				setFullScreen( !isFullScreen() );
				break;
			case KeyEvent::KEY_v:
				// toggle vertical sync
				gl::enableVerticalSync( !gl::isVerticalSyncEnabled() );
				break;
			case KeyEvent::KEY_w:
				// toggle warp edit mode
				Warp::enableEditMode( !Warp::isEditModeEnabled() );
				break;
			case KeyEvent::KEY_a:
				// toggle drawing a random region of the image
				if( mSrcArea.getWidth() != mImage->getWidth() || mSrcArea.getHeight() != mImage->getHeight() )
					mSrcArea = mImage->getBounds();
				else {
					int x1 = Rand::randInt( 0, mImage->getWidth() - 150 );
					int y1 = Rand::randInt( 0, mImage->getHeight() - 150 );
					int x2 = Rand::randInt( x1 + 150, mImage->getWidth() );
					int y2 = Rand::randInt( y1 + 150, mImage->getHeight() );
					mSrcArea = Area( x1, y1, x2, y2 );
				}
				break;
			case KeyEvent::KEY_SPACE:
				// toggle drawing mode
				mUseBeginEnd = !mUseBeginEnd;
				updateWindowTitle();
				break;
		}
	}
}

void ReymentaServerApp::keyUp( KeyEvent event )
{
	// pass this key event to the warp editor first
	if( !Warp::handleKeyUp( mWarps, event ) ) {
		// let your application perform its keyUp handling here
	}
}

void ReymentaServerApp::updateWindowTitle()
{
	if( mUseBeginEnd )
		getWindow()->setTitle( "Warping Sample - Using begin() and end()" );
	else
		getWindow()->setTitle( "Warping Sample - Using draw()" );
}

CINDER_APP( ReymentaServerApp, RendererGl( RendererGl::Options().msaa( 8 ) ), &ReymentaServerApp::prepare )
