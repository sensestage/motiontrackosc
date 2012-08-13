/* $Id: motiontrackosc.c 54 2009-02-06 14:54:20Z nescivi $ 
 *
 * Copyright (C) 2009, Marije Baalman <nescivi _at_ gmail.com>
 *
 * based on examples for
 * motempl.c from samples/c from the openCV library
 * and liblo by Steve Harris, Uwe Koloska
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

#ifdef _CH_
#pragma package <opencv>
#endif

#ifndef _EiC
// motion templates sample code
#include <cv.h>
#include <highgui.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <lo/lo.h>

#endif

/* struct{ */
/*   var x; */
/*   var y; */
/* } motionpoint; */


int done = 0;
int paused = 0;

lo_address t;
lo_server s;
lo_server_thread st;

// various tracking parameters (in seconds)
double MHI_DURATION = 0.5;
double MAX_TIME_DELTA = 0.25;
double MIN_TIME_DELTA = 0.025;
// number of cyclic frame buffer used for motion detection
// (should, probably, depend on FPS)
const int N = 4;

float prediction = 0.375;
float mot_threshold = 0.05;
int diff_threshold = 30;
int max_area = 100;

char settings [255];

int send_pos = 0;
int send_dir = 0;
int send_rect = 0;
int send_pred = 0;
int hide = 0;
int hidecam = 0;

// ring image buffer
IplImage **buf = 0;
int last = 0;

// temporary images
IplImage *mhi = 0; // MHI
IplImage *orient = 0; // orientation
IplImage *mask = 0; // valid orientation mask
IplImage *segmask = 0; // motion segmentation map
CvMemStorage* storage = 0; // temporary storage

    int line_type = CV_AA; // change it to 8 to see non-antialiased graphics
    CvFont font;
    CvSize text_size;
    CvPoint text_pos;

//CvPoint *track_points;

void error(int num, const char *m, const char *path);
int info_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data);
int pause_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data);
int hide_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data);
int hidecam_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data);
int mhidur_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data);
int maxdelta_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data);
int mindelta_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data);
int prediction_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data);
int threshold_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data);
int diff_threshold_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data);
int max_area_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data);

int subscribe_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data);
int subscribe_pred_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data);
int subscribe_rect_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data);
int subscribe_pos_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data);
int subscribe_dir_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data);

int generic_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data);
int quit_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data);


