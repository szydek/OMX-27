#define NUM_STEPS 16

// the MIDI channel number to send messages
const int midiChannel = 1;

int currentTick = 0;            // A tick of the clock
bool clockSource = 0;     // Internal clock (0), external clock (1)
bool playing = 0;         // Are we playing?
bool paused = 0;          // Are we paused?
bool stopped = 1;         // Are we stopped? (Must init to 1)
byte songPosition = 0;    // A place to store the current MIDI song position
bool seqLedRefresh = 1;   // Should we refresh the LED array?
int playingPattern = 0;  // The currently playing pattern, 0-7
byte patternAmount = 1;   // How many patterns will play


word stepCV;
int seq_velocity = 100;
int seq_acc_velocity = 127;

// int lastNote[8] = {0, 0, 0, 0, 0, 0, 0, 0};            // A place to remember the last MIDI note we played
int seqPos[8] = {0, 0, 0, 0, 0, 0, 0, 0};				// What position in the sequence are we in?
bool patternMute[8] = {false, false, false, false, false, false, false, false};     
int patternLength[8] = {16, 16, 16, 16, 16, 16, 16, 16};
int patternStart[8] = {0, 0, 0, 0, 0, 0, 0, 0};
int pattLen[8] = {patternLength[0],patternLength[1],patternLength[2],patternLength[3],patternLength[4],patternLength[5],patternLength[6],patternLength[7]};

volatile unsigned long step_micros; 

const byte max_tracks = 8;
typedef struct pattern_specs {
	int channel;
	int length;
	float mult;
	int start;
	bool mute;
  } pattern_specs;
  pattern_specs pattern[max_tracks];

//{notenum,vel,len,p1,p2,p3,p4,p5}
const unsigned int max_notes = 64;
typedef struct note_type {
	int note;
	int velocity;
	unsigned long length;     
	int channel;      
	int plock[5];      
	unsigned long timestamp;
	unsigned int id;
	bool isplaying;
  }  note_type;
  note_type noteStack[max_notes];

int noteStack_tail = 0;
int noteStack_head = 0;  
int noteStack_count = 0;  

bool inline noteStackFull() {return noteStack_count == max_notes; };
bool inline noteStackEmpty() {return noteStack_count == 0; };
int inline  noteStackNext(int eventIndex) {return (eventIndex+1)%max_notes; };


// bool plocks[16];

// Determine how to play a step
// -1: restart
// 0: mute
// 1: play
// 2: accent

