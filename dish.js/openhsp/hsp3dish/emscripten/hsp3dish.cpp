/*--------------------------------------------------------
	HSP3dish main (emscripten & SDL)
  --------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#if defined( __GNUC__ )
#include <ctype.h>
#endif

#ifdef HSPDISHGP
#include "gamehsp.h"
#endif

#include "hsp3dish.h"
#include "../../hsp3/hsp3config.h"
#include "../../hsp3/strbuf.h"
#include "../../hsp3/hsp3.h"
#include "../hsp3gr.h"
#include "../supio.h"
#include "../hgio.h"
#include "../sysreq.h"
//#include "../hsp3ext.h"
#include "../../hsp3/strnote.h"
#include "appengine.h"

#include <GL/gl.h>
//#include <GL/glut.h>

#include "SDL/SDL.h"
#include "SDL/SDL_image.h"
#include "SDL/SDL_opengl.h"

#include <emscripten.h>
#include <emscripten/html5.h>

//#define USE_OBAQ

#ifdef USE_OBAQ
#include "../obaq/hsp3dw.h"
#endif

//typedef BOOL (CALLBACK *HSP3DBGFUNC)(HSP3DEBUG *,int,int,int);

/*----------------------------------------------------------*/

static Hsp3 *hsp = NULL;
static HSPCTX *ctx;
static HSPEXINFO *exinfo;								// Info for Plugins

static char fpas[]={ 'H'-48,'S'-48,'P'-48,'H'-48,
					 'E'-48,'D'-48,'~'-48,'~'-48 };
static char optmes[] = "HSPHED~~\0_1_________2_________3______";

static int hsp_wx, hsp_wy, hsp_wd, hsp_ss;
static int drawflag;
static int hsp_fps;
static int hsp_limit_step_per_frame;
static std::string syncdir;
static bool fs_initialized = false;
static int capkey;

//static	HWND m_hWnd;

#ifndef HSPDEBUG
static int hsp_sscnt, hsp_ssx, hsp_ssy;
#endif

static bool keys[SDLK_LAST];

#ifdef HSPDISHGP
gamehsp *game;
gameplay::Platform *platform;

//-------------------------------------------------------------
//		gameplay Log
//-------------------------------------------------------------

static std::string gplog;

extern "C" {
	static void logfunc( gameplay::Logger::Level level, const char *msg )
	{
		if (( level == gameplay::Logger::LEVEL_ERROR )||( level == gameplay::Logger::LEVEL_WARN )) printf( "#%s\n",msg );
		gplog += msg;
	}
}

#endif

/*----------------------------------------------------------*/

bool get_key_state(int sym)
{
	return keys[sym];
}

void getCanvasRate( double &rx, double &ry )
{
	double cx, cy;
	int sx, sy;
	emscripten_get_element_css_size( 0, &cx, &cy );
	hgio_getSize( sx, sy );
	rx = (( cx < 1.0 ) || ( sx < 1 )) ? 1.0 : cx / sx;
	ry = (( cy < 1.0 ) || ( sy < 1 )) ? 1.0 : cy / sy;
}

void focusCanvas()
{
	if ( capkey > 0 ) EM_ASM({ Module.canvas.focus(); });
}

bool hasFocusCanvas()
{
	return (bool)EM_ASM_INT_V({ return document.activeElement == Module.canvas; });
}

EM_BOOL key_callback( int eventType, const EmscriptenKeyboardEvent *e, void *userData )
{
	// 'keyCode' attribute is deprecated.
	keys[e->keyCode] = (eventType == EMSCRIPTEN_EVENT_KEYDOWN);
	if ( capkey > 0 && hasFocusCanvas() ) {
		// フォーカスがある場合は preventDefault
		return 1;
	} else {
		return 0;
	}
}

