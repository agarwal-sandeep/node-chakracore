Debugging with Node-ChakraCore using VS Code
===
This project enables debugging using Node-ChakraCore inside Visual Studio Code.

### Instructions for regular debugging 
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

