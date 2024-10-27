/* 
  Starting point for adding comms between a rPI-Arduino
  link over serial comms. 

   Notes:
   - Arduino sets up serial port for comms
   - Commands (function + args) can be passed as JSON over serial
   - New commands must be registered (see below #include) to be called remotely
   - Arduino will send JSON response for:
      - Execution completed successfully (status code 0)
      - Valid function, beginning execution (status code 1)
      - RX'ing invalid JSON (status code 2)
      - RX'ing valid JSON, but invalid args (status code 3-13)
      - RX'ing valid JSON, but function not registered (status code 14)
      - Status code 15 + 16 reserved :)
      - Completing command execution w/ error (status <16, depending on fxn err)
      - Encountering an unexpected error (no response?)
*/

#include <ArduinoJson.h>
#include <stdio.h>
#define HWSERIAL Serial1

bool DEBUG = false;

/* 
  To register a new function for remote calling:
  - add pointer to functions[] list
  - add keyword to lookupFxnIndex(), map to same index as position in functions[]
  - implement function to match FunctionCallback typedef
*/

// function type
typedef int (*FunctionCallback)(int, int, int, int, float, float, const char*, const char*);
FunctionCallback functions[] = {&move_forward, &move_backward, &rotate, &do_nothing};

// function LuT
int lookupFxnIndex(const char* function){
  int fxnIndex = -1;
  if (strcmp(function,"move_forward()") == 0) {
    fxnIndex = 0;
  } else if (strcmp(function,"move_backward()") == 0) {
    fxnIndex = 1;
  } else if (strcmp(function,"rotate()") == 0) {
    fxnIndex = 2;
  } else if (strcmp(function,"do_nothing()") == 0) {
    fxnIndex = 3;
  } else {
    fxnIndex = -1;
    if (DEBUG) {
      Serial.print("Function not registered: ");
      Serial.println(function);
    }
  }

  if (fxnIndex > -1) {
      if (DEBUG) {
        Serial.print("Function registered: ");
        Serial.println(function);
      }
  }

  return fxnIndex;
}

void setup() {  
  // Initialize "debug" serial port
  Serial.begin(9600);
  HWSERIAL.begin(115200);
  Serial.println("Serial INIT'ed");

}

void loop() {
  // Check if the rPI is transmitting
  if (HWSERIAL.available() > 0) 
  // if (Serial.available() > 0) 
  { 
    unsigned long start = micros();
    int cmdID = -1;
    int status = -1;

    if (DEBUG) {
      Serial.print("Serial RX'ed\n");
    }

    // Allocate the JSON documents
    JsonDocument docRX;
    JsonDocument docTX;

    // Read the JSON document from the "link" serial port
    DeserializationError err = deserializeJson(docRX, HWSERIAL);

    if (err == DeserializationError::Ok) 
    {
      Serial.print("ARD01 <-- ");
      serializeJson(docRX, Serial); // Echo for visibility
      Serial.print("\n");
      
      if (DEBUG) {
      Serial.print("Valid JSON decoded\n");
      }
      // Translate json to function call
      status = handle_remote_call(docRX, docTX);
      // TODO: send json return to rPI with status
    } 
    else 
    {
      if (DEBUG) {
        Serial.print("deserializeJson() returned ");
        Serial.println(err.c_str());
      }
      status = 2;

      // Flush all bytes in the "link" serial port buffer
      while (HWSERIAL.available() > 0) {
        HWSERIAL.read();
      }
      // Flush all bytes in the "link" serial port buffer
      while (Serial.available() > 0){
        Serial.read();
      }
    }

    // Compute the time it took
    unsigned long end = micros();
    unsigned long delta = end - start;
    if (DEBUG) {
      Serial.print("execution time (us): ");
      Serial.println(delta);
    }
    // Create the response JSON document
    cmdID = docRX["cmdID"];
    docTX["cmdID"] = cmdID; 
    docTX["t_exec_us"] = delta;
    docTX["status"] = status;

    // Send the JSON document over the "link" serial port
    Serial.print("ARD01 --> ");
    serializeJson(docTX, Serial); // Send status response (DEBUG)
    serializeJson(docTX, HWSERIAL); // Send status response (LINK)
    HWSERIAL.print("|");
    Serial.print("\n");

  }
}