EM_BOOL mouse_callback( int eventType, const EmscriptenMouseEvent *e, void *userData )
{
	double crx, cry;
	getCanvasRate( crx, cry );
	hgio_touch( e->canvasX / crx, e->canvasY / cry, e->buttons );
	if ( eventType == EMSCRIPTEN_EVENT_MOUSEDOWN ) {
		focusCanvas();
	}
	return 0;
}

EM_BOOL wheel_callback( int eventType, const EmscriptenWheelEvent *e, void *userData )
{
	if ( exinfo != NULL ) {
		Bmscr *bm = (Bmscr *)exinfo->HspFunc_getbmscr(0);
		bm->savepos[BMSCR_SAVEPOS_MOSUEW] = (int)(e->deltaY * 100);
	}
	return 0;
}

EM_BOOL touch_callback( int eventType, const EmscriptenTouchEvent *e, void *userData )
{
	double crx, cry;
	getCanvasRate( crx, cry );
	for( int i = 0; i < e->numTouches; i++ ) {
		const EmscriptenTouchPoint *t = &e->touches[i];
		bool touch = ( eventType == EMSCRIPTEN_EVENT_TOUCHSTART ) || ( eventType == EMSCRIPTEN_EVENT_TOUCHMOVE );
		hgio_mtouchid( t->identifier, t->canvasX / crx, t->canvasY / cry, touch, i );
	}
	if ( eventType == EMSCRIPTEN_EVENT_TOUCHSTART ) {
		focusCanvas();
	}
	return 0;
}

EM_BOOL deviceorientation_callback( int eventType, const EmscriptenDeviceOrientationEvent *e, void *userData )
{
	hgio_setinfo( GINFO_EXINFO_GYRO_Z, e->alpha );
	hgio_setinfo( GINFO_EXINFO_GYRO_X, e->beta );
	hgio_setinfo( GINFO_EXINFO_GYRO_Y, e->gamma );
	return 0;
}

EM_BOOL devicemotion_callback( int eventType, const EmscriptenDeviceMotionEvent *e, void *userData )
{
	hgio_setinfo( GINFO_EXINFO_ACCEL_X, e->accelerationX );
	hgio_setinfo( GINFO_EXINFO_ACCEL_Y, e->accelerationY );
	hgio_setinfo( GINFO_EXINFO_ACCEL_Z, e->accelerationZ );
	return 0;
}

EM_BOOL fullscreenchange_callback( int eventType, const EmscriptenFullscreenChangeEvent *e, void *userData )
{
	focusCanvas();
	return 0;
}

#define SET_EVENT(f, r) ret = (f); if ( ret < 0 ) Alertf( "event setting error: %s (%d)", #r, ret )

void initHtmlEvent()
{
	EMSCRIPTEN_RESULT ret;
	const char *element = "#canvas";
	
	if ( capkey == 2 ) {
		SET_EVENT( emscripten_set_keydown_callback( element, 0, true, key_callback ), key down );
	} else {
		SET_EVENT( emscripten_set_keydown_callback( 0, 0, true, key_callback ), key down );
	}
	SET_EVENT( emscripten_set_keyup_callback( 0, 0, true, key_callback ), key up );

	SET_EVENT( emscripten_set_mousedown_callback( element, 0, true, mouse_callback ), mouse down );
	SET_EVENT( emscripten_set_mouseup_callback( 0, 0, true, mouse_callback ), mouse up );
	SET_EVENT( emscripten_set_mousemove_callback( element, 0, true, mouse_callback ), mouse move );

	SET_EVENT( emscripten_set_wheel_callback( element, 0, true, wheel_callback ), wheel );

	SET_EVENT( emscripten_set_touchstart_callback( element, 0, true, touch_callback ), touch start );
	SET_EVENT( emscripten_set_touchend_callback( 0, 0, true, touch_callback ), touch end );
	SET_EVENT( emscripten_set_touchmove_callback( element, 0, true, touch_callback ), touch move );
	SET_EVENT( emscripten_set_touchcancel_callback( 0, 0, true, touch_callback ), touch cancel );

	SET_EVENT( emscripten_set_deviceorientation_callback( 0, true, deviceorientation_callback ), device orientation );
	SET_EVENT( emscripten_set_devicemotion_callback( 0, true, devicemotion_callback ), device motion );
	
	if ( capkey > 0 ) {
		SET_EVENT( emscripten_set_fullscreenchange_callback( 0, 0, true, fullscreenchange_callback ), fullscreen change );
	}
}

