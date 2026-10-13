# Дисплей / Display

**Статус: ✅ работает.** Текст U-Boot выводится на AMOLED 1080×2400.
**Status: ✅ working.** U-Boot text renders on the 1080×2400 AMOLED.

---

## ЧТО / What

| RU | EN |
|---|---|
| Панель: ILI7835F | Panel: ILI7835F |
| Интерфейс: DSI, 2 канала | Interface: DSI, 2 lanes |
| Разрешение: 1080 × 2400 | Resolution: 1080 × 2400 |
| Формат: ARGB8888, 32 бит/пиксель | Format: ARGB8888, 32 bits/pixel |
| Шаг строки: 1080 × 4 = **4320 байт** | Stride: 1080 × 4 = **4320 bytes** |
| Размер буфера: 4320 × 2400 = **10 368 000 байт** (~9.9 МиБ) | Buffer size: 4320 × 2400 = **10 368 000 bytes** (~9.9 MiB) |
| Адрес буфера: `0xfeabe000` — то, что ставит LK | Buffer address: `0xfeabe000` — the one LK programs |
| Резерв в DT: `framebuffer@fe0c8000`, `0x1f2f000` (31 МиБ) | DT reservation: `framebuffer@fe0c8000`, `0x1f2f000` (31 MiB) |
| Диапазон памяти: MMSYS `0x14000000` | Memory range: MMSYS `0x14000000` |

### Два адреса — не путайте / Two addresses, do not confuse

| RU | EN |
|---|---|
| `0xfe0c8000` — **начало зарезервированной области** в `reserved-memory` | `0xfe0c8000` — **start of the reserved area** in `reserved-memory` |
| `0xfeabe000` — **адрес кадра, который поставил LK**, и дефолт драйвера | `0xfeabe000` — **framebuffer address LK programmed**, and the driver default |
| `0xfeabe000` лежит **внутри** резерва `0xfe0c8000…0xfffb7000` — конфликта нет, но путать их легко | `0xfeabe000` lies **inside** the reservation `0xfe0c8000…0xfffb7000` — no conflict, but easy to mix up |
| Драйверу важен тот адрес, который реально стоит в `OVL0_L0_ADDR` | What matters to the driver is the address actually present in `OVL0_L0_ADDR` |

---

## Кто чем занимается / Who does what

Главная особенность: **DSI-контроллер и панель инициализирует не U-Boot, а LK**,
который грузится раньше. Наш драйвер не инициализирует панель заново — он
перенастраивает только тракт передачи кадра.

The key point: **the DSI controller and the panel are initialised by LK, not by
U-Boot** — LK runs first. Our driver does not re-initialise the panel; it only
reconfigures the frame transfer path.

| Блок / Block | База / Base | Кто настраивает / Configured by |
|---|---|---|
| `MMSYS` | `0x14000000` | наш драйвер — гейты, сброс, мультиплексоры / our driver — gates, reset, muxes |
| `MUTEX0` | `0x14001000` | наш драйвер — блокировки / our driver — mutexes |
| `OVL0` | `0x14005000` | наш драйвер — слой 0, адрес кадра / our driver — layer 0, frame address |
| `RDMA0` | `0x14007000` | наш драйвер — чтение кадра из DRAM / our driver — reads the frame from DRAM |
| `DSI0` | `0x14013000` | **LK** — тайминги, PLL, сам DSI / **LK** — timings, PLL, DSI itself |
| Панель ILI7835F | через DSI | **LK** — команды инициализации панели / **LK** — panel init commands |

```
   framebuffer (0xfeabe000, 10.4 МБ / MiB)
            │
            ▼
        OVL0  ──── слой 0: адрес, размер, формат
            │     выход через MUTEX0
            │     layer 0: address, size, format
            │     output through MUTEX0
            ▼
       RDMA0  ──── читает пиксели из DRAM
            │     reads pixels from DRAM
            ▼
      DITHER0 ──── преобразование в формат панели
            │     conversion to panel format
            ▼
        DSI0  ──── уже настроен LK, шлёт команды в панель
            │     already set up by LK, sends commands to the panel
            ▼
      ILI7835F ─── AMOLED 1080×2400
```

