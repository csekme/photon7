# PHOTON-7 LANDER Projekt

## Mi ez?

Egy ESP32 mikrokontrollerre épülő "űrszonda" stílusú környezeti monitor, sci-fi hangulatú felhasználói felülettel. A projekt egy 0.91"-os OLED kijelzőn jelenít meg valós idejű szenzor adatokat, animációkkal és interaktív menürendszerrel.

---

## Hardver komponensek

| Komponens | GPIO | Funkció |
|-----------|------|---------|
| ESP32 NodeMCU WROOM | - | Vezérlő |
| OLED 0.91" (SSD1306) | SDA: 21, SCL: 22 | Kijelző (I2C) |
| BME280 | SDA: 21, SCL: 22 | Hőmérséklet, páratartalom, légnyomás (I2C) |
| Fényellenállás (LDR) | GPIO34 | Környezeti fény mérés |
| Piros LED | GPIO25 | Műhold-stílusú villogó jelzőfény |
| Nyomógomb 1 | GPIO32 | Menü / balra lapozás |
| Nyomógomb 2 | GPIO33 | Kiválasztás / jobbra lapozás |

---

## Bekötési rajz

```
                    ┌─────────────────────┐
                    │      ESP32          │
                    │                     │
    ┌───────┐       │                     │
    │BME280 │       │                     │       ┌─────┐
    │  VCC──┼───────┼──3.3V               │       │ LDR │
    │  GND──┼───────┼──GND                │       └──┬──┘
    │  SDA──┼───┬───┼──GPIO21             │          │
    │  SCL──┼───┼─┬─┼──GPIO22      GPIO34─┼──────────┴───┐
    └───────┘   │ │ │                     │              │
                │ │ │                     │           [40kΩ]
    ┌───────┐   │ │ │                     │              │
    │ OLED  │   │ │ │                     │             GND
    │  VCC──┼───┼─┼─┼──3.3V               │
    │  GND──┼───┼─┼─┼──GND                │
    │  SDA──┼───┘ │ │                     │       LED
    │  SCL──┼─────┘ │              GPIO25─┼──[220Ω]──►|──GND
    └───────┘       │                     │         (piros)
                    │                     │
                    │              GPIO32─┼────[BTN1]────GND
                    │              GPIO33─┼────[BTN2]────GND
                    │                     │
                    │                3.3V─┼───── LDR másik lába
                    └─────────────────────┘
```

---

## Funkciók

### Boot szekvencia

- Sci-fi stílusú indítási animáció
- "PHOTON-7 LANDER" felirat villogással
- Terrain mapping effekt
- Több lépcsős progress bar animáció:
  - Reaktor init
  - Környezet scan
  - Légkör analízis
  - Gravitáció kalibráció
  - Pajzs diagnosztika
  - Szenzor szinkron
  - Kommunikációs csatorna
  - Rendszer stabil
- "System ONLINE" visszajelzés

### Képernyők (lapozhatók)

1. **Dashboard** - Összesített nézet:
   - P-7 fejléc pulzáló jelzőfénnyel
   - Fényerő grafikon (élő)
   - Forgó radar animáció
   - Hőmérséklet (°C)
   - Páratartalom (%)
   - Légnyomás (hPa)

2. **Hőmérséklet**
   - Hőmérő ikon animált kitöltéssel
   - Érték °C-ban
   - 15-35°C skála jelzővel

3. **Páratartalom**
   - Vízcsepp ikon
   - Érték %-ban
   - Progress bar vizualizáció

4. **Légnyomás**
   - Barométer ikon mutatóval
   - Érték hPa-ban
   - L/N/H (Low/Normal/High) skála

5. **Fényerő**
   - Nap ikon sugarakkal
   - Aktuális LDR érték
   - Élő grafikon az utolsó mérésekről

### Éjszakai mód

- Automatikusan aktiválódik alacsony fényviszonyoknál (< 2500 LDR érték)
- Megjelenés: halvány hold sarló és villogó csillagok
- Kikapcsolható a menüből

### Menürendszer

- Hosszan nyomva BTN1 → menü megnyitás
- Opciók:
  - **LED** - Jelzőfény BE/KI kapcsolása
  - **EJJ** - Éjszakai mód BE/KI kapcsolása
  - **BACK** - Vissza a főképernyőre

### LED jelzőfény

- Műhold-stílusú dupla villanás minta:
  - 50ms ON
  - 100ms OFF
  - 50ms ON
  - 1800ms OFF (hosszú szünet)
- Szinkronban a kijelzőn látható P-7 melletti pöttyel

---

## Irányítás

| Művelet | Gomb |
|---------|------|
| Balra lapozás | BTN1 röviden |
| Jobbra lapozás | BTN2 röviden |
| Menü megnyitás | BTN1 hosszan (0.8s) |
| Menüben navigálás | BTN1 röviden |
| Menüpont kiválasztás | BTN2 röviden |

---

## Szoftveres megoldások

### Debounce (pergésmentesítés)
A mechanikus gombok többszörös jelzést adhatnak egyetlen nyomásra. A kód 50ms-os debounce idővel szűri ezeket.

### LDR simítás
Exponenciális átlagolás a stabil fényérték méréshez:
```cpp
ldrSmoothed = (ldrSmoothed * 7 + rawLdr) / 8;
```

### Dinamikus grafikon
A fényerő grafikon automatikusan skálázódik a min/max értékekhez, így mindig látható a változás.

### I2C megosztás
Az OLED és BME280 közös I2C buszon működik (SDA: GPIO21, SCL: GPIO22), de különböző címeken:
- OLED: 0x3C
- BME280: 0x76 (vagy 0x77)

---

## Szükséges könyvtárak

- `Wire.h` - I2C kommunikáció
- `Adafruit_GFX.h` - Grafikus primitívek
- `Adafruit_SSD1306.h` - OLED driver
- `Adafruit_BME280.h` - BME280 szenzor driver

Arduino IDE-ben telepítés: Sketch → Include Library → Manage Libraries

---

## Konfigurálható értékek

```cpp
#define NIGHT_THRESHOLD 2500    // Éjszakai mód küszöbérték
#define LONG_PRESS_TIME 800     // Hosszú gombnyomás (ms)
#define DEBOUNCE_TIME 50        // Pergésmentesítés (ms)
```

---

## Tippek és trükkök

- **Beépített LED kikapcsolása**: A NodeMCU WROOM-on a GPIO2-n lévő kék LED kikapcsolható:
  ```cpp
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
  ```

- **BME280 cím**: Ha nem találja a szenzort, ellenőrizd az I2C címét (0x76 vagy 0x77)

- **Fényérzékenység**: A `NIGHT_THRESHOLD` értékét a környezeti fényviszonyoknak megfelelően állítsd

---

## Összegzés

Egy kompakt, vizuálisan látványos "space gadget" ami valós környezeti adatokat mér és jelenít meg. Tökéletes asztali dekorációnak, tanulóprojektnek, vagy kiindulási alapnak komolyabb IoT alkalmazásokhoz.

---

*Készült: 2025*
