# ligui-input

Low-level Windows input for Ligui: scan-code keys, raw mouse strokes, window
focus/launch, and desktop inspection. The primary backend is the
[Interception](https://github.com/oblitum/Interception) keyboard/mouse filter
driver, loaded dynamically from `interception.dll`; `SendInput` is the fallback.

This is user-mode code. It does not build, install, or patch a driver.

## Build

```sh
INTERCEPTION_DLL=D:\autoauto\third_party\interception\library\x64\interception.dll \
  ./native/build.sh
```

`INTERCEPTION_DLL` is optional at build time; the helper also looks for
`interception.dll` beside the exe or on `PATH` at runtime. Override the driver
DLL path with `LIGUI_INTERCEPTION_DLL`, and the device slots with
`LIGUI_INTERCEPTION_KEYBOARD` / `LIGUI_INTERCEPTION_MOUSE`.

Check the driver and selected devices:

```sh
./native/ligui-input.exe status
```

The Interception driver must be installed separately (`INSTALL.cmd` as
administrator, then reboot). The DLL is third-party and is not committed.

## Actions

```text
status | display | cursor | windows | active
focus --title <substr> | --hwnd 0x...
launch --target <exe|url> [--args <args>]
key --key <name|chord>        f, shift+w, ctrl+shift+esc, alt+f4
keydown --key <name> | keyup --key <name>
type --text <text>
move --x <px> --y <px>        absolute primary-screen point
rawmove --dx <px> --dy <px>   relative mouse (camera/raw input)
click [--x --y] [--button left|right|middle|mouse4|mouse5] [--count N]
down --button <name> | up --button <name>
scroll --amount <notches>     positive up, negative down
releaseall
```

Every action prints one JSON object. `status` reports the backend and device
slots; the rest return `{"ok":true,...}` or `{"ok":false,"error":"..."}`.

## Safety

The native path does not implement Windows-MCP's user-takeover gate. Focus the
intended window before sending input, call `releaseall` when a task stops, and
stop `ligui-input.exe` to end control. Automating a game can violate its terms
and be detected; this is not an anti-cheat bypass.
