'use strict'

const { contextBridge, ipcRenderer } = require('electron')

contextBridge.exposeInMainWorld('ligui', {
  platform: (typeof process !== 'undefined' && process.platform) || 'win32',
  window: {
    minimize: () => ipcRenderer.send('ligui:window-action', 'minimize'),
    toggleMaximize: () => ipcRenderer.send('ligui:window-action', 'maximize'),
    close: () => ipcRenderer.send('ligui:window-action', 'close'),
    isMaximized: () => ipcRenderer.invoke('ligui:window-state'),
    onMaximized: (callback) => {
      const listener = (_event, value) => callback(Boolean(value))
      ipcRenderer.on('ligui:maximized', listener)
      return () => ipcRenderer.removeListener('ligui:maximized', listener)
    },
  },
})

/**
 * Runs before the dsh client bundle. Enables the Web client's caption layout:
 * the top strip is a drag region and every column is padded below it.
 */
function enableWindowsTitlebar() {
  const root = document.documentElement
  if (!root) return false
  root.setAttribute('data-windows-titlebar', '')
  root.style.setProperty('--dsh-windows-titlebar-height', '40px')
  return true
}

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', enableWindowsTitlebar, { once: true })
  window.addEventListener('load', enableWindowsTitlebar, { once: true })
} else {
  enableWindowsTitlebar()
}
