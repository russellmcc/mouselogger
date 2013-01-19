//
//  TouchLogger.cpp
//  MouseLogger
//
//  Created by James Russell McClellan on 1/19/13.
//
//

#include "TouchLogger.h"
#include "cinder/Cinder.h"
#include "cinder/System.h"
#include "cinder/app/AppNative.h"
#include <sstream>
#include <boost/optional.hpp>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>

using namespace std;
using namespace ci;
using namespace ci::app;
using boost::optional;

namespace {
    const string kAddedTag = "added";
    const string kEndedTag = "ended";
    const string kMovedTag = "moved";
}

class TouchEventLogger : public TouchLogger
{
  public:
    TouchEventLogger(ostream& stream):
      mStream(stream)
    {}
    
    virtual void LogFrame(const TouchList& currTouches) override
    {
        // detect any changes since the last log.
        stringstream changes("");    
        for(const pair<uint32_t, Vec2f>& t : currTouches) {
            if(mLastLoggedList.find(t.first) == mLastLoggedList.end())
                changes<<","<<kAddedTag<<","<<t.first<<","<<t.second.x<<","<<t.second.y;
            else if(mLastLoggedList[t.first] != t.second)
                changes<<","<<kMovedTag<<","<<t.first<<","<<t.second.x<<","<<t.second.y;
        }
        
        for(const pair<uint32_t, Vec2f>& t : mLastLoggedList) {
            if(currTouches.find(t.first) == currTouches.end())
                changes<<","<<kEndedTag<<","<<t.first<<","<<t.second.x<<","<<t.second.y;
        }
        
        // figure out what time it is.
        double currTime = mTimeZero ?
                            getElapsedSeconds() - *mTimeZero :
                            0;
        
        if(not mTimeZero) mTimeZero = getElapsedSeconds();        
        
        // if there were changes, write them out.
        if(changes.str().length())
        {
            mStream<<currTime<<changes.str()<<endl;
        }
        
        mLastLoggedList = currTouches;
    }
    
  private:
    TouchList mLastLoggedList;
    ostream& mStream;
    optional<double> mTimeZero;
};

class TouchEventLogReader : public TouchLogReader
{
  public:
    TouchEventLogReader(istream& stream) :
      mStream(stream),
      mCurrentTime(0)
    {
        nextLine();
    }
    
    virtual void readUntil(double logTime) override
    {

        while(logTime >= mCurrentTime)
        {
            if(isLogComplete())
                return;

            // do the current line.
            boost::char_separator<char> sep(",");
            boost::tokenizer<boost::char_separator<char> > tok(mCurrLine, sep);
            boost::tokenizer<boost::char_separator<char> >::iterator curr = tok.begin();
            ++curr; // skip the time token
            while(curr != tok.end())
            {
                try {
                    string tag = *curr;
                    uint32_t id = boost::lexical_cast<uint32_t>(*(++curr));
                    Vec2f v;
                    v.x = boost::lexical_cast<double>(*(++curr));
                    v.y = boost::lexical_cast<double>(*(++curr));
                    if(tag == kAddedTag) {
                        mTouchBegan->call(id, v);
                        mCurrentState[id] = v;
                    } else if (tag == kMovedTag) {
                        mTouchMoved->call(id, v);
                        mCurrentState[id] = v;
                    } else if (tag == kEndedTag) {
                        mTouchEnded->call(id, v);
                        mCurrentState.erase(id);
                    } else {
                        console() << "Parsing error! - unknown tag " << tag;
                    }
                    ++curr;
                }
                catch(...)
                {
                    console() << "Parsing error!";
                }
            }
            nextLine();
        }
    }
    
    virtual TouchList getCurrentState() override
    {
        return mCurrentState;
    }
    
    virtual bool isLogComplete() override
    {
        return not(mCurrLine.length() or mStream.good());
    }
    
  private:
    TouchList mCurrentState;
    double mCurrentTime;
    istream& mStream;
    string mCurrLine;
    
    void nextLine()
    {
        while(not isLogComplete())
        {
            if(mStream.good())
            {
                string line;
                getline(mStream, line);
                
                // the first token is the time.
                boost::char_separator<char> sep(",");
                boost::tokenizer<boost::char_separator<char> > tok(line, sep);
                boost::tokenizer<boost::char_separator<char> >::iterator curr = tok.begin();
            
                // blank line?
                if(curr == tok.end())
                    continue;
                
                try {
                    double newTime = boost::lexical_cast<double>(*curr);
                    mCurrLine = line;
                    mCurrentTime = newTime;
                    return;
                }
                catch(...){}
            }
            else
                mCurrLine = "";
        }
    }
};

shared_ptr<TouchLogger>
TouchLogger::create(ostream& logStream)
{
    // c+11: should be make_shared<TouchEventLogger>(logStream)
    return static_pointer_cast<TouchLogger, TouchEventLogger>(
        shared_ptr<TouchEventLogger>(new TouchEventLogger(logStream))
    );
}

shared_ptr<TouchLogReader>
TouchLogReader::create(istream& logStream)
{
    // c+11: should be make_shared<TouchEventLogger>(logStream)
    return static_pointer_cast<TouchLogReader, TouchEventLogReader>(
        shared_ptr<TouchEventLogReader>(new TouchEventLogReader(logStream))
    );
}

TouchLogReader::TouchLogReader() :
    mTouchBegan(new TouchSignal()),
    mTouchMoved(new TouchSignal()),
    mTouchEnded(new TouchSignal())
{
}

namespace {
    bool SignalIsGone(pair<weak_ptr<TouchLogReader::TouchSignal>, CallbackId>& t){
        return not t.first.lock();
    }
}

// stupid glue function
void TouchLogReader::addListener(TouchLogReader::Listener& l)
{
    // delete all deleted listeners here so the connection list
    // doesn't balloon
    remove_if(l.fConnections.begin(), l.fConnections.end(), SignalIsGone);
    
    l.fConnections.push_back(make_pair(mTouchBegan,mTouchBegan->registerCb(
        bind(&Listener::touchBegan, &l, _1, _2))));
    l.fConnections.push_back(make_pair(mTouchMoved,mTouchMoved->registerCb(
        bind(&Listener::touchMoved, &l, _1, _2))));
    l.fConnections.push_back(make_pair(mTouchEnded,mTouchEnded->registerCb(
        bind(&Listener::touchEnded, &l, _1, _2))));
}

TouchLogReader::Listener::~Listener()
{
    for(const pair<weak_ptr<TouchSignal>, CallbackId>& connection : fConnections) {
        shared_ptr<TouchSignal> locked = connection.first.lock();
        if(locked)
            locked->unregisterCb(connection.second);
    }
}

