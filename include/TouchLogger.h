//
//  TouchLogger.h
//  MouseLogger
//
//  Created by James Russell McClellan on 1/19/13.
//
//

#ifndef MouseLogger_TouchLogger_h
#define MouseLogger_TouchLogger_h

#include <map>
#include <memory>
#include "cinder/Vector.h"
#include "cinder/Function.h"

typedef ci::CallbackMgr<void (uint32_t, ci::Vec2f)> TouchSignal;
typedef std::map<uint32_t, ci::Vec2f> TouchList;

class TouchProvider;

// derive from this to indicate that you listen to events.
class TouchListener
{
  public:
    virtual ~TouchListener();
    virtual void	touchBegan( uint32_t id, ci::Vec2f pos ) = 0;
    virtual void	touchMoved( uint32_t id, ci::Vec2f pos ) = 0;
    virtual void	touchEnded( uint32_t id, ci::Vec2f pos ) = 0;

  private:
    // we unregister these when we die.  this is really frustrating!  I would use
    // boost::signals which handles all of this for me but cinder's bullshit
    // fucking with the global namespace prevents it.  never fuck with the global namespace!
    std::vector<std::pair<std::weak_ptr<TouchSignal>, ci::CallbackId> > fConnections;
    
    friend class TouchProvider;
};

// derive from this to provide touches to others.
class TouchProvider
{
public:
    TouchProvider();
    virtual ~TouchProvider() {}
    virtual void addListener(TouchListener& l);
    
protected:
    std::shared_ptr<TouchSignal> mTouchBegan;
    std::shared_ptr<TouchSignal> mTouchMoved;
    std::shared_ptr<TouchSignal> mTouchEnded;

private:
    TouchProvider(const TouchProvider&);
    const TouchProvider& operator=(const TouchProvider&);
};

class TouchLogger : public TouchListener
{
  public:
    virtual ~TouchLogger(){}
  
    static std::shared_ptr<TouchLogger> create(std::ostream& logStream);
};

class TouchLogReader : public TouchProvider
{
  public:
    virtual ~TouchLogReader(){}
    
    virtual void readUntil(double logTime) = 0;
    virtual TouchList getCurrentState() = 0;
    virtual bool isLogComplete() = 0;

    static std::shared_ptr<TouchLogReader> create(std::istream& logStream);
};

#endif
