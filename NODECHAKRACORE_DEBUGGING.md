Debugging with Node-ChakraCore using VS Code
===
Instructions for using experimental debugging features of Node-ChakraCore using Visual Studio Code 

### Regular debugging during development
The goal for this work is to make Node-ChakraCore interoperable with existing Node debugging tools. Using the following instructions one can make VSCode debugger work with Node-ChakraCore without the need for modifying the debugging tool itself.

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
* Modify launch.jason to point to the node.exe that was built in earlier step
```batch
  "runtimeExecutable":"C:\\dbg\\node-chakracore\\Release\\node.exe"
```
* Launch the debugger
* Set breakpoints 
* Inspect locals, callstacks, add watches etc.

### Time Travel Debbuging
Currently, Time Travel debugging using Node-ChakraCore is an experimental project. [VSCode](https://code.visualstudio.com/) has been forked for this experiment to showcase the potential of Time travel debugging using Node-ChakraCore. Feel free to try your own experiemnts and submit PR to VSCode github repo or Node-ChakraCore.

Here are the instructions, code and required binaries for ChakraCore with Time-Travel Debugging as shown at //build 2016.
Download this self contained package (zip file), it contains the following:

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
* Notice 'Arunesh' tab has an error - this error will record the trace as per the intrumention in the code.
* Exit out of the record mode using Ctrl+C 
* Run DebugTrace.bat to launch VSCode in TraceDebug mode
* Start the debugger
* Notice the break point will hit where the error was raised
* Using the custom step back icon in the debug navigation bar you can step back in time to identify where the error originated. 