// parameters:
//  img - input video frame
//  dst - resultant motion picture
//  args - optional parameters
void  update_mhi( IplImage* img, IplImage* dst, int diff_threshold_in )
{
    double timestamp = (double)clock()/CLOCKS_PER_SEC; // get current time in seconds
    CvSize size = cvSize(img->width,img->height); // get current frame size
    int i, idx1 = last, idx2;
    IplImage* silh;
    CvSeq* seq;
    CvRect comp_rect;
    double count;
    double angle;

    CvPoint center, corner1, corner2;
    CvPoint predpos;
    double magnitude;          
    CvScalar color;
    CvScalar color2;

    color2 = CV_RGB(0,255,0);

    int nvalid = -1;

    // allocate images at the beginning or
    // reallocate them if the frame size is changed
    if( !mhi || mhi->width != size.width || mhi->height != size.height ) {
        if( buf == 0 ) {
            buf = (IplImage**)malloc(N*sizeof(buf[0]));
            memset( buf, 0, N*sizeof(buf[0]));
        }
        
        for( i = 0; i < N; i++ ) {
            cvReleaseImage( &buf[i] );
            buf[i] = cvCreateImage( size, IPL_DEPTH_8U, 1 );
            cvZero( buf[i] );
        }
        cvReleaseImage( &mhi );
        cvReleaseImage( &orient );
        cvReleaseImage( &segmask );
        cvReleaseImage( &mask );
        
        mhi = cvCreateImage( size, IPL_DEPTH_32F, 1 );
        cvZero( mhi ); // clear MHI at the beginning
        orient = cvCreateImage( size, IPL_DEPTH_32F, 1 );
        segmask = cvCreateImage( size, IPL_DEPTH_32F, 1 );
        mask = cvCreateImage( size, IPL_DEPTH_8U, 1 );
    }

    cvCvtColor( img, buf[last], CV_BGR2GRAY ); // convert frame to grayscale

    idx2 = (last + 1) % N; // index of (last - (N-1))th frame
    last = idx2;

    silh = buf[idx2];
    cvAbsDiff( buf[idx1], buf[idx2], silh ); // get difference between frames
    
    cvThreshold( silh, silh, diff_threshold_in, 1, CV_THRESH_BINARY ); // and threshold it
    cvUpdateMotionHistory( silh, mhi, timestamp, MHI_DURATION ); // update MHI

    // convert MHI to blue 8u image
    cvCvtScale( mhi, mask, 255./MHI_DURATION,
                (MHI_DURATION - timestamp)*255./MHI_DURATION );
    cvZero( dst );
    cvCvtPlaneToPix( mask, 0, 0, 0, dst );

    // calculate motion gradient orientation and valid orientation mask
    cvCalcMotionGradient( mhi, mask, orient, MAX_TIME_DELTA, MIN_TIME_DELTA, 3 );
    
    if( !storage )
        storage = cvCreateMemStorage(0);
    else
        cvClearMemStorage(storage);
    
    // segment motion: get sequence of motion components
    // segmask is marked motion components map. It is not used further
    seq = cvSegmentMotion( mhi, segmask, storage, timestamp, MAX_TIME_DELTA );

    // iterate through the motion components,
    // One more iteration (i == -1) corresponds to the whole image (global motion)
    for( i = -1; i < seq->total; i++ ) {

        if( i < 0 ) { // case of the whole image
            comp_rect = cvRect( 0, 0, size.width, size.height );
            color = CV_RGB(255,255,255);
            magnitude = 100;
        }
        else { // i-th motion component
            comp_rect = ((CvConnectedComp*)cvGetSeqElem( seq, i ))->rect;
            if( comp_rect.width + comp_rect.height < max_area ) // reject very small components
                continue;
            color = CV_RGB(255,0,0);
            magnitude = 30;
        }

        // select component ROI
        cvSetImageROI( silh, comp_rect );
        cvSetImageROI( mhi, comp_rect );
        cvSetImageROI( orient, comp_rect );
        cvSetImageROI( mask, comp_rect );

        // calculate orientation
        angle = cvCalcGlobalOrientation( orient, mask, mhi, timestamp, MHI_DURATION);
        angle = 360.0 - angle;  // adjust for images with top-left origin

        count = cvNorm( silh, 0, CV_L1, 0 ); // calculate number of points within silhouette ROI

        cvResetImageROI( mhi );
        cvResetImageROI( orient );
        cvResetImageROI( mask );
        cvResetImageROI( silh );

        // check for the case of little motion
        if( count < comp_rect.width*comp_rect.height * mot_threshold )
            continue;

	if ( i != -1 )
	  nvalid = nvalid + 1;

        // draw a clock with arrow indicating the direction
        center = cvPoint( (comp_rect.x + comp_rect.width/2),
                          (comp_rect.y + comp_rect.height/2) );

        corner1 = cvPoint( comp_rect.x, comp_rect.y );
	corner2 = cvPoint( comp_rect.x+ comp_rect.width, comp_rect.y+ comp_rect.height );

	predpos = cvPoint( center.x + cvRound( ( comp_rect.width * prediction * cos(angle*CV_PI/180) ) ),
			   center.y - cvRound( ( comp_rect.height* prediction * sin(angle*CV_PI/180) ) ) );

// 	printf( "predpos (%i,%i), center (%i,%i), rect (%i,%i), angle %f\n", predpos.x, predpos.y, center.x, center.y, comp_rect.width, comp_rect.height, angle );

	if ( !hide ){
		cvRectangle( dst, corner1, corner2, color, 3, CV_AA, 0 );
		cvCircle( dst, center, cvRound(magnitude*1.2), color, 3, CV_AA, 0 );
		cvCircle( dst, predpos, cvRound(magnitude*1.2), color2, 3, CV_AA, 0 );
		cvLine( dst, center, cvPoint( cvRound( center.x + magnitude*cos(angle*CV_PI/180)),
			cvRound( center.y - magnitude*sin(angle*CV_PI/180))), color, 3, CV_AA, 0 );

		sprintf( settings, "%i", nvalid);
		cvInitFont( &font, CV_FONT_HERSHEY_COMPLEX, 0.6, 0.6, 0.0, 1, line_type );
		text_pos = cvPoint( center.x, center.y + 5 );
		cvPutText( dst, settings, text_pos, &font, CV_RGB(255,255,255));
	}

/// here I can send the data over to SC
// i center.x center.y angle comp_rect.width comp_rect.height
// width and height are both an indication of size, and of speed, as the trace is longer when movement is faster
// maybe the position can be updated according to that, based on a previous position

/// NOTE this can be streamlined more, by sending one bundle for all objects detected. However, we have to wait for liblo 0.25 to become widely available!
/// or do some tricksing with putting the message pointers in an array

	lo_bundle b = lo_bundle_new( LO_TT_IMMEDIATE );

	lo_message m4 = lo_message_new();
	if ( send_dir ) {
		lo_message_add_int32( m4, nvalid );
		lo_message_add_float( m4, cos(angle*CV_PI/180) );
		lo_message_add_float( m4, -1*sin(angle*CV_PI/180)  );
		lo_message_add_float( m4, angle );
		lo_bundle_add_message( b, "/motiontrack/direction", m4 );
	}

	lo_message m2 = lo_message_new();
	if ( send_rect ) {
		lo_message_add_int32( m2, nvalid );
		lo_message_add_float( m2, (float) corner1.x / (float) size.width );
		lo_message_add_float( m2, (float) corner1.y / (float) size.height  );
		lo_message_add_float( m2, (float) corner2.x / (float) size.width );
		lo_message_add_float( m2, (float) corner2.y / (float) size.height  );
		lo_bundle_add_message( b, "/motiontrack/rectangle", m2 );
	}

	lo_message m3 = lo_message_new();
	if ( send_pred ){
		lo_message_add_int32( m3, nvalid );
		lo_message_add_float( m3, (float) predpos.x / (float) size.width );
		lo_message_add_float( m3, (float) predpos.y / (float) size.height  );
		lo_bundle_add_message( b, "/motiontrack/predicted", m3 );
	}

	// make sure the position message is last in the bundle:
	lo_message m1 = lo_message_new();
	if ( send_pos ) {
		lo_message_add_int32( m1, nvalid );
		lo_message_add_float( m1, (float) center.x / (float) size.width );
		lo_message_add_float( m1, (float) center.y / (float) size.height  );
		lo_bundle_add_message( b, "/motiontrack/position", m1 );
	}

	if ( lo_send_bundle_from( t, s, b )  == -1 ){
		{ printf("motion update: OSC error %d: %s\n", lo_address_errno(t), lo_address_errstr(t)); }
	}

	lo_bundle_free( b );
	lo_message_free( m1 );
	lo_message_free( m2 );
	lo_message_free( m3 );
	lo_message_free( m4 );

    }

    if ( !hide ){
      sprintf( settings, "valid: %i", nvalid);
      cvInitFont( &font, CV_FONT_HERSHEY_COMPLEX, 0.4, 0.4, 0.0, 1, line_type );
      text_pos = cvPoint( 20, size.height - 20 );
      cvPutText( dst, settings, text_pos, &font, CV_RGB(255,255,255));


      sprintf( settings, "mhi %5.4f, max %5.4f, min %5.4f, pred %5.4f, diff %i, motion %5.4f, area %i", MHI_DURATION, MAX_TIME_DELTA, MIN_TIME_DELTA, prediction,  diff_threshold, mot_threshold, max_area);
      cvInitFont( &font, CV_FONT_HERSHEY_COMPLEX, 0.4, 0.4, 0.0, 1, line_type );
      text_pos = cvPoint( 20, size.height - 10 );
      cvPutText( dst, settings, text_pos, &font, CV_RGB(255,255,255));
   }

    lo_send_from( t, s, LO_TT_IMMEDIATE, "/motiontrack/framedone", "i", nvalid );

}

