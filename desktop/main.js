'use strict'

const { app, BrowserWindow, Menu, dialog, ipcMain, shell } = require('electron')
const { spawn } = require('node:child_process')
const fs = require('node:fs')
const net = require('node:net')
const os = require('node:os')
const path = require('node:path')

const isDev = !app.isPackaged
const runtimeRoot = isDev
  ? path.join(__dirname, 'runtime')
  : path.join(process.resourcesPath, 'runtime')
const dshBin = path.join(runtimeRoot, 'node_modules', '@deepseek-ai', 'dsh', 'lib', 'bin.js')
const patchPath = isDev
  ? path.join(__dirname, '..', 'cordis.patch.yml')
  : path.join(process.resourcesPath, 'ligui', 'cordis.patch.yml')

let server = null
let win = null
let shuttingDown = false

app.setAppUserModelId('com.morcoan.ligui')

if (!app.requestSingleInstanceLock()) {
  app.quit()
} else {
  app.on('second-instance', () => {
    if (win) {
      if (win.isMinimized()) win.restore()
      win.show()
      win.focus()
    }
  })

  app.whenReady().then(start).catch(fail)
}

function freePort() {
  return new Promise((resolve, reject) => {
    const probe = net.createServer()
    probe.unref()
    probe.on('error', reject)
    probe.listen(0, '127.0.0.1', () => {
      const { port } = probe.address()
      probe.close(() => resolve(port))
    })
  })
}

function startServer(port) {
  return spawn(
    process.execPath,
    [dshBin, 'web', '--patch', patchPath, '--no-open', '--port', String(port)],
    {
      env: {
        ...process.env,
        ELECTRON_RUN_AS_NODE: '1',
        PATH: [path.join(os.homedir(), '.local', 'bin'), process.env.PATH].filter(Boolean).join(path.delimiter),
      },
      stdio: ['ignore', 'pipe', 'pipe'],
      windowsHide: true,
    },
  )
}

function waitForUrl(child, timeoutMs = 90000) {
  return new Promise((resolve, reject) => {
    let output = ''
    let settled = false

    const finish = (error, url) => {
      if (settled) return
      settled = true
      clearTimeout(timer)
      child.stdout.off('data', onStdout)
      child.off('exit', onExit)
      if (error) reject(error)
      else resolve(url)
    }

    const onStdout = (chunk) => {
      output += chunk.toString()
      const match = output.match(/http:\/\/127\.0\.0\.1:\d+\/\?token=[A-Za-z0-9_-]+/)
      if (match) finish(null, match[0])
    }

    const onExit = (code) => {
      finish(new Error(`dsh exited with code ${code} before the UI was ready.\n\n${output.slice(-3000)}`))
    }

    const timer = setTimeout(() => {
      finish(new Error(`Timed out waiting for the dsh server.\n\n${output.slice(-3000)}`))
    }, timeoutMs)

    child.stdout.on('data', onStdout)
    child.stderr.on('data', (chunk) => {
      output += chunk.toString()
    })
    child.on('exit', onExit)
  })
}

async function start() {
  if (!fs.existsSync(dshBin)) {
    throw new Error(`Bundled dsh runtime not found at:\n${dshBin}\n\nRun "npm install --prefix runtime @deepseek-ai/dsh@alpha" first.`)
  }
  if (!fs.existsSync(patchPath)) {
    throw new Error(`Ligui patch not found at:\n${patchPath}`)
  }

  const port = await freePort()
  server = startServer(port)
  const url = await waitForUrl(server)

  server.on('exit', (code) => {
    if (shuttingDown) return
    dialog.showErrorBox('Ligui stopped', `The background dsh server exited with code ${code}.`)
    app.quit()
  })

  bindWindowIpc()
  createWindow(url)
}
function bindWindowIpc() {
  ipcMain.handle('ligui:window-state', (event) => {
    if (!win || win.isDestroyed() || event.sender !== win.webContents) return false
    return win.isMaximized()
  })

  ipcMain.on('ligui:window-action', (event, action) => {
    if (!win || win.isDestroyed() || event.sender !== win.webContents) return
    if (action === 'minimize') win.minimize()
    else if (action === 'maximize') win.isMaximized() ? win.unmaximize() : win.maximize()
    else if (action === 'close') win.close()
  })
}

function createWindow(url) {
  Menu.setApplicationMenu(null)

  win = new BrowserWindow({
    width: 1440,
    height: 900,
    minWidth: 980,
    minHeight: 640,
    title: '厉鬼',
    backgroundColor: '#0a0607',
    frame: false,
    hasShadow: true,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    },
  })

  win.on('page-title-updated', (event) => event.preventDefault())
  win.on('closed', () => {
    win = null
  })

  const sendMaximized = () => {
    if (!win || win.isDestroyed()) return
    win.webContents.send('ligui:maximized', win.isMaximized())
  }
  win.on('maximize', sendMaximized)
  win.on('unmaximize', sendMaximized)

  win.webContents.setWindowOpenHandler(({ url: target }) => {
    if (/^https?:/i.test(target)) void shell.openExternal(target)
    return { action: 'deny' }
  })

  win.loadURL(url)

  if (process.env.LIGUI_DEBUG_WINDOW) {
    console.log('[ligui] bounds', JSON.stringify(win.getBounds()), 'content', JSON.stringify(win.getContentBounds()))
  }

  win.webContents.on('before-input-event', (event, input) => {
    const key = String(input.key || '').toLowerCase()
    if (input.type !== 'keyDown') return
    if (input.key === 'F12' || (input.control && input.shift && key === 'i')) {
      event.preventDefault()
      win.webContents.toggleDevTools()
    } else if (input.control && key === 'r') {
      event.preventDefault()
      win.webContents.reload()
    }
  })

  const shot = process.env.LIGUI_SCREENSHOT
  if (shot) {
    win.webContents.once('did-finish-load', () => {
      setTimeout(async () => {
        try {
          const image = await win.webContents.capturePage()
          fs.writeFileSync(shot, image.toPNG())
        } catch (error) {
          console.error(error)
        }
        app.quit()
      }, 8000)
    })
  }
}

function stopServer() {
  if (!server) return
  const child = server
  server = null
  if (process.platform === 'win32' && child.pid) {
    spawn('taskkill', ['/pid', String(child.pid), '/T', '/F'], { windowsHide: true })
  } else {
    child.kill('SIGTERM')
  }
}

function fail(error) {
  console.error('[ligui] failed', error)
  if (!process.env.LIGUI_NO_DIALOG) dialog.showErrorBox('Ligui could not start', String(error && error.stack ? error.stack : error))
  app.quit()
}

app.on('window-all-closed', () => {
  app.quit()
})

app.on('before-quit', () => {
  shuttingDown = true
  stopServer()
})

app.on('quit', stopServer)
