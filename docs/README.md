# Документация проекта / Project documentation

Здесь лежит всё, что **не помещается в код**, но без чего нельзя понять, почему код
выглядит именно так.

Everything that does not fit in the code itself, but is required to understand *why* the
code looks the way it does.

---

## Как читать / How to read this

Каждый документ отвечает на три вопроса. Ниже русский текст, под ним —
английский, без меток.

Each document answers three questions. Russian text comes first, English below, unmarked.

| Вопрос / Question | О чём / About | Где / Where |
|---|---|---|
| **ЧТО** / *what* | что это за железо или драйвер | [`hardware/`](hardware/) |
| **С КЕМ и КОГО** / *with what, which driver* | какой узел DT ↔ какой compatible ↔ какой драйвер ↔ что реально стоит в телефоне | [`compatibility.md`](compatibility.md) |
| **ПОЧЕМУ** / *why* | почему принято именно такое решение, а не «правильное» | [`decisions/`](decisions/) |
| **КАК получено** / *how it was obtained* | откуда взяты адреса, регистры и значения | [`reverse-engineering/`](reverse-engineering/) |

---

## Карта репозитория / Repository map

```
docs/
├── README.md                  ← вы здесь / you are here
├── compatibility.md           ← ГЛАВНАЯ таблица: DT ↔ драйвер ↔ железо ↔ почему
├── hardware/
│   ├── display.md             ← AMOLED, OVL0→RDMA0→DSI0, панель ILI7835F
│   ├── usb.md                 ← USB 2.0 MUSB, fastboot / ACM / UMS
│   ├── buttons.md             ← Volume Up/Down, самописный драйвер KP
│   ├── clocks.md              ← дерево часов, переписанный драйвер
│   ├── sd-card-msdc1.md       ← SD-карта: что сломано и куда копать (блокер)
│   └── memory-map.md          ← карта памяти и периферии MT6789
├── decisions/                 ← ADR: контекст → решение → последствия
│   ├── README.md
│   ├── 0001-custom-display-driver.md
│   ├── 0002-usb-pio-only.md
│   ├── 0003-mmc-on-mt8183-compat.md
│   ├── 0004-early-log-ring-buffer.md
│   └── 0005-rewrite-mt6789-clock-driver.md
└── reverse-engineering/
    ├── README.md
    ├── lk-disassembly.md      ← как читался стоковый LK
    └── android-live-dumps.md  ← как снимались регистры с живого Android
```

---

## Три ссылки для быстрого старта

1. [`compatibility.md`](compatibility.md) — что за что отвечает и почему именно так.
2. [`hardware/sd-card-msdc1.md`](hardware/sd-card-msdc1.md) — текущий блокер и все
   уже сделанные попытки, чтобы **не повторять** их.
3. [`decisions/README.md`](decisions/README.md) — почему в коде именно такие решения.

1. [`compatibility.md`](compatibility.md) — what maps to what, and why.
2. [`hardware/sd-card-msdc1.md`](hardware/sd-card-msdc1.md) — the current blocker plus
   every attempt already made, so they are **not repeated**.
3. [`decisions/README.md`](decisions/README.md) — rationale behind the code as written.

---

## Что в проекте самописное / What is custom in this project

В форке `ellyq/u-boot-mtk-g99` **не было** ни одного из этих драйверов — они
написаны с нуля для X676B.

None of these drivers existed in the `ellyq/u-boot-mtk-g99` fork — all were written
from scratch for the X676B.

| Файл / File | Строк / Lines | Назначение / Purpose |
|---|---|---|
| `drivers/video/mediatek/mtk_disp_g99.c` | 453 | дисплейный тракт OVL0→RDMA0→DITHER0→DSI0 / display pipeline |
| `drivers/button/button-mtk-kp.c` | 204 | кнопки Volume Up/Down через KP-контроллер / keypad buttons |
| `include/early_log.h` | 72 | буфер раннего лога в DRAM / early log ring buffer |
| `configs/mt6789_infinix_x676b_defconfig` | 130 | конфигурация сборки под устройство / device build config |
| `arch/arm/dts/mt6789-infinix-x676b.dts` | 229 | device tree устройства / device tree |
| `drivers/clk/mediatek/clk-mt6789.c` | переписан | часы под реальные гейты / clocks rewritten |

Также переписаны или сильно изменены: `drivers/mmc/mtk-sd.c` (+442),
`drivers/usb/musb-new/mt85xx.c` (239), `include/dt-bindings/clock/mediatek,mt6789-clk.h` (247).

Also rewritten or heavily modified: `drivers/mmc/mtk-sd.c` (+442 lines),
`drivers/usb/musb-new/mt85xx.c` (239), `include/dt-bindings/clock/mediatek,mt6789-clk.h` (247).

---

## Связанные репозитории / Related repositories

| Репозиторий / Repository | Содержимое / Contents |
|---|---|
| [**u-boot-mtk-g99**](https://github.com/mnnoee/u-boot-mtk-g99) | исходники U-Boot, самописные драйверы, DTS / U-Boot sources, custom drivers |
| [**u-boot-x676b**](https://github.com/mnnoee/u-boot-x676b) | рабочие образы `.img`, `PROJECT_STATE.md`, скрипты / images, scripts |
| [**u-boot-x676b_docs**](https://github.com/mnnoee/u-boot-x676b_docs) | сырые заметки по сессиям, живые дампы Android / raw session notes, live dumps |
| [**u-boot-x676b_dtbo**](https://github.com/mnnoee/u-boot-x676b_dtbo) | Device Tree и оверлеи / device tree and overlays |

> Правило проекта: **сырой анализ** живёт в `u-boot-x676b_docs/AGENTS/`,
> **выжимка** — здесь.
>
> Project rule: **raw analysis** lives in `u-boot-x676b_docs/AGENTS/`,
> **distilled knowledge** — here.