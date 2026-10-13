# USB / USB 2.0

**Статус: ✅ работает.** fastboot, консоль ACM и mass storage по USB.
**Status: ✅ working.** fastboot, ACM console and mass storage over USB.

---

## ЧТО / What

| | |
|---|---|
| Контроллер | MediaTek MUSB (musb-new), база **`0x11200000`** |
| PHY | T-PHY, `0x11f40000`, порт `u2port0` |
| В реальном DT | `mediatek,mt6789-usb20` |
| У нас в DTS | `mediatek,mt8518-musb`, `mediatek,mtk-musb` |
| Режим передачи | **PIO** (DMA-модуль для MT6789 не дописан) |
| Разъём на телефоне | USB-C, OTG |

---

## Кто чем занимается / Who does what

| Подсистема | Файл | Задача |
|---|---|---|
| Контроллер | `drivers/usb/musb-new/mt85xx.c` | инициализация MUSB, PHY-питание, подключение |
| Gadget | `drivers/usb/musb-new/musb_gadget.c` | функции USB: ACM, fastboot, UMS |
| PHY | `drivers/phy/mediatek/` | T-PHY, включение/сохранение питания |
| Консоль | `include/stdio.h` + `drivers/usb/gadget/cdc_acm.c` | `usbacm` — UART через USB |
| Fastboot | `drivers/fastboot/fastboot_usb.c` | протокол fastboot по USB |
| Mass storage | `drivers/usb/gadget/mass_storage.c` | UMS |

---

## ПОЧЕМУ так / Why

### 1. Почему `mt8518-musb`, а не `mt6789-usb20`

В форке `musb-new` содержит **единственный** MUSB-драйвер — `mt85xx.c`, и он
биндится только на одну строку:

```c
/* drivers/usb/musb-new/mt85xx.c:432 */
{ .compatible = "mediatek,mt8518-musb" },
```

Драйвера под `mediatek,mt6789-usb20` в форке **нет вообще** — это другое
семейство контроллеров. Ядро MUSB у них общее, различается обвязка PHY.
Поэтому в DTS оставлена строка, на которую реально подписывается драйвер.

### 2. Почему `PIO_ONLY`

Первая попытка собрать с DMA (`CONFIG_USB_MUSB_GADGET` без `_PIO_ONLY`) падала
на линковке — в `musb_dma` для этого SoC не хватает функций:

```
undefined reference to `dma_init'
undefined reference to `dma_poll_complete'
undefined reference to `dma_unmap_single'
```

Попытка `CONFIG_ARCH_DMA_PADDR_LEN=64` не помогла. Модуль `musb_dma` в форке
дописан не для MT6789. **Рабочий компромисс** — PIO:

```kconfig
CONFIG_USB_MUSB_GADGET=y
CONFIG_USB_MUSB_PIO_ONLY=y      /* ← работает */
CONFIG_USB_MUSB_MT85XX=y
```

PIO медленнее по скорости (USB 2.0 high-speed), но для консоли и fastboot
достаточно, а для UMS — терпимо.

### 3. Почему добавлено включение pullup после сброса

При сбросе контроллера `softconnect` оставался выключенным — ПК не видел
устройство вообще (журнал коммита `993cce998be`: *«PC doesn't detect USB device
(pullup toggles off)»*). Добавлено в `musb_gadget.c`:

```c
#ifdef __UBOOT__
        /* Re-enable pullup after reset if it was on before */
        if (musb->softconnect)
                musb_pullup(musb, 1);
#endif
```

### 4. Почему ACM не запускается автоматически

В `preboot` автоматический `usb start` ломал перечисление (v177: *«remove debug
logging (broke USB enumeration)»*). Сейчас ACM включается **вручную пунктом меню** —
так надёжнее: консоль всегда доступна, но USB не мешает загрузке.

---

## Что работает / What works

| Функция | Пункт меню | Команда |
|---|---|---|
| **Fastboot по USB** | `Fastboot USB` | `fastboot -l $fastboot_addr_r usb 0` |
| **Консоль ACM** | `Enable serial console gadget` | `usb start` + `usbacm` на stdin/stdout/stderr |
| **Mass storage (UMS)** | `Enable USB mass storage` | `ums 0 scsi 2` |

### Параметры fastboot

```kconfig
CONFIG_FASTBOOT_BUF_ADDR=0x60000000
CONFIG_FASTBOOT_BUF_SIZE=0x10000000
CONFIG_FASTBOOT_FLASH=y
CONFIG_FASTBOOT_FLASH_BLOCK=y
CONFIG_FASTBOOT_FLASH_BLOCK_INTERFACE_NAME="scsi"    /* ← UFS через scsi */
CONFIG_FASTBOOT_FLASH_BLOCK_DEVICE_ID=2
CONFIG_FASTBOOT_OEM_RUN=y
CONFIG_FASTBOOT_CMD_OEM_CONSOLE=y
CONFIG_FASTBOOT_OEM_BOARD=y
```

Блок через `scsi` — особенность MediaTek: разделы `lk_a`/`lk_b` доступны
не как обычные флеш-разделы, а через SCSI-слой UFS.

---

## Как пользоваться / Usage

### Консоль по USB (ACM)

Подключить USB-C к ПК **до включения** телефона. В меню выбрать
`Enable serial console gadget`. На ПК появится последовательный порт:

```bash
# Linux, права
sudo usermod -aG dialout $USER
picocom -b 115200 /dev/ttyACM0
```

> ⚠️ **Не открывайте порт и не закрывайте телефон, пока не закончите.**
> При каждой перезагрузке порт переподключается.

### Fastboot

```bash
fastboot devices      # должен появиться mnnoee/u-boot-mtk-g99
fastboot flash lk_a uboot-lk-x676b.img
fastboot flash lk_b uboot-lk-x676b.img
fastboot reboot
```

### Mass storage (UMS)

Пункт `Enable USB mass storage` — телефон появится на ПК как диск.
Это **самый безопасный способ залить прошивку**: работает даже когда
fastboot по USB не поднимается.

---

## Известные проблемы / Known issues

| Симптом | Причина | Обходной путь |
|---|---|---|
| `undefined reference to dma_init` при сборке | `musb_dma` не дописан для MT6789 | `CONFIG_USB_MUSB_PIO_ONLY=y` |
| ПК не видит устройство | pullup не вернулся после сброса | исправлено в `musb_gadget.c` |
| USB отваливается при включённом `preboot=usb start` | ломает перечисление | ACM только по пункту меню (v177) |
| Низкая скорость UMS | PIO вместо DMA | приемлемо; быстрее — только через `fastboot flash` |
| `ums` не видит раздел | неверный LUN | `ums 0 scsi 2` — LUN 2 = `sdb` (boot) |

---

## Связанное / Related

- Решение: [`../decisions/0002-usb-pio-only.md`](../decisions/0002-usb-pio-only.md)
- Совместимость строк: [`../compatibility.md`](../compatibility.md)
- Почему нет `simple-framebuffer` и прочие решения:
  [`../decisions/README.md`](../decisions/README.md)