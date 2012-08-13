// =====================================================================
// MotionTrackOSC SuperCollider help file
// =====================================================================

// open an instance of motion track osc and start it:
m = MotionTrackOSC.new( 0, true );

// get the current parameters:
m.info;

// see the parameters:
[m.prediction, m.mhidur, m.maxdelta, m.mindelta, m.threshold, m.difference]

// each of these parameters can be set

// subscribe to all data
m.subscribe( 1 );
m.subscribePos; // position
m.subscribeDir; // direction
m.subscribeRect; // rectangle
m.subscribePred; // predicted position

// the raw data is in a dictionary called motion:
m.motion

// the global direction:
m.motion[\global]

// the first item:
m.motion[0]

//and so on...

// The parameters of each item are:
m.motion[0].keys

size: size of rectangle
angle: direction angle
(dx,dy): direction x and y
(cx,cy): center x and y
(px,py): predicted x and y
(rx1,ry1): corner 1 of rectangle
(rx2,ry2): corner 2 of rectangle
lastTime: last time value was updated (since start of this process).

// you can set actions:
m.posAction
m.predAction
m.dirAction
m.rectAction
m.closeAction

// pause the tracking:
m.pause

m.pause( 0 );

m.pause( 1 );

// hide the graphical rects and circles:
m.hide( 1 ); // only works when motiontracking is not paused

m.hide( 0 );


// stop
m.stop;
// or:
m.quit;


// =====================================================================

//// TODO:
// Write an algorithm to match tracked motion dots to previous one, based on proximity to center position
// Filter out top N trackpoints (based on size, age, etc).

