# Часы / Clocks (MT6789)

**Статус: ✅ работает, драйвер переписан с нуля.** Без него не включаются ни
дисплей, ни SD-контроллер, ни USB.
**Status: ✅ working, driver rewritten from scratch.** Without it the display,
the SD controller and USB cannot be clocked.

---

## ЧТО / What

Дерево часов MediaTek состоит из **трёх уровней**, в U-Boot это три отдельных
драйвера в одном файле.

The MediaTek clock tree has **three levels**; in U-Boot they are three separate
drivers in one file.

```text
        XTAL 26 MHz
            │
            ▼
   ┌─────────────────────┐
   │  apmixedsys (PLL)   │  аналоговые PLL: MAINPLL, UNIVPLL, MSDCPLL…
   └─────────────────────┘
            │
            ▼
   ┌─────────────────────┐
   │  topckgen (SEL)     │  делители (fixed_div) + мультиплексоры (mux)
   └─────────────────────┘
            │
            ▼
   ┌─────────────────────┐
   │  infracfg_ao (CG)   │  гейты отдельных блоков: MSDC1, GCE, UART0…
   └─────────────────────┘
```

| Драйвер / Driver | `compatible` | База / Base | Что делает / Does |
|---|---|---|---|
| `mtk_clk_apmixedsys` | `mediatek,mt6789-apmixedsys` | `0x1000c000` | PLL-генераторы / PLL generators |
| `mtk_clk_topckgen` | `mediatek,mt6789-topckgen` | `0x10000000` | фиксированные частоты, делители, муксы / fixed clocks, dividers, muxes |
| `mtk_clk_infracfg` | `mediatek,mt6789-infracfg_ao` | `0x10001000` | гейты `IFRAO0`…`IFRAO5` / infrastructure gates |

Общий каркас — `drivers/clk/mediatek/clk-mtk.c` (не переписан, только логирование).
The shared framework is `drivers/clk/mediatek/clk-mtk.c` (not rewritten, only instrumented).

---

## ПОЧЕМУ переписан / Why it was rewritten

В форке `ellyq/u-boot-mtk-g99` файл `drivers/clk/mediatek/clk-mt6789.c` **не
работал вообще**. Три независимые причины:

In the `ellyq/u-boot-mtk-g99` fork the file `drivers/clk/mediatek/clk-mt6789.c`
**did not work at all**. Three independent reasons:

### 1. Файл не собирался / The file was never built

```makefile
# drivers/clk/mediatek/Makefile — было / was
//obj-$(CONFIG_TARGET_MT6789) += clk-mt6789.o
```

Строка закомментирована двойным слэшем (в Makefile комментарий — это `#`),
и привязана к несуществующему `CONFIG_TARGET_MT6789`. Итог: **драйвер часов
MT6789 не компилировался**, и ни один контроллер не получал тактовую частоту.

The line was "commented out" with a double slash (a Makefile comment is `#`),
and was tied to a non-existent `CONFIG_TARGET_MT6789`. Result: **the MT6789 clock
driver never compiled**, and no controller received a clock.

```makefile
# стало / now
obj-$(CONFIG_ARCH_MEDIATEK) += clk-mt6789.o
```

### 2. Синтаксическая ошибка в C / A syntax error in C

```c
static int mt6789-infracfg_ao_probe(struct udevice *dev)   /* дефис! / hyphen! */
```

Дефис в имени функции недопустим в C. Даже если бы строка Makefile была
раскомментирована, файл не скомпилировался бы. Исправлено на
`mt6789_infracfg_ao_probe`.

A hyphen in a function name is invalid in C. Even with the Makefile line
uncommented the file would not have compiled. Fixed to
`mt6789_infracfg_ao_probe`.

### 3. Содержимое было от другого чипа / The content came from another chip

| | Форк / Fork | У нас / Ours |
|---|---|---|
| Уникальных `CLK_*` ID | 222 | 145 |
| PLL | 13: включая `ARMPLL`, `CCIPLL`, `MPLL` | 10 — только реальные для MT6789 |
| Записей PLL/GATE/MUX | 144 | 91 |