static void hsp3dish_initwindow( engine* engine, int sx, int sy, char *windowtitle )
{
	printf("INIT %dx%d %s\n", sx,sy,windowtitle);
	// glutInit(NULL, NULL);
	// glutInitWindowSize(sx, sy);

	// glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);

	// glutCreateWindow(windowtitle);

	if ( capkey > 0 ) {
		// SDL のキーボードキャプチャを無効化して html の input/textare 要素へ入力できるようにする
		// ただし backspace/tab などの操作も有効になる
		EM_ASM({
			Module.canvas.setAttribute('tabindex', 0);
			Module.canvas.focus();
			Module['doNotCaptureKeyboard'] = true;
		});
	}

	SDL_Surface *screen;

	// Slightly different SDL initialization
	if ( SDL_Init(SDL_INIT_VIDEO) != 0 ) {
		printf("Unable to initialize SDL: %s\n", SDL_GetError());
		return;
	}

	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

	screen = SDL_SetVideoMode( sx, sy, 16, SDL_OPENGL );
	if ( !screen ) {
		printf("Unable to set video mode: %s\n", SDL_GetError());
		return;
	}

	// 描画APIに渡す
	hgio_init( 0, sx, sy, engine );
	hgio_clsmode( CLSMODE_SOLID, 0xffffff, 0 );

	// マルチタッチ初期化
	//MTouchInit( m_hWnd );
}


void hsp3dish_dialog( char *mes )
{
	//MessageBox( NULL, mes, "Error",MB_ICONEXCLAMATION | MB_OK );
	printf( "%s\n", mes );
}


#ifdef HSPDEBUG
char *hsp3dish_debug( int type )
{
	//		デバッグ情報取得
	//
	char *p;
	p = code_inidbg();

	switch( type ) {
	case DEBUGINFO_GENERAL:
//		hsp3gr_dbg_gui();
		code_dbg_global();
		break;
	case DEBUGINFO_VARNAME:
		break;
	case DEBUGINFO_INTINFO:
		break;
	case DEBUGINFO_GRINFO:
		break;
	case DEBUGINFO_MMINFO:
		break;
	}
	return p;
}
#endif


void hsp3dish_drawon( void )
{
	//		描画開始指示
	//
	if ( drawflag == 0 ) {
		hgio_render_start();
		drawflag = 1;
	}
}


void hsp3dish_drawoff( void )
{
	//		描画終了指示
	//
	if ( drawflag ) {
		hgio_render_end();
		drawflag = 0;
	}
}


int hsp3dish_debugopen( void )
{
	return 0;
}

int hsp3dish_wait( int tick )
{
	//		時間待ち(wait)
	//		(awaitに変換します)
	//
	if ( ctx->waitcount <= 0 ) {
		ctx->runmode = RUNMODE_RUN;
		return RUNMODE_RUN;
	}
	ctx->waittick = tick + ( ctx->waitcount * 10 );
	return RUNMODE_AWAIT;
}


int hsp3dish_await( int tick )
{
	//		時間待ち(await)
	//
	if ( ctx->waittick < 0 ) {
		if ( ctx->lasttick == 0 ) ctx->lasttick = tick;
		ctx->waittick = ctx->lasttick + ctx->waitcount;
	}
	if ( tick >= ctx->waittick ) {
		ctx->lasttick = tick;
		ctx->runmode = RUNMODE_RUN;
		return RUNMODE_RUN;
	}
	return RUNMODE_AWAIT;
}


