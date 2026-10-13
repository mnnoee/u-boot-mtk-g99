# 0002. USB только в режиме PIO, без DMA / USB PIO-only, no DMA

**Статус:** принято / accepted
**Дата:** июль 2026 (коммит `993cce998be` и далее / and onwards)

---

## Контекст / Context

Задача: любой способ общаться с телефоном с ПК **без UART-кабеля**.
U-Boot умеет fastboot, консоль ACM и mass storage — но только если USB
контроллер поднялся.

The goal: any way to talk to the phone from a PC **without a UART cable**. U-Boot
can do fastboot, an ACM console and mass storage — but only if the USB controller
comes up.

Проблема: сборка с DMA (`CONFIG_USB_MUSB_GADGET` без `_PIO_ONLY`) падала
на линковке — модуль `musb_dma` в форке не дописан для MT6789:

The problem: building with DMA (`CONFIG_USB_MUSB_GADGET` without `_PIO_ONLY`)
failed at link time — the `musb_dma` module is not finished for MT6789:

```
undefined reference to `dma_init'
undefined reference to `dma_poll_complete'
undefined reference to `dma_unmap_single'
```

Попытка `CONFIG_ARCH_DMA_PADDR_LEN=64` проблему не решила.
`CONFIG_ARCH_DMA_PADDR_LEN=64` did not solve it.

Вторая проблема: даже когда контроллер поднимался, **ПК не видел устройство** —
pullup не возвращался после сброса (журнал коммита: *«PC doesn't detect USB
device (pullup toggles off)»*).

The second problem: even when the controller came up, **the PC did not see the
device** — the pullup was not restored after a reset (commit log: *"PC doesn't
detect USB device (pullup toggles off)"*).

---

## Решение / Decision

**1. Работать в режиме PIO / Operate in PIO mode:**

```kconfig
CONFIG_USB_MUSB_GADGET=y
CONFIG_USB_MUSB_PIO_ONLY=y      /* ← DMA отключён намеренно / DMA off by design */
CONFIG_USB_MUSB_MT85XX=y
```

**2. Вернуть pullup после сброса / Restore the pullup after reset**
в `drivers/usb/musb-new/musb_gadget.c`:

```c
#ifdef __UBOOT__
        /* Re-enable pullup after reset if it was on before */
        if (musb->softconnect)
                musb_pullup(musb, 1);
#endif
```

**3. Не запускать USB автоматически в `preboot`** — ACM включается пунктом меню.
**Do not start USB automatically in `preboot`** — ACM is enabled from a menu entry.

---

## Последствия / Consequences

**Плюсы / Pros**
- USB работает: fastboot, ACM-консоль, UMS — всё три.
  USB works: fastboot, ACM console, UMS — all three.
- Скорость PIO достаточна: для консоли и даже для fastboot.
  PIO speed is sufficient for a console and even for fastboot.
- Не требует дописывать чужой `musb_dma`.
  No need to finish someone else's `musb_dma`.

**Минусы / Cons**
- UMS заметно медленнее, чем через DMA.
  UMS is noticeably slower than with DMA.
- При большой передаче fastboot возможны таймауты на стороне ПК.
  Large fastboot transfers may time out on the PC side.
- Полагаемся на `musb-new` для SoC, для которого драйвер писался под MT8518.
  We depend on `musb-new` for a SoC the driver was originally written for MT8518.

---

## Отвергнутые варианты / Rejected alternatives

| Вариант / Option | Почему отвергнут / Why rejected |
|---|---|
| Дописать `musb_dma` для MT6789 | Более крупная задача, чем весь USB-проект вместе взятый / larger than the whole USB project |
| `CONFIG_ARCH_DMA_PADDR_LEN=64` | Не помогло, линковка та же / did not help, same link error |
| `preboot=usb start` для автозапуска ACM | v177: **ломало USB-перечисление** / **broke USB enumeration** |
| Другая строка compatible | `musb-new` биндится **только** на `mediatek,mt8518-musb` / binds to **only** `mediatek,mt8518-musb` |

---

## Побочный эффект, о котором стоит помнить / Side effect worth remembering

Отладочная печать (`printf`) в MMC-коде **ломала перечисление USB** (v175–v176),
и это выглядело как «USB сломался». Реальная причина — не USB, а печать.
С v177 печать убрана, и оба интерфейса работают одновременно.

Debug printing (`printf`) in the MMC code **broke USB enumeration** (v175–v176),
and it looked like "USB is broken". The real cause was not USB but the printing.
Since v177 the printing is gone and both interfaces work at the same time.

Если снова столкнётесь с «сломанным USB» — сначала убирайте печать,
и только потом ищите проблему в USB.

If you hit "broken USB" again — remove the printing first, and only then look
for a problem in USB.

---

## Связанное / Related

- Подробности / details: [`../hardware/usb.md`](../hardware/usb.md)
- Часы / clocks: [`../hardware/clocks.md`](../hardware/clocks.md)
- Совместимость строк / string compatibility: [`../compatibility.md`](../compatibility.md)