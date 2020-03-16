This is HatLag
Just double-click HatLag.exe to use (if you pass an argument, that will override the path for keybinds.txt)
It works on DLC2.1, DLC2.31 (TAS patch), and DLC2.32 (110% patch)

Then, keybinds.txt contains your binds, in the format "keycode: millisecond-duration", one on each line
There can be an unlimited amount of binds, but the duration is hardcoded at 400ms now, but may still be needed for parsing to not crash

The keycode list can be found at https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
Feel free to follow the example keybinds.txt file, which is set to lag for 400ms (note that changing this will not have an effect on this version) whenever the "R" key is pressed

You can easily use programs such as DS4Windows, x360ce, or AntiMicro to bind lag keys to your controller; the game does not switch to keyboard input if you use modifier keycodes

NOTE: there is currently a bug where, if you open HatLag before the game, that it will not actually lag. Just restart HatLag if you experience this
NOTE: you most likely still need to put it a value for millisecond-duration, though it is now ignored, as 400ms works for all known lag tricks