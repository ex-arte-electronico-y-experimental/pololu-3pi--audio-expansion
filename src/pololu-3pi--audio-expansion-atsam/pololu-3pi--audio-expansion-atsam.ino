/*******************************************************************************
   %%SECTION_HEADER%%
   This header information is automatically generated by KodeUtils.

   File 'pololu-3pi--audio-expansion-atsam.ino' edited by RileyStarlight, last modified: 2022-03-07.
   This file is part of 'Pololu 3Ppi+ Audio Expansion' package, please see the readme files
   for more information about this file and this package.

   Copyright (C) 2022 by RileyStarlight <riley.garcia@akornsys-rdi.net>
   Released under the GNU General Public License

     This program is free software: you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published by
     the Free Software Foundation, either version 3 of the License, or
     (at your option) any later version.

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
     GNU General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with this program.  If not, see <http://www.gnu.org/licenses/>.

   %%EOS_HEADER%%
 ******************************************************************************/


/*******************************************************************************
   B O A R D   C O N F I G U R A T I O N
   - MCU: ATSAMD21G18
   - Board: Arduino Zero (Native USB Port)
******************************************************************************/


/*******************************************************************************
    G P I O   C O N S T A N T S
 ******************************************************************************/
#define PWRL                    6                   // PWRL on PA20 (physical pin 29), interlink bidir communication.
#define CS                      10                  // CS on PA18 (physical pin 27), SPI chip select.
#define DEBUG                   13                  // DEBUG on PA17 (physical pin 26), debug LED.
#define AN                      A0                  // AN on PA02 (physical pin 3), unconnected.
//I??S on PA07, PA10 and PA11 (physical pin 12, 15, 16).
//SPI on PA12, PB10 and PB11 (physical pin 21, 19, 20).
// Other pins are unused.

/*******************************************************************************
    A P I   C O N S T A N T S
 ******************************************************************************/
#define PWRL_PRE_US             750                 // Microseconds waiting time between pulses.
#define PWRL_PULSE_US           50                  // Microseconds interlink pulse duration.
#define CHECK_ERROR             '#'                 // Initial check error (ATSAMD21 to ATmega32U4).
#define CHECK_OK                '%'                 // Initial check ok (ATSAMD21 to ATmega32U4).
#define ID_REQUEST              '@'                 // Request identifier (ATmega32U4 to ATSAMD21).
#define ID_ANSWER_UNKNOWN       '@'                 // Answer: Unknown identifier (ATSAMD21 to ATmega32U4).
#define ID_ANSWER_FIRST         'A'                 // Answer: Minimum identifier number (ATSAMD21 to ATmega32U4).
#define ID_ANSWER_LAST          '~'                 // Answer: Maximum identifier number (ATSAMD21 to ATmega32U4).
#define REMOTE_PAUSE            '$'                 // Set pause mode (ATSAMD21 to ATmega32U4).
#define REMOTE_RESUME           '&'                 // Set active mode (ATSAMD21 to ATmega32U4).
#define PLAY_A_FILE             '>'                 // Start of A-Type audio playback (ATmega32U4 to ATSAMD21).
#define STOP_A_FILE             '='                 // End of A-Type audio playback (ATmega32U4 to ATSAMD21).
#define PLAY_B_FILE             '!'                 // Start of B-Type audio playback (ATmega32U4 to ATSAMD21).
#define LOW_BATT                '-'                 // Low battery indicator (ATmega32U4 to ATSAMD21).


/*******************************************************************************
    D E F A U L T   C O N S T A N T S
 ******************************************************************************/
#define N_ROBOTS                5                   // Maximum number of simultaneously active robots.
#define VOLUME                  70                  // Playback volume (0-100).
#define USB_DEBUG               false               // If true use usb debugging.
#define PWRL_PULSE_THRESHOLD    5                   // Microsecond threshold for detection of the pulse sent by ATtiny85.
#define PLAY_MIN_REPETITION     1                   // Minimum number of repetitions of A-Type audios.
#define PLAY_MAX_REPETITION     5                   // Maximum number of repetitions of A-Type audios.


/*******************************************************************************
    P L A Y T Y P E   C O N S T A N T S
 ******************************************************************************/
#define PLAY_NONE               0                   // No file in playback. For use with the `type_playing` variable.
#define PLAY_ATYPE              1                   // A-Type playback. For use with the `type_playing` variable.
#define PLAY_BTYPE              2                   // B-Type playback. For use with the `type_playing` variable.
#define PLAY_SPECIAL            3                   // Special playback. For use with the `type_playing` variable.


/*******************************************************************************
    L I B R A R I E S
 ******************************************************************************/
