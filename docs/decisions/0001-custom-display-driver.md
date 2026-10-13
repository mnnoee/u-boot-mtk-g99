# 0001. Собственный драйвер дисплея / Custom display driver

**Статус:** принято / accepted
**Дата:** июль–август 2026 (v183 → v187)

---

## Контекст / Context

Нужно выводить текст U-Boot на AMOLED 1080×2400 телефона Infinix X676B.

Проблема: в форке `ellyq/u-boot-mtk-g99` **нет DRM-драйвера для MT6789** —
ни `mtk_disp`, ни панельных драйверов. Варианты с `simple-framebuffer`
из DT тоже не работают: адрес `0xfe0c8000` (из реального DT) не отображён
в MMU U-Boot, и обращение к нему даёт **Synchronous Abort**.

Дополнительно: **DSI-контроллер и панель ILI7835F уже инициализированы
stock LK**, который грузится раньше U-Boot. То есть заново настраивать панель
не нужно и даже вредно.

---

## Решение / Decision

Написать минимальный драйвер `UCLASS_VIDEO`:
`drivers/video/mediatek/mtk_disp_g99.c` (453 строки), который:

1. **не** инициализирует панель и DSI заново — считает, что это сделал LK;
2. читает адрес кадра из `OVL0_L0_ADDR` (то, что поставил LK), с откатом
   на `0xfeabe000`;
3. перенастраивает тракт **OVL0 → RDMA0 → DITHER0 → DSI0** под свой кадр;
4. обновляет кадр через DSI DCS `Write Memory Start`.

Узел с `compatible = "simple-framebuffer"` **удалён из DTS**. Область памяти
`0xfe0c8000` осталась в `reserved-memory`, но **без флага `no-map`** —
чтобы U-Boot мог к ней обращаться.

Включение: `CONFIG_VIDEO=y`, `CONFIG_VIDEO_MTK_G99=y`, `CONFIG_VIDEO_DAMAGE=y`.

---

## Последствия / Consequences

**Плюсы**
- Текст на экране работает — единственный способ в форке без большой работы.
- Адрес кадра берётся у того, кто реально инициализировал панель.
- 453 строки вместо переноса DRM-стека Linux.

**Минусы**
- Привязка к тому, что оставил LK. Если LK изменится — драйвер сломается.
- Только текст: ни графики, ни картинок (`VIDCONSOL` в UCLASS_VIDEO).
- Значения регистров дисплея MT6789 нигде не задокументированы — восстановлены
  из дизассемблированного LK и живых дампов Android.

---

## Отвергнутые варианты / Rejected alternatives

| Вариант | Почему отвергнут |
|---|---|
| `simple-framebuffer` из DT | `0xfe0c8000` не отображён в MMU → Synchronous Abort |
| Перенос DRM `mtk_disp` из mainline | Огромный объём для U-Boot; панель уже поднята LK |
| Перенастройка DSI в U-Boot | Ломала картинку: пробовались `reset + CMDQ_SIZE 0x0001 + CON_CTRL DSI_EN` |
| `CMDQ` по старому смещению `0x200` | У MT6789 FIFO команд по **`0xd00`** |

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

---

## Связанное / Related

- Подробности и регистры: [`../hardware/display.md`](../hardware/display.md)
- Источник знаний: [`../reverse-engineering/lk-disassembly.md`](../reverse-engineering/lk-disassembly.md)