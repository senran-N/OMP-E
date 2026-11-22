# OMP Desktop Localization Packs (BCP-47)

OMP Desktop uses a data-driven, extensible JSON i18n catalog (`ompe::I18nCatalog`).
Adding a new language or customizing existing strings requires **zero C++ code changes**.

## Adding a New Language (e.g. `ja-JP`, `ko-KR`, `fr-FR`, `de-DE`, `zh-TW`)

1. Copy `en-US.json` to `<bcp47-code>.json` inside either:
   - `~/.omp/desktop/locales/` (user drop-in directory — persists across app updates, supports live Reload in **Settings → General → Language & Locale**), or
   - `assets/locales/` (bundled with the app).
2. Update `_meta`:
   ```json
   {
     "_meta": {
       "code": "ja-JP",
       "name": "日本語",
       "englishName": "Japanese",
       "fallback": "en-US",
       "cjkSpacing": true,
       "rtl": false
     },
     "strings": {
       "welcome.headline": "今日は何を作りましょうか？"
     }
   }
   ```
3. Partial translations are fully supported: any untranslated key automatically falls back through `_meta.fallback` → `en-US`.
4. Named placeholders such as `{nodes}`, `{branches}`, `{sec}`, `{name}` can be freely reordered to match target language grammar.
