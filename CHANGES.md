# PROBLEM-6-VST – changes

The newest first. To release a new version: raise the number in VERSION and put its notes here under
"## <the new number>". After the merge into main the release is made by itself (auto-release.yml), the
downloads are built into it, and the website and the Discord post follow.

## 1.1.4
LOW SPLIT: POINT now shows its note on a small display right above the knob (default C3). Note names now follow Cubase and the MPC: middle C is C3.
The text that popped up under the program display (now hidden behind PREV / NEXT) is gone.

## 1.1.3
New: PROBLEM 6 for Linux - VST3, LV2 and a standalone app (64-bit, built on Ubuntu 22.04). Checked with pluginval like the Windows version.

## 1.1.2
The window opens as large as you left it - also after closing and reopening the DAW (before, it came back at the smallest size). A new instance starts at the size you used last.
Saving your own sounds fixed: the save window now opens inside the plugin (before, it could open hidden behind the DAW's plugin window, so nothing got saved). If Windows keeps apps out of the Documents folder, your sounds are saved in AppData instead. A failed save now says so.

## 1.1.1
(see the release page on GitHub)
