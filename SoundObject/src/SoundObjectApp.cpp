#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/params/Params.h"

#include "cinder/gl/Vbo.h"
#include "cinder/ObjLoader.h"
#include "cinder/MayaCamUI.h"

#include "cinder/audio/Context.h"
#include "cinder/audio/MonitorNode.h"

#include "ciXtract.h"


using namespace ci;
using namespace ci::app;
using namespace std;


class SoundObjectApp : public AppNative {
    
public:
    
    void prepareSettings( Settings *settings );
	void setup();
	void mouseDown( MouseEvent event );
	void mouseDrag( MouseEvent event );
	void update();
	void draw();
    void initAudio();
    void processData();
    
    // Xtract
    ciXtractRef                 mXtract;
    ciXtractFeatureRef          mBark;
    vector<double>              mBarkData;
    float                       mBarkGain;
    float                       mBarkOffset;
    float                       mBarkDamping;
    
    // Audio
	audio::InputDeviceNodeRef	mInputDeviceNode;
	audio::MonitorNodeRef       mMonitorNode;
    audio::Buffer               mPcmBuffer;
    
    // Mesh
    TriMesh                     mTriMesh;
	gl::VboMeshRef              mVbo;
    ColorA                      mMeshCol;
    float                       mDistorsion;
    
    params::InterfaceGlRef      mParams;
	MayaCamUI                   mMayaCam;
    float                       mFps;
};


void SoundObjectApp::prepareSettings( Settings *settings )
{
    settings->setWindowSize( 1200, 900 );
}


void SoundObjectApp::setup()
{
    mBarkGain       = 1.0f;
    mBarkOffset     = 0.0f;
    mBarkDamping    = 0.98f;
    
    mParams = params::InterfaceGl::create( "params", Vec2i( 200, 250 ) );
    mParams->addParam( "FPS",  &mFps );
    mParams->addSeparator();
    mParams->addParam( "Bark gain",     &mBarkGain,     "min=0.1 max=500.0 step=0.01" );
    mParams->addParam( "Bark offset",   &mBarkOffset,   "min=-1.0 max=1.0 step=0.01" );
    mParams->addParam( "Bark damping",  &mBarkDamping,  "min=0.7 max=0.99 step=0.01" );
    mParams->addSeparator();
    mParams->addParam( "Mesh color", &mMeshCol );
    mParams->addParam( "Distortion", &mDistorsion, "min=0.0 max=100.0 step=0.1" );
    
    // load mesh
	ObjLoader loader( loadAsset( "head-low.obj" ) );
	loader.load( &mTriMesh );
	gl::VboMesh::Layout layout;
	layout.setStaticIndices();
	layout.setDynamicPositions();
    mVbo = gl::VboMesh::create( mTriMesh, layout );
    
    mMeshCol    = ColorA::white();
    mDistorsion = 3.0f;
    
    // initialise camera
	CameraPersp initialCam;
	initialCam.setPerspective( 45.0f, getWindowAspectRatio(), 0.1, 10000 );
	mMayaCam.setCurrentCam( initialCam );
    
    // initialise Xtract
    mXtract = ciXtract::create();
    
    // enable features
    mXtract->enableFeature( XTRACT_BARK_COEFFICIENTS );
    
    // get feature references
    mBark       = mXtract->getFeature( XTRACT_BARK_COEFFICIENTS );
    
    initAudio();
}


void SoundObjectApp::mouseDown( MouseEvent event )
{
	if( event.isAltDown() )
		mMayaCam.mouseDown( event.getPos() );
}


void SoundObjectApp::mouseDrag( MouseEvent event )
{
	if( event.isAltDown() )
		mMayaCam.mouseDrag( event.getPos(), event.isLeftDown(), event.isMiddleDown(), event.isRightDown() );
}


void SoundObjectApp::update()
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
    
    // update Vbo
    std::shared_ptr<double> data        = mBark->getResults();
    int                     dataSize    = mBark->getResultsN();
    
    vector<Vec3f> verts = mTriMesh.getVertices();
    gl::VboMesh::VertexIter iter = mVbo->mapVertexBuffer();
    
    for( int k=0; k < mVbo->getNumVertices(); k++ )     // for( int k=0; k < dataSize; k++ )
    {
        // iter.setPosition( verts[k] + mDistorsion * verts[k].normalized() * ( 1.0f + data.get()[k%dataSize] ) );
        iter.setPosition( verts[k] + mDistorsion * verts[k].normalized() * data.get()[k%dataSize] );
        ++iter;
    }
    
    mFps = getAverageFps();
}


void SoundObjectApp::draw()
{
	gl::clear( Color::gray( 0.1f ) );
    gl::enableAlphaBlending();
    
    // render 3D scene
	gl::setMatrices( mMayaCam.getCamera() );
    gl::enableWireframe();
    gl::color( mMeshCol );
    gl::draw( mVbo );
    gl::disableWireframe();
	gl::setMatricesWindow( getWindowSize() );
    
    gl::color( Color::white() );
    
    // render 2D
    ciXtract::drawPcm( Rectf( 0, 0, getWindowWidth(), 60 ), mPcmBuffer.getData(), mPcmBuffer.getSize() / mPcmBuffer.getNumChannels() );
    
    ciXtract::drawData( mBark, Rectf( 15, 60, 140, 100 ) );
    
    mParams->draw();
}


void SoundObjectApp::initAudio()
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


CINDER_APP_NATIVE( SoundObjectApp, RendererGl )

