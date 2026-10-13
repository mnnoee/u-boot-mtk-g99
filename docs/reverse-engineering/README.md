# Реверс-инжиниринг / Reverse engineering

Откуда взяты адреса, регистры и значения, на которых построен весь проект.
Where the addresses, registers and values behind this project came from.

---

## Два источника / Two sources

В проекте нет ни одной спецификации MT6789. Всё получено двумя путями:

| | Источник / Source | Что даёт / Gives |
|---|---|---|
| **1** | **Стоковый LK**, дизассемблированный в Ghidra | что и в каком порядке делает загрузчик |
| **2** | **Живой Android** на телефоне | реальные значения, когда всё работает |

> **Правило:** спорить с живым Android бессмысленно — он работает.
> LK нужен, чтобы понять *почему*, Android — чтобы узнать *что*.
>
> **Rule:** arguing with a working Android is pointless. LK explains *why*,
> Android tells *what*.

---

## Документы / Documents

| Файл | О чём / About |
|---|---|
| [`lk-disassembly.md`](lk-disassembly.md) | как читался LK, что в нём нашли |
| [`android-live-dumps.md`](android-live-dumps.md) | как снимались регистры с живого Android |

---

## Сырые данные / Raw material

Документация, написанная «в моменте», лежит отдельно — её **не нужно** переносить сюда:

- `docs/AGENTS/` — заметки по каждой сессии, включая неудачные гипотезы
- `docs/AGENTS/android_live_dump_2026-08-08/` — дампы часов, пинов, pwrap, регуляторов

Правило проекта: **сырой анализ** — в `AGENTS/`, **выжимка** — здесь.
Project rule: **raw analysis** in `AGENTS/`, **distilled knowledge** here.

---

## Инструменты / Tools

| Задача / Task | Инструмент |
|---|---|
| Дизассемблирование LK | Ghidra (`lk_a_lk.bin.c`, ~98 000 строк) |
| Распаковка образов | `liblk`, `Android_boot_image_editor` |
| Работа с телефоном | `adb` + KernelSU root, `mtkclient` |
| Дампы регистров | `/sys/kernel/debug/*`, `devmem`, тумблеры PMIC по I²C |
| Управление загрузчиком | `fastboot` по USB, `mtkclient` в режиме preloader |

---

## Связанное / Related

- Таблица соответствий: [`../compatibility.md`](../compatibility.md)
- Дисплей: [`../hardware/display.md`](../hardware/display.md)
- SD-карта: [`../hardware/sd-card-msdc1.md`](../hardware/sd-card-msdc1.md)