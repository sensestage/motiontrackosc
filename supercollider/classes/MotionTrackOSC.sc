
/* $Id: MotionTrackOSC.sc 54 2009-02-06 14:54:20Z nescivi $ 
 *
 * Copyright (C) 2009, Marije Baalman <nescivi _at_ gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* class to interact with motiontrackosc */

MotionTrackOSC {

	//	var <>action;

	var <motion;
	var <prevmotion;
	
	var <>frameAction;
	var <>posAction;
	var <>dirAction;
	var <>rectAction;
	var <>predAction;

	var <>closeAction, <>startAction;
	var <prediction,<threshold,<mhidur,<maxdelta,<mindelta,<difference,<maxarea;
	var <>dumpEvents = false;

	var <responders;
	classvar <>path = "/usr/local/bin/";
	var <appaddr;
	
	*new { |device,autostart=true,autosubscribe=false|
		^super.new.init(device,autostart,autosubscribe);
	}

	makeResponders{
		responders = [
			OSCresponderNode(appaddr, '/motiontrack/error', 
				{ |t, r, msg| ("MotionTrack error: "+msg).postln; 
				}),
			OSCresponderNode(appaddr, '/motiontrackosc/quit', 
				{ |t, r, msg| 
					closeAction.value; 
					if ( dumpEvents ) { msg.postln };
				}),
			OSCresponderNode(appaddr, '/motiontrackosc/started', 
				{ |t, r, msg| 
					startAction.value;
					if ( dumpEvents ) { msg.postln };
				}),
			OSCresponderNode(appaddr, '/motiontrack/rectangle', 
				{ |t, r, msg| 
					//					if ( dumpEvents ) { msg.postln };
					rectAction.value( msg.copyToEnd( 1 ) );
					this.setMotion( \rectangle, msg[1], msg.copyToEnd( 2 ) );
				}),
			OSCresponderNode(appaddr, '/motiontrack/position', 
				{ |t, r, msg| 
					//					if ( dumpEvents ) { msg.postln };
					posAction.value( msg.copyToEnd( 1 ) );
					this.setMotion( \position, msg[1], msg.copyToEnd( 2 ) );
				}),
			OSCresponderNode(appaddr, '/motiontrack/direction', 
				{ |t, r, msg| 
					//					if ( dumpEvents ) { msg.postln };
					dirAction.value( msg.copyToEnd( 1 ) );
					this.setMotion( \direction, msg[1], msg.copyToEnd( 2 ) );
				}),
			OSCresponderNode(appaddr, '/motiontrack/predicted', 
				{ |t, r, msg| 
					//					if ( dumpEvents ) { msg.postln };
					predAction.value( msg.copyToEnd( 1 ) );
					this.setMotion( \predicted, msg[1], msg.copyToEnd( 2 ) );
				}),
			OSCresponderNode(appaddr, '/motiontrack/prediction', 
				{ |t, r, msg| 
					if ( dumpEvents ) { msg.postln };
					prediction = msg[1];
				}),
			OSCresponderNode(appaddr, '/motiontrack/frameupdate', 
				{ |t, r, msg| 
					if ( dumpEvents ) { msg.postln };
					this.frameupdate;
				}),
			OSCresponderNode(appaddr, '/motiontrack/framedone', 
				{ |t, r, msg| 
					if ( dumpEvents ) { msg.postln };
					// msg[1] is the number of detected points
					frameAction.value( msg[1] );
				}),
			OSCresponderNode(appaddr, '/motiontrack/maxarea', 
				{ |t, r, msg| 
					if ( dumpEvents ) { msg.postln };
					maxarea = msg[1];
				}),
			OSCresponderNode(appaddr, '/motiontrack/threshold', 
				{ |t, r, msg| 
					if ( dumpEvents ) { msg.postln };
					threshold = msg[1];
				}),
			OSCresponderNode(appaddr, '/motiontrack/difference', 
				{ |t, r, msg| 
					if ( dumpEvents ) { msg.postln };
					difference = msg[1];
				}),
			OSCresponderNode(appaddr, '/motiontrack/maxdelta', 
				{ |t, r, msg| 
					if ( dumpEvents ) { msg.postln };
					maxdelta = msg[1];
				}),
			OSCresponderNode(appaddr, '/motiontrack/mindelta', 
				{ |t, r, msg| 
					if ( dumpEvents ) { msg.postln };
					mindelta = msg[1];
				}),
			OSCresponderNode(appaddr, '/motiontrack/mhidur', 
				{ |t, r, msg| 
					if ( dumpEvents ) { msg.postln };
					mhidur = msg[1];
				})
		];
		
		responders.do{ |it| it.add };
	}

	clearResponders{
		responders.do{ |it| it.remove };
		responders = [];
	}

	pause{ |onoff=1|
		appaddr.sendMsg( "/motiontrack/pause", onoff );
	}

	hide{ |onoff=1|
		appaddr.sendMsg( "/motiontrack/hide", onoff );
	}

	hidecam{ |onoff=1|
		appaddr.sendMsg( "/motiontrack/hidecam", onoff );
	}

	subscribe{ |onoff=1|
		appaddr.sendMsg( "/motiontrack/subscribe", onoff );
	}

	subscribePos{ |onoff=1|
		appaddr.sendMsg( "/motiontrack/subscribe/position", onoff );
	}

	subscribeRect{ |onoff=1|
		appaddr.sendMsg( "/motiontrack/subscribe/rectangle", onoff );
	}

	subscribeDir{ |onoff=1|
		appaddr.sendMsg( "/motiontrack/subscribe/direction", onoff );
	}

	subscribePred{ |onoff=1|
		appaddr.sendMsg( "/motiontrack/subscribe/prediction", onoff );
	}

	start{ |device=""|
		var command;
		command = (path+/+"motiontrackosc "++device.asString++" \""++NetAddr.langPort.asString++"\" \"57151\"");
		command.unixCmd;
		command.postln;

		UI.registerForShutdown({
			this.stop;
		});
	}

	frameupdate{
		var newitem;
		motion.keysValuesDo{ |key,item|
			newitem = IdentityDictionary.new;
			item.keysValuesDo{ |k2,it2|
				newitem.put( k2, it2 );
			};
			prevmotion.put( key, newitem );
		};
	}

	setMotion{ |type,id,pars|
		if ( dumpEvents, { [type,id,pars].postln; });
		if ( id == -1 ) { id = \global };
		if ( motion[id].isNil, {
			motion.add(id -> IdentityDictionary.new );
		});
		if ( type == \position ){
			motion[ id ].put( \cx, pars[0] );
			motion[ id ].put( \cy, pars[1] );
		};
		if ( type == \direction ){
			motion[ id ].put( \dx, pars[0] );
			motion[ id ].put( \dy, pars[1] );
			motion[ id ].put( \angle, pars[2] );
		};
		if ( type == \rectangle ){
			motion[ id ].put( \rx1, pars[0] );
			motion[ id ].put( \ry1, pars[1] );
			motion[ id ].put( \rx2, pars[2] );
			motion[ id ].put( \ry2, pars[3] );
			motion[ id ].put( \size, ( pars[2]-pars[0] ) * (pars[3]*pars[1]) );
		};
		if ( type == \predicted ){
			motion[ id ].put( \px, pars[0] );
			motion[ id ].put( \py, pars[1] );
		};
		motion[id].put( \lastTime, Process.elapsedTime );
	}

	info{
		appaddr.sendMsg( "/motiontrack/info" );
	}

	mhidur_{ |val|
		appaddr.sendMsg( "/motiontrack/mhidur", val );
	}

	maxarea_{ |val|
		appaddr.sendMsg( "/motiontrack/maxarea", val.asInteger );
	}

	maxdelta_{ |val|
		appaddr.sendMsg( "/motiontrack/maxdelta", val );
	}

	printInfo{
		// see the parameters:
		[
			[\prediction, \mhidur, \maxdelta, \mindelta, \threshold, \difference, \maxarea],
			[prediction, mhidur, maxdelta, mindelta, threshold, difference, maxarea]
		].flop.postcs;
	}

	mindelta_{ |val|
		appaddr.sendMsg( "/motiontrack/mindelta", val );
	}

	prediction_{ |val|
		appaddr.sendMsg( "/motiontrack/prediction", val );
	}

	difference_{ |val|
		appaddr.sendMsg( "/motiontrack/difference", val );
	}

	threshold_{ |val|
		appaddr.sendMsg( "/motiontrack/threshold", val );
	}

	stop{
		appaddr.sendMsg( "/motiontrack/quit" );
		this.clearResponders;
	}

	quit{
		this.stop;
	}

	init { |device,autostart=true,autosubscribe=false|

		motion = IdentityDictionary.new;
		prevmotion = IdentityDictionary.new;

		appaddr = NetAddr.new( "localhost", 57151 );
		this.makeResponders;

		if ( autosubscribe, { startAction = { this.subscribe }});

		if ( autostart , { this.start( device ); });

		frameAction = {};
		posAction = {};
		dirAction = {};
		rectAction = {};
		predAction = {};
		closeAction = { "MotionTrackOSC closed".postln; };
	}
}
