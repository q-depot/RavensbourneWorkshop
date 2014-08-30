#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/params/Params.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Texture.h"
#include "cinder/gl/Vbo.h"
#include "cinder/ObjLoader.h"
#include "cinder/MayaCamUI.h"

#include "cinder/audio/Context.h"
#include "cinder/audio/MonitorNode.h"

#include "ciXtract.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class SoundShaderObjectApp : public AppNative {

public:
    
    void prepareSettings( Settings *settings );
	void setup();
	void update();
	void draw();
    void initAudio();
    
    void keyDown( KeyEvent event );
    void mouseDown( MouseEvent event );
    void mouseDrag( MouseEvent event );
    void resize();
    void fileDrop( FileDropEvent event );
    
    void loadShader();
    void loadObject( fs::path filepath );
    
    // Xtract
    ciXtractRef                 mXtract;
    ciXtractFeatureRef          mBark;
    float                       mBarkGain;
    float                       mBarkOffset;
    float                       mBarkDamping;
    
    // Audio
	audio::InputDeviceNodeRef	mInputDeviceNode;
	audio::MonitorNodeRef       mMonitorNode;
    audio::Buffer               mPcmBuffer;
    
    
    ColorA                      mObjColor;
    gl::GlslProgRef             mShader;
    MayaCamUI                   mMayaCam;
    bool                        mRenderWireframe;
    
    gl::VboMesh                 mVbo;
    
	Surface32f                  mFeatureSurf;			// data is stored in Surface
	gl::Texture                 mFeatureTex;			// use a Texture to pass the data to the shader
    
    params::InterfaceGlRef      mParams;
    float                       mFps;
};


void SoundShaderObjectApp::prepareSettings( Settings *settings )
{
    settings->setWindowSize( 1200, 900 );
}


void SoundShaderObjectApp::setup()
{
    mBarkGain           = 1.0f;
    mBarkOffset         = 0.0f;
    mBarkDamping        = 0.95f;
    mObjColor           = ColorA( 0.0f, 1.0f, 1.0f, 1.0f );
    mRenderWireframe    = true;
    
    mParams = params::InterfaceGl::create( "params", Vec2i( 200, 250 ) );
    mParams->addParam( "FPS",  &mFps );
    mParams->addSeparator();
    mParams->addParam( "Bark gain",     &mBarkGain,     "min=0.1 max=500.0 step=0.01" );
    mParams->addParam( "Bark offset",   &mBarkOffset,   "min=-1.0 max=1.0 step=0.01" );
    mParams->addParam( "Bark damping",  &mBarkDamping,  "min=0.7 max=0.99 step=0.01" );
    mParams->addSeparator();
    mParams->addParam( "Obj color",     &mObjColor );
    mParams->addParam( "Wireframe",     &mRenderWireframe );
    
    // initialise Xtract
    mXtract = ciXtract::create();
    
    // enable features
    mXtract->enableFeature( XTRACT_BARK_COEFFICIENTS );
    
    // get feature references
    mBark               = mXtract->getFeature( XTRACT_BARK_COEFFICIENTS );
    
    loadObject( getAssetPath( "cube.obj" ) );
    
    loadShader();
    
	CameraPersp initialCam;
	initialCam.setPerspective( 45.0f, getWindowAspectRatio(), 0.1, 1000 );
	mMayaCam.setCurrentCam( initialCam );
    
	mFeatureSurf    = Surface32f( 32, 32, false );	// we can store up to 1024 values(32x32)
	mFeatureTex     = gl::Texture( 32, 32 );
    
    initAudio();
}


void SoundShaderObjectApp::update()
{
    // update params
    mBark->setGain( mBarkGain );
    mBark->setOffset( mBarkOffset );
    mBark->setDamping( mBarkDamping );
    
    if ( !mMonitorNode )
        return;
    
    mPcmBuffer = mMonitorNode->getBuffer();                     // get PCM buffer
    
    if ( !mPcmBuffer.isEmpty() )
        mXtract->update( mPcmBuffer.getData() );                // update Xtract
    
    // update Surface
    int x, y;
    for( int k=0; k < mBark->getResultsN(); k++ )
    {
        x = k % mFeatureSurf.getWidth();
        y = k / mFeatureSurf.getWidth();
        mFeatureSurf.setPixel( Vec2i(x, y), Color::gray( mBark->getDataValue(k) ) );
    }
    mFeatureTex = gl::Texture( mFeatureSurf );

    mFps = getAverageFps();
}


void SoundShaderObjectApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) );
    gl::enableAlphaBlending();
    gl::enableDepthRead();
    gl::enableDepthWrite();
    
    gl::pushMatrices();
    
	gl::setMatrices( mMayaCam.getCamera() );
	
	if ( mBark && mFeatureTex )
	{
		mShader->bind();
		mFeatureTex.enableAndBind();
		mShader->uniform( "dataTex",		0 );
		mShader->uniform( "texWidth",		(float)mFeatureTex.getWidth() );
		mShader->uniform( "texHeight",		(float)mFeatureTex.getHeight() );
		mShader->uniform( "soundDataSize",  (float)mBark->getResultsN() );
		mShader->uniform( "spread",         1.0f );
		mShader->uniform( "spreadOffset",   0.0f );
        mShader->uniform( "time",           (float)getElapsedSeconds() );
		mShader->uniform( "tintColor",      mObjColor );
	}
    
    if ( mRenderWireframe )
        gl::enableWireframe();
    
	gl::color( Color(1.0f, 0.0f, 0.0f ) );
    
	if ( mVbo )
	    gl::draw( mVbo );
    
    if ( mRenderWireframe )
        gl::disableWireframe();
    
	mShader->unbind();
	mFeatureTex.unbind();
    
	gl::color( Color::white() );
    //	gl::drawCoordinateFrame();
    
	gl::popMatrices();
    
    gl::disableDepthRead();
    gl::disableDepthWrite();
    
	gl::setMatricesWindow( getWindowSize() );
	
	gl::draw( mFeatureSurf );
    
    ciXtract::drawPcm( Rectf( 0, 0, getWindowWidth(), 60 ), mPcmBuffer.getData(), mPcmBuffer.getSize() / mPcmBuffer.getNumChannels() );
    
    ciXtract::drawData( mBark, Rectf( 15, 60, 140, 100 ) );
    
    mParams->draw();
}


void SoundShaderObjectApp::initAudio()
{
    auto ctx = audio::Context::master();
    
    vector<audio::DeviceRef> devices = audio::Device::getInputDevices();
    console() << "List audio devices:" << endl;
    for( auto k=0; k < devices.size(); k++ )
        console() << devices[k]->getName() << endl;
    
    // find and initialise a device by name
    audio::DeviceRef dev    = audio::Device::findDeviceByName( "Soundflower (2ch)" );
    mInputDeviceNode        = ctx->createInputDeviceNode( dev );
    
    // initialise default input device
    //    mInputDeviceNode = ctx->createInputDeviceNode();
    
    // initialise MonitorNode to get the PCM data
	auto monitorFormat = audio::MonitorNode::Format().windowSize( CIXTRACT_PCM_SIZE );
	mMonitorNode = ctx->makeNode( new audio::MonitorNode( monitorFormat ) );
    
    // pipe the input device into the MonitorNode
	mInputDeviceNode >> mMonitorNode;
    
	// InputDeviceNode (and all InputNode subclasses) need to be enabled()'s to process audio. So does the Context:
	mInputDeviceNode->enable();
	ctx->enable();
}


void SoundShaderObjectApp::keyDown( KeyEvent event )
{
    char c = event.getChar();
    
    if ( c == 'f' )
        setFullScreen( !isFullScreen() );
    
    else if ( c == 'r' )
        loadShader();
}


void SoundShaderObjectApp::mouseDown( MouseEvent event )
{
    if( event.isAltDown() )
		mMayaCam.mouseDown( event.getPos() );
}


void SoundShaderObjectApp::mouseDrag( MouseEvent event )
{
    if( event.isAltDown() )
		mMayaCam.mouseDrag( event.getPos(), event.isLeftDown(), event.isMiddleDown(), event.isRightDown() );
}


void SoundShaderObjectApp::resize()
{
	CameraPersp cam;
    cam.setPerspective( 45.0f, getWindowAspectRatio(), 0.1, 10000 );
	mMayaCam.setCurrentCam( cam );
    
}


void SoundShaderObjectApp::fileDrop( FileDropEvent event )
{
    fs::path filepath = event.getFile(0);
    
    if ( filepath.extension() == ".obj" )
    {
        loadObject( filepath );
    }
}


void SoundShaderObjectApp::loadShader()
{
    console() << getAssetPath( "shaders/passThru.vert" ) << endl;
    
    try {
		mShader = gl::GlslProg::create( loadAsset( "shaders/passThru.vert" ), loadAsset( "shaders/sound.frag" ) );
        console() << "Shader loaded " << getElapsedSeconds() << endl;
	}
	catch( gl::GlslProgCompileExc &exc ) {
		console() << "Shader compile error: " << endl;
		console() << exc.what();
	}
	catch( ... ) {
		console() << "Unable to load shader" << endl;
	}
}


void SoundShaderObjectApp::loadObject( fs::path filepath )
{
    try {
		ObjLoader loader( DataSourcePath::create( filepath.string() ) );
		TriMesh mesh;
		loader.load( &mesh );
		mVbo = gl::VboMesh( mesh );
	}
	catch( ... ) {
		console() << "cannot load file: " << filepath.generic_string() << endl;
		return;
	}
}


CINDER_APP_NATIVE( SoundShaderObjectApp, RendererGl )





















