#include <SD.h>

#include <ArduinoSound.h>
SDWaveFile wave_file;

#include <SDConfig.h>
const char config_file[] = "MAIN.CFG";

void setup() {
  pinMode(PWRL, OUTPUT);
  pinMode(CS, OUTPUT);
  pinMode(DEBUG, OUTPUT);
  randomSeed(analogRead(AN));
  Serial.begin(115200);
  SerialUSB.begin(115200);
  //while (!SerialUSB);
}


void loop() {
  static boolean first_run_setup = true;            // Initial setup flag.
  static boolean file_chkok_present = false;        // CHKOK special file exists flag.
  static boolean file_chkko_present = false;        // CHKKO special file exists flag.
  static boolean file_lwbat_present = false;        // LWBAT special file exists flag.
  static boolean usb_debug = false;                 // CONFIG - Configuration variable for serial port debugging.
  static boolean atype_queued = false;              // A-Type file queued for playback.
  static boolean btype_queued = false;              // B-Type file queued for playback.
  static boolean low_battery_queued = false;        // Low battery file queued for playback.
  static boolean stop_atype_command = false;        // Reception of stop play command for A-Type files.
  boolean pwrl = false;                             // PWRL Interlink signal level value.
  static boolean prev_pwrl = false;                 // Previous value of the PWRL Interlink signal.
  static unsigned char volume = 0;                  // CONFIG - Configuration variable for volume adjustment.
  static unsigned char pwrl_pulse_threshold = 0;    // CONFIG - Configuration variable for the detection threshold of the identifier pulse.
  static unsigned char play_min_repetition = 0;     // CONFIG - Configuration variable for the minimum number of repetitions of A-Type audio playbacks.
  static unsigned char play_max_repetition = 0;     // CONFIG - Configuration variable for the maximum number of repetitions of A-Type audio playbacks.
  static unsigned char type_playing = PLAY_NONE;    // Type of file currently playing.
  static unsigned char atype_play_remaining = 0;    // Remaining reproductions for the current A-Type file.
  char c = '\0';                                    // Command character read by serial port (ATmega32U4-ATSAMD21 communication).
  char filename[15] = "";                           // Filename for the current playback.
  static unsigned int file_atype_count = 0;         // Number of A-Type files found on the SD card.
  static unsigned int file_btype_count = 0;         // Number of B-Type files found on the SD card.
  static unsigned int file_atype_selected = 0;      // File number of file A-Type selected for playback.
  unsigned int file_btype_selected = 0;             // File number of file B-Type selected for playback.
  unsigned int pulse_length = 0;                    // Duration in microseconds of the read identifier pulse.
  unsigned int pulse_min = 0;                       // Minimum pulse duration time for identifier.
  unsigned int pulse_max = 0;                       // Maximum pulse duration time for identifier.
  static unsigned int identifier = N_ROBOTS;        // Identifier assigned to the robot.

  if (first_run_setup) {
    if (SD.begin(CS)) {
      digitalWrite(PWRL, HIGH);
      setConfig(&volume, &pwrl_pulse_threshold, &play_min_repetition, &play_max_repetition, &usb_debug);
      AudioOutI2S.volume(volume);
      checkFiles(&file_atype_count, &file_btype_count, &file_chkok_present, &file_chkko_present, &file_lwbat_present, &usb_debug);
      if ((file_atype_count > 0) && (file_btype_count > 0)) {
        if (file_chkok_present) {
          wave_file = SDWaveFile("CHKOK.WAV");
          AudioOutI2S.play(wave_file);
          while (AudioOutI2S.isPlaying());
        }
        Serial.write(CHECK_OK);
        digitalWrite(PWRL, LOW);
        delayMicroseconds(PWRL_PRE_US);
        digitalWrite(PWRL, HIGH);
        delayMicroseconds(PWRL_PULSE_US);
        digitalWrite(PWRL, LOW);
        pinMode(PWRL, INPUT);
        pulse_length = pulseIn(PWRL, 10000);
        for (unsigned int i = 0; i < N_ROBOTS; i++) {
          pulse_min = (360 / N_ROBOTS) + (((360 / N_ROBOTS) * i) - (pwrl_pulse_threshold * ( i + 1)));
          pulse_max = (360 / N_ROBOTS) + (((360 / N_ROBOTS) * i) + (pwrl_pulse_threshold * ( i + 1)));
          if ((pulse_length > pulse_min) && (pulse_length < pulse_max)) {
            identifier = i;
          }
        }
      }
      else {
        Serial.write(CHECK_ERROR);
        if (usb_debug) SerialUSB.print(F("Fatal error: No required files found."));
        if (file_chkko_present) {
          wave_file = SDWaveFile("CHKKO.WAV");
          AudioOutI2S.play(wave_file);
        }
        while (1) {
          digitalWrite(DEBUG, millis() & 0x20);
        }
      }
    }
    else {
      Serial.write(CHECK_ERROR);
      if (usb_debug) SerialUSB.print(F("Fatal error: SD card not found."));
      while (1) {
        digitalWrite(DEBUG, millis() & 0x20);
      }
    }
    first_run_setup = false;
  }
  else {
    // Serial port receiving
    if (Serial.available()) {
      c = Serial.read();
      if (c == ID_REQUEST) {
        if (usb_debug) SerialUSB.println(F("Identifier request."));
        if ((identifier == N_ROBOTS) || (identifier > (ID_ANSWER_LAST - ID_ANSWER_FIRST))) {
          Serial.write(ID_ANSWER_UNKNOWN);
          if (usb_debug) SerialUSB.println(F("Identifier answer: Unknown."));
        }
        else {
          Serial.write(ID_ANSWER_FIRST + identifier);
          if (usb_debug) SerialUSB.print(F("Identifier answer: "));
          if (usb_debug) SerialUSB.println(identifier);
        }
      }
      else if (c == PLAY_A_FILE) {
        if (usb_debug) SerialUSB.println(F("Play queued - A-Type."));
        atype_queued = true;
      }
      else if (c == STOP_A_FILE) {
        if (usb_debug) SerialUSB.println(F("Stop play received."));
        stop_atype_command = true;
      }
      else if (c == PLAY_B_FILE) {
        if (usb_debug) SerialUSB.println(F("Play queued - B-Type."));
        btype_queued = true;
      }
      else if (c == LOW_BATT) {
        if (usb_debug) SerialUSB.println(F("Play queued - Low battery."));
        low_battery_queued = true;
      }
    }
    // Remote mode management
    pwrl = digitalRead(PWRL);
    if (pwrl != prev_pwrl) {
      if (pwrl) {
        Serial.write(REMOTE_RESUME);
        if (usb_debug) SerialUSB.println(F("REMOTE: Resume received."));
      }
      else {
        Serial.write(REMOTE_PAUSE);
        if (usb_debug) SerialUSB.println(F("REMOTE: Pause received."));
      }
      prev_pwrl = pwrl;
    }
    // Playback queue handling
    if (AudioOutI2S.isPlaying()) {
      digitalWrite(DEBUG, HIGH);
      if (stop_atype_command) {
        if (type_playing == PLAY_ATYPE) {
          AudioOutI2S.stop();
          if (usb_debug) SerialUSB.println(F("Stopping playback."));
        }
        else {
          if (usb_debug) SerialUSB.println(F("Ignoring playback stop."));
        }
        stop_atype_command = false;
      }
      else if ((atype_queued) && (type_playing != PLAY_SPECIAL)) {
        if (usb_debug) SerialUSB.println(F("Ignoring A-Type playback."));
        atype_queued = false;
      }
      else if ((btype_queued) && (type_playing != PLAY_SPECIAL)) {
        if (usb_debug) SerialUSB.println(F("Ignoring B-Type playback."));
        btype_queued = false;
      }
    }
    else {
      digitalWrite(DEBUG, LOW);
      type_playing = PLAY_NONE;
      if (low_battery_queued) {
        if (file_lwbat_present) {
          wave_file = SDWaveFile("LWBAT.WAV");
          AudioOutI2S.play(wave_file);
          if (usb_debug) SerialUSB.println(F("Playing low battery."));
        }
        type_playing = PLAY_SPECIAL;
        low_battery_queued = false;
      }
      else if (atype_queued) {
        if (atype_play_remaining == 0) {
          file_atype_selected = random(0, file_atype_count - 1);
          atype_play_remaining = random(play_min_repetition, play_max_repetition);
        }
        atype_play_remaining--;
        composeName('A', file_atype_selected, filename);
        wave_file = SDWaveFile(filename);
        AudioOutI2S.loop(wave_file);
        if (usb_debug) SerialUSB.print(F("Playing: "));
        if (usb_debug) SerialUSB.println(filename);
        if (usb_debug) SerialUSB.print(F("Remaining plays: "));
        if (usb_debug) SerialUSB.println(atype_play_remaining);
        type_playing = PLAY_ATYPE;
        atype_queued = false;
      }
      else if (btype_queued) {
        file_btype_selected = random(0, file_btype_count - 1);
        composeName('B', file_btype_selected, filename);
        wave_file = SDWaveFile(filename);
        AudioOutI2S.play(wave_file);
        if (usb_debug) SerialUSB.print(F("Playing: "));
        if (usb_debug) SerialUSB.println(filename);
        type_playing = PLAY_BTYPE;
        btype_queued = false;
      }
      else if (stop_atype_command) {
        if (usb_debug) SerialUSB.println(F("Ignoring playback stop."));
        stop_atype_command = false;
      }
    }
  }
}


