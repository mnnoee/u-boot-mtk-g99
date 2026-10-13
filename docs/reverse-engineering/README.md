# Реверс-инжиниринг / Reverse engineering

Откуда взяты адреса, регистры и значения, на которых построен весь проект.
Where the addresses, registers and values behind this project came from.

---

## Два источника / Two sources

В проекте нет ни одной спецификации MT6789. Всё получено двумя путями:

The project has no MT6789 datasheet whatsoever. Everything was obtained two ways:

| | Источник / Source | Что даёт / Gives |
|---|---|---|
| **1** | **Стоковый LK**, дизассемблированный в Ghidra | что и в каком порядке делает загрузчик / what the bootloader does and in what order |
| **2** | **Живой Android** на телефоне | реальные значения, когда всё работает / real values when everything works |

> **Правило:** спорить с живым Android бессмысленно — он работает.
> LK нужен, чтобы понять *почему*, Android — чтобы узнать *что*.
>
> **Rule:** arguing with a working Android is pointless. LK explains *why*,
> Android tells *what*.

---

## Документы / Documents

| Файл / File | О чём / About |
|---|---|
| [`lk-disassembly.md`](lk-disassembly.md) | как читался LK, что в нём нашли / how the LK was read and what was found |
| [`android-live-dumps.md`](android-live-dumps.md) | как снимались регистры с живого Android / how registers were captured from live Android |

---

## Сырые данные / Raw material

Документация, написанная «в моменте», лежит отдельно — её **не нужно** переносить сюда:

Documentation written "in the moment" lives elsewhere and **should not be moved here**:

- `u-boot-x676b_docs/AGENTS/` — заметки по каждой сессии, включая неудачные гипотезы
  / per-session notes, including failed hypotheses
- `u-boot-x676b_docs/AGENTS/android_live_dump_2026-08-08/` — дампы часов, пинов,
  pwrap, регуляторов / clock, pin, pwrap and regulator dumps

Правило проекта: **сырой анализ** — в `AGENTS/`, **выжимка** — здесь.
Project rule: **raw analysis** in `AGENTS/`, **distilled knowledge** here.

---

## Инструменты / Tools

| Задача / Task | Инструмент / Tool |
|---|---|
| Дизассемблирование LK / LK disassembly | Ghidra (`lk_a_lk.bin.c`, ~98 000 строк / lines) |
| Распаковка образов / image unpacking | `liblk`, `Android_boot_image_editor` |
| Работа с телефоном / working with the phone | `adb` + KernelSU root, `mtkclient` |
| Дампы регистров / register dumps | `/sys/kernel/debug/*`, `devmem`, PMIC тумблеры по I²C / PMIC toggles over I²C |
| Управление загрузчиком / bootloader control | `fastboot` по USB, `mtkclient` в режиме preloader |

---

## Связанное / Related

- Таблица соответствий / compatibility matrix: [`../compatibility.md`](../compatibility.md)
- Дисплей / display: [`../hardware/display.md`](../hardware/display.md)
- SD-карта / SD card: [`../hardware/sd-card-msdc1.md`](../hardware/sd-card-msdc1.md)
- Часы / clocks: [`../hardware/clocks.md`](../hardware/clocks.md)