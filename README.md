# U-Boot for Infinix Note 12 Pro (X676B) — MediaTek MT6789 / Helio G99

**Замена стокового LK (Little Kernel) на U-Boot для Infinix X676B.**
**Replacing stock LK (Little Kernel) bootloader with U-Boot on Infinix X676B.**

Проект не завершён. Ниже — честный статус: что работает, что нет, и как собрать.
The project is **not finished**. Below is an honest status: what works, what doesn't, and how to build it.

---

## Статус / Status

### ✅ Работает / Working

| | RU | EN |
|---|---|---|
| 🔌 **USB fastboot** | `fastboot` доступен по USB, устройство видно на ПК | `fastboot` over USB works, device is enumerated on PC |
| 🖥 **USB ACM (serial по USB)** | консоль U-Boot через USB CDC ACM, отдельный UART-кабель не нужен | U-Boot console over USB CDC ACM, no UART cable required |
| 💾 **UMS / fastboot block по UFS** | Mass storage через `scsi`, диск телефона виден на ПК | Mass storage via `scsi`, phone storage visible on PC |
| 📺 **Дисплей (текст)** | текст U-Boot рисуется на AMOLED через свой драйвер `mtk_disp_g99.c` | U-Boot text renders on AMOLED via custom `mtk_disp_g99.c` driver |
| 🔘 **Кнопки** | Volume Up / Down работают, свой драйвер `button-mtk-kp.c`, управляют bootmenu | Volume Up / Down work, custom `button-mtk-kp.c` driver drives the bootmenu |
| ⏻ **Poweroff** | команда `poweroff` в меню загрузки | `poweroff` command in the boot menu |
| 🔁 **A/B разделы** | прошивка в `lk_a` и `lk_b`, откат возможен | flash to `lk_a` / `lk_b`, rollback supported |

### ❌ Не работает / Not working

| | RU | EN |
|---|---|---|
| 💳 **SD-карта (SDC)** | контроллер MSDC1 (`0x11240000`) не инициализируется, карта не определяется. **На этом работа остановлена.** | MSDC1 controller (`0x11240000`) fails to initialise, card not detected. **Work stopped here.** |
| 🤖 **Загрузка Android** | ядро стартует и паникует: `Attempted to kill init!` — не тот DTB | kernel starts then panics: `Attempted to kill init!` — wrong DTB |

---

## Железо / Hardware

