import { execFile } from 'node:child_process'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const here = path.dirname(fileURLToPath(import.meta.url))
const helper = path.join(here, 'native', 'ligui-input.exe')

const FLAGS = {
  key: '--key',
  text: '--text',
  x: '--x',
  y: '--y',
  dx: '--dx',
  dy: '--dy',
  button: '--button',
  count: '--count',
  amount: '--amount',
  title: '--title',
  target: '--target',
  args: '--args',
  hwnd: '--hwnd',
}

const ACTIONS = [
  'status', 'display', 'cursor', 'windows', 'active', 'focus', 'launch',
  'key', 'keydown', 'keyup', 'type', 'move', 'rawmove', 'click', 'down', 'up',
  'scroll', 'releaseall',
]

const ACTION_SET = new Set(ACTIONS)

export const name = 'ligui-native-computer'
export const inject = ['tools']

function validate(args) {
  if (!args || typeof args !== 'object' || Array.isArray(args)) {
    throw new Error('arguments must be an object')
  }
  if (typeof args.action !== 'string' || !ACTION_SET.has(args.action)) {
    throw new Error(`unknown action: ${String(args.action)}`)
  }
  for (const field of ['x', 'y', 'dx', 'dy', 'count', 'amount']) {
    if (args[field] !== undefined && (typeof args[field] !== 'number' || !Number.isFinite(args[field]))) {
      throw new Error(`${field} must be a finite number`)
    }
  }
  for (const field of ['key', 'text', 'button', 'title', 'target', 'args', 'hwnd']) {
    if (args[field] !== undefined && typeof args[field] !== 'string') {
      throw new Error(`${field} must be a string`)
    }
  }
}

function invoke(args, signal) {
  const argv = [args.action]
  for (const [field, flag] of Object.entries(FLAGS)) {
    const value = args[field]
    if (value !== undefined && value !== null && value !== '') argv.push(flag, String(value))
  }
  return new Promise((resolve, reject) => {
    execFile(
      helper,
      argv,
      {
        windowsHide: true,
        timeout: 30000,
        maxBuffer: 8 * 1024 * 1024,
        signal,
        env: process.env,
      },
      (error, stdout, stderr) => {
        if (error) {
          reject(new Error((stderr || '').trim() || error.message))
          return
        }
        resolve(stdout.trim())
      },
    )
  })
}

export function apply(ctx) {
  ctx.tools.register({
    name: 'computer',
    description: `Low-level Windows desktop control through the Interception driver.

Observe first with mcp__windows__Snapshot (UI tree and element coordinates) or mcp__windows__Screenshot (pixels). Then act here:
- key: scan-code key or chord, e.g. f, shift+w, ctrl+shift+esc, alt+f4
- type: text into the focused control
- rawmove: relative mouse motion (camera/aiming)
- move/click: absolute primary-screen coordinates
- focus/launch: bring a window forward or start an app/URL
- windows/active/display/cursor: inspect desktop state
- status: report whether the Interception driver is loaded

Focus the target window before sending input, and verify with Snapshot or Screenshot afterward.`,
    parameters: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: ACTIONS,
          description: 'status | display | cursor | windows | active | focus | launch | key | keydown | keyup | type | move | rawmove | click | down | up | scroll | releaseall',
        },
        key: { type: 'string', description: 'Key name or chord: f, shift+w, ctrl+shift+esc, alt+f4' },
        text: { type: 'string', description: 'Text to type' },
        x: { type: 'number', description: 'Absolute x on the primary monitor' },
        y: { type: 'number', description: 'Absolute y on the primary monitor' },
        dx: { type: 'number', description: 'Relative mouse x delta' },
        dy: { type: 'number', description: 'Relative mouse y delta' },
        button: { type: 'string', description: 'left | right | middle | mouse4 | mouse5' },
        count: { type: 'number', description: 'Click count, 1-10' },
        amount: { type: 'number', description: 'Scroll notches, positive up' },
        title: { type: 'string', description: 'Window title substring for focus' },
        target: { type: 'string', description: 'Executable, document, or URL for launch' },
        args: { type: 'string', description: 'Arguments for launch' },
        hwnd: { type: 'string', description: 'Window handle from windows/active, e.g. 0x9D0C04' },
      },
      required: ['action'],
      additionalProperties: false,
    },
    output: {
      schema: { type: 'string' },
      render: (_args, value) => [{ type: 'text', text: value }],
    },
    async execute(args, exec) {
      validate(args)
      return invoke(args, exec.signal)
    },
  })
}
