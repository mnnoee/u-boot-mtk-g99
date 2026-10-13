# 0002. USB только в режиме PIO, без DMA / USB PIO-only, no DMA

**Статус:** принято / accepted
**Дата:** июль 2026 (коммит `993cce998be` и далее)

---

## Контекст / Context

Задача: любой способ общаться с телефоном с ПК **без UART-кабеля**.
U-Boot умеет fastboot, консоль ACM и mass storage — но только если USB
контроллер поднялся.

Проблема: сборка с DMA (`CONFIG_USB_MUSB_GADGET` без `_PIO_ONLY`) падала
на линковке — модуль `musb_dma` в форке не дописан для MT6789:

```
undefined reference to `dma_init'
undefined reference to `dma_poll_complete'
undefined reference to `dma_unmap_single'
```

Попытка `CONFIG_ARCH_DMA_PADDR_LEN=64` проблему не решила.

Вторая проблема: даже когда контроллер поднимался, **ПК не видел устройство** —
pullup не возвращался после сброса (журнал коммита: *«PC doesn't detect USB
device (pullup toggles off)»*).

---

## Решение / Decision

**1. Работать в режиме PIO:**

```kconfig
CONFIG_USB_MUSB_GADGET=y
CONFIG_USB_MUSB_PIO_ONLY=y      /* ← DMA отключён намеренно */
CONFIG_USB_MUSB_MT85XX=y
```

**2. Вернуть pullup после сброса** в `drivers/usb/musb-new/musb_gadget.c`:

```c
#ifdef __UBOOT__
        /* Re-enable pullup after reset if it was on before */
        if (musb->softconnect)
                musb_pullup(musb, 1);
#endif
```

**3. Не запускать USB автоматически в `preboot`** — ACM включается пунктом меню.

---

## Последствия / Consequences

**Плюсы**
- USB работает: fastboot, ACM-консоль, UMS — всё три.
- Скорость PIO достаточна: для консоли и даже для fastboot.
- Не требует дописывать чужой `musb_dma`.

**Минусы**
- UMS заметно медленнее, чем через DMA.
- При большой передаче fastboot возможны таймауты на стороне ПК.
- Полагаемся на `musb-new` для SoC, для которого драйвер писался под MT8518.

---

## Отвергнутые варианты / Rejected alternatives

| Вариант | Почему отвергнут |
|---|---|
| Дописать `musb_dma` для MT6789 | Более крупная задача, чем весь USB-проект вместе взятый |
| `CONFIG_ARCH_DMA_PADDR_LEN=64` | Не помогло, линковка та же |
| `preboot=usb start` для автозапуска ACM | v177: **ломало USB-перечисление** после отладочной печати |
| Другая строка compatible | `musb-new` биндится **только** на `mediatek,mt8518-musb` |

---

## Побочный эффект, о котором стоит помнить / Side effect worth remembering

Отладочная печать (`printf`) в MMC-коде **ломала перечисление USB** (v175–v176),
и это выглядело как «USB сломался». Реальная причина — не USB, а печать.
С v177 печать убрана, и оба интерфейса работают одновременно.

Если снова столкнётесь с «сломанным USB» — сначала убирайте печать,
и только потом ищите проблему в USB.

---

## Связанное / Related

- Подробности: [`../hardware/usb.md`](../hardware/usb.md)
- Совместимость строк: [`../compatibility.md`](../compatibility.md)