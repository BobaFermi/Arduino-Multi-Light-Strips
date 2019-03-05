#include <FastLED.h>
#include <LiquidCrystal.h>
#define btnRIGHT 0
#define btnUP 1
#define btnDOWN 2
#define btnLEFT 3
#define btnSELECT 4
#define btnNONE 5
#define btnUNKNOWN 6
#define numcupLEDs 42
#define numfoodLEDs 30
#define pirPin 19
#define cupdataPin 20
#define fooddataPin 21
#define lcdpin 22
#define max_brightness 255

CRGB cupleds[numcupLEDs];
CRGB foodleds[numfoodLEDs];
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

int totalLEDs = numcupLEDs + numfoodLEDs;
bool setsig = LOW; //setsig is used for PIR motion sensor, to compare against when motion is detected or stops being detected
bool pirsig = LOW; //pirsig gives a reading from the pir sensor
unsigned long lighttick, pirtimeout, pirreaddelay, btntimer; //various timers used throughout the program - lighttick times the lighting for Xmas and rainbow settings, pirtimeout records last time pir detected motion for lights to switch off, pirreaddelay gives the pir sensor some time to get a proper digital reading, btntimer records last time a button was pressed for lcd backlight to switch off
uint16_t adc_key_in, rainbowmax, rainbowmin;
uint8_t optionsNum, colourInd, pushed, brightness, dispBright, redbrightness, greenbrightness, bluebrightness, rainbowinitflag, rainbowdelay; //various brightnesses and indices
bool lightStatus, christmasSwitch, tock, tocker, pirinactive, screenactive, updatelights; //booleans, usually flags to say whether an event has occurred or state machine stuff
const char* colours[] = {"White  ", "Red    ", "Green  ", "Blue   ", "Cyan   ", "Magenta", "Yellow ", "Rainbow", "Xmas!  "}; //Colour choice (With spaces included to clear the rest of the LCD after the colour)
int redtrend[numcupLEDs + numfoodLEDs]; //An array of integers to tell each LED whether it's supposed to be adding or subtracting brightness from its red module
int greentrend[numcupLEDs + numfoodLEDs]; //An array of integers to tell each LED whether it's supposed to be adding or subtracting brightness from its green module
int bluetrend[numcupLEDs + numfoodLEDs]; //An array of integers to tell each LED whether it's supposed to be adding or subtracting brightness from its blue module
int redbrightmultiplier[numcupLEDs + numfoodLEDs]; //An array keeping track of the red brightness values during rainbow mode, usually a multiplier of redbrightness from 0-1
int greenbrightmultiplier[numcupLEDs + numfoodLEDs]; //An array keeping track of the green brightness values during rainbow mode, usually a multiplier of greenbrightness from 0-1
int bluebrightmultiplier[numcupLEDs + numfoodLEDs]; //An array keeping track of the blue brightness values during rainbow mode, usually a multiplier of bluebrightness from 0-1

void dosensor(); //forward declaration of the function dosensor, which handles the pir sensor and retriggers the light timeout if it detects motion
void dolight(CRGB* leds, uint8_t numLEDs); //forward declaration of the function dolight, handles all the light functions, except rainbow mode
void dorainbow(CRGB* leds, uint8_t numLEDs); //forward declaration of the function dorainbow, handles rainbow mode
  
