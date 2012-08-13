// =====================================================================
// MotionTracker help file
// =====================================================================

// Note: to use this class, you need to install the SenseWorld quark.
// Install it by:
Quarks.install("SenseWorld");
// and recompile the library. For more information see the Quarks helpfile

~activecam = ~activecam ? 0;

(
// open an instance of motion track osc and start it:
q = MotionTrackOSC.new( ~activecam , true, true );

p = MotionTracker.new;
p.tracker = q;

p.makeGUI;
);

(
// settings for the motion tracking:
q.maxarea = 150;
q.prediction = 0.2;
q.mhidur = 0.5;
q.maxdelta = 0.125;
q.threshold = 0.05;
q.difference = 30;

p.th = 6; // portion of the max dist a point has to be (max. dist (2.sqrt)/th)
p.minAge = 0.75;
p.forgetTime = 5;

);

(
q.frameAction = { |maxn|
	var tp,speed,acc;
	p.update( maxn );
	(1..10).do{ |it|
		tp = p.trackdots.at( it );
		if( tp.notNil ){
			speed = tp.speed;
			acc = tp.accelleration;
			[ it,
				tp.position.x, tp.position.y, 
				tp.direction.x, tp.direction.y, 
				tp.predicted.x, tp.predicted.y, 
				speed.x, speed.y, 
				acc.x, acc.y,
				tp.rectangle.extent.rho,
				tp.active.binaryValue ].postln;
		}
	}

};
);
