#define USE_OTA

#include <Arduino.h>

#include <Wire.h>

#include <WiFi.h>

#include <PubSubClient.h>

#define INV     500

/*
** time in seconds.
*/

#include <ESP32httpUpdate.h>

#include <Update.h>

#define STRINGIFY( s ) # s
#define TOSTR( s ) STRINGIFY( s )

typedef struct
    {
    const char * ssid;
    const char * passwd;
    } WiFiInfo_t;

static WiFiInfo_t _wifiInfo[] =
    {
    { "s1616", "4026892842" },
    { "GW4026892842-1", "40268928" },
    { "GW4026892842-2", "40268928" },
    { "GW4026892842-3", "40268928" },
    { "GW4026892842", "40268928" },
    { "sheepshed-mifi", "4026892842" },
    { "Jimmy-MiFi", "4026892842" },
    { NULL, NULL }
    };

static char _buf[ 200 ];
static char _mac[ 20 ];
        
static unsigned long _endTime = 0;

static WiFiClient _clnt;
static PubSubClient _mqtt;

static bool _aht20 = false;
static bool _bme280 = false;

static int _numDevice = 0;

static String _devList;

static void _connect();
static void _loadMac();

static void _rpt(
        const int aLine,
        const char * const aFrmt,
        ...
        )
    {
    static bool oneTime = true;
    static char _topic[ 30 ];

    va_list ap;

    if ( oneTime )
        {
        oneTime = false;

        strcpy( _topic, "/RPT/" );
        strcat( _topic, _mac );

        Serial.print( "_topic: " ); Serial.println( _topic );
        }

    va_start( ap, aFrmt );
    vsnprintf( _buf, sizeof( _buf ), aFrmt, ap );
    va_end( ap );

    Serial.printf( "(_rpt)(%d) - _topic: %s - _payload: (%u)%s\r\n",
            aLine, _topic, strlen( _buf ), _buf );

    if ( !_mqtt.connected() )
        {
        _connect();
        }

    if ( _mqtt.connected() )
        {
        _mqtt.publish( _topic, _buf );

        _mqtt.loop();
        }
    else
        {
        Serial.printf( "(NOT CONNECTED) - %s - %s\r\n",
                _topic, _buf );
        }
    }

static void _cb(
        char * aTopic,
        byte * aPayload,
        unsigned int aPayloadLen
        )
    {
    char cmd[ 20 ];

    char ch;
    const char * wp;

    unsigned cnt;

    Serial.printf( "_cb(entered) - aTopic: %s, aPayload: %*.*s\r\n",
            aTopic, (int) aPayloadLen, aPayloadLen, (char *) aPayload );

    for( cnt = 0, wp = (char *) aPayload; 
            (cnt < aPayloadLen) && ((ch = wp[ cnt ]) != ' '); cnt ++ )
        {
        cmd[ cnt ] = ch;
        }
    cmd[ cnt ] = '\0';

    // Serial.printf( "cmd: %s\r\n", cmd );

    if ( strcmp( aTopic, "track/light/MGMT" ) == 0 )
        {
        if ( strcmp( cmd, "reboot" ) == 0 )
            {
            _rpt( __LINE__, "reboot - mqtt message" );
            Serial.println( "reboot(commanded)" );
            delay( 1000 );
            ESP.restart();
            }
        }
    else
        {
        String defTopic;

        defTopic = "track/light/";
        defTopic += _mac;

        if ( defTopic == aTopic )
            {
            if ( strcmp( cmd, "ON" ) == 0 )
                {
                digitalWrite( LED_BUILTIN, HIGH );
                }
            else if ( strcmp( cmd, "OFF" ) == 0 )
                {
                digitalWrite( LED_BUILTIN, LOW );
                }
            }
        }
    }

#define CONNECT_TIME_LIMIT_SEC_l    20
#define MGMT_TOPIC "track/light/#"

static void _subscribe(
        )
    {
    String defTopic;

    _mqtt.subscribe( MGMT_TOPIC );

    Serial.printf( "Subscribe: %s\r\n", MGMT_TOPIC );
    }