void hsp3dish_msgfunc( HSPCTX *hspctx )
{
	int tick;

	while(1) {
		// logmes なら先に処理する
		if ( hspctx->runmode == RUNMODE_LOGMES ) {
			hspctx->runmode = RUNMODE_RUN;
			return;
		}

		switch( hspctx->runmode ) {
		case RUNMODE_STOP:
			// while(1) {
			// 	GetMessage( &msg, NULL, 0, 0 );
			// 	if ( msg.message == WM_QUIT ) throw HSPERR_NONE;
			// 	hsp3dish_dispatch( &msg );
			// 	if ( hspctx->runmode != RUNMODE_STOP ) break;
			// }
			// MsgWaitForMultipleObjects(0, NULL, FALSE, 1000, QS_ALLINPUT );
			return;
		case RUNMODE_WAIT:
			tick = hgio_gettick();
			hspctx->runmode = code_exec_wait( tick );
		case RUNMODE_AWAIT:
			tick = hgio_gettick();
			if ( code_exec_await( tick ) != RUNMODE_RUN ) {
				//MsgWaitForMultipleObjects(0, NULL, FALSE, hspctx->waittick - tick, QS_ALLINPUT );
				//printf("AWAIT WAIT %d < %d\n", tick, ctx->waittick);
			} else {
				//printf("AWAIT RUN %d < %d\n", tick, ctx->waittick);
				ctx->runmode = RUNMODE_AWAIT;
#ifndef HSPDEBUG
				if ( ctx->hspstat & HSPSTAT_SSAVER ) {
					if ( hsp_sscnt ) hsp_sscnt--;
				}
#endif
			}
				return;
			break;
//		case RUNMODE_END:
//			throw HSPERR_NONE;
		case RUNMODE_RETURN:
			throw HSPERR_RETURN_WITHOUT_GOSUB;
		case RUNMODE_INTJUMP:
			throw HSPERR_INTJUMP;
		case RUNMODE_ASSERT:
			hspctx->runmode = RUNMODE_STOP;
#ifdef HSPDEBUG
			hsp3dish_debugopen();
#endif
			break;
	//	case RUNMODE_LOGMES:
		default:
			return;
		}
	}
}


/*----------------------------------------------------------*/
//		デバイスコントロール関連
/*----------------------------------------------------------*/
static HSP3DEVINFO *mem_devinfo;
static int devinfoi_res;

static int hsp3dish_devprm( char *name, char *value )
{
	if ( strcmp( name, "urlquery" )==0 ) {
		EM_ASM_({
			history.replaceState('', null, '?' + Pointer_stringify($0))
		}, value);
		return 0;
	}
	char *tp = strtok( name, "." );
	if ( strcmp( tp, "value" )==0 ) {
		tp = strtok( NULL, "." );
		if ( tp != NULL ) {
			EM_ASM_({
				document.getElementById(Pointer_stringify($0)).value = Pointer_stringify($1)
			}, tp, value);
		}
	}
	return -1;
}

static int hsp3dish_devcontrol( char *cmd, int p1, int p2, int p3 )
{
	if ( strcmp( cmd, "syncfs" )==0 ) {
		if (syncdir.size() > 0) {
			// IDBに保存
			EM_ASM_({
				var dir = Pointer_stringify($0);
				FS.syncfs(function (err) {
					console.log("syncfs", err);
					});
				}, syncdir.c_str());
			return 0;
		}
	}
	return -1;
}

static int *hsp3dish_devinfoi( char *name, int *size )
{
	if ( strcmp( name, "focus" )==0 ) {
		if ( capkey == 0 ) {
			devinfoi_res = 1;
		} else {
			devinfoi_res = hasFocusCanvas();
		}
		*size = 1;
		return &devinfoi_res;
	}
	*size = -1;
	return NULL;
}