int handle_remote_call(JsonDocument docRX, JsonDocument docTX){
  int status;
  
  // JSON input example:
  // {
  //  "function": "move_forward()",
  //  "cmdID": 8351824120,
  //  "intArg0": 420,
  //  "intArg0": 69,
  //  "intArg2": 0,
  //  "intArg3": -1,
  //  "floatArg0": 1.1,
  //  "floatArg1": 2.2,
  //  "strArg0": "abc",
  //  "strArg1": "def",
  //  }

  // Required args
  const char* function = docRX["function"];
  int cmdID = docRX["cmdID"];
  int intArg0 = docRX["intArg0"];
  int intArg1 = docRX["intArg1"];
  int intArg2 = docRX["intArg2"];
  int intArg3 = docRX["intArg3"];
  float floatArg0 = docRX["floatArg0"];
  float floatArg1 = docRX["floatArg1"];
  const char* strArg0 = docRX["strArg0"];
  const char* strArg1 = docRX["strArg1"];
  
  if (DEBUG) {
    //DEBUG
    Serial.print("Interpreted header:\n");
    
    Serial.print("\tfunction name = ");
    Serial.println(function);

    Serial.print("\tcmdID = ");
    Serial.println(cmdID);
  }

  // check that function exists in LuT
  int fxnIndex = lookupFxnIndex(function);
  
  if (fxnIndex == -1){ 
    status = 14;
  } else {
    
    // send ACK for function execution
    status = 1;
    docTX["cmdID"] = docRX["cmdID"];
    docTX["status"] = 1;
    Serial.print("ARD01 --> ");
    serializeJson(docTX, Serial); // ACK valid command (DEBUG) 
    serializeJson(docTX, HWSERIAL); // ACK valid command (LINK)
    HWSERIAL.print("|");
    Serial.print("\n");
    docTX.clear();

    // call function
    status = functions[fxnIndex](intArg0, intArg1, intArg2, intArg3, floatArg0, floatArg1, strArg0, strArg1);

  }
  
  // get return code from function and return it
  
  return status;
}

int move_forward(int speed, int stopType, int, int, float stopThresh, float, const char*, const char*){

  // {"function": "move_forward()", "cmdID": 83518, "intArg0": 90, "intArg1": 1, "intArg2": -1, "intArg3": -1, "floatArg0": 0.40, "floatArg1": -1.0, "strArg0": "", "strArg1": ""}
  if (DEBUG) {
    Serial.print("Successfully entered: move_forward()\n");

    Serial.print("Passed args:\n");
    Serial.print("\tspeed = ");
    Serial.println(speed);

    Serial.print("\tstopType = ");
    Serial.println(stopType);

    Serial.print("\tstopThresh = ");
    Serial.println(stopThresh);
  }

  int status = 0;
  return status;
}

int move_backward(int speed, int stopType, int, int, float stopThresh, float, const char*, const char*){
  
  // {"function": "move_backward()", "cmdID": 83518, "intArg0": 30, "intArg1": 1, "intArg2": -1, "intArg3": -1, "floatArg0": 0.40, "floatArg1": -1.0, "strArg0": "", "strArg1": ""}
  if (DEBUG) {
    Serial.print("Successfully entered: move_backward()\n");

    Serial.print("Passed args:\n");
    Serial.print("\tspeed = ");
    Serial.println(speed);

    Serial.print("\tstopType = ");
    Serial.println(stopType);

    Serial.print("\tstopThresh = ");
    Serial.println(stopThresh);
  }

  int status = 0;
  return status;
}

int rotate(int degrees, int, int, int, float, float, const char*, const char*){
  
  // {"function": "rotate()", "cmdID": 83518, "intArg0": 90, "intArg1": -1, "intArg2": -1, "intArg3": -1, "floatArg0": -1.0, "floatArg1": -1.0, "strArg0": "", "strArg1": ""}
  if (DEBUG) {
    Serial.print("Successfully entered: rotate()\n");

    Serial.print("Passed args:\n");
    Serial.print("\tdegrees = ");
    Serial.println(degrees);
  }

  int status = 0;
  return status;
}

int do_nothing(int, int, int, int, float, float, const char*, const char*){
  
  // {"function": "do_nothing()", "cmdID": 83518, "intArg0": -1, "intArg1": -1, "intArg2": -1, "intArg3": -1, "floatArg0": -1.0, "floatArg1": -1.0, "strArg0": "", "strArg1": ""}
  if (DEBUG) {
    Serial.print("Successfully entered: do_nothing()\n");
    Serial.print("Exiting do_nothing() with return code 0\n");
  }

  int status = 0;
  return status;
}