| | |
|---|---|
| **Устройство / Device** | Infinix Note 12 Pro, модель **X676B** |
| **SoC** | MediaTek Helio G99 (**MT6789**), 2× Cortex-A76 @ 2.2 GHz + 6× Cortex-A55 @ 2.0 GHz |
| **RAM / Память** | 8 ГБ / 8 GB |
| **Хранилище / Storage** | UFS 2.2, 256 ГБ, 3 LUN (sda/sdb boot, sdc userdata) |
| **Дисплей / Display** | AMOLED 6.7", 1080×2400 |
| **Android / ОС** | Android 12, XOS, A/B-разметка |
| **U-Boot** | 2025.10-rc5, форк [ellyq/u-boot-mtk-g99](https://github.com/ellyq/u-boot-mtk-g99) |
| **Конфиг / Config** | `configs/mt6789_infinix_x676b_defconfig` |
| **Device tree** | `arch/arm/dts/mt6789-infinix-x676b.dts` |

---

## Сборка / Build

### Зависимости / Dependencies

```bash
# Arch / Fedora / Debian-ish
sudo pacman -S aarch64-linux-gnu-gcc          # Arch
# sudo apt install gcc-aarch64-linux-gnu       # Debian/Ubuntu
```

Нужен только тулчейн; внешние библиотеки не требуются.
Only a cross-toolchain is needed, no external libraries.

### Сборка / Build steps

```bash
git clone https://github.com/mnnoee/u-boot-mtk-g99.git
cd u-boot-mtk-g99

# 1. Конфигурация / Configure
make CROSS_COMPILE=aarch64-linux-gnu- mt6789_infinix_x676b_defconfig

# 2. Сборка / Build
make CROSS_COMPILE=aarch64-linux-gnu- -j$(nproc)
```

Результат / output: `u-boot.bin`

### Упаковка в формат LK / Pack into LK container

Раздел `lk_a`/`lk_b` ожидает не голый `u-boot.bin`, а бинарник, упакованный в
обёртку LK. Используется `gimme-uboot.py` из `liblk`:

The `lk_a`/`lk_b` partitions do not take a raw `u-boot.bin` — they expect an LK-wrapped
binary. Use `gimme-uboot.py` from `liblk`:

```bash
# Нужен бинарник LK из стоковой прошивки / Need the stock LK binary
# (дамп раздела lk_a, он же служит базой для упаковки / also the packing base)
python3 gimme-uboot.py \
    lk_a_dump.img \
    u-boot.bin \
    uboot-lk-x676b.img
```

Требуется Python 3 и пакет `liblk` (или `pip install liblk`).

---

## Готовый образ / Prebuilt image

Не хочется собирать — можно скачать готовый:

**[`uboot-lk-x676b-v219.img`](https://github.com/mnoee/u-boot-mtk-g99/releases/tag/x676b-v219)** — 2.6 МБ,
pre-release, только для X676B.

Don't want to build? Download the prebuilt image.

> ⚠️ **Перед прошивкой сохраните стоковый LK из `lk_a`/`lk_b`** — без него
> вернуть телефон в рабочее состояние нечем, потому что сам U-Boot живёт
> в этих разделах.
>
> ⚠️ **Back up the stock LK from `lk_a`/`lk_b` before flashing** — there is no way
> back without it, since U-Boot itself is flashed to those partitions.

```bash
adb reboot bootloader
fastboot flash lk_a uboot-lk-x676b-v219.img
fastboot flash lk_b uboot-lk-x676b-v219.img
fastboot reboot
```

Полные заметки релиза, включая запасной путь через mtkclient и контрольную
сумму файла, — на странице релиза.

Full release notes, including the mtkclient fallback and the file checksum, are on
the release page.

---

## Прошивка / Flashing

### Через fastboot (рекомендуется) / Via fastboot (recommended)

```bash
adb reboot bootloader          # или: кнопка питания + громкость вверх
# or: hold power + volume up

fastboot flash lk_a uboot-lk-x676b.img
fastboot flash lk_b uboot-lk-x676b.img
fastboot reboot
```

### Через mtkclient (BYPASS: если телефон не входит в fastboot)

```bash
cd mtkclient
python mtk w lk_a ../uboot-lk-x676b.img
```

### Откат / Rollback

Прошивка идёт в обе A/B-части, поэтому откат — это прошивка **стокового LK**:

The image goes to both A/B slots, so rollback means flashing the **stock LK** back:

```bash
fastboot flash lk_a lk_a_stock.img
fastboot flash lk_b lk_b_stock.img
fastboot reboot
```

> ⚠️ **Осторожно / Warning.** Стоковый LK обязателен: U-Boot грузится из `lk_a`/`lk_b`,
> и без рабочего образа восстановления телефон не загрузится.
> The stock LK is mandatory — U-Boot itself is flashed to `lk_a`/`lk_b`, so without a
> working recovery image the phone will not boot.

---

## Структура репозитория / Repository layout

| Файл / Path | Назначение / Purpose |
|---|---|
| `configs/mt6789_infinix_x676b_defconfig` | конфигурация сборки / build configuration |
| `arch/arm/dts/mt6789-infinix-x676b.dts` | device tree устройства / device tree |
| `drivers/video/mediatek/mtk_disp_g99.c` | драйвер дисплея (текст на AMOLED) / display driver |
| `drivers/button/button-mtk-kp.c` | драйвер кнопок Volume Up / Down (самописный) / button driver |
| `drivers/mmc/mtk-sd.c` | MSDC-контроллер, здесь идёт работа над SD-картой / MSDC controller, where SD-card work is happening |
| `drivers/clk/mediatek/clk-mt6789.c` | часы MT6789, переписан под реальные гейты / clocks, rewritten |
| `include/early_log.h` | буфер раннего лога в DRAM / early log buffer |
| `board/mediatek/mtk-phone.env` | окружение U-Boot / U-Boot environment |
| `arch/arm/mach-mediatek/mt6789/init.c` | инициализация платы / board init |
| `gimme-uboot.py` | упаковка в LK / LK packing helper |

Оригинальный README U-Boot переименован в `README.u-boot`.
The original U-Boot README was renamed to `README.u-boot`.

---

## Документация / Documentation

📁 **[`docs/`](docs/)** — подробная документация проекта.

**[`docs/compatibility.md`](docs/compatibility.md)** — главная таблица
**«с чем, кого, почему»**: что реально стоит в телефоне, какая строка стоит
в нашем DT, какой драйвер на неё подхватывается и почему между ними расхождение.

**[`docs/compatibility.md`](docs/compatibility.md)** is the main matrix:
what the phone actually reports, what our DT says, which driver binds — and why
they differ.

| Раздел / Section | О чём / About |
|---|---|
| [`docs/hardware/display.md`](docs/hardware/display.md) | AMOLED, конвейер OVL0→RDMA0→DSI0, панель ILI7835F / AMOLED, pipeline, panel |
| [`docs/hardware/usb.md`](docs/hardware/usb.md) | MUSB, fastboot / ACM / UMS, почему PIO / MUSB, fastboot / ACM / UMS, why PIO |
| [`docs/hardware/sd-card-msdc1.md`](docs/hardware/sd-card-msdc1.md) | **SD-карта: полный журнал v161…v219 и что делать дальше** / full experiment log and next step |
| [`docs/hardware/memory-map.md`](docs/hardware/memory-map.md) | карта памяти и периферии / memory and peripheral map |
| [`docs/decisions/`](docs/decisions/) | ADR: почему решения именно такие / why decisions were made |
| [`docs/reverse-engineering/`](docs/reverse-engineering/) | как получены адреса и значения / how values were obtained |

> SD-карта — единственное, что не работает. Начните с
> [`sd-card-msdc1.md`](docs/hardware/sd-card-msdc1.md): там все уже сделанные
> эксперименты и вывод из них.
>
> The SD card is the only thing left broken. Start with
> [`sd-card-msdc1.md`](docs/hardware/sd-card-msdc1.md) — it lists every experiment
> already done and what came out of it.

---

## Лицензия / License

Ядро U-Boot — GPL-2.0+ (`README.u-boot`, `LICENSE`).
Изменения в этом форке — те же GPL-2.0+.
Код дисплейного драйвера и DTS для X676B — GPL-2.0+.

U-Boot itself is GPL-2.0+. Changes in this fork are GPL-2.0+ as well.

---

## Contributing

Приветствуются правки и эксперименты. Особенно нужна помощь с **SD-картой** —
это текущий блокер.
Pull requests and experiments are welcome. Help with the **SD card** is especially
needed — that is the current blocker.