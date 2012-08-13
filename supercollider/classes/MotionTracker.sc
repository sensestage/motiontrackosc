/* $Id: MotionTracker.sc 54 2009-02-06 14:54:20Z nescivi $ 
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

MotionTracker{

	// the motion tracker (most likely MotionTrackOSC)
	var <>tracker;

	var <trackdots;
	var <tempTrackdots;

	var <allocator;
	var <tempAllocator;

	var <>minAge = 1;

	var <>th = 20;
	var <>forgetTime = 1;
	var <>activerate = 5;

	var <>hissize = 20;
	var <>scale = 0.1;
	var <>accscale = 1;
	var <>speedscale = 1;

	var <>boundingRect;

	var <>transformFunc;

	// gui stuff:
	var <w,<updButton,<buttons,<drawview,<speedview,<accview;
	var <guiwatcher;

	*new{
		^super.new.init;
	}

	init{
		trackdots = IdentityDictionary.new;
		tempTrackdots = IdentityDictionary.new;
		allocator = LowNumberAllocator.new(1,20);
		tempAllocator = LowNumberAllocator.new(1,1000);

		transformFunc = {|v| v };
		boundingRect = Rect( 0, 0, 1, 1 );
	}

	updateTD{
		var time;
		trackdots.keysValuesDo{ |key,item2|
			item2.updated = false;
			time = (Process.elapsedTime - item2.lastTime);
			if ( time > forgetTime ){
				trackdots.removeAt( key );
				allocator.free( key );
			};
			if ( time > (forgetTime/activerate) ){
				item2.active = false;
			}
		};
		tempTrackdots.keysValuesDo{ |key,item2|
			item2.updated = false;
			time = (Process.elapsedTime - item2.lastTime);
			if ( time > minAge ){
				tempTrackdots.removeAt( key );
				tempAllocator.free( key );
			};
			if ( item2.age > minAge ){
				trackdots.put( allocator.alloc, item2 );
				tempTrackdots.removeAt( key );
				tempAllocator.free( key );
			};
		};
	}

	update{	|maxn|
		var tp, op, dist;
		var mdist;
		var mid;
		//	maxn.postln;
		this.updateTD;
		if ( maxn > -1){
			tracker.motion.keysValuesDo{ |key,item|
				mid = nil;
				mdist = 2.sqrt / th;
				//	[key, maxn, mdist].postln;
				if ( (key != \global) and: ( key < (maxn+1) ) ){
					tp = Point( item[\cx], item[\cy]);
					tp = transformFunc.value( tp );
					if ( boundingRect.contains( tp ) ){
						trackdots.keysValuesDo{ |key2,item2|
							if ( item2.updated.not ){
								dist = tp.dist( item2.position );
								//	[ key, key2, item[\cx].round(0.0001), item[\cy].round(0.0001), item2.position.x.round(0.0001), item2.position.y.round(0.0001), dist ].postln;
								if ( dist < mdist ){
									mdist = dist;
									mid = key2;
								};
							};
						};
						if ( mid.notNil) {
							//	[ key, mid, mdist ].postln;
							// update the trackdot:
							trackdots[mid].parsFromDict( item );
							
						}{
							tempTrackdots.keysValuesDo{ |key2,item2|
								if ( item2.updated.not ){
									dist = tp.dist( item2.position );
									//	[ key, key2, item[\cx].round(0.0001), item[\cy].round(0.0001), item2.position.x.round(0.0001), item2.position.y.round(0.0001), dist ].postln;
									if ( dist < mdist ){
										mdist = dist;
										mid = key2;
									};
								};
							};
							if ( mid.notNil) {
								//	[ key, mid, mdist ].postln;
								// update the trackdot:
								tempTrackdots[mid].parsFromDict( item );
							}{
								//	"new trackdot".postln;
								// no trackdot found, so it is a new one:
								tempTrackdots.put( tempAllocator.alloc, MTTrackDot.new( item.at( \lastTime) ).parsFromDict( item ) );
							}
						};
					};
				};
			};
		}
	}


	makeGUI{ |parent,xpos,ypos|
		var xsize, ysize, butview;

		xsize = 400;
		ysize = 300;
		xpos = xpos ? 180;
		ypos = ypos ? 400;

		w = parent ?? { 
			w = GUI.window.new("MotionTracker", Rect(xpos, ypos, xsize, ysize + (xsize/2))).front; 
			guiwatcher = SkipJack( { this.updateGui }, 0.1, {w.isClosed}, "MotionTracker", autostart: false );
			w;
		};

		drawview = GUI.userView.new( w, Rect( 0, 0, xsize, ysize ) ).background_( Color.white );

		speedview = GUI.userView.new( w, Rect( 0, ysize, xsize/2, xsize/2 ) ).background_( Color.black );
		accview = GUI.userView.new( w, Rect( xsize/2, ysize, xsize/2, xsize/2 ) ).background_( Color.gray );

		drawview.drawFunc = {
			var xc,yc,size,his,x2,y2,id;
			GUI.pen.color = Color.blue;
			GUI.pen.strokeRect( 
				boundingRect.moveTo( drawview.bounds.left, drawview.bounds.top ).resizeBy( drawview.bounds.width, drawview.bounds.height );
			);
			trackdots.keysValuesDo{ |key,it|
				// draw the dots:
				if ( it.active ){
					GUI.pen.color = Color.black;
				}{
					GUI.pen.color = Color.gray;
				};
				size = it.rectangle.extent.rho * 10;
				xc = (it.position.x * xsize).round(1) + drawview.bounds.left + 10;
				yc = (it.position.y * ysize).round(1) + drawview.bounds.top + 10;

				//	[key,xc,yc].postln;
				//	GUI.pen.fillOval( Rect( xc - (size/2) , yc - (size/2), size, size ));
				GUI.pen.fillRect( 
					Rect( 
						xc - ( it.rectangle.width  * xsize * scale), 
						yc - ( it.rectangle.height * ysize * scale), 
						it.rectangle.width * xsize * scale, 
						it.rectangle.height * ysize * scale
					) );
				// draw history:
				
				his = it.history.getLast( hissize ).collect{ |jt| jt.data; };

				/*
				// only take active ones:
				id = his.detectIndex{ |jt| jt.last == false };
				if ( id.notNil, {
					his = his.copyFromStart( id-1 );
				});
				*/
					
				GUI.pen.moveTo( xc @ yc );
				// collect only position:
				his = his.collect{ |jt| jt[0] };
				his.do{ |jt,j|
					x2 = (jt.x*xsize + drawview.bounds.left + 10);
					y2 = (jt.y*ysize + drawview.bounds.top + 10);
					GUI.pen.lineTo( x2.floor @ y2.floor );
				};
				GUI.pen.stroke;
				
				// draw the direction vector:
				if ( it.active ){
					GUI.pen.color = Color.red;
				}{
					GUI.pen.color = Color.gray;
				};

				GUI.pen.moveTo( xc @ yc );
				//	id = it.calcVector( dirsize ) * 2;
				x2 = ( (it.predicted.x *xsize) + (it.direction.x*10) + drawview.bounds.left + 10);
				y2 = ( (it.predicted.y *ysize) + (it.direction.y*10) + drawview.bounds.top + 10);
				GUI.pen.lineTo( x2 @ y2 );
				GUI.pen.stroke;
				
				if ( it.active ){
					GUI.pen.color = Color.blue;
				}{
					GUI.pen.color = Color.gray;
				};
				
				GUI.pen.stringAtPoint( key.asString, Point( xc, yc + 5 ) );
				xc = (it.predicted.x * xsize).round(1);
				yc = (it.predicted.y * ysize).round(1);
				//	[key,xc,yc].postln;
				GUI.pen.fillOval( Rect( xc - (size/2) + 10 + drawview.bounds.left, yc - (size/2) + 10 + drawview.bounds.top, size, size ));
				
			};
		};

		speedview.drawFunc = {
			var xc,yc,size,his,x2,y2,id;
			var speed;
			trackdots.keysValuesDo{ |key,it|
				// draw the dots:
				GUI.pen.color = Color.white;
				if ( it.active ){
					GUI.pen.color = Color.white;
				}{
					GUI.pen.color = Color.gray;
				};
				size = it.rectangle.extent.rho * 10;
				speed = it.speed * speedscale;
				xc = (xsize/2) + (speed.x * (xsize/4)).round(1) + speedview.bounds.left + 10;
				yc = (xsize/2) + (speed.y * (xsize/4)).round(1) + speedview.bounds.top + 10;

				//	[xc,yc,speedview.bounds].postln;

				GUI.pen.fillOval( Rect( xc - (size/2) , yc - (size/2), size, size ));
				
				GUI.pen.stringAtPoint( key.asString, Point( xc, yc + 5 ) );
				
				GUI.pen.color = Color(0.25,0.25,0.25,1);
				GUI.pen.moveTo( (xsize/4 + speedview.bounds.left) @ speedview.bounds.top );
				GUI.pen.lineTo( (xsize/4 + speedview.bounds.left) @ (speedview.bounds.top + speedview.bounds.height) );
				GUI.pen.stroke;
				GUI.pen.moveTo( speedview.bounds.left @ (xsize/4 + speedview.bounds.top) );
				GUI.pen.lineTo( (speedview.bounds.left + speedview.bounds.width) @ (xsize/4 + speedview.bounds.top) );
				GUI.pen.stroke;
			};
		};

		accview.drawFunc = {
			var xc,yc,size,his,x2,y2,id;
			var speed;
			trackdots.keysValuesDo{ |key,it|
				// draw the dots:
				if ( it.active ){
					GUI.pen.color = Color.white;
				}{
					GUI.pen.color = Color.black;
				};
				size = it.rectangle.extent.rho * 10;
				speed = it.accelleration * accscale;
				xc = (xsize/2) + (speed.x * (xsize/4)).round(1) + accview.bounds.left + 10;
				yc = (xsize/2) + (speed.y * (xsize/4)).round(1) + accview.bounds.top + 10;

				GUI.pen.fillOval( Rect( xc - (size/2) , yc - (size/2), size, size ));
				
				GUI.pen.stringAtPoint( key.asString, Point( xc, yc + 5 ) );

				GUI.pen.color = Color(0.25,0.25,0.25,1);
				GUI.pen.moveTo( (xsize/4 + accview.bounds.left) @ accview.bounds.top );
				GUI.pen.lineTo( (xsize/4 + accview.bounds.left) @ (accview.bounds.top + accview.bounds.height) );
				GUI.pen.stroke;
				GUI.pen.moveTo( accview.bounds.left @ (xsize/4 + accview.bounds.top) );
				GUI.pen.lineTo( (accview.bounds.left + accview.bounds.width) @ (xsize/4 + accview.bounds.top) );
				GUI.pen.stroke;
				
			};
		};
	
		if ( guiwatcher.notNil, { guiwatcher.start } );

		w.front;
	}

	updateGui{
		defer{
			// refresh the drawview:
			w.refresh;
		}
	}


}