static void _connect(
        )
    {
    Serial.println( "_connect(entered)" );

    static char _clntId[ 30 ];

    int loopCnt;

    unsigned long et;

    for ( loopCnt = 0, et = millis() + 500; millis() < et; )
        {
        loopCnt ++;

        if ( _mqtt.connected() )
            {
            _mqtt.loop();
            delay( 100 );
            }
        else
            {
            break;
            }
        }
    
    if ( _mqtt.connected() )
        {
        return;
        }

    strcpy( _clntId, "c-" );
    strcat( _clntId, _mac );

    for ( et = millis() + (CONNECT_TIME_LIMIT_SEC_l * 1000); millis() < et; )
        {
        if ( _mqtt.connected() )
            {
            break;
            }

        if ( _mqtt.connect( _clntId ) )
            {
            break;
            }

        delay( 1000 );
        }

    if ( millis() >= et )
        {
        Serial.printf( "UNABLE TO CONNECT MQTT\r\n" );
        delay( 10000 );
        ESP.restart();
        }

    _subscribe();
    }

static void _setupPubSub(
        )
    {
    _mqtt.setClient( _clnt );
    _mqtt.setBufferSize( 1024 );
    // _mqtt.setServer( "pharmdata.ddns.net", 1883 );
    _mqtt.setServer( "mqtt.lam", 1883 );
    _mqtt.setCallback( _cb );
    
    _connect();
    }

static char _ssid[ 30 ];

static void _setupWiFi(
        )
    {
    int cnt;
    int numSsid;
    int ssidIdx;

    WiFiInfo_t * wp;

    Serial.println( "Scan WiFi Network:" );
    numSsid = WiFi.scanNetworks();

    if ( numSsid <= 0 )
        {
        Serial.printf( "numSsid: %d (delay/restart)\r\n", numSsid );
        delay( 60000 );
        ESP.restart();
        }
    
    for( ssidIdx = 0; ssidIdx < numSsid; ssidIdx ++ )
        {
        String ssid = WiFi.SSID( ssidIdx );
    
        Serial.print( "SSID: " ); Serial.print( ssid );
        Serial.print( ", RSSI: " ); Serial.println( WiFi.RSSI( ssidIdx ) );

        for( wp = _wifiInfo; wp->ssid != NULL; wp ++ )
            {
            if ( ssid == wp->ssid )
                {
                Serial.printf( "SSID: '%s', Attempt to connect.\r\n", wp->ssid );

                WiFi.begin( wp->ssid, wp->passwd );

                for( cnt = 0; (cnt < 20) && (WiFi.status() != WL_CONNECTED); cnt ++ )
                    {
                    Serial.print( "." );
                    delay( 500 );
                    }
                Serial.println( "" );

                if ( WiFi.status() == WL_CONNECTED )
                    {
                    Serial.printf( "SSID: '%s', CONNECTED\r\n", wp->ssid );
                    strcpy( _ssid, wp->ssid );
                    return;
                    }
                }
            }
        }

    if ( ssidIdx >= numSsid )
        {
        Serial.println( "Unable to connect to WiFi(delay/restart)" );
        delay( 60000 );
        ESP.restart();
        }
    }

static void _loadMac(
        )
    {
    uint8_t rm[ 6 ];

    WiFi.macAddress( rm );
    sprintf( _mac, "%02X:%02X:%02X:%02X:%02X:%02X",
            rm[ 0 ], rm[ 1 ], rm[ 2 ], 
            rm[ 3 ], rm[ 4 ], rm[ 5 ] );
    }

#ifdef USE_OTA
#define CBINV       10
#define PROGRESSINV 2

static void _progress(
        size_t aDone,
        size_t aTotal
        )
    {
    static long int _nxtTime;
    static long int _stTime;
    static size_t _lstDone;

    long int elapseTime;
    size_t pct10;

    if ( millis() > _nxtTime )
        {
        _nxtTime = millis() + (PROGRESSINV * 1000);

        if ( _lstDone > aDone )
            {
            _stTime = millis();
            }
        elapseTime = millis() - _stTime;

        pct10 = (aDone * 1000) / aTotal;
        Serial.printf( "\r(%u/%u(%%%d.%d) (%u byte/sec) (total Elapsed %lu (millis))", 
                aDone, aTotal, pct10/ 10, pct10 % 10, (_lstDone < aDone) ?
                ((aDone - _lstDone) / PROGRESSINV) : 0, elapseTime );

        _lstDone = aDone;
        }
    }