int stepPlay[8][16] = {
  {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

// Slide note: 1
// Normal note: 0
bool stepSlide[8][16] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

// The MIDI values of the notes in each step.
// C3 = 40
int stepNote[8][16] = {
  {60, 60, 62, 64, 62, 62, 60, 60, 60, 62, 72, 65, 65, 72, 60, 72},
  {40, 40,  0,  0, 42, 42, 43, 40, 40,  0,  0, 38, 38,  0, 40, 11},
  {60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60},
  {36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36},
  {38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38},
  {44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44},
  {46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46},
  {39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39}
};
int stepNoteP[8][16][8] = { 		// {notenum,vel,len,p1,p2,p3,p4,p5}
	{ {60, 100, 1, -1, -1, -1, -1, -1}, {62, 100, 1, -1, -1, -1, -1, -1},{60, 100, 1, -1, -1, -1, -1, -1},{64, 100, 1, -1, -1, -1, -1, -1},{63, 100, 1, -1, -1, -1, -1, -1},{63, 100, 1, -1, -1, -1, -1, -1},{60, 100, 1, -1, -1, -1, -1, -1},{60, 100, 1, -1, -1, -1, -1, -1},{60, 100, 1, -1, -1, -1, -1, -1},{62, 100, 1, -1, -1, -1, -1, -1},{72, 100, 1, -1, -1, -1, -1, -1},{65, 100, 1, -1, -1, -1, -1, -1},{65, 100, 1, -1, -1, -1, -1, -1},{72, 100, 1, -1, -1, -1, -1, -1},{60, 100, 1, -1, -1, -1, -1, -1},{72, 100, 1, -1, -1, -1, -1, -1} },
	{ {48, 100, 1, -1, -1, -1, -1, -1}, {48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{63, 100, 1, -1, -1, -1, -1, -1},{63, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{65, 100, 1, -1, -1, -1, -1, -1},{65, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1} },
	{ {48, 100, 1, -1, -1, -1, -1, -1}, {48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{63, 100, 1, -1, -1, -1, -1, -1},{63, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{65, 100, 1, -1, -1, -1, -1, -1},{65, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1} },
	{ {48, 100, 1, -1, -1, -1, -1, -1}, {48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{63, 100, 1, -1, -1, -1, -1, -1},{63, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{65, 100, 1, -1, -1, -1, -1, -1},{65, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1} },
	{ {48, 100, 1, -1, -1, -1, -1, -1}, {48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{63, 100, 1, -1, -1, -1, -1, -1},{63, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{65, 100, 1, -1, -1, -1, -1, -1},{65, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1} },
	{ {48, 100, 1, -1, -1, -1, -1, -1}, {48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{63, 100, 1, -1, -1, -1, -1, -1},{63, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{65, 100, 1, -1, -1, -1, -1, -1},{65, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1} },
	{ {48, 100, 1, -1, -1, -1, -1, -1}, {48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{63, 100, 1, -1, -1, -1, -1, -1},{63, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{65, 100, 1, -1, -1, -1, -1, -1},{65, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1} },
	{ {48, 100, 1, -1, -1, -1, -1, -1}, {48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{63, 100, 1, -1, -1, -1, -1, -1},{63, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{65, 100, 1, -1, -1, -1, -1, -1},{65, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1},{48, 100, 1, -1, -1, -1, -1, -1} }
};

int lastNote[8][16] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

int stepVelocity[8][16] = {
  {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
  {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
  {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
  {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
  {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
  {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
  {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
  {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100}
};

int stepLength[8][16] = {
  {2, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
};

void seqStart() {
  playing = 1;
  paused = 0;
  stopped = 0;
}

void seqContinue() {
  playing = 1;
  paused = 0;
  stopped = 0;
}

void seqPause() {
  playing = 0;
  paused = 1;
  stopped = 0;
}

void seqStop() {
  currentTick = 0;
  // seqPos = 0;
  playing = 0;
  paused = 0;
  stopped = 1;
  seqLedRefresh = 1;
}

///// 

void note_off(int midiNote, int midiChan){
 	Serial.print(midiNote);
	Serial.println(" note off");
		usbMIDI.sendNoteOff(midiNote, 0, midiChan);
		MIDI.sendNoteOff(midiNote, 0, midiChan);
		analogWrite(CVPITCH_PIN, 0);
		digitalWrite(CVGATE_PIN, LOW);
}

////turn off notes that have been on for more than noteLength
void turnNotesOff() {
// 	unsigned long nowTime = micros();

	for (unsigned int i = 0; i < max_notes; i = i+1) {
		if (noteStack[i].isplaying){
// 		if (noteStack[i].id == i){

// 			Serial.print("id: ");
// 			Serial.println(noteStack[i].id);
// 			Serial.print("timestamp: ");
// 			Serial.println(noteStack[i].timestamp);
// 			Serial.print("nowTime: ");
// 			Serial.println(micros());
// 			Serial.print("adj: ");
// 			Serial.println(micros() - noteStack[i].timestamp);

			if ((noteStack_count > 0) && (micros() - noteStack[i].timestamp) >= noteStack[i].length) {
				note_off(noteStack[i].note, noteStack[i].channel);
				noteStack[i].isplaying = false;
				noteStack_count -= 1;
				noteStack_tail = (noteStack_tail+1) % max_notes;
			}
		}	
// 		}	
	}
}
//
////check if note has been played recently without being turned off
int isNotePlaying(int playingNote) {
	for (int i = noteStack_tail, count=noteStack_count; count>0; i = (i+1)%max_notes, count=count-1) {
		if (noteStack[i].note == playingNote ) return 1;
	}
	return 0;
}

/*
void addEvent(int newNote, int newVel, int newChan, unsigned long newLen, int p1, int p2, int p3, int p4, int p5, unsigned long tmstmp) {
	if (noteStackFull()) { // Buffer full, remove oldest, noteStack_tail 
		noteStack_count -= 1;
		noteStack_tail = (noteStack_tail+1) % max_notes; 
	}
	Serial.print("add note ");
	Serial.println(newNote);

	noteStack[noteStack_head].note = newNote;
	noteStack[noteStack_head].velocity = newVel;
	noteStack[noteStack_head].length = newLen;
	noteStack[noteStack_head].channel = newChan;
	noteStack[noteStack_head].plock[0] = p1;
	noteStack[noteStack_head].plock[1] = p2;
	noteStack[noteStack_head].plock[2] = p3;
	noteStack[noteStack_head].plock[3] = p4;
	noteStack[noteStack_head].plock[4] = p5;
	noteStack[noteStack_head].timestamp = tmstmp;
	noteStack[noteStack_head].id = noteStack_head;
	noteStack[noteStack_head].isplaying = false;

	noteStack_count += 1;	
	noteStack_head=(noteStack_head+1)%max_notes;

}
*/
void addEvent(note_type eventStack) {
	if (noteStackFull()) { /* Buffer full, remove oldest, noteStack_tail */
		noteStack_count -= 1;
		noteStack_tail = (noteStack_tail+1) % max_notes; 
	}

	noteStack[noteStack_head] = eventStack;

	noteStack_count += 1;	
	noteStack_head=(noteStack_head+1)%max_notes;

}

void playStep(note_type thisStack) {

	seq_velocity = thisStack.velocity;
	Serial.print("seq_velocity ");
	Serial.println(seq_velocity);
	usbMIDI.sendNoteOn(thisStack.note, thisStack.velocity, thisStack.channel);
	MIDI.sendNoteOn(thisStack.note, thisStack.velocity, thisStack.channel);
	//Serial.println();

	// send param locks // {notenum,vel,len,p1,p2,p3,p4,p5}
	for (int q=0; q<4; q++){	
		int tempCC = thisStack.plock[q];
		if (tempCC > -1) {
			usbMIDI.sendControlChange(pots[q],tempCC,thisStack.channel);
			prevPlock[q] = tempCC;
		} else if (prevPlock[q] != potValues[q]) {
			usbMIDI.sendControlChange(pots[q],potValues[q],thisStack.channel);
			prevPlock[q] = potValues[q];
		}
	}
	stepCV = map (thisStack.note, 35, 90, 0, 4096);
	digitalWrite(CVGATE_PIN, HIGH);
	analogWrite(CVPITCH_PIN, stepCV);

}
// Play a note
void playNote(int patternNum) {
  //Serial.println(stepNoteP[patternNum][seqPos][0]); // Debug

  switch (stepPlay[patternNum][seqPos[patternNum]]) {
    case -1:
      // Skip the remaining notes
      seqPos[patternNum] = 15;
      break;
    case 0:
      // Don't play a note
      // Turn off the previous note
//       usbMIDI.sendg(lastNote[patternNum], 0, midiChannel);
//       MIDI.sendNoteOff(lastNote[patternNum], 0, midiChannel);
//       analogWrite(CVPITCH_PIN, 0);
//       digitalWrite(CVGATE_PIN, LOW);
      break;

    case 1:	// regular note on
      // Turn off the previous note and play a new note.
//       usbMIDI.sendNoteOff(lastNote[patternNum], 0, midiChannel);
//       MIDI.sendNoteOff(lastNote[patternNum], 0, midiChannel);
//       analogWrite(CVPITCH_PIN, 0);

// stepNoteP[playingPattern][selectedStep][1]
		seq_velocity = stepNoteP[playingPattern][seqPos[patternNum]][1];
		usbMIDI.sendNoteOn(stepNoteP[patternNum][seqPos[patternNum]][0], seq_velocity, midiChannel);
		MIDI.sendNoteOn(stepNoteP[patternNum][seqPos[patternNum]][0], seq_velocity, midiChannel);
		//Serial.println(stepNoteP[patternNum][seqPos[patternNum]]);

		// send param locks // {notenum,vel,len,p1,p2,p3,p4,p5}
		for (int q=0; q<4; q++){	
			int tempCC = stepNoteP[patternNum][seqPos[patternNum]][q+3];
			if (tempCC > -1) {
				usbMIDI.sendControlChange(pots[q],tempCC,midiChannel);
				prevPlock[q] = tempCC;
			} else if (prevPlock[q] != potValues[q]) {
				//if (tempCC != prevPlock[q]) {
				usbMIDI.sendControlChange(pots[q],potValues[q],midiChannel);
				prevPlock[q] = potValues[q];
			}
		}
		lastNote[patternNum][seqPos[patternNum]] = stepNoteP[patternNum][seqPos[patternNum]][0];
		stepCV = map (lastNote[patternNum][seqPos[patternNum]], 35, 90, 0, 4096);
		digitalWrite(CVGATE_PIN, HIGH);
		analogWrite(CVPITCH_PIN, stepCV);
      break;

    case 2:		 // accented note on - NOT USED?
      // Turn off the previous note, and play a new accented note
//       usbMIDI.sendNoteOff(lastNote[patternNum], 0, midiChannel);
//       MIDI.sendNoteOff(lastNote[patternNum], 0, midiChannel);
//       analogWrite(CVPITCH_PIN, 0);
      
		usbMIDI.sendNoteOn(stepNoteP[patternNum][seqPos[patternNum]][0], seq_acc_velocity, midiChannel);
		MIDI.sendNoteOn(stepNoteP[patternNum][seqPos[patternNum]][0], seq_acc_velocity, midiChannel);
		lastNote[patternNum][seqPos[patternNum]] = stepNoteP[patternNum][seqPos[patternNum]][0];
      	stepCV = map (lastNote[patternNum][seqPos[patternNum]], 35, 90, 0, 4096);
      	digitalWrite(CVGATE_PIN, HIGH);
      	analogWrite(CVPITCH_PIN, stepCV);
      break;
  }
}
void allNotesOff() {
	analogWrite(CVPITCH_PIN, 0);
	digitalWrite(CVGATE_PIN, LOW);
	for (int j=0; j<128; j++){
		usbMIDI.sendNoteOff(j, 0, midiChannel);
		MIDI.sendNoteOff(j, 0, midiChannel);
	}
}