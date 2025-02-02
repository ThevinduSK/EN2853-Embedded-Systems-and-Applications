int summation = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  summation = local_addition(9,4);
  Serial.print("Local summation: ");
  Serial.println(summation);
  delay(1000);

  global_addition(8,2);
  Serial.print("Global summation: ");
  Serial.println(summation);
}

int local_addition(int a, int b)
{
  // creating local variable
  int sum = a + b;
  return sum;
}

void global_addition(int a, int b)
{
  //accesing the global variable
  summation = a + b;
}