static char *hsp3dish_devinfo( char *name )
{
	if ( strcmp( name, "name" )==0 ) {
		return mem_devinfo->devname;
	}
	if ( strcmp( name, "error" )==0 ) {
		return mem_devinfo->error;
	}
	if ( strcmp( name, "urlquery" )==0 ) {
		return (char*)EM_ASM_INT_V({
			return (allocate(intArrayFromString(window.location.search.substring(1)), 'i8', ALLOC_STACK));
		});
	}
	char *tp = strtok( name, "." );
	if ( strcmp( tp, "value" )==0 ) {
		tp = strtok( NULL, "." );
		if ( tp != NULL ) {
			return (char*)EM_ASM_INT({
				return (allocate(intArrayFromString(document.getElementById(Pointer_stringify($0)).value), 'i8', ALLOC_STACK));
			}, tp);
		}
	}
	return NULL;
}

static void hsp3dish_setdevinfo( HSP3DEVINFO *devinfo )
{
	//		Initalize DEVINFO
	mem_devinfo = devinfo;
	devinfo->devname = "emscripten";
	devinfo->error = "";
	devinfo->devprm = hsp3dish_devprm;
	devinfo->devcontrol = hsp3dish_devcontrol;
	devinfo->devinfo = hsp3dish_devinfo;
	devinfo->devinfoi = hsp3dish_devinfoi;
}

/*----------------------------------------------------------*/

