# Кнопки / Buttons

**Статус: ✅ работает.** Volume Up и Volume Down нажимаются, ими управляется
bootmenu — отдельный UART-кабель не нужен.
**Status: ✅ working.** Volume Up and Volume Down drive the bootmenu — no UART
cable required.

---

## ЧТО / What

| RU | EN |
|---|---|
| Контроллер: MediaTek KP (keypad), база **`0x10010000`** | Controller: MediaTek KP (keypad), base **`0x10010000`** |
| Драйвер: **`drivers/button/button-mtk-kp.c` — самописный, 204 строки** | Driver: **`drivers/button/button-mtk-kp.c` — custom, 204 lines** |
| Основан на: `drivers/input/keyboard/mt6779-keypad.c` из ядра Linux | Based on: `drivers/input/keyboard/mt6779-keypad.c` from the Linux kernel |
| Подсистема U-Boot: `button-uclass` (`CONFIG_BUTTON=y`) | U-Boot subsystem: `button-uclass` |
| Совместимость в DTS: `mediatek,mt6779-keypad` | DTS compatibility: `mediatek,mt6779-keypad` |
| Конфиг: `CONFIG_BUTTON=y`, `CONFIG_BUTTON_MTK_KP=y`, `CONFIG_BUTTON_REMAP_PHONE_KEYS=y` | Config: same |

> ⚠️ **В форке `ellyq/u-boot-mtk-g99` этого драйвера не было.**
> В U-Boot были только `button-adc`, `button-gpio` и `button-qcom-pmic` —
> ни один не подходит к матричной клавиатуре MTK.
>
> ⚠️ **There was no such driver in the `ellyq` fork.** U-Boot only had `button-adc`,
> `button-gpio` and `button-qcom-pmic` — none of which fits the MTK matrix keypad.

---

## Кто чем занимается / Who does what

| Файл / File | Задача / Task |
|---|---|
| `drivers/button/button-mtk-kp.c` | **написан с нуля**: чтение матрицы, распознавание нажатий, включение гейтов / **written from scratch**: matrix reading, press detection, gate enabling |
| `arch/arm/dts/mt6789-infinix-x676b.dts` | узлы `volume-up` и `volume-down` / the two nodes |
| `board/mediatek/mtk-phone.env` | подключение `button-kbd` к `stdin` в `preboot` / attaching `button-kbd` to `stdin` |
| `drivers/button/Kconfig`, `Makefile` | регистрация нового драйвера / registering the new driver |

---

## Как это работает / How it works

### Регистры контроллера / Controller registers

| Регистр / Register | Смещение / Offset | Что / What |
|---|---|---|
| `KP_MEM1` | `0x0004` | матрица, биты [15:0] / matrix, bits [15:0] |
| `KP_MEM2` | `0x0008` | |
| `KP_MEM3` | `0x000c` | |
| `KP_MEM4` | `0x0010` | |
| `KP_MEM5` | `0x0014` | |

**Активный уровень — низкий:** `0` = нажато, `1` = отпущено.
**Active level is low:** `0` = pressed, `1` = released.
Всего 136 позиций (4 × 32 + 8).
136 positions in total (4 × 32 + 8).

### Узлы в DTS / DTS nodes

Обе кнопки читаются из одного регистра — различаются значением матрицы:
Both buttons are read from the same register — they differ by the matrix value:

```dts
volume-down {
        compatible = "mediatek,mt6779-keypad";
        reg = <0x10010000 0x1000>;
        label = "vdown";
        linux,code = <KEY_VOLUMEDOWN>;    /* 114 */
        mediatek,matrix-val = <0xbfdf>;
        mediatek,matrix-reg = <0x0c>;     /* KP_MEM3 */
        status = "okay";
};

volume-up {
        compatible = "mediatek,mt6779-keypad";
        reg = <0x10010000 0x1000>;
        label = "vup";
        linux,code = <KEY_VOLUMEUP>;      /* 115 */
        mediatek,matrix-val = <0xdfef>;
        mediatek,matrix-reg = <0x0c>;     /* KP_MEM3 */
        status = "okay";
};
```

Коды клавиш: Volume Down = `114`, Volume Up = `115`.
Key codes: Volume Down = `114`, Volume Up = `115`.

### Побочный эффект: гейты часов / Side effect: clock gates

Драйвер заодно **включает гейты INFRACFG_AO** для периферии загрузки —
иначе контроллер клавиатуры не читается:

The driver also **enables the INFRACFG_AO gates** for boot peripherals — otherwise
the keypad controller cannot be read:

| Группа / Group | Базовый адрес / Base | SET / CLR / STA |
|---|---|---|
| IFRAO0 | `0x10001000` | `+0x80` / `+0x84` / `+0x90` |
| IFRAO1 | `0x10001000` | `+0x88` / `+0x8c` / `+0x94` |
| IFRAO2 | `0x10001000` | `+0xa4` / `+0xa8` / `+0xac` |
| IFRAO4 | `0x10001000` | `+0xe0` / `+0xe4` / `+0xe8` |
| IFRAO5 | `0x10001000` | `+0xd0` / `+0xd4` / `+0xd8` |

> 💡 Гейты включаются через **SET**, а не CLR. В SD-драйвере наоборот —
> обход зависания шины `infra_ao` пришлось делать через CLR. Разные регионы
> ведут себя по-разному, см. [`sd-card-msdc1.md`](sd-card-msdc1.md).
>
> 💡 Gates are enabled via **SET**, not CLR. In the SD driver it is the opposite —
> working around the `infra_ao` bus hang required CLR. Different registers behave
> differently.

---

## Как это используется / Usage

### Подключение к консоли / Attaching to the console

```kconfig
preboot=setenv stdin serial,button-kbd; setenv stdout serial,vidconsole; setenv stderr serial,vidconsole; scsi reset; scsi scan
```

`button-kbd` добавляется в `stdin` — кнопки работают вместе с serial
и выводом на экран.

`button-kbd` is added to `stdin` — the buttons work alongside serial and the
on-screen output.

### Управление bootmenu / Driving the bootmenu

| Кнопка / Button | Действие / Action |
|---|---|
| **Volume Up** | Enter — выбрать пункт / pick the entry |
| **Volume Down** | скролл вниз / scroll down |

Навигация зацикливается: с последнего пункта — на первый, с первого — на последний.
Navigation wraps around: past the last entry goes to the first, and back.

### Проверка / Checking

```bash
# в консоли U-Boot / in the U-Boot console
button list
button vup
button vdown
```

---

## Контекст / How it came about

| Версия / Version | Что сделано / What was done |
|---|---|
| v88 | драйвер заработал — `button vup` / `button vdown` корректно определяют нажатие / the driver started working |
| v89 | кнопки подключены к `bootmenu` / buttons wired into the `bootmenu` |

Коммиты / commits: `9501b443546` (регистры из Linux-драйвера / registers from
the Linux driver), `167d98d575c` (интеграция в bootmenu / integration into bootmenu).

---

## Связанное / Related

- Регистры KP / KP registers: `drivers/button/button-mtk-kp.c`
- Узлы / nodes: `arch/arm/dts/mt6789-infinix-x676b.dts`, строки 210–230
- Окружение / environment: `board/mediatek/mtk-phone.env`
- Гейты / gates: [`clocks.md`](clocks.md)
- Совместимость строк / string compatibility: [`../compatibility.md`](../compatibility.md)