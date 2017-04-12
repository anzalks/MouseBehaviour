/***
 *       Filename:  main.ino
 *
 *    Description:  Protocol for EyeBlinkConditioning.
 *
 *        Version:  0.0.1
 *        Created:  2017-04-11

 *         Author:  Dilawar Singh <dilawars@ncbs.res.in>
 *   Organization:  NCBS Bangalore
 *
 *        License:  GNU GPL2
 */

#include <avr/wdt.h>

#define         DRY_RUN                     1

// Pins etc.
#define         TONE_PIN                    2
#define         LED_PIN                     3
#define         MOTION1_PIN                 4
#define         MOTION2_PIN                 7
#define         CAMERA_TTL_PIN              10
#define         PUFF_PIN                    11
#define         IMAGING_TRIGGER_PIN         13

#define         SENSOR_PIN                  A5

/*-----------------------------------------------------------------------------
 *  Change parameters here.
 *-----------------------------------------------------------------------------*/
#define         TONE_FREQ                   4500
#define         TONE_DURATION                 50
#define         LED_DURATION                  50
#define         PUFF_DURATION                 50


unsigned long stamp_ = 0;
unsigned dt_ = 2;
unsigned write_dt_ = 2;
unsigned trial_count_ = 0;

char msg_[80];

unsigned long trial_start_time_ = 0;
unsigned long trial_end_time_ = 0;


char trial_state_[5] = "PRE_";

/*-----------------------------------------------------------------------------
 *  User response
 *-----------------------------------------------------------------------------*/
int incoming_byte_ = 0;
bool reboot_ = false;

unsigned long currentTime( )
{
    return millis() - trial_start_time_;
}

/*-----------------------------------------------------------------------------
 *  WATCH DOG
 *-----------------------------------------------------------------------------*/
/**
 * @brief Interrupt serviving routine.
 *
 * @param _vect
 */
ISR(WDT_vect)
{
    // Handle interuppts here.
    // Nothing to handle.
}

void reset_watchdog( )
{
    if( not reboot_ )
        wdt_reset( );
}


/**
 * @brief  Read a command from command line. Consume when character is matched.
 *
 * @param command
 *
 * @return False when not mactched. If first character is matched, it is
 * consumed, second character is left in  the buffer.
 */
bool is_command_read( char command, bool consume = true )
{
    if( ! Serial.available() )
        return false;

    // Peek for the first character.
    if( command == Serial.peek( ) )
    {
        if( consume )
            Serial.read( );
        return true;
    }

    return false;
}

/**
 * @brief Write data line to Serial port.
 *   NOTE: Use python dictionary format. It can't be written at baud rate of
 *   38400 at least.
 * @param data
 * @param timestamp
 */
void write_data_line( )
{
    reset_watchdog( );

    // Just read the registers where pin data is saved.
    int puff = bitRead( PORTD, PUFF_PIN );
    int tone = bitRead( PORTD, TONE_PIN );
    int led = bitRead( PORTD, LED_PIN );

    int microscope = bitRead( PORTD, IMAGING_TRIGGER_PIN );
    int camera = bitRead( PORTD, CAMERA_TTL_PIN );

    unsigned long timestamp = millis() - trial_start_time_;

    int motion1 = analogRead( MOTION1_PIN );
    int motion2 = analogRead( MOTION2_PIN );
    
    sprintf(msg_  
            , "%lu,%d,%d,%d,%d,%d,%d,%d,%d,%s"
            , timestamp, trial_count_, puff, tone, led
            , motion1, motion2, camera, microscope, trial_state_
            );
    Serial.println(msg_);
    Serial.flush( );
}

void check_for_reset( void )
{
    if( is_command_read( 'r', true ) )
    {
        Serial.println( ">>> Reboot in 2 seconds" );
        reboot_ = true;
    }
}

/**
 * @brief Play tone for given period and duty cycle. 
 *
 * NOTE: We can not block the arduino using delay, since we have to write the
 * values onto Serial as well.
 *
 * @param period
 * @param duty_cycle
 */