void print_help(){

printf("\
================= HOT KEYS ================\n \
ESC - quit \n\
p   - pause \n\
o   - hide \n\
l   - help \n\
w,e - mhi duration down,up \n\
s,d - max time delta down,up \n\
x,c - min time delta down,up \n\
t,y - prediction down,up \n\
g,h - difference down,up \n\
b,n - motion threshold down,up \n\
f,r - max area down,up \n\
=========================================== \n\
");
fflush(stdout);
}

int main(int argc, char** argv)
{

    char *port = "57151";
    char *outport = "57120";
    char *ip = "127.0.0.1";

    char *input = "0";

    IplImage* motion = 0;
    CvCapture* capture = 0;

    printf("argv: %s %s %s %s %s %i\n", argv[0], argv[1], argv[2], argv[3], argv[4], argc );

    if ( argc == 1 || (argc >= 2 && strlen(argv[1]) == 1 && isdigit(argv[1][0])) ) {
      /*      if ( argc >= 2 )
	input = argv[1];
      else
      input = "0"*/
       capture = cvCaptureFromCAM( argc >= 2 ? argv[1][0] - '0' : 0 );
// 	  capture = cvCaptureFromCAM( CV_CAP_IEEE1394 );
    } else if( argc >= 2 ) {
      capture = cvCaptureFromFile( argv[1] );
      input = argv[1];
    }

	if ( argc == 5 )
		{
		ip = argv[4];
		port = argv[3];
		outport = argv[2];
		}
	else if ( argc == 4 )
		{
		port = argv[3];
		outport = argv[2];
		}
	else if ( argc == 3 )
		{
		outport = argv[2];
		}

    printf("============================================================================\n");
    printf("motiontrackosc - v0.2 - sends out osc data based on motion tracking data\n");
    printf("                     (c) 2009, Marije Baalman\n");
    printf("                http://www.nescivi.nl/motiontrackosc\n");
    printf("Written using OpenVC and liblo\n");
    printf("This is free software released under the GNU/General Public License\n");
    printf("start with \"motiontrackosc <file or device> <target_port> <recv_port> <target_ip>\" \n");
    printf("============================================================================\n\n");
    printf("video input from: %s\n", input );
    printf("Listening to port: %s\n", port );
    printf("Sending to ip and port: %s %s\n", ip, outport );
    fflush(stdout);

	print_help();

    /* create liblo addres */
    t = lo_address_new(ip, outport); // change later to use other host

    lo_server_thread st = lo_server_thread_new(port, error);

    lo_server_thread_add_method(st, "/motiontrack/mhidur", "f", mhidur_handler, NULL);
    lo_server_thread_add_method(st, "/motiontrack/maxdelta", "f", maxdelta_handler, NULL);
    lo_server_thread_add_method(st, "/motiontrack/mindelta", "f", mindelta_handler, NULL);
    lo_server_thread_add_method(st, "/motiontrack/prediction", "f", prediction_handler, NULL);
    lo_server_thread_add_method(st, "/motiontrack/threshold", "f", threshold_handler, NULL);
    lo_server_thread_add_method(st, "/motiontrack/difference", "i", diff_threshold_handler, NULL);
	lo_server_thread_add_method(st, "/motiontrack/maxarea", "i", max_area_handler, NULL);
    lo_server_thread_add_method(st, "/motiontrack/info", "", info_handler, NULL);

    lo_server_thread_add_method(st, "/motiontrack/subscribe/position", "i", subscribe_pos_handler, NULL);
    lo_server_thread_add_method(st, "/motiontrack/subscribe/direction", "i", subscribe_dir_handler, NULL);
    lo_server_thread_add_method(st, "/motiontrack/subscribe/rectangle", "i", subscribe_rect_handler, NULL);
    lo_server_thread_add_method(st, "/motiontrack/subscribe/prediction", "i", subscribe_pred_handler, NULL);
    lo_server_thread_add_method(st, "/motiontrack/subscribe", "i", subscribe_handler, NULL);

    lo_server_thread_add_method(st, "/motiontrack/hide", "i", hide_handler, NULL);

    lo_server_thread_add_method(st, "/motiontrack/hidecamera", "i", hidecam_handler, NULL);

    lo_server_thread_add_method(st, "/motiontrack/pause", "i", pause_handler, NULL);
    lo_server_thread_add_method(st, "/motiontrack/quit", "", quit_handler, NULL);
    lo_server_thread_add_method(st, NULL, NULL, generic_handler, NULL);

    lo_server_thread_start(st);
 
    lo_server s = lo_server_thread_get_server( st );


    lo_send_from( t, s, LO_TT_IMMEDIATE, "/motiontrackosc/started", "" );

    if( capture )
    {

	printf( "capturing at framerate %f\n", cvGetCaptureProperty( capture, CV_CAP_PROP_FPS ) );

        cvNamedWindow( "CameraImage", 1 );

        cvNamedWindow( "MotionTrackOSC", 1 );

	//	track_points = (CvPoint *) malloc( sizeOf( CvPoint ) * 10 );
        
        while( !done )
        {
            IplImage* image;
//             if( !cvGrabFrame( capture ))
//                 break;
//               image = cvRetrieveFrame( capture );
            image = cvQueryFrame( capture );

            if( image )
            {
	      if ( !hidecam )
		cvShowImage( "CameraImage", image );
	      if( !motion )
                {
                    motion = cvCreateImage( cvSize(image->width,image->height), 8, 3 );
                    cvZero( motion );
                    motion->origin = image->origin;
                }
            }

	    if ( !paused ){
	      lo_send_from( t, s, LO_TT_IMMEDIATE, "/motiontrack/frameupdate", "" );

	      update_mhi( image, motion, diff_threshold );

	      if ( !hidecam )
		cvShowImage( "MotionTrackOSC", motion );

	    }

	    int c = cvWaitKey(40);
		switch( (char) c )
		{
		case '\x1b':
			printf("Exiting ...\n");
			goto exit_main;
		case 'p':
			paused = !paused;

			break;
		case 'o':
			hide = !hide;
			break;
		case 'i':
			hidecam = !hidecam;
			break;
		case 'l':
			print_help();
			break;
		case 'w':
			MHI_DURATION = MHI_DURATION - 0.05;
			if ( MHI_DURATION < 0.005 ) 
				MHI_DURATION = 0.005;
			break;
		case 'e':
			MHI_DURATION = MHI_DURATION + 0.05;
			break;
		case 's':
			MAX_TIME_DELTA = MAX_TIME_DELTA - 0.025;
			if ( MAX_TIME_DELTA < 0.005 ) 
				MAX_TIME_DELTA = 0.005;
			break;
		case 'd':
			MAX_TIME_DELTA = MAX_TIME_DELTA + 0.025;
			break;
		case 'x':
			MIN_TIME_DELTA = MIN_TIME_DELTA - 0.0025;
			if ( MIN_TIME_DELTA < 0.0005 ) 
				MIN_TIME_DELTA = 0.0005;
			break;
		case 'c':
			MIN_TIME_DELTA = MIN_TIME_DELTA + 0.0025;
			break;
		case 't':
			prediction = prediction - 0.005;
			if ( prediction < 0.005 ) 
				prediction = 0.005;
			break;
		case 'y':
			prediction = prediction + 0.005;
			if ( prediction > 2. ) 
				prediction = 2.;
			break;
		case 'g':
			diff_threshold = diff_threshold - 1;
			if ( diff_threshold < 1 ) 
				diff_threshold = 1;
			break;
		case 'h':
			diff_threshold = diff_threshold + 1;
			break;
		case 'f':
			max_area = max_area - 1;
			if ( max_area < 1 ) 
				max_area = 1;
			break;
		case 'r':
			max_area = max_area + 1;
			break;
		case 'b':
			mot_threshold = mot_threshold - 0.005;
			if ( mot_threshold < 0.005 ) 
				mot_threshold = 0.005;
			break;
		case 'n':
			mot_threshold = mot_threshold + 0.005;
			if ( mot_threshold > 1. ) 
				mot_threshold = 1.;
			break;
		}
// 	    if ( c >= 0 )
// 		{
// 		printf( "paused %i, DUR %f, MAXDT %f, MINDT %f\n", paused, MHI_DURATION, MAX_TIME_DELTA, MIN_TIME_DELTA );
// 		}

	if ( !hide && paused ){
		cvInitFont( &font, CV_FONT_HERSHEY_COMPLEX, 3, 3, 0.0, 3, line_type );
		text_pos = cvPoint( image->width / 8, image->height /2 );
		cvPutText( motion, "PAUSED", text_pos, &font, CV_RGB(255,255,0));
		cvShowImage( "MotionTrackOSC", motion );
		}
	else if ( hide && paused ){
		cvInitFont( &font, CV_FONT_HERSHEY_COMPLEX, 3, 3, 0.0, 3, line_type );
		text_pos = cvPoint( image->width / 8, image->height /2 );
		cvPutText( motion, "PAUSED", text_pos, &font, CV_RGB(0,0,0));
		cvShowImage( "MotionTrackOSC", motion );
		}

        }

exit_main:

        cvReleaseCapture( &capture );
        cvDestroyWindow( "MotionTrackOSC" );
        cvDestroyWindow( "CameraImage" );
    }

    //    free( track_points );

    lo_send_from( t, s, LO_TT_IMMEDIATE, "/motiontrackosc/quit", "s", "nothing more to do, quitting" );
    lo_server_thread_free( st );
    lo_address_free( t );

    return 0;
}

