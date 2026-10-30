/**
 * Ligui theme, browser half.
 *
 * The Web shell loads this bundle as a factory. `apply` stacks one alias-token
 * layer over the active light or dark palette; it never changes the user's
 * preference, so Settings → General → Appearance keeps working.
 *
 * Token values are `{ light, dark }` pairs because the theme service refuses
 * a single value that would go illegible when the scheme flips.
 */
window.__ModuleLoader__.load({
  id: 'dsh-ligui',
  factory: (require) => {
    const module = { exports: {} }
    const exports = module.exports
    const { jsx } = require('react/jsx-runtime')

    /** Required service: the theme registry over the --dsw-* token sheets. */
    const inject = ['theme', 'slots']

    /** Pair one value per palette; repeat a value when the token is invariant. */
    const p = (light, dark) => ({ light, dark })

    const TOKENS = {
      // Surfaces — black glass over a red undertone
      '--dsw-alias-bg-base': p('#f7f0ea', '#0a0607'),
      '--dsw-alias-bg-layer-1': p('#fffaf5', '#120a0c'),
      '--dsw-alias-bg-layer-2': p('#f1e7e1', '#1a0e11'),
      '--dsw-alias-bg-layer-3': p('#e9ddd5', '#241316'),
      '--dsw-alias-bg-overlay': p('#fffdf9', '#140b0d'),
      '--dsw-alias-bg-skeleton': p('#efe4dd', '#1c0f12'),
      '--dsw-specific-sidebar-fill': p('#f2e9e2', '#0d0709'),
      '--dsw-specific-bubble': p('#f0e4de', '#170c0f'),
      '--dsw-specific-input-major': p('#fffdf9', '#120a0c'),
      '--dsw-specific-menu': p('#fffdf9', '#140b0d'),
      '--dsw-menu-surface-fill': p('rgba(255, 253, 249, 0.96)', 'rgba(20, 11, 13, 0.96)'),

      // Borders — warm white hairlines
      '--dsw-alias-border-l1': p('rgba(40, 16, 16, 0.10)', 'rgba(255, 237, 229, 0.08)'),
      '--dsw-alias-border-l2': p('rgba(40, 16, 16, 0.18)', 'rgba(255, 237, 229, 0.14)'),
      '--dsw-alias-border-l3': p('rgba(40, 16, 16, 0.26)', 'rgba(255, 237, 229, 0.20)'),
      '--dsw-alias-border-l4': p('rgba(40, 16, 16, 0.34)', 'rgba(255, 237, 229, 0.28)'),

      // Text — warm white, never pure gray
      '--dsw-alias-label-primary': p('#1b0f0f', '#f6ede7'),
      '--dsw-alias-label-secondary': p('#5b4543', '#c8b5ad'),
      '--dsw-alias-label-tertiary': p('#826b65', '#987f77'),
      '--dsw-alias-label-caption': p('#927b74', '#816b64'),
      '--dsw-alias-label-dimmed': p('#927b74', '#816b64'),

      // Brand, links, states — red primary, orange links, yellow success
      '--dsw-alias-brand-primary': p('#b3121c', '#e01f2d'),
      '--dsw-alias-brand-text': p('#fffaf5', '#fff8f2'),
      '--dsw-alias-link': p('#b45309', '#ff7a1a'),
      '--dsw-alias-state-success-primary': p('#a16207', '#ffc21a'),
      '--dsw-alias-state-warn-primary': p('#b45309', '#ff7a1a'),
      '--dsw-alias-state-error-primary': p('#c02f2f', '#ff4d4d'),
      '--dsw-alias-state-idle-primary': p('#826b65', '#8a7570'),

      // Buttons
      '--dsw-alias-button-primary-fill': p('#b3121c', '#c91824'),
      '--dsw-alias-button-primary-hover': p('#d01824', '#e01f2d'),
      '--dsw-alias-button-primary-dimmed': p('rgba(179, 18, 28, 0.30)', 'rgba(224, 31, 45, 0.35)'),
      '--dsw-alias-button-contrast-fill': p('#1b0f0f', '#f6ede7'),

      // Code and diffs
      '--dsw-alias-markdown-code-block': p('#f1e7e1', '#120a0c'),
      '--dsw-alias-markdown-inline-code': p('#e9ddd5', '#1e1013'),
      '--dsw-alias-code-diff-added': p('#a16207', '#ffc21a'),
      '--dsw-alias-code-diff-deleted': p('#c02f2f', '#ff4d4d'),

      // Menus, toasts, turn triggers
      '--dsw-alias-tooltip-bg': p('#1b0f0f', '#1a0e11'),
      '--dsw-alias-toast-bg': p('#1b0f0f', '#1a0e11'),
      '--dsw-alias-toast-label': p('#fffaf5', '#f6ede7'),
      '--dsw-alias-turn-trigger-bg': p('#f1e7e1', '#160c0f'),
      '--dsw-alias-turn-trigger-bg-hover': p('#e9ddd5', '#1f1013'),

      // Interaction — red wash with orange accents
      '--dsw-alias-interactive-bg-hover': p('rgba(179, 18, 28, 0.08)', 'rgba(224, 31, 45, 0.10)'),
      '--dsw-alias-interactive-bg-active': p('rgba(179, 18, 28, 0.14)', 'rgba(224, 31, 45, 0.18)'),
      '--dsw-alias-interactive-bg-hover-accent': p('rgba(194, 65, 12, 0.16)', 'rgba(255, 122, 26, 0.18)'),
      '--dsw-specific-sidebar-nav-item-active': p('rgba(179, 18, 28, 0.10)', 'rgba(224, 31, 45, 0.12)'),
      '--dsw-specific-sidebar-nav-item-active-accent': p('#b3121c', '#e01f2d'),

      // Scrollbars
      '--dsw-alias-scrollbar-bg-l1': p('rgba(40, 16, 16, 0.14)', 'rgba(255, 237, 229, 0.10)'),
      '--dsw-alias-scrollbar-hover-l1': p('rgba(40, 16, 16, 0.26)', 'rgba(255, 237, 229, 0.22)'),

      // Density: tighter corners than the stock 4/8/12/16/20/28 scale
      '--dsw-radius-xs': p('3px', '3px'),
      '--dsw-radius-sm': p('5px', '5px'),
      '--dsw-radius-md': p('7px', '7px'),
      '--dsw-radius-lg': p('10px', '10px'),
      '--dsw-radius-xl': p('14px', '14px'),
      '--dsw-radius-panel': p('18px', '18px'),

      // Snappier motion than the stock 0.1s/0.2s/0.3s ease
      '--ds-transition-duration-fast': p('0.09s', '0.09s'),
      '--ds-transition-duration': p('0.14s', '0.14s'),
      '--ds-transition-duration-slow': p('0.22s', '0.22s'),
      '--ds-ease-in-out': p('cubic-bezier(0.2, 0, 0, 1)', 'cubic-bezier(0.2, 0, 0, 1)'),
    }


    function LiguiName() {
      return jsx('span', {
        style: {
          fontFamily: '"KaiTi", "STKaiti", "SimSun", serif',
          fontSize: '1.3rem',
          fontWeight: 700,
          letterSpacing: '0.12em',
          lineHeight: 1,
          color: 'var(--dsw-alias-label-primary)',
        },
        children: '厉鬼',
      })
    }
    /**
     * Stack the Ligui layer over the active palette.
     * @param ctx - client root context (the `theme` service is ready).
     */
    function apply(ctx) {
      ctx.effect(() => ctx.theme.overrideTokens('dsh-ligui', TOKENS))
      const NoMark = () => null
      ctx.slots.inject('sidebar.brand.mark', () =>
        ctx.slots.inject('conversation.hero.brand.mark', function* () {
          yield ctx.slots.register({ name: 'sidebar.brand.mark' }, NoMark)
          yield ctx.slots.register({ name: 'conversation.hero.brand.mark' }, NoMark)
        }))
      ctx.slots.inject('sidebar.brand.name', () =>
        ctx.slots.register({ name: 'sidebar.brand.name' }, LiguiName))
      ctx.effect(() => {
        const pairs = [
          ['DeepSeek Harness', 'Ligui'],
          ['DeepSeek account', 'model provider account'],
        ]
        const replace = () => {
          const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT)
          let node = walker.nextNode()
          while (node) {
            let value = node.nodeValue
            for (const pair of pairs) {
              if (value.includes(pair[0])) value = value.split(pair[0]).join(pair[1])
            }
            if (value !== node.nodeValue) node.nodeValue = value
            node = walker.nextNode()
          }
        }
        const timers = [400, 1200, 3000, 6000].map((ms) => setTimeout(replace, ms))
        return () => {
          for (const timer of timers) clearTimeout(timer)
        }
      })
      ctx.effect(() => {
        const api = window.ligui && window.ligui.window
        if (!api) return
        const style = document.createElement('style')
        style.textContent = `#ligui-titlebar{position:fixed;top:0;left:0;right:0;height:var(--dsh-windows-titlebar-height,40px);display:flex;align-items:center;justify-content:flex-end;z-index:2147483000;background:linear-gradient(90deg,rgba(224,31,45,.12) 0%,rgba(224,31,45,0) 32%),var(--dsw-specific-sidebar-fill,#0d0709);border-bottom:1px solid var(--dsw-alias-border-l1,rgba(255,255,255,.08));box-shadow:inset 0 -1px 0 rgba(255,122,26,.28);-webkit-app-region:drag;user-select:none}
#ligui-titlebar .ligui-titlebar-controls{display:flex;height:100%;-webkit-app-region:no-drag}
#ligui-titlebar .ligui-titlebar-button{width:46px;height:100%;display:flex;align-items:center;justify-content:center;border:0;background:transparent;color:var(--dsw-alias-label-secondary,#a3afa6);cursor:pointer;padding:0;transition:background var(--ds-transition-duration-fast,.09s) var(--ds-ease-in-out),color var(--ds-transition-duration-fast,.09s)}
#ligui-titlebar .ligui-titlebar-button:hover{background:var(--dsw-alias-interactive-bg-hover,rgba(224,31,45,.10));color:var(--dsw-alias-label-primary,#f6ede7)}
#ligui-titlebar .ligui-titlebar-close:hover{background:var(--dsw-alias-state-error-primary,#ff4d4d);color:#fff}
#ligui-titlebar .ligui-titlebar-button svg{width:12px;height:12px;stroke:currentColor;fill:none;stroke-width:1.2;stroke-linecap:round}
#ligui-titlebar .ligui-titlebar-close svg{width:10px;height:10px}
#ligui-frame{position:fixed;inset:0;border:1px solid var(--dsw-alias-border-l1,rgba(255,255,255,.08));pointer-events:none;z-index:2147483001;box-sizing:border-box}`
        const bar = document.createElement('div')
        bar.id = 'ligui-titlebar'

        const makeButton = (kind, label, svg) => {
          const button = document.createElement('button')
          button.type = 'button'
          button.className = 'ligui-titlebar-button ligui-titlebar-' + kind
          button.setAttribute('aria-label', label)
          button.innerHTML = svg
          return button
        }
        const MIN_SVG = '<svg viewBox="0 0 12 12" aria-hidden="true"><path d="M2 6h8"/></svg>'
        const MAX_SVG = '<svg viewBox="0 0 12 12" aria-hidden="true"><rect x="2.2" y="2.2" width="7.6" height="7.6" rx="0.6"/></svg>'
        const RESTORE_SVG = '<svg viewBox="0 0 12 12" aria-hidden="true"><rect x="2" y="3.6" width="6.4" height="6.4" rx="0.6"/><path d="M4.2 3.6V2.2h5.6v5.6H8.6"/></svg>'
        const CLOSE_SVG = '<svg viewBox="0 0 12 12" aria-hidden="true"><path d="M2.5 2.5l7 7M9.5 2.5l-7 7"/></svg>'
        const minimize = makeButton('min', 'Minimize', MIN_SVG)
        const maximize = makeButton('max', 'Maximize', MAX_SVG)
        const close = makeButton('close', 'Close', CLOSE_SVG)
        const controls = document.createElement('div')
        controls.className = 'ligui-titlebar-controls'
        controls.append(minimize, maximize, close)
        bar.append(controls)

        const frame = document.createElement('div')
        frame.id = 'ligui-frame'
        document.head.appendChild(style)
        document.body.append(bar, frame)

        const setMaximized = (value) => {
          maximize.innerHTML = value ? RESTORE_SVG : MAX_SVG
          maximize.setAttribute('aria-label', value ? 'Restore' : 'Maximize')
        }
        api.isMaximized().then(setMaximized).catch(() => {})
        const off = typeof api.onMaximized === 'function' ? api.onMaximized(setMaximized) : null

        const onMinimize = () => api.minimize()
        const onMaximize = () => api.toggleMaximize()
        const onClose = () => api.close()
        const onDoubleClick = (event) => {
          if (event.target.closest('button')) return
          api.toggleMaximize()
        }
        minimize.addEventListener('click', onMinimize)
        maximize.addEventListener('click', onMaximize)
        close.addEventListener('click', onClose)
        bar.addEventListener('dblclick', onDoubleClick)

        return () => {
          if (typeof off === 'function') off()
          minimize.removeEventListener('click', onMinimize)
          maximize.removeEventListener('click', onMaximize)
          close.removeEventListener('click', onClose)
          bar.removeEventListener('dblclick', onDoubleClick)
          bar.remove()
          frame.remove()
          style.remove()
        }
      })
    }

    exports.apply = apply
    exports.inject = inject
    return module.exports
  },
})