void play_tone( unsigned long period, double duty_cycle = 0.5 )
{
    reset_watchdog( );
    check_for_reset( );
    unsigned long toneStart = millis();
    while( millis() - toneStart <= period )
    {
        write_data_line();
        if( millis() - toneStart <= (period * duty_cycle) )
            tone( TONE_PIN, TONE_FREQ );
        else
            noTone( TONE_PIN );
    }
    noTone( TONE_PIN );
}


/**
 * @brief Play puff for given duration.
 *
 * @param duration
 */
void play_puff( unsigned long duration )
{
    check_for_reset( );
    stamp_ = millis();
    while( millis() - stamp_ <= duration )
    {
        digitalWrite( PUFF_PIN, HIGH);
        write_data_line( );
    }
    digitalWrite( PUFF_PIN, LOW );
}

void led_on( unsigned int duration )
{
    stamp_ = millis( );
    while( millis() - stamp_ <= duration )
    {
        digitalWrite( LED_PIN, HIGH );
        write_data_line( );
    }
    digitalWrite( LED_PIN, LOW );
}


/**
 * @brief Configure the experiment here. All parameters needs to be set must be
 * done here.
 */
void configure_experiment( )
{
    // While this is not answered, keep looping 
    Serial.println( "?? Please configure your experiment" );
    Serial.println( "NOTE: This has been disabled" );
    while( true )
    {
        break;
#if 0
        incoming_byte_ = Serial.read( ) - '0';
        if( incoming_byte_ < 0 )
        {
            Serial.println( ">>> ... Waiting for response" );
            delay( 1000 );
        }
        else if( incoming_byte_ == 0 )
        {
            Serial.print( ">>> Valid response. Got " );
            Serial.println( incoming_byte_  );
            Serial.flush( );
            tsubtype_ = first;
            return;
        }
        else if( incoming_byte_ == 1 )
        {
            Serial.print( ">>> Valid response. Got " );
            Serial.println( incoming_byte_ );
            Serial.flush( );
            tsubtype_ = second;
            return;
        }
        else
        {
            Serial.print( ">>> Unexpected response, recieved : " );
            Serial.println( incoming_byte_ - '0' );
            delay( 100 );
        }
#endif
    }
}


/**
 * @brief Wait for trial to start.
 */
void wait_for_start( )
{
    sprintf( trial_state_, "INVA" );
    while( true )
    {
        write_data_line( );
        if( is_command_read( 's', true ) )
        {
            Serial.println( ">>> Start" );
            break;                              /* Only START can break the loop */
        }
        else if( is_command_read( 'p', true ) ) 
        {
            Serial.println( ">>> Playing puff" );
            play_puff( PUFF_DURATION );
        }
        else if( is_command_read( 't', true ) ) 
        {
            Serial.println( ">>> Playing tone" );
            play_tone( TONE_DURATION, 1.0);
        }
        else if( is_command_read( 'l', true ) ) 
        {
            Serial.println( ">>> LED ON" );
            led_on( LED_DURATION );
        }
        else
        {
            char c = Serial.read( );
            if( c != -1 )
            {
                Serial.print( ">>> Unknown command : " );
                Serial.println( c );
            }
        }
    }
}


void setup()
{
    Serial.begin( 38400 );

    // setup watchdog. If not reset in 2 seconds, it reboots the system.
    wdt_enable( WDTO_2S );
    wdt_reset();
    stamp_ = 0;

    pinMode( TONE_PIN, OUTPUT );
    pinMode( PUFF_PIN, OUTPUT );
    pinMode( CAMERA_TTL_PIN, OUTPUT );
    pinMode( IMAGING_TRIGGER_PIN, OUTPUT );

    /*  Pins set by sensors */
    pinMode( MOTION1_PIN, INPUT );
    pinMode( MOTION2_PIN, INPUT );

    configure_experiment( );
    Serial.println( ">>> Waiting for 's' to be pressed" );

    wait_for_start( );
}

void do_zero_trial( )
{
    return;
}

void do_first_trial( )
{
    return;
}

/**
 * @brief Do a single trial.
 *
 * @param trial_num. Index of the trial.
 * @param ttype. Type of the trial.
 */