---

## ПОЧЕМУ свой драйвер / Why a custom driver

| Проблема / Problem | Решение / Solution |
|---|---|
| В форке **нет** DRM-драйвера для MT6789 — ни `mtk_disp`, ни панельных | Написан минимальный драйвер `UCLASS_VIDEO` — 453 строки / minimal `UCLASS_VIDEO` driver — 453 lines |
| `simple-framebuffer` из DT давал **Synchronous Abort**: адрес `0xfe0c8000` не отображён в MMU U-Boot | Узел с `compatible = "simple-framebuffer"` **удалён из DTS**. Область `0xfe0c8000` осталась в `reserved-memory`, но **без `no-map`**, чтобы U-Boot мог обращаться / The node was removed; the area stays reserved but **without `no-map`** so U-Boot can reach it |
| DSI полностью настроен LK — повторная инициализация ломала картинку (пробовались `reset + CMDQ_SIZE 0x0001 + CON_CTRL DSI_EN`) | Триггер оставлен как в v183: `CMDQ_SIZE 0x8001` + переключение `DSI_START` / trigger left as in v183 |
| `OVL0 SRC_CON` был `0x3` (два слоя от LK) | Ставится `0x3` — слой 0 + слой 1, LK оставил их активными / stays `0x3` — layer 0 + layer 1, left active by LK |
| `OVL0 L0_CON = 0x1001020` не подходил для ARGB8888 | Заменён на `0x000001ff` — 32 бита, 1080×2400 / replaced with `0x000001ff` — 32-bit, 1080×2400 |

---

## Как устроен драйвер / How the driver works

### Порядок инициализации / Init sequence

1. **Проверка MMIO** — читаем `MMSYS_EN`, если недоступно, выходим.
   **MMIO check** — read `MMSYS_EN`, bail out if inaccessible.
2. **Определение адреса кадра** — берём из `OVL0_L0_ADDR`, иначе дефолт:
   **Determine the framebuffer address** — from `OVL0_L0_ADDR`, otherwise the default:
   ```c
   fb_phys = readl(OVL0_L0_ADDR);
   if (!fb_phys || fb_phys == 0xffffffff)
           fb_phys = LK_DEFAULT_FB;   /* 0xfeabe000 */
   ```
3. **Проверка диапазона** — вне `0x40000000 … 0x2fffffffff` → откат на `LK_DEFAULT_FB`.
   **Range check** — outside `0x40000000 … 0x2fffffffff` → fall back to `LK_DEFAULT_FB`.
4. **Настройка конвейера** — гейты, сброс, мультиплексоры, слой 0, RDMA0.
   **Pipeline setup** — gates, reset, muxes, layer 0, RDMA0.
5. **Отрисовка** — vidconsole рисует символы, затем `dsync` шлёт кадр через DSI.
   **Drawing** — vidconsole renders glyphs, then `dsync` pushes the frame over DSI.

### Ключевые регистры / Key registers

| Регистр / Register | Адрес / Address | Зачем / Purpose |
|---|---|---|
| `MMSYS_CLK_CLR` | `0x14000108` | снять гейт (управление через **CLR**, не SET) / clear gate (control via **CLR**, not SET) |
| `MMSYS_SW_RST` | `0x14000140` | write-1-to-deassert |
| `MMSYS_CFG_UPDATE` | `0x1400f0c` | загрузка теневых настроек мультиплексоров / shadow load of mux settings |
| `MMSYS_OVL0_MOUT` | `0x1400f18` | выход OVL0 / OVL0 output |
| `MMSYS_RDMA0_SEL_IN` | `0x1400f28` | вход RDMA0 / RDMA0 input |
| `MMSYS_DITHER0_MOUT` | `0x1400f20` | выход DITHER0 / DITHER0 output |
| `MMSYS_DSI0_SEL_IN` | `0x1400f30` | вход DSI0 / DSI0 input |
| `OVL0_EN` | `0x14005000` | бит 22 — теневой перезапуск / bit 22 — shadow reload |
| `OVL0_SRC_CON` | `0x1400502c` | включение слоёв / layer enable |
| `OVL0_L0_ADDR` | `0x14005f40` | адрес кадра в DRAM / frame address in DRAM |
| `DSI0_START` | `0x14013000` | бит 0 — запуск передачи / bit 0 — start transfer |
| `DSI0_CMDQ_SIZE` | `0x14013060` | биты [5:0] — счётчик, бит 15 — запуск / bits [5:0] count, bit 15 trigger |
| `DSI0_CMDQ` | `0x14013d00` | **FIFO команд — для MT6789 это `0xd00`, а не `0x200`** / **command FIFO — MT6789 uses `0xd00`, not `0x200`** |

