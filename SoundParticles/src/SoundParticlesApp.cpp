#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/params/Params.h"
#include "cinder/Rand.h"

#include "cinder/audio/Context.h"
#include "cinder/audio/MonitorNode.h"

#include "ciXtract.h"


using namespace ci;
using namespace ci::app;
using namespace std;


class SoundParticlesApp : public AppNative {
    
public:
    
    struct Particle
    {
        Vec2f   pos;
        Vec2f   vel;
        float   size;
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
    
    params::InterfaceGlRef      mParams;
    float                       mFps;
};


void SoundParticlesApp::prepareSettings( Settings *settings )
{
    settings->setWindowSize( 1200, 900 );
}


void SoundParticlesApp::setup()
{
    mBarkGain       = 1.0f;
    mBarkOffset     = 0.0f;
    mBarkDamping    = 0.95f;
    mMinDist        = 100.0f;
    
    mParams = params::InterfaceGl::create( "params", Vec2i( 200, 250 ) );
    mParams->addParam( "FPS",  &mFps );
    mParams->addSeparator();
    mParams->addParam( "Bark gain",     &mBarkGain,     "min=0.1 max=500.0 step=0.01" );
    mParams->addParam( "Bark offset",   &mBarkOffset,   "min=-1.0 max=1.0 step=0.01" );
    mParams->addParam( "Bark damping",  &mBarkDamping,  "min=0.7 max=0.99 step=0.01" );
    mParams->addSeparator();
    mParams->addParam( "Min dist.",     &mMinDist,      "min=0.0 max=1000.0 step=1.0" );
    
    // initialise Xtract
    mXtract = ciXtract::create();
    
    // enable features
    mXtract->enableFeature( XTRACT_BARK_COEFFICIENTS );
    mXtract->enableFeature( XTRACT_SPECTRUM );
    
    // get feature references
    mBark       = mXtract->getFeature( XTRACT_BARK_COEFFICIENTS );
    
    initAudio();
    
    for( int k=0; k < 100; k++ )
    {
        Particle p;
        p.pos   = Vec2f( randInt( getWindowWidth() ), randInt( getWindowHeight() ) );
        p.vel   = Vec2f( randFloat( -0.1f, 0.1f ), 0.0f );
        p.size  = 5.0f;
        p.col   = Color::white();
        
        mParticles.push_back( p );
    }
}


void SoundParticlesApp::update()
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


void SoundParticlesApp::draw()
{
	gl::clear( Color::gray( 0.1f ) );
    gl::enableAlphaBlending();
    
    ciXtract::drawPcm( Rectf( 0, 0, getWindowWidth(), 60 ), mPcmBuffer.getData(), mPcmBuffer.getSize() / mPcmBuffer.getNumChannels() );
    
    ciXtract::drawData( mBark, Rectf( 15, 60, 140, 100 ) );
    
    drawParticles();
    
    mParams->draw();
}


void SoundParticlesApp::initAudio()
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


void SoundParticlesApp::updateParticles()
{
    for( size_t k=0; k < mParticles.size(); k++ )
    {
        
        mParticles[k].pos += mParticles[k].vel;
        
        if ( mParticles[k].pos.x < 0 && mParticles[k].vel.x < 0 )                  mParticles[k].pos.x = getWindowWidth() + mParticles[k].size;
        if ( mParticles[k].pos.x > getWindowWidth() && mParticles[k].vel.x > 0 )   mParticles[k].pos.x = - mParticles[k].size;
    }
}


void SoundParticlesApp::drawParticles()
{
    float               dist;
    int                 idx;
    shared_ptr<double>  data    = mBark->getResults();
    ColorA              col     = ColorA::white();
    
    for( size_t k=0; k < mParticles.size(); k++ )
    {
        gl::color( mParticles[k].col );
        gl::drawStrokedCircle( mParticles[k].pos, mParticles[k].size );
        
        for( size_t i=0; i < mParticles.size(); i++ )
        {
            dist    = mParticles[k].pos.distance( mParticles[i].pos );
            
            if ( k != i && dist < mMinDist )
            {
                idx     = ( k + i ) % mBark->getResultsN();
                col.a   = data.get()[idx];
                
                gl::color( col );
                gl::drawLine( mParticles[k].pos, mParticles[i].pos );
            }
        }
    }
}


CINDER_APP_NATIVE( SoundParticlesApp, RendererGl )