В нашей версии: **10 PLL, 54 делителя, 5 муксов, 68 гейтов**.
Our version: **10 PLLs, 54 dividers, 5 muxes, 68 gates**.

`ARMPLL`, `CCIPLL`, `MPLL` на MT6789 не существуют — файл был смесью таблиц
разных SoC. Оставлено только то, что есть на реальном чипе.

`ARMPLL`, `CCIPLL` and `MPLL` do not exist on MT6789 — the file was a mixture of
tables from different SoCs. Only what exists on the real chip was kept.

`include/dt-bindings/clock/mediatek,mt6789-clk.h` пересобран: **169 строк
удалено, 78 добавлено**.

`include/dt-bindings/clock/mediatek,mt6789-clk.h` was rebuilt: **169 lines removed,
78 added**.

---

## Грабля, которая стоила больше всего / The costliest bug

Нумерация `CLK_*` ID определяет, как драйвер индексирует свои массивы.
`fclks`, `fdivs` и `muxes` — **три последовательных массива**, разделённые
смещениями `fdivs_offs` и `muxes_offs`. ID обязаны идти подряд.

The numbering of `CLK_*` ids determines how the driver indexes its arrays.
`fclks`, `fdivs` and `muxes` are **three consecutive arrays** separated by the
offsets `fdivs_offs` and `muxes_offs`. The ids must be contiguous.

В форке были **пропуски** (31–36, 54–55), из-за чего индекс выходил за границу:

The fork had **gaps** (31–36, 54–55), so the index went out of bounds:

| Симптом / Symptom | Что происходило / What happened |
|---|---|
| `CLK_TOP_MSDC30_1_SEL` = 116, при `muxes_offs` = 95 | индекс **21** при 5 муксах в массиве → чтение мусора / index **21** with 5 muxes → reading garbage |
| `CLK_TOP_F26M` = 63 | попадал в диапазон factor → `fdivs[62]` при 54 делителях → OOB / fell into the factor range → `fdivs[62]` with 54 dividers → OOB |

Результат: **неверные частоты у блоков**, а значит — неработающие контроллеры.
Именно это было одной из причин, почему SD-карта не определялась.

The result: **wrong clock rates for blocks**, hence non-working controllers.
This was one of the reasons the SD card was not detected.

Исправлено в v161 — ID перенумерованы подряд:

Fixed in v161 — the ids were renumbered contiguously:

```text
fclks   0…1     (F26M, CLKRTC)
fdivs   2…55    (54 делителя / 54 dividers)
muxes   56…60   (5 мультиплексоров / 5 muxes)
```

---

## Три уровня подробно / The three levels in detail

### 1. apmixedsys — PLL

10 PLL, все в `apmixed_plls[]`. Значения столбца «Зачем» — для этого проекта.

10 PLLs, all in `apmixed_plls[]`. The "why" column is for this project.

| PLL | Регистр | Для чего в этом проекте / Used in this project for |
|---|---|---|
| `MAINPLL` | `0x340` | AXI, UART, дисплей / AXI, UART, display |
| `UNIVPLL` | `0x308` | USB, дисплей, UART |
| `MSDCPLL` | `0x350` | **SD-карта** — 192 МГц / **SD card** — 192 MHz |
| `MMPLL` | `0x360` | дисплей / display |
| `USBPLL` | `0x3C4` | USB |
| `APLL1`, `APLL2` | `0x318`, `0x32C` | APLL для камеры/аудио / APLL for camera/audio |
| `NPUPLL` | `0x3B4` | нейропроцессор, не используется / neural processor, unused |
| `MFGPLL` | `0x268` | — |
| `TVDPLL` | `0x380` | — |

Диапазон PLL: `MT6789_PLL_FMIN` = 1500 МГц, `FMAX` = 3800 МГц.
PLL range: `MT6789_PLL_FMIN` = 1500 MHz, `FMAX` = 3800 MHz.

### 2. topckgen — делители и мультиплексоры