int hsp3dish_init( char *startfile )
{
	//		システム関連の初期化
	//		( mode:0=debug/1=release )
	//
	int a,orgexe, mode;
	int hsp_sum, hsp_dec;
	char a1;
#ifdef HSPDEBUG
	int i;
#endif
	InitSysReq();

#ifdef HSPDISHGP
	SetSysReq( SYSREQ_MAXMATERIAL, 64 );            // マテリアルのデフォルト値

	game = NULL;
	platform = NULL;
#endif

	//		HSP関連の初期化
	//
	hsp = new Hsp3();

	if ( startfile != NULL ) {
		hsp->SetFileName( startfile );
	}

	//		実行ファイルかデバッグ中かを調べる
	//
	mode = 0;
	orgexe=0;
#ifdef HSPEMSCRIPTEN
	hsp_wx = 960;
	hsp_wy = 640;
#else
	hsp_wx = 320;
	hsp_wy = 480;
#endif
//	hsp_wx = 640;
//	hsp_wy = 480;
	hsp_wd = 0;
	hsp_ss = 0;

	for( a=0 ; a<8; a++) {
		a1=optmes[a]-48;if (a1==fpas[a]) orgexe++;
	}
	if ( orgexe == 0 ) {
		mode = atoi(optmes+9) + 0x10000;
		a1=*(optmes+17);
		if ( a1 == 's' ) hsp_ss = HSPSTAT_SSAVER;
		hsp_wx=*(short *)(optmes+20);
		hsp_wy=*(short *)(optmes+23);
		hsp_wd=( *(short *)(optmes+26) );
		hsp_sum=*(unsigned short *)(optmes+29);
		hsp_dec=*(int *)(optmes+32);
		hsp->SetPackValue( hsp_sum, hsp_dec );
	}

	char *env_wx = getenv( "HSP_WX" );
	if ( env_wx ) {
		int v = atoi( env_wx );
		if ( v > 0 ) {
			hsp_wx = v;
		}
	}

	char *env_wy = getenv( "HSP_WY" );
	if ( env_wy ) {
		int v = atoi( env_wy );
		if ( v > 0 ) {
			hsp_wy = v;
		}
	}

	float sx = 0, sy = 0;

	char *env_sx = getenv( "HSP_SX" );
	if ( env_sx ) {
		sx = atof( env_sx );
	}

	char *env_sy = getenv( "HSP_SY" );
	if ( env_sy ) {
		sy = atof( env_sy );
	}

	if ( sx > 0 && sy > 0 ) {
		//OK
	} else {
		sx = hsp_wx;
		sy = hsp_wy;
	}

	char *env_autoscale = getenv( "HSP_AUTOSCALE" );
	int autoscale = 0;
	if ( env_autoscale ) {
		autoscale = atoi( env_autoscale );
	}

	char *env_fps = getenv( "HSP_FPS" );
	hsp_fps = 0;
	if ( env_fps ) {
		hsp_fps = atoi( env_fps );
	}

	char *env_step = getenv( "HSP_LIMIT_STEP" );
	hsp_limit_step_per_frame = 5000;
	if ( env_step ) {
		hsp_limit_step_per_frame = atoi( env_step );
	}

	char *env_cap = getenv( "HSP_CAPTURE_KEY" );
	capkey = 0;
	if ( env_cap ) {
		capkey = atoi( env_cap );
	}

	// printf("Screen %f %f\n", sx, sy);

	char *env_syncdir = getenv( "HSP_SYNC_DIR" );
	if ( env_syncdir ) {
		syncdir = env_syncdir;
	}

	if ( hsp->Reset( mode ) ) {
		hsp3dish_dialog( "Startup failed." );
		return 1;
	}

	for (int i = 0; i < SDLK_LAST; i++) {
		keys[i] = false;
	}

	ctx = &hsp->hspctx;

	//		Register Type
	//
	drawflag = 0;
	ctx->msgfunc = hsp3dish_msgfunc;

	//		Initalize Window
	//
	hsp3dish_initwindow( NULL, sx, sy, "HSPDish ver" hspver " - " modname);

	if ( sx != hsp_wx || sy != hsp_wy ) {
#ifndef HSPDISHGP
		hgio_view( hsp_wx, hsp_wy );
		hgio_size( sx, sy );
		hgio_autoscale( autoscale );
#endif
	}

//	hsp3typeinit_dllcmd( code_gettypeinfo( TYPE_DLLFUNC ) );
//	hsp3typeinit_dllctrl( code_gettypeinfo( TYPE_DLLCTRL ) );

#ifdef HSPDISHGP
	//		Initalize gameplay
	//
	game = new gamehsp;

	gameplay::Logger::set(gameplay::Logger::LEVEL_INFO, logfunc);
	gameplay::Logger::set(gameplay::Logger::LEVEL_WARN, logfunc);
	gameplay::Logger::set(gameplay::Logger::LEVEL_ERROR, logfunc);

	//	platform = gameplay::Platform::create( game, NULL, hsp_wx, hsp_wy, false );
	platform = gameplay::Platform::create( game, NULL, hsp_wx, hsp_wy, false );
	if ( platform == NULL ) {
		hsp3dish_dialog( (char *)gplog.c_str() );
		hsp3dish_dialog( "OpenGL initalize failed." );
		return 1;
	}
	platform->enterMessagePump();
	game->frame();
#endif

	//		Initalize GUI System
	//
	hsp3typeinit_extcmd( code_gettypeinfo( TYPE_EXTCMD ) );
	hsp3typeinit_extfunc( code_gettypeinfo( TYPE_EXTSYSVAR ) );

	exinfo = ctx->exinfo2;

#ifdef USE_OBAQ
	HSP3TYPEINFO *tinfo = code_gettypeinfo( TYPE_USERDEF );
	tinfo->hspctx = ctx;
	tinfo->hspexinfo = exinfo;
	hsp3typeinit_dw_extcmd( tinfo );
	//hsp3typeinit_dw_extfunc( code_gettypeinfo( TYPE_USERDEF+1 ) );
#endif


	//		Initalize DEVINFO
	HSP3DEVINFO *devinfo;
	devinfo = hsp3extcmd_getdevinfo();
	hsp3dish_setdevinfo( devinfo );

	initHtmlEvent();

	return 0;
}


static void hsp3dish_bye( void )
{
	//		Window関連の解放
	//
	hsp3dish_drawoff();

	//		タイマーの開放
	//
	emscripten_cancel_main_loop();

#ifdef HSPDISHGP
	//		gameplay関連の解放
	//
	if ( platform != NULL ) {
		platform->shutdownInternal();
		delete platform;
	}
	if ( game != NULL ) {
		delete game;
	}
#endif

	//		HSP関連の解放
	//
	if ( hsp != NULL ) { delete hsp; hsp = NULL; }

	// if ( m_hWnd != NULL ) {
	// 	hgio_term();
	// 	DestroyWindow( m_hWnd );
	// 	m_hWnd = NULL;
	// }
}


