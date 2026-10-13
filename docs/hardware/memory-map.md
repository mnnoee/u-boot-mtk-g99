# Карта памяти и периферии / Memory and peripheral map

Адреса, с которыми работает проект. Всё — из DT, реального устройства и
дизассемблированного LK. **Спецификации MT6789 нет.**

Addresses this project works with. Everything comes from the DT, the real device
and the disassembled LK. **There is no MT6789 datasheet.**

---

## Что используется / What is used

| Подсистема / Subsystem | База / Base | В DTS / In DT | Драйвер / Driver |
|---|---|---|---|
| **MMSYS** (мультиплексоры дисплея) | `0x14000000` | — | `mtk_disp_g99.c` |
| ├ MUTEX0 | `0x14001000` | — | там же |
| ├ OVL0 | `0x14005000` | — | там же |
| └ RDMA0 | `0x14007000` | — | там же |
| **DSI0** | `0x14013000` | — | инициализирует **LK** |
| **MSDC1** (SD-карта) | `0x11240000` | `mmc@11240000` | `drivers/mmc/mtk-sd.c` |
| **MSDC_TOP** | `0x11ef0000` | второй `reg` узла | там же |
| **MUSB** (USB 2.0) | `0x11200000` | `usb0@11200000` | `musb-new/mt85xx.c` |
| **UFS** | `0x11270000` | `ufshci@11270000` | `ufs/ufs-mediatek.c` |
| **T-PHY** (USB) | `0x11f40000` | `t-phy@11f40000` | `drivers/phy/mediatek/` |
| **KP** (кнопки) | `0x10010000` | узлы `volume-up` / `volume-down` | `drivers/button/button-mtk-kp.c` |
| **UART0** | `0x11002000` | `serial@11002000` | стандартный |
| **SPM** (питание) | `0x10006000` | — | — |
| **APMIXEDSYS** (PLL) | `0x1000c000` | `syscon@1000c000` | `drivers/clk/mediatek/clk-mt6789.c` |
| **TOPCKGEN** (мультиплексоры часов) | `0x10000000` | `syscon@10000000` | там же |
| **INFRACFG_AO** (гейты) | `0x10001000` | `syscon@10001000` | там же |

---

## Ключевые регистры дисплея / Display registers

Подробно: [`display.md`](display.md). Главное:

| Регистр | Адрес |
|---|---|
| `MMSYS_CLK_CLR` | `0x14000108` |
| `MMSYS_SW_RST` | `0x14000140` |
| `OVL0_L0_ADDR` | `0x14005f40` |
| `DSI0_CMDQ` | **`0x14013d00`** (не `0x200`!) |
| `DISP_PWR_CON` | `0x10006354` |

---

## Буферы в DRAM / DRAM buffers

| Адрес | Размер | Назначение |
|---|---|---|
| `0x43fff000` | 16 МБ | **ранний лог** [`early_log`](../decisions/0004-early-log-ring-buffer.md) |
| `0xfe0c8000` | 31 МиБ | резерв кадра в `reserved-memory` |
| `0xfeabe000` | 10.4 МиБ | кадр, который поставил LK (`0xfeabe000` + `4320×2400`) |
| `0xff4a1400` | — | диагностический буфер драйвера дисплея (magic `0xDEADBEEF`) |

> ⚠️ `0x43fff000` — это **16 МБ, занятые навсегда**. При желании можно уменьшить
> в `include/early_log.h`.

---

## Системные области из `reserved-memory`

| Узел | Адрес | Размер | Флаги |
|---|---|---|---|
| `lk@50700000` | `0x50700000` | 8 МБ | `no-map` |
| `aee_lk@50700000` | `0x50700000` | 8 МБ | `no-map` |
| `tee@70000000` | `0x70000000` | 96 МБ | `no-map` |
| `tee_sec@7de00000` | `0x7de00000` | 8 МБ | `no-map` |
| `mcupm@7feb0000` | `0x7feb0000` | 1 МБ | `no-map` |
| `sspm@9fe70000` | `0x9fe70000` | 1.5 МБ | `no-map` |
| `bl31@48200000` | `0x48200000` | 2 МБ | `no-map` |
| `drambuf@48500000` | `0x48500000` | — | — |
| `scp@bfa00000` | `0xbfa00000` | — | — |
| `fb_mem@fe0c8000` | `0xfe0c8000` | 31 МиБ | **без `no-map`** — доступен U-Boot |

> `no-map` означает, что область **не отображается** в MMU. Такую память
> нельзя читать через указатель — только `readl` по физическому адресу.
> Именно поэтому ранний лог сделан на MMIO, а не на `memcpy`.

---

## DRAM / Memory

| | |
|---|---|
| Реально на телефоне | 8 ГБ (два банка по 4 ГБ) |
| Что объявлено в DT | `reg = <0x0 0x40000000 0x1 0x0>` — **4 ГБ** |
| U-Boot берёт размер | `fdtdec_setup_mem_size_base()` — то есть **4 ГБ из DT** |

> ⚠️ **Известное расхождение:** телефон имеет 8 ГБ, а DT объявляет 4 ГБ.
> Для U-Boot это безопасно — всё, что нужно загрузчику, помещается в первые
> 4 ГБ. Но если появится задача на «повышенные» регионы DRAM (например,
> framebuffer за пределами 4 ГБ), DT придётся править.
>
> ⚠️ **Known mismatch:** the phone has 8 GB, the DT declares 4 GB. Harmless for
> U-Boot, but code that needs memory above the 4 GB boundary would require a DT
> fix.

---

## Связанное / Related

- Дисплей: [`display.md`](display.md)
- USB: [`usb.md`](usb.md)
- SD-карта: [`sd-card-msdc1.md`](sd-card-msdc1.md)