# 厉鬼 (ligui)

Ligui is a thin, model-agnostic agent built on
[DeepSeek Harness](https://github.com/deepseek-ai/deepseek-harness) (`dsh`).

This repository is a **dsh bundle**, not a fork. It composes the published
harness and contributes one patch layer: the agent's identity, the model routes
it exposes, and its Web theme. Upstream stays upstream, so a `dsh` upgrade is a
dependency update rather than a merge.

## What's in the box

| File | Purpose |
|---|---|
| `cordis.patch.yml` | The composition layer: persona, provider routes, theme row |
| `client.js` | Browser half: the Ligui theme's `--dsw-*` token layer |
| `index.js` | Host half: the Loader row, plus the native `computer` tool |
| `native/` | C input helper: Interception driver, SendInput fallback |
| `package.json` | The `dsh.bundle` + `dsh.client` manifest |
| `examples/custom-routes.example.yml` | Shapes for OpenAI Chat/Responses and Anthropic Messages gateways |

## Requirements

- Node.js **24.9+**
- `pnpm` on `PATH` (only when installing the bundle into a profile)
- `dsh` 0.2.x — the alpha line, while the harness is in developer preview

## Try it without installing a profile

Point the stock Web profile at this patch:

```sh
npx -y @deepseek-ai/dsh@alpha web --patch ./cordis.patch.yml
```

## Install as a profile

The `web` template supplies the browser UI; the bundle then adds Ligui's layer.
`--dump-config` creates the profile without booting it:

```sh
npx -y @deepseek-ai/dsh@alpha --profile ligui --from-default-profile web --dump-config
npx -y @deepseek-ai/dsh@alpha plugin --profile ligui add .
npx -y @deepseek-ai/dsh@alpha --profile ligui
```

Inspect the composed tree at any time without booting:

```sh
npx -y @deepseek-ai/dsh@alpha --profile ligui --dump-config
```

## Model support

Ligui deliberately does not pin a model. `dsh-base` mounts the native DeepSeek
adapter and the multi-provider `dsh-llm-pi-ai` adapter; this bundle activates
the built-in provider routes below.

| Wire protocol | Provider id | Credential env |
|---|---|---|
| DeepSeek native API | `deepseek-official` | `DEEPSEEK_API_KEY` |
| OpenAI Chat Completions + Responses | `openai` | `OPENAI_API_KEY` |
| Anthropic Messages | `anthropic` | `ANTHROPIC_API_KEY` |
| Google Gemini | `google` | `GEMINI_API_KEY` |
| OpenRouter (many upstreams) | `openrouter` | `OPENROUTER_API_KEY` |
| Moonshot / Kimi | `moonshotai` | `MOONSHOT_API_KEY` |
| Z.ai / GLM | `zai` | `ZAI_API_KEY` |

Export a key before launch, or enter it in **Settings → Models**. A saved
credential outranks the environment reference. A route with no key still
appears in the picker; its requests fail with `MISSING_CREDENTIAL` until a
credential exists.

Selecting a model in the picker makes it the default for new sessions. Before
any selection, the default is `deepseek-official` / `deepseek-flash`, inherited
from `dsh-base`.

### Add a custom gateway

Any endpoint that speaks one of the three protocols works without code. Add a
route under `providers` in `cordis.patch.yml` (or your profile patch) with
`api`, `baseURL`, and an explicit `models` list. See
[`examples/custom-routes.example.yml`](examples/custom-routes.example.yml).

A gateway that serves two protocols needs two routes: one provider speaks one
`api`.

### Add a bespoke protocol

If the model's wire format is not OpenAI Chat, OpenAI Responses, or Anthropic
Messages, implement an `LlmAdapter` and register it on `ctx.llm`; no fork is
needed. Start from the upstream guide:
[LLM adapters](https://github.com/deepseek-ai/deepseek-harness/blob/main/docs/user/develop/practice/llm-adapter.md).

## Theme

The Web UI boots with a Ligui skin: near-black, green-tinted surfaces, one acid
green accent, tighter corners (3–18px instead of the stock 4–28px), and faster
motion (0.09s / 0.14s / 0.22s instead of 0.1s / 0.2s / 0.3s).

`client.js` registers one `ctx.theme.overrideTokens` layer over the active
palette. It never changes the user's preference, so **Settings → General →
Appearance** keeps working in light, dark, and system mode. There is no build
step: `dsh.client` serves the browser half as-is.

To tune the skin, edit `TOKENS` in `client.js` and reload the page. Every value
must be a `{ light, dark }` pair; `overrideTokens` rejects a bare string so a
scheme switch can never make the UI illegible.

The theme does not replace the product wordmark: the sidebar still reads
"DeepSeek Harness". Changing that needs a slot registration (a React client
plugin), which this repo does not ship yet.

## Upgrading dsh

`package.json` pins the compatible harness line with `engines.dsh`
(`>=0.2.0 <0.3.0`). After a dsh upgrade:

```sh
npx -y @deepseek-ai/dsh@alpha plugin --profile ligui update
npx -y @deepseek-ai/dsh@alpha --profile ligui --dump-config
```

`dsh` is in developer preview and makes breaking changes within a minor line;
`--dump-config` is the quickest way to catch a renamed row or config field.

## License

Not chosen yet. The upstream harness is MIT.

## Desktop

The `desktop/` folder is a thin Electron shell. It starts the bundled dsh
runtime on a free loopback port, reads the token URL from its output, and loads
Ligui in a native window — no browser tab and no browser chrome.

It shares `~/.dsh` with the CLI, so sessions, workspaces, and settings are the
same in both.

**Prerequisites:** Node.js 24+, npm.

```sh
cd desktop
npm install
npm install --prefix runtime @deepseek-ai/dsh@alpha
npm start            # development window
npm run dist         # portable Windows exe in desktop/dist
```

The build produces:

- `desktop/dist/win-unpacked/Ligui.exe` — instant launch, keep the folder together
- `desktop/dist/Ligui-0.1.0-portable.exe` — one file, but it self-extracts the
  bundled runtime on first launch, which takes minutes

Electron is pinned to **44.7.0**. dsh's native module loader accepts only
43.0.0, 44.0.0, 44.7.0, and 45.0.0-alpha.6; any other Electron build fails at
boot with `unsupported Electron runtime fingerprint`.

The shell is not code-signed, so Windows SmartScreen may warn on first launch.


## Native computer use

Ligui drives the real Windows desktop. Observation and input use two paths:

| Path | Backend | Tools |
|---|---|---|
| Observe | Windows-MCP (UI Automation) | `mcp__windows__Snapshot`, `Screenshot`, `Wait`, `WaitFor`, `DisplayInventory`, `ControlStatus` |
| Act | `native/ligui-input.exe` (Interception) | `computer` |

The native helper sends **scan-code** key strokes and raw mouse strokes through
the [Interception](https://github.com/oblitum/Interception) keyboard/mouse filter
driver. That is the path that reaches applications which ignore `SendInput`
(the AION 2 client does). `SendInput` remains the fallback when the driver or
DLL is missing.

### Build the helper

The helper is C and builds with gcc (MinGW-w64):

```sh
INTERCEPTION_DLL=D:\autoauto\third_party\interception\library\x64\interception.dll \
  ./native/build.sh
```

This produces `native/ligui-input.exe` and copies `interception.dll` beside it.
The DLL is third-party and is not committed. Check the driver:

```sh
./native/ligui-input.exe status
# {"ok":true,"backend":"interception","driverLoaded":true,...}
```

The Interception driver is installed separately (`INSTALL.cmd` as admin, then
reboot); [`native/README.md`](native/README.md) covers the details.

### The `computer` tool

`computer` takes one `action` plus optional fields:

| Action | Fields | Does |
|---|---|---|
| `status` | — | Driver, device, and DLL state |
| `display`, `cursor`, `windows`, `active` | — | Desktop and window inspection |
| `focus` | `title` or `hwnd` | Bring a window to the foreground |
| `launch` | `target`, `args` | Start an app, document, or URL |
| `key` | `key` | Scan-code key or chord (`f`, `shift+w`, `ctrl+shift+esc`, `alt+f4`) |
| `keydown` / `keyup` | `key` | Hold or release one key |
| `type` | `text` | Type text (ASCII scan codes, Unicode via SendInput) |
| `move` | `x`, `y` | Absolute primary-screen point |
| `rawmove` | `dx`, `dy` | Relative mouse (camera/aiming) |
| `click` | `x`, `y`, `button`, `count` | Click a point |
| `down` / `up` | `button` | Hold or release a button |
| `scroll` | `amount` | Wheel notches |
| `releaseall` | — | Release common keys and all mouse buttons |

The agent is instructed to `Snapshot` first, `focus` the target, act with
`computer`, then `Snapshot` again to verify.

### Install Windows-MCP

The observation half is a Python MCP server:

```sh
uv tool install windows-mcp
```

`uv` installs Python 3.14 for it when needed. The desktop shell adds
`%USERPROFILE%\.local\bin` to the dsh server PATH.

### Risk

This is full desktop control. The native path sends input at the driver level
and does not implement Windows-MCP's user-takeover gate. Call `releaseall` when
a task stops, focus the intended window before acting, and keep a human stop
(the app window, or kill `ligui-input.exe`).

Interception is a kernel filter driver. Using it to automate a game can violate
that game's terms and be detected; it is not an anti-cheat bypass and no
guarantee is made. Prefer a disposable VM for untrusted prompts.

Ligui can drive the actual Windows desktop — not a browser tab. The
`mcp-windows` row mounts [Windows-MCP](https://github.com/CursorTouch/Windows-MCP)
as an MCP server, so the model gets tools to:

- launch, switch, move, and resize applications (`App`)
- see the desktop as a UI tree (`Snapshot`) or an image (`Screenshot`)
- click, type, scroll, move, and press shortcuts (`Click`, `Type`, `Scroll`, `Move`, `Shortcut`)
- wait for text, windows, or elements (`Wait`, `WaitFor`)
- run PowerShell and manage processes (`PowerShell`, `Process`)

Install the server once:

```sh
uv tool install windows-mcp
```

`uv` installs Python 3.14 for it when needed. The desktop shell adds
`%USERPROFILE%\.local\bin` to the server PATH; a CLI launch needs
`windows-mcp` on `PATH`.

The tools appear as `mcp__windows__Screenshot`, `mcp__windows__Snapshot`,
`mcp__windows__Click`, `mcp__windows__Type`, `mcp__windows__App`, and so on.
`Snapshot` is the text-friendly path and works with text-only models;
`Screenshot` needs a vision-capable model.

Windows-MCP has a **user takeover** gate: moving the physical mouse or pressing
`Ctrl+Alt+Shift+Backspace` returns control to you. It is full desktop control,
not a sandbox — the agent can launch apps, type into anything, run PowerShell,
read the clipboard, and change files. Only use it with prompts and models you
trust; prefer a disposable VM for untrusted work.

## Dedicated browser

Ligui has its own Chromium instance and profile, separate from your Edge or
Chrome windows. Playwright MCP drives it through the `mcp__browser__*` tools:
`browser_navigate`, `browser_snapshot`, `browser_click`, `browser_type`,
`browser_press_key`, `browser_tabs`, `browser_take_screenshot`,
`browser_console_messages`, and `browser_network_requests`.

The profile lives at `~/.dsh/ligui-browser`, so cookies and logins survive
across sessions. Delete that directory to reset the browser.

The server starts with `npx @playwright/mcp@0.0.80`. It prefers the newest
Playwright Chromium under `%LOCALAPPDATA%\ms-playwright` and falls back to the
installed Edge channel when no Playwright browser is present. Node/npx must be
on `PATH`.

Use the browser tools for websites; use the native `computer` tool only for
desktop applications or OS-level input. The browser is not a security boundary:
pages can read anything you log into, so treat the profile as its own account.