void hsp3dish_error( void )
{
	char errmsg[1024];
	char *msg;
	char *fname;
	HSPERROR err;
	int ln;
	err = code_geterror();
	ln = code_getdebug_line();
	msg = hspd_geterror(err);
	fname = code_getdebug_name();

	if ( ln < 0 ) {
		sprintf( errmsg, "#Error %d --> %s\n",(int)err,msg );
		fname = NULL;
	} else {
		sprintf( errmsg, "#Error %d in line %d (%s)\n-->%s\n",(int)err, ln, fname, msg );
	}
	hsp3dish_debugopen();
	hsp3dish_dialog( errmsg );
}


char *hsp3dish_getlog(void)
{
#ifdef HSPDISHGP
	return (char *)gplog.c_str();
#else
	return "";
#endif
}


extern int code_execcmd_one( int& prev );

void hsp3dish_exec_one( void )
{
	if (!fs_initialized) {
		printf("Sync\n");
		return;
	}
	// hgio_test();
	// return;
	int tick;
	switch( ctx->runmode ) {
	case RUNMODE_WAIT:
		tick = hgio_gettick();
		if ( code_exec_wait( tick ) != RUNMODE_RUN ) {
			return;
		}
		break;
	case RUNMODE_AWAIT:
		tick = hgio_gettick();
		if ( code_exec_await( tick ) != RUNMODE_RUN ) {
			//printf("AWAIT %d < %d\n", tick, ctx->waittick);
			return;
		}
		break;
	}
	//		実行の開始
	//
	static int code_execcmd_state = 0;
	int runmode;
	bool stop = false;
	int i;
	for (i = 0; !stop && i < hsp_limit_step_per_frame; i++) {
	//for (int i = 0; !stop; i++) {
		runmode = code_execcmd_one(code_execcmd_state);
		switch ( ctx->runmode ){
		case RUNMODE_RUN:
			break;
		case RUNMODE_WAIT:
		case RUNMODE_AWAIT:
			return;
		case RUNMODE_END:
		case RUNMODE_ERROR:
			//printf("BREAK #%d %d %d\n", i, runmode, ctx->runmode);
			stop = true;
			break;
		}
	}
	if (i == hsp_limit_step_per_frame) {
		fprintf(stderr, "OVER HSP_LIMIT_STEP %d\n", hsp_limit_step_per_frame);
	}
	//exit(-1);
	//printf("RUN %d %d\n", runmode, ctx->runmode);
	if ( runmode == RUNMODE_RUN ) {
		return;
	}
	if ( runmode == RUNMODE_ERROR ) {
		try {
			hsp3dish_error();
		}
		catch( ... ) {
		}
		emscripten_cancel_main_loop();
		exit(-1);
		return;
	}
	int endcode = ctx->endcode;
	hsp3dish_bye();
	exit(0);
}

extern "C"
{
void EMSCRIPTEN_KEEPALIVE hsp3dish_sync_done( void )
{
	fs_initialized = true;
}
}

int hsp3dish_exec( void )
{
	if (syncdir.size() > 0) {
		// IDBから読み込み
		fs_initialized = false;
		EM_ASM_({
			var dir = Pointer_stringify($0);
			FS.mkdir(dir);
			FS.mount(IDBFS, {}, dir);
			FS.syncfs(true, function (err) {
					console.log(err);
					ccall('hsp3dish_sync_done', 'v', '', []);
				});
			}, syncdir.c_str());
	} else {
		fs_initialized = true;
	}

	//		実行メインを呼び出す
	//
	hsp3dish_msgfunc( ctx );

	//		実行の開始
	//
	emscripten_set_main_loop(hsp3dish_exec_one, hsp_fps, 1);

	return 0;
}
