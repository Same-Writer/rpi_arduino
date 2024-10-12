import serial
import time
import json
import re
import random

MAX_CMDID = 99999

class arduino_control():
     
    def __init__(self):
        self.hwport='/dev/ttyS0'
        self.baudrate = 115200
        self.serial_timeout = .1
        self.arduino = serial.Serial(port=self.hwport, baudrate=self.baudrate, timeout=self.serial_timeout) 

### Internal functions

    def _generate_cmdid(self) -> int:
        rand_id = random.randrange(MAX_CMDID)
        return int(rand_id)
    
    def _init_cmd_dict(self, function_name: str) -> dict:
        cmd_dict = {}
        cmd_dict["function"] = str(function_name + "()")
        cmd_dict["cmdID"] = int(self._generate_cmdid())

        return cmd_dict
    
    def _send_command_to_arduino(self, cmd: dict) -> list:
        # TODO: check that arduino is connected first?

        cmd = str(cmd).replace("\'", "\"")
        print("\tARD01 <-- " + str(cmd))
        self.arduino.write(cmd.encode())

        response = self._read_response_from_arduino()

        for frame in response:
            if frame["status"] == 1: # status 1 is a command ACK
                print("\tARD01 --> " + str(frame))
                response.remove(frame)
                break 
            elif frame["status"] == 14: # status 14 is a command not registered
                print(f"Function for cmdID {frame['cmdID']} not registered in Arduino image")
                break
        else:
            raise RuntimeError
        
        return response # Return all non-ACK responses

    def _read_response_from_arduino(self) -> dict:
        read_timeout = time.time() + 1
        cleaned_output = []
        
        while time.time() < read_timeout:
            try:
                raw_output = self.arduino.read_all().decode("utf-8")
            except UnicodeDecodeError:
                pass
            
            if raw_output:
                match = r"\|" # currently using a pipe | as a delimiter between json frames in a string
                cleaned_output = [x for x in re.split(match, raw_output) if x]
                break
            else:
                time.sleep(.1)
        
        try:
            response_list = [json.loads(x) for x in cleaned_output] 
        except json.decoder.JSONDecodeError:
            print(f"error parsing: {cleaned_output}")
            raise

        return response_list

    def _poll_for_serial_response(self, timeout_secs: int = 2) -> dict:
        
        for _ in range(timeout_secs):
            response = self._read_response_from_arduino()
            if response:
                print("\tARD01 --> " + str(response))
                break
            else:
                time.sleep(1 - self.serial_timeout)
        else:
            raise TimeoutError
        
        return response

    def run_debug(self) -> None:

        self.do_nothing()        

        valid_commands = [
            '{"function": "move_forward()", "cmdID": 33333, "intArg0": 90, "intArg1": 1, "intArg2": -1, "intArg3": -1, "floatArg0": 0.40, "floatArg1": -1.0, "strArg0": "", "strArg1": ""}',
            '{"function": "move_forward()", "cmdID": 44444, "intArg0": 90, "intArg1": 1, "floatArg0": 0.40}',
            '{"function": "move_backward()", "cmdID": 55555, "intArg0": 30, "intArg1": 1, "intArg2": -1, "intArg3": -1, "floatArg0": 0.40, "floatArg1": -1.0, "strArg0": "", "strArg1": ""}',
            '{"function": "rotate()", "cmdID": 66666, "intArg0": 90, "intArg1": -1, "intArg2": -1, "intArg3": -1, "floatArg0": -1.0, "floatArg1": -1.0, "strArg0": "", "strArg1": ""}',
            '{"function": "do_nothing()", "cmdID": 77777, "intArg0": -1, "intArg1": -1, "intArg2": -1, "intArg3": -1, "floatArg0": -1.0, "floatArg1": -1.0, "strArg0": "", "strArg1": ""}',
            '{"function": "do_nothing()", "cmdID": 88888}',
        ]

        print(f"Executing {len(valid_commands)} valid commands:")
        for command_string in valid_commands:
            try:
                response = self._send_command_to_arduino(json.loads(command_string))
                if not response:
                    response = self._poll_for_serial_response()                    
                print("\tARD01 --> " + str(response))
            except KeyboardInterrupt:
                self.cleanup()

        invalid_commands = [
            '{"function": "move_left()", "cmdID": 33333, "intArg0": 90, "intArg1": 1, "intArg2": -1, "intArg3": -1, "floatArg0": 0.40, "floatArg1": -1.0, "strArg0": "", "strArg1": ""}',
            # '{"function": "move_forward()",/ "cmdID": 44444, "intArg0": 0, "intArg1": 1, "floatArg0": 0.40}',  # TODO figure out how to handle this better on arduino
        ]

        print(f"Executing {len(invalid_commands)} invalid commands:")
        for command_string in invalid_commands:
            try:
                response = self._send_command_to_arduino(json.loads(command_string))
                if not response:
                    response = self._poll_for_serial_response()                    
                print("\tARD01 --> " + str(response))
            except KeyboardInterrupt:
                self.cleanup()

### User API Functions

    def do_nothing(self):
        cmd = self._init_cmd_dict(self.do_nothing.__name__)
        
        # Add other args to command dict, as needed
        
        response = self._send_command_to_arduino(cmd)
        if not response:
            response = self._poll_for_serial_response(timeout_secs = 1)

        for frame in response:
            if frame["cmdID"] == cmd["cmdID"] and frame["status"] == 0:
                print("\tARD01 --> " + str(frame))
                # print(f"\t\t(Command {cmd['cmdID']} executed successfully)")

    def cleanup(self):
        self.arduino.close()

if __name__ == "__main__":
    arduino_debug = arduino_control()
    arduino_debug.run_debug()
    arduino_debug.cleanup()