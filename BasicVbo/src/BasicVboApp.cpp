#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "cinder/gl/Vbo.h"
#include "cinder/gl/Texture.h"
#include "cinder/Camera.h"
#include "cinder/ImageIo.h"
#include "cinder/Rand.h"

#include "cinder/params/Params.h"


using namespace ci;
using namespace ci::app;
using namespace std;


class BasicVboApp : public AppNative {

public:
    
	void setup();
    void mouseDown( MouseEvent event );
    void mouseDrag( MouseEvent event );
	void update();
	void draw();
    void createVboTriangles();
    
	gl::VboMeshRef          mVboMesh;
	gl::TextureRef          mTexture;
	CameraPersp             mCamera;
    
    params::InterfaceGlRef  mParams;
};


void BasicVboApp::setup()
{
    mParams = params::InterfaceGl::create( "params", Vec2i( 200, 250 ) );
    
    createVboTriangles();
}


void BasicVboApp::mouseDown( MouseEvent event )
{
    
}


void BasicVboApp::mouseDrag( MouseEvent event )
{
    
}


void BasicVboApp::update()
{
    // update Vbo
    // ..
}


void BasicVboApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) );
    
//  gl::setMatrices( mCamera );
    
    gl::draw( mVboMesh );
    
    gl::setMatricesWindow( getWindowSize() );
}


void BasicVboApp::createVboTriangles()
{
    vector<uint32_t>    indices;
    vector<Vec3f>       positions;
    Vec3f               pos( 15, getWindowHeight() * 0.5f, 0 );
    
    int numPrimitives   = 10;
    int numVertices     = numPrimitives * 3;
    int numIndices      = numVertices;
    
	gl::VboMesh::Layout layout;
	layout.setStaticIndices();
	layout.setStaticPositions();
	mVboMesh = gl::VboMesh::create( numVertices, numIndices, layout, GL_TRIANGLES );
	
	// buffer our static data - the texcoords and the indices
	for( int k = 0; k < numPrimitives; k++ )
    {
        // create a 3 indices for each triangle(indipendent vertices)
        indices.push_back( k * 3 + 0 );
        indices.push_back( k * 3 + 1 );
        indices.push_back( k * 3 + 2 );
        
        // create 3 vertices
        positions.push_back( pos + Vec3f( randFloat(50), randFloat(50), 0 ) );
        positions.push_back( pos + Vec3f( randFloat(50), randFloat(50), 0 ) );
        positions.push_back( pos + Vec3f( randFloat(50), randFloat(50), 0 ) );
        
        pos.x += 60;
	}
	
	mVboMesh->bufferIndices( indices );
    mVboMesh->bufferPositions( positions );
	
//	mTexture = gl::Texture::create( loadImage( loadAsset( "lena.jpg" ) ) );
}


CINDER_APP_NATIVE( BasicVboApp, RendererGl )
