# USB / USB 2.0

**Статус: ✅ работает.** fastboot, консоль ACM и mass storage по USB.
**Status: ✅ working.** fastboot, ACM console and mass storage over USB.

---

## ЧТО / What

| RU | EN |
|---|---|
| Контроллер: MediaTek MUSB (musb-new), база **`0x11200000`** | Controller: MediaTek MUSB (musb-new), base **`0x11200000`** |
| PHY: T-PHY, `0x11f40000`, порт `u2port0` | PHY: T-PHY, `0x11f40000`, port `u2port0` |
| В реальном DT: `mediatek,mt6789-usb20` | On the real device: `mediatek,mt6789-usb20` |
| У нас в DTS: `mediatek,mt8518-musb`, `mediatek,mtk-musb` | In our DTS: `mediatek,mt8518-musb`, `mediatek,mtk-musb` |
| Режим передачи: **PIO** (DMA-модуль для MT6789 не дописан) | Transfer mode: **PIO** (the DMA module is incomplete for MT6789) |
| Разъём: USB-C, OTG | Connector: USB-C, OTG |

---

## Кто чем занимается / Who does what

| Подсистема / Subsystem | Файл / File | Задача / Task |
|---|---|---|
| Контроллер / Controller | `drivers/usb/musb-new/mt85xx.c` | инициализация MUSB, питание PHY, подключение / MUSB init, PHY power, attach |
| Gadget | `drivers/usb/musb-new/musb_gadget.c` | функции USB: ACM, fastboot, UMS / USB functions: ACM, fastboot, UMS |
| PHY | `drivers/phy/mediatek/` | T-PHY, включение и сохранение питания / T-PHY, power on and save |
| Консоль / Console | `include/stdio.h` + `drivers/usb/gadget/cdc_acm.c` | `usbacm` — UART через USB / `usbacm` — UART over USB |
| Fastboot | `drivers/fastboot/fastboot_usb.c` | протокол fastboot по USB / fastboot protocol over USB |
| Mass storage | `drivers/usb/gadget/mass_storage.c` | UMS |

---

## ПОЧЕМУ так / Why

### 1. Почему `mt8518-musb`, а не `mt6789-usb20`

В форке `musb-new` содержит **единственный** MUSB-драйвер — `mt85xx.c`, и он
биндится только на одну строку:

The fork's `musb-new` contains **a single** MUSB driver — `mt85xx.c` — and it binds
to exactly one string:

```c
/* drivers/usb/musb-new/mt85xx.c:432 */
{ .compatible = "mediatek,mt8518-musb" },
```

Драйвера под `mediatek,mt6789-usb20` в форке **нет вообще** — это другое
семейство контроллеров. Ядро MUSB у них общее, различается обвязка PHY.
Поэтому в DTS оставлена строка, на которую реально подписывается драйвер.

There is **no driver at all** for `mediatek,mt6789-usb20` in the fork — a different
controller family. The MUSB core is shared, the PHY glue differs. Hence the DTS
keeps the string the driver actually binds to.

### 2. Почему `PIO_ONLY`

Первая попытка собрать с DMA (`CONFIG_USB_MUSB_GADGET` без `_PIO_ONLY`) падала
на линковке — в `musb_dma` для этого SoC не хватает функций:

The first attempt to build with DMA (`CONFIG_USB_MUSB_GADGET` without `_PIO_ONLY`)
failed at link time — `musb_dma` lacks these functions for this SoC:

