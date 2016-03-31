Debugging with Node-ChakraCore using VS Code
===
Instructions for using experimental debugging features of Node-ChakraCore using Visual Studio Code 

### Debugging during development
The goal for this work is to make Node-ChakraCore interoperable with existing Node debugging tools. Using the following instructions one can make VSCode debugger work with Node-ChakraCore without any modification to VSCode. You can follow the progress of this work [here](https://github.com/agarwal-sandeep/node-chakracore/tree/debugging ).

* Create a workspace  
 ```batch
 md c:\dbg
  cd c:\dbg
  ```

* Get the sources
 ```batch
 git clone --single-branch --branch debugging https://github.com/agarwal-sandeep/node-chakracore.git
 ```

* Build it
 ```batch
 cd node-chakracore
 Vcbuild chakracore nosign x86 (make sure python 2.7 is in path)
 ```
* Install [Visual Studio Code](https://code.visualstudio.com/)
* Create a test project (or open an existing one)
* Modify launch.json to point to the node.exe that was built in earlier step
```batch
  "runtimeExecutable":"C:\\dbg\\node-chakracore\\Release\\node.exe"
```
* Launch the debugger
* Set breakpoints 
* Inspect locals, callstacks, add watches etc.

### Time Travel Debbuging
Time Travel debugging using Node-ChakraCore is an **experimental** project. [VSCode](https://code.visualstudio.com/) has been forked for this experiment to showcase the potential of Time travel debugging using Node-ChakraCore. You can follow the progress of this work at our [TimeTravel repo](https://github.com/Microsoft/ChakraCore/tree/TimeTravelDebugging).

Here are the instructions, code and required binaries for ChakraCore with Time-Travel Debugging as shown at //build 2016.
Download [this self contained package](http://research.microsoft.com/en-us/um/people/marron/samples/TTDBuildDemo.zip) (zip file), it contains the following:

* Custom Node-ChakraCore and related binaries that support TimeTravel record and replay functionality 
* Custom build of VSCode which supports UI affordance to step back when in TimeTravel mode
* Sample Node.js project called "Workitems Tracker" to show case the TimeTravel feature.
* RecordTrace.bat - helper file to run Node-ChakraCore in recording mode.
* DebugTrace.bat - helper file to start custom VSCode in TraceDebug mode.

After you expand the downloaded file 

* Run RecordTrace.bat - Notice the following prompt
```batch
Server running at http://127.0.0.1:1338/'
```
* Navigate to the address in the browser.
* Click through all the developer names to see the remaining taks for each of the developers.
* Notice 'Arunesh' tab unexpectedly prints **Infinity**  - this error will record the trace as per the intrumention in the code.
* Exit out of the record mode using Ctrl+C 
* Run DebugTrace.bat to launch VSCode in TraceDebug mode
* Start the debugger
* Notice the break point will hit where the error was raised
* Using the custom step back icon in the debug navigation bar you can step back in time to identify where the error originated. 

Please let us know on the [issues page](https://github.com/Microsoft/ChakraCore/issues) or on twitter [@ChakraCore](https://twitter.com/chakraCore) if you run into issues with any of this. 
Also, feel free to try your own experiemnts and submit PR to [VSCode github repo](https://github.com/Microsoft/vscode) or [Node-ChakraCore repo](https://github.com/nodejs/node-chakracore/).