void setup() {
    lcd.begin(16, 2); //initialise LCD module
    optionsNum = sizeof(colours)/2; //calculates the number of options in the colours array
    //initialises all flags to their untriggered values - better way to do this?
    colourInd = 0; //Index of which colour is selected
    brightness = 0; //Brightness all LEDs should follow (maximum brightness that should be displayed by any module in rainbow mode)
    redbrightness = 0; //Brightness of red module, needs to be used in case different brightnesses 
    greenbrightness = 0; //Brightness of green module, needs to be used in case different brightnesses 
    bluebrightness = 0; //Brightness of blue module, needs to be used in case different brightnesses 
    lightStatus = 0; //Whether LEDs should be active or not
    pirinactive = 0; //Whether lights should be switched off due to pir sensor inactivity
    christmasSwitch = 0; //Is Christmas mode activated?
    rainbowinitflag = 0; //Has rainbow been initialised? This means the LED colours have all been set, and can just be added or subtracted from
    updatelights = 0;
    screenactive = 1; //Is the LCD backlight switched on?
    pinMode(pirPin, INPUT); //Set pin used for PIR sensor as an input
    pinMode(lcdpin, OUTPUT); //Set pin used for LCD backlight as an output
    digitalWrite(lcdpin, HIGH); //Turn LCD backlight on
    FastLED.addLeds<NEOPIXEL, cupdataPin>(cupleds, numcupLEDs); //Initialise LEDs, with the numbers given in the #define block
    FastLED.addLeds<NEOPIXEL, fooddataPin>(foodleds, numfoodLEDs); //Initialise LEDs, with the numbers given in the #define block
    //To check that LEDs are connected properly, flash them on white for half a second
    for (uint8_t i=0; i<numcupLEDs; i++){
        cupleds[i-1].r = 100;
        cupleds[i-1].g = 100;
        cupleds[i-1].b = 100;
        if (i <= numfoodLEDs){
            foodleds[i-1].r = 100;
            foodleds[i-1].g = 100;
            foodleds[i-1].b = 100;
        }
    }
    FastLED.show();
    delay(500);
    for (uint8_t i=0; i<numcupLEDs; i++){
        cupleds[i-1].r = 0;
        cupleds[i-1].g = 0;
        cupleds[i-1].b = 0;
        if (i <= numfoodLEDs){
            foodleds[i-1].r = 0;
            foodleds[i-1].g = 0;
            foodleds[i-1].b = 0;    
        }
    }
    FastLED.show();
}

void loop() {
    buttonrespond(); //check whether buttons are being pressed or not
    updatescreen(); //update screen based on what buttons have been pressed, if any
    if (lightStatus) dosensor(); //if the LEDs are switched on, check pir sensor to timeout LEDs
    if (colourInd == 7 && lightStatus && !pirinactive){
        dorainbow(cupleds, numcupLEDs); //if the light is on, rainbow is selected and pir sensor timeout hasn't occurred
        dorainbow(foodleds, numfoodLEDs); //if the light is on, rainbow is selected and pir sensor timeout hasn't occurred
    }
    else{ 
        dolight(cupleds, numcupLEDs); //dolight catches all other situations whether lights are on or not
        dolight(foodleds, numfoodLEDs); //dolight catches all other situations whether lights are on or not
    }
} 

int readkeypad(){
    int adc_key_in = analogRead(0); //Raw analog read of buttons, can give a huge range of values for each button
    int ret = btnUNKNOWN; //Catch in case none of the following button cases apply

    if (adc_key_in < 50) ret = btnRIGHT; 
    if ( (adc_key_in > 500) && (adc_key_in < 1150) ) ret = btnNONE;
    if ( (adc_key_in > 120) && (adc_key_in < 150) ) ret = btnUP;
    if ( (adc_key_in > 250) && (adc_key_in < 350) ) ret = btnDOWN;
    if ( (adc_key_in > 450) && (adc_key_in < 500) ) ret = btnLEFT;
    if ( (adc_key_in > 700) && (adc_key_in < 750) ) ret = btnSELECT;

    return ret;
}