### Кэш обязателен / Cache flush is mandatory

> ⚠️ OVL читает кадр **из DRAM**. `mw.l` пишет только в кэш CPU.
> Без `dc cvac` экран останется пустым.
>
> ⚠️ OVL reads the frame **from DRAM**. `mw.l` writes only to the CPU cache.
> Without `dc cvac` the screen stays blank.

```bash
# в U-Boot, после записи в кадр — драйвер делает это сам, вручную:
# in U-Boot, after writing the frame - the driver does this itself, manually:
dc cvac 0xfeabe000 0xa3e000    # или цикл по 64-байтным строкам / or a loop over 64-byte lines
```

Драйвер вызывает `flush_dcache_range()` — при ручной записи в кадр
(`mw.l`) нужно делать `dc cvac` самому.

The driver calls `flush_dcache_range()` — when writing the frame by hand
(`mw.l`), you must do `dc cvac` yourself.

---

## Диагностика / Diagnostics

После `probe()` драйвер пишет блок «магических» значений в память **сразу за кадром**.

After `probe()` the driver writes a block of "magic" values to memory **right after
the framebuffer**.

| Адрес / Address | Что / What |
|---|---|
| `0xff4a1400` | база диагностического буфера (`LK_DEFAULT_FB + DISP_FB_SIZE`) / diagnostic buffer base |
| `+0x00` | magic `0xDEADBEEF` |
| `+0x04` | `uc_priv->fb` |
| `+0x08` | `plat->base` |
| `+0x0c` | `uc_priv->fb_size` |

Читается **после загрузки U-Boot**, в консоли:

Read **after U-Boot has booted**, from the console:

```bash
md.l 0xff4a1400 20
```

Если `magic` не `0xDEADBEEF` — драйвер не дошёл до конца `probe()`, смотрите
сообщения в консоли ACM.

If `magic` is not `0xDEADBEEF`, the driver did not reach the end of `probe()` —
check the messages in the ACM console.

---

## Известные проблемы / Known issues

| Симптом / Symptom | Причина / Cause | Статус / Status |
|---|---|---|
| Пустой экран / blank screen | не сделан `dc cvac` после записи в кадр / no `dc cvac` after writing the frame | ✅ решено в драйвере / fixed in the driver |
| `Synchronous Abort` при старте / at startup | `simple-framebuffer` `0xfe0c8000` не отображён в MMU / not mapped in the MMU | ✅ узел удалён из DTS / node removed from the DTS |
| Артефакты, двоится текст / artefacts, doubled text | не применён `CFG_UPDATE` после мультиплексоров / no `CFG_UPDATE` after the muxes | ✅ вызывается в драйвере / issued by the driver |
| Логотип LK поверх текста / LK logo over U-Boot text | LK не очистил слои / LK did not clear the layers | ✅ `OVL0_SRC_CON` = `0x3` |

---

## Связанное / Related

- Решение / decision: [`../decisions/0001-custom-display-driver.md`](../decisions/0001-custom-display-driver.md)
- Часы, которые тянет дисплей / clocks the display depends on: [`clocks.md`](clocks.md)
- Живые дампы / live dumps: `u-boot-x676b_docs/AGENTS/mt6789_display_init_sequence.md`,
  `mt6789_ovl_layer_registers.md`, `mtk_disp_g99_diag_ram.md`
- Исходный LK / source LK: [`../reverse-engineering/lk-disassembly.md`](../reverse-engineering/lk-disassembly.md)