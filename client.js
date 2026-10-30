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
    const { jsx, jsxs } = require('react/jsx-runtime')

    /** Required service: the theme registry over the --dsw-* token sheets. */
    const inject = ['theme', 'slots']

    /** Pair one value per palette; repeat a value when the token is invariant. */
    const p = (light, dark) => ({ light, dark })

    const TOKENS = {
      // Surfaces
      '--dsw-alias-bg-base': p('#f6f8f3', '#0a0d0b'),
      '--dsw-alias-bg-layer-1': p('#ffffff', '#101412'),
      '--dsw-alias-bg-layer-2': p('#eef2ec', '#161b18'),
      '--dsw-alias-bg-layer-3': p('#e7ece5', '#1c221e'),
      '--dsw-alias-bg-overlay': p('#ffffff', '#121714'),
      '--dsw-alias-bg-skeleton': p('#e9eee7', '#1a201c'),
      '--dsw-specific-sidebar-fill': p('#f0f4ee', '#0d100e'),
      '--dsw-specific-bubble': p('#e9efe7', '#131a15'),
      '--dsw-specific-input-major': p('#ffffff', '#101412'),
      '--dsw-specific-menu': p('#ffffff', '#121714'),
      '--dsw-menu-surface-fill': p('rgba(255, 255, 255, 0.96)', 'rgba(18, 23, 20, 0.94)'),

      // Borders
      '--dsw-alias-border-l1': p('rgba(18, 26, 20, 0.10)', 'rgba(232, 238, 233, 0.08)'),
      '--dsw-alias-border-l2': p('rgba(18, 26, 20, 0.18)', 'rgba(232, 238, 233, 0.14)'),
      '--dsw-alias-border-l3': p('rgba(18, 26, 20, 0.26)', 'rgba(232, 238, 233, 0.20)'),
      '--dsw-alias-border-l4': p('rgba(18, 26, 20, 0.34)', 'rgba(232, 238, 233, 0.28)'),

      // Text
      '--dsw-alias-label-primary': p('#121814', '#e8eee9'),
      '--dsw-alias-label-secondary': p('#47534a', '#a3afa6'),
      '--dsw-alias-label-tertiary': p('#66726a', '#7c887f'),
      '--dsw-alias-label-caption': p('#7b877f', '#667269'),
      '--dsw-alias-label-dimmed': p('#7b877f', '#667269'),

      // Brand, links, states
      '--dsw-alias-brand-primary': p('#3d7a00', '#b8ff3d'),
      '--dsw-alias-brand-text': p('#ffffff', '#0a0d0b'),
      '--dsw-alias-link': p('#336b00', '#b8ff3d'),
      '--dsw-alias-state-success-primary': p('#1f8a4c', '#5ee08c'),
      '--dsw-alias-state-warn-primary': p('#9a6400', '#ffc857'),
      '--dsw-alias-state-error-primary': p('#c02f2f', '#ff6b6b'),
      '--dsw-alias-state-idle-primary': p('#66726a', '#7c887f'),

      // Buttons
      '--dsw-alias-button-primary-fill': p('#3d7a00', '#b8ff3d'),
      '--dsw-alias-button-primary-hover': p('#4c9400', '#c9ff66'),
      '--dsw-alias-button-primary-dimmed': p('rgba(61, 122, 0, 0.30)', 'rgba(184, 255, 61, 0.35)'),
      '--dsw-alias-button-contrast-fill': p('#121814', '#e8eee9'),

      // Code and diffs
      '--dsw-alias-markdown-code-block': p('#eef2ec', '#0e120f'),
      '--dsw-alias-markdown-inline-code': p('#e4eae2', '#1a201b'),
      '--dsw-alias-code-diff-added': p('#1f8a4c', '#5ee08c'),
      '--dsw-alias-code-diff-deleted': p('#c02f2f', '#ff6b6b'),

      // Menus, toasts, turn triggers
      '--dsw-alias-tooltip-bg': p('#121814', '#1a201b'),
      '--dsw-alias-toast-bg': p('#121814', '#1a201b'),
      '--dsw-alias-toast-label': p('#f6f8f3', '#e8eee9'),
      '--dsw-alias-turn-trigger-bg': p('#e9efe7', '#131a15'),
      '--dsw-alias-turn-trigger-bg-hover': p('#e0e8de', '#1a221b'),

      // Interaction
      '--dsw-alias-interactive-bg-hover': p('rgba(61, 122, 0, 0.08)', 'rgba(184, 255, 61, 0.08)'),
      '--dsw-alias-interactive-bg-active': p('rgba(61, 122, 0, 0.14)', 'rgba(184, 255, 61, 0.14)'),
      '--dsw-alias-interactive-bg-hover-accent': p('rgba(61, 122, 0, 0.16)', 'rgba(184, 255, 61, 0.16)'),
      '--dsw-specific-sidebar-nav-item-active': p('rgba(61, 122, 0, 0.10)', 'rgba(184, 255, 61, 0.12)'),
      '--dsw-specific-sidebar-nav-item-active-accent': p('#3d7a00', '#b8ff3d'),

      // Scrollbars
      '--dsw-alias-scrollbar-bg-l1': p('rgba(18, 26, 20, 0.14)', 'rgba(232, 238, 233, 0.10)'),
      '--dsw-alias-scrollbar-hover-l1': p('rgba(18, 26, 20, 0.26)', 'rgba(232, 238, 233, 0.22)'),

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

    const GHOST_PATH = 'M4 3.5C4 2.67 4.67 2 5.5 2h13c.83 0 1.5.67 1.5 1.5v16.2c0 .42-.48.66-.82.4L16.6 18l-2.2 1.8a.62.62 0 0 1-.79 0L11.4 18l-2.2 1.8a.62.62 0 0 1-.79 0L6.2 18l-1.38 1.1c-.34.26-.82.02-.82-.4V3.5z'

    function LiguiMark(props) {
      const size = Number(props && props.size) || 22
      return jsxs('svg', {
        width: size,
        height: size,
        viewBox: '0 0 24 24',
        className: props && props.className,
        'aria-hidden': 'true',
        children: [
          jsx('path', { d: GHOST_PATH, fill: 'currentColor' }),
          jsx('rect', {
            x: 7.4, y: 7.6, width: 2.6, height: 3.4, rx: 0.8,
            style: { fill: 'var(--dsw-alias-brand-primary, #b8ff3d)' },
          }),
          jsx('rect', {
            x: 14, y: 7.6, width: 2.6, height: 3.4, rx: 0.8,
            style: { fill: 'var(--dsw-alias-brand-primary, #b8ff3d)' },
          }),
        ],
      })
    }

    function LiguiName() {
      return jsx('span', {
        style: {
          fontWeight: 650,
          letterSpacing: '0.16em',
          fontSize: '0.8rem',
          lineHeight: 1,
          textTransform: 'uppercase',
        },
        children: 'LIGUI',
      })
    }
    /**
     * Stack the Ligui layer over the active palette.
     * @param ctx - client root context (the `theme` service is ready).
     */
    function apply(ctx) {
      ctx.effect(() => ctx.theme.overrideTokens('dsh-ligui', TOKENS))
      ctx.slots.inject('sidebar.brand.mark', () =>
        ctx.slots.inject('sidebar.brand.name', () =>
          ctx.slots.inject('conversation.hero.brand.mark', function* () {
            yield ctx.slots.register({ name: 'sidebar.brand.mark' }, LiguiMark)
            yield ctx.slots.register({ name: 'sidebar.brand.name' }, LiguiName)
            yield ctx.slots.register({ name: 'conversation.hero.brand.mark' }, LiguiMark)
          })))
      ctx.effect(() => {
        if (!document.documentElement.hasAttribute('data-windows-titlebar')) return
        const style = document.createElement('style')
        style.textContent = '#ligui-titlebar-label{position:fixed;top:0;left:0;height:var(--dsh-windows-titlebar-height,40px);display:flex;align-items:center;padding-left:18px;z-index:2147483000;pointer-events:none;user-select:none;font:600 10px/1 ui-monospace,SFMono-Regular,Menlo,monospace;letter-spacing:.22em;color:var(--dsw-alias-label-tertiary,#7c887f)}#ligui-titlebar-dot{width:6px;height:6px;border-radius:50%;background:var(--dsw-alias-brand-primary,#b8ff3d);margin-right:9px}'
        const label = document.createElement('div')
        label.id = 'ligui-titlebar-label'
        const dot = document.createElement('span')
        dot.id = 'ligui-titlebar-dot'
        label.append(dot, document.createTextNode('LIGUI'))
        document.head.appendChild(style)
        document.body.appendChild(label)
        return () => {
          label.remove()
          style.remove()
        }
      })
    }

    exports.apply = apply
    exports.inject = inject
    return module.exports
  },
})
