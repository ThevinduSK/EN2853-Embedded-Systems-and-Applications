#define Buzzer 13

int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;

int notes[] = {C,D,E,F,G,A,B,C_H};

int n_notes = 8;

void setup(){
  //put your setup code here, to run once;
  pinMode(Buzzer, OUTPUT);

  for (int i = 0; i < n_notes; i++){
    tone(Buzzer, notes[i]);
    delay(500);
    noTone(Buzzer);
    delay(2);
  }
}

void loop(){
  // put your main code here, to run repeatedly:
  //delay(10); //this speeds up the simulation
}