```
undefined reference to `dma_init'
undefined reference to `dma_poll_complete'
undefined reference to `dma_unmap_single'
```

Попытка `CONFIG_ARCH_DMA_PADDR_LEN=64` не помогла. Модуль `musb_dma` в форке
дописан не для MT6789. **Рабочий компромисс** — PIO:

`CONFIG_ARCH_DMA_PADDR_LEN=64` did not help. The `musb_dma` module is not finished
for MT6789. **The working compromise** is PIO:

```kconfig
CONFIG_USB_MUSB_GADGET=y
CONFIG_USB_MUSB_PIO_ONLY=y      /* ← работает / works */
CONFIG_USB_MUSB_MT85XX=y
```

PIO медленнее по скорости (USB 2.0 high-speed), но для консоли и fastboot
достаточно, а для UMS — терпимо.

PIO is slower (USB 2.0 high-speed), but fast enough for a console and fastboot,
and acceptable for UMS.

### 3. Почему добавлено включение pullup после сброса

При сбросе контроллера `softconnect` оставался выключенным — ПК не видел
устройство вообще (журнал коммита `993cce998be`: *«PC doesn't detect USB device
(pullup toggles off)»*). Добавлено в `musb_gadget.c`:

After a controller reset `softconnect` stayed off — the PC did not see the device
at all (commit `993cce998be`: *"PC doesn't detect USB device (pullup toggles
off)"*). Added to `musb_gadget.c`:

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

An automatic `usb start` in `preboot` broke enumeration (v177: *"remove debug
logging (broke USB enumeration)"*). ACM is now **started manually from a menu
entry** — more reliable: the console is always available, but USB does not get in
the way of booting.

---

## Что работает / What works

| Функция / Function | Пункт меню / Menu entry | Команда / Command |
|---|---|---|
| **Fastboot по USB** | `Fastboot USB` | `fastboot -l $fastboot_addr_r usb 0` |
| **Консоль ACM** | `Enable serial console gadget` | `usb start` + `usbacm` на stdin/stdout/stderr |
| **Mass storage (UMS)** | `Enable USB mass storage` | `ums 0 scsi 2` |

### Параметры fastboot / Fastboot settings

```kconfig
CONFIG_FASTBOOT_BUF_ADDR=0x60000000
CONFIG_FASTBOOT_BUF_SIZE=0x10000000
CONFIG_FASTBOOT_FLASH=y
CONFIG_FASTBOOT_FLASH_BLOCK=y
CONFIG_FASTBOOT_FLASH_BLOCK_INTERFACE_NAME="scsi"    /* ← UFS через scsi / UFS via scsi */
CONFIG_FASTBOOT_FLASH_BLOCK_DEVICE_ID=2
CONFIG_FASTBOOT_OEM_RUN=y
CONFIG_FASTBOOT_CMD_OEM_CONSOLE=y
CONFIG_FASTBOOT_OEM_BOARD=y
```

Блок через `scsi` — особенность MediaTek: разделы `lk_a`/`lk_b` доступны
не как обычные флеш-разделы, а через SCSI-слой UFS.

Block access via `scsi` is a MediaTek peculiarity: the `lk_a`/`lk_b` partitions
are not exposed as ordinary flash partitions but through the UFS SCSI layer.

---

## Как пользоваться / Usage

### Консоль по USB (ACM)

Подключить USB-C к ПК **до включения** телефона. В меню выбрать
`Enable serial console gadget`. На ПК появится последовательный порт:

Connect USB-C to the PC **before powering the phone on**. Choose
`Enable serial console gadget` in the menu. A serial port appears on the PC:

```bash
# Linux, права / permissions
sudo usermod -aG dialout $USER
picocom -b 115200 /dev/ttyACM0
```

> ⚠️ **Не закрывайте порт и не выключайте телефон, пока не закончите.**
> При каждой перезагрузке порт переподключается.
>
> ⚠️ **Do not close the port or power off the phone until you are done.**
> The port re-enumerates on every reboot.

### Fastboot

```bash
fastboot devices      # должен появиться mnnoee/u-boot-mtk-g99 / should show up as
fastboot flash lk_a uboot-lk-x676b.img
fastboot flash lk_b uboot-lk-x676b.img
fastboot reboot
```

### Mass storage (UMS)

Пункт `Enable USB mass storage` — телефон появится на ПК как диск.
Это **самый безопасный способ залить прошивку**: работает даже когда
fastboot по USB не поднимается.

The `Enable USB mass storage` entry makes the phone appear as a disk on the PC.
This is **the safest way to flash**: it works even when USB fastboot fails to
come up.

---

## Известные проблемы / Known issues

| Симптом / Symptom | Причина / Cause | Обходной путь / Workaround |
|---|---|---|
| `undefined reference to dma_init` при сборке / at build time | `musb_dma` не дописан для MT6789 / not finished for MT6789 | `CONFIG_USB_MUSB_PIO_ONLY=y` |
| ПК не видит устройство / PC sees no device | pullup не вернулся после сброса / pullup not restored after reset | исправлено в `musb_gadget.c` / fixed in `musb_gadget.c` |
| USB отваливается при `preboot=usb start` / drops with `preboot=usb start` | ломает перечисление / breaks enumeration | ACM только по пункту меню / ACM only via the menu entry (v177) |
| Низкая скорость UMS / slow UMS | PIO вместо DMA / PIO instead of DMA | приемлемо; быстрее — только через `fastboot flash` / acceptable; faster only via `fastboot flash` |
| `ums` не видит раздел / `ums` sees no partition | неверный LUN / wrong LUN | `ums 0 scsi 2` — LUN 2 = `sdb` (boot) |

---

## Связанное / Related

- Решение / decision: [`../decisions/0002-usb-pio-only.md`](../decisions/0002-usb-pio-only.md)
- Часы / clocks: [`clocks.md`](clocks.md) — `SSUSB`, `USBPLL`
- Совместимость строк / string compatibility: [`../compatibility.md`](../compatibility.md)
- Почему нет `simple-framebuffer` и прочие решения / other decisions:
  [`../decisions/README.md`](../decisions/README.md)