static void _checkOTA(
        )
    {
    static bool oneTime = true;

    static char _prog[ 200 ];

    Serial.println( "_checkOTA(entered)" );

    if ( oneTime )
        {
        oneTime = false;
        
        strcpy( _prog, "firmware/main/" );
        strcat( _prog, TOSTR( __BOARD ) );
        strcat( _prog, "/LC/firmware" );

        Serial.printf( "_checkOTA - (_prog: %s)\r\n", _prog );
        }


    Serial.printf( "(%d) _checkOTA - (_prog: %s)\r\n", __LINE__, _prog );
    Serial.printf( "(%d) _checkOTA - (repo: %s) file: %s\r\n", 
            __LINE__, "pharmdata.ddns.net", _prog );

    Update.onProgress( _progress );

    ESPhttpUpdate.rebootOnUpdate( true );

    t_httpUpdate_return ret = ESPhttpUpdate.update( "pharmdata.ddns.net", 80,
                "/firmware/update.php", _prog );

    /*
    ** reconnect the PUBSUB
    */
    _connect();

    if ( ESPhttpUpdate.getLastError() != 0 )
        {
	    Serial.printf( "(%d) %s\r\n", 
                ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str() );
         }

    switch( ret )
        {
        case HTTP_UPDATE_FAILED:
            Serial.printf( "HTTP_UPDATE_FAILED(%d)\r\n", ret );
            break;

        case HTTP_UPDATE_NO_UPDATES:
            // Serial.printf( "HTTP_UPDATE_NO_UPDATES(%d)\r\n", ret );
            break;

        case HTTP_UPDATE_OK:
            Serial.printf( "HTTP_UPDATE_OK(%d)\r\n", ret );
            break;
        }
    }
#endif

static void _scanI2C(
        )
    {
    bool ret;

    byte addr;
    byte err;

    delay( 1000 );
    Serial.printf( "Scanning i2c\r\n" );

    for( addr = 1; addr < 127; addr ++ )
        {
        Wire.beginTransmission( addr );
        err = Wire.endTransmission();

        if ( err == 0 )
            {
            char devId[ 50 ];
            sprintf( devId, "%d(0x%X) ", (int) addr, (int) addr );
            _devList += devId;

            _numDevice ++;
            if ( addr == 0x38 )
                {
                _aht20 = true;
                _devList += "(_aht20: true) ";
                }

            if ( addr == 0x76 )
                {
                _bme280 = true;
                _devList += "(_bme280: true) ";
                }
            }
        }

    Serial.printf( "_devList: %s\r\n", _devList.c_str() );
    }

static bool _use_lc = false;

void setup(
        )
    {
    _loadMac();

    delay( 1000 );

    Serial.begin( 115200 );
    delay( 1000 );

    bool wireStatus = Wire.begin();

    Serial.printf( "\r\nHello:\r\n%s(%d)\r\nCompile: %s %s\r\nMAC: %s\r\n",
            __FILE__, __LINE__, __DATE__, __TIME__, _mac );

    pinMode( LED_BUILTIN, OUTPUT );

    _setupWiFi();
    _setupPubSub();

#ifdef USE_OTA
    _checkOTA();
#endif

    _rpt( __LINE__, "\r\n\nFile: " __FILE__ "\r\nCompile: " __DATE__ " " __TIME__ "\r\nPROG: %s\r\nmac: %s\r\n\n",
            TOSTR( PROG ), _mac );
    
    }

#define BLINKINV 500
#define CHKBATTINV 60
#define CHKCELLINV 60
#ifdef USE_INA3221
#define CHKINA3221INV 10
#endif
/*
TMO is in millis
*/
#define RCVTMO  5000
#define RPTINV 10

void loop(
        )
    {

    static bool _blinkState = false;

    static unsigned long _batTime = 0;
    static unsigned long _cellTime = 0;
#ifdef USE_INA3221
    static unsigned long _INA3221Time = 0;
#endif
    static unsigned long _blinkTime = 0;
    static unsigned long _rptTime = 0;

    unsigned long now = millis();

    if ( !_mqtt.connected() )
        {
        _connect();
        }

    _mqtt.loop();
    }

