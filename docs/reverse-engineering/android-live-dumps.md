# Живые дампы с Android / Live dumps from Android

**Источник:** телефон с **работающим стоковым LK и Android 12**, где SD-карта
и дисплей работают правильно.

**Source:** a phone running the **stock LK and Android 12**, where the SD card
and the display work correctly.

Если дамп расходится с предположением — прав дамп: Android работает.

Where a live dump disagrees with an assumption, the dump wins: Android works.

---

## Как снималось / How it was collected

```bash
# root через KernelSU / root via KernelSU
adb shell su

# debugfs обязателен — иначе /sys/kernel/debug пуст
# debugfs is required, otherwise /sys/kernel/debug is empty
mount -t debugfs debugfs /sys/kernel/debug
```

| Что / What | Откуда / From where | Файл дампа / Dump file |
|---|---|---|
| Часы / Clocks | `/sys/kernel/debug/clk/clk_summary` | `clk_summary.txt` |
| Пины / pinctrl | `/sys/kernel/debug/pinctrl/*/pinmux-pins` | `pins.txt` (222 пина / pins) |
| Регуляторы / Regulators | `/sys/kernel/debug/regulator/regulator_summary` | `regulator_summary.txt` |
| PMIC (pwrap) | регистры через `devmem` + I²C-тумблеры / registers via `devmem` + I²C toggles | `pwrap_registers.txt` |
| Регистры MSDC1 | значения из вендорского модуля `mtk-mmc` / values from the vendor `mtk-mmc` module | отдельные файлы по датам / dated files |

Полный набор / full set: `u-boot-x676b_docs/AGENTS/android_live_dump_2026-08-08/`

---

## Что дало сравнение / What the comparison yielded

### 1. Часы — MSDCPLL включён / Clocks — MSDCPLL is on

```text
# clk_summary.txt
```

Ключевой вывод: `MSDCSEL`/`MSDCPLL` **в Android включены**. Это опровергло
предположение, что PLL нужно поднимать вручную, и дало источник 192 МГц.

Key finding: `MSDCSEL`/`MSDCPLL` **are enabled in Android**. This disproved the
assumption that the PLL has to be raised manually and provided the 192 MHz source.

### 2. Пины — реальные состояния / Pins — real states

222 зарегистрированных пина с фактическими значениями. Из них взяты режим,
подтяжки и drive strength для `mmc1_pins_default`:

222 registered pins with actual values. From these, the mode, pull-ups and drive
strength for `mmc1_pins_default` were taken:

```dts
pins-cmd-dat {
        pinmux = <0x4901 0x4a01 0x4b01 0x4c01 0x4801>;
        input-enable;
        drive-strength = <3>;
        bias-pull-up;
};
pins-clk {
        pinmux = <0x4701>;
        drive-strength = <3>;
        bias-pull-down;          /* ← отличается от стандартного / differs from the standard */
};
```

### 3. Питание SD — 2.9 В и 1.8 В / SD power — 2.9 V and 1.8 V

Реальные напряжения с PMIC, а не 3.3 В по умолчанию. Это легло в `pwrap`
init в v191.

Real voltages from the PMIC rather than the 3.3 V default. This went into the
`pwrap` init in v191.

### 4. Порядок работы MSDC1 в Android / MSDC1 behaviour order in Android

Отсюда — bind-эталон: состояние контроллера **до** первой команды, снятое
трижды для проверки стабильности.

From this, the bind reference: the controller state **before** the first command,
captured three times to check stability.

---

## Главный урок / The main lesson

### Ложные следы / Red herrings

При сравнении «наших» регистров с эталоном Android находились различия —
но часть из них оказалась **артефактами момента измерения**:

Differences showed up when comparing "our" registers with the Android reference —
but some turned out to be **artefacts of when the measurement was taken**:

| «Отличие» / "Difference" | На самом деле / In reality |
|---|---|
| `PATCH_BIT1 = 0xffe64309` | наблюдается **1–2 мс при reset** — автотюнинг вендора / visible for **1–2 ms during reset** — vendor auto-tuning |
| `DTOC` = 2 вместо 3 / = 2 instead of 3 | вендор сам ставит 3, расхождение незначимо / the vendor sets 3, the divergence is insignificant |
| `PS` (CMD в покое) = 0 / `PS` (CMD idle) = 0 | **норма** для Android / **normal** for Android |

> ⚠️ **Правило:** снимать регистры нужно **в одинаковый момент**.
> Иначе сравниваешь разные моменты времени и получаешь выводы, которых нет.
>
> ⚠️ **Rule:** registers must be sampled at the **same point in time**. Otherwise
> you compare different moments and draw conclusions that do not exist.

### Что осталось настоящей загадкой / What remains a genuine mystery

| Наблюдение / Observation | Значение / Meaning |
|---|---|
| Android: `CMD8` → `0x1aa` с первой попытки / first attempt | карта отвечает / the card answers |
| U-Boot: `CMD8` → `INT=0x400`, `RSP=0` | ответа нет вовсе / no response at all |
| `DAT0` в первой попытке поднимается к сэмплу 2–3 / rises by sample 2–3 | линия живая / the line is alive |
| Вторая попытка: линии прижаты к нулю / second attempt: lines pulled low | карта не отвечает ни битом / the card does not answer with a single bit |

Статические регистры сведены к эталону 1:1 (v219) — **значит проблема не в них**.

The static registers were matched 1:1 to the reference in v219 — so **the problem
is not in them**.

---

## Следующий шаг / Next step

Сравнить **внутреннее состояние** через отладочный мукс:

Compare the **internal state** via the debug mux:

```text
MSDC_DBG_SEL = 0xa0   → записать sel / write sel
MSDC_DBG_OUT = 0xa4   → прочитать значение / read the value
```

| Среда / Environment | sel 0x07 | sel 0x19 | остальные / rest |
|---|---|---|---|
| U-Boot (v219) | 1 | 1 | 0 |
| Android | **снять / capture** | **снять / capture** | **снять / capture** |

Команда `msdcbg` в U-Boot уже делает это. Зеркалит вендорский
`msdc_dump_dbg_register()` (`mtk-mmc-dbg.c`).

The `msdcbg` command in U-Boot already does this. It mirrors the vendor
`msdc_dump_dbg_register()` (`mtk-mmc-dbg.c`).

---

## Связанное / Related

- Журнал SD / SD log: [`../hardware/sd-card-msdc1.md`](../hardware/sd-card-msdc1.md)
- LK: [`lk-disassembly.md`](lk-disassembly.md)
- Сырые дампы / raw dumps: `u-boot-x676b_docs/AGENTS/android_live_dump_2026-08-08/`