void error(int num, const char *msg, const char *path)
{
     printf("liblo server error %d in path %s: %s\n", num, path, msg);
     fflush(stdout);
}

int subscribe_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
    send_pos = argv[0]->i;
    send_dir = argv[0]->i;
    send_rect = argv[0]->i;
    send_pred = argv[0]->i;
    return 0;
}

int subscribe_pos_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
    send_pos = argv[0]->i;
    return 0;
}

int subscribe_dir_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
    send_dir = argv[0]->i;
    return 0;
}

int subscribe_rect_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
    send_rect = argv[0]->i;
    return 0;
}

int subscribe_pred_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
    send_pred = argv[0]->i;
    return 0;
}

int mhidur_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
    MHI_DURATION = argv[0]->f;
    lo_send_from( t, s, LO_TT_IMMEDIATE, "/motiontrack/mhidur", "f", MHI_DURATION );
    return 0;
}

int prediction_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
    prediction = argv[0]->f;
    lo_send_from( t, s, LO_TT_IMMEDIATE, "/motiontrack/prediction", "f", prediction );
    return 0;
}

int threshold_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
    mot_threshold = argv[0]->f;
    lo_send_from( t, s, LO_TT_IMMEDIATE, "/motiontrack/threshold", "f", mot_threshold );
    return 0;
}