MTTrackDot{

	classvar <>overwrite = true;

	var <direction;
	var <position;
	var <rectangle;
	var <predicted;
	var <lastTime;
	var <>age;

	var <sp,<acc;

	var <history;

	var <>updated = false;

	var <>active = false;
	//	var <overlap = true;

	*new{ |lt|
		^super.new.init(lt);
	}

	init{ |lt|
		age = 0;
		position = Point(0,0);
		predicted = Point(0,0);
		rectangle = Rect(0,0,0,0);
		direction = Point( 0, 0);
		lastTime = lt;
		history = DataCollector.new;

		sp = Point(0,0);
		acc = Point(0,0);
	}

	parsFromDict{ |dict|

		age = age + dict[\lastTime] - lastTime;

		active = true;
		updated = true;
		position.set( dict[\cx], dict[\cy]);
		direction.set( dict[\dx], dict[\dy]);
		predicted.set( dict[\px], dict[\py]);
		rectangle.set( dict[\rx1], dict[\ry1], dict[\rx2] - dict[\rx1], dict[\ry2] - dict[\ry1] );

		lastTime = dict[\lastTime];

		history.addData( [ Point.fromPoint( position), Rect.fromRect(rectangle), Point.fromPoint( direction ), Point.fromPoint( predicted ), lastTime, active ], overwrite: overwrite );
	}

	speed{
		var a,b,c;
		a = history.getLast( 3 );
		b = a.collect{ |it| it.data[0] };
		if ( b.size >= 3 ){
			c = b[2] - 4*b[1] + 3*b[0] / 2;
			sp = c;
		}{
			c = sp;
		};
		^c;
	}

	accelleration{
		var a,b,c;
		a = history.getLast( 3 );
		b = a.collect{ |it| it.data[0] };
		if ( b.size >= 3 ){
			c = b[2] - 2*b[1] + b[0];
			acc = c;
		}{
			c = acc;
		};
		^c;
	}

	/*
	inactive{ 
		active = false;
		history.addData( [ position, rectangle, direction, predicted, lastTime, active ], overwrite: true );
	}
	*/

	/*
	calcVector{ |last=10|
		var his,id,list;
		his = history.getLast( last ).collect{ |jt| jt.data; };
		id = his.detectIndex{ |jt| jt[2] == false };
		if ( id.notNil, {
			his = his.copyFromStart( id-1 );
		});
		his = his.collect{ |jt| jt[0] };
		if ( his.size > 1, {
			list = Array.fill( his.size-1, 0 );
			his.doAdjacentPairs{ |it,that,i|
				list.put( i, it - that );
			};
			^direction = list.mean;
		});
		^direction = [0,0];
	}
	*/

}