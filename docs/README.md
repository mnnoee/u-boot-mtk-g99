# Документация проекта / Project documentation

Здесь лежит всё, что **не помещается в код**, но без чего нельзя понять, почему код
выглядит именно так.

Everything that does not fit in the code itself, but is required to understand
*why* the code looks the way it does.

---

## Как читать / How to read this

Каждый документ отвечает на три вопроса:

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
├── README.md                  ← вы здесь
├── compatibility.md           ← ГЛАВНАЯ таблица: DT ↔ драйвер ↔ железо ↔ почему
├── hardware/
│   ├── display.md             ← AMOLED, OVL0→RDMA0→DSI0, панель ILI7835F
│   ├── usb.md                 ← USB 2.0 MUSB, fastboot / ACM / UMS
│   ├── buttons.md             ← Volume Up/Down, самописный драйвер KP
│   ├── sd-card-msdc1.md       ← SD-карта: что сломано и куда копать (текущий блокер)
│   └── memory-map.md          ← карта памяти и периферии MT6789
├── decisions/                 ← ADR: контекст → решение → последствия
│   ├── README.md
│   ├── 0001-custom-display-driver.md
│   ├── 0002-usb-pio-only.md
│   ├── 0003-mmc-on-mt8183-compat.md
│   └── 0004-early-log-ring-buffer.md
└── reverse-engineering/
    ├── README.md
    ├── lk-disassembly.md      ← как читался стоковый LK
    └── android-live-dumps.md  ← как снимались регистры с живого Android
```

---

## Три ссылки для быстрого старта

**RU**
1. [`compatibility.md`](compatibility.md) — что за что отвечает и почему именно так.
2. [`hardware/sd-card-msdc1.md`](hardware/sd-card-msdc1.md) — текущий блокер и все
   уже сделанные попытки, чтобы **не повторять** их.
3. [`decisions/README.md`](decisions/README.md) — почему в коде именно такие решения.

**EN**
1. [`compatibility.md`](compatibility.md) — what maps to what, and why.
2. [`hardware/sd-card-msdc1.md`](hardware/sd-card-msdc1.md) — the current blocker plus
   every attempt already made, so they are **not repeated**.
3. [`decisions/README.md`](decisions/README.md) — rationale behind the code as written.

---

## Связанные репозитории / Related repositories

| Репозиторий | Содержимое / Contents |
|---|---|
| **этот / this** | исходники U-Boot, драйвер дисплея, DTS, драйверы USB/MMC |
| `u-boot-x676b` | рабочие образы `.img`, `PROJECT_STATE.md`, скрипты сборки |
| `docs/AGENTS/` | сырые заметки по каждой сессии, живые дампы регистров Android |

> Правило проекта: **сырой анализ** живёт в `docs/AGENTS/`, **выжимка** — здесь.
> Project rule: **raw analysis** lives in `docs/AGENTS/`, **distilled** knowledge — here.