int diff_threshold_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
    diff_threshold = argv[0]->i;
    lo_send_from( t, s, LO_TT_IMMEDIATE, "/motiontrack/difference", "i", diff_threshold );
    return 0;
}

int max_area_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
    max_area = argv[0]->i;
    lo_send_from( t, s, LO_TT_IMMEDIATE, "/motiontrack/maxarea", "i", diff_threshold );
    return 0;
}

int maxdelta_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
    MAX_TIME_DELTA = argv[0]->f;
    lo_send_from( t, s, LO_TT_IMMEDIATE, "/motiontrack/maxdelta", "f", MAX_TIME_DELTA );
    return 0;
}

int mindelta_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
    MIN_TIME_DELTA = argv[0]->f;
    lo_send_from( t, s, LO_TT_IMMEDIATE, "/motiontrack/mindelta", "f", MIN_TIME_DELTA );
    return 0;
}

int pause_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
    paused = argv[0]->i;

    return 0;
}

int hide_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
    hide = argv[0]->i;
    return 0;
}

int hidecam_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
    hidecam = argv[0]->i;
    return 0;
}

int info_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{

	lo_bundle b = lo_bundle_new( LO_TT_IMMEDIATE );

	lo_message m1 = lo_message_new();
	lo_message_add_float( m1, MHI_DURATION );
	lo_bundle_add_message( b, "/motiontrack/mhidur", m1 );

	lo_message m2 = lo_message_new();
	lo_message_add_float( m2, MIN_TIME_DELTA );
	lo_bundle_add_message( b, "/motiontrack/mindelta", m2 );

	lo_message m3 = lo_message_new();
	lo_message_add_float( m3, MAX_TIME_DELTA );
	lo_bundle_add_message( b, "/motiontrack/maxdelta", m3 );

	lo_message m4 = lo_message_new();
	lo_message_add_float( m4, prediction );
	lo_bundle_add_message( b, "/motiontrack/prediction", m4 );

	lo_message m5 = lo_message_new();
	lo_message_add_float( m5,mot_threshold );
	lo_bundle_add_message( b, "/motiontrack/threshold", m5 );

	lo_message m6 = lo_message_new();
	lo_message_add_float( m6, diff_threshold );
	lo_bundle_add_message( b, "/motiontrack/difference", m6 );

	lo_message m7 = lo_message_new();
	lo_message_add_float( m7, max_area );
	lo_bundle_add_message( b, "/motiontrack/maxarea", m7 );

	if ( lo_send_bundle_from( t, s, b )  == -1 ){
		{ printf("motion update: OSC error %d: %s\n", lo_address_errno(t), lo_address_errstr(t)); }
	}

	lo_bundle_free( b );
	lo_message_free( m1 );
	lo_message_free( m2 );
	lo_message_free( m3 );
	lo_message_free( m4 );
	lo_message_free( m5 );
	lo_message_free( m6 );
	lo_message_free( m7 );

    fflush(stdout);

    return 0;
}


/* catch any incoming messages and display them. returning 1 means that the
 * message has not been fully handled and the server should try other methods */
int generic_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data)
{
    int i;

    printf("path: <%s>\n", path);
    for (i=0; i<argc; i++) {
	printf("arg %d '%c' ", i, types[i]);
	lo_arg_pp(types[i], argv[i]);
	printf("\n");
    }
    printf("\n");
    fflush(stdout);

    return 1;
}

int quit_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
    done = 1;
    printf("motiontrackosc: allright, that's it, I quit\n");
    fflush(stdout);

    return 0;
}

                                
#ifdef _EiC
main(1,"motiontrackosc.c");
#endif
