#include "cinder/app/AppCocoaTouch.h"
#include "cinder/app/Renderer.h"
#include "cinder/Surface.h"
#include "cinder/Rand.h"
#include "TouchLogger.h"

#include <cmath>
#include <fstream>

#import <UIKit/UIKit.h>

using namespace ci;
using namespace ci::app;

#include <map>
using namespace std;

namespace
{
    const Vec2f kRecordButtonPos(25,25);
    const double kRecordButtonRadius = 25;
}

// this class taken from the cinder example.
class MouseTouchDrawer : public TouchListener
{
    struct TouchPoint {
        TouchPoint() {}
        TouchPoint( const Vec2f &initialPt, const Color &color ) : mColor( color ), mTimeOfDeath( -1.0 ) 
        {
            mLine.push_back( initialPt ); 
        }
        
        void addPoint( const Vec2f &pt ) { mLine.push_back( pt ); }
        
        void draw() const
        {
            if( mTimeOfDeath > 0 ) // are we dying? then fade out
                gl::color( ColorA( mColor, ( mTimeOfDeath - getElapsedSeconds() ) / 2.0f ) );
            else
                gl::color( mColor );

            gl::draw( mLine );
        }
        
        void startDying() { mTimeOfDeath = getElapsedSeconds() + 2.0f; } // two seconds til dead
        
        bool isDead() const { return getElapsedSeconds() > mTimeOfDeath; }
        
        PolyLine<Vec2f>	mLine;
        Color			mColor;
        float			mTimeOfDeath;
    };
  public:

   virtual void	touchBegan( uint32_t id, ci::Vec2f pos )
   {
   		Color newColor( CM_HSV, Rand::randFloat(), 1, 1 );
		mActivePoints.insert( make_pair( id, TouchPoint( pos, newColor ) ) );
   }
   virtual void	touchMoved( uint32_t id, ci::Vec2f pos )
   {
        mActivePoints[id].addPoint( pos );
   }
   virtual void	touchEnded( uint32_t id, ci::Vec2f pos )
   {
        mActivePoints[id].startDying();
		mDyingPoints.push_back( mActivePoints[id] );
		mActivePoints.erase( id );
   }
   void draw()
   {
        gl::enableAlphaBlending();

        for( map<uint32_t,TouchPoint>::const_iterator activeIt = mActivePoints.begin(); activeIt != mActivePoints.end(); ++activeIt ) {
            activeIt->second.draw();
        }

        for( list<TouchPoint>::iterator dyingIt = mDyingPoints.begin(); dyingIt != mDyingPoints.end(); ) {
            dyingIt->draw();
            if( dyingIt->isDead() )
                dyingIt = mDyingPoints.erase( dyingIt );
            else
                ++dyingIt;
        } 
   }
   private:
        map<uint32_t,TouchPoint>	mActivePoints;
        list<TouchPoint>			mDyingPoints;

};

class MouseLoggerApp : public AppCocoaTouch, public TouchProvider {
  public:
	virtual void	setup();
	virtual void	update();
	virtual void	draw();
    
    void	touchesBegan( TouchEvent event );
	void	touchesMoved( TouchEvent event );
	void	touchesEnded( TouchEvent event );

    void    toggleLog();

  private:
    bool mChanged;
    
    void startLogPlayback();

    shared_ptr<TouchLogger> mLogger;
    shared_ptr<TouchLogReader> mReader;
    double mPlaybackStartTime;
    shared_ptr<MouseTouchDrawer> mDrawer;
    stringstream mLog;
};

void MouseLoggerApp::setup()
{
    mChanged = false;
    mLogger.reset();
    mDrawer.reset(new MouseTouchDrawer());
    addListener(*mDrawer);
}

void MouseLoggerApp::startLogPlayback()
{
    //reset the log.
    mLog.clear();
    mLog.seekg(0);
    mPlaybackStartTime = getElapsedSeconds();
    mDrawer.reset(new MouseTouchDrawer());
    mReader = TouchLogReader::create(mLog);
    mReader->addListener(*mDrawer);
}

void MouseLoggerApp::update()
{
    if(mReader)
    {
        double playbackTime = getElapsedSeconds() - mPlaybackStartTime;
        mReader->readUntil(playbackTime);
        // loop playback.
        if(mReader->isLogComplete())
            startLogPlayback();
    }
}

void MouseLoggerApp::touchesBegan( TouchEvent event )
{
    for(const TouchEvent::Touch& t : event.getTouches()) {
        mTouchBegan->call(t.getId(), t.getPos());
    }
}

void MouseLoggerApp::touchesMoved( TouchEvent event )
{
    for(const TouchEvent::Touch& t : event.getTouches()) {
        mTouchMoved->call(t.getId(), t.getPos());
    }
}

void MouseLoggerApp::toggleLog()
{
    if(mLogger) {
        mLogger.reset();
        startLogPlayback();
    }
    else {
        mLog.clear();
        mLog.seekp(0);
        mDrawer.reset(new MouseTouchDrawer());
        mLogger = TouchLogger::create(mLog);
        addListener(*mLogger);
        addListener(*mDrawer);
    }
}

void MouseLoggerApp::touchesEnded( TouchEvent event )
{
    for(const TouchEvent::Touch& t : event.getTouches()) {
        mTouchEnded->call(t.getId(), t.getPos());
        
        if(t.getPos().distanceSquared(kRecordButtonPos) < kRecordButtonRadius*kRecordButtonRadius)
            toggleLog();
    }
}

namespace{
    Color colorFromHex(uint32_t hex)
    {
        return Color(static_cast<double>((hex>>16)&0xFF) / 0xFF,
                     static_cast<double>((hex>> 8)&0xFF) / 0xFF,
                     static_cast<double>((hex    )&0xFF) / 0xFF);
    }
}

void MouseLoggerApp::draw()
{    
    static const Color kBgColor(colorFromHex(0x987DC4));
    
    gl::clear( kBgColor );
    
    // draw the record button
    {
        static const double kBlinkTime = 2;
        static const Color kRecordOffColor(colorFromHex(0x855DC4));
        static const Color kRecordOnColor1(colorFromHex(0x8FB6FF));
        static const Color kRecordOnColor2(colorFromHex(0x6580D8));
        
        double whocares;
        double blinkIndex = modf(getElapsedSeconds() / kBlinkTime, &whocares);
        double blinkBlend = blinkIndex < .5 ? blinkIndex * 2 :
                                              2* (1 - blinkIndex);
        gl::color( mLogger ?
                    (kRecordOnColor1*blinkBlend + kRecordOnColor2*(1-blinkBlend)) :
                    kRecordOffColor
                    );
        
        gl::drawSolidCircle(kRecordButtonPos, kRecordButtonRadius);
    }
    
    if(mDrawer)
        mDrawer->draw();
}

CINDER_APP_COCOA_TOUCH( MouseLoggerApp, RendererGl )
