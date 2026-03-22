# Nerd Servo Buddy — Implementation Plan

## Remaining Work

### Phase 5: Config Section — IN PROGRESS

**5.2 — EEPROM Lock/Unlock**
- "Unlock EEPROM" / "Lock EEPROM (Save)" button in config section
- Helper text explaining what it does

**5.3 — Mode Settings**
- Operating mode: Servo / Motor toggle (both SC and STS)
- Phase (direction reversal): 0 or 253 (STS only, register 18)
- Offset: numeric input −2048 to 2047 (STS only, registers 31-32)

**5.4b — Capture + Go buttons for angle limits**
- `← Capture` reads current position into the min/max field
- `Go →` sends servo to that limit to verify it

**5.4c — Max Torque field in config (STS only)**

**5.5 — Motion Settings**
- Acceleration: numeric input (0–254, STS only, register 41)
- CW / CCW Dead Zones: numeric inputs (0–255, both SC and STS, registers 26-27)

**5.6 — PID Tuning (STS only)**
- P, I, D: numeric inputs (0–255, registers 21-23)
- Speed P, Speed I: numeric inputs (0–255, registers 37, 39)

**5.8b — Set Middle button (STS only, uses `set_offset()`)**

**Library registers still needed:**

| Register | Addr | Size |
|----------|------|------|
| Phase | 18 | 1 |
| P Coefficient | 21 | 1 |
| D Coefficient | 22 | 1 |
| I Coefficient | 23 | 1 |
| Speed P | 37 | 1 |
| Speed I | 39 | 1 |

---

### Phase 6: Check Button

- "Check" button per card (orange/warn style)
- Client-side check against servo config, shows warning banner

**Warnings to check:**
- ID is 1 (factory default)
- Min/Max angle at full range (no limits set)
- Max torque at 1000 (no protection)
- Acceleration is 0 in servo mode (jerky motion)
- EEPROM left unlocked
- Temperature > 55°C
- Voltage < 6.0V
- Min angle ≥ max angle (invalid config)

---

### Future Ideas

- Batch actions: "Center All", "Zero All" buttons above cards
- WebSocket migration if polling latency becomes an issue
- LittleFS for HTML if PROGMEM gets too large
- Preset positions / simple motion sequences for testing
- Visual warnings for over-temperature / over-load