void buttonrespond(){
    if (pushed != readkeypad()){ //This if statement, paired with the pushed variable, stops the Arduino cycling through options really quickly while the button is held down
        pushed = readkeypad(); //Set pushed variable to whatever the pushed button is
        btntimer = millis(); //Timer so the LCD backlight can be turned off after the buttons have not been pressed for some time
        if (screenactive){ //Stops recording button presses if the LCD backlight is off, so the screen turns on if the button is pressed while changing nothing else
            if (pushed == btnRIGHT){
                if (colourInd == 7) rainbowinitflag = 0; //This means that if rainbow mode is de-selected, then selected again without the light being turned off, it re-initialises the colours
                if (colourInd == optionsNum-1) colourInd=0; //This case allows to cycle through the colour options in a wraparound fashion
                else colourInd++; //If we're not at the end of the colour options, we can keep going up
                updatelights = 1;
            }
            else if (pushed == btnLEFT){
                if (colourInd == 7) rainbowinitflag = 0; //This means that if rainbow mode is de-selected, then selected again without the light being turned off, it re-initialises the colours
                if (colourInd == 0) colourInd = optionsNum-1; //This case allows to cycle through the colour options in a wraparound fashion
                else colourInd--; //If we're not at the start of the colour options, we can keep going down
                updatelights = 1;
            }
            else if (pushed == btnUP && brightness < max_brightness){
                dispBright++; //If we're not a max brightness, we can keep going up
                updatelights = 1;
            }
            else if (pushed == btnDOWN && brightness > 0){
              dispBright--; //If we're not at min brightness, we can keep going down
              updatelights = 1;
            }
            else if (pushed == btnSELECT){
              lightStatus = !lightStatus; //If LEDs are off and the select button is pushed, turn them on, if they're already on, turn them off
              updatelights = 1;
            }
            if (dispBright > 0) brightness = dispBright*16-1; //Set the brightness values from the display brightness selected (powers of 2 minus 1 as a general rule, because the max the LEDs can handle is 255)
            else brightness = 0; //To stop the strange case where you end up with brightness = -1
        }
    }
}

void updatescreen(){
  //Set the cursor to various locations and print out the stats of the LEDs - Colour, brightness, on or off?
    lcd.setCursor(0,0);
    lcd.print("Colour: ");
    lcd.setCursor(8,0);
    lcd.print(colours[colourInd]);
    lcd.setCursor(0,1);
    lcd.print("Brightness: ");
    lcd.print(dispBright);
    lcd.print(" ");
    lcd.print(lightStatus);
    lcd.print(" ");
    //An if statement to turn the LCD backlight off if buttons haven't been touched in 10 seconds
    if (millis() > btntimer+10000){ 
        digitalWrite(lcdpin, LOW);
        screenactive = 0;
    }
    //The else to switch the LCD backlight on if buttons have been touched
    else{ 
        digitalWrite(lcdpin, HIGH);
        screenactive = 1;
    }
}

void dosensor(){
  //Don't check the pir sensor too often, or the readout can become unreliable
    if (millis() > pirreaddelay+150){
        pirsig = digitalRead(pirPin);
        pirreaddelay = millis();
    }
    //If the pir sensor detects motion
    if (pirsig == HIGH){
        pirtimeout = millis(); //take a new time for the LED timeout
        if (setsig == LOW){ //If motion wasn't being detected before
            setsig = HIGH; //Note the fact motion has been detected now
            if (tocker){ //If the LEDs have been timed out already
                tocker = 0; //Start recording time once the pir stops detecting motion again and stops the LEDs being repetitively reactivated
                pirinactive = 0; //Reactivate the lights
                updatelights = 1;
            }
        }
    }
    else{ //If the pir sensor doesn't detect motion
        if(setsig == HIGH) setsig = LOW; //If the pir sensor detected motion before, record the fact motion isn't being sensed anymore
        if (millis() > pirtimeout+60000 && !tocker){ //If 60 seconds have passed since motion was last detected
            tocker = 1; //No need to keep tracking time or deactivating the lights if it's already happened
            pirinactive = 1; //Deactivates the lights
            updatelights = 1;
        }
    }
}

