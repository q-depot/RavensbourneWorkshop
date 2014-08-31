#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/params/Params.h"
#include "cinder/Rand.h"
#include <math.h>

#include "cinder/audio/Context.h"
#include "cinder/audio/MonitorNode.h"

#include "ciXtract.h"


using namespace ci;
using namespace ci::app;
using namespace std;


class SoundCirclesApp : public AppNative {
    
public:
    
    struct Particle
    {
        float   angle;
        float   vel;
        float   size;
        float   radius;
        float   initRadius;
        ColorA  col;
    };
    
    void prepareSettings( Settings *settings );
	void setup();
	void update();
	void draw();
    void initAudio();
    void updateParticles();
    void drawParticles();
    
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
    
    vector<Particle>            mParticles;
    float                       mMinDist;
    float                       mRadius;
    float                       mSpeed;
    int                         mParticlesN;
    
    params::InterfaceGlRef      mParams;
    float                       mFps;
};


void SoundCirclesApp::prepareSettings( Settings *settings )
{
    settings->setWindowSize( 1200, 900 );
}


void SoundCirclesApp::setup()
{
    mBarkGain       = 1.0f;
    mBarkOffset     = 0.0f;
    mBarkDamping    = 0.95f;
    mMinDist        = 100.0f;
    mRadius         = 100.0f;
    mSpeed          = 1.0f;
    mParticlesN     = 50;
    
    mParams = params::InterfaceGl::create( "params", Vec2i( 200, 250 ) );
    mParams->addParam( "FPS",  &mFps );
    mParams->addSeparator();
    mParams->addParam( "Bark gain",     &mBarkGain,     "min=0.1 max=500.0 step=0.01" );
    mParams->addParam( "Bark offset",   &mBarkOffset,   "min=-1.0 max=1.0 step=0.01" );
    mParams->addParam( "Bark damping",  &mBarkDamping,  "min=0.7 max=0.99 step=0.01" );
    mParams->addSeparator();
    mParams->addParam( "Min dist.",     &mMinDist,      "min=0.0 max=1000.0 step=1.0" );
    mParams->addParam( "Radius",        &mRadius,       "min=0.0 max=1000.0 step=1.0" );
    mParams->addParam( "Speed",         &mSpeed,        "min=0.0 max=10.0 step=0.1" );
    mParams->addParam( "Particles N",   &mParticlesN,   "min=0 max=" + to_string(mParticlesN*2) );
    
    // initialise Xtract
    mXtract = ciXtract::create();
    
    // enable features
    mXtract->enableFeature( XTRACT_BARK_COEFFICIENTS );
    mXtract->enableFeature( XTRACT_SPECTRUM );
    
    // get feature references
    mBark       = mXtract->getFeature( XTRACT_BARK_COEFFICIENTS );
    
    initAudio();
    
    for( int k=0; k < mParticlesN*2; k++ )
    {
        Particle p;
        p.angle         = randFloat( toRadians(360.0f) );
        p.vel           = randFloat( -0.01f, 0.01f );
        p.size          = 5.0f;
        p.initRadius    = 100.0f;
        p.radius        = p.initRadius;
        p.col           = Color::white();
        
        mParticles.push_back( p );
    }
}


void SoundCirclesApp::update()
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
    
    updateParticles();
    
    mFps = getAverageFps();
}


void SoundCirclesApp::draw()
{
	gl::clear( Color::gray( 0.1f ) );
    gl::enableAlphaBlending();
    
    ciXtract::drawPcm( Rectf( 0, 0, getWindowWidth(), 60 ), mPcmBuffer.getData(), mPcmBuffer.getSize() / mPcmBuffer.getNumChannels() );
    
    ciXtract::drawData( mBark, Rectf( 15, 60, 140, 100 ) );
    
    drawParticles();
    
    mParams->draw();
}


void SoundCirclesApp::initAudio()
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


void SoundCirclesApp::updateParticles()
{
    shared_ptr<double>  data    = mBark->getResults();
    
    for( size_t k=0; k < mParticlesN; k++ )
    {
        mParticles[k].angle += mParticles[k].vel * mSpeed;
        mParticles[k].radius = mParticles[k].initRadius + ( mRadius * data.get()[ k % mBark->getResultsN() ] );
    }
}


void SoundCirclesApp::drawParticles()
{
    gl::pushMatrices();
    
    gl::translate( getWindowCenter() );
    
    Vec2f   posA, posB;
    float   dist;
    ColorA  col = ColorA::white();
    
    for( size_t k=0; k < mParticlesN; k++ )
    {
        gl::color( mParticles[k].col );
        posA = Vec2f( cos( mParticles[k].angle ), sin( mParticles[k].angle ) ) * mParticles[k].radius;
        gl::drawStrokedCircle( posA, mParticles[k].size );

        for( size_t i=0; i < mParticlesN; i++ )
        {
            if ( k == i )
                continue;
            
            posB = Vec2f( cos( mParticles[i].angle ), sin( mParticles[i].angle ) ) * mParticles[i].radius;
            dist = posA.distance( posB );
            
            if ( dist < mMinDist )
            {
                col.a = 1.0f - dist / mMinDist;
                
                gl::color( col );
                gl::drawLine( posA, posB );
            }
        }
    }
    
    gl::popMatrices();
}


CINDER_APP_NATIVE( SoundCirclesApp, RendererGl )

