'use strict'

/**
 * Runs before the dsh client bundle. Enables the Web client's Windows caption
 * layout: the top strip is a drag region and every column is padded below it.
 * documentElement may not exist yet at preload start, so wait for it.
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
