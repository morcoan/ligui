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
| `index.js` | Node half: the empty Loader row the client scanner keys on |
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
