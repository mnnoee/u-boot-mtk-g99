# 0001. Собственный драйвер дисплея / Custom display driver

**Статус:** принято / accepted
**Дата:** июль–август 2026 (v183 → v187)

---

## Контекст / Context

Нужно выводить текст U-Boot на AMOLED 1080×2400 телефона Infinix X676B.

We need to render U-Boot text on the Infinix X676B's 1080×2400 AMOLED.

Проблема: в форке `ellyq/u-boot-mtk-g99` **нет DRM-драйвера для MT6789** —
ни `mtk_disp`, ни панельных драйверов. Варианты с `simple-framebuffer`
из DT тоже не работают: адрес `0xfe0c8000` (из реального DT) не отображён
в MMU U-Boot, и обращение к нему даёт **Synchronous Abort**.

The problem: the fork has **no DRM driver for MT6789** — neither `mtk_disp` nor
panel drivers. The `simple-framebuffer` DT approach also fails: the address
`0xfe0c8000` (from the real DT) is not mapped in the U-Boot MMU, and accessing
it causes a **Synchronous Abort**.

Дополнительно: **DSI-контроллер и панель ILI7835F уже инициализированы
stock LK**, который грузится раньше U-Boot. То есть заново настраивать панель
не нужно и даже вредно.

Additionally: **the DSI controller and the ILI7835F panel are already
initialised by the stock LK**, which runs before U-Boot. Re-initialising the
panel is neither needed nor harmful to avoid.

---

## Решение / Decision

Написать минимальный драйвер `UCLASS_VIDEO`:
`drivers/video/mediatek/mtk_disp_g99.c` (453 строки), который:

Write a minimal `UCLASS_VIDEO` driver —
`drivers/video/mediatek/mtk_disp_g99.c` (453 lines) — which:

1. **не** инициализирует панель и DSI заново — считает, что это сделал LK;
   does **not** re-initialise the panel and DSI — assumes LK already did;
2. читает адрес кадра из `OVL0_L0_ADDR` (то, что поставил LK), с откатом
   на `0xfeabe000`;
   reads the framebuffer address from `OVL0_L0_ADDR` (what LK programmed),
   falling back to `0xfeabe000`;
3. перенастраивает тракт **OVL0 → RDMA0 → DITHER0 → DSI0** под свой кадр;
   reconfigures the **OVL0 → RDMA0 → DITHER0 → DSI0** path for its frame;
4. обновляет кадр через DSI DCS `Write Memory Start`.
   updates the frame via DSI DCS `Write Memory Start`.

Узел с `compatible = "simple-framebuffer"` **удалён из DTS**. Область памяти
`0xfe0c8000` осталась в `reserved-memory`, но **без флага `no-map`** —
чтобы U-Boot мог к ней обращаться.

The node with `compatible = "simple-framebuffer"` was **removed from the DTS**.
The memory area `0xfe0c8000` stayed in `reserved-memory`, but **without the
`no-map` flag** so that U-Boot can reach it.

Включение: `CONFIG_VIDEO=y`, `CONFIG_VIDEO_MTK_G99=y`, `CONFIG_VIDEO_DAMAGE=y`.

---

## Последствия / Consequences

**Плюсы / Pros**
- Текст на экране работает — единственный способ в форке без большой работы.
  Text on screen works — the only viable approach in this fork without a large effort.
- Адрес кадра берётся у того, кто реально инициализировал панель.
  The framebuffer address comes from whoever actually initialised the panel.
- 453 строки вместо переноса DRM-стека Linux.
  453 lines instead of porting the Linux DRM stack.

**Минусы / Cons**
- Привязка к тому, что оставил LK. Если LK изменится — драйвер сломается.
  Tied to what LK left behind. If LK changes, the driver breaks.
- Только текст: ни графики, ни картинок (`VIDCONSOL` в UCLASS_VIDEO).
  Text only: no graphics, no images.
- Значения регистров дисплея MT6789 нигде не задокументированы — восстановлены
  из дизассемблированного LK и живых дампов Android.
  MT6789 display register values are undocumented — they were recovered from the
  disassembled LK and live Android dumps.

---

## Отвергнутые варианты / Rejected alternatives

| Вариант / Option | Почему отвергнут / Why rejected |
|---|---|
| `simple-framebuffer` из DT | `0xfe0c8000` не отображён в MMU → Synchronous Abort / not mapped → Abort |
| Перенос DRM `mtk_disp` из mainline | Огромный объём для U-Boot; панель уже поднята LK / far too large for U-Boot; the panel is already up |
| Перенастройка DSI в U-Boot | Ломала картинку: `reset + CMDQ_SIZE 0x0001 + CON_CTRL DSI_EN` / broke the picture |
| `CMDQ` по старому смещению `0x200` | У MT6789 FIFO команд по **`0xd00`** / MT6789 has the FIFO at **`0xd00`** |

---

## Ключевые детали, которые легко забыть / Easy to forget

```c
/* Управление гейтами — через CLR, не через SET */
#define MMSYS_CLK_CLR  (MMSYS_BASE + 0x108)

/* Теневой перезапуск OVL — бит 22 */
#define OVL_SHADOW_RELOAD  0x00400001

/* FIFO команд DSI у MT6789 — НЕ 0x200 */
#define DSI0_CMDQ  (DSI0_BASE + 0xd00)

/* DCS Write Memory Start, 24-битный код */
#define DSI_CMDQ_WRITE_MEM_START  0x2c3909
```

**И самое частое:** после записи в кадр обязателен `dc cvac` — OVL читает
из DRAM, а `mw.l` пишет только в кэш CPU.

**And the most frequent one:** after writing to the framebuffer `dc cvac` is
mandatory — OVL reads from DRAM while `mw.l` writes only to the CPU cache.

---

## Связанное / Related

- Подробности и регистры / details and registers: [`../hardware/display.md`](../hardware/display.md)
- Источник знаний / source of knowledge: [`../reverse-engineering/lk-disassembly.md`](../reverse-engineering/lk-disassembly.md)