void do_trial( unsigned int trial_num, bool isporobe = false )
{
    reset_watchdog( );
    check_for_reset( );

    trial_start_time_ = millis( );

    /*-----------------------------------------------------------------------------
     *  PRE. Start imaging for 10 seconds.
     *-----------------------------------------------------------------------------*/
    unsigned duration = 10000;
    unsigned endBlockTime = millis( );

    sprintf( trial_state_, "PRE_" );
    digitalWrite( IMAGING_TRIGGER_PIN, HIGH);   /* Start imaging. */
    digitalWrite( CAMERA_TTL_PIN, LOW );

    while( millis( ) - trial_start_time_ < duration ) /* PRE_ time */
    {
        // 500 ms before the PRE_ ends, start camera pin high. We start
        // recording as well.

        if( millis( ) - endBlockTime >= (duration - 500 ) )
            digitalWrite( CAMERA_TTL_PIN, HIGH );
        else
            digitalWrite( CAMERA_TTL_PIN, LOW );

        write_data_line( );
    }

    /*-----------------------------------------------------------------------------
     *  CS: 50 ms duration. No tone is played here. Write LED pin to HIGH.
     *-----------------------------------------------------------------------------*/
    endBlockTime = millis( );
    led_on( LED_DURATION );
    endBlockTime = millis( );

    /*-----------------------------------------------------------------------------
     *  TRACE. The duration of trace varies from trial to trial.
     *-----------------------------------------------------------------------------*/
    if( 6 <= trial_num || trial_num <= 7 )
        duration = 0;
    else if( 10 <= trial_num || trial_num <= 11 )
        duration = 350;
    else if( 12 <= trial_num || trial_num <= 13 )
        duration = 450;
    else
        duration = 250;

    sprintf( trial_state_, "TRAC" );
    while( millis( ) - endBlockTime <= duration )
    {
        check_for_reset( );
        write_data_line( );
    }
    endBlockTime = millis( );

    /*-----------------------------------------------------------------------------
     *  PUFF for 50 ms.
     *-----------------------------------------------------------------------------*/
    duration = 50;
    if( isporobe )
    {
        sprintf( trial_state_, "PROBE" );
        while( millis( ) - endBlockTime <= duration )
            write_data_line( );
    }
    else
    {
        sprintf( trial_state_, "PUFF" );
        play_puff( duration );
    }
    endBlockTime = millis( );
    
    /*-----------------------------------------------------------------------------
     *  POST, flexible duration till trial is over. It is 10 second long.
     *-----------------------------------------------------------------------------*/
    // Last phase is post. If we are here just spend rest of time here.
    duration = 10000;
    sprintf( trial_state_, "POST" );
    while( millis( ) - endBlockTime <= duration )
    {
        // Switch camera OFF after 500 ms into POST.
        if( millis() - endBlockTime >= 500 )
            digitalWrite( CAMERA_TTL_PIN, LOW );

        check_for_reset( );
        write_data_line( );
    }

    /*-----------------------------------------------------------------------------
     *  End trial.
     *-----------------------------------------------------------------------------*/
    digitalWrite( IMAGING_TRIGGER_PIN, LOW ); /* Shut down the imaging. */
    Serial.print( ">>END Trial " );
    Serial.print( trial_count_ );
    Serial.println( " is over. Starting new");
    trial_count_ += 1;
}

void loop()
{
    reset_watchdog( );

    // The probe trial occurs every 10th trial with +/- of 2 trials.
    unsigned numProbeTrials = 0;
    unsigned nextProbbeTrialIndex = random(8, 13);

    for (size_t i = 1; i <= 100; i++) 
    {
        reset_watchdog( );

        // Probe trial.
        if( i == nextProbbeTrialIndex )
        {
            do_trial( i, true );
            numProbeTrials +=1 ;
            nextProbbeTrialIndex = random( 
                    (numProbeTrials+1)*10-2, (numProbeTrials+1)*10+3
                    );
        }
        else
            do_trial( i, false );


        
        /*-----------------------------------------------------------------------------
         *  ITI.
         *-----------------------------------------------------------------------------*/
        unsigned long duration = random( 5000, 10001);
        unsigned long stamp_ = millis( );
        sprintf( trial_state_, "ITI_" );
        while( millis( ) - stamp_ <= duration )
            write_data_line( );
        
    }

    // We are done with all trials. Nothing to do.
    reset_watchdog( );
    Serial.println( "All done. Party!" );
    Serial.flush( );
    delay( 100 );
    exit( 0 );
}
