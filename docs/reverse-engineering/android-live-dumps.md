# Живые дампы с Android / Live dumps from Android

**Источник:** телефон с **работающим стоковым LK и Android 12**, где SD-карта
и дисплей работают правильно.

**Это главный источник правды в проекте.** Там, где дамп Android расходится
с предположением, прав Android.

This is the project's primary source of truth. Where a live dump disagrees with an
assumption, the dump wins.

---

## Как снималось / How it was collected

```bash
# root через KernelSU
adb shell su

# debugfs обязателен — иначе /sys/kernel/debug пуст
mount -t debugfs debugfs /sys/kernel/debug
```

| Что | Откуда | Файл дампа |
|---|---|---|
| Часы | `/sys/kernel/debug/clk/clk_summary` | `clk_summary.txt` |
| Пины / pinctrl | `/sys/kernel/debug/pinctrl/*/pinmux-pins` | `pins.txt` (222 пина) |
| Регуляторы питания | `/sys/kernel/debug/regulator/regulator_summary` | `regulator_summary.txt` |
| PMIC (pwrap) | регистры через `devmem` + I²C-тумблеры | `pwrap_registers.txt` |
| Регистры MSDC1 | значения из вендорского модуля `mtk-mmc` | отдельные файлы по датам |

Полный набор: `docs/AGENTS/android_live_dump_2026-08-08/`

---

## Что дало сравнение / What the comparison yielded

### 1. Часы — MSDCPLL включён

```text
# clk_summary.txt
```

Ключевой вывод: `MSDCSEL`/`MSDCPLL` **в Android включены**. Это опровергло
предположение, что PLL нужно поднимать вручную, и дало источник 192 МГц.

### 2. Пины — реальные состояния

222 зарегистрированных пина с фактическими значениями. Из них взяты режим,
подтяжки и drive strength для `mmc1_pins_default`:

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
        bias-pull-down;          /* ← отличается от стандартного */
};
```

### 3. Питание SD — 2.9 В и 1.8 В

Реальные напряжения с PMIC, а не 3.3 В по умолчанию. Это легло в `pwrap`
init в v191.

### 4. Порядок работы MSDC1 в Android

Отсюда — bind-эталон: состояние контроллера **до** первой команды, снятое
трижды для проверки стабильности.

---

## Главный урок / The main lesson

### Ложные следы

При сравнении «наших» регистров с эталоном Android находились различия —
но часть из них оказалась **артефактами момента измерения**:

| «Отличие» | На самом деле |
|---|---|
| `PATCH_BIT1 = 0xffe64309` | наблюдается **1–2 мс при reset** — автотюнинг вендора |
| `DTOC` = 2 вместо 3 | вендор сам ставит 3, расхождение незначимо |
| `PS` (CMD в покое) = 0 | **норма** для Android |

> ⚠️ **Правило:** снимать регистры нужно **в одинаковый момент**.
> Иначе сравниваешь разные моменты времени и получаешь выводы, которых нет.
>
> **Rule:** registers must be sampled at the **same point in time**. Otherwise
> you compare different moments and draw conclusions that do not exist.

### Что осталось настоящей загадкой

| Наблюдение | Значение |
|---|---|
| Android: `CMD8` → `0x1aa` с первой попытки | карта отвечает |
| U-Boot: `CMD8` → `INT=0x400`, `RSP=0` | ответа нет вовсе |
| `DAT0` в первой попытке поднимается к сэмплу 2–3 | линия живая |
| Вторая попытка: линии прижаты к нулю | карта не отвечает ни битом |

Статические регистры сведены к эталону 1:1 (v219) — **значит проблема не в них**.

---

## Следующий шаг / Next step

Сравнить **внутреннее состояние** через отладочный мукс:

```text
MSDC_DBG_SEL = 0xa0   → записать sel
MSDC_DBG_OUT = 0xa4   → прочитать значение
```

| Среда | sel 0x07 | sel 0x19 | остальные |
|---|---|---|---|
| U-Boot (v219) | 1 | 1 | 0 |
| Android | **снять** | **снять** | **снять** |

Команда `msdcbg` в U-Boot уже делает это. Зеркалит вендорский
`msdc_dump_dbg_register()` (`mtk-mmc-dbg.c`).

---

## Связанное / Related

- Журнал SD: [`../hardware/sd-card-msdc1.md`](../hardware/sd-card-msdc1.md)
- LK: [`lk-disassembly.md`](lk-disassembly.md)
- Сырые дампы: `docs/AGENTS/android_live_dump_2026-08-08/`