**Фиксированные частоты** (`top_fixed_clks`):

**Fixed clocks** (`top_fixed_clks`):

| Часы / Clock | Источник / Source | Частота / Rate |
|---|---|---|
| `CLK_TOP_F26M` | `CLK_XTAL` | 26 МГц / 26 MHz |
| `CLK_TOP_CLKRTC` | `CLK_XTAL` | 32 КГц / 32 kHz |

**Делители** — 54 штуки: `MAINPLL_D4…D9`, `UNIVPLL_D4…D7`, `APLL1/2_D2…D8`,
`MMPLL_D4…D9`, `MSDCPLL`, `MSDCPLL_D2/D4`, `OSC_D2…D20`.

**Dividers** — 54 of them: as listed above.

**Мультиплексоры** — 5 штук (`top_muxes[]`):

**Muxes** — 5 (`top_muxes[]`):

| Мукс / Mux | Регистр / Register | Родители / Parents |
|---|---|---|
| `CLK_TOP_AXI_SEL` | `0x18`, биты 0–3 | F26M, MAINPLL_D4_D4/D7_D2/D4_D2/D5_D2/D6_D2, OSC_D4 |
| `CLK_TOP_SPM_SEL` | `0x18`, биты 8–9 | F26M, OSC_D10, MAINPLL_D7_D4, CLKRTC |
| `CLK_TOP_MSDC30_1_SEL` | `0x80` + set/clr/upd | **см. ниже / see below** |
| `CLK_TOP_UART_SEL` | `0x78`, бит 8 | F26M, UNIVPLL_D6_D8 |
| `CLK_TOP_DISP_SEL` | `0x28`, биты 0–3 | F26M, UNIVPLL_D6_D2, MAINPLL_D5_D2, MMPLL_D6_D2, … |

### 3. infracfg_ao — гейты

Шесть групп регистров в базе `0x10001000`:

Six register groups at base `0x10001000`:

| Группа / Group | SET | CLR | STA | Что внутри / Contains |
|---|---|---|---|---|
| IFRAO0 | `+0x80` | `+0x84` | `+0x90` | GCE, UART0–3, PWM, I2C, BTIF, **GCE_26M** |
| IFRAO1 | `+0x88` | `+0x8C` | `+0x94` | **MSDC1**, **MSDC1_SRC**, SPI0, AUXADC, CCIF |
| IFRAO2 | `+0xA4` | `+0xA8` | `+0xAC` | SSUSB, **UFS**, UFS_AES, SPI1–5 |
| IFRAO3 | `+0xC0` | `+0xC4` | `+0xC8` | AP_MSDC0, MD_MSDC0, CCIF2, SPI6/7 |
| IFRAO4 | `+0xE0` | `+0xE4` | `+0xE8` | 66MP bus |
| IFRAO5 | `+0xD0` | `+0xD4` | `+0xD8` | AP_DMA |

Всего 68 гейтов. Для проекта важны:

68 gates in total. The ones that matter for this project:

| Гейт / Gate | Бит / Bit | Что без него / Without it |
|---|---|---|
| `CLK_IFRAO_MSDC1` | IFRAO1, бит 4 | **SD-карта не работает** / **the SD card does not work** |
| `CLK_IFRAO_MSDC1_SRC` | IFRAO1, бит 16 | источник частоты MSDC1 / MSDC1 clock source |
| `CLK_IFRAO_GCE` | IFRAO0, бит 8 | **графика дисплея не включается** / **display graphics does not come up** |
| `CLK_IFRAO_GCE2` | IFRAO0, бит 9 | второй графический блок / second graphics block |
| `CLK_IFRAO_GCE_26M` | IFRAO0, бит 27 | 26 МГц для GCE / 26 MHz for GCE |
| `CLK_IFRAO_UART0` | IFRAO0, бит 22 | UART0 |
| `CLK_IFRAO_UFS` | IFRAO2, бит 28 | UFS |
| `CLK_IFRAO_SSUSB` | IFRAO2, бит 1 | USB |