void dolight(CRGB* leds, uint8_t numLEDs){
    if (colourInd == 0 || colourInd == 1 || colourInd == 5 || colourInd == 6 || colourInd == 8) redbrightness = brightness; //Any of the cases that require red light - White, Red, Magenta, Yellow, Xmas
    else redbrightness = 0;
    if (colourInd == 0 || colourInd == 2 || colourInd == 4 || colourInd == 6 || colourInd == 8) greenbrightness = brightness; //Any of the cases that require green light - White, Green, Cyan, Yellow, Xmas
    else greenbrightness = 0;
    if (colourInd == 0 || colourInd == 3 || colourInd == 4 || colourInd == 5 || colourInd == 7) bluebrightness = brightness; //Any of the cases that require blue light - White, Blue, Cyan, Magenta, I think rainbow is included for the specific case where xmas mode is changed to rainbow mode, so something changes causing the if statement at the end of this block to be evaluated...?
    else bluebrightness = 0;
    if (!lightStatus || pirinactive){ //If the lights have been switched off, or the pir timeout has been triggered
      //switch all light brightnesses to 0
        redbrightness = 0;
        greenbrightness = 0;
        bluebrightness = 0;
        rainbowinitflag = 0; //This flag set to 0 in case rainbow mode was set, and the lights are turned off and on again - allows for the rainbow mode to be reinitialised
    }  
    if (colourInd == 8 && lightStatus){ //In the specific case where Xmas mode has been selected and the lights are switched on
        if (tock){
            for (uint8_t i=0; i<numLEDs; i++){ //For all the LEDs
                leds[i].b = 0; //blue off, this probably helps if set to Christmas and change the light setting without switching the lights off first
                if (i%2!=0){ //for all even numbers of LED
                    leds[i].r = redbrightness*christmasSwitch; //christmasSwitch controls which LEDs are green, and which are red. This LED brightness can either be the value of redbrightness, or 0.
                    leds[i-1].r = redbrightness-(redbrightness*christmasSwitch); //This brightness can either be redbrightness, or 0. Whatever the LEDs on the previous line do, these LEDs do the opposite. 
                    leds[i-1].g = greenbrightness*christmasSwitch; //Same as before, but for green light
                    leds[i].g = greenbrightness-(greenbrightness*christmasSwitch);  //Same as before, but for green light. Whatever the LEDs in the previous line do, these LEDs do the opposite
                }
                if (numLEDs%2!=0 && i==numLEDs-2){ //If there are an odd number of LEDs, and this is the second last LED...
                    leds[i+1].r = redbrightness-(redbrightness*christmasSwitch);
                    leds[i+1].g = greenbrightness*christmasSwitch;
                }
            }
           FastLED.show(); //Write the brightness value to the LEDs
        }
          if (!tock){ //If the christmasSwitch has just been flipped
              lighttick = millis(); //Note the current time
              tock = 1; //Don't come back in here until the christmasSwitch has been flipped again
          }
          if (millis()>lighttick+500){ //If 500 ms have passed since we last flipped the christmasSwitch
              tock = 0; //We want to go into the previous if statement when the program loops back around
              christmasSwitch = !christmasSwitch; //Flip christmasSwitch to whatever it isn't right now, 1->0 or 0->1
          }
    }

    if(updatelights){
        if (!(colourInd == 8 && lightStatus)){ //Only do this if Christmas mode isn't active (When Christmas mode was active and we changed brightness, there would be a yellow flash because we were performing this for loop, setting red and green brightness to all LEDs no matter what)
            for(uint8_t i=0; i<numLEDs; i++){ //
                leds[i].r = redbrightness; //Set the red modules of all LEDs to the redbrightness
                leds[i].g = greenbrightness; //Set the green modules of all LEDs to the greenbrightness
                leds[i].b = bluebrightness; //Set the blue modules of all LEDs to the bluebrightness
            }
            FastLED.show(); //Write these settings to the LED
            if(leds == foodleds) updatelights = 0;
        }
    }
}