// Tries to read the SD configuration, and if it cannot, set the default configuration.
// Required arguments:
//      vol - Pointer to variable in which to store the configuration for the volume.
//      pulse - Pointer to variable in which to store the configuration for the PWRL pulse threshold.
//      min_rep - Pointer to variable in which to store the configuration for the minimum number of repetitions of A-Type audios.
//      max_rep - Pointer to variable in which to store the configuration for the maximum number of repetitions of A-Type audios.
//      debug - Pointer to variable in which to store the configuration for serial port debugging.
void setConfig(unsigned char *vol, unsigned char *pulse, unsigned char *min_rep, unsigned char *max_rep, boolean *debug) {
  boolean volume_present = false;
  boolean pulse_threshold_present = false;
  boolean min_repetition_present = false;
  boolean max_repetition_present = false;
  boolean debug_present = false;
  unsigned char max_line_length = 127;
  SDConfig cfg;

  if (!cfg.begin(config_file, max_line_length)) {
    if (*debug) SerialUSB.print(F("Failed to open configuration file: "));
    if (*debug) SerialUSB.println(config_file);
  }
  else {
    if (*debug) SerialUSB.println(F("Configuration file found. Reading configuration..."));
    while (cfg.readNextSetting()) {
      if (cfg.nameIs("volume")) {
        volume_present = true;
        *vol = cfg.getIntValue();
      }
      else if (cfg.nameIs("pwrl_pulse_threshold")) {
        pulse_threshold_present = true;
        *pulse = cfg.getIntValue();
      }
      else if (cfg.nameIs("play_min_repetition")) {
        min_repetition_present = true;
        *min_rep = cfg.getIntValue();
      }
      else if (cfg.nameIs("play_max_repetition")) {
        max_repetition_present = true;
        *max_rep = cfg.getIntValue();
      }
      else if (cfg.nameIs("usb_debug")) {
        debug_present = true;
        *debug = cfg.getBooleanValue();
      }
      else {
        if (*debug) SerialUSB.print(F("Unknown configuration property: "));
        if (*debug) SerialUSB.println(cfg.getName());
      }
    }
    cfg.end();
  }
  if (*debug) SerialUSB.println("");
  // VOLUME
  if (volume_present) {
    if (*debug) SerialUSB.print(F("[CONFIGFILE] Volume = "));
  }
  else {
    *vol = VOLUME;
    if (*debug) SerialUSB.print(F("[DEFAULT] Volume = "));
  }
  if (*debug) SerialUSB.println(*vol);
  // PWRL_PULSE_THRESHOLD
  if (pulse_threshold_present) {
    if (*debug) SerialUSB.print(F("[CONFIGFILE] Interlink pulse threshold = "));
  }
  else {
    *pulse = PWRL_PULSE_THRESHOLD;
    if (*debug) SerialUSB.print(F("[DEFAULT] Interlink pulse threshold = "));
  }
  if (*debug) SerialUSB.println(*pulse);
  // PLAY_MIN_REPETITION
  if (min_repetition_present) {
    if (*debug) SerialUSB.print(F("[CONFIGFILE] Minimum playback repetitions = "));
  }
  else {
    *min_rep = PLAY_MIN_REPETITION;
    if (*debug) SerialUSB.print(F("[DEFAULT] Minimum playback repetitions = "));
  }
  if (*debug) SerialUSB.println(*min_rep);
  // PLAY_MAX_REPETITION
  if (max_repetition_present) {
    if (*debug) SerialUSB.print(F("[CONFIGFILE] Maximum playback repetitions = "));
  }
  else {
    *max_rep = PLAY_MAX_REPETITION;
    if (*debug) SerialUSB.print(F("[DEFAULT] Maximum playback repetitions = "));
  }
  if (*debug) SerialUSB.println(*max_rep);
  //USB_DEBUG
  if (debug_present) {
    if (*debug) SerialUSB.print(F("[CONFIGFILE] USB Debug = "));
  }
  else {
    *debug = USB_DEBUG;
    if (*debug) SerialUSB.print(F("[DEFAULT] USB Debug = "));
  }
  if (*debug) SerialUSB.println(*debug);
  if (*debug) SerialUSB.println("");
}


