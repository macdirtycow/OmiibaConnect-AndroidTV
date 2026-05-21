# Regression checklist — Omiiba Connect Android TV

Pre-release manual test on a TV where [RFCOMM spike](android-tv-bluetooth.md) passed.

## Connection

- [ ] Bonded XM3 appears in connect list
- [ ] Connect + handshake succeeds
- [ ] Battery / codec / firmware shown after refresh
- [ ] Disconnect works

## Ambient

- [ ] Ambient on/off
- [ ] ASM level 0–max
- [ ] Focus on voice toggle

## Virtual sound

- [ ] Sound position: 3+ changes in a row
- [ ] Surround preset
- [ ] Surround → position (slow path)
- [ ] Position → surround

## Equalizer

- [ ] EQ presets apply
- [ ] Manual EQ (if UI exposed in build)

## Extra

- [ ] Touch sensor toggle (XM3/XM4)
- [ ] Voice guidance toggle

## Idle

- [ ] Pause 2–3 minutes, then change ANC (no refresh needed)
- [ ] App resume from background still controls headphones

## Errors

- [ ] Refresh recovers stale session
- [ ] No permanent “connected but dead” without user action