void dorainbow(CRGB* leds, uint8_t numLEDs){
    /*
     * Tried to make this function simple by giving a start brightness for each module in each LED and always adding or subtracting from them in a triangular wave fashion. 
     * Rather than telling each module when it should stop getting brighter or dimmer, we just keep going up past the max brightness to rainbowmax, but if there's a min function to stop the LED getting any brighter past that point.
     * The triangular wave goes down past zero too, so there's a max function to deal with that.
     * The green wave is offset from the red wave by 1/3 of the number of LEDs, the blue wave is offset from the green wave by 1/3 of the number of LEDs.
     *                /\                              /\
     *               /  \                            /  \
     *              /    \                          /    \
     *             /      \                        /      \
     *            /        \                      /        \
     *___________/__________\____________________/__________\_____
     *          /            \                  /            \
     *         /              \                /              \
     *        /                \              /                \
     *       /                  \            /                  \
     *      /                    \          /                    \
     *_____/______________________\________/______________________\
     *    /                        \      /                        \
     *   /                          \    /                          \
     *  /                            \  /                            \
     * /                              \/                              \
     */
    uint8_t rainbowarrayindex;
    if (updatelights || rainbowinitflag<2){
        int rainbowduration = 10; //Should be a duration of time over which to cycle the LEDs during rainbow mode, should be divided by brightness to give the same rate of colour transform regardless of brightness, didn't really work when I tried it.
        int rainbowdelay = rainbowduration/brightness; //Supposed to take the full rainbow loop duration set globally, and divide it by the brightness so the rainbow takes the same time regardless of set brightness. Didn't really work that well?
        rainbowmax = brightness*3; //The maximum for our triangular wave
        rainbowmin = -1*rainbowmax; //The minimum for our triangular wave
    }
    if (rainbowinitflag < 2){ //If rainbow hasn't been initialised yet
        float rainbowgradient = 1.0*numLEDs/12; //rainbow split into 12 segments for all the different colours (excluding white light)
        float gradientmultiplier = brightness/rainbowgradient; //How much the brightness of each module should change from one LED to the next
        if (numLEDs == numcupLEDs){
            //Red stuff first
            for(uint8_t i=0; i<numLEDs; i++){
                if (i<(3*numLEDs)/12){ //if we're in the first quarter of the LEDs
                    redbrightmultiplier[i] = (int)(i)*gradientmultiplier; //Set the red value in the red array for this LED
                    redtrend[i-i] = 1; //When we begin the rainbow loop, the red module for this LED is adding, rather than subtracting
                }
                else if (i>=(3*numLEDs)/12 && i<(6*numLEDs)/12){ //If we're in the second quarter of the LEDs
                    redbrightmultiplier[i] = (int)rainbowmax-(i-((3*numLEDs)/12))*gradientmultiplier;
                    redtrend[i] = -1;
                }
                else if (i>=(6*numLEDs)/12 && i<(9*numLEDs)/12){
                    redbrightmultiplier[i] = (int)rainbowmax-(i-((3*numLEDs)/12))*gradientmultiplier;
                    redtrend[i] = -1;
                }
                else if (i>=(9*numLEDs)/12 && i<numLEDs){
                    redbrightmultiplier[i] = (int)rainbowmin+(i-((9*numLEDs)/12))*gradientmultiplier;
                    redtrend[i] = 1;
                }
                //Green stuff
                if (i>=(4*numLEDs)/12 && i<(7*numLEDs)/12){
                    greenbrightmultiplier[i] = (int)(i-((4*numLEDs)/12))*gradientmultiplier;
                    greentrend[i] = 1;
                }
                else if (i>=(7*numLEDs)/12 && i<(11*numLEDs)/12){
                    greenbrightmultiplier[i] = (int)rainbowmax-(i-((7*numLEDs)/12))*gradientmultiplier;
                    greentrend[i] = -1;
                }
                else if (i>=(11*numLEDs)/12 && i<numLEDs){
                    greenbrightmultiplier[i] = (int)rainbowmax-(i-((7*numLEDs)/12))*gradientmultiplier;
                    greentrend[i] = -1;
                }
                else if (i<numLEDs/12){
                    greenbrightmultiplier[i] = (int)rainbowmax-(i+11)*gradientmultiplier;
                    greentrend[i] = -1;
                }
                else if (i>=numLEDs/12 && i<(4*numLEDs)/12){
                    greenbrightmultiplier[i] = (int)rainbowmin+(i-(numLEDs/12))*gradientmultiplier;
                    greentrend[i] = 1;
                }
                //Blue stuff
                if (i>=(8*numLEDs)/12 && i<(11*numLEDs)/12){
                    bluebrightmultiplier[i] = (int)(i-((8*numLEDs)/12))*gradientmultiplier;
                    bluetrend[i] = 1;
                }
                else if (i>=(11*numLEDs)/12 && i<numLEDs){
                    bluebrightmultiplier[i] = (int)rainbowmax-(i-((11*numLEDs)/12))*gradientmultiplier;
                    bluetrend[i] = -1;
                }
                else if (i<(2*numLEDs)/12){
                    bluebrightmultiplier[i] = (int)rainbowmax-(i+3)*gradientmultiplier;
                    bluetrend[i] = -1;
                }
                else if (i>=(2*numLEDs)/12 && i<(5*numLEDs)/12){
                    bluebrightmultiplier[i] = (int)rainbowmax-(i+3)*gradientmultiplier;
                    bluetrend[i] = -1;
                }
                else if (i>=(5*numLEDs)/12 && i<(8*numLEDs)/12){
                    bluebrightmultiplier[i] = (int)rainbowmin+((i-((5*numLEDs)/12))*gradientmultiplier);
                    bluetrend[i] = 1;
                }
                leds[i].r = max(min(redbrightmultiplier[i],brightness),0);
                leds[i].g = max(min(greenbrightmultiplier[i],brightness),0);
                leds[i].b = max(min(bluebrightmultiplier[i],brightness),0);
            }
        }
        else if (numLEDs == numfoodLEDs){ 
            //Red stuff first  
            for(uint8_t i=0; i<numLEDs; i++){ //Go through all LEDs
                uint8_t rainbowarrayindex = numcupLEDs+i;
                if (i<(3*numLEDs)/12){ //if we're in the first quarter of the LEDs
                    redbrightmultiplier[rainbowarrayindex] = (int)(i)*gradientmultiplier; //Set the red value in the red array for this LED
                    redtrend[rainbowarrayindex] = 1; //When we begin the rainbow loop, the red module for this LED is adding, rather than subtracting
                }
                else if (i>=(3*numLEDs)/12 && i<(6*numLEDs)/12){ //If we're in the second quarter of the LEDs
                    redbrightmultiplier[rainbowarrayindex] = (int)rainbowmax-(i-((3*numLEDs)/12))*gradientmultiplier;
                    redtrend[rainbowarrayindex] = -1;
                }
                else if (i>=(6*numLEDs)/12 && i<(9*numLEDs)/12){
                    redbrightmultiplier[rainbowarrayindex] = (int)rainbowmax-(i-((3*numLEDs)/12))*gradientmultiplier;
                    redtrend[rainbowarrayindex] = -1;
                }
                else if (i>=(9*numLEDs)/12 && i<numLEDs){
                    redbrightmultiplier[rainbowarrayindex] = (int)rainbowmin+(i-((9*numLEDs)/12))*gradientmultiplier;
                    redtrend[rainbowarrayindex] = 1;
                }
                //Green stuff
                if (i>=(4*numLEDs)/12 && i<(7*numLEDs)/12){
                    greenbrightmultiplier[rainbowarrayindex] = (int)(i-((4*numLEDs)/12))*gradientmultiplier;
                    greentrend[rainbowarrayindex] = 1;
                }
                else if (i>=(7*numLEDs)/12 && i<(11*numLEDs)/12){
                    greenbrightmultiplier[rainbowarrayindex] = (int)rainbowmax-(i-((7*numLEDs)/12))*gradientmultiplier;
                    greentrend[rainbowarrayindex] = -1;
                }
                else if (i>=(11*numLEDs)/12 && i<numLEDs){
                    greenbrightmultiplier[rainbowarrayindex] = (int)rainbowmax-(i-((7*numLEDs)/12))*gradientmultiplier;
                    greentrend[rainbowarrayindex] = -1;
                }
                else if (i<numLEDs/12){
                    greenbrightmultiplier[rainbowarrayindex] = (int)rainbowmax-(i+11)*gradientmultiplier;
                    greentrend[rainbowarrayindex] = -1;
                }
                else if (i>=numLEDs/12 && i<(4*numLEDs)/12){
                    greenbrightmultiplier[rainbowarrayindex] = (int)rainbowmin+(i-(numLEDs/12))*gradientmultiplier;
                    greentrend[rainbowarrayindex] = 1;
                }
                //Blue stuff
                if (i>=(8*numLEDs)/12 && i<(11*numLEDs)/12){
                    bluebrightmultiplier[rainbowarrayindex] = (int)(i-((8*numLEDs)/12))*gradientmultiplier;
                    bluetrend[rainbowarrayindex] = 1;
                }
                else if (i>=(11*numLEDs)/12 && i<numLEDs){
                    bluebrightmultiplier[rainbowarrayindex] = (int)rainbowmax-(i-((11*numLEDs)/12))*gradientmultiplier;
                    bluetrend[rainbowarrayindex] = -1;
                }
                else if (i<(2*numLEDs)/12){
                    bluebrightmultiplier[rainbowarrayindex] = (int)rainbowmax-(i+3)*gradientmultiplier;
                    bluetrend[rainbowarrayindex] = -1;
                }
                else if (i>=(2*numLEDs)/12 && i<(5*numLEDs)/12){
                    bluebrightmultiplier[rainbowarrayindex] = (int)rainbowmax-(i+3)*gradientmultiplier;
                    bluetrend[rainbowarrayindex] = -1;
                }
                else if (i>=(5*numLEDs)/12 && i<(8*numLEDs)/12){
                    bluebrightmultiplier[rainbowarrayindex] = (int)rainbowmin+((i-((5*numLEDs)/12))*gradientmultiplier);
                    bluetrend[rainbowarrayindex] = 1;
                }
                leds[i].r = max(min(redbrightmultiplier[rainbowarrayindex],brightness),0);
                leds[i].g = max(min(greenbrightmultiplier[rainbowarrayindex],brightness),0);
                leds[i].b = max(min(bluebrightmultiplier[rainbowarrayindex],brightness),0);
            }
        }
        FastLED.show(); //Write to the LEDs
        rainbowinitflag++; //Now rainbow mode has been initialised, so the program doesn't go back into that intialise for loop
    }
    if (millis()>lighttick+rainbowdelay){ //Only change the LEDs if the delay period has passed
        lighttick = millis(); //Note the time, we want to delay so the rainbow transition isn't too fast
        for(uint8_t i=0; i<numLEDs; i++){ //for all LEDs
            if (numLEDs == numcupLEDs) rainbowarrayindex = i;
            else if (numLEDs == numfoodLEDs) rainbowarrayindex = i+numcupLEDs;
            if (redbrightmultiplier[rainbowarrayindex] == rainbowmax+1 || redbrightmultiplier[rainbowarrayindex] == rainbowmin-1) redtrend[rainbowarrayindex] = redtrend[rainbowarrayindex]*-1; //Reverse the direction of red brightness
            if (greenbrightmultiplier[rainbowarrayindex] == rainbowmax+1 || greenbrightmultiplier[rainbowarrayindex] == rainbowmin-1) greentrend[rainbowarrayindex] = greentrend[rainbowarrayindex]*-1; //Reverse the direction of green brightness change
            if (bluebrightmultiplier[rainbowarrayindex] == rainbowmax+1 || bluebrightmultiplier[rainbowarrayindex] == rainbowmin-1) bluetrend[rainbowarrayindex] = bluetrend[rainbowarrayindex]*-1; //Reverse the direction of blue brightness change
            redbrightmultiplier[rainbowarrayindex] += redtrend[rainbowarrayindex]; //Increase/decrease red brightness by one 
            greenbrightmultiplier[rainbowarrayindex] += greentrend[rainbowarrayindex]; //Increase/decrease red brightness by one 
            bluebrightmultiplier[rainbowarrayindex] += bluetrend[rainbowarrayindex]; //Increase/decrease red brightness by one 
            if (redbrightmultiplier[rainbowarrayindex] >= brightness) leds[i].r = brightness; //If the triangle wave exceeds the max brightness, just set this LED to the maximum brightness
            else if (redbrightmultiplier[rainbowarrayindex] <= 0) leds[i].r = 0; //If the waveform is less than zero, set the LED to zero.
            else leds[i].r = redbrightmultiplier[rainbowarrayindex]; //Otherwise, the waveform brightness is within the acceptable range, set the brightness to this value
            if (greenbrightmultiplier[rainbowarrayindex] >= brightness) leds[i].g = brightness;
            else if (greenbrightmultiplier[rainbowarrayindex] <= 0) leds[i].g = 0;
            else leds[i].g = greenbrightmultiplier[rainbowarrayindex];
            if (bluebrightmultiplier[rainbowarrayindex] >= brightness) leds[i].b = brightness;
            else if (bluebrightmultiplier[rainbowarrayindex] <= 0) leds[i].b = 0;
            else leds[i].b = bluebrightmultiplier[rainbowarrayindex];
        }
        FastLED.show();
    }
}