// Checks the SD for playable audio files.
// Required arguments:
//      atype_count - Pointer to variable in which to store how many files of A-Type have been found.
//      btype_count - Pointer to variable in which to store how many files of B-Type have been found.
//      ok_present - Pointer to variable in which to store if a special audio exists for initial check OK.
//      ko_present - Pointer to variable in which to store if a special audio exists for initial check error.
//      lw_present - Pointer to variable in which to store if a special audio exists for low battery.
//      debug - Pointer to a variable indicating if debugging is available via serial port.
void checkFiles(unsigned int *atype_count, unsigned int *btype_count, boolean *ok_present, boolean *ko_present, boolean *lw_present, boolean *debug) {
  boolean file_valid = false;
  char file_name[15] = "";
  unsigned int counter = 0;

  *atype_count = 0;
  if (SD.exists("A000.WAV")) {
    composeName('A', counter, file_name);
    do {
      file_valid = false;
      if (SD.exists(file_name)) {
        wave_file = SDWaveFile(file_name);
        if (wave_file) {
          if (AudioOutI2S.canPlay(wave_file)) {
            file_valid = true;
            counter++;
            composeName('A', counter, file_name);
          }
        }
      }
    } while (file_valid);
    *atype_count = counter;
    if (*debug) SerialUSB.print(F("Total number of A-Type files found: "));
    if (*debug) SerialUSB.println(*atype_count);
  }
  else if (*debug) SerialUSB.println(F("No A-Type audio could be found"));
  counter = 0;
  *btype_count = 0;
  if (SD.exists("B000.WAV")) {
    composeName('B', counter, file_name);
    do {
      file_valid = false;
      if (SD.exists(file_name)) {
        wave_file = SDWaveFile(file_name);
        if (wave_file) {
          if (AudioOutI2S.canPlay(wave_file)) {
            file_valid = true;
            counter++;
            composeName('B', counter, file_name);
          }
        }
      }
    } while (file_valid);
    *btype_count = counter;
    if (*debug) SerialUSB.print(F("Total number of B-Type files found: "));
    if (*debug) SerialUSB.println(*btype_count);
  }
  else if (*debug) SerialUSB.println(F("No B-Type audio could be found"));
  *ok_present = false;
  if (SD.exists("CHKOK.WAV")) {
    wave_file = SDWaveFile("CHKOK.WAV");
    if (wave_file) {
      if (AudioOutI2S.canPlay(wave_file)) {
        *ok_present = true;
        if (*debug) SerialUSB.println(F("CHKOK.WAV File is present"));
      }
      else if (*debug) SerialUSB.println(F("CHKOK.WAV File is not playable"));
    }
    else if (*debug) SerialUSB.println(F("CHKOK.WAV File is invalid"));
  }
  else if (*debug) SerialUSB.println(F("CHKOK.WAV File could not be found"));
  *ko_present = false;
  if (SD.exists("CHKKO.WAV")) {
    wave_file = SDWaveFile("CHKKO.WAV");
    if (wave_file) {
      if (AudioOutI2S.canPlay(wave_file)) {
        *ko_present = true;
        if (*debug) SerialUSB.println(F("CHKKO.WAV File is present"));
      }
      else if (*debug) SerialUSB.println(F("CHKKO.WAV File is not playable"));
    }
    else if (*debug) SerialUSB.println(F("CHKKO.WAV File is invalid"));
  }
  else if (*debug) SerialUSB.println(F("CHKKO.WAV File could not be found"));
  *lw_present = false;
  if (SD.exists("LWBAT.WAV")) {
    wave_file = SDWaveFile("LWBAT.WAV");
    if (wave_file) {
      if (AudioOutI2S.canPlay(wave_file)) {
        *lw_present = true;
        if (*debug) SerialUSB.println(F("LWBAT.WAV File is present"));
      }
      else if (*debug) SerialUSB.println(F("LWBAT.WAV File is not playable"));
    }
    else if (*debug) SerialUSB.println(F("LWBAT.WAV File is invalid"));
  }
  else if (*debug) SerialUSB.println(F("LWBAT.WAV File could not be found"));
}


// Generates a file name from type and number.
// Required arguments:
//      type - Audio type (`A` or `B`).
//      count - File number (0- 999).
//      buf - Variable used to generate the resulting file name.
void composeName(char type, unsigned int count, char buf[]) {
  if (count < 1000) {
    if ((type == 'a') || (type == 'A')) {
      sprintf(buf, "A%03d.WAV", count);
    }
    else if ((type == 'b') || (type == 'B')) {
      sprintf(buf, "B%03d.WAV", count);
    }
    else buf[0] = '\0';
  }
  else buf[0] = '\0';
}
