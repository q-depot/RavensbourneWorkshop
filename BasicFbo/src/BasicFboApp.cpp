#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "cinder/gl/Fbo.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class BasicFboApp : public AppNative {
  public:
	void setup();
	void mouseDown( MouseEvent event );	
	void update();
	void draw();
	void renderSceneToFbo();
	
	gl::Fbo         mFbo;
};


void BasicFboApp::setup()
{
	gl::Fbo::Format format;
    //	format.setSamples( 4 ); // uncomment this to enable 4x antialiasing
	mFbo = gl::Fbo( 300, 250, format );
}


void BasicFboApp::mouseDown( MouseEvent event )
{
}


void BasicFboApp::update()
{
    renderSceneToFbo();
}


void BasicFboApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) );
    
    gl::draw( mFbo.getTexture() );
}


void BasicFboApp::renderSceneToFbo()
{
	// this will restore the old framebuffer binding when we leave this function
	// on non-OpenGL ES platforms, you can just call mFbo.unbindFramebuffer() at the end of the function
	// but this will restore the "screen" FBO on OpenGL ES, and does the right thing on both platforms
	gl::SaveFramebufferBinding bindingSaver;
	
	// bind the framebuffer - now everything we draw will go there
	mFbo.bindFramebuffer();
    
	// setup the matrices and the viewport to match the dimensions of the FBO
    gl::setMatricesWindow( mFbo.getSize() );
	gl::setViewport( mFbo.getBounds() );
    
	// clear out the FBO with blue
	gl::clear( Color( 0.25, 0.5f, 1.0f ) );
    
    // draw something
    gl::drawSolidCircle( mFbo.getSize() * 0.5f, 50 );
    
    // unbind the framebuffer - now everything we draw will go there ( not needed in this case because of "SaveFramebufferBinding"
	mFbo.unbindFramebuffer();
    
    // reset the matrices and the viewport to the current window size
    gl::setMatricesWindow( getWindowSize() );
    gl::setViewport( getWindowBounds() );
}


CINDER_APP_NATIVE( BasicFboApp, RendererGl )