---

## Особый случай: `MSDC30_1_SEL` / Special case

Большинство муксов topckgen пишутся одним регистром. Этот — исключение,
и именно на нём споткнулась SD-карта.

Most topckgen muxes are written through a single register. This one is the
exception, and it is exactly where the SD card stumbled.

```c
{
        .id = CLK_TOP_MSDC30_1_SEL,
        .mux_reg    = 0x80,
        .mux_set_reg = 0x84,     /* отдельный SET  */
        .mux_clr_reg = 0x88,     /* отдельный CLR  */
        .upd_reg    = 0x04,      /* регистр обновления / update register */
        .upd_shift  = 29,
        .mux_shift  = 8,
        .mux_mask   = 0x7,
        .gate_reg   = 0x80,
        .gate_shift = 15,
        .parent     = msdc30_1_parents,
        .flags      = CLK_MUX_SETCLR_UPD,
},
```

**Что это значит:** запись идёт не прямо в регистр мультиплексора, а через
пару **set/clear**, а затем требуется **апдейт** битом 29 в регистре `0x04`.
Без апдейта мукс не переключается.

**What this means:** the write does not go straight into the mux register but
through a **set/clear** pair, and then an **update** by bit 29 in register `0x04`
is required. Without the update the mux does not switch.

В форке это был обычный `MUX_GATE(..., 0x88, 8, 3, 15)` — один регион,
без set/clr/upd. Отсюда — неверная частота на MSDC1.

In the fork it was a plain `MUX_GATE(..., 0x88, 8, 3, 15)` — a single region
without set/clr/upd. Hence the wrong rate on MSDC1.

Исправлено в v190. Fixed in v190.

---

## Как проверять / How to verify

```bash
# в консоли U-Boot / in the U-Boot console
clk dump            # таблица всех часов и их частот / all clocks and rates
clk tree            # дерево часов / clock tree

clk info MSDC1_SRC  # конкретные часы / a specific clock
clk setrate ...
```

Ранний лог с отладочными метками (добавлены в `clk-mtk.c`):

The early log with debug markers (added in `clk-mtk.c`):

```bash
md.b 0x43fff010 400
```

Ищите строки `mtk_cg:top_rate:mux` и `mtk_cg:top_rate:factor` — они показывают,
по какой ветке вычисляется частота.

Look for `mtk_cg:top_rate:mux` and `mtk_cg:top_rate:factor` — they show which
branch the rate is computed from.

---

## Известные грабли / Known traps

| Грабли / Trap | Что делать / What to do |
|---|---|
| Пропуски в `CLK_*` ID | ID обязаны идти подряд: `fclks` → `fdivs` → `muxes`, иначе OOB / ids must be contiguous, otherwise OOB |
| Часы включаются через `SET` | Гейты IFRAO — через **SET**; но в MSDC есть случаи, когда запись в **CLR** вешает шину / IFRAO gates via **SET**, but in MSDC a write to **CLR** hangs the bus |
| `F26M` = 26 МГц | Это `CLK_XTAL`, не отдельный генератор / it is `CLK_XTAL`, not a separate generator |
| `MSDC30_1_SEL` требует апдейта | Забытый `upd` = мукс не переключился / a forgotten `upd` means the mux never switched |

---

## Связанное / Related

- Решение / decision: [`../decisions/0005-rewrite-mt6789-clock-driver.md`](../decisions/0005-rewrite-mt6789-clock-driver.md)
- Журнал SD-карты / SD card log: [`sd-card-msdc1.md`](sd-card-msdc1.md)
- Дисплей (использует `CLK_TOP_DISP_SEL` и `GCE`): [`display.md`](display.md)
- USB (использует `SSUSB`, `USBPLL`): [`usb.md`](usb.md)
- Кнопки (драйвер сам включает `INFRACFG_AO`): [`buttons.md`](buttons.md)
- Живые данные / live data: `u-boot-x676b_docs/AGENTS/mt6789_msdc1_clock_tree.md`, `msdc1_clock_bus_hang.md`