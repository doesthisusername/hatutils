This is HatLag
Just double-click HatLag.exe to use (if you pass an argument, that will override the path for keybinds.txt)
It works on DLC1.5 (DW Any%), DLC2.1 (Any%), DLC2.31 (TAS patch), and DLC2.32 (110%)

Then, keybinds.txt contains your binds, in the format "keycode: millisecond-duration", one on each line
There can be an unlimited amount of binds, each with their own lag durations. Please note that, unless the --i-am-testing commandline argument is added, that every duration will be replaced with 400 milliseconds

The keycode list can be found at https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
Feel free to follow the example keybinds.txt file, which lags for 1000ms whenever Return (Enter) is pressed, and lags for 300ms whenever "0" is pressed

You can easily use programs such as DS4Windows or x360ce to bind lag keys to your controller; the game does not switch to keyboard input if you use modifier keycodes (e.g. Right Shift)

Additionally, whenever the "J" key is pressed, the program will reload keybinds.txt -- this